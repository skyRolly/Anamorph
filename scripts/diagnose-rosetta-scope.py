#!/usr/bin/env python3
"""Bounded nonshipping native characterization, or inert --self-test controls."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import runpy
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

SOURCE_HEAD = '14bde706e2d121c9970cf00c757cb12d3178b9a5'
# Identical to the normal Build Rosetta prerequisite, including compiler/flags.
AVX2_SOURCE = '#include <immintrin.h>\nint main(void){__m256i a=_mm256_set1_epi32(1);a=_mm256_add_epi32(a,a);return _mm256_extract_epi32(a,0)-2;}\n'
COMPLETION = re.compile(r'^SCOPE_DIAGNOSTIC_COMPLETE counts=([01]) frames=([01]) straddles=(\d+) clamp=([01]) oversized=([01])\s*$', re.M)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class PrerequisiteError(RuntimeError):
    def __init__(self, classification, message):
        super().__init__(message)
        self.classification = classification


def process_ok(result):
    return (result.get('exit_code') == 0 and not result.get('timed_out')
            and not result.get('launch_error') and not result.get('budget_exhausted')
            and not result.get('execution_wait_error')
            and not result.get('surviving_processes') and not result.get('cleanup_error')
            and result.get('cleanup_complete', True))


def focused_outcome(result, output):
    """Exit zero alone and an LLDB wrapper's status never certify the fixture."""
    matches = list(COMPLETION.finditer(output))
    markers = output.count('SCOPE_DIAGNOSTIC_COMPLETE')
    mismatch = [line for line in output.splitlines() if 'SCOPE_MISMATCH_BITS' in line]
    fields = None
    if len(matches) == 1 and markers == 1:
        fields = dict(zip(('counts', 'frames', 'straddles', 'clamp', 'oversized'),
                          map(int, matches[0].groups())))
    assertions_ok = fields is not None and all(fields[k] == 1 for k in
                    ('counts', 'frames', 'clamp', 'oversized')) and fields['straddles'] > 0
    # A mismatch remains evidence even if a wrapper claims success or times out.
    denied = bool(re.search(r'(?im)^.*(?:error:.*(?:not permitted|permission denied)|process launch failed.*(?:denied|not allowed))', output))
    if denied:
        outcome = 'permission_denied'
    elif result.get('timed_out'):
        outcome = 'timeout'
    elif result.get('launch_error') or result.get('budget_exhausted'):
        outcome = 'not_completed'
    elif mismatch:
        outcome = 'mismatch_captured'
    elif fields is None:
        outcome = 'missing_or_invalid_completion'
    elif not assertions_ok:
        outcome = 'assertion_failure'
    elif not process_ok(result):
        outcome = 'execution_failure'
    else:
        outcome = 'bounded_nonreproduction'
    return {'outcome': outcome, 'completion': fields, 'assertions_ok': assertions_ok,
            'first_mismatch': mismatch[0] if mismatch else None,
            'mismatch_observed': bool(mismatch), 'completion_valid': fields is not None}


def focused_flow(command, record, probe_source, build_focus, read_log, save,
                 verifier=focused_outcome, debugger_requested=True):
    """Production selection boundary; inert controls replace commands, never hosts."""
    record['focused_direct'] = {'outcome': 'not_started'}
    record['focused_debugger'] = {'outcome': 'not_requested' if not debugger_requested else 'not_started'}
    basic = command('rosetta', ['/usr/bin/arch', '-x86_64', '/usr/bin/true'], timeout=20)
    if not process_ok(basic):
        raise PrerequisiteError('execution_prerequisite_unmet', 'basic Rosetta translation did not complete')
    probe = probe_source.with_suffix('')
    record['avx2_probe'] = {'source_sha256': sha(probe_source), 'compiler_selection': 'cc',
                            'flags': ['-arch', 'x86_64', '-march=haswell', '-O0'],
                            'ROSETTA_ADVERTISE_AVX': '1', 'binary_sha256': None}
    compiled = command('avx2-compile', ['cc', '-arch', 'x86_64', '-march=haswell', '-O0',
                                       str(probe_source), '-o', str(probe)], timeout=60)
    if not process_ok(compiled):
        raise PrerequisiteError('build_prerequisite_failed', 'AVX2 probe compilation failed; translation support is unknown')
    if not probe.is_file():
        raise PrerequisiteError('build_prerequisite_failed', 'AVX2 compiler produced no probe executable')
    record['avx2_probe']['binary_sha256'] = sha(probe)
    executed = command('avx2-execution', ['/usr/bin/arch', '-x86_64', str(probe)], timeout=20)
    if not process_ok(executed):
        raise PrerequisiteError('execution_prerequisite_unmet', 'AVX2 executable prerequisite did not complete successfully')
    record['avx2_probe']['execution_prerequisite_met'] = True
    save()
    focus = build_focus()
    digest = sha(focus)
    direct = command('focused-direct', ['/usr/bin/arch', '-x86_64', str(focus)], timeout=60)
    direct.update(verifier(direct, read_log('focused-direct')))
    direct['binary_sha256'] = digest
    record['focused_direct'] = direct
    save()
    if direct['outcome'] == 'permission_denied':
        raise PrerequisiteError('execution_permission_denied', 'explicit execution denial; no debugger bypass attempted')
    if direct.get('surviving_processes') or direct.get('cleanup_error'):
        raise PrerequisiteError('cleanup_incomplete', 'test-owned process cleanup incomplete; later execution withheld')
    if debugger_requested:
        record['debugger_question'] = ('Capture the first focused mismatch at scope_mismatch.'
            if direct['mismatch_observed'] else
            'Does translated LLDB launch complete after the retained timeout, compared with direct execution?')
        commands = probe_source.parent / 'lldb.commands'
        commands.write_text('''settings set target.disable-aslr false
breakpoint set -n scope_mismatch
breakpoint command add 1 -o "thread backtrace all" -o "register read" -o "frame variable" -o "disassemble --frame" -o "image list -o -f" -o "continue"
run
''')
        debug = command('focused-debugger', ['xcrun', 'lldb', '--arch', 'x86_64', '--batch',
                                            '-s', str(commands), '--', str(focus)], timeout=120)
        debug.update(verifier(debug, read_log('focused-debugger')))
        debug['binary_sha256'] = digest
        record['focused_debugger'] = debug
        save()
    return (direct['outcome'] == 'bounded_nonreproduction'
            and (not debugger_requested or record['focused_debugger']['outcome'] == 'bounded_nonreproduction'))



