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

from isolation import IsolationError, Profile, install_signal_handlers, run_child

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
PRODUCT = 'f1d3ff81e0f239d0bec7c8873f54ebfce78e15c7'
JUCE = 'd60b7a78030661574a615b586d9325ba7a7fa7dd'


def output(cmd, cwd=ROOT):
    return subprocess.check_output(cmd, cwd=cwd, text=True).strip()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    install_signal_handlers()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True, help='built Ninja Release tree')
    parser.add_argument('--juce', type=Path, required=True, help='verified JUCE Git checkout')
    parser.add_argument('--out', type=Path, required=True, help='new output directory (never overwritten)')
    args = parser.parse_args()
    build, juce, out = args.build.resolve(), args.juce.resolve(), args.out.resolve()
    if out.exists():
        raise SystemExit('output directory must not already exist')
    out.mkdir(parents=True, mode=0o700)
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
    manifest = {'schema': 2, 'product_commit': PRODUCT, 'checkout_head': output(['git', 'rev-parse', 'HEAD']),
                'juce_commit': JUCE, 'build': str(build), 'juce': str(juce),
                'compiler': output([state[0][0], '--version']).splitlines()[0],
                'cmake': output(['cmake', '--version']).splitlines()[0],
                'instrumentation': 'five juce_gui_basics TUs, PluginEditor.cpp and probe; all other linked objects are uninstrumented',
                'environment': {k: os.environ.get(k, '') for k in ('PATH', 'LD_LIBRARY_PATH', 'PKG_CONFIG_PATH', 'PKG_CONFIG_SYSROOT_DIR', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH')},
                'commands': [], 'results': []}
    def save():
        pending = out / 'manifest.json.tmp'
        pending.write_text(json.dumps(manifest, indent=2) + '\n')
        pending.replace(out / 'manifest.json')
    def run(tag, command, env=None, profile=None):
        item = {'name': tag, 'argv': [str(x) for x in command], 'cwd': str(build)}
        if profile is not None:
            if out.is_relative_to(profile.root):
                raise IsolationError('evidence output must be outside the disposable profile')
            item['environment'] = {key: env[key] for key in ('HOME', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME',
                                   'XDG_CACHE_HOME', 'XDG_STATE_HOME', 'XDG_RUNTIME_DIR', 'TMPDIR')}
            if 'ASAN_OPTIONS' in env:
                item['environment']['ASAN_OPTIONS'] = env['ASAN_OPTIONS']
            item['resolved_paths'] = profile.validate(path_probe, env)
        manifest['commands'].append(item)
        save()
        stdout, stderr = out / (tag + '.stdout'), out / (tag + '.stderr')
        try:
            with stdout.open('wb') as so, stderr.open('wb') as se:
                exit_code = run_child(command, cwd=build, env=env, stdout=so, stderr=se)
        except BaseException:
            item['interrupted_or_failed_to_start'] = True
            save()
            raise
        item.update(exit=exit_code, stdout=stdout.name, stdout_sha256=sha(stdout),
                    stderr=stderr.name, stderr_sha256=sha(stderr))
        save()
        return exit_code

    # Link the path witness to the same JUCE core and production PresetManager as
    # the suite. Its only operation is printing resolved paths; it writes no presets.
    path_obj = out / 'path-probe.o'
    path_src = HERE / 'path-probe.cpp'
    path_cmd = [str(path_src) if x == str(ROOT / 'tests/state_tests.cpp') else x for x in state[0]]
    if run('compile-path-probe', native.with_output(path_cmd, path_obj)):
        raise SystemExit(f'path witness compilation failed; see {out}')
    path_probe = out / 'path-probe'
    path_link = [str(path_obj) if x == native.output_path(state[0]) else x for x in links[0]]
    if run('link-path-probe', native.with_output(path_link, path_probe)):
        raise SystemExit(f'path witness link failed; see {out}')
    if run('isolation-controls', [sys.executable, str(HERE / 'test_isolation.py'), '--probe', str(path_probe)]):
        raise SystemExit(f'isolation controls failed; no State/diagnostic case was started; see {out}')

    state_binary = build / native.output_path(links[0])
    if not state_binary.is_file():
        raise SystemExit('built State test executable is missing')
    with Profile() as profile:
        code = run('state-suite', ['xvfb-run', '-a', str(state_binary)], profile.env, profile)
    state_output = (out / 'state-suite.stdout').read_text(errors='replace')
    if (code != 0 or 'State test 8:' not in state_output or 'State test 86:' not in state_output
            or 'State test 88:' not in state_output or '13959 checks, 0 failure(s)' not in state_output):
        raise SystemExit(f'State suite failed or expected preset/wheel checks did not execute; see {out}')
    manifest['state_suite'] = {'exit': code, 'checks': 13959, 'failures': 0}
    save()
    replacements = {}
    for i, cmd in enumerate(sorted(gui + editor, key=lambda c: str(source(c)))):
        obj = out / f'overlay-{i}.o'
        replacements[native.output_path(cmd)] = str(obj)
        if run(f'compile-overlay-{i}', native.with_output(cmd, obj) + flags):
            raise SystemExit(f'overlay compilation failed; see {out}')
    for name, file in [('standalone', HERE / 'original-probe.cpp'), ('held', HERE / 'held-wheel-probe.cpp')]:
        obj = out / f'{name}.o'
        cmd = [str(file) if x == str(ROOT / 'tests/state_tests.cpp') else x for x in state[0]]
        if run('compile-' + name, native.with_output(cmd, obj) + flags):
            raise SystemExit(f'probe compilation failed; see {out}')
        repl = dict(replacements)
        repl[native.output_path(state[0])] = str(obj)
        link = [repl.get(x, x) for x in links[0]]
        binary = out / name
        if run('link-' + name, native.with_output(link, binary) + ['-fsanitize=address']):
            raise SystemExit(f'probe link failed; see {out}')
        for case in ('live', 'close'):
            argv = ['xvfb-run', '-a', str(binary)] + (['wheel', case] if name == 'standalone' else [case])
            with Profile() as profile:
                env = profile.env.copy()
                env['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1:symbolize=1'
                code = run(name + '-' + case, argv, env, profile)
            err = (out / (name + '-' + case + '.stderr')).read_text(errors='replace')
            stdout = (out / (name + '-' + case + '.stdout')).read_text(errors='replace')
            location = 'ScopedDragNotification::~ScopedDragNotification' if name == 'standalone' else 'Knob::takeWheelNotch'
            expected_outcome = (code == 0 and not err and all(s in stdout for s in ('width=0.53', 'begins=1', 'ends=1'))
                                if case == 'live' else
                                code == 1 and 'AddressSanitizer: heap-use-after-free' in err and location in err)
            manifest['results'].append({'case': name + '-' + case, 'exit': code,
                                        'expected_outcome_observed': expected_outcome,
                                        'asan_options': env['ASAN_OPTIONS'], 'binary_sha256': sha(binary),
                                        'source_sha256': sha(file)})
            save()
            print(f'{name}-{case}: exit={code}; expected outcome observed={expected_outcome}', flush=True)
    if not all(r['expected_outcome_observed'] for r in manifest['results']):
        raise SystemExit('one or more expected outcomes were not observed; inspect raw output')


if __name__ == '__main__':
    try:
        main()
    except (IsolationError, InterruptedError, OSError) as error:
        raise SystemExit(f'NR-026 reproduction stopped: {error}')
