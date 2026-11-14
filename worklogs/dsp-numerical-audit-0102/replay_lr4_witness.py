#!/usr/bin/env python3
"""Rebuild the committed isolated LR4 witness with its committed JUCE stub."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from replay_provenance import BASE, verify_source_tree
from rebuild_probe import sha, version
from replay_experiment import blob


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--checkout', type=Path, required=True)
    p.add_argument('--packet-commit', required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    checkout, out = a.checkout.resolve(), a.out.resolve()
    if out.exists() or out.is_relative_to(checkout):
        raise ValueError('output must be new and outside the source checkout')
    tree = verify_source_tree(checkout, BASE)
    inputs = ['worklogs/h4-review-followup-0102/lr4_hidden_state.cpp',
              'worklogs/h4-review-followup-0102/stub/juce_dsp/juce_dsp.h']
    out.mkdir(parents=True)
    packet = out/'packet'; hashes = {}
    for name in inputs:
        data = blob(a.packet_commit, name)
        dest = packet/name; dest.parent.mkdir(parents=True, exist_ok=True); dest.write_bytes(data)
        hashes[name] = hashlib.sha256(data).hexdigest()
    tool_paths = [Path(__file__).resolve(), Path(__file__).with_name('replay_provenance.py'),
                  Path(__file__).with_name('rebuild_probe.py'), Path(__file__).with_name('replay_experiment.py')]
    tools = {str(path): sha(path) for path in tool_paths}
    stub = packet/'worklogs/h4-review-followup-0102/stub'
    source = packet/inputs[0]; binary = out/'lr4-witness'
    compile_cmd = ['g++', '-std=c++17', '-O2', '-ffp-contract=off', '-I'+str(stub),
                   '-I'+str(checkout), str(source), '-o', str(binary)]
    subprocess.run(compile_cmd, cwd=out, check=True)
    binary_hash = sha(binary)
    result = subprocess.run([str(binary)], cwd=out, capture_output=True)
    stdout, stderr = out/'stdout', out/'stderr'
    stdout.write_bytes(result.stdout); stderr.write_bytes(result.stderr)
    if verify_source_tree(checkout, BASE) != tree or sha(binary) != binary_hash:
        raise ValueError('source or binary changed during execution')
    if any(sha(path) != digest for path, digest in tools.items()):
        raise ValueError('witness tool changed during execution')
    for name, digest in hashes.items():
        if sha(packet/name) != digest:
            raise ValueError('committed probe or stub changed during execution')
    record = {'source_head': BASE, 'source_tree': tree, 'packet_commit': a.packet_commit,
        'inputs_sha256': hashes, 'tool_sha256': tools, 'compiler_version': version('g++'),
        'compile': compile_cmd, 'binary_sha256': binary_hash, 'run': [str(binary)],
        'cwd': str(out), 'exit': result.returncode, 'stdout_sha256': sha(stdout), 'stderr_sha256': sha(stderr),
        'environment': {key: os.environ[key] for key in ('PATH','CC','CXX','CFLAGS','CXXFLAGS','CPATH',
                        'CPLUS_INCLUDE_PATH','LIBRARY_PATH','LANG','LC_ALL') if key in os.environ},
        'dependency': 'committed minimal JUCE MathConstants stub; not a JUCE production build'}
    (out/'identity.json').write_text(json.dumps(record, indent=2)+'\n')
    print(result.stdout.decode(errors='replace'), end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
