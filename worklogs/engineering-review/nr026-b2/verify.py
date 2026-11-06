#!/usr/bin/env python3
"""Verify retained B2 evidence; not implementation approval or a native-host claim."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def verify(root):
    manifest = json.loads((root / 'execution.json').read_text())
    for name, expected in manifest['payloads_sha256'].items():
        if digest((root / name).read_bytes()) != expected:
            raise ValueError('payload digest mismatch: ' + name)
    build = json.loads((root / 'build.json').read_text())
    if build['source_commit'] != manifest['source_commit'] or build['binary_sha256'] != manifest['binary_sha256']:
        raise ValueError('source/build identity mismatch')
    if digest((root / 'test-header.cpp.executed.txt').read_bytes()) != manifest['test_header_sha256']:
        raise ValueError('executed test identity mismatch')
    patch = gzip.decompress((root / 'production-source.patch.gz').read_bytes())
    if digest(patch) != manifest['production_patch_sha256']:
        raise ValueError('production source patch identity mismatch')
    streams = json.loads(gzip.decompress((root / 'streams.json.gz').read_bytes()))
    for name, count in [('positive', 2115), ('full-state', 16074), ('focused', 2443), ('dsp', 945)]:
        output = streams[name]['stdout']
        summaries = re.findall(r'(\d+) checks, (\d+) failure', output)
        if not summaries or tuple(map(int, summaries[-1])) != (count, 0) or '[FAIL]' in output:
            raise ValueError('positive result changed: ' + name)
    if streams['positive']['stdout'].count('B2 target=') != 166 or streams['positive']['stderr']:
        raise ValueError('positive lifecycle matrix incomplete or detector output appeared')
    controls = json.loads((root / 'mutations.json').read_text())
    required = {'missing-end', 'duplicate-end', 'lost-endpoint', 'wrong-redo',
                'closed-record-reuse', 'lost-bank-continuation', 'missing-press-staging'}
    if {x['case'] for x in controls} != required or len(controls) != 7:
        raise ValueError('negative-control matrix changed')
    for item in controls:
        name = item['case']
        stream = streams[name]
        for kind in ['stdout', 'stderr']:
            if digest(stream[kind].encode()) != item[kind + '_sha256']:
                raise ValueError('control stream mismatch: ' + name)
        archive = gzip.decompress((root / (name + '.patch.gz')).read_bytes())
        if digest(archive) != item['patch_sha256']:
            raise ValueError('control source identity mismatch: ' + name)
        asan = 'ERROR: AddressSanitizer:' in stream['stderr']
        if item['exit'] == 0 or not item['detected'] or asan != item['asan']:
            raise ValueError('control no longer detects incorrect behavior: ' + name)
        if item['test_header_sha256'] != manifest['test_header_sha256']:
            raise ValueError('control used a different oracle')
        if item['expected'] == 'asan':
            if not asan or 'heap-use-after-free' not in stream['stderr']:
                raise ValueError('staging control lacks required ASan witness')
        elif asan or stream['stdout'].count('[FAIL]') != item['assertion_failures'] or item['assertion_failures'] == 0:
            raise ValueError('contract control did not distinguish clean sanitizer exit')
    return 'B2 retained evidence: 2115 positive checks; six contract controls and one ASan control detected'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    if args.self_test:
        verify(root)
        for name in ['test-header.cpp.executed.txt', 'mutations.json', 'streams.json.gz']:
            with tempfile.TemporaryDirectory() as directory:
                copy = Path(directory) / 'bundle'
                shutil.copytree(root, copy)
                (copy / name).write_bytes((copy / name).read_bytes() + b'corruption')
                try:
                    verify(copy)
                except (ValueError, OSError):
                    continue
                raise ValueError('evidence corruption was accepted: ' + name)
        print('B2 bundle verifier: three corruption controls rejected')
    print(verify(root))


if __name__ == '__main__':
    main()
