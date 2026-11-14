"""Fail-closed input checks for the historical baseline replay.

These checks assume a trusted local Git/CMake/Ninja/compiler environment, no
concurrent writers, and unmodified toolchain/system dependencies. They do not
attest to historical builds or to the compiler, operating system or libraries.
"""
from pathlib import Path
import subprocess

BASE = '113b9189f0ff4d3b6348c553c559c22ee8ade782'
JUCE = 'd60b7a78030661574a615b586d9325ba7a7fa7dd'
ARCHIVE = '6b55dfadcd89c23b7739c1fbb694adef707bed92'


def git(path, *args):
    return subprocess.check_output(['git', '-C', str(path), *args])


def clean_checkout(path, expected):
    path = Path(path).resolve()
    top = Path(git(path, 'rev-parse', '--show-toplevel').decode().strip()).resolve()
    if top != path or git(path, 'rev-parse', 'HEAD').decode().strip() != expected:
        raise ValueError(f'wrong Git root or HEAD: {path}')
    if git(path, 'status', '--porcelain=v1', '--untracked-files=all'):
        raise ValueError(f'modified or untracked files in {path}')
    # An ignored header/source can shadow a tracked include without appearing in
    # status. Reject ignored files in source inputs too. Build trees outside these
    # directories are permitted, but the build is always reconstructed below.
    if git(path, 'ls-files', '--others', '--ignored', '--exclude-standard', '-z',
           '--', 'src', 'tests', 'cmake', 'modules'):
        raise ValueError(f'ignored source input in {path}')


def cache_values(build):
    values = {}
    for line in (Path(build) / 'CMakeCache.txt').read_text().splitlines():
        if line and not line.startswith(('#', '//')) and '=' in line and ':' in line.split('=', 1)[0]:
            key, value = line.split('=', 1)
            values[key.split(':', 1)[0]] = value
    return values


def verify_inputs(checkout, build):
    checkout, build = Path(checkout).resolve(), Path(build).resolve()
    clean_checkout(checkout, BASE)
    cache = cache_values(build)
    if Path(cache.get('CMAKE_HOME_DIRECTORY', '/nonexistent')).resolve() != checkout:
        raise ValueError('build was not configured from this checkout')
    juce = cache.get('ANAMORPH_JUCE_PATH') or cache.get('FETCHCONTENT_SOURCE_DIR_JUCE')
    juce = Path(juce) if juce else build / '_deps' / 'juce-src'
    if not juce.is_absolute():
        juce = checkout / juce
    clean_checkout(juce, JUCE)
    return juce.resolve()


def verify_archive_file(repository, path, relative):
    expected = git(repository, 'show', f'{ARCHIVE}:worklogs/dsp-numerical-audit-0102/original/{relative}')
    if Path(path).read_bytes() != expected:
        raise ValueError(f'archived input differs from {ARCHIVE}: {relative}')


def fresh_build(checkout, supplied_build, fresh, target):
    # The supplied build is checked for operator mistakes but never trusted as
    # an object or generated-build-rule source. A new directory gets new rules
    # and new objects from the checked checkout and pinned JUCE.
    checkout, fresh = Path(checkout).resolve(), Path(fresh).resolve()
    juce = verify_inputs(checkout, supplied_build)
    if fresh.exists():
        raise ValueError(f'fresh build directory already exists: {fresh}')
    if fresh.is_relative_to(checkout) or fresh.is_relative_to(juce):
        raise ValueError('fresh build must be outside both source trees')
    subprocess.run(['cmake', '-S', str(checkout), '-B', str(fresh), '-G', 'Ninja',
                    '-DCMAKE_BUILD_TYPE=Release', '-DANAMORPH_BUILD_STANDALONE=OFF',
                    f'-DANAMORPH_JUCE_PATH={juce}'], check=True)
    verify_inputs(checkout, fresh)
    subprocess.run(['cmake', '--build', str(fresh), '--target', target, '--parallel', '2'], check=True)
    verify_inputs(checkout, fresh)
    return fresh
