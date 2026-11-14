#!/usr/bin/env python3
"""Replay reviewed one-TU controls using objects built in this invocation.

The committed specification selects exact patches and expected outcomes. A
fresh build produces all baseline objects; only a reviewed engine translation
unit is replaced for each control. No pre-existing objects are accepted.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

AUDIT = Path(__file__).resolve().parents[1]/'dsp-numerical-audit-0102'
sys.path.insert(0, str(AUDIT))
from replay_provenance import JUCE, cache_values, fresh_checkout_build, git, git_environment, verify_source_tree
from rebuild_probe import link_objects, sha, version
from replay_experiment import blob


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--checkout', type=Path, required=True)
    p.add_argument('--juce', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--spec-commit', required=True)
    p.add_argument('--spec-path', required=True)
    a = p.parse_args()
    checkout, juce, out = a.checkout.resolve(), a.juce.resolve(), a.out.resolve()
    tools = {str(path): sha(path) for path in [Path(__file__).resolve(),
             AUDIT/'replay_provenance.py', AUDIT/'replay_experiment.py', AUDIT/'rebuild_probe.py']}
    raw_spec = blob(a.spec_commit, a.spec_path)
    spec = json.loads(raw_spec)
    if not spec.get('purpose') or not spec.get('variants') or not spec.get('runs'):
        raise ValueError('incomplete reviewed control specification')
    if out.exists() or out.is_relative_to(checkout) or out.is_relative_to(juce):
        raise ValueError('output must be new and outside both source trees')
    out.mkdir(parents=True)
    (out/'spec.json').write_bytes(raw_spec)
    base_head = spec['source_head']
    if verify_source_tree(checkout, base_head) != spec['source_tree']:
        raise ValueError('base source tree does not match reviewed specification')
    targets = ['AnamorphTests', 'AnamorphStateTests']
    identity = fresh_checkout_build(checkout, base_head, juce, out/'fresh-build', targets)
    build = Path(identity['build_directory'])
    cache = cache_values(build); compiler = cache['CMAKE_CXX_COMPILER']
    identity.update({'purpose': spec['purpose'], 'spec_commit': a.spec_commit, 'spec_path': a.spec_path,
        'spec_sha256': hashlib.sha256(raw_spec).hexdigest(), 'tool_sha256': tools,
        'checkout': str(checkout), 'juce': str(juce), 'compiler': compiler,
        'compiler_version': version(compiler), 'cmake_version': version('cmake'),
        'ninja_version': version('ninja'), 'cache_sha256': sha(build/'CMakeCache.txt'),
        'rules_sha256': sha(build/'build.ninja'), 'compile_commands_sha256': sha(build/'compile_commands.json'),
        'environment': {key: os.environ[key] for key in ('PATH', 'CC', 'CXX', 'CFLAGS', 'CXXFLAGS',
            'LDFLAGS', 'CPATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH', 'LANG', 'LC_ALL') if key in os.environ},
        'targets_detail': {}, 'variants': {}, 'runs': []})
    commands = {}; compile_cmds = {}; links = {}; binaries = {}
    for target in targets:
        commands[target] = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', target], text=True).splitlines()
        compile_cmds[target] = next(shlex.split(line) for line in commands[target]
                                    if line.endswith('/src/dsp/AnamorphEngine.cpp') and ' -c ' in line)
        link = next(shlex.split(line) for line in commands[target] if ' -o '+target+'_artefacts/Release/'+target+' ' in line)
        if link[:2] == [':', '&&'] and link[-2:] == ['&&', ':']:
            link = link[2:-2]
        links[target] = link
        binary = build/(target+'_artefacts/Release/'+target)
        binaries[('base', target)] = binary
        identity['targets_detail'][target] = {'ninja_commands': commands[target],
            'link_objects_sha256': link_objects(link, build), 'binary_sha256': sha(binary)}
    engine_path = 'src/dsp/AnamorphEngine.cpp'
    for variant in spec['variants']:
        name = variant['name']
        if not re.fullmatch('[a-z0-9-]+', name) or not variant.get('purpose'):
            raise ValueError('invalid variant name or missing purpose')
        commit = a.spec_commit if variant['commit'] == 'spec' else variant['commit']
        patch = blob(commit, variant['path'])
        if hashlib.sha256(patch).hexdigest() != variant['sha256']:
            raise ValueError('variant patch differs from reviewed bytes: '+name)
        with tempfile.TemporaryDirectory(prefix='anamorph-control-index-') as directory:
            env = git_environment(); env['GIT_INDEX_FILE'] = str(Path(directory)/'index')
            subprocess.run(['git', '-C', str(checkout), 'read-tree', base_head], env=env, check=True)
            subprocess.run(['git', '-C', str(checkout), 'apply', '--cached', '--binary', '--unidiff-zero', '-'],
                           env=env, input=patch, check=True)
            tree = subprocess.check_output(['git', '-C', str(checkout), 'write-tree'], env=env, text=True).strip()
        if tree != variant['source_tree']:
            raise ValueError('variant tree differs from reviewed tree: '+name)
        changed = git(checkout, 'diff-tree', '--no-commit-id', '--name-only', '-r', base_head, tree).decode().splitlines()
        if changed != [engine_path]:
            raise ValueError('controlled object reuse requires exactly the engine TU to change')
        engine = git(checkout, 'show', tree+':'+engine_path)
        if b'__FILE__' in engine or b'__BASE_FILE__' in engine:
            raise ValueError('relocating this translation unit can change its compiled semantics')
        directory = out/name; directory.mkdir()
        source = directory/'AnamorphEngine.cpp'; source.write_bytes(engine)
        entry = {'purpose': variant['purpose'], 'patch_commit': commit, 'patch_path': variant['path'],
                 'patch_sha256': variant['sha256'], 'source_tree': tree,
                 'engine_sha256': sha(source), 'targets': {}}
        for target in targets:
            cc = compile_cmds[target].copy(); old_obj = cc[cc.index('-o')+1]
            obj = directory/(target+'.o')
            for flag, value in [('-o', str(obj)), ('-c', str(source)), ('-MF', str(obj)+'.d'), ('-MT', str(obj))]:
                if flag in cc: cc[cc.index(flag)+1] = value
            compile_log = directory/(target+'-compile.log')
            with compile_log.open('wb') as log:
                subprocess.run(cc, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
            link = [str(obj) if value == old_obj else value for value in links[target]]
            binary = directory/target; link[link.index('-o')+1] = str(binary)
            objects = link_objects(link, build)
            subprocess.run(link, cwd=build, check=True)
            binaries[(name, target)] = binary
            entry['targets'][target] = {'compile': cc, 'compile_log_sha256': sha(compile_log),
                'link': link, 'link_objects_sha256': objects, 'binary_sha256': sha(binary)}
        identity['variants'][name] = entry
        print('built', name, tree, flush=True)
    failed = False
    for i, run in enumerate(spec['runs']):
        binary = binaries[(run['variant'], run['target'])]
        command = (['xvfb-run', '-a'] if run['target'] == 'AnamorphStateTests' else []) + [str(binary), run['selector']]
        result = subprocess.run(command, cwd=out, capture_output=True)
        stdout, stderr = out/(f'run-{i}.stdout'), out/(f'run-{i}.stderr')
        stdout.write_bytes(result.stdout); stderr.write_bytes(result.stderr)
        summaries = re.findall(rb'(?m)^\s*(\d+) checks, (\d+) failures?\s*$', result.stdout)
        observed = tuple(map(int, summaries[-1])) if summaries else None
        expected = (run['checks'], run['failures'])
        expected_exit = 0 if run['failures'] == 0 else 1
        correct = observed == expected and result.returncode == expected_exit
        failed |= not correct
        identity['runs'].append({'variant': run['variant'], 'target': run['target'],
            'argv': command, 'binary_sha256': sha(binary), 'exit': result.returncode,
            'expected_exit': expected_exit, 'observed_summary': observed, 'expected_summary': expected,
            'stdout_sha256': sha(stdout), 'stderr_sha256': sha(stderr), 'matches_expectation': correct})
        if verify_source_tree(checkout, base_head) != spec['source_tree']:
            raise ValueError('base source changed during control execution')
        verify_source_tree(juce, JUCE)
        if any(sha(path) != digest for path, digest in tools.items()):
            raise ValueError('replay tool changed during execution')
        (out/'identity.json').write_text(json.dumps(identity, indent=2)+'\n')
        print(run['variant'], run['target'], run['selector'], observed, 'exit', result.returncode, flush=True)
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
