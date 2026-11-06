#!/usr/bin/env python3
"""Check the rejected B1 evidence bundle, not production acceptance."""
import argparse
from collections import Counter
import gzip
import hashlib
import json
from pathlib import Path
import shutil
import tempfile


def verify(root):
    manifest = json.loads((root / 'execution.json').read_text())
    for name, expected in manifest['payloads_sha256'].items():
        if hashlib.sha256((root / name).read_bytes()).hexdigest() != expected:
            raise ValueError('payload digest mismatch: ' + name)
    original_patch = gzip.decompress((root / 'b1.patch.gz').read_bytes())
    if hashlib.sha256(original_patch).hexdigest() != manifest['original_patch_sha256']:
        raise ValueError('original patch identity mismatch')
    matrix = json.loads((root / 'matrix.json').read_text())
    streams = json.loads(gzip.decompress((root / 'streams.json.gz').read_bytes()))
    if matrix['binary_sha256'] != manifest['binary_sha256']:
        raise ValueError('binary identity mismatch')
    for file, key in [('probe.cpp.executed.txt', 'probe_sha256'), ('profile.h.executed.txt', 'profile_sha256')]:
        if hashlib.sha256((root / file).read_bytes()).hexdigest() != matrix[key]:
            raise ValueError('executed source identity mismatch')
    expected = {(mode, phase, delta) for mode in ('standalone', 'held')
                for phase in ('live', 'begin', 'value', 'end')
                for delta in ('positive', 'negative', 'horizontal', 'reverse', 'zero', 'duplicate')}
    actual = set()
    counts = Counter()
    for result in matrix['results']:
        tag = result['case']
        if not tag.endswith('-omit-end'):
            _, mode, phase, delta = tag.split('-')
            actual.add((mode, phase, delta))
        data = streams[tag]
        for kind in ('stdout', 'stderr'):
            if hashlib.sha256(data[kind].encode()).hexdigest() != result[kind + '_sha256']:
                raise ValueError('stream digest mismatch: ' + tag)
        if data['stdout'] != result['stdout']:
            raise ValueError('stdout transcription mismatch')
        asan = 'AddressSanitizer' in data['stderr']
        if asan != result['asan']:
            raise ValueError('sanitizer classification mismatch')
        classification = 'asan_failure' if asan else 'pass' if result['exit'] == 0 else 'contract_failure'
        counts[classification] += 1
        if classification == 'pass' and data['stdout'].count('accepted=1') != 2:
            raise ValueError('passing case did not complete two accepted cycles')
        if tag.endswith('-omit-end') and not ('begin=1 end=0' in data['stdout'] and 'undoable=0' in data['stdout'] and result['exit'] != 0):
            raise ValueError('omit-End detector did not fail')
    if actual != expected or len(matrix['results']) != 49 or len(streams) != 49:
        raise ValueError('matrix cases missing, duplicated or unexpected')
    if dict(counts) != {'pass': 31, 'asan_failure': 12, 'contract_failure': 6} or dict(counts) != manifest['results']:
        raise ValueError('rejected prototype classification changed')
    return counts


def self_test(root):
    verify(root)
    for name in ('probe.cpp.executed.txt', 'matrix.json', 'streams.json.gz'):
        with tempfile.TemporaryDirectory() as temp:
            copy = Path(temp) / 'bundle'
            shutil.copytree(root, copy)
            (copy / name).write_bytes((copy / name).read_bytes() + b'corruption')
            try:
                verify(copy)
            except (ValueError, OSError):
                continue
            raise ValueError('corruption accepted: ' + name)
    print('B1 bundle verifier: three source/matrix/stream corruption controls rejected')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    if args.self_test:
        self_test(here)
    print('B1 retained outcomes:', dict(verify(here)))
