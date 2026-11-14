#!/usr/bin/env python3
"""Build archived probes with newly generated baseline objects and an input record."""
import argparse
import hashlib
import json
import os
import shlex
import subprocess
from pathlib import Path

from replay_provenance import (ARCHIVE, BASE, JUCE, cache_values, fresh_build, git,
                               verify_archive_file, verify_inputs, verify_variant)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def version(command):
    return subprocess.check_output([command, '--version'], text=True).splitlines()[0]


def link_objects(command, build):
    result = {}
    for arg in command:
        if arg.endswith(('.o', '.a')):
            path = Path(arg)
            if not path.is_absolute():
                path = build/path
            if not path.is_file():
                raise ValueError('link object is unavailable: ' + str(path))
            result[str(path.resolve())] = sha(path)
    if not result:
        raise ValueError('link command contains no identifiable objects')
    return result


def main():
    tools = {str(Path(__file__).resolve()): sha(__file__),
             str(Path(__file__).with_name('replay_provenance.py').resolve()):
                 sha(Path(__file__).with_name('replay_provenance.py'))}
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkout', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--probe', choices=['probe', 'gate', 'overlap', 'processor', 'os', 'heal', 'drive'], required=True)
    variants = ['baseline', 'live_toggle', 'endpoint', 'always_dry', 'trace', 'os_trace',
                'retain_dry', 'drive_endpoint', 'drive_mix', 'mix_zero', 'mix_one']
    parser.add_argument('--variant', default='baseline', choices=variants)
    parser.add_argument('--also-variant', action='append', default=[], choices=variants)
    parser.add_argument('--run', action='store_true', help='run each resulting executable and retain output')
    parser.add_argument('--run-arg', action='append', default=[], help='argument supplied to each execution')
    args = parser.parse_args()
    checkout, supplied, out = args.checkout.resolve(), args.build.resolve(), args.out.resolve()
    selected = [args.variant, *args.also_variant]
    if len(set(selected)) != len(selected):
        raise ValueError('each selected variant must be unique')
    if out.exists():
        raise ValueError('output directory already exists: ' + str(out))
    out.mkdir(parents=True)
    root = Path(__file__).resolve().parent/'original'
    state = args.probe == 'processor'
    target = 'AnamorphStateTests' if state else 'AnamorphTests'
    main_source = 'state_tests.cpp' if state else 'dsp_tests.cpp'
    names = {'probe': 'probe.cpp', 'gate': 'gate-probe.cpp', 'overlap': 'overlap-matrix.cpp',
             'processor': 'processor_probe.cpp', 'os': 'os-probe.cpp',
             'heal': 'heal-probe.cpp', 'drive': 'drive-probe.cpp'}
    archive_source = root/names[args.probe]
    verify_archive_file(root, archive_source, archive_source.name)
    for name in selected:
        if name != 'baseline':
            verify_variant(checkout, root, name)
    build = fresh_build(checkout, supplied, out/'fresh-build', target)
    juce = verify_inputs(checkout, build)
    cache = cache_values(build)
    compiler = cache.get('CMAKE_CXX_COMPILER')
    if not compiler:
        raise ValueError('fresh cache does not identify the C++ compiler')
    commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', target], text=True).splitlines()
    cc = next(shlex.split(x) for x in commands if x.endswith('/tests/'+main_source) and ' -c ' in x)
    link = next(shlex.split(x) for x in commands if ' -o '+target+'_artefacts/Release/'+target+' ' in x)
    if link[:2] == [':', '&&'] and link[-2:] == ['&&', ':']:
        link = link[2:-2]
    record = {'source_head': BASE, 'source_tree': git(checkout, 'rev-parse', 'HEAD^{tree}').decode().strip(),
              'juce_head': JUCE, 'juce_tree': git(juce, 'rev-parse', 'HEAD^{tree}').decode().strip(),
              'archive_commit': ARCHIVE, 'checkout': str(checkout), 'juce': str(juce),
              'supplied_build': str(supplied), 'fresh_build': str(build), 'target': target,
              'compiler': compiler, 'compiler_version': version(compiler),
              'cmake_version': version('cmake'), 'ninja_version': version('ninja'),
              'environment': {key: os.environ[key] for key in
                  ('PATH', 'CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS', 'CPATH',
                   'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH', 'LANG', 'LC_ALL') if key in os.environ},
              'cache_sha256': sha(build/'CMakeCache.txt'), 'rules_sha256': sha(build/'build.ninja'),
              'compile_commands_sha256': sha(build/'compile_commands.json'),
              'ninja_commands': commands, 'archive_probe_sha256': sha(archive_source),
              'tool_sha256': tools,
              'variants': {}}
    # Compile from this newly created output directory, not the archive's
    # directory, so an unverified neighboring header cannot shadow checkout
    # headers. The one archived absolute test path is translated as before.
    source = out/archive_source.name
    source.write_text(archive_source.read_text().replace(
        '/workspace/anamorph-audit-113b9189/source/tests/', str(checkout/'tests')+'/'))
    obj = out/'probe.o'
    old_obj = cc[cc.index('-o')+1]

    def compile_as(template, src, obj_path):
        command = template.copy()
        for flag, value in [('-o', str(obj_path)), ('-c', str(src)),
                            ('-MF', str(obj_path)+'.d'), ('-MT', str(obj_path))]:
            if flag in command:
                command[command.index(flag)+1] = value
        subprocess.run(command, cwd=build, check=True)
        return command

    probe_compile = compile_as(cc, source, obj)
    for name in selected:
        link_cmd = [str(obj) if arg == old_obj else arg for arg in link]
        entry = {'probe_source_sha256': sha(source), 'probe_compile': probe_compile,
                 'probe_object_sha256': sha(obj)}
        if name != 'baseline':
            original_engine = root/('engine-'+name+'.cpp')
            engine = out/('engine-'+name+'.cpp')
            engine.write_bytes(original_engine.read_bytes())
            entry['engine_sha256'] = sha(engine)
            entry['patch_sha256'] = sha(root/(name+'.patch'))
            ec = next(shlex.split(x) for x in commands if x.endswith('/src/dsp/AnamorphEngine.cpp') and ' -c ' in x)
            engine_obj = out/('engine-'+name+'.o')
            entry['engine_compile'] = compile_as(ec, engine, engine_obj)
            link_cmd = [str(engine_obj) if arg == ec[ec.index('-o')+1] else arg for arg in link_cmd]
        binary = out/(args.probe+'-'+name)
        link_cmd[link_cmd.index('-o')+1] = str(binary)
        entry['link'] = link_cmd
        entry['link_objects_sha256'] = link_objects(link_cmd, build)
        subprocess.run(link_cmd, cwd=build, check=True)
        entry['binary_sha256'] = sha(binary)
        if args.run:
            invocation = [str(binary), *args.run_arg]
            result = subprocess.run(invocation, cwd=out, capture_output=True)
            stdout, stderr = out/(name+'.stdout'), out/(name+'.stderr')
            stdout.write_bytes(result.stdout); stderr.write_bytes(result.stderr)
            entry['run'] = {'argv': invocation, 'cwd': str(out), 'exit': result.returncode,
                            'stdout_sha256': sha(stdout), 'stderr_sha256': sha(stderr)}
            if sha(binary) != entry['binary_sha256']:
                raise ValueError('binary changed during execution')
        verify_archive_file(root, archive_source, archive_source.name)
        if name != 'baseline':
            verify_variant(checkout, root, name)
            if sha(engine) != entry['engine_sha256']:
                raise ValueError('diagnostic engine changed during build or execution')
        verify_inputs(checkout, build)
        if any(sha(path) != digest for path, digest in tools.items()):
            raise ValueError('replay tool changed during execution')
        record['variants'][name] = entry
        print(str(binary)); print('sha256 '+entry['binary_sha256'])
    (out/'identity.json').write_text(json.dumps(record, indent=2)+'\n')
    if any(entry.get('run', {}).get('exit', 0) != 0 for entry in record['variants'].values()):
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
