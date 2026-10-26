#!/usr/bin/env python3
"""Trace retained hello launches once; --self-test uses only inert fixtures."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import resource
import runpy
import stat
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('tiny_launch', ROOT/'scripts/diagnose-rosetta-launch.py')
launch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launch)
cleanup = launch.cleanup
RUN = 40338130238
ARTIFACT = 12657441997
HEAD = '04d2dd431a980073e572cfb3855ff53f3edf55e8'
TRACE_CAP = 2 * 1024 * 1024
UUIDS = {'arm64': 'CF19E771-A413-409B-BEF2-F51F0FBA7226',
         'x86_64': '2B17D5E5-1BE1-4E89-B4E2-D19D7E58825E'}
# Read from authenticated artifact 12657441997, never from installed receipts
# or freshly rebuilt executables. Include every retained dSYM member.
INPUTS = {
    'hello-arm64': '71549ac1adc34e3ce9bb489bc68fb551c799acf4499b5f75ec482172773e0742',
    'hello-arm64.dSYM/Contents/Info.plist': '4612830ba9bdfe3f339ddc5c2cb7b6add113735b9cf76d5364534bef27eb06e9',
    'hello-arm64.dSYM/Contents/Resources/DWARF/hello-arm64': '4cebb185679b52fa9c4e6d7d2948bd3f88bda57fb89400d72f6e566fdc2411f7',
    'hello-arm64.dSYM/Contents/Resources/Relocations/aarch64/hello-arm64.yml': 'b45a47654cea8025eb10771bd3c6795508a8ab9eb1e7b910ee152abf5c3d3c16',
    'hello-x86_64': '85f86323f48bf49e9c04dcea06a1acba41337552d163d6d6c81be99abf3b43e1',
    'hello-x86_64.dSYM/Contents/Info.plist': 'ff621e673d17a323ae9d03b722f651ef843abd75bead2f2a3bdc390fec35e83e',
    'hello-x86_64.dSYM/Contents/Resources/DWARF/hello-x86_64': 'f185c58e3864f9fa4d72d701e21d80b527ccbe882d11f3ab1a4b002374172e34',
    'hello-x86_64.dSYM/Contents/Resources/Relocations/x86_64/hello-x86_64.yml': '7ac0d86dc7a15485c441514fe0d946c31e8859a88f27bf2ffa12086969ac1c70',
    'hello.c': 'f7bb3bed820c555593d54f41efafeaef0b2d0cdd13deeba6cb8cc8c975951ab3',
    'lldb.commands': 'ad77281e28d40d773d07115a35433c771718d2fdee8652779f05008be46430d8',
    'launch-control.json': '35a0f78dfd4afbe082cc11f4dfff424b757f9e98d809fbf3af064450aa6c1ef5',
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verified_bytes(root, relative, expected):
    path = root/relative
    for part in [path, *path.parents]:
        if part == root.parent:
            break
        if part.is_symlink():
            raise ValueError('linked retained input: ' + relative)
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_size > 512 * 1024:
        raise ValueError('invalid retained input: ' + relative)
    data = path.read_bytes()
    if sha(data) != expected:
        raise ValueError('retained input hash mismatch: ' + relative)
    return data


def provenance(run, artifact):
    if (run['id'] != RUN or run['head_sha'] != HEAD or run['event'] != 'workflow_dispatch'
            or run['repository']['full_name'] != 'skyRolly/Anamorph'
            or artifact['id'] != ARTIFACT or artifact['expired']
            or artifact['name'] != 'Anamorph-Rosetta-launch-control'
            or artifact['workflow_run']['id'] != RUN or artifact['workflow_run']['head_sha'] != HEAD):
        raise ValueError('retained artifact/run/head binding rejected')


def prepare(source, destination, run, artifact):
    destination.mkdir(mode=0o700, parents=True, exist_ok=False)
    record = {'status': 'in_progress', 'run': RUN, 'artifact': ARTIFACT, 'producer_head': HEAD,
              'transport': 'pinned download-artifact extraction; ZIP bytes not downloaded or claimed verified',
              'files': {}, 'binary_bytes_changed': False, 'native_targets_executed': 0}
    try:
        if 'artifacts' in artifact:
            matches = [a for a in artifact['artifacts'] if a['id'] == ARTIFACT]
            if len(matches) != 1: raise ValueError('retained artifact listing ID is missing or duplicated')
            artifact = matches[0]
        provenance(run, artifact)
        for name, expected in INPUTS.items():
            data = verified_bytes(source, name, expected)
            target = destination/name
            target.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
            target.write_bytes(data)
            mode = 0o700 if name in ('hello-arm64', 'hello-x86_64') else 0o600
            target.chmod(mode)  # Only fresh task-owned copies, no byte/signature change.
            assert sha(target.read_bytes()) == expected
            record['files'][name] = {'sha256': expected, 'bytes': len(data),
                'downloaded_mode': oct(stat.S_IMODE((source/name).stat().st_mode)), 'prepared_mode': oct(mode)}
        original = json.loads((destination/'launch-control.json').read_text())
        if original['consumer_head'] != HEAD or int(original['run']) != RUN:
            raise ValueError('original report source/run rejected')
        record['status'] = 'verified'
        return record
    except Exception as error:
        record.update(status='incomplete', error=type(error).__name__ + ': ' + str(error))
        raise
    finally:
        (destination/'preparation.json').write_text(json.dumps(record, indent=2) + '\n')


def logging_plan(inventory, help_text):
    channels = {}; current = None
    for line in inventory.splitlines():
        header = re.search(r"Logging categories for ['\"]([^'\"]+)['\"]:", line)
        if header:
            current = header[1]; channels[current] = set()
        elif current:
            category = re.match(r'\s+([a-z][a-z-]*)\s+-\s+', line)
            if category: channels[current].add(category[1])
    plan = [('lldb', ['process', 'host']), ('gdb-remote', ['packets'])]
    for channel, categories in plan:
        if not set(categories) <= channels.get(channel, set()):
            raise ValueError('unsupported launch logging: ' + channel)
    for flag in ('-a', '-T', '-f'):
        if not re.search(r'(?m)^\s*' + re.escape(flag) + r'(?:\s|$)', help_text):
            raise ValueError('unsupported log enable option: ' + flag)
    return plan


def trace_commands(path, plan):
    quoted = '"' + str(path).replace('\\', '\\\\').replace('"', '\\"') + '"'
    return (''.join('log enable -a -T -f ' + quoted + ' ' + channel + ' ' + ' '.join(categories) + '\n'
                    for channel, categories in plan)
            + launch.COMMANDS.replace('run\n', 'settings show plugin.process.gdb-remote.packet-timeout\nrun\n'))


def trace_outcome(result, output, debugger=False):
    base = launch.outcome(result, output, debugger)
    if result.get('output_limit'):
        base['outcome'] = 'output_budget_exceeded'
    elif debugger and base['outcome'] == 'complete' and not result.get('trace_bytes'):
        base['outcome'] = 'logging_unverified'
    base['first_mismatch'] = next((s for s in output.splitlines() if 'SCOPE_MISMATCH_BITS' in s), None)
    if base['first_mismatch'] and base['outcome'] == 'complete':
        base['outcome'] = 'unexpected_scope_output'
    return base


def traced_controls(run, lldb, inputs, commands, record, save, classify=trace_outcome):
    plan = [('direct-x86_64', ['/usr/bin/arch', '-x86_64', str(inputs/'hello-x86_64')], 5, False),
            ('lldb-arm64', [lldb, '--no-lldbinit', '--batch', '--arch', 'arm64', '-s', str(commands['arm64']), '--', str(inputs/'hello-arm64')], 20, True),
            ('lldb-x86_64', [lldb, '--no-lldbinit', '--batch', '--arch', 'x86_64', '-s', str(commands['x86_64']), '--', str(inputs/'hello-x86_64')], 30, True)]
    record['controls'] = {name: {'outcome': 'not_run', 'reason': 'prior prerequisites incomplete'} for name, *_ in plan}
    for name, argv, cap, debugger in plan:
        result, output = run(name, argv, cap, observe=True, traced=debugger)
        result.update(classify(result, output, debugger))
        record['controls'][name] = result
        save()
        if result['outcome'] != 'complete':
            record['classification'] = name + ':' + result['outcome']
            for later in record['controls'].values():
                if later['outcome'] == 'not_run': later['reason'] = record['classification']
            return False
    record['classification'] = 'retained_tiny_trace_controls_completed'
    return True


def verify_route(workflow, selector):
    for event, branch, repo in [
        ('workflow_dispatch', 'codex/rosetta-scope-characterization', 'skyRolly/Anamorph'),
        ('push', 'codex/rosetta-scope-characterization', 'skyRolly/Anamorph'),
        ('pull_request', 'codex/rosetta-scope-characterization', 'skyRolly/Anamorph'),
        ('workflow_dispatch', 'main', 'skyRolly/Anamorph'),
        ('workflow_dispatch', 'codex/other', 'skyRolly/Anamorph'),
        ('workflow_dispatch', 'codex/rosetta-scope-characterization', 'fork/Anamorph')]:
        expected = ['handshake-trace'] if (event, branch, repo) == ('workflow_dispatch', 'codex/rosetta-scope-characterization', 'skyRolly/Anamorph') else []
        actual = selector(workflow, {'github.event_name': event, 'github.ref': 'refs/heads/' + branch,
                                    'github.repository': repo, 'inputs.mode': 'handshake-trace'})
        assert actual == expected, 'handshake trace route rejected: ' + repr(actual)
    launch.verify_route(workflow, selector)
    body = workflow.split('  handshake-trace:\n', 1)[1]
    for required in ('timeout-minutes: 10', 'actions: read', 'artifact-ids: 12657441997', 'run-id: 40338130238',
                     '--prepare-inputs retained-download', '--inputs retained-inputs --report-dir handshake-evidence'):
        assert required in body, 'trace job prerequisite missing: ' + required
    for forbidden in ('cmake ', 'brew ', 'original-source', 'diagnose-rosetta-scope.py --source',
                      'diagnose-rosetta-launch.py --report-dir', 'sudo ', 'debugserver --'):
        assert forbidden not in body, 'old execution reached trace job: ' + forbidden


def route_liveness(workflow, selector, verifier=verify_route):
    verifier(workflow, selector)
    broken = workflow.replace("inputs.mode == 'handshake-trace'", "inputs.mode == 'never-selected'", 1)
    assert broken != workflow
    try:
        verifier(broken, selector)
    except AssertionError as error:
        assert str(error).startswith('handshake trace route rejected:'), str(error)
    else:
        raise AssertionError('trace verifier accepted missing job')


def owned_rows(text, pgid, leader, known):
    rows = {}
    for line in text.splitlines():
        fields = line.split(None, 9)
        if len(fields) == 10:
            pid, ppid, group = map(int, fields[:3])
            rows[pid] = {'pid': pid, 'ppid': ppid, 'pgid': group, 'session_field': fields[3],
                         'start_label': ' '.join(fields[4:9]), 'executable_path_observed': fields[9]}
    owned = {p for p, r in rows.items() if p == leader or r['pgid'] == pgid}
    for pid, identity in known.items():
        if pid in rows and all(rows[pid][k] == identity[k] for k in ('start_label', 'executable_path_observed')):
            owned.add(pid)
    for _ in rows:
        children = {p for p, r in rows.items() if r['ppid'] in owned}
        if children <= owned: break
        owned |= children
    for pid in owned: known[pid] = rows[pid]
    return {'owned': [rows[p] for p in sorted(owned)],
            'unmatched_backend_candidates': [dict(r, association='unknown; name is not ownership')
                for p, r in rows.items() if p not in owned and Path(r['executable_path_observed']).name == 'debugserver']}


class Driver:
    """The tiny trace's fixed deadline, observations and unchanged group cleanup."""
    def __init__(self, out):
        self.started = time.monotonic(); self.deadline = self.started + 120
        self.out = out; out.mkdir(mode=0o700, parents=True, exist_ok=False)
        self.env = dict(os.environ)
        self.record = {'consumer_head': os.environ.get('GITHUB_SHA'), 'run': os.environ.get('GITHUB_RUN_ID'),
            'status': 'in_progress', 'classification': 'prerequisites_incomplete', 'budget_seconds': 120,
            'commands': {}, 'controls': {n: {'outcome': 'not_run', 'reason': 'prerequisites incomplete'}
                for n in ('direct-x86_64', 'lldb-arm64', 'lldb-x86_64')},
            'outside_group_signals': False, 'host_security_settings_changed': False,
            'service_cleanup_scope': 'Known invocation group only; unseen/reparented services unverified. Read runner post-job cleanup.'}

    def save(self):
        self.record['elapsed_seconds'] = round(time.monotonic()-self.started, 6)
        (self.out/'handshake.json').write_text(json.dumps(self.record, indent=2) + '\n')

    def remaining(self, reserve=0):
        return max(0, self.deadline-time.monotonic()-reserve)

    def digest(self, path, deadline=None):
        h = hashlib.sha256()
        with Path(path).open('rb') as stream:
            while True:
                if time.monotonic() >= min(self.deadline, deadline or self.deadline):
                    raise RuntimeError('driver/observation deadline exhausted while hashing')
                data = stream.read(1024*1024)
                if not data: return h.hexdigest()
                h.update(data)

    def observe(self, pgid, pid, known, deadline):
        limit = min(0.35, max(0, deadline-time.monotonic()))
        if limit <= 0: raise RuntimeError('identity observation deadline exhausted')
        table = subprocess.run(['/bin/ps', '-ww', '-axo', 'pid=,ppid=,pgid=,sess=,lstart=,comm='],
                               text=True, capture_output=True, timeout=limit)
        if table.returncode: raise RuntimeError('identity ps failed: ' + table.stderr)
        result = owned_rows(table.stdout, pgid, pid, known)
        # A ps-reported executable path plus its current file hash is stronger
        # than a configured candidate, but is not a kernel/memory image hash.
        for row in result['owned'] + result['unmatched_backend_candidates']:
            path = Path(row['executable_path_observed'])
            row['executing_memory_image_digest'] = None
            if path.is_absolute() and path.is_file() and self.remaining(1) > 0:
                try: row['file_sha256_at_observed_path'] = self.digest(path, deadline)
                except OSError as error: row['file_hash_error'] = str(error)
        return result

    def run(self, label, argv, cap, observe=False, traced=False):
        began = time.monotonic(); limit = min(cap, self.remaining(cleanup.CLEANUP_SECONDS))
        execution_end = began + limit; cleanup_end = min(self.deadline, execution_end + cleanup.CLEANUP_SECONDS)
        result = {'argv': list(map(str, argv)), 'timeout_seconds': round(limit, 6), 'exit_code': None,
                  'signal': None, 'timed_out': False, 'cleanup_complete': False,
                  'start_offset_seconds': round(began-self.started, 6), 'identity_observations': []}
        log = self.out/(label+'.log'); trace = self.out/(label+'.trace')
        if traced: trace.write_bytes(b'')
        self.record['commands'][label] = result
        self.save()  # The attempted argv/deadline survive an interrupted wait.
        known = {}; inspection_errors = []
        def sample(stage, process, end):
            entry = {'stage': stage, 'offset_seconds': round(time.monotonic()-self.started, 6)}
            try: entry.update(self.observe(process.pid, process.pid, known, min(end, self.deadline)))
            except Exception as error:
                entry['error'] = type(error).__name__ + ': ' + str(error)
                inspection_errors.append(entry['error'])
            result['identity_observations'].append(entry)
            return entry
        def output_limit():
            return log.stat().st_size >= TRACE_CAP or (traced and trace.stat().st_size >= TRACE_CAP)
        with log.open('wb') as output:
            if limit <= 0:
                result['budget_exhausted'] = True
            else:
                try:
                    def child_file_limit():
                        resource.setrlimit(resource.RLIMIT_FSIZE, (TRACE_CAP, TRACE_CAP))
                    process = subprocess.Popen(result['argv'], stdout=output, stderr=subprocess.STDOUT,
                        start_new_session=True, env=self.env, preexec_fn=child_file_limit if traced else None)
                    try:
                        schedule = iter((2, 8, 18)); next_sample = next(schedule, None)
                        if observe: sample('after_spawn', process, execution_end)
                        while process.poll() is None:
                            now = time.monotonic()
                            if output_limit(): result['output_limit'] = True; break
                            with log.open('rb') as current:
                                current.seek(max(0, log.stat().st_size-8192))
                                partial = current.read(8192).decode(errors='replace')
                            if launch.outcome({}, partial)['outcome'] == 'explicit_denial':
                                result['explicit_denial_observed'] = True; break
                            if now >= execution_end: result['timed_out'] = True; break
                            if observe and next_sample is not None and now-began >= next_sample:
                                sample('live_' + str(next_sample), process, execution_end)
                                next_sample = next(schedule, None)
                            time.sleep(min(0.05, max(0, execution_end-time.monotonic())))
                    except Exception as error:
                        result['execution_wait_error'] = type(error).__name__ + ': ' + str(error)
                    finally:
                        if observe: sample('before_termination', process, min(cleanup_end-4, time.monotonic()+0.35))
                        result.update(cleanup.cleanup_process(process, process.pid, cleanup_end))
                        if observe:
                            final = sample('after_cleanup', process, min(self.deadline, time.monotonic()+0.35))
                            if final.get('owned') or final.get('unmatched_backend_candidates'):
                                inspection_errors.append('live owned identities or unknown backend candidates remain; no outside-group signal attempted')
                            if inspection_errors:
                                result['cleanup_complete'] = False
                                result['cleanup_error'] = '; '.join(filter(None, [result.get('cleanup_error')] + inspection_errors))
                except Exception as error:
                    result['launch_error'] = type(error).__name__ + ': ' + str(error)
        result['output_limit'] = result.get('output_limit', False) or output_limit()
        result.update(log=log.name, log_sha256=sha(log.read_bytes()), elapsed_seconds=round(time.monotonic()-began, 6))
        if traced:
            result.update(trace=trace.name, trace_bytes=trace.stat().st_size, trace_sha256=sha(trace.read_bytes()),
                          trace_limit_bytes=TRACE_CAP, trace_limit_enforcement='child-only RLIMIT_FSIZE, one shared append trace file; bounded polling')
        self.record['commands'][label] = result; self.save()
        return result, log.read_text(errors='replace')

    def required(self, label, argv, cap=4):
        result, output = self.run(label, argv, cap)
        if not cleanup.process_ok(result) or result.get('output_limit'):
            raise RuntimeError(label + ':metadata_prerequisite_unmet')
        return output.strip()


