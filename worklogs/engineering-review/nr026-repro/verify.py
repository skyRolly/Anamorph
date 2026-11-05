#!/usr/bin/env python3
"""Verify retained NR-026 bytes and the four distinct result oracles."""
import argparse
import gzip
import hashlib
import json
import shutil
import tempfile
from pathlib import Path

if not __debug__:
    raise SystemExit('evidence verification requires assertions enabled')


def verify(root):
    def retained_stream(directory, name):
        path = directory / name
        if path.is_file():
            return path.read_bytes()
        return gzip.decompress((directory / (name + '.gz')).read_bytes())

    ledger = json.loads((root / 'worklogs/engineering-review/nr026-juce905.json').read_text())
    record = ledger['new_observation']
    for entry in record['materials']:
        path = root / entry['path']
        assert path.is_file(), entry['path']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry['sha256'], entry['path']

    historic = json.loads((root / 'worklogs/engineering-review/gui-restore-lifetime.json').read_text())
    original = historic['reproduction_sources']['gui-inventory-probe-v2.cpp']
    assert (root / 'worklogs/engineering-review/nr026-repro/original-probe.cpp').read_bytes() == original['content'].encode()
    manifest = json.loads((root / record['command_manifest']).read_text())
    assert hashlib.sha256((root / record['command_manifest']).read_bytes()).hexdigest() == record['command_manifest_sha256']
    assert manifest['product_commit'] == ledger['product_source']
    assert manifest['juce_commit'] == ledger['dependency']['commit']
    manifest_results = {item['case']: item for item in manifest['results']}
    recorded_cases = {item['case']: item for item in record['raw_cases']}
    for case in ('standalone-live', 'standalone-close', 'held-live', 'held-close'):
        command = next(x for x in manifest['commands'] if x['name'] == case)
        result = manifest_results[case]
        recorded = recorded_cases[case]
        assert command['exit'] == recorded['exit'] == result['exit']
        assert command['stdout'] == recorded['stdout']
        assert command['stderr'] == recorded['stderr']
        assert command['stdout_sha256'] == recorded['stdout_sha256']
        assert command['stderr_sha256'] == recorded['stderr_sha256']
        assert result['source_sha256'] == recorded['source_sha256']
        assert result['binary_sha256'] == recorded['binary_sha256']
        assert result['expected_outcome_observed'] == recorded['expected_outcome_observed']
        source_name = 'original-probe.cpp' if case.startswith('standalone') else 'held-wheel-probe.cpp'
        source = root / 'worklogs/engineering-review/nr026-repro' / source_name
        assert hashlib.sha256(source.read_bytes()).hexdigest() == recorded['source_sha256']
        stdout = (root / 'worklogs/engineering-review/nr026-repro/observed-2026-11-05' / command['stdout']).read_bytes()
        stderr_path = root / 'worklogs/engineering-review/nr026-repro/observed-2026-11-05' / command['stderr']
        stderr_bytes = stderr_path.read_bytes()
        stderr = gzip.decompress(stderr_bytes) if stderr_path.suffix == '.gz' else stderr_bytes
        assert hashlib.sha256(stdout).hexdigest() == command['stdout_sha256']
        assert hashlib.sha256(stderr).hexdigest() == command['stderr_sha256']
        if case.endswith('live'):
            assert command['exit'] == 0 and not stderr
            assert b'width=0.53' in stdout and b'begins=1' in stdout and b'ends=1' in stdout
        else:
            assert command['exit'] == 1 and b'AddressSanitizer: heap-use-after-free' in stderr
            token = b'ScopedDragNotification::~ScopedDragNotification' if case.startswith('standalone') else b'Knob::takeWheelNotch'
            assert token in stderr
    suite = record['state_suite']
    state_dir = root / 'worklogs/engineering-review/nr026-repro/observed-2026-11-05'
    state = (state_dir / suite['stdout']).read_bytes()
    state_stderr = (state_dir / suite['stderr']).read_bytes()
    assert hashlib.sha256(state).hexdigest() == suite['stdout_sha256']
    assert hashlib.sha256(state_stderr).hexdigest() == suite['stderr_sha256']
    assert suite['checks'] == 13959 and suite['failures'] == 0 and not state_stderr
    assert b'13959 checks, 0 failure(s)' in state

    isolated = ledger['isolated_observation']
    for entry in isolated['runner_inputs'] + isolated['materials'] + [isolated['command_manifest']]:
        path = root / entry['path']
        assert path.is_file(), entry['path']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry['sha256'], entry['path']
    directory = (root / isolated['command_manifest']['path']).parent
    current = json.loads((root / isolated['command_manifest']['path']).read_text())
    assert current['schema'] == 2
    assert current['checkout_head'] == isolated['execution_checkout']
    assert current['product_commit'] == isolated['product_source'] == ledger['product_source']
    assert current['juce_commit'] == isolated['juce_commit'] == ledger['dependency']['commit']
    assert current['state_suite'] == isolated['state_suite'] == {'exit': 0, 'checks': 13959, 'failures': 0}
    assert current['results'] == isolated['cases']
    commands = {entry['name']: entry for entry in current['commands']}
    assert len(commands) == len(current['commands'])
    for entry in current['commands']:
        assert 'exit' in entry and 'interrupted_or_failed_to_start' not in entry
        for stream in ('stdout', 'stderr'):
            raw = retained_stream(directory, entry[stream])
            assert hashlib.sha256(raw).hexdigest() == entry[stream + '_sha256']
    controls = commands['isolation-controls']
    control_output = (directory / controls['stderr']).read_bytes()
    assert controls['exit'] == 0 and b'Ran 7 tests' in control_output and b'\nOK\n' in control_output
    refresh = isolated['control_refresh']
    for entry in (refresh['source'], refresh['stdout'], refresh['stderr']):
        assert hashlib.sha256((root / entry['path']).read_bytes()).hexdigest() == entry['sha256']
    refreshed_output = (root / refresh['stderr']['path']).read_bytes()
    assert refresh['exit'] == 0 and refresh['tests'] == 8
    assert b'Ran 8 tests' in refreshed_output and b'\nOK\n' in refreshed_output
    assert b'test_legacy_xdg_only_profile_still_resolves_caller_home' in refreshed_output

    profiles = []
    for name in ('state-suite', 'standalone-live', 'standalone-close', 'held-live', 'held-close'):
        entry = commands[name]
        env = entry['environment']
        paths = entry['resolved_paths']
        home = Path(env['HOME'])
        profile = home.parent
        profiles.append(profile)
        assert profile.parent == Path('/tmp') and profile.name.startswith('anamorph-nr026-')
        assert home.name == 'home'
        config = home / '.config'
        assert paths == {'home': str(home), 'application_data': str(config),
                         'preset': str(config / 'RollyTech/Anamorph/Presets'),
                         'temporary': str(profile / 'tmp')}
        assert env['XDG_CONFIG_HOME'] == str(config) and env['TMPDIR'] == str(profile / 'tmp')
        for key, name_part in (('XDG_DATA_HOME', 'data'), ('XDG_CACHE_HOME', 'cache'),
                               ('XDG_STATE_HOME', 'state'), ('XDG_RUNTIME_DIR', 'runtime')):
            assert env[key] == str(profile / name_part)
    assert len(set(profiles)) == 5
    state_entry = commands['state-suite']
    state_output = (directory / state_entry['stdout']).read_bytes()
    assert state_entry['exit'] == 0 and not (directory / state_entry['stderr']).read_bytes()
    assert all(token in state_output for token in (b'State test 8:', b'State test 86:', b'State test 88:',
                                                    b'13959 checks, 0 failure(s)'))
    results = {entry['case']: entry for entry in current['results']}
    assert set(results) == {'standalone-live', 'standalone-close', 'held-live', 'held-close'}
    for name, result in results.items():
        entry = commands[name]
        assert result['exit'] == entry['exit'] and result['expected_outcome_observed'] is True
        source_name = 'original-probe.cpp' if name.startswith('standalone') else 'held-wheel-probe.cpp'
        assert result['source_sha256'] == hashlib.sha256((directory.parent / source_name).read_bytes()).hexdigest()
        stdout = (directory / entry['stdout']).read_bytes()
        stderr = retained_stream(directory, entry['stderr'])
        if name.endswith('live'):
            assert entry['exit'] == 0 and not stderr
            assert all(token in stdout for token in (b'width=0.53', b'begins=1', b'ends=1'))
        else:
            location = (b'ScopedDragNotification::~ScopedDragNotification' if name.startswith('standalone')
                        else b'Knob::takeWheelNotch')
            assert entry['exit'] == 1 and b'AddressSanitizer: heap-use-after-free' in stderr and location in stderr


