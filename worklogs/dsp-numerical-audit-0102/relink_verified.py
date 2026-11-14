#!/usr/bin/env python3
"""Relink only against a committed receipt for a previously fresh build.

This verifies the exact source/JUCE trees, generated rules, recorded commands,
all link objects and baseline executable before and after use. It trusts the
recorded build process, local tools, system dependencies and absence of writers;
it is not a compiler, host or historical-execution attestation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

from replay_provenance import JUCE, cache_values, git, verify_source_tree
from rebuild_probe import link_objects, sha, version
from replay_experiment import blob


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--checkout', type=Path, required=True)
    p.add_argument('--juce', type=Path, required=True)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--out', type=Path)
    p.add_argument('--receipt-commit', required=True)
    p.add_argument('--receipt-path', required=True)
    p.add_argument('--spec-commit', required=True)
    p.add_argument('--spec-path', required=True)
    p.add_argument('--diagnostic-checkout', type=Path)
    p.add_argument('--check-only', action='store_true')
    a = p.parse_args()
    checkout, juce, build = a.checkout.resolve(), a.juce.resolve(), a.build.resolve()
    receipt_bytes = blob(a.receipt_commit, a.receipt_path)
    receipt = json.loads(receipt_bytes)
    spec_bytes = blob(a.spec_commit, a.spec_path)
    spec = json.loads(spec_bytes)
    target = 'AnamorphTests'
    if (receipt['source_head'] != spec['source_head'] or receipt['patch_sha256'] is not None
            or receipt['juce_head'] != JUCE or target not in receipt['targets_detail']):
        raise ValueError('receipt is not for this clean source, JUCE and target')
    if Path(receipt['build_directory']).resolve() != build or Path(receipt['checkout']).resolve() != checkout:
        raise ValueError('receipt identifies a different build or source directory')
    if Path(receipt['juce']).resolve() != juce:
        raise ValueError('receipt identifies another JUCE source')
    base_tree = receipt['source_tree']
    recorded = receipt['targets_detail'][target]
    tools = {str(path): sha(path) for path in (Path(__file__).resolve(),
        Path(__file__).with_name('replay_provenance.py'), Path(__file__).with_name('rebuild_probe.py'),
        Path(__file__).with_name('replay_experiment.py'))}

    def verify_build():
        if verify_source_tree(checkout, receipt['source_head']) != base_tree:
            raise ValueError('source differs from recorded fresh build')
        if verify_source_tree(juce, JUCE) != receipt['juce_tree']:
            raise ValueError('JUCE differs from recorded fresh build')
        cache = cache_values(build)
        if (Path(cache.get('CMAKE_HOME_DIRECTORY', '/nonexistent')).resolve() != checkout
                or Path(cache.get('ANAMORPH_JUCE_PATH', '/nonexistent')).resolve() != juce):
            raise ValueError('build cache identifies a different source or dependency')
        for filename, key in [('CMakeCache.txt', 'cache_sha256'), ('build.ninja', 'rules_sha256'),
                              ('compile_commands.json', 'compile_commands_sha256')]:
            if sha(build/filename) != receipt[key]:
                raise ValueError('generated build input changed: '+filename)
        if version(cache['CMAKE_CXX_COMPILER']) != receipt['compiler_version']:
            raise ValueError('effective compiler version differs from receipt')
        commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', target], text=True).splitlines()
        if commands != recorded['ninja_commands']:
            raise ValueError('effective generated build commands differ from receipt')
        for filename, digest in recorded['link_objects_sha256'].items():
            path = Path(filename).resolve()
            if not path.is_relative_to(build) or not path.is_file() or sha(path) != digest:
                raise ValueError('recorded fresh-build object changed or unavailable: '+filename)
        binary = build/(target+'_artefacts/Release/'+target)
        if sha(binary) != recorded['binary_sha256']:
            raise ValueError('recorded fresh-build executable changed')
        if any(sha(path) != digest for path, digest in tools.items()):
            raise ValueError('relink tool changed during execution')
        return commands

    commands = verify_build()
    if a.check_only:
        print('verified recorded fresh build, source, JUCE, rules and link objects')
        return 0
    if a.out is None:
        raise ValueError('--out is required for relinking')
    out = a.out.resolve()
    if out.exists() or out.is_relative_to(checkout) or out.is_relative_to(juce) or out.is_relative_to(build):
        raise ValueError('output must be new and outside source, JUCE and build')
    out.mkdir(parents=True)
    (out/'spec.json').write_bytes(spec_bytes)
    probe = spec['probes'][0]
    if len(spec['probes']) != 1 or probe['target'] != target:
        raise ValueError('this relinker supports exactly one DSP probe')
    commit = a.spec_commit if probe['commit'] == 'spec' else probe['commit']
    packet = out/'packet'; inputs = {}
    for name in [probe['path'], *probe.get('support', [])]:
        data = blob(commit, name); destination = packet/name
        destination.parent.mkdir(parents=True, exist_ok=True); destination.write_bytes(data)
        inputs[commit+':'+name] = hashlib.sha256(data).hexdigest()
    source = packet/probe['path']
    cc = next(shlex.split(line) for line in commands if line.endswith('/tests/dsp_tests.cpp') and ' -c ' in line)
    link = next(shlex.split(line) for line in commands if ' -o '+target+'_artefacts/Release/'+target+' ' in line)
    if link[:2] == [':', '&&'] and link[-2:] == ['&&', ':']:
        link = link[2:-2]
    old = cc[cc.index('-o')+1]; obj = out/'probe.o'
    for flag, value in [('-o', str(obj)), ('-c', str(source)), ('-MF', str(obj)+'.d'), ('-MT', str(obj))]:
        if flag in cc: cc[cc.index(flag)+1] = value
    subprocess.run(cc, cwd=build, check=True)
    link = [str(obj) if value == old else value for value in link]
    engine_record = None
    if 'patch' in spec:
        if a.diagnostic_checkout is None:
            raise ValueError('a diagnostic checkout matching the committed patch is required')
        diagnostic = a.diagnostic_checkout.resolve()
        patch_spec = spec['patch']; patch_commit = a.spec_commit if patch_spec['commit'] == 'spec' else patch_spec['commit']
        patch_bytes = blob(patch_commit, patch_spec['path'])
        if hashlib.sha256(patch_bytes).hexdigest() != patch_spec['sha256']:
            raise ValueError('diagnostic patch differs from reviewed bytes')
        patch = out/'diagnostic.patch'; patch.write_bytes(patch_bytes)
        tree = verify_source_tree(diagnostic, spec['source_head'], patch, patch_spec['sha256'])
        changed = git(diagnostic, 'diff-tree', '--no-commit-id', '--name-only', '-r', spec['source_head'], tree).decode().splitlines()
        if tree != spec['source_tree'] or changed != ['src/dsp/AnamorphEngine.cpp']:
            raise ValueError('controlled reuse requires exactly the reviewed engine TU change')
        engine = diagnostic/'src/dsp/AnamorphEngine.cpp'
        if b'__FILE__' in engine.read_bytes() or b'__BASE_FILE__' in engine.read_bytes():
            raise ValueError('changed source location can affect compiled semantics')
        ec = next(shlex.split(line) for line in commands if line.endswith('/src/dsp/AnamorphEngine.cpp') and ' -c ' in line)
        previous = ec[ec.index('-o')+1]; engine_obj = out/'engine.o'
        for flag, value in [('-o', str(engine_obj)), ('-c', str(engine)), ('-MF', str(engine_obj)+'.d'), ('-MT', str(engine_obj))]:
            if flag in ec: ec[ec.index(flag)+1] = value
        subprocess.run(ec, cwd=build, check=True)
        link = [str(engine_obj) if value == previous else value for value in link]
        engine_record = {'source_tree': tree, 'checkout': str(diagnostic), 'source_sha256': sha(engine),
                         'patch_sha256': patch_spec['sha256'], 'compile': ec}
    elif spec['source_tree'] != base_tree:
        raise ValueError('unpatched experiment source differs from recorded build')
    binary = out/probe['name']; link[link.index('-o')+1] = str(binary)
    objects = link_objects(link, build)
    subprocess.run(link, cwd=build, check=True)
    record = {'receipt_commit': a.receipt_commit, 'receipt_path': a.receipt_path,
        'receipt_sha256': hashlib.sha256(receipt_bytes).hexdigest(), 'spec_commit': a.spec_commit,
        'spec_path': a.spec_path, 'spec_sha256': hashlib.sha256(spec_bytes).hexdigest(),
        'source_head': spec['source_head'], 'source_tree': spec['source_tree'], 'juce_head': JUCE,
        'build': str(build), 'tool_sha256': tools, 'packet_inputs': inputs, 'probe_compile': cc,
        'engine': engine_record, 'link': link, 'link_objects_sha256': objects,
        'binary_sha256': sha(binary), 'runs': []}
    failed = False
    for i, run in enumerate(spec['runs']):
        if run['program'] != probe['name']:
            raise ValueError('run selects a different program')
        command = [str(binary), *run.get('args', [])]
        result = subprocess.run(command, cwd=out, capture_output=True)
        stdout, stderr = out/(f'run-{i}.stdout'), out/(f'run-{i}.stderr')
        stdout.write_bytes(result.stdout); stderr.write_bytes(result.stderr)
        record['runs'].append({'argv': command, 'exit': result.returncode,
            'expected_exit': run['expected_exit'], 'stdout_sha256': sha(stdout), 'stderr_sha256': sha(stderr)})
        failed |= result.returncode != run['expected_exit']
        verify_build()
        if sha(binary) != record['binary_sha256'] or link_objects(link, build) != objects:
            raise ValueError('relinked executable or inputs changed')
        if engine_record and verify_source_tree(diagnostic, spec['source_head'], patch, patch_spec['sha256']) != tree:
            raise ValueError('diagnostic source changed during execution')
        (out/'identity.json').write_text(json.dumps(record, indent=2)+'\n')
        print('run', i, 'exit', result.returncode, 'expected', run['expected_exit'], flush=True)
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
