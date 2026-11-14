"""Fail-closed input checks for the historical baseline replay.

These checks assume a trusted local Git/CMake/Ninja/compiler environment, no
concurrent writers, and unmodified toolchain/system dependencies. They do not
attest to historical builds or to the compiler, operating system or libraries.
"""
from pathlib import Path
import difflib
import hashlib
import os
import re
import stat
import subprocess
import tempfile

BASE = '113b9189f0ff4d3b6348c553c559c22ee8ade782'
JUCE = 'd60b7a78030661574a615b586d9325ba7a7fa7dd'
ARCHIVE = '6b55dfadcd89c23b7739c1fbb694adef707bed92'


def git_environment():
    # Do not let caller-provided Git routing/index/config environment select a
    # different repository or index. Git and the repository-local configuration
    # are still trusted; working-tree bytes are checked independently below.
    env = {key: value for key, value in os.environ.items() if not key.startswith('GIT_')}
    env['GIT_CONFIG_NOSYSTEM'] = '1'
    env['GIT_CONFIG_GLOBAL'] = os.devnull
    env['GIT_ATTR_NOSYSTEM'] = '1'
    return env


def git(path, *args):
    return subprocess.check_output(['git', '-C', str(path), *args], env=git_environment())


def clean_checkout(path, expected):
    return verify_source_tree(path, expected)


def verify_source_tree(path, expected_sha, patch=None, patch_sha256=None):
    """Match all tracked contents to HEAD plus an explicit diagnostic patch.

    The patch identity must come from the experiment specification. Ignored
    inputs anywhere in the checkout, all untracked files, staged changes,
    submodules and source symlinks are refused. Builds and outputs belong
    outside the checkout. This supports Git SHA-1 trees whose raw checkout bytes
    equal their blobs; it deliberately rejects transformed CRLF/filter outputs.
    This is not a compiler or system-dependency attestation.
    """
    path = Path(path).resolve()
    if not re.fullmatch(r'[0-9a-f]{40}', expected_sha):
        raise ValueError('expected source identity must be a full lowercase commit SHA')
    if Path(git(path, 'rev-parse', '--show-toplevel').decode().strip()).resolve() != path:
        raise ValueError(f'wrong Git root: {path}')
    if git(path, 'rev-parse', 'HEAD').decode().strip() != expected_sha:
        raise ValueError(f'wrong source HEAD: {path}')
    if git(path, 'rev-parse', '--show-object-format').strip() != b'sha1':
        raise ValueError('only Git SHA-1 object format is supported')
    expected_tree = git(path, 'rev-parse', expected_sha + '^{tree}').strip()
    try:
        index_tree = git(path, 'write-tree').strip()
    except subprocess.CalledProcessError as error:
        raise ValueError('unmerged or invalid source index') from error
    if index_tree != expected_tree:
        raise ValueError(f'staged changes in {path}; diagnostic patches must be unstaged')
    if git(path, 'ls-files', '--others', '--exclude-standard', '-z'):
        raise ValueError(f'untracked files in {path}')
    if git(path, 'ls-files', '--others', '--ignored', '--exclude-standard', '-z'):
        raise ValueError(f'ignored input in {path}; keep builds and outputs outside the checkout')
    if (patch is None) != (patch_sha256 is None):
        raise ValueError('patch and its independently specified SHA-256 must be supplied together')
    patch_bytes = None
    if patch is not None:
        patch_bytes = Path(patch).read_bytes()
        if not re.fullmatch(r'[0-9a-f]{64}', patch_sha256) or hashlib.sha256(patch_bytes).hexdigest() != patch_sha256:
            raise ValueError('diagnostic patch SHA-256 mismatch')
    with tempfile.TemporaryDirectory(prefix='anamorph-source-index-') as directory:
        env = git_environment(); env['GIT_INDEX_FILE'] = str(Path(directory)/'index')
        def indexed(*args, input_bytes=None):
            return subprocess.check_output(['git', '-C', str(path), *args], input=input_bytes, env=env).strip()
        indexed('read-tree', expected_sha)
        if patch_bytes is not None:
            indexed('apply', '--cached', '--binary', '--unidiff-zero', '--whitespace=nowarn', '-', input_bytes=patch_bytes)
        intended = indexed('write-tree').decode()
        entries = indexed('ls-files', '--stage', '-z').split(b'\0')
        for entry in entries:
            if not entry:
                continue
            header, name = entry.split(b'\t', 1)
            mode, oid, stage = header.split(b' ')
            if stage != b'0' or mode not in (b'100644', b'100755'):
                raise ValueError('unsupported unmerged, submodule or symlink source entry')
            target = path / os.fsdecode(name)
            try:
                # A symlinked parent can otherwise redirect tracked source to
                # unverified material despite the final entry appearing regular.
                parent = target.parent
                while parent != path:
                    if not stat.S_ISDIR(parent.lstat().st_mode):
                        raise ValueError(f'source parent is not a real directory: {parent}')
                    parent = parent.parent
                info = target.lstat()
                if not stat.S_ISREG(info.st_mode) or bool(info.st_mode & 0o111) != (mode == b'100755'):
                    raise ValueError(f'source type or executable mode mismatch: {target}')
                data = target.read_bytes()
            except OSError as error:
                raise ValueError(f'cannot read expected source: {target}') from error
            blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest().encode()
            if blob != oid:
                raise ValueError(f'checkout contents differ from selected tree: {target}')
    return intended