def verify_dispatch_route(workflow, selector):
    events = workflow.split('\non:\n', 1)[1].split('\npermissions:', 1)[0]
    if re.findall(r'^  ([a-z_]+):', events, re.M) != ['workflow_dispatch']:
        raise AssertionError('diagnostic route must be dispatch-only')
    if '--ordinary-runs 0' not in workflow or '--ordinary-runs 3' in workflow:
        raise AssertionError('diagnostic route must request zero ordinary invocations')
    for event, branch, repo, expected in [
        ('workflow_dispatch', 'codex/rosetta-scope-characterization', 'skyRolly/Anamorph', ['characterize']),
        ('push', 'codex/rosetta-scope-characterization', 'skyRolly/Anamorph', []),
        ('pull_request', 'codex/rosetta-scope-characterization', 'skyRolly/Anamorph', []),
        ('workflow_dispatch', 'main', 'skyRolly/Anamorph', []),
        ('workflow_dispatch', 'codex/unrelated', 'skyRolly/Anamorph', []),
        ('workflow_dispatch', 'codex/rosetta-scope-characterization', 'fork/Anamorph', [])]:
        selected = selector(workflow, {'github.event_name': event, 'github.ref': 'refs/heads/' + branch,
                                       'github.repository': repo})
        if selected != expected:
            raise AssertionError('diagnostic event/ref selection rejected: ' + event + '/' + branch)


CLEANUP_SECONDS = 6


def group_members(pgid, timeout):
    """Observe only the already-owned invocation group; failure is unknown state."""
    table = subprocess.run(['ps', '-axo', 'pid=,pgid=,stat=,command='],
                           capture_output=True, text=True, timeout=timeout, check=False)
    if table.returncode:
        raise RuntimeError('ps could not inspect the owned group: exit ' + str(table.returncode))
    return [line.strip() for line in table.stdout.splitlines()
            if len(line.split(None, 3)) >= 3 and line.split(None, 3)[1] == str(pgid)]


