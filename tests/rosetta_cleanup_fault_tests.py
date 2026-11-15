#!/usr/bin/env python3
"""Exercise owned-group finalization when fixture identity is unavailable."""
import ctypes
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

from rosetta_cleanup_handshake_tests import wait_for
from rosetta_cleanup_process_tests import driver, independent_finally_cleanup


FIXTURE = r'''
import json, os, signal, sys, time
from pathlib import Path
root=Path(sys.argv[1]); mode=sys.argv[2]; leader=os.getpid(); pgid=os.getpgrp()
child=os.fork()
if not child:
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    (root/'control-child.tmp').write_text(str(os.getpid()))
    (root/'control-child.tmp').replace(root/'control-child')
    if mode != 'child_timeout': (root/'ready').write_text('ready')
    while True: time.sleep(1)
until=time.monotonic()+3
while not (root/'control-child').exists() and time.monotonic()<until: time.sleep(0.005)
if not (root/'control-child').exists(): raise RuntimeError('child did not start')
identity=root/'identity.json'
if mode == 'publication_failure':
    identity.mkdir(); identity.write_text('unreachable')
elif mode == 'malformed': identity.write_text('{')
elif mode == 'unreadable':
    identity.write_text(json.dumps({'leader':leader,'child':child,'pgid':pgid})); identity.chmod(0)
elif mode != 'missing': identity.write_text(json.dumps({'leader':leader,'child':child,'pgid':pgid}))
if mode in ('held', 'child_timeout'):
    while True: time.sleep(1)
os._exit(0)
'''


def run_case(mode, failure=None):
    with tempfile.TemporaryDirectory(prefix='rosetta-cleanup-fault-') as private:
        root = Path(private)
        proc = subprocess.Popen([sys.executable, '-c', FIXTURE, private, mode],
                                start_new_session=True, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        first = None
        try:
            wait_for(root/'control-child', time.monotonic()+3)
            child = int((root/'control-child').read_text())
            assert os.getpgid(child) == proc.pid
            if mode == 'child_timeout':
                try:
                    wait_for(root/'ready', time.monotonic()+0.05)
                except TimeoutError:
                    pass
                else:
                    raise AssertionError('child readiness timeout did not execute')
            elif mode == 'held':
                try:
                    wait_for(root/'observer-never-ready', time.monotonic()+0.05)
                except TimeoutError:
                    pass
                else:
                    raise AssertionError('observer timeout did not execute')
            else:
                proc.wait(timeout=3)
                # Simulate the observer's unavailable/not-yet-adopted identity.
                # The child marker is test instrumentation, never a cleanup input.
                identity = None
                if mode in ('missing', 'malformed', 'unreadable', 'publication_failure'):
                    try:
                        json.loads((root/'identity.json').read_text())
                    except (OSError, ValueError):
                        pass
                    else:
                        raise AssertionError('identity fault was not exercised')
                assert proc.poll() is not None and identity is None
                # This is exactly the former finalizer predicate and success
                # shortcut. Show the still-owned child before safely cleaning it.
                legacy_need_cleanup = proc.poll() is None or (identity is not None
                    and identity['child'] != identity['leader'])
                legacy_claim_reaped = identity is None
                members = driver.group_members(proc.pid, 1)
                assert not legacy_need_cleanup and legacy_claim_reaped and members
            if failure == 'signal':
                def reject_signal(*unused):
                    raise PermissionError('injected fixture signal failure')
                first = independent_finally_cleanup(proc, time.monotonic()+0.15,
                                                    kill_group=reject_signal)
            elif failure == 'reap':
                def reject_wait(*unused):
                    raise OSError('injected fixture wait failure')
                first = independent_finally_cleanup(proc, time.monotonic()+1,
                                                    wait_group=reject_wait)
            elif failure == 'observation':
                def reject_observation(*unused):
                    raise RuntimeError('injected fixture observation failure')
                first = independent_finally_cleanup(proc, time.monotonic()+1,
                                                    group_probe=reject_observation)
            if failure:
                assert first and not first['complete'] and first['errors'], first
        finally:
            result = independent_finally_cleanup(proc, time.monotonic()+2)
            if not result['complete']:
                raise RuntimeError('fault fixture cleanup failed: ' + repr(result))
        return mode + (':' + failure if failure else '')


def main():
    if sys.platform != 'linux' or os.getuid() == 0:
        raise RuntimeError('requires an ordinary-user Linux host')
    libc = ctypes.CDLL(None, use_errno=True)
    old = ctypes.c_int()
    if libc.prctl(37, ctypes.byref(old), 0, 0, 0) != 0 or libc.prctl(36, 1, 0, 0, 0) != 0:
        raise OSError(ctypes.get_errno(), 'fixture child-subreaper setup')
    try:
        cases = [run_case(mode) for mode in ('missing', 'malformed', 'unreadable',
                 'publication_failure', 'leader_exit', 'held', 'child_timeout')]
        cases += [run_case('held', failure) for failure in ('signal', 'reap', 'observation')]
        print('cleanup fixture fault controls: ' + ', '.join(cases))
    finally:
        if libc.prctl(36, old.value, 0, 0, 0) != 0:
            raise OSError(ctypes.get_errno(), 'fixture child-subreaper restore')


if __name__ == '__main__':
    main()