def cache_values(build):
    values = {}
    for line in (Path(build) / 'CMakeCache.txt').read_text().splitlines():
        if line and not line.startswith(('#', '//')) and '=' in line and ':' in line.split('=', 1)[0]:
            key, value = line.split('=', 1)
            values[key.split(':', 1)[0]] = value
    return values


def verify_inputs(checkout, build):
    checkout, build = Path(checkout).resolve(), Path(build).resolve()
    verify_source_tree(checkout, BASE)
    cache = cache_values(build)
    if Path(cache.get('CMAKE_HOME_DIRECTORY', '/nonexistent')).resolve() != checkout:
        raise ValueError('build was not configured from this checkout')
    juce = cache.get('ANAMORPH_JUCE_PATH') or cache.get('FETCHCONTENT_SOURCE_DIR_JUCE')
    juce = Path(juce) if juce else build / '_deps' / 'juce-src'
    if not juce.is_absolute():
        juce = checkout / juce
    verify_source_tree(juce, JUCE)
    return juce.resolve()


def verify_archive_file(repository, path, relative):
    expected = git(repository, 'show', f'{ARCHIVE}:worklogs/dsp-numerical-audit-0102/original/{relative}')
    if Path(path).read_bytes() != expected:
        raise ValueError(f'archived input differs from {ARCHIVE}: {relative}')


def verify_variant(checkout, archive, name):
    """Bind the archived engine copy to its separately recorded patch."""
    archive = Path(archive)
    source = archive / f'engine-{name}.cpp'
    patch = archive / f'{name}.patch'
    verify_archive_file(archive, source, source.name)
    verify_archive_file(archive, patch, patch.name)
    lines = patch.read_text().splitlines(keepends=True)
    if len(lines) < 3 or not lines[0].startswith('--- ') or not lines[1].startswith('+++ '):
        raise ValueError(f'invalid archived diagnostic patch: {patch}')
    baseline = (Path(checkout) / 'src/dsp/AnamorphEngine.cpp').read_text().splitlines(keepends=True)
    variant = source.read_text().splitlines(keepends=True)
    difference = ''.join(difflib.unified_diff(baseline, variant,
        fromfile=lines[0][4:].rstrip('\n'), tofile=lines[1][4:].rstrip('\n')))
    if difference != ''.join(lines):
        raise ValueError(f'diagnostic source does not match recorded patch: {name}')


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
                    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                    f'-DANAMORPH_JUCE_PATH={juce}'], check=True)
    verify_inputs(checkout, fresh)
    subprocess.run(['cmake', '--build', str(fresh), '--target', target, '--parallel', '2'], check=True)
    verify_inputs(checkout, fresh)
    return fresh


def fresh_checkout_build(checkout, expected_sha, juce, fresh, targets, patch=None, patch_sha256=None):
    """Configure a new build from a fully specified clean or diagnostic tree."""
    checkout, juce, fresh = Path(checkout).resolve(), Path(juce).resolve(), Path(fresh).resolve()
    tree = verify_source_tree(checkout, expected_sha, patch, patch_sha256)
    juce_tree = verify_source_tree(juce, JUCE)
    if fresh.exists():
        raise ValueError(f'fresh build directory already exists: {fresh}')
    if fresh.is_relative_to(checkout) or fresh.is_relative_to(juce):
        raise ValueError('fresh build must be outside both source trees')
    subprocess.run(['cmake', '-S', str(checkout), '-B', str(fresh), '-G', 'Ninja',
                    '-DCMAKE_BUILD_TYPE=Release', '-DANAMORPH_BUILD_STANDALONE=OFF',
                    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                    f'-DANAMORPH_JUCE_PATH={juce}'], check=True)
    cache = cache_values(fresh)
    if (Path(cache.get('CMAKE_HOME_DIRECTORY', '/nonexistent')).resolve() != checkout
            or Path(cache.get('ANAMORPH_JUCE_PATH', '/nonexistent')).resolve() != juce):
        raise ValueError('fresh configure does not identify the requested source and dependency')
    if (verify_source_tree(checkout, expected_sha, patch, patch_sha256) != tree
            or verify_source_tree(juce, JUCE) != juce_tree):
        raise ValueError('source or dependency changed during configuration')
    subprocess.run(['cmake', '--build', str(fresh), '--target', *targets, '--parallel', '2'], check=True)
    if (verify_source_tree(checkout, expected_sha, patch, patch_sha256) != tree
            or verify_source_tree(juce, JUCE) != juce_tree):
        raise ValueError('source or dependency changed during build')
    return {'source_head': expected_sha, 'source_tree': tree, 'patch_sha256': patch_sha256,
            'juce_head': JUCE, 'juce_tree': juce_tree, 'build_directory': str(fresh),
            'targets': list(targets)}