def cleanup_process(process, pgid, deadline):
    """Bounded cleanup of a Popen created with start_new_session=True, only."""
    started = time.monotonic()
    result = {'invocation_pgid': pgid, 'signal_attempts': [], 'reaping': [],
              'observations': [], 'surviving_processes': None, 'group_state': 'unknown',
              'child_reaped': False, 'cleanup_complete': False}
    errors = []

    def remaining(reserve=0):
        return max(0, deadline - time.monotonic() - reserve)

    def observe(stage, reserve):
        limit = min(1, remaining(reserve))
        entry = {'stage': stage, 'timeout_seconds': limit, 'members': None}
        if limit <= 0:
            entry.update(status='not_observed', error='cleanup observation deadline exhausted')
        else:
            try:
                entry.update(status='observed', members=group_members(pgid, limit))
            except Exception as error:
                entry.update(status='error', error=type(error).__name__ + ': ' + str(error))
        result['observations'].append(entry)
        if 'error' in entry:
            errors.append(stage + ': ' + entry['error'])
        return entry['members']

    def send(sig):
        entry = {'target': 'invocation_group', 'pgid': pgid, 'signal': signal.Signals(sig).name}
        try:
            os.killpg(pgid, sig)
            entry['outcome'] = 'sent'
        except ProcessLookupError:
            entry['outcome'] = 'disappeared'
        except Exception as error:
            entry.update(outcome='error', error=type(error).__name__ + ': ' + str(error))
            errors.append(entry['signal'] + ': ' + entry['error'])
        result['signal_attempts'].append(entry)

    def reap(stage, cap, reserve):
        limit = min(cap, remaining(reserve))
        entry = {'stage': stage, 'timeout_seconds': limit}
        try:
            code = process.wait(timeout=limit)
            entry.update(outcome='reaped', exit_code=code)
            result['child_reaped'] = True
        except subprocess.TimeoutExpired:
            entry['outcome'] = 'timeout'
        except Exception as error:
            entry.update(outcome='error', error=type(error).__name__ + ': ' + str(error))
            errors.append(stage + ': ' + entry['error'])
        result['reaping'].append(entry)

    # pid is captured immediately after our new-session Popen. Do not look up
    # an exited leader's PGID again, or ever signal zero/the caller's group.
    if (not isinstance(pgid, int) or pgid <= 1 or pgid != process.pid
            or pgid == os.getpgrp()):
        result['cleanup_error'] = 'invalid task-owned invocation PGID; no group was signaled'
        return result

    initial = observe('initial', 4)
    # Enumeration is observational, never a precondition for either signal.
    send(signal.SIGTERM)
    term_started = time.monotonic()
    reap('after_term', 2, 2)
    if initial is None or initial:
        # Reaping a wrapper is not a grace period for its in-group child.
        grace = min(max(0, 2 - (time.monotonic() - term_started)), remaining(2))
        if grace > 0:
            try:
                time.sleep(grace)
            except Exception as error:
                errors.append('TERM grace: ' + type(error).__name__ + ': ' + str(error))
    after_term = observe('after_term', 1)
    if not result['child_reaped'] or after_term is None or after_term:
        send(signal.SIGKILL)
        reap('after_kill', 1, 0.2)
    final = observe('final', 0.05)
    result['surviving_processes'] = final
    result['group_state'] = 'unknown' if final is None else ('members' if final else 'empty')
    try:
        result['exit_code'] = process.poll()
    except Exception as error:
        result['exit_code'] = None
        errors.append('final poll: ' + type(error).__name__ + ': ' + str(error))
    code = result['exit_code']
    result['signal'] = -code if code is not None and code < 0 else None
    if not result['child_reaped']:
        errors.append('direct-child reaping unconfirmed')
    if final is None:
        errors.append('final invocation-group state unknown')
    elif final:
        errors.append('invocation-group members remain')
    result['cleanup_elapsed_seconds'] = round(time.monotonic() - started, 6)
    result['cleanup_complete'] = not errors
    if errors:
        result['cleanup_error'] = '; '.join(errors)
    return result


def command_timeout(timeout, remaining):
    # Reserve bounded termination/observation time inside the same deadline.
    return timeout if remaining is None else min(timeout, max(0, remaining - CLEANUP_SECONDS))


