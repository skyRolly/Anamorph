#!/usr/bin/env python3
"""Compare recorded instruction data; this is not a build-provenance verifier."""
import gzip
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent
CASES = ('band1', 'band2', 'band3', 'band4', 'wrapped4', 'partial4', 'fade4', 'glide4')
VARIANTS = ('baseline', 'a2', 'candidate')


def inspect(variant):
    root = ROOT/f'h4-instruction-{variant}-results'
    record = json.loads((root/'identity.json').read_text())
    if len(record['runs']) != len(CASES):
        raise ValueError(f'{variant}: missing recorded runs')
    if variant == 'baseline':
        events = [json.loads(line) for line in (root/'docker-events.jsonl').read_text().splitlines()]
        if not any(event['Action'] == 'die' and event['Actor']['Attributes'].get('exitCode') == '0' for event in events):
            raise ValueError('baseline container exit not verified')
    elif (root/'outer.exit').read_text().strip() != '0':
        raise ValueError(f'{variant}: nonzero runner exit')
    result = []
    for i, (case, run) in enumerate(zip(CASES, record['runs'])):
        if run['exit'] != 0 or run['expected_exit'] != 0:
            raise ValueError(f'{variant}/{case}: execution failure')
        stdout, stderr = ((root/f'run-{i}.{ext}').read_bytes() for ext in ('stdout', 'stderr'))
        if any(hashlib.sha256(value).hexdigest() != run[key]
               for value, key in ((stdout, 'stdout_sha256'), (stderr, 'stderr_sha256'))):
            raise ValueError(f'{variant}/{case}: stream digest mismatch')
        name = 'callgrind-'+case
        item = run['result_validation']['artifacts'][name]
        data = gzip.decompress((root/(name+'.gz')).read_bytes())
        if len(data) != item['size'] or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError(f'{variant}/{case}: callgrind digest mismatch')
        match = re.search(rb'(?m)^summary:\s*(\d+)\s*$', data)
        checksum = re.search(rb'checksum[= ]([^\s]+)', stdout)
        if not match or not checksum or b'256 measured blocks' not in stdout:
            raise ValueError(f'{variant}/{case}: missing workload data')
        if case == 'wrapped4' and b'wrapped_blocks=256' not in stdout:
            raise ValueError(f'{variant}/{case}: wrapped path did not run')
        if case == 'fade4' and not re.search(rb'fade_blocks=([1-9]\d*)', stdout):
            raise ValueError(f'{variant}/{case}: fade did not run')
        if case == 'glide4' and not re.search(rb'proved sustained-glide samples ([1-9]\d*)', stdout):
            raise ValueError(f'{variant}/{case}: sustained glide not established')
        result.append({'case': case, 'instructions': int(match.group(1)),
                       'checksum': checksum.group(1).decode(), 'observation': stdout.decode().strip()})
    flags = set(record['probes']['count']['compile'])
    if not {'-O3', '-ffp-contract=off', '-march=haswell', '-std=c++23'} <= flags:
        raise ValueError(f'{variant}: unexpected compile flags')
    return record, result


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: compare_h4_instruction_results.py OUTPUT.json')
    all_data = {name: inspect(name) for name in VARIANTS}
    for field in ('source_head', 'juce_head', 'compiler_version', 'cmake_version', 'ninja_version'):
        if len({all_data[name][0][field] for name in VARIANTS}) != 1:
            raise ValueError(f'unmatched {field}')
    rows = []
    for i, case in enumerate(CASES):
        values = {name: all_data[name][1][i] for name in VARIANTS}
        if len({value['checksum'] for value in values.values()}) != 1:
            raise ValueError(f'{case}: workload checksums differ')
        base = values['baseline']['instructions']
        row = {'case': case, 'checksum': values['baseline']['checksum']}
        for name in VARIANTS:
            n = values[name]['instructions']
            row[name] = {'instructions': n, 'per_block': n/256, 'per_sample': n/(256*128),
                         'percent_vs_baseline': 100*(n/base-1), 'observation': values[name]['observation']}
        row['candidate_minus_a2_per_block'] = (values['candidate']['instructions']-values['a2']['instructions'])/256
        rows.append(row)
    out = {'scope': 'recorded instruction artifacts and workload observations, not host or binary attestation',
           'source_head': all_data['baseline'][0]['source_head'],
           'juce_head': all_data['baseline'][0]['juce_head'],
           'compiler_version': all_data['baseline'][0]['compiler_version'], 'rows': rows}
    Path(sys.argv[1]).write_text(json.dumps(out, indent=2)+'\n')
    print(f'{len(rows)} matched cases; artifact hashes, observed paths and checksums verified')


if __name__ == '__main__':
    main()
