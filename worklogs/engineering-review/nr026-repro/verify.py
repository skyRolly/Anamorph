#!/usr/bin/env python3
"""Verify retained NR-026 bytes and the four distinct result oracles."""
import gzip
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
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
print('NR-026 evidence: retained hashes, source identity and four result oracles verified')