def cleanup_tests(records):
    """Fault injection reaches the production command and cleanup, without Popen."""
    import ast
    from unittest import mock
    main_node = next(n for n in ast.parse(Path(__file__).read_text()).body
                     if isinstance(n, ast.FunctionDef) and n.name == 'main')
    command_node = next(n for n in main_node.body
                        if isinstance(n, ast.FunctionDef) and n.name == 'command')
    command_ast = ast.Module(body=[command_node], type_ignores=[])

    class CleanupControls(unittest.TestCase):
        def exercise(self, mode='resistant', replacement=None):
            clock = [0.0]; signals = []; probes = []; waits = []
            state = {'code': 0 if mode.startswith('leader_exit') else None,
                     'group_alive': True, 'execution_wait': True}

            class Process:
                pid = 424242
                def poll(self):
                    return state['code']
                def wait(self, timeout):
                    waits.append(timeout)
                    if state['code'] is not None:
                        return state['code']
                    clock[0] += timeout
                    if state['execution_wait'] and mode == 'exhausted':
                        clock[0] += 10
                    state['execution_wait'] = False
                    raise subprocess.TimeoutExpired(['inert'], timeout)

            def inspect_group(pgid, timeout):
                probes.append({'pgid': pgid, 'timeout': timeout, 'at': clock[0]})
                index = len(probes)
                if mode in ('first_nonzero', 'leader_exit_first_failure') and index == 1:
                    raise RuntimeError('ps exit 1')
                if (mode == 'first_timeout' and index == 1) or (mode == 'second_timeout' and index == 2):
                    clock[0] += timeout
                    raise subprocess.TimeoutExpired(['ps'], timeout)
                if mode == 'second_nonzero' and index == 2:
                    raise RuntimeError('ps exit 2')
                if mode == 'final_failure' and index == 3:
                    raise RuntimeError('final ps failed')
                return ['424243 424242 S inert-child'] if state['group_alive'] else []

            def send(pgid, sig):
                self.assertEqual(pgid, Process.pid)
                signals.append(signal.Signals(sig).name)
                if mode == 'signal_error' and sig == signal.SIGTERM:
                    raise PermissionError('inert TERM denial')
                if mode == 'disappeared':
                    state.update(code=-15, group_alive=False)
                    raise ProcessLookupError('inert disappeared group')
                if sig == signal.SIGKILL:
                    state['group_alive'] = False
                    if mode != 'reap_timeout' and not mode.startswith('leader_exit'):
                        state['code'] = -9

            def launch(*argv, **kwargs):
                self.assertIs(kwargs['start_new_session'], True)
                return Process()

            with tempfile.TemporaryDirectory() as private:
                env = dict(globals(), deadline=None, report_dir=Path(private), record={}, save=lambda: None)
                exec(compile(command_ast, __file__, 'exec'), env)
                with mock.patch.object(sys.modules[__name__], 'group_members', inspect_group), \
                     mock.patch.object(os, 'killpg', send), \
                     mock.patch.object(subprocess, 'Popen', launch), \
                     mock.patch.object(time, 'monotonic', lambda: clock[0]), \
                     mock.patch.object(time, 'sleep', lambda seconds: clock.__setitem__(0, clock[0] + seconds)):
                    if replacement is not None:
                        env['cleanup_process'] = replacement
                    result = env['command']('inert', ['inert'], timeout=0.1)
                self.assertEqual(env['record']['inert'], result)
            record = {'case': mode, 'result': result, 'signals': signals,
                      'censuses': probes, 'wait_timeouts': waits,
                      'real_processes_spawned': 0, 'real_signals_sent': 0}
            records.append(record)
            return record

        def required_actions(self, record):
            self.assertEqual(record['signals'], ['SIGTERM', 'SIGKILL'])
            self.assertIn('reaping', record['result'])
            self.assertEqual(record['result']['invocation_pgid'], 424242)

        def test_first_census_nonzero_still_terminates(self):
            r = self.exercise('first_nonzero'); self.required_actions(r)
            self.assertIn('ps exit 1', r['result']['cleanup_error'])
            self.assertFalse(process_ok(r['result']))

        def test_first_census_timeout_still_terminates(self):
            r = self.exercise('first_timeout'); self.required_actions(r)
            self.assertIn('TimeoutExpired', r['result']['cleanup_error'])
            self.assertTrue(r['result']['timed_out'])

        def test_second_census_failure_cannot_skip_kill(self):
            for mode in ['second_nonzero', 'second_timeout']:
                r = self.exercise(mode); self.required_actions(r)
                self.assertFalse(r['result']['cleanup_complete'])
                self.assertEqual(r['result']['exit_code'], -9)

        def test_term_resistant_child_requires_kill_and_reap(self):
            r = self.exercise(); self.required_actions(r)
            self.assertTrue(r['result']['child_reaped'])
            self.assertTrue(r['result']['cleanup_complete'])
            self.assertEqual(r['result']['signal'], 9)
            self.assertTrue(r['result']['timed_out'])
            self.assertFalse(process_ok(r['result']))

        def test_exited_leader_does_not_hide_in_group_child(self):
            r = self.exercise('leader_exit'); self.required_actions(r)
            self.assertEqual(r['result']['exit_code'], 0)
            self.assertEqual(r['result']['group_state'], 'empty')
            self.assertFalse(r['result']['timed_out'])

        def test_zero_exit_and_reaped_child_do_not_hide_observation_error(self):
            r = self.exercise('leader_exit_first_failure'); self.required_actions(r)
            self.assertEqual(r['result']['exit_code'], 0)
            self.assertTrue(r['result']['child_reaped'])
            self.assertFalse(r['result']['timed_out'])
            self.assertFalse(process_ok(r['result']))

        def test_process_lookup_still_reaps_and_observes(self):
            r = self.exercise('disappeared')
            self.assertEqual(r['result']['signal_attempts'][0]['outcome'], 'disappeared')
            self.assertTrue(r['result']['child_reaped'])
            self.assertEqual(len(r['result']['observations']), 3)

        def test_term_signal_error_does_not_skip_kill(self):
            r = self.exercise('signal_error'); self.required_actions(r)
            self.assertIn('PermissionError', r['result']['cleanup_error'])
            self.assertFalse(r['result']['cleanup_complete'])

        def test_final_census_failure_keeps_command_record(self):
            r = self.exercise('final_failure'); self.required_actions(r)
            self.assertIsNone(r['result']['surviving_processes'])
            self.assertEqual(r['result']['group_state'], 'unknown')
            self.assertFalse(process_ok(r['result']))

        def test_bounded_reaping_timeout_is_not_success(self):
            r = self.exercise('reap_timeout'); self.required_actions(r)
            self.assertFalse(r['result']['child_reaped'])
            self.assertIsNone(r['result']['exit_code'])
            self.assertIn('reaping unconfirmed', r['result']['cleanup_error'])
            self.assertLessEqual(max(r['wait_timeouts'][1:]), 2)

        def test_exhausted_observation_budget_cannot_skip_signals(self):
            r = self.exercise('exhausted'); self.required_actions(r)
            self.assertEqual(r['censuses'], [])
            self.assertEqual(r['result']['group_state'], 'unknown')
            self.assertFalse(process_ok(r['result']))

        def test_no_op_cleanup_fails_control(self):
            r = self.exercise(replacement=lambda *args: {'cleanup_error': 'inert no-op'})
            with self.assertRaises(AssertionError):
                self.required_actions(r)

        def test_invalid_or_runner_groups_are_never_signaled(self):
            from unittest import mock
            process = type('InertProcess', (), {'pid': 424242})()
            with mock.patch.object(os, 'killpg') as send:
                for pgid in [0, 1, os.getpgrp(), 424241]:
                    result = cleanup_process(process, pgid, time.monotonic() + 6)
                    self.assertIn('invalid task-owned', result['cleanup_error'])
                send.assert_not_called()

    return unittest.defaultTestLoader.loadTestsFromTestCase(CleanupControls)


