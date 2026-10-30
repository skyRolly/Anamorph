#!/usr/bin/env python3
"""Bounded native Intel observation; never substitutes for the pluginval gate."""
import argparse
import hashlib
import json
import os
import platform
from pathlib import Path
import re
import signal
import shlex
import subprocess
import sys
import time

MARKER = 'ANAMORPH_LLDB_RESULT='


def classify(output):
    records = re.findall(r'^'+MARKER+r'(\{.*\})$', output, re.M)
    if len(records) != 1:
        return 'observer-error'
    result = json.loads(records[0])
    if result['state'] == 'stopped':
        if result.get('target_exception') and 'bad_function_call' in result.get('exception_type',''):
            return 'bad-function-call-throw'
        if 'SIGABRT' in str(result['stops']) and 'bad_function_call' in output:
            return 'bad-function-call-abort'
        return 'other-stop'
    if result['state'] != 'exited':
        return 'observer-error'
    if result['exit'] != 0:
        return 'inferior-failed'
    return 'completed' if re.search(r'^SUCCESS\s*$', output, re.M) else 'missing-success'


def classify_apple_control(output):
    outcome=classify(output)
    if outcome=='missing-success' and re.search(r'^Apple AU listener control completed; callbacks=[1-9][0-9]*$',output,re.M):
        return 'completed'
    return outcome


def self_test():
    def record(state, exit_code=0, stops=None, target=False):
        return MARKER+json.dumps({'state':state,'exit':exit_code,'stops':stops or [],
            'target_exception':target,'exception_type':'NSt3__117bad_function_callE' if target else ''})+'\n'
    cases = [
        ('SUCCESS\n'+record('exited'), 'completed'),
        ('SUCCESS\nlibc++abi: bad_function_call\n'+record('stopped',stops=['signal SIGABRT']), 'bad-function-call-abort'),
        ('SUCCESS\n'+record('exited',9), 'inferior-failed'),
        ('SUCCESS\n'+record('stopped',stops=['signal SIGSEGV']), 'other-stop'),
        ('SUCCESS\n', 'observer-error'),
        ('error: process launch failed\n', 'observer-error'),
        (record('exited'), 'missing-success'),
        ('SUCCESS\n'+record('running'), 'observer-error'),
        ('SUCCESS\n'+record('exited')+record('exited'), 'observer-error'),
        (record('stopped',stops=['breakpoint 1.1'],target=True), 'bad-function-call-throw'),
        ('bad_function_call\n'+record('stopped',stops=['breakpoint 1.1']), 'other-stop'),
    ]
    for text, expected in cases:
        assert classify(text) == expected, (text, expected)
    control_cases=[('Apple AU listener control completed; callbacks=8\n'+record('exited'),'completed'),
        ('Apple AU listener control completed; callbacks=0\n'+record('exited'),'missing-success'),
        (record('exited',4),'inferior-failed'),
        (record('exited',5),'inferior-failed'),
        ('Apple AU listener control completed; callbacks=8\n','observer-error')]
    for text,expected in control_cases:
        assert classify_apple_control(text)==expected,(text,expected)
    print(f'AU teardown observer self-test: {len(cases)+len(control_cases)} outcome controls')


