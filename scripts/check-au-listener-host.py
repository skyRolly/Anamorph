#!/usr/bin/env python3
"""Verify native AU host routing and real callback failure; no crash is excused."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess

LOADED = 'ANAMORPH_AU_LISTENER_ADAPTER=loaded'
ROUTED = 'ANAMORPH_AU_LISTENER_ADAPTER=dispatch-main'


def routed(text):
    lines = text.splitlines()
    return LOADED in lines and ROUTED in lines


def callback_crashed(code, text):
    return code == -signal.SIGABRT and LOADED in text.splitlines() and 'bad_function_call' in text


def no_cf_delivery(text):
    return routed(text) and re.findall(r'^ANAMORPH_AU_LISTENER_FRAMEWORK_CF=(\d+)$',text,re.M)==['0']


def self_test():
    good = LOADED+'\n'+ROUTED+'\n'
    for text, expected in [(good, True), ('SUCCESS\n', False), (LOADED+'\n', False),
                           (ROUTED+'\n', False), ('prefix '+good, False)]:
        assert routed(text) is expected
    for code,text,expected in [(-signal.SIGABRT,LOADED+'\nbad_function_call',True),
        (0,LOADED+'\nbad_function_call',False),(-signal.SIGSEGV,LOADED+'\nbad_function_call',False),
        (-signal.SIGABRT,'bad_function_call',False),(-signal.SIGABRT,LOADED,False)]:
        assert callback_crashed(code,text) is expected
    for text,expected in [(good+'ANAMORPH_AU_LISTENER_FRAMEWORK_CF=0\n',True),
        (good,False),(good+'ANAMORPH_AU_LISTENER_FRAMEWORK_CF=1\n',False),
        (good+'ANAMORPH_AU_LISTENER_FRAMEWORK_CF=0\n'*2,False)]:
        assert no_cf_delivery(text) is expected
    print('AU host checker self-test: 14 routing/failure controls')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--verify-log',type=Path)
    parser.add_argument('--require-no-cf',action='store_true')
    parser.add_argument('--library',type=Path)
    parser.add_argument('--control',type=Path)
    parser.add_argument('--report-dir',type=Path,default=Path('au-host-evidence'))
    args = parser.parse_args()
    if args.self_test: self_test(); return 0
    if args.verify_log:
        text=args.verify_log.read_text()
        if not (no_cf_delivery(text) if args.require_no_cf else routed(text)):
            raise RuntimeError('Native AU validator did not load and exercise the listener adapter')
        return 0
    assert platform.system() == 'Darwin' and platform.machine() == 'x86_64'
    assert args.library and args.library.is_file() and args.control and args.control.is_file()
    assert subprocess.run(['sysctl','-n','sysctl.proc_translated'],capture_output=True,text=True).stdout.strip() in ('','0')
    root=args.report_dir; root.mkdir(parents=True,exist_ok=True)
    environment=os.environ.copy(); environment.pop('DYLD_INSERT_LIBRARIES',None)
    adapted={**environment,'DYLD_INSERT_LIBRARIES':str(args.library.resolve()),'ANAMORPH_AU_LISTENER_MODE':'dispatch'}
    results=[]
    for label,arguments,env in [('missing', ['--require-adapter'],environment),
        ('baseline',[],environment),('adapted',['--require-adapter'],adapted),
        ('callback-crash',['--require-adapter','--throw-in-callback'],adapted)]:
        process=subprocess.run([str(args.control.resolve()),*arguments],env=env,
            stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=30)
        output=process.stdout; (root/(label+'.log')).write_text(output)
        results.append({'case':label,'exit':process.returncode})
        if label=='missing': assert process.returncode==6, 'absent adapter accepted'
        elif label=='baseline': assert process.returncode==0, output
        elif label=='adapted': assert process.returncode==0 and routed(output), output
        else: assert callback_crashed(process.returncode,output), 'callback exception was not a genuine fatal bad_function_call: '+output
    report={'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        'library_sha256':hashlib.sha256(args.library.read_bytes()).hexdigest(),
        'control_sha256':hashlib.sha256(args.control.read_bytes()).hexdigest(),'results':results}
    (root/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('AU host API/lifetime and genuine callback-crash controls verified')
    return 0


if __name__=='__main__': raise SystemExit(main())