def self_test(report=None):
    """No compiler, fixture executable, installer, network or native guard runs."""
    complete = 'SCOPE_DIAGNOSTIC_COMPLETE counts=1 frames=1 straddles=2 clamp=1 oversized=1\n'
    okay = {'exit_code': 0, 'timed_out': False}
    mismatch = 'SCOPE_MISMATCH_BITS left=00000000 expectedLeft=00000001 leftEqual=0 rightEqual=1\n'

    def rejection_controls(verifier):
        for result, output, wanted in [
            (okay, '', 'missing_or_invalid_completion'),
            ({**okay, 'timed_out': True}, complete, 'timeout'),
            (okay, mismatch + complete, 'mismatch_captured'),
            (okay, complete.replace('frames=1', 'frames=0'), 'assertion_failure')]:
            if verifier(result, output)['outcome'] != wanted:
                raise AssertionError('focused result rejection control failed: ' + wanted)
        if verifier(okay, complete)['outcome'] != 'bounded_nonreproduction':
            raise AssertionError('valid focused control was rejected')

    class Controls(unittest.TestCase):
        def exercise(self, overrides=None, output=None, debugger=True):
            with tempfile.TemporaryDirectory() as private:
                source = Path(private) / 'avx2probe.c'; source.write_text(AVX2_SOURCE)
                # These are inert data bytes; the recording boundary never executes them.
                source.with_suffix('').write_bytes(b'inert probe bytes')
                focus = Path(private) / 'focus'; focus.write_bytes(b'inert focused bytes')
                calls = []; record = {}; overrides = overrides or {}; output = output or {}
                def command(label, argv, timeout):
                    calls.append((label, argv, timeout))
                    return dict(overrides.get(label, okay))
                def builder():
                    calls.append(('build-focus', [], 0)); return focus
                try:
                    status = focused_flow(command, record, source, builder,
                        lambda label: output.get(label, complete), lambda: None,
                        debugger_requested=debugger)
                except PrerequisiteError as error:
                    status = error.classification
                return calls, record, status

        def test_production_parser_controls(self):
            rejection_controls(focused_outcome)

        def test_disabled_verifier_is_rejected(self):
            with self.assertRaisesRegex(AssertionError, 'focused result rejection control'):
                rejection_controls(lambda result, output: {'outcome': 'bounded_nonreproduction'})

        def test_old_marker_only_verifier_is_rejected(self):
            def old(result, output):
                return {'outcome': 'bounded_nonreproduction' if result['exit_code'] == 0
                        and 'SCOPE_DIAGNOSTIC_COMPLETE' in output else 'incomplete'}
            with self.assertRaisesRegex(AssertionError, 'focused result rejection control'):
                rejection_controls(old)

        def test_failed_avx2_stops_before_build_and_execution(self):
            calls, record, status = self.exercise({'avx2-execution': {'exit_code': -4}})
            self.assertEqual(status, 'execution_prerequisite_unmet')
            self.assertEqual([x[0] for x in calls], ['rosetta', 'avx2-compile', 'avx2-execution'])
            self.assertEqual(record['focused_direct']['outcome'], 'not_started')

        def test_failed_compile_does_not_claim_unsupported_translation(self):
            calls, _, status = self.exercise({'avx2-compile': {'exit_code': 1}})
            self.assertEqual(status, 'build_prerequisite_failed')
            self.assertEqual([x[0] for x in calls], ['rosetta', 'avx2-compile'])

        def test_direct_precedes_debugger_and_uses_normal_probe(self):
            calls, _, status = self.exercise()
            self.assertTrue(status)
            self.assertEqual([x[0] for x in calls], ['rosetta', 'avx2-compile', 'avx2-execution',
                                                    'build-focus', 'focused-direct', 'focused-debugger'])
            self.assertEqual(calls[1][1][:6], ['cc', '-arch', 'x86_64', '-march=haswell', '-O0', calls[1][1][5]])
            self.assertEqual(calls[-2][2], 60); self.assertEqual(calls[-1][2], 120)

        def test_direct_timeout_is_not_erased_by_debugger_pass(self):
            _, record, status = self.exercise({'focused-direct': {**okay, 'timed_out': True}})
            self.assertFalse(status); self.assertEqual(record['focused_direct']['outcome'], 'timeout')

        def test_zero_exit_without_completion_fails(self):
            _, record, status = self.exercise(output={'focused-direct': ''})
            self.assertFalse(status)
            self.assertEqual(record['focused_direct']['outcome'], 'missing_or_invalid_completion')

        def test_mismatch_overrides_success_status(self):
            _, record, status = self.exercise(output={'focused-direct': mismatch + complete})
            self.assertFalse(status); self.assertTrue(record['focused_direct']['mismatch_observed'])

        def test_direct_pass_does_not_erase_debugger_timeout(self):
            _, record, status = self.exercise({'focused-debugger': {'exit_code': -15, 'timed_out': True}},
                                              {'focused-debugger': '(lldb) run\n'})
            self.assertFalse(status)
            self.assertEqual(record['focused_direct']['outcome'], 'bounded_nonreproduction')
            self.assertEqual(record['focused_debugger']['outcome'], 'timeout')

        def test_unrequested_is_distinct(self):
            calls, record, status = self.exercise(debugger=False)
            self.assertTrue(status); self.assertEqual(record['focused_debugger']['outcome'], 'not_requested')
            self.assertNotIn('focused-debugger', [x[0] for x in calls])

        def test_malformed_duplicate_and_failed_assertions(self):
            for text in [complete + complete, complete.replace('straddles=2', 'straddles=0'),
                         complete.replace('counts=1', 'counts=7')]:
                self.assertNotEqual(focused_outcome(okay, text)['outcome'], 'bounded_nonreproduction')

        def test_dispatch_route_and_old_push_rejection(self):
            root = Path(__file__).resolve().parents[1]
            workflow = (root / '.github/workflows/rosetta-scope.yml').read_text()
            selector = runpy.run_path(str(root / 'scripts/test-workflow-routing.py'))['selection']
            verify_dispatch_route(workflow, selector)
            old = workflow.replace('on:\n  workflow_dispatch:',
                                   'on:\n  push:\n    branches: [codex/rosetta-scope-characterization]\n  workflow_dispatch:')
            with self.assertRaisesRegex(AssertionError, 'must be dispatch-only'):
                verify_dispatch_route(old, selector)
            with self.assertRaisesRegex(AssertionError, 'event/ref selection rejected'):
                verify_dispatch_route(workflow, lambda source, context: ['characterize'])

        def test_deadline_is_clamped_and_not_restarted(self):
            self.assertEqual(command_timeout(120, 36), 30)
            self.assertEqual(command_timeout(60, 5), 0)
            self.assertEqual(command_timeout(60, 100), 60)

        def test_explicit_denial_stops_and_cleanup_survivors_stop(self):
            calls, record, status = self.exercise(output={'focused-direct': 'error: Operation not permitted\n'})
            self.assertEqual(status, 'execution_permission_denied')
            self.assertNotIn('focused-debugger', [x[0] for x in calls])
            calls, _, status = self.exercise({'focused-direct': {**okay, 'surviving_processes': ['test-owned child']}})
            self.assertEqual(status, 'cleanup_incomplete')
            self.assertNotIn('focused-debugger', [x[0] for x in calls])

        def test_normal_probe_source_stays_in_sync(self):
            workflow = (Path(__file__).resolve().parents[1] / '.github/workflows/build.yml').read_text()
            literal = AVX2_SOURCE.replace('\n', r'\n')
            self.assertIn("printf '" + literal + "'", workflow)
            self.assertIn('cc -arch x86_64 -march=haswell -O0', workflow)

    records = []
    suite = unittest.TestSuite([unittest.defaultTestLoader.loadTestsFromTestCase(Controls), cleanup_tests(records)])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if report is not None:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps({'kind': 'inert production command/cleanup controls',
            'tests_run': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
            'real_processes_spawned': 0, 'real_signals_sent': 0, 'cases': records}, indent=2) + '\n')
    return result.wasSuccessful()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--self-test-report', type=Path)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--report-dir', type=Path)
    parser.add_argument('--ordinary-runs', type=int, choices=[0], default=0)
    args = parser.parse_args()
    if args.self_test:
        return 0 if self_test(args.self_test_report) else 1
    if args.source is None or args.report_dir is None:
        parser.error('--source and --report-dir are required for native characterization')
    source = args.source.resolve(); report_dir = args.report_dir.resolve()
    report_dir.mkdir(parents=True, exist_ok=False)
    record = {'source_head': SOURCE_HEAD, 'consumer_head': os.environ.get('GITHUB_SHA'),
              'run_id': os.environ.get('GITHUB_RUN_ID'), 'run_attempt': os.environ.get('GITHUB_RUN_ATTEMPT'),
              'kind': 'controlled reconstruction; original executable not recovered',
              'original_build_run': 40221042187, 'original_build_job': 121252472086,
              'original_build_number': 1878, 'ordinary_invocations': [],
              'ordinary_run_budget': 0, 'execution_budget_seconds': 600,
              'cause_established': False, 'original_failure_erased': False,
              'shipping_gate': False, 'host_security_settings_changed': False,
              'status': 'in_progress', 'classification': 'setup_incomplete'}
    deadline = None; binary = None; started_budget = None; complete = False

    def save():
        (report_dir / 'characterization.json').write_text(json.dumps(record, indent=2) + '\n')

    def command(label, argv, timeout=60, cwd=None):
        started = time.monotonic()
        remaining = None if deadline is None else deadline - started
        limit = command_timeout(timeout, remaining)
        # Reserve cleanup before launch. One fixed monotonic deadline covers all
        # observation, TERM/grace, KILL/reaping and final-report operations.
        cleanup_deadline = started + limit + CLEANUP_SECONDS
        if deadline is not None:
            cleanup_deadline = min(cleanup_deadline, deadline)
        result = {'argv': argv, 'exit_code': None, 'signal': None, 'timed_out': False,
                  'timeout_seconds': round(limit, 3), 'surviving_processes': None,
                  'cleanup_complete': False}
        log = report_dir / (label + '.log')
        with log.open('wb') as output:
            if limit <= 0:
                result['budget_exhausted'] = True
            else:
                try:
                    process = subprocess.Popen(argv, cwd=cwd, stdout=output, stderr=subprocess.STDOUT,
                                               start_new_session=True)
                    pgid = process.pid
                    try:
                        process.wait(timeout=limit)
                    except subprocess.TimeoutExpired:
                        result['timed_out'] = True
                    except Exception as error:
                        result['execution_wait_error'] = str(error)
                    finally:
                        result.update(cleanup_process(process, pgid, cleanup_deadline))
                except OSError as error:
                    result['launch_error'] = str(error)
                except Exception as error:
                    result['cleanup_error'] = type(error).__name__ + ': ' + str(error)
        result.update(elapsed_seconds=round(time.monotonic() - started, 3), log=log.name, log_sha256=sha(log))
        record[label.replace('-', '_')] = result
        save()
        return result

    def required(label, argv, timeout=60):
        result = command(label, argv, timeout)
        if not process_ok(result):
            raise PrerequisiteError('build_prerequisite_failed', label + ' did not complete')
        return result

    def build_focus():
        diagnostic = Path(__file__).resolve().parents[1] / 'tests/scope_ring_diagnostic.cpp'
        shutil.copyfile(diagnostic, source / 'tests/scope_ring_diagnostic.cpp')
        # Private, separate target only; shipping CMake and ordinary executable stay intact.
        addition = '''
juce_add_console_app(AnamorphScopeDiagnostic PRODUCT_NAME "AnamorphScopeDiagnostic")
target_sources(AnamorphScopeDiagnostic PRIVATE tests/scope_ring_diagnostic.cpp)
target_include_directories(AnamorphScopeDiagnostic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
get_target_property(scope_definitions AnamorphTests COMPILE_DEFINITIONS)
target_compile_definitions(AnamorphScopeDiagnostic PUBLIC ${scope_definitions})
target_link_libraries(AnamorphScopeDiagnostic PRIVATE juce::juce_dsp juce::juce_audio_basics
    AnamorphHardening juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
'''
        with (source / 'CMakeLists.txt').open('a') as file:
            file.write(addition)
        required('diagnostic-configure', ['cmake', '-S', str(source), '-B', str(source / 'build')], 180)
        required('diagnostic-build', ['cmake', '--build', str(source / 'build'), '--target', 'AnamorphScopeDiagnostic', '--verbose'], 300)
        focus = source / 'build/AnamorphScopeDiagnostic_artefacts/Release/AnamorphScopeDiagnostic'
        record['diagnostic_binary'] = {'path': str(focus), 'sha256': sha(focus),
            'source_sha256': sha(diagnostic), 'kind': 'separate single-threaded Test 61 variant'}
        return focus

    save()
    try:
        if sys.platform != 'darwin' or platform.machine() != 'arm64':
            raise PrerequisiteError('host_prerequisite_unmet', 'requires a real disposable Apple Silicon Mac')
        if os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted' or os.environ.get('GITHUB_REPOSITORY') != 'skyRolly/Anamorph':
            raise PrerequisiteError('host_prerequisite_unmet', 'requires this repository\'s genuine disposable hosted runner')
        actual = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True, timeout=10).strip()
        if actual != SOURCE_HEAD:
            raise PrerequisiteError('source_prerequisite_unmet', 'retained source checkout differs from the original failure')
        record['host'] = {'machine': platform.machine(), 'system': platform.platform(),
            'runner_image': os.environ.get('ImageOS'), 'runner_image_version': os.environ.get('ImageVersion')}
        record['environment'] = {key: os.environ.get(key) for key in ('ROSETTA_ADVERTISE_AVX', 'ANAMORPH_TESTS_NO_FTZ')}
        if record['environment'] != {'ROSETTA_ADVERTISE_AVX': '1', 'ANAMORPH_TESTS_NO_FTZ': None}:
            raise PrerequisiteError('environment_prerequisite_unmet', 'normal AVX advertisement and unrelaxed FP policy required')
        for label, argv in [('os', ['sw_vers']), ('compiler', ['xcrun', 'clang++', '--version']),
                            ('probe-compiler', ['cc', '--version']), ('sdk', ['xcrun', '--show-sdk-path']),
                            ('sdk-version', ['xcrun', '--show-sdk-version']), ('xcode', ['xcodebuild', '-version'])]:
            required(label, argv)
        compiler = shutil.which('cc')
        record['probe_compiler_identity'] = {'selection': 'cc', 'path': compiler,
                                             'sha256': sha(Path(compiler)) if compiler else None}
        binary = source / 'build/AnamorphTests_artefacts/Release/AnamorphTests'
        record['ordinary_binary'] = {'path': str(binary), 'sha256': sha(binary), 'executed': False, 'original_executable': False}
        record['source_sha256'] = {name: sha(source / name) for name in
            ['CMakeLists.txt', 'tests/dsp_tests.cpp', 'src/dsp/ScopeBuffer.h', 'scripts/run-tests.sh']}
        record['driver_sha256'] = sha(Path(__file__).resolve())
        juce = source / 'build/_deps/juce-src'
        record['juce_head'] = subprocess.check_output(['git', '-C', str(juce), 'rev-parse', 'HEAD'], text=True, timeout=10).strip()
        if record['juce_head'] != '3fb5cbc0340758596dac39f7c8de47d562b4896e':
            raise PrerequisiteError('source_prerequisite_unmet', 'JUCE checkout differs from the product pin')
        record['juce_comparison_sha256'] = sha(juce / 'modules/juce_core/maths/juce_MathsFunctions.h')
        required('binary-uuid', ['xcrun', 'dwarfdump', '--uuid', str(binary)])
        required('binary-architectures', ['lipo', '-archs', str(binary)])
        required('binary-sdk', ['xcrun', 'vtool', '-show-build', str(binary)])
        required('binary-symbols', ['xcrun', 'dsymutil', str(binary), '-o', str(report_dir / 'AnamorphTests.dSYM')], 120)
        required('symbol-uuid', ['xcrun', 'dwarfdump', '--uuid', str(report_dir / 'AnamorphTests.dSYM')])
        # One deadline, including capability/build/direct/debug and diagnostics.
        started_budget = time.monotonic(); deadline = started_budget + 600
        probe_source = report_dir / 'avx2probe.c'; probe_source.write_text(AVX2_SOURCE)
        complete = focused_flow(command, record, probe_source, build_focus,
            lambda label: (report_dir / (label + '.log')).read_text(errors='replace'), save)
        focus = Path(record['diagnostic_binary']['path'])
        for label, argv in [('diagnostic-uuid', ['xcrun', 'dwarfdump', '--uuid', str(focus)]),
                            ('diagnostic-sdk', ['xcrun', 'vtool', '-show-build', str(focus)]),
                            ('diagnostic-symbols', ['xcrun', 'dsymutil', str(focus), '-o', str(report_dir / 'ScopeDiagnostic.dSYM')]),
                            ('diagnostic-symbol-uuid', ['xcrun', 'dwarfdump', '--uuid', str(report_dir / 'ScopeDiagnostic.dSYM')])]:
            required(label, argv)
        record['classification'] = 'bounded_nonreproduction' if complete else 'focused_incomplete_or_failure'
    except PrerequisiteError as error:
        record['error'] = str(error); record['classification'] = error.classification
    except Exception as error:
        record['error'] = str(error); record['classification'] = 'infrastructure_or_orchestration_failure'
    finally:
        if binary is not None and 'ordinary_binary' in record:
            record['ordinary_binary']['unchanged_after_focus'] = sha(binary) == record['ordinary_binary']['sha256']
            if not record['ordinary_binary']['unchanged_after_focus']:
                record['classification'] = 'ordinary_binary_changed'; complete = False
        compile_commands = source / 'build/compile_commands.json'
        if compile_commands.is_file():
            shutil.copyfile(compile_commands, report_dir / 'compile_commands.json')
        record['execution_elapsed_seconds_after_setup'] = None if started_budget is None else round(time.monotonic() - started_budget, 3)
        record['status'] = 'bounded_characterization_complete' if complete and record['classification'] == 'bounded_nonreproduction' else 'incomplete_or_failure'
        record['interpretation'] = 'Native outcomes remain separate; bounded nonreproduction does not establish the original cause. UUID matching does not prove complete symbols.'
        save()
    return 0 if record['status'] == 'bounded_characterization_complete' else 1


if __name__ == '__main__':
    raise SystemExit(main())
