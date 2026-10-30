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
        if 'SIGABRT' in str(result['stops']) and 'bad_function_call' in output:
            return 'bad-function-call-abort'
        return 'other-stop'
    if result['state'] != 'exited':
        return 'observer-error'
    if result['exit'] != 0:
        return 'inferior-failed'
    return 'completed' if re.search(r'^SUCCESS\s*$', output, re.M) else 'missing-success'


def self_test():
    def record(state, exit_code=0, stops=None):
        return MARKER+json.dumps({'state':state,'exit':exit_code,'stops':stops or []})+'\n'
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
    ]
    for text, expected in cases:
        assert classify(text) == expected, (text, expected)
    print(f'AU teardown observer self-test: {len(cases)} outcome controls')


def observe(executable, args, log, timeout):
    result_command = ("script import lldb,json; p=lldb.debugger.GetSelectedTarget().GetProcess(); "
        "print('"+MARKER+"'+json.dumps({'state': 'stopped' if p.GetState() in (lldb.eStateStopped,lldb.eStateCrashed) else "
        "'exited' if p.GetState()==lldb.eStateExited else 'other', 'exit':p.GetExitStatus(), "
        "'stops':[t.GetStopDescription(256) for t in p]}))")
    backtrace = ("script lldb.debugger.HandleCommand('thread backtrace all') if "
        "lldb.debugger.GetSelectedTarget().GetProcess().GetState() in (lldb.eStateStopped,lldb.eStateCrashed) else None")
    command = ['xcrun','lldb','--batch','-o','settings set target.disable-aslr false',
               '-o','process handle -s true -n true -p false SIGABRT','-o','run',
               '-o',result_command,'-o',backtrace,'-o','image list -o -f',
               '-k',result_command,'-k',backtrace,'-k','image list -o -f','--',str(executable),*args]
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
    parser.add_argument('--report-dir',type=Path,default=Path('au-teardown-evidence'))
    parser.add_argument('--attempts',type=int,default=16)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    assert platform.system() == 'Darwin' and platform.machine() == 'x86_64'
    assert subprocess.run(['sysctl','-n','sysctl.proc_translated'],capture_output=True,text=True).stdout.strip() in ('','0')
    assert args.pluginval and args.pluginval.is_file() and args.bundle and args.bundle.is_dir()
    assert 1 <= args.attempts <= 20
    root=args.report_dir.resolve();root.mkdir(parents=True,exist_ok=True)
    canary=root/'canary.cpp'
    canary.write_text('#include <functional>\nint main() { std::function<void()> empty; empty(); }\n')
    subprocess.run(['xcrun','clang++','-std=c++17','-g',str(canary),'-o',str(root/'canary')],check=True)
    control=observe(root/'canary',[],root/'canary-lldb.log',60)
    report={'intent':'capture a throw stack; not a pluginval conformance gate',
            'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
            'pluginval_sha256':hashlib.sha256(args.pluginval.read_bytes()).hexdigest(),
            'canary':control,'runs':[]}
    def save():
        (root/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    save()
    assert control['outcome']=='bad-function-call-abort', 'debugger cannot capture the known throw'
    deadline=time.monotonic()+1000
    for attempt in range(args.attempts):
        remaining=deadline-time.monotonic()
        if remaining < 180:
            break
        outcome=observe(args.pluginval.resolve(),['--strictness-level','10','--randomise',
            '--random-seed','0x3e1c70','--timeout-ms','600000','--validate',str(args.bundle.resolve())],
            root/f'pass-{attempt+1:02d}.log',min(180,remaining))
        report['runs'].append(outcome);save()
        print(f"AU diagnostic {attempt+1}: {outcome['outcome']}",flush=True)
        if outcome['outcome']=='bad-function-call-abort':
            return 0
        if outcome['outcome']!='completed':
            return 2
    return 3  # bounded campaign did not reproduce: never claim a fix


if __name__=='__main__':
    sys.exit(main())
