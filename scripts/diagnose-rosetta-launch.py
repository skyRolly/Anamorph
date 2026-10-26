#!/usr/bin/env python3
"""One bounded hello-only launch comparison, or entirely inert --self-test."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import runpy
import subprocess
import sys
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('scope_cleanup', ROOT / 'scripts/diagnose-rosetta-scope.py')
cleanup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cleanup)
MARKER = 'ANAMORPH_LAUNCH_CONTROL_COMPLETE v=1'
SOURCE = '#include <stdio.h>\nint main(void) { puts("' + MARKER + '"); return 0; }\n'
COMMANDS = 'settings set target.disable-aslr false\nsettings show target.disable-aslr\nrun\n'


def outcome(result, output, debugger=False):
    """The runtime marker, inferior exit and debugger completion are independent."""
    markers = [line for line in output.splitlines() if line.strip() == MARKER]
    exits = re.findall(r'^Process (\d+) exited with status = (\d+)\b', output, re.M)
    denied = bool(re.search(r'(?im)^.*(?:error:.*(?:not permitted|permission denied)|process launch failed.*(?:denied|not allowed))', output))
    if denied:
        state = 'explicit_denial'
    elif result.get('timed_out'):
        state = 'timeout'
    elif result.get('budget_exhausted'):
        state = 'budget_exhausted'
    elif result.get('launch_error'):
        state = 'launch_failure'
    elif result.get('cleanup_error') or result.get('cleanup_complete') is False:
        state = 'cleanup_incomplete'
    elif not cleanup.process_ok(result):
        state = 'execution_failure'
    elif len(markers) != 1:
        state = 'missing_completion'
    elif debugger and (len(exits) != 1 or exits[0][1] != '0'):
        state = 'inferior_exit_unverified'
    elif debugger and not re.search(r'target\.disable-aslr[^\n]*=\s*false\b', output):
        state = 'ASLR_setting_unverified'
    else:
        state = 'complete'
    return {'outcome': state, 'runtime_marker_count': len(markers),
            'inferior_exit_records': [{'pid': int(p), 'status': int(s)} for p, s in exits],
            'inferior_identity_from_log_only': True}


def controls(run, lldb, binaries, commands, record, save, classify=outcome):
    """Production control order; a failed stage withholds all later targets."""
    plan = [('direct-x86_64', ['/usr/bin/arch', '-x86_64', str(binaries['x86_64'])], 10, False),
            ('lldb-arm64', [lldb, '--no-lldbinit', '--batch', '--arch', 'arm64', '-s', str(commands), '--', str(binaries['arm64'])], 30, True),
            ('lldb-x86_64', [lldb, '--no-lldbinit', '--batch', '--arch', 'x86_64', '-s', str(commands), '--', str(binaries['x86_64'])], 30, True)]
    record['controls'] = {name: {'outcome': 'not_run'} for name, *_ in plan}
    for name, argv, cap, debugger in plan:
        result, output = run(name, argv, cap, observe=True)
        result.update(classify(result, output, debugger))
        record['controls'][name] = result
        save()
        if result['outcome'] != 'complete':
            record['classification'] = name + ':' + result['outcome']
            return False
    record['classification'] = 'tiny_launch_controls_completed'
    return True


def verify_route(workflow, select):
    events = workflow.split('\non:\n', 1)[1].split('\npermissions:', 1)[0]
    assert re.findall(r'^  ([a-z_]+):', events, re.M) == ['workflow_dispatch'], 'dispatch-only required'
    assert re.search(r'        default: scope\s*$', events, re.M), 'legacy scope default changed'
    for mode, expected in [('launch-control', ['launch-control']), ('scope', ['characterize']), ('', ['characterize']), ('invalid', [])]:
        for event, branch, repo, valid in [
            ('workflow_dispatch', 'codex/rosetta-scope-characterization', 'skyRolly/Anamorph', True),
            ('push', 'codex/rosetta-scope-characterization', 'skyRolly/Anamorph', False),
            ('pull_request', 'codex/rosetta-scope-characterization', 'skyRolly/Anamorph', False),
            ('workflow_dispatch', 'main', 'skyRolly/Anamorph', False),
            ('workflow_dispatch', 'codex/other', 'skyRolly/Anamorph', False),
            ('workflow_dispatch', 'codex/rosetta-scope-characterization', 'fork/Anamorph', False)]:
            actual = select(workflow, {'github.event_name': event, 'github.ref': 'refs/heads/' + branch,
                           'github.repository': repo, 'inputs.mode': mode})
            if actual != (expected if valid else []):
                raise AssertionError('native launch route rejected: ' + mode + '/' + event + ': ' + repr(actual))
    body = workflow.split('  launch-control:\n', 1)[1]
    assert 'timeout-minutes: 10' in body
    assert body.count('uses: actions/checkout@') == 1
    assert 'python3 scripts/diagnose-rosetta-launch.py --report-dir launch-evidence' in body
    for forbidden in ('original-source', 'cmake ', 'brew ', 'AnamorphTests', 'ScopeDiagnostic', '--source ', 'scope_mismatch'):
        assert forbidden not in body, 'scope campaign reached launch job: ' + forbidden
    assert '--ordinary-runs 0' in workflow


def route_liveness(workflow, select, verifier=verify_route):
    verifier(workflow, select)
    broken = workflow.replace("inputs.mode == 'launch-control'", "inputs.mode == 'never-selected'", 1)
    assert broken != workflow
    try:
        verifier(broken, select)
    except AssertionError as error:
        assert str(error).startswith('native launch route rejected: launch-control/workflow_dispatch:'), str(error)
    else:
        raise AssertionError('route verifier accepted missing launch job')


def self_test(report):
    good = {'exit_code': 0, 'timed_out': False, 'cleanup_complete': True}
    debugger = 'target.disable-aslr (boolean) = false\n' + MARKER + '\nProcess 123 exited with status = 0 (0x00000000)\n'
    class Inert(unittest.TestCase):
        def test_runtime_and_inferior_controls(self):
            self.assertEqual(outcome(good, MARKER + '\n')['outcome'], 'complete')
            self.assertEqual(outcome(good, debugger, True)['outcome'], 'complete')
            for output in ('', '(lldb) echo ' + MARKER, '"' + MARKER + '"'):
                self.assertEqual(outcome(good, output)['outcome'], 'missing_completion')
            self.assertEqual(outcome(good, MARKER + '\n', True)['outcome'], 'inferior_exit_unverified')
            self.assertEqual(outcome(good, debugger.replace('status = 0', 'status = 1'), True)['outcome'], 'inferior_exit_unverified')
            self.assertEqual(outcome(good, debugger.replace('= false', '= true'), True)['outcome'], 'ASLR_setting_unverified')
            self.assertEqual(outcome(dict(good, timed_out=True), debugger, True)['outcome'], 'timeout')
            self.assertEqual(outcome(good, 'error: process launch failed: Operation not permitted', True)['outcome'], 'explicit_denial')
        def test_order_caps_and_stop(self):
            for stop in (None, 'direct-x86_64', 'lldb-arm64'):
                calls = []; record = {}
                def run(name, argv, cap, observe):
                    calls.append((name, argv, cap, observe))
                    return dict(good, timed_out=name == stop), debugger if name.startswith('lldb') else MARKER + '\n'
                done = controls(run, '/inert/lldb', {'arm64': Path('/inert/arm'), 'x86_64': Path('/inert/intel')}, Path('/inert/commands'), record, lambda: None)
                count = {None: 3, 'direct-x86_64': 1, 'lldb-arm64': 2}[stop]
                self.assertEqual(len(calls), count)
                self.assertEqual(done, stop is None)
                self.assertEqual([c[2] for c in calls], [10, 30, 30][:count])
                for name, argv, *_ in calls:
                    if name.startswith('lldb'): self.assertIn('--no-lldbinit', argv)
                self.assertEqual([r['outcome'] for r in record['controls'].values()].count('not_run'), 3-count)
        def test_cleanup_error_withholds_debuggers(self):
            for change, output in [({'cleanup_error': 'inert observation fault'}, MARKER + '\n'),
                                   ({'exit_code': 1}, ''), ({'budget_exhausted': True}, ''),
                                   ({}, 'error: launch failed: Operation not permitted')]:
                record = {}; calls = []
                def run(name, *args, **kwargs):
                    calls.append(name)
                    return dict(good, **change), output
                self.assertFalse(controls(run, 'inert', {'arm64': '', 'x86_64': ''}, '', record, lambda: None))
                self.assertEqual(calls, ['direct-x86_64'])
        def test_route_and_no_op_rejection(self):
            source = (ROOT / '.github/workflows/rosetta-scope.yml').read_text()
            select = runpy.run_path(str(ROOT / 'scripts/test-workflow-routing.py'))['selection']
            route_liveness(source, select)
            with self.assertRaisesRegex(AssertionError, 'verifier accepted missing launch job'):
                route_liveness(source, select, verifier=lambda *args: None)
            cleanup.verify_dispatch_route(source, select)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Inert))
    if report:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps({'kind': 'inert launch parser/production routing controls',
            'tests_run': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
            'native_controls_executed': 0, 'real_processes_spawned': 0}, indent=2) + '\n')
    return result.wasSuccessful()


class Stop(RuntimeError):
    pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--report-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        return 0 if self_test(args.report) else 1
    if args.report_dir is None:
        parser.error('--report-dir is required')
    started = time.monotonic(); deadline = started + 120
    out = args.report_dir.resolve(); out.mkdir(mode=0o700, parents=True, exist_ok=False)
    record = {'consumer_head': os.environ.get('GITHUB_SHA'), 'run': os.environ.get('GITHUB_RUN_ID'),
              'kind': 'tiny baseline hello; no project/DSP code, no AVX2 certification',
              'budget_seconds': 120, 'commands': {}, 'status': 'in_progress', 'classification': 'setup_incomplete',
              'initialization_difference': '--no-lldbinit for both controls; historical init state is not replayed',
              'host_security_settings_changed': False, 'outside_group_signals': False}
    record['controls'] = {name: {'outcome': 'not_run'} for name in ('direct-x86_64','lldb-arm64','lldb-x86_64')}
    command_env = dict(os.environ)
    def save():
        record['elapsed_seconds'] = round(time.monotonic()-started, 6)
        (out/'launch-control.json').write_text(json.dumps(record, indent=2) + '\n')
    def remaining(reserve=0):
        return max(0, deadline-time.monotonic()-reserve)
    def digest(path):
        h = hashlib.sha256()
        with Path(path).open('rb') as stream:
            while True:
                if remaining() <= 0: raise Stop('execution_budget_exhausted')
                data = stream.read(1024*1024)
                if not data: break
                h.update(data)
        return h.hexdigest()

    def observe(pgid, leader, known):
        limit = min(0.5, remaining(0.05))
        if limit <= 0: raise Stop('identity observation deadline exhausted')
        table = subprocess.run(['/bin/ps', '-ww', '-axo', 'pid=,ppid=,pgid=,sess=,lstart=,comm='],
                               text=True, capture_output=True, timeout=limit)
        if table.returncode: raise Stop('identity ps failed: ' + table.stderr)
        rows = {}
        for line in table.stdout.splitlines():
            fields = line.split(None, 9)
            if len(fields) == 10:
                pid, ppid, group = map(int, fields[:3])
                rows[pid] = {'pid': pid, 'ppid': ppid, 'pgid': group, 'session_field': fields[3],
                             'start_label': ' '.join(fields[4:9]), 'executable_label': fields[9]}
        owned = {p for p,r in rows.items() if p == leader or (pgid is not None and r['pgid'] == pgid)}
        for p, identity in known.items():
            if p in rows and all(rows[p][k] == identity[k] for k in ('start_label','executable_label')):
                owned.add(p)
        for _ in range(len(rows)):
            children = {p for p,r in rows.items() if r['ppid'] in owned}
            if children <= owned: break
            owned |= children
        observed = [rows[p] for p in sorted(owned)]
        for row in observed: known[row['pid']] = row
        return observed

    def run(label, argv, cap, observe=False):
        began = time.monotonic(); limit = min(cap, remaining(cleanup.CLEANUP_SECONDS))
        execution_end = began + limit
        cleanup_end = min(deadline, execution_end + cleanup.CLEANUP_SECONDS)
        result = {'argv': [str(x) for x in argv], 'timeout_seconds': round(limit, 3),
                  'exit_code': None, 'signal': None, 'timed_out': False, 'cleanup_complete': False,
                  'identity_observations': [], 'start_offset_seconds': round(began-started, 6)}
        log = out/(label+'.log'); known = {}; inspection_errors = []
        def sample(stage, pgid, pid):
            entry = {'stage': stage}
            try:
                entry['owned'] = identity_observer(pgid, pid, known)
            except Exception as error:
                entry['error'] = type(error).__name__ + ': ' + str(error)
                inspection_errors.append(entry['error'])
            result['identity_observations'].append(entry)
            return entry.get('owned')
        with log.open('wb') as output:
            if limit <= 0:
                result['budget_exhausted'] = True
            else:
                try:
                    if observe:
                        # The prelaunch snapshot records only this controller,
                        # never makes its group eligible for any signal.
                        controller = {}; result['controller_before_launch'] = identity_observer(None, os.getpid(), controller)
                    process = subprocess.Popen(result['argv'], stdout=output, stderr=subprocess.STDOUT,
                                               start_new_session=True, env=command_env)
                    pgid = process.pid
                    try:
                        if observe: sample('after_spawn', pgid, process.pid)
                        try:
                            process.wait(timeout=min(0.25, max(0, execution_end-time.monotonic())))
                        except subprocess.TimeoutExpired:
                            if observe: sample('during', pgid, process.pid)
                            process.wait(timeout=max(0, execution_end-time.monotonic()))
                    except subprocess.TimeoutExpired:
                        result['timed_out'] = True
                    except Exception as error:
                        result['execution_wait_error'] = type(error).__name__ + ': ' + str(error)
                    finally:
                        result.update(cleanup.cleanup_process(process, pgid, cleanup_end))
                        if observe:
                            final = sample('after_cleanup', pgid, process.pid)
                            if final: inspection_errors.append('known owned identities remain after group cleanup; no outside-group signal attempted')
                            if inspection_errors:
                                result['cleanup_error'] = '; '.join(filter(None, [result.get('cleanup_error')] + inspection_errors))
                                result['cleanup_complete'] = False
                except Exception as error:
                    result['launch_error'] = type(error).__name__ + ': ' + str(error)
        # Preserve the command record even after its execution deadline. These
        # bounded hello/tool logs are data; hashing never starts another process.
        try:
            result['log_sha256'] = hashlib.sha256(log.read_bytes()).hexdigest()
        except OSError as error:
            result['log_error'] = str(error)
            result['cleanup_complete'] = False
        result.update(log=log.name, elapsed_seconds=round(time.monotonic()-began, 6))
        try:
            captured = log.read_text(errors='replace')
        except OSError as error:
            captured = ''
            result.update(log_error=str(error), cleanup_complete=False)
        record['commands'][label] = result
        save()
        return result, captured

    identity_observer = observe
    def required(label, argv, cap=4):
        result, output = run(label, argv, cap)
        if not cleanup.process_ok(result): raise Stop(label + ':tool_or_build_prerequisite_failed')
        return output.strip()
    save()
    try:
        if (sys.platform != 'darwin' or platform.machine() != 'arm64'
                or os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted'
                or os.environ.get('GITHUB_REPOSITORY') != 'skyRolly/Anamorph'
                or os.environ.get('GITHUB_REF') != 'refs/heads/codex/rosetta-scope-characterization'
                or os.environ.get('GITHUB_EVENT_NAME') != 'workflow_dispatch'):
            raise Stop('genuine_disposable_host_prerequisite_unmet')
        record['host'] = {'platform': platform.platform(), 'architecture': platform.machine(),
                          'image': os.environ.get('ImageOS'), 'image_version': os.environ.get('ImageVersion')}
        preferred = next((p for p in (Path('/Applications/Xcode_26.6.app/Contents/Developer'),
                                      Path('/Applications/Xcode_26.6.0.app/Contents/Developer')) if p.is_dir()), None)
        if preferred is not None:
            command_env['DEVELOPER_DIR'] = str(preferred)
        record['toolchain_selection'] = {'command_DEVELOPER_DIR': command_env.get('DEVELOPER_DIR'),
             'scope': 'command-only existing installation; no global selection or installation change',
             'preference': 'existing named 26.6 candidate' if preferred else 'runner current selection; actual version is authoritative'}
        record['macOS'] = required('macOS', ['/usr/bin/sw_vers'])
        record['Xcode'] = required('Xcode', ['/usr/bin/xcodebuild','-version'])
        record['global_developer_selection'] = required('developer', ['/usr/bin/xcode-select','-p'])
        developer = Path(command_env.get('DEVELOPER_DIR') or record['global_developer_selection'])
        record['SDK_path'] = required('SDK-path', ['/usr/bin/xcrun','--show-sdk-path'])
        record['SDK_version'] = required('SDK-version', ['/usr/bin/xcrun','--show-sdk-version'])
        compiler = required('compiler-path', ['/usr/bin/xcrun','--find','clang'])
        lldb = required('lldb-path', ['/usr/bin/xcrun','--find','lldb'])
        record['tools'] = {}
        for name, path in [('compiler',compiler),('lldb',lldb)]:
            resolved = Path(path).resolve(strict=True)
            record['tools'][name] = {'selected_path':path,'resolved_path':str(resolved),'sha256':digest(resolved),
                                     'version': required(name+'-version',[path,'--version'])}
        candidates = [Path(lldb).parent/'debugserver', developer.parent/'SharedFrameworks/LLDB.framework/Resources/debugserver',
                      developer/'Toolchains/XcodeDefault.xctoolchain/usr/libexec/debugserver']
        record['debugserver_candidates'] = [{'path':str(p),'resolved_path':str(p.resolve()),'sha256':digest(p),
             'identity_scope':'on-disk candidate; executing identity not established by presence'} for p in candidates if p.is_file()]
        record['debugserver_candidate_paths_checked'] = [str(p) for p in candidates]
        record['debugserver_candidate_status'] = ('on_disk_candidates_only' if record['debugserver_candidates']
                                                  else 'not_found_in_bounded_candidate_paths')
        record['identity_precision'] = 'ps start labels have second precision; no outside-group signal identity claim'
        help_text = required('lldb-help',[lldb,'--help'])
        for option in ('--batch','--arch','--no-lldbinit'):
            if option not in help_text: raise Stop('selected_lldb_option_missing:' + option)
        source = out/'hello.c'; source.write_text(SOURCE)
        commands = out/'lldb.commands'; commands.write_text(COMMANDS)
        record['source_sha256'] = digest(source); record['command_file_sha256'] = digest(commands)
        record['driver_sha256'] = digest(Path(__file__))
        record['binaries'] = {}; binaries = {}
        for arch in ('arm64','x86_64'):
            binary = out/('hello-'+arch); binaries[arch] = binary
            flags = ['-arch',arch,'-g','-O0']
            required('compile-'+arch,[compiler,*flags,str(source),'-o',str(binary)],15)
            actual = required('architecture-'+arch, ['/usr/bin/lipo','-archs',str(binary)])
            if actual != arch: raise Stop('thin_architecture_mismatch:' + arch)
            record['binaries'][arch] = {'path':str(binary),'sha256':digest(binary),'flags':flags,'actual_architecture':actual,
                'UUID':required('UUID-'+arch,['/usr/bin/xcrun','dwarfdump','--uuid',str(binary)]),
                'symbols':'-g requested; UUID alone does not certify complete symbols'}
        save()
        done = controls(run,lldb,binaries,commands,record,save)
        record['status'] = 'completed' if done else 'incomplete'
    except Exception as error:
        record.update(status='incomplete',classification=str(error),error=type(error).__name__ + ': ' + str(error))
    finally:
        record['outside_group_service_limit'] = 'No claim about unseen/reparented services. Compare final runner cleanup log with recorded owned identities; never kill by name.'
        save()
    if record['status'] != 'completed':
        print('launch-control incomplete: ' + record['classification'], file=sys.stderr)
    return 0 if record['status'] == 'completed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
