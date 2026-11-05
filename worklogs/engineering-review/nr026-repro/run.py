#!/usr/bin/env python3
"""Rebuild focused ASan overlays of the real state-test target; not a CI gate."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
PRODUCT = 'f1d3ff81e0f239d0bec7c8873f54ebfce78e15c7'
JUCE = 'd60b7a78030661574a615b586d9325ba7a7fa7dd'


def output(cmd, cwd=ROOT):
    return subprocess.check_output(cmd, cwd=cwd, text=True).strip()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True, help='built Ninja Release tree')
    parser.add_argument('--juce', type=Path, required=True, help='verified JUCE Git checkout')
    parser.add_argument('--out', type=Path, required=True, help='new or empty output directory')
    args = parser.parse_args()
    build, juce, out = args.build.resolve(), args.juce.resolve(), args.out.resolve()
    if out.exists() and any(out.iterdir()):
        raise SystemExit('output directory must be empty')
    out.mkdir(parents=True, exist_ok=True)
    if output(['git', '-C', str(juce), 'rev-parse', 'HEAD']) != JUCE:
        raise SystemExit('JUCE checkout does not match the recorded commit')
    if output(['git', '-C', str(juce), 'status', '--porcelain']):
        raise SystemExit('JUCE checkout has local changes')
    if subprocess.run(['git', 'diff', '--quiet', PRODUCT, '--', 'src', 'tests', 'CMakeLists.txt', 'cmake'], cwd=ROOT).returncode:
        raise SystemExit('product build inputs differ from the recorded source')
    if not (build / 'build.ninja').exists():
        raise SystemExit('build must be a configured and built Ninja tree')
    cache = (build / 'CMakeCache.txt').read_text()
    if 'CMAKE_BUILD_TYPE:STRING=Release' not in cache:
        raise SystemExit('expected a Release build')
    if str(juce) not in cache:
        raise SystemExit('build cache does not name the supplied JUCE checkout')

    spec = importlib.util.spec_from_file_location('native', ROOT / 'scripts/native_validation.py')
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    lines = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'AnamorphStateTests'], text=True).splitlines()
    commands = [native.command_args(line, False) for line in lines if line.strip()]
    compiles = [c for c in commands if '-c' in c and 'AnamorphStateTests.dir' in native.output_path(c)]
    def source(c):
        return next((Path(x) for x in c if x.endswith('.cpp')), None)
    gui = [c for c in compiles if source(c) and source(c).parent == juce / 'modules/juce_gui_basics']
    editor = [c for c in compiles if source(c) == ROOT / 'src/PluginEditor.cpp']
    state = [c for c in compiles if source(c) == ROOT / 'tests/state_tests.cpp']
    links = [c for c in commands if '-c' not in c and '-o' in c
             and 'AnamorphStateTests_artefacts/' in native.output_path(c)]
    expected = {'juce_gui_basics.cpp'} | {f'juce_gui_basics_{i}.cpp' for i in range(2, 6)}
    if {source(c).name for c in gui} != expected or len(gui) != 5 or len(editor) != 1 or len(state) != 1 or len(links) != 1:
        raise SystemExit('unexpected target commands; refusing incomplete instrumentation')
    flags = ['-O1', '-g', '-fsanitize=address', '-fno-omit-frame-pointer']
    manifest = {'schema': 1, 'product_commit': PRODUCT, 'checkout_head': output(['git', 'rev-parse', 'HEAD']),
                'juce_commit': JUCE, 'build': str(build), 'juce': str(juce),
                'compiler': output([state[0][0], '--version']).splitlines()[0],
                'cmake': output(['cmake', '--version']).splitlines()[0],
                'instrumentation': 'five juce_gui_basics TUs, PluginEditor.cpp and probe; all other linked objects are uninstrumented',
                'environment': {k: os.environ.get(k, '') for k in ('PATH', 'LD_LIBRARY_PATH', 'PKG_CONFIG_PATH', 'PKG_CONFIG_SYSROOT_DIR', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH')},
                'commands': [], 'results': []}
    def save():
        (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    def run(tag, command, env=None):
        item = {'name': tag, 'argv': [str(x) for x in command], 'cwd': str(build)}
        manifest['commands'].append(item)
        save()
        result = subprocess.run(command, cwd=build, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = out / (tag + '.stdout'), out / (tag + '.stderr')
        stdout.write_bytes(result.stdout); stderr.write_bytes(result.stderr)
        item.update(exit=result.returncode, stdout=stdout.name, stdout_sha256=sha(stdout),
                    stderr=stderr.name, stderr_sha256=sha(stderr))
        save()
        return result
    replacements = {}
    for i, cmd in enumerate(sorted(gui + editor, key=lambda c: str(source(c)))):
        obj = out / f'overlay-{i}.o'
        replacements[native.output_path(cmd)] = str(obj)
        if run(f'compile-overlay-{i}', native.with_output(cmd, obj) + flags).returncode:
            raise SystemExit(f'overlay compilation failed; see {out}')
    for name, file in [('standalone', HERE / 'original-probe.cpp'), ('held', HERE / 'held-wheel-probe.cpp')]:
        obj = out / f'{name}.o'
        cmd = [str(file) if x == str(ROOT / 'tests/state_tests.cpp') else x for x in state[0]]
        if run('compile-' + name, native.with_output(cmd, obj) + flags).returncode:
            raise SystemExit(f'probe compilation failed; see {out}')
        repl = dict(replacements)
        repl[native.output_path(state[0])] = str(obj)
        link = [repl.get(x, x) for x in links[0]]
        binary = out / name
        if run('link-' + name, native.with_output(link, binary) + ['-fsanitize=address']).returncode:
            raise SystemExit(f'probe link failed; see {out}')
        for case in ('live', 'close'):
            env = os.environ.copy()
            env['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1:symbolize=1'
            env['XDG_CONFIG_HOME'] = str(out / ('profile-' + name + '-' + case))
            argv = ['xvfb-run', '-a', str(binary)] + (['wheel', case] if name == 'standalone' else [case])
            result = run(name + '-' + case, argv, env)
            err = result.stderr.decode(errors='replace')
            expected_outcome = result.returncode == 0 if case == 'live' else (result.returncode != 0 and 'AddressSanitizer: heap-use-after-free' in err)
            manifest['results'].append({'case': name + '-' + case, 'exit': result.returncode,
                                        'expected_outcome_observed': expected_outcome,
                                        'asan_options': env['ASAN_OPTIONS'], 'binary_sha256': sha(binary),
                                        'source_sha256': sha(file)})
            save()
            print(f'{name}-{case}: exit={result.returncode}; expected outcome observed={expected_outcome}', flush=True)
    if not all(r['expected_outcome_observed'] for r in manifest['results']):
        raise SystemExit('one or more expected outcomes were not observed; inspect raw output')


if __name__ == '__main__':
    main()