def observe(executable, args, log, timeout, target_environment=None):
    result_command = ("script import lldb,json,au_teardown_lldb; p=lldb.debugger.GetSelectedTarget().GetProcess(); "
        "print('"+MARKER+"'+json.dumps({'state': 'stopped' if p.GetState() in (lldb.eStateStopped,lldb.eStateCrashed) else "
        "'exited' if p.GetState()==lldb.eStateExited else 'other', 'exit':p.GetExitStatus(), "
        "'stops':[t.GetStopDescription(256) for t in p], 'target_exception':au_teardown_lldb.seen_bad_call, "
        "'exception_type':au_teardown_lldb.exception_type}))")
    backtrace = ("script lldb.debugger.HandleCommand('thread backtrace all') if "
        "lldb.debugger.GetSelectedTarget().GetProcess().GetState() in (lldb.eStateStopped,lldb.eStateCrashed) else None")
    command = ['xcrun','lldb','--batch','-o','settings set target.disable-aslr false',
               '-o','command script import '+shlex.quote(str(Path(__file__).with_name('au_teardown_lldb.py').resolve())),
               '-o','script au_teardown_lldb.install(lldb.debugger.GetSelectedTarget())',
               '-o','process handle -s true -n true -p false SIGABRT','-o','run',
               '-o',result_command,'-o',backtrace,'-o','image list -o -f',
               '-k',result_command,'-k',backtrace,'-k','image list -o -f','--',str(executable),*args]
    if target_environment:
        # Set inferior environment through LLDB: a protected xcrun/lldb launcher
        # can strip DYLD_* from its own inherited process environment.
        command[3:3]=['-o','settings set target.env-vars '+shlex.join(
            key+'='+value for key,value in target_environment.items())]
    with log.open('w') as output:
        process = subprocess.Popen(command,stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
        try:
            debugger_exit = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid,signal.SIGKILL)
            process.wait()
            return {'outcome':'observer-timeout','command':command}
    return {'outcome':classify(log.read_text()),'debugger_exit':debugger_exit,'command':command}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--pluginval',type=Path)
    parser.add_argument('--bundle',type=Path)
    parser.add_argument('--apple-control',type=Path,help='Independent Apple-only listener control executable')
    parser.add_argument('--pending-control',type=Path,help='Independent queued-framework-block disposal control')
    parser.add_argument('--pending-library',type=Path,help='Interposition library linked by the independent pending-block control')
    parser.add_argument('--au-source-commit',help='Historical source commit that produced the retained AU')
    parser.add_argument('--report-dir',type=Path,default=Path('au-teardown-evidence'))
    parser.add_argument('--attempts',type=int,default=16)
    parser.add_argument('--expect',choices=('capture','completed'),default='capture')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.au_source_commit or not re.fullmatch(r'[0-9a-f]{40}',args.au_source_commit):
        parser.error('--au-source-commit must identify the retained AU source with a full commit SHA')
    assert platform.system() == 'Darwin' and platform.machine() == 'x86_64'
    assert subprocess.run(['sysctl','-n','sysctl.proc_translated'],capture_output=True,text=True).stdout.strip() in ('','0')
    assert args.pluginval and args.pluginval.is_file() and args.bundle and args.bundle.is_dir()
    au_binary=args.bundle/'Contents/MacOS/Anamorph'
    assert au_binary.is_file()
    assert 1 <= args.attempts <= 20
    root=args.report_dir.resolve();root.mkdir(parents=True,exist_ok=True)
    canary=root/'canary.cpp'
    canary.write_text('#include <functional>\nint main() { std::function<void()> empty; empty(); }\n')
    subprocess.run(['xcrun','clang++','-std=c++17','-g',str(canary),'-o',str(root/'canary')],check=True)
    control=observe(root/'canary',[],root/'canary-lldb.log',60)
    report={'intent':'capture a throw stack; not a pluginval conformance gate',
            'diagnostic_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
            'au_source_commit':args.au_source_commit,
            'au_binary_sha256':hashlib.sha256(au_binary.read_bytes()).hexdigest(),
            'pluginval_sha256':hashlib.sha256(args.pluginval.read_bytes()).hexdigest(),
            'canary':control,'runs':[]}
    def save():
        (root/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    save()
    assert control['outcome']=='bad-function-call-throw', 'debugger cannot capture the known first throw'
    control_threw=False
    if args.pending_control:
        assert args.pending_library and args.pending_library.is_file()
        pending_environment={'DYLD_INSERT_LIBRARIES':str(args.pending_library.resolve())}
        report['pending_control_library_sha256']=hashlib.sha256(args.pending_library.read_bytes()).hexdigest()
        negative_log=root/'apple-pending-disarmed.log'
        negative=observe(args.pending_control.resolve(),['--no-enqueue-hook'],negative_log,60,pending_environment)
        report['pending_control_disarmed']=negative
        save()
        records=re.findall(r'^'+MARKER+r'(\{.*\})$',negative_log.read_text(),re.M)
        assert negative['outcome']=='inferior-failed' and negative.get('debugger_exit')==0 \
            and len(records)==1 and json.loads(records[0])['exit']==5, \
            'pending-block control accepted an unobserved enqueue/disposal path'
        log=root/'apple-pending.log'
        result=observe(args.pending_control.resolve(),[],log,60,pending_environment)
        if result['outcome']!='observer-timeout':
            result['outcome']=classify_apple_control(log.read_text())
            if result['outcome']=='completed' and result['debugger_exit']!=0:
                result['outcome']='observer-error'
        report['pending_control']=result
        save()
        print(f"Apple-only pending-block control: {result['outcome']}",flush=True)
        if result['outcome'] not in ('completed','bad-function-call-throw'):
            return 2
        control_threw |= result['outcome']=='bad-function-call-throw'
    if args.apple_control:
        report['apple_controls']=[]
        for mode,control_args in [('entry',['--entry-disposal','--cycles','128']),
                                 ('drained',['--cycles','128']),('queued',['--queued-disposal','--cycles','128'])]:
            log=root/f'apple-{mode}.log'
            result=observe(args.apple_control.resolve(),control_args,log,180)
            if result['outcome']!='observer-timeout':
                result['outcome']=classify_apple_control(log.read_text())
                if result['outcome']=='completed' and result['debugger_exit']!=0:
                    result['outcome']='observer-error'
            report['apple_controls'].append({'mode':mode,**result})
            save()
            print(f"Apple-only {mode} control: {result['outcome']}",flush=True)
            if result['outcome'] not in ('completed','bad-function-call-throw'):
                return 2
            control_threw |= result['outcome']=='bad-function-call-throw'
    deadline=time.monotonic()+1500
    for attempt in range(args.attempts):
        remaining=deadline-time.monotonic()
        if remaining < 180:
            break
        outcome=observe(args.pluginval.resolve(),['--strictness-level','10','--randomise',
            '--random-seed','0x3e1c70','--timeout-ms','600000','--validate',str(args.bundle.resolve())],
            root/f'pass-{attempt+1:02d}.log',min(180,remaining))
        report['runs'].append(outcome);save()
        print(f"AU diagnostic {attempt+1}: {outcome['outcome']}",flush=True)
        if outcome['outcome']=='bad-function-call-throw':
            return 0 if args.expect=='capture' else 1
        if outcome['outcome']!='completed':
            return 2
    if args.expect=='completed' and len(report['runs'])==args.attempts:
        return 1 if control_threw else 0
    return 3  # bounded campaign did not meet its declared expectation


if __name__=='__main__':
    sys.exit(main())
