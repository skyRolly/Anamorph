#!/usr/bin/env python3
"""Three harmless ordinary-user Linux process cases; no Rosetta or debugger."""
import argparse
import ctypes
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('rosetta_cleanup_driver', ROOT / 'scripts/diagnose-rosetta-scope.py')
driver = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = driver
spec.loader.exec_module(driver)

# Only these fixture processes are created. The third leader exits, leaving
# one same-group child. The harness adopts that known child to prevent a zombie
# leaking to an unrelated init. PR_SET_CHILD_SUBREAPER is process-local, not a
# host security/permission setting, and the original value is restored.
FIXTURE = r'''
import json, os, signal, sys, time
from pathlib import Path
root=Path(sys.argv[1]); leader=os.getpid(); group=os.getpgrp()
if sys.argv[2]=='leader_exit':
    child=os.fork()
    if child:
        (root/'identity.json').write_text(json.dumps({'leader':leader,'child':child,'pgid':group}))
        until=time.monotonic()+1
        while not (root/'ready').exists() and time.monotonic()<until: time.sleep(0.005)
        os._exit(0)
else:
    (root/'identity.json').write_text(json.dumps({'leader':leader,'child':leader,'pgid':group}))
signal.signal(signal.SIGTERM, signal.SIG_IGN)
(root/'ready').write_text('ready')
while True: time.sleep(1)
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    started = time.monotonic(); deadline = started + 20
    report = {'kind': 'real ordinary-user Linux POSIX cleanup, not native macOS debugging',
              'platform': platform.platform(), 'python': sys.version, 'uid': os.getuid(),
              'consumer_head': os.environ.get('GITHUB_SHA'),
              'suite_budget_seconds': 20, 'maximum_cases': 3, 'cases': [],
              'source_sha256': hashlib.sha256((ROOT / 'scripts/diagnose-rosetta-scope.py').read_bytes()).hexdigest(),
              'fixture_sha256': hashlib.sha256(FIXTURE.encode()).hexdigest(),
              'host_security_settings_changed': False, 'status': 'in_progress'}
    libc = None; old_reaper = ctypes.c_int(); reaper_set = False

    def save():
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')

    try:
        if sys.platform != 'linux' or os.getuid() == 0:
            raise RuntimeError('requires an ordinary-user Linux host with process-local child adoption')
        libc = ctypes.CDLL(None, use_errno=True)
        if libc.prctl(37, ctypes.byref(old_reaper), 0, 0, 0) != 0:
            raise OSError(ctypes.get_errno(), 'PR_GET_CHILD_SUBREAPER')
        if libc.prctl(36, 1, 0, 0, 0) != 0:
            raise OSError(ctypes.get_errno(), 'PR_SET_CHILD_SUBREAPER for this fixture harness')
        reaper_set = True
        report['fixture_process_local_subreaper'] = {'pid': os.getpid(), 'original': old_reaper.value,
            'scope': 'this test harness only; reaps only recorded fixture child PIDs'}
        save()
        for mode in ['initial_observation_failure', 'escalation_observation_failure', 'leader_exit']:
            if deadline - time.monotonic() < 5:
                raise RuntimeError('insufficient remaining suite budget; no new case started')
            case_started = time.monotonic()
            case = {'case': mode, 'fixture_processes_created': 2 if mode == 'leader_exit' else 1,
                    'independent_finally_cleanup': {}, 'status': 'in_progress'}
            report['cases'].append(case)
            with tempfile.TemporaryDirectory(prefix='rosetta-cleanup-process-') as private:
                root = Path(private); process = None; identity = None; adopted_status = None
                original_probe = driver.group_members

                def reap_adopted(until):
                    nonlocal adopted_status
                    if identity is None or identity['child'] == identity['leader'] or adopted_status is not None:
                        return
                    while True:
                        try:
                            pid, status = os.waitpid(identity['child'], os.WNOHANG)
                        except ChildProcessError:
                            return
                        if pid:
                            adopted_status = os.waitstatus_to_exitcode(status)
                            return
                        if time.monotonic() >= until:
                            return
                        time.sleep(min(0.005, max(0, until - time.monotonic())))

                try:
                    with (root/'fixture.log').open('wb') as output:
                        case['fixture_argv'] = [sys.executable, '-c', FIXTURE, private,
                            'leader_exit' if mode == 'leader_exit' else 'single']
                        process = subprocess.Popen(case['fixture_argv'],
                            stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
                        # This identity comes from our new-session Popen, not ps.
                        pgid = process.pid
                        if pgid <= 1 or pgid == os.getpgrp():
                            raise RuntimeError('invalid fixture invocation group')
                        until = min(time.monotonic()+2, deadline-3)
                        while not (root/'ready').exists():
                            if time.monotonic() >= until:
                                raise RuntimeError('fixture readiness deadline exhausted')
                            time.sleep(0.005)
                        identity = json.loads((root/'identity.json').read_text())
                        assert identity['leader'] == pgid == identity['pgid']
                        assert os.getpgid(identity['child']) == pgid
                        case['owned_identities'] = identity
                        if mode == 'leader_exit':
                            assert process.wait(timeout=min(1, max(0, deadline-time.monotonic()-3))) == 0
                        case['leader_exit_before_cleanup'] = process.poll()
                        calls = [0]

                        def injected_probe(group, timeout):
                            assert group == pgid
                            calls[0] += 1
                            if mode == 'initial_observation_failure' and calls[0] == 1:
                                raise RuntimeError('injected initial ps nonzero exit')
                            if mode == 'escalation_observation_failure' and calls[0] == 2:
                                raise subprocess.TimeoutExpired(['ps'], timeout)
                            # Fixture-only ownership/reaping, not a replacement for
                            # production group observation or group termination.
                            observed_at = time.monotonic()
                            reap_adopted(min(observed_at+0.3, deadline-1))
                            available = timeout-(time.monotonic()-observed_at)
                            if available <= 0:
                                raise subprocess.TimeoutExpired(['ps'], timeout)
                            return original_probe(group, available)

                        driver.group_members = injected_probe
                        cleanup_deadline = min(time.monotonic()+driver.CLEANUP_SECONDS, deadline-2)
                        result = driver.cleanup_process(process, pgid, cleanup_deadline)
                        case['cleanup'] = result
                        if mode != 'leader_exit':
                            stage = 0 if mode == 'initial_observation_failure' else 1
                            observed = result['observations'][stage]
                            assert observed['status'] == 'error', observed
                            assert ('injected' in observed['error'] if stage == 0
                                    else 'TimeoutExpired' in observed['error']), observed
                        attempts = [x['signal'] for x in result['signal_attempts']]
                        assert attempts == ['SIGTERM', 'SIGKILL'], attempts
                        assert result['child_reaped'] and result['group_state'] == 'empty', result
                        if mode == 'leader_exit':
                            assert result['exit_code'] == 0 and adopted_status == -signal.SIGKILL
                            assert result['cleanup_complete'] is True
                        else:
                            assert result['exit_code'] == -signal.SIGKILL
                            assert result['cleanup_complete'] is False and result['cleanup_error']
                            assert driver.process_ok(dict(result, timed_out=True)) is False
                        case['fixture_adopted_child_exit'] = adopted_status
                        case['status'] = 'verified'
                except Exception as error:
                    case['error'] = type(error).__name__ + ': ' + str(error)
                    case['status'] = 'failure'
                finally:
                    driver.group_members = original_probe
                    # Independent bounded cleanup, including if production cleanup
                    # is broken/no-op. No broad PID discovery or name-based kill.
                    final = case['independent_finally_cleanup']
                    if process is not None:
                        try:
                            need_cleanup = process.poll() is None or (identity is not None
                                and identity['child'] != identity['leader'] and adopted_status is None)
                            if need_cleanup:
                                try:
                                    os.killpg(process.pid, signal.SIGKILL)
                                    final['group_KILL'] = 'sent'
                                except ProcessLookupError:
                                    final['group_KILL'] = 'disappeared'
                            try:
                                process.wait(timeout=min(0.5, max(0, deadline-time.monotonic())))
                                final['leader_reaped'] = True
                            except subprocess.TimeoutExpired:
                                final['leader_reaped'] = False
                            reap_adopted(min(time.monotonic()+0.5, deadline))
                            final['adopted_child_reaped'] = identity is None or identity['child']==identity['leader'] or adopted_status is not None
                            available = min(0.5, max(0, deadline-time.monotonic()))
                            final['surviving_group_members'] = original_probe(process.pid, available) if available > 0 else None
                        except Exception as error:
                            final['error'] = type(error).__name__ + ': ' + str(error)
                    case['elapsed_seconds'] = round(time.monotonic()-case_started, 6)
                    save()
            if (case['status'] != 'verified' or not final.get('leader_reaped')
                    or not final.get('adopted_child_reaped') or final.get('surviving_group_members') != []):
                raise RuntimeError(mode + ' did not complete with owned fixtures reaped')
        report['status'] = 'verified'
    except Exception as error:
        report['error'] = type(error).__name__ + ': ' + str(error)
        report['status'] = 'failure'
    finally:
        if reaper_set:
            restored = libc.prctl(36, old_reaper.value, 0, 0, 0) == 0
            report['fixture_process_local_subreaper']['restored'] = restored
            if not restored:
                report['status'] = 'failure'; report['restore_error'] = ctypes.get_errno()
        report['elapsed_seconds'] = round(time.monotonic()-started, 6)
        if report['elapsed_seconds'] > 20:
            report['status'] = 'failure'; report['suite_deadline_exceeded'] = True
        save()
    if report['status'] != 'verified':
        print(json.dumps(report), file=sys.stderr)
    return 0 if report['status'] == 'verified' else 1


if __name__ == '__main__':
    raise SystemExit(main())