def native(inputs, out):
    driver = Driver(out); record = driver.record; driver.save()
    try:
        if (sys.platform != 'darwin' or platform.machine() != 'arm64'
                or os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted'
                or os.environ.get('GITHUB_REPOSITORY') != 'skyRolly/Anamorph'
                or os.environ.get('GITHUB_REF') != 'refs/heads/codex/rosetta-scope-characterization'
                or os.environ.get('GITHUB_EVENT_NAME') != 'workflow_dispatch'):
            raise RuntimeError('genuine_disposable_host_prerequisite_unmet')
        record['retained_producer'] = {'head': HEAD, 'run': RUN, 'artifact': ARTIFACT}
        record['input_preparation'] = json.loads((inputs/'preparation.json').read_text())
        if record['input_preparation']['status'] != 'verified': raise ValueError('input preparation incomplete')
        for name, expected in INPUTS.items(): verified_bytes(inputs, name, expected)
        record['input_hashes'] = INPUTS; record['driver_sha256'] = driver.digest(__file__)
        record['host'] = {'platform': platform.platform(), 'architecture': platform.machine(),
                          'image': os.environ.get('ImageOS'), 'image_version': os.environ.get('ImageVersion')}
        preferred = next((p for p in (Path('/Applications/Xcode_26.6.app/Contents/Developer'),
                                      Path('/Applications/Xcode_26.6.0.app/Contents/Developer')) if p.is_dir()), None)
        if preferred: driver.env['DEVELOPER_DIR'] = str(preferred)
        record['command_DEVELOPER_DIR'] = driver.env.get('DEVELOPER_DIR')
        for label, argv in [('macOS',['/usr/bin/sw_vers']), ('Xcode',['/usr/bin/xcodebuild','-version']),
                            ('global_developer_selection',['/usr/bin/xcode-select','-p']),
                            ('SDK_path',['/usr/bin/xcrun','--show-sdk-path']), ('SDK_version',['/usr/bin/xcrun','--show-sdk-version'])]:
            record[label] = driver.required(label, argv)
        record['tools'] = {}
        for name in ('clang', 'lldb'):
            path = driver.required(name+'-path', ['/usr/bin/xcrun','--find',name])
            record['tools'][name] = {'path': path, 'resolved_path': str(Path(path).resolve()),
                                    'sha256': driver.digest(path), 'version': driver.required(name+'-version',[path,'--version'])}
        lldb = record['tools']['lldb']['path']
        developer = Path(driver.env.get('DEVELOPER_DIR') or record['global_developer_selection'])
        candidates = [Path(lldb).parent/'debugserver', developer.parent/'SharedFrameworks/LLDB.framework/Resources/debugserver',
                      developer/'Toolchains/XcodeDefault.xctoolchain/usr/libexec/debugserver']
        record['debugserver_candidates'] = [{'path': str(p), 'resolved_path': str(p.resolve()), 'sha256': driver.digest(p),
            'scope': 'on-disk candidate, not executing-image identity'} for p in candidates if p.is_file()]
        help_text = driver.required('lldb-help',[lldb,'--help'])
        for option in ('--no-lldbinit','--batch','--arch'):
            if option not in help_text: raise RuntimeError('unsupported LLDB option: ' + option)
        inventory = driver.required('lldb-log-inventory',[lldb,'--no-lldbinit','--batch', '-o','log list'])
        log_help = driver.required('lldb-log-help',[lldb,'--no-lldbinit','--batch','-o','help log enable'])
        plan = logging_plan(inventory, log_help); record['logging_plan'] = plan
        packet = driver.required('packet-timeout',[lldb,'--no-lldbinit','--batch','-o','settings show plugin.process.gdb-remote.packet-timeout'])
        if not re.search(r'packet-timeout[^\n]*=\s*\d+', packet): raise RuntimeError('packet-timeout setting unverified')
        record['packet_timeout_unmodified'] = packet
        record['binaries'] = {}; commands = {}
        for arch in ('arm64','x86_64'):
            binary = inputs/('hello-'+arch); dwarf = inputs/('hello-'+arch+'.dSYM/Contents/Resources/DWARF/hello-'+arch)
            actual = driver.required('architecture-'+arch,['/usr/bin/lipo','-archs',str(binary)])
            uuid = driver.required('UUID-'+arch,['/usr/bin/xcrun','dwarfdump','--uuid',str(binary)])
            dsym = driver.required('dSYM-UUID-'+arch,['/usr/bin/xcrun','dwarfdump','--uuid',str(dwarf)])
            if actual != arch or UUIDS[arch] not in uuid or UUIDS[arch] not in dsym:
                raise RuntimeError('retained architecture/UUID prerequisite rejected: ' + arch)
            record['binaries'][arch] = {'sha256': driver.digest(binary), 'actual_architecture': actual,
                'UUID': uuid, 'dSYM_UUID': dsym, 'dSYM_DWARF_sha256': driver.digest(dwarf), 'complete_symbol_coverage_claimed': False}
            command = out/('lldb-'+arch+'.commands'); commands[arch] = command
            command.write_text(trace_commands(out/('lldb-'+arch+'.trace'), plan))
            record['binaries'][arch]['trace_command_sha256'] = driver.digest(command)
        driver.save()
        done = traced_controls(driver.run, lldb, inputs, commands, record, driver.save)
        for name, expected in INPUTS.items(): verified_bytes(inputs, name, expected)
        record['input_bytes_unchanged_after_controls'] = True
        record['status'] = 'completed' if done else 'incomplete'
    except Exception as error:
        record.update(status='incomplete', classification=str(error), error=type(error).__name__ + ': ' + str(error))
    finally:
        driver.save()
    if record['status'] != 'completed': print('handshake trace incomplete: ' + record['classification'], file=sys.stderr)
    return 0 if record['status'] == 'completed' else 1


