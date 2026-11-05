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
assert manifest['product_commit'] == ledger['product_source']
assert manifest['juce_commit'] == ledger['dependency']['commit']
for case in ('standalone-live', 'standalone-close', 'held-live', 'held-close'):
    command = next(x for x in manifest['commands'] if x['name'] == case)
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
state = (root / 'worklogs/engineering-review/nr026-repro/observed-2026-11-05/state-suite.stdout').read_bytes()
assert b'13959 checks, 0 failure(s)' in state
print('NR-026 evidence: retained hashes, source identity and four result oracles verified')
