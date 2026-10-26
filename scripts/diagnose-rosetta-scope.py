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
            and not result.get('surviving_processes') and not result.get('cleanup_error'))


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


def command_timeout(timeout, remaining):
    # Reserve bounded termination/observation time inside the same deadline.
    return timeout if remaining is None else min(timeout, max(0, remaining - 6))


def self_test():
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

    return unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls)).wasSuccessful()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--source', type=Path)
    parser.add_argument('--report-dir', type=Path)
    parser.add_argument('--ordinary-runs', type=int, choices=[0], default=0)
    args = parser.parse_args()
    if args.self_test:
        return 0 if self_test() else 1
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

    def group_members(pgid):
        # Read-only bounded observation of this invocation's process group.
        table = subprocess.run(['ps', '-axo', 'pid=,pgid=,stat=,command='],
                               capture_output=True, text=True, timeout=1, check=False)
        if table.returncode:
            raise RuntimeError('could not inspect the test-owned process group')
        return [line.strip() for line in table.stdout.splitlines()
                if len(line.split(None, 3)) >= 3 and line.split(None, 3)[1] == str(pgid)]

    def command(label, argv, timeout=60, cwd=None):
        remaining = None if deadline is None else deadline - time.monotonic()
        limit = command_timeout(timeout, remaining)
        result = {'argv': argv, 'exit_code': None, 'signal': None, 'timed_out': False,
                  'timeout_seconds': round(limit, 3), 'surviving_processes': []}
        started = time.monotonic(); log = report_dir / (label + '.log')
        with log.open('wb') as output:
            if limit <= 0:
                result['budget_exhausted'] = True
            else:
                try:
                    process = subprocess.Popen(argv, cwd=cwd, stdout=output, stderr=subprocess.STDOUT,
                                               start_new_session=True)
                    try:
                        process.wait(timeout=limit)
                    except subprocess.TimeoutExpired:
                        result['timed_out'] = True
                    # Terminate only this task-owned process group, including children
                    # left behind by an exited wrapper. Preserve errors/survivors.
                    try:
                        members = group_members(process.pid)
                        if process.poll() is None or members:
                            os.killpg(process.pid, signal.SIGTERM)
                            try:
                                process.wait(timeout=2)
                            except subprocess.TimeoutExpired:
                                pass
                            members = group_members(process.pid)
                            if process.poll() is None or members:
                                os.killpg(process.pid, signal.SIGKILL)
                                try:
                                    process.wait(timeout=1)
                                except subprocess.TimeoutExpired:
                                    pass
                        result['surviving_processes'] = group_members(process.pid)
                    except ProcessLookupError:
                        result['surviving_processes'] = group_members(process.pid)
                    except Exception as error:
                        result['cleanup_error'] = str(error)
                    result['exit_code'] = process.poll()
                    result['signal'] = -process.returncode if process.returncode is not None and process.returncode < 0 else None
                except OSError as error:
                    result['launch_error'] = str(error)
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
