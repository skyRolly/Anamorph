#!/usr/bin/env python3
"""Run a committed experiment specification using a new, verified CMake build.

The specification is reviewed evidence, not proof that its selected patch
expresses the intended experiment. All supplied probes and diagnostic patches
must be blobs in committed Git trees; linked build objects are newly produced.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import subprocess

from replay_provenance import JUCE, cache_values, fresh_checkout_build, git, verify_source_tree
from rebuild_probe import link_objects, sha, version


REPO = Path(__file__).resolve().parents[2]


def blob(commit, name):
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise ValueError('packet commit must be a full SHA-1 commit')
    path = PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts or not path.parts:
        raise ValueError('invalid packet path')
    actual = git(REPO, 'rev-parse', commit+'^{commit}').decode().strip()
    if actual != commit:
        raise ValueError('packet does not identify an exact commit')
    meta = git(REPO, 'ls-tree', commit, '--', name)
    if not meta.startswith((b'100644 blob ', b'100755 blob ')):
        raise ValueError('packet input must be a committed regular file: ' + name)
    return git(REPO, 'show', commit+':'+name)


def main():
    tools = {str(Path(__file__).resolve()): sha(__file__),
             str(Path(__file__).with_name('replay_provenance.py').resolve()):
                 sha(Path(__file__).with_name('replay_provenance.py')),
             str(Path(__file__).with_name('rebuild_probe.py').resolve()):
                 sha(Path(__file__).with_name('rebuild_probe.py'))}
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkout', type=Path, required=True)
    parser.add_argument('--juce', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--spec-commit', required=True)
    parser.add_argument('--spec-path', required=True)
    args = parser.parse_args()
    checkout, juce, out = args.checkout.resolve(), args.juce.resolve(), args.out.resolve()
    spec_bytes = blob(args.spec_commit, args.spec_path)
    spec = json.loads(spec_bytes)
    if not spec.get('purpose') or not spec.get('targets') or not spec.get('runs'):
        raise ValueError('experiment must specify its purpose, targets and runs')
    if out.exists() or out.is_relative_to(checkout) or out.is_relative_to(juce):
        raise ValueError('output must be a new directory outside both source trees')
    out.mkdir(parents=True)
    (out/'spec.json').write_bytes(spec_bytes)
    patch_path = None
    patch_digest = None
    if 'patch' in spec:
        patch = spec['patch']
        patch_bytes = blob(args.spec_commit if patch['commit'] == 'spec' else patch['commit'], patch['path'])
        patch_digest = hashlib.sha256(patch_bytes).hexdigest()
        if patch_digest != patch['sha256'] or not patch.get('purpose'):
            raise ValueError('diagnostic patch identity or purpose mismatch')
        patch_path = out/'diagnostic.patch'; patch_path.write_bytes(patch_bytes)
    expected = spec['source_head']
    actual_tree = verify_source_tree(checkout, expected, patch_path, patch_digest)
    if actual_tree != spec['source_tree']:
        raise ValueError('selected diagnostic or clean source tree does not match the reviewed specification')
    identity = fresh_checkout_build(checkout, expected, juce, out/'fresh-build',
                                    spec['targets'], patch_path, patch_digest)
    build = Path(identity['build_directory'])
    cache = cache_values(build)
    compiler = cache.get('CMAKE_CXX_COMPILER')
    if not compiler:
        raise ValueError('configured compiler is not recorded')
    identity.update({'spec_commit': args.spec_commit, 'spec_path': args.spec_path,
        'spec_sha256': hashlib.sha256(spec_bytes).hexdigest(), 'purpose': spec['purpose'],
        'checkout': str(checkout), 'juce': str(juce), 'compiler': compiler,
        'compiler_version': version(compiler), 'cmake_version': version('cmake'),
        'ninja_version': version('ninja'), 'cache_sha256': sha(build/'CMakeCache.txt'),
        'rules_sha256': sha(build/'build.ninja'), 'compile_commands_sha256': sha(build/'compile_commands.json'),
        'environment': {key: os.environ[key] for key in ('PATH', 'CC', 'CXX', 'CFLAGS', 'CXXFLAGS',
                        'LDFLAGS', 'CPATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH', 'LANG', 'LC_ALL')
                        if key in os.environ}, 'tool_sha256': tools,
        'inputs': {}, 'targets_detail': {}, 'probes': {}, 'runs': []})
    executables = {target: build/(target+'_artefacts/Release/'+target) for target in spec['targets']}
    for target, binary in executables.items():
        commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', target], text=True).splitlines()
        link = next(shlex.split(line) for line in commands if ' -o '+target+'_artefacts/Release/'+target+' ' in line)
        if link[:2] == [':', '&&'] and link[-2:] == ['&&', ':']:
            link = link[2:-2]
        identity['targets_detail'][target] = {'binary_sha256': sha(binary), 'ninja_commands': commands,
                                               'link_objects_sha256': link_objects(link, build)}
    for probe in spec.get('probes', []):
        name, target = probe['name'], probe['target']
        if not re.fullmatch('[a-zA-Z0-9_-]+', name) or target not in executables:
            raise ValueError('invalid probe name or target')
        commit = args.spec_commit if probe['commit'] == 'spec' else probe['commit']
        packet_root = out/('packet-'+name)
        for filename in [probe['path'], *probe.get('support', [])]:
            data = blob(commit, filename)
            destination = packet_root/filename
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
            identity['inputs'][commit+':'+filename] = hashlib.sha256(data).hexdigest()
        source = packet_root/probe['path']
        commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', target], text=True).splitlines()
        main_source = 'state_tests.cpp' if target == 'AnamorphStateTests' else 'dsp_tests.cpp'
        compile_cmd = next(shlex.split(line) for line in commands if line.endswith('/tests/'+main_source) and ' -c ' in line)
        link = next(shlex.split(line) for line in commands if ' -o '+target+'_artefacts/Release/'+target+' ' in line)
        if link[:2] == [':', '&&'] and link[-2:] == ['&&', ':']:
            link = link[2:-2]
        old_obj = compile_cmd[compile_cmd.index('-o')+1]
        new_obj = out/(name+'.o')
        for flag, value in [('-o', str(new_obj)), ('-c', str(source)), ('-MF', str(new_obj)+'.d'), ('-MT', str(new_obj))]:
            if flag in compile_cmd:
                compile_cmd[compile_cmd.index(flag)+1] = value
        subprocess.run(compile_cmd, cwd=build, check=True)
        binary = out/name
        link = [str(new_obj) if value == old_obj else value for value in link]
        link[link.index('-o')+1] = str(binary)
        objects = link_objects(link, build)
        subprocess.run(link, cwd=build, check=True)
        executables[name] = binary
        identity['probes'][name] = {'packet_commit': commit, 'source': probe['path'],
            'compile': compile_cmd, 'link': link, 'link_objects_sha256': objects,
            'binary_sha256': sha(binary), 'ninja_commands': commands}
    failed = False
    for number, run in enumerate(spec['runs']):
        binary = executables[run['program']]
        if not binary.is_file():
            raise ValueError('specified executable does not exist: '+str(binary))
        binary_sha = sha(binary)
        # Substitution is argument-wise, never through a shell. It permits the
        # verified checkout's own State isolation wrapper to run the binary.
        def expand(value):
            return value.replace('{checkout}', str(checkout)).replace('{build}', str(build)).replace('{out}', str(out))
        command = [*(expand(value) for value in run.get('prefix', [])), str(binary),
                   *(expand(value) for value in run.get('args', []))]
        result = subprocess.run(command, cwd=out, capture_output=True)
        stdout, stderr = out/(f'run-{number}.stdout'), out/(f'run-{number}.stderr')
        stdout.write_bytes(result.stdout); stderr.write_bytes(result.stderr)
        identity['runs'].append({'argv': command, 'cwd': str(out), 'binary_sha256': binary_sha,
            'exit': result.returncode, 'expected_exit': run['expected_exit'],
            'stdout_sha256': sha(stdout), 'stderr_sha256': sha(stderr)})
        failed |= result.returncode != run['expected_exit']
        if sha(binary) != binary_sha:
            raise ValueError('executable changed during execution')
        if verify_source_tree(checkout, expected, patch_path, patch_digest) != actual_tree:
            raise ValueError('source changed during execution')
        verify_source_tree(juce, JUCE)
        if any(sha(path) != digest for path, digest in tools.items()):
            raise ValueError('replay tool changed during execution')
        (out/'identity.json').write_text(json.dumps(identity, indent=2)+'\n')
        print('run', number, 'exit', result.returncode, 'expected', run['expected_exit'], flush=True)
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
