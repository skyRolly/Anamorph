#!/usr/bin/env python3
"""Validate one recorded recovery-cost experiment, not compiler/host provenance."""
import gzip
import hashlib
import json
from pathlib import Path
import re
import sys


def validate(root):
    root = Path(root)
    identity = json.loads((root/'identity.json').read_text())
    if len(identity['runs']) != 4 or (root/'inner.exit').read_text().strip() != '0':
        raise ValueError('missing runs or nonzero inner runner exit')
    names = ('normal', 'recovery', 'wrapped-normal', 'wrapped-recovery')
    rows = []
    for i, (name, run) in enumerate(zip(names, identity['runs'])):
        if run['exit'] != 0 or run['expected_exit'] != 0:
            raise ValueError(name+': unexpected executable exit')
        stdout = (root/f'run-{i}.stdout').read_bytes()
        stderr = (root/f'run-{i}.stderr').read_bytes()
        if hashlib.sha256(stdout).hexdigest() != run['stdout_sha256'] or hashlib.sha256(stderr).hexdigest() != run['stderr_sha256']:
            raise ValueError(name+': stream digest mismatch')
        inject, wrapped = i % 2 == 1, i >= 2
        required = ['256 measured blocks', f'injected={int(inject)}', f'recovered_blocks={256 if inject else 0}',
                    'active_blocks=256', f'reset_blocks={256 if inject else 0}',
                    f'sanitized_blocks={256 if inject else 0}', f'wrapped_blocks={256 if wrapped else 0}',
                    'nonfinite_output=0']
        if any(marker.encode() not in stdout for marker in required):
            raise ValueError(name+': required observed recovery path missing')
        checksum = re.search(rb'checksum=([^\s]+)', stdout)
        artifact = 'callgrind-'+name
        item = run['result_validation']['artifacts'][artifact]
        data = gzip.decompress((root/(artifact+'.gz')).read_bytes())
        if len(data) != item['size'] or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError(name+': callgrind digest mismatch')
        count = re.search(rb'(?m)^summary:\s*(\d+)\s*$', data)
        if not count or not checksum:
            raise ValueError(name+': missing count or checksum')
        rows.append({'case': name, 'instructions': int(count.group(1)), 'checksum': checksum.group(1).decode()})
    if rows[0]['checksum'] != rows[1]['checksum'] or rows[2]['checksum'] != rows[3]['checksum']:
        raise ValueError('normal and recovery wet checksums differ')
    return {'rows': rows, 'delta_instructions_per_block': [(rows[1]['instructions']-rows[0]['instructions'])/256,
                                                           (rows[3]['instructions']-rows[2]['instructions'])/256]}


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: validate_h4_recovery_cost.py RESULTS_DIR OUTPUT.json')
    result = validate(sys.argv[1])
    Path(sys.argv[2]).write_text(json.dumps(result, indent=2)+'\n')
    print('four recovery-cost cases, artifact hashes and observed execution premises verified')
