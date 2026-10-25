#!/usr/bin/env python3
"""Bounded native characterization; this is not an installer or shipping gate."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import signal
import subprocess
import sys
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--report-dir', type=Path, required=True)
    parser.add_argument('--ordinary-runs', type=int, choices=range(4), default=3)
    args = parser.parse_args()
    source = args.source.resolve()
    report_dir = args.report_dir.resolve()
    report_dir.mkdir(parents=True, exist_ok=False)
    record = {'source_head': '14bde706e2d121c9970cf00c757cb12d3178b9a5',
              'consumer_head': os.environ.get('GITHUB_SHA'),
              'run_id': os.environ.get('GITHUB_RUN_ID'),
              'kind': 'controlled reconstruction; original executable not recovered',
              'original_build_run': 40221042187, 'original_build_job': 121252472086,
              'original_build_number': 1878, 'ordinary_invocations': [],
              'execution_budget_seconds': 600, 'cause_established': False,
              'original_failure_erased': False, 'shipping_gate': False,
              'host_security_settings_changed': False}
    failed = False
    deadline = None

    def save():
        (report_dir / 'characterization.json').write_text(json.dumps(record, indent=2) + '\n')

    def command(label, argv, timeout=60, cwd=None, within_budget=False):
        nonlocal deadline
        if within_budget:
            timeout = min(timeout, max(0, deadline - time.monotonic()))
            if timeout <= 0:
                raise RuntimeError('combined ten-minute execution budget exhausted')
        started = time.monotonic()
        with (report_dir / (label + '.log')).open('wb') as output:
            process = subprocess.Popen(argv, cwd=cwd, stdout=output, stderr=subprocess.STDOUT,
                                       start_new_session=True)
            timed_out = False
            try:
                code = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                timed_out = True
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    code = process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    code = process.wait()
        return {'argv': argv, 'exit_code': code, 'timed_out': timed_out,
                'elapsed_seconds': round(time.monotonic() - started, 3),
                'log': label + '.log', 'log_sha256': sha(report_dir / (label + '.log'))}

    try:
        if sys.platform != 'darwin' or platform.machine() != 'arm64':
            raise RuntimeError('requires a real disposable Apple Silicon Mac')
        if os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted' or os.environ.get('GITHUB_REPOSITORY') != 'skyRolly/Anamorph':
            raise RuntimeError('requires this repository\'s genuine disposable hosted runner')
        actual = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
        if actual != record['source_head']:
            raise RuntimeError('the retained source checkout does not match the original failure')
        record['host'] = {'machine': platform.machine(), 'system': platform.platform(),
                          'runner_image': os.environ.get('ImageOS'),
                          'runner_image_version': os.environ.get('ImageVersion')}
        record['environment'] = {'ROSETTA_ADVERTISE_AVX': os.environ.get('ROSETTA_ADVERTISE_AVX'),
                                 'ANAMORPH_TESTS_NO_FTZ': os.environ.get('ANAMORPH_TESTS_NO_FTZ')}
        if record['environment']['ANAMORPH_TESTS_NO_FTZ'] is not None:
            raise RuntimeError('normal native DSP validation must not relax the denormal guard')
        for label, argv in [('os', ['sw_vers']), ('compiler', ['xcrun', 'clang++', '--version']),
                            ('sdk', ['xcrun', '--show-sdk-path']), ('xcode', ['xcodebuild', '-version']),
                            ('rosetta', ['/usr/bin/arch', '-x86_64', '/usr/bin/true'])]:
            result = command(label, argv)
            record[label] = result
            if result['exit_code']:
                raise RuntimeError(label + ' prerequisite failed; no capability is fabricated')
        candidates = list((source / 'build').glob('AnamorphTests_artefacts/Release/AnamorphTests'))
        if len(candidates) != 1:
            raise RuntimeError('expected one exact Release DSP executable')
        binary = candidates[0]
        record['ordinary_binary'] = {'path': str(binary), 'sha256': sha(binary),
                                     'original_executable': False}
        record['source_sha256'] = {name: sha(source / name) for name in
                                  ['CMakeLists.txt', 'tests/dsp_tests.cpp', 'src/dsp/ScopeBuffer.h', 'scripts/run-tests.sh']}
        juce = source / 'build/_deps/juce-src'
        record['juce_head'] = subprocess.check_output(['git', '-C', str(juce), 'rev-parse', 'HEAD'], text=True).strip()
        if record['juce_head'] != '3fb5cbc0340758596dac39f7c8de47d562b4896e':
            raise RuntimeError('JUCE checkout does not match the product pin')
        record['juce_comparison_sha256'] = sha(juce / 'modules/juce_core/maths/juce_MathsFunctions.h')
        record['binary_uuid'] = command('binary-uuid', ['xcrun', 'dwarfdump', '--uuid', str(binary)])
        record['binary_architectures'] = command('binary-architectures', ['lipo', '-archs', str(binary)])
        record['symbols'] = command('binary-symbols', ['xcrun', 'dsymutil', str(binary), '-o', str(report_dir / 'AnamorphTests.dSYM')], timeout=120)
        record['symbol_uuid'] = command('symbol-uuid', ['xcrun', 'dwarfdump', '--uuid', str(report_dir / 'AnamorphTests.dSYM')])
        # Setup is complete. Every ordinary invocation is retained, even on failure;
        # the fixed count is not conditional on a pass, and never becomes a retry loop.
        deadline = time.monotonic() + 600
        for index in range(args.ordinary_runs):
            result = command('ordinary-' + str(index + 1), ['/usr/bin/arch', '-x86_64', str(binary)],
                             timeout=600, cwd=source, within_budget=True)
            result['binary_sha256'] = record['ordinary_binary']['sha256']
            record['ordinary_invocations'].append(result)
            failed |= result['exit_code'] != 0
            save()
            if result['timed_out']:
                raise RuntimeError('ordinary DSP execution exhausted its bounded budget')
        # Add a diagnostic target only in the private reconstruction checkout.
        # The original executable and all of its ordinary outputs remain intact.
        diagnostic = Path(__file__).resolve().parents[1] / 'tests/scope_ring_diagnostic.cpp'
        shutil.copyfile(diagnostic, source / 'tests/scope_ring_diagnostic.cpp')
        addition = '''
juce_add_console_app(AnamorphScopeDiagnostic PRODUCT_NAME "AnamorphScopeDiagnostic")
target_sources(AnamorphScopeDiagnostic PRIVATE tests/scope_ring_diagnostic.cpp)
target_include_directories(AnamorphScopeDiagnostic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
get_target_property(scope_definitions AnamorphTests COMPILE_DEFINITIONS)
target_compile_definitions(AnamorphScopeDiagnostic PUBLIC ${scope_definitions})
target_link_libraries(AnamorphScopeDiagnostic PRIVATE juce::juce_dsp juce::juce_audio_basics
    AnamorphHardening juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
'''
        with (source / 'CMakeLists.txt').open('a') as file:
            file.write(addition)
        record['diagnostic_configure'] = command('diagnostic-configure', ['cmake', '-S', str(source), '-B', str(source / 'build')], timeout=180, within_budget=True)
        if record['diagnostic_configure']['exit_code']:
            raise RuntimeError('focused diagnostic configure failed')
        record['diagnostic_build'] = command('diagnostic-build', ['cmake', '--build', str(source / 'build'), '--target', 'AnamorphScopeDiagnostic', '--verbose'], timeout=300, within_budget=True)
        if record['diagnostic_build']['exit_code']:
            raise RuntimeError('focused diagnostic build failed')
        focus = source / 'build/AnamorphScopeDiagnostic_artefacts/Release/AnamorphScopeDiagnostic'
        record['diagnostic_binary'] = {'sha256': sha(focus), 'source_sha256': sha(diagnostic),
                                       'kind': 'separately instrumented single-threaded Test 61 variant'}
        record['diagnostic_uuid'] = command('diagnostic-uuid', ['xcrun', 'dwarfdump', '--uuid', str(focus)])
        commands = report_dir / 'lldb.commands'
        commands.write_text('''settings set target.disable-aslr false
breakpoint set -n scope_mismatch
breakpoint command add 1 -o "thread backtrace all" -o "register read" -o "frame variable" -o "disassemble --frame" -o "image list -o -f" -o "continue"
run
''')
        record['focused_debugger'] = command('focused-debugger', ['xcrun', 'lldb', '--arch', 'x86_64', '--batch', '-s', str(commands), '--', str(focus)],
                                             timeout=120, cwd=source, within_budget=True)
        text = (report_dir / 'focused-debugger.log').read_text(errors='replace')
        record['focused_mismatch_observed'] = 'SCOPE_MISMATCH_BITS' in text
        record['focused_fixture_completed'] = 'SCOPE_DIAGNOSTIC_COMPLETE' in text
        failed |= record['focused_debugger']['exit_code'] != 0 or not record['focused_fixture_completed'] or record['focused_mismatch_observed']
        if sha(binary) != record['ordinary_binary']['sha256']:
            raise RuntimeError('ordinary executable unexpectedly changed during focused setup')
        shutil.copyfile(source / 'build/compile_commands.json', report_dir / 'compile_commands.json')
        record['interpretation'] = 'Preserve all outcomes. No mismatch means bounded nonreproduction, not a repair or retrospective main pass. Instrumented bytes are a separate diagnostic input.'
    except Exception as error:
        record['error'] = str(error)
        failed = True
    finally:
        record['execution_elapsed_seconds_after_setup'] = None if deadline is None else round(time.monotonic() - (deadline - 600), 3)
        record['status'] = 'incomplete_or_failure' if failed else 'bounded_characterization_complete'
        save()
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