def self_test(root):
    with tempfile.TemporaryDirectory(prefix='nr026-evidence-control-', dir='/tmp') as tmp:
        test_root = Path(tmp)
        source = root / 'worklogs/engineering-review'
        target = test_root / 'worklogs/engineering-review'
        target.parent.mkdir(parents=True)
        shutil.copytree(source, target, ignore=shutil.ignore_patterns('__pycache__'))
        verify(test_root)
        paths = [target / 'nr026-repro/observed-2026-11-05/standalone-close.stderr.gz',
                 target / 'nr026-repro/observed-isolated-2026-11-05/standalone-live.stdout',
                 target / 'nr026-repro/observed-isolated-2026-11-05/manifest.json',
                 target / 'nr026-repro/path-probe.cpp']
        for path in paths:
            original = path.read_bytes()
            path.write_bytes(original + b'altered')
            try:
                verify(test_root)
            except AssertionError:
                pass
            else:
                raise AssertionError(f'failed to reject modified evidence: {path.name}')
            finally:
                path.write_bytes(original)
        ledger_path = target / 'nr026-juce905.json'
        original = ledger_path.read_bytes()
        changed = json.loads(original)
        changed['isolated_observation']['cases'][0]['exit'] = 1
        ledger_path.write_text(json.dumps(changed))
        try:
            verify(test_root)
        except AssertionError:
            pass
        else:
            raise AssertionError('failed to reject altered case classification')
    print('NR-026 evidence verifier: five corruption controls passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[3]
    if args.self_test:
        self_test(repository)
    else:
        verify(repository)
        print('NR-026 evidence: historical and isolated identities and result oracles verified')