def self_test(report):
    good = {'exit_code': 0, 'cleanup_complete': True, 'trace_bytes': 12}
    output = 'target.disable-aslr (boolean) = false\n' + launch.MARKER + '\nProcess 123 exited with status = 0 (0x00000000)\n'
    class Inert(unittest.TestCase):
        def test_production_route_and_noop_rejection(self):
            workflow = (ROOT/'.github/workflows/rosetta-scope.yml').read_text()
            selector = runpy.run_path(str(ROOT/'scripts/test-workflow-routing.py'))['selection']
            route_liveness(workflow, selector)
            with self.assertRaisesRegex(AssertionError, 'verifier accepted missing job'):
                route_liveness(workflow, selector, verifier=lambda *args: None)
        def test_supported_logging_and_unchanged_settings(self):
            inventory = "Logging categories for 'lldb':\n  process - Processes\n  host - Launch\nLogging categories for 'gdb-remote':\n  packets - Packets\n"
            plan = logging_plan(inventory, '  -a ( --append )\n  -T ( --timestamp )\n  -f <file> ( --file )\n')
            with self.assertRaisesRegex(ValueError, 'unsupported launch logging'):
                logging_plan(inventory.replace('packets -', 'never -'), '  -a\n  -T\n  -f\n')
            with self.assertRaisesRegex(ValueError, 'unsupported log enable option'):
                logging_plan(inventory, '  -a\n  -f\n')
            text = trace_commands(Path('/private/space path/test.trace'), plan)
            self.assertIn('"/private/space path/test.trace"', text)
            self.assertTrue(text.endswith(launch.COMMANDS.replace('run\n','settings show plugin.process.gdb-remote.packet-timeout\nrun\n')))
            self.assertNotIn('settings set plugin.process', text)
        def test_order_caps_and_failure_withholds_later_controls(self):
            for change in ({}, {'timed_out': True}, {'output_limit': True}, {'cleanup_complete': False}, {'budget_exhausted': True}):
                calls = []; record = {}
                def run(name, argv, cap, **kwargs):
                    calls.append((name, cap)); return dict(good, **change), output
                done = traced_controls(run, 'inert-lldb', Path('/inert'), dict(arm64='', x86_64=''), record, lambda: None)
                self.assertEqual(done, not change)
                self.assertEqual([c[1] for c in calls], [5] if change else [5,20,30])
                if change:
                    self.assertEqual(record['controls']['lldb-arm64']['outcome'], 'not_run')
                    self.assertIn('direct-x86_64:', record['controls']['lldb-arm64']['reason'])
        def test_completion_trace_and_timeout_remain_independent(self):
            self.assertEqual(trace_outcome(good, output, True)['outcome'], 'complete')
            for result, text, state in [(good, '', 'missing_completion'),
                (dict(good,timed_out=True),output,'timeout'),(dict(good,output_limit=True),output,'output_budget_exceeded'),
                (dict(good,trace_bytes=0),output,'logging_unverified'),
                (good,'error: process launch failed: Operation not permitted','explicit_denial')]:
                self.assertEqual(trace_outcome(result,text,True)['outcome'], state)
            # A disabled classifier must be rejected on the same production path.
            with self.assertRaises(AssertionError):
                assert trace_outcome(dict(good,timed_out=True),output,True)['outcome'] == 'complete'
        def test_later_timeout_does_not_erase_completed_controls(self):
            for stop in ('lldb-arm64','lldb-x86_64'):
                record={};calls=[]
                def run(name,argv,cap,**kwargs):
                    calls.append(name);return dict(good,timed_out=name==stop),output
                self.assertFalse(traced_controls(run,'inert',Path('/inert'),dict(arm64='',x86_64=''),record,lambda:None))
                self.assertEqual(record['controls']['direct-x86_64']['outcome'],'complete')
                self.assertEqual(record['controls'][stop]['outcome'],'timeout')
                if stop=='lldb-arm64':
                    self.assertEqual(record['controls']['lldb-x86_64']['outcome'],'not_run')
                    self.assertEqual(len(calls),2)
                else: self.assertEqual(record['controls']['lldb-arm64']['outcome'],'complete')
        def test_identity_unknown_is_not_owned_by_name_or_pid_order(self):
            text = '10 1 10 0 Mon Oct 26 10:00:00 2026 /tool/lldb\n11 10 10 0 Mon Oct 26 10:00:01 2026 /tool/debugserver\n12 1 12 0 Mon Oct 26 10:00:02 2026 /tool/debugserver\n'
            known = {}; result = owned_rows(text,10,10,known)
            self.assertEqual([r['pid'] for r in result['owned']], [10,11])
            self.assertEqual(result['unmatched_backend_candidates'][0]['pid'],12)
            later = owned_rows(text.replace('11 10 10','11 1 30'),10,10,known)
            self.assertIn(11,[r['pid'] for r in later['owned']])
            reused = owned_rows(text.replace('10:00:01','10:01:01').replace('11 10 10','11 1 30'),10,10,known)
            self.assertNotIn(11,[r['pid'] for r in reused['owned']])
        def test_retained_provenance_and_snapshot_hash(self):
            run = {'id':RUN,'head_sha':HEAD,'event':'workflow_dispatch','repository':{'full_name':'skyRolly/Anamorph'}}
            artifact = {'id':ARTIFACT,'expired':False,'name':'Anamorph-Rosetta-launch-control','workflow_run':{'id':RUN,'head_sha':HEAD}}
            provenance(run,artifact)
            with self.assertRaisesRegex(ValueError,'binding rejected'): provenance(dict(run,head_sha='wrong'),artifact)
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); (root/'inert').write_bytes(b'inert bytes, never executed')
                expected = sha((root/'inert').read_bytes())
                verified_bytes(root,'inert',expected)
                (root/'inert').write_bytes(b'changed')
                with self.assertRaisesRegex(ValueError,'hash mismatch'): verified_bytes(root,'inert',expected)
        def test_actual_runner_deadline_observation_cap_and_cleanup(self):
            for fault in ('timeout','output','observation','unknown_backend'):
                with tempfile.TemporaryDirectory() as tmp:
                    driver = Driver(Path(tmp)/'out'); tick = [100.0]
                    driver.started=100; driver.deadline=220
                    class Process:
                        pid=4321
                        def poll(self): return None
                    snapshots=[]; cleanup_calls=[]
                    def observe(*args):
                        snapshots.append(tick[0])
                        if fault=='observation': raise RuntimeError('inert ps failure')
                        return {'owned':[],'unmatched_backend_candidates':[{'pid':999,'association':'unknown'}] if fault=='unknown_backend' else []}
                    def clean(process,pgid,deadline):
                        cleanup_calls.append((pgid,deadline))
                        return {'exit_code':-15,'signal':15,'child_reaped':True,'cleanup_complete':True}
                    def sleep(amount):
                        tick[0]+=amount
                        if fault=='output':
                            with (driver.out/'lldb-x86_64.trace').open('wb') as f: f.truncate(TRACE_CAP)
                    driver.observe=observe
                    with mock.patch.object(time,'monotonic',side_effect=lambda:tick[0]), mock.patch.object(time,'sleep',side_effect=sleep), \
                         mock.patch.object(subprocess,'Popen',return_value=Process()), mock.patch.object(cleanup,'cleanup_process',side_effect=clean):
                        result,_=driver.run('lldb-x86_64',['inert-lldb'],30,observe=True,traced=True)
                    self.assertEqual(len(cleanup_calls),1)
                    self.assertEqual(cleanup_calls[0][0],4321)
                    self.assertLessEqual(cleanup_calls[0][1],220)
                    self.assertLessEqual(len(snapshots),6)
                    if fault=='timeout':
                        self.assertTrue(result['timed_out']);self.assertTrue(any(t>=118 for t in snapshots))
                    elif fault=='output': self.assertTrue(result['output_limit'])
                    elif fault=='observation': self.assertFalse(result['cleanup_complete']);self.assertIn('inert ps failure',result['cleanup_error'])
                    else: self.assertFalse(result['cleanup_complete']);self.assertIn('unknown backend',result['cleanup_error'])
        def test_shared_deadline_reserves_cleanup_and_withholds_spawn(self):
            with tempfile.TemporaryDirectory() as tmp:
                driver=Driver(Path(tmp)/'out');tick=[100.0];driver.started=100;driver.deadline=108
                class Process:
                    pid=4321
                    def poll(self):return None
                def sleep(amount):tick[0]+=amount
                with mock.patch.object(time,'monotonic',side_effect=lambda:tick[0]), mock.patch.object(time,'sleep',side_effect=sleep), \
                     mock.patch.object(subprocess,'Popen',return_value=Process()) as spawn, \
                     mock.patch.object(cleanup,'cleanup_process',return_value={'exit_code':-15,'cleanup_complete':True}):
                    result,_=driver.run('first',['inert'],30)
                    second,_=driver.run('second',['inert'],30)
                self.assertEqual(result['timeout_seconds'],2)
                self.assertTrue(second['budget_exhausted'])
                self.assertEqual(spawn.call_count,1)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Inert))
    if report:
        report.parent.mkdir(parents=True,exist_ok=True)
        report.write_text(json.dumps({'kind':'inert retained-input/trace/routing/deadline/identity controls',
            'tests_run':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),
            'native_controls_executed':0,'real_processes_spawned':0},indent=2)+'\n')
    return result.wasSuccessful()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test',action='store_true');parser.add_argument('--report',type=Path)
    parser.add_argument('--prepare-inputs',type=Path);parser.add_argument('--prepared-inputs',type=Path)
    parser.add_argument('--run-metadata',type=Path);parser.add_argument('--artifact-metadata',type=Path)
    parser.add_argument('--inputs',type=Path);parser.add_argument('--report-dir',type=Path)
    args=parser.parse_args()
    if args.self_test: return 0 if self_test(args.report) else 1
    if args.prepare_inputs is not None:
        if not all([args.prepared_inputs,args.run_metadata,args.artifact_metadata]): parser.error('input preparation metadata/destination required')
        prepare(args.prepare_inputs.resolve(),args.prepared_inputs.resolve(),json.loads(args.run_metadata.read_text()),json.loads(args.artifact_metadata.read_text()))
        return 0
    if args.inputs is None or args.report_dir is None: parser.error('--inputs and --report-dir required')
    return native(args.inputs.resolve(),args.report_dir.resolve())


if __name__ == '__main__':
    raise SystemExit(main())
