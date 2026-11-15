"""Bound supported compiler search inputs for the Linux/Ninja replay tools.

Source and JUCE trees are verified separately. Toolchain implicit headers and
explicit /usr/include or /usr/local/include directories remain trusted system
dependencies; this is not an attestation of the compiler or host.
"""
import hashlib
import os
from pathlib import Path
import shlex
import stat


def directory_manifest(root):
    root = Path(root)
    if not root.exists() and not root.is_symlink():
        return {'<root>': {'kind': 'absent'}}
    if not stat.S_ISDIR(root.lstat().st_mode):
        raise ValueError('include root is not a real directory: ' + str(root))
    result = {'<root>': {'kind': 'directory'}}
    def walk(path):
        for entry in sorted(os.scandir(path), key=lambda item: item.name):
            name = str(Path(entry.path).relative_to(root))
            info = entry.stat(follow_symlinks=False)
            if stat.S_ISDIR(info.st_mode):
                result[name] = {'kind': 'directory'}
                walk(Path(entry.path))
            elif stat.S_ISREG(info.st_mode):
                result[name] = {'kind': 'file', 'mode': stat.S_IMODE(info.st_mode),
                                'sha256': hashlib.sha256(Path(entry.path).read_bytes()).hexdigest()}
            else:
                raise ValueError('unsupported generated include entry: ' + entry.path)
    walk(root)
    return result


def generated_include_manifest(commands, build, checkout, juce):
    """Snapshot every build-owned explicit include tree in supported commands."""
    build, checkout, juce = (Path(p).resolve() for p in (build, checkout, juce))
    system = (Path('/usr/include'), Path('/usr/local/include'))
    result = {}
    for command in commands:
        i = 0
        while i < len(command):
            arg = command[i]
            if arg.startswith(('@', '-Wp,', '-Xpreprocessor', '-include', '-imacros', '--sysroot', '-isysroot')):
                raise ValueError('unsupported compiler input option: ' + arg)
            value = None
            for flag in ('-isystem', '-iquote', '-idirafter', '-I'):
                if arg == flag:
                    i += 1
                    if i >= len(command): raise ValueError('missing include directory')
                    value = command[i]
                    break
                if arg.startswith(flag) and len(arg) > len(flag):
                    value = arg[len(flag):]
                    break
            if value is not None:
                if not value or value.startswith('='):
                    raise ValueError('unsupported include directory: ' + value)
                raw = Path(os.path.abspath(build/value)) if not Path(value).is_absolute() else Path(os.path.abspath(value))
                path = raw.resolve()
                if raw.is_relative_to(build):
                    if path != raw:
                        raise ValueError('symlinked build include path: ' + str(raw))
                    result[str(raw)] = directory_manifest(raw)
                elif path.is_relative_to(checkout) or path.is_relative_to(juce):
                    pass  # the full raw source trees are verified separately
                elif any(path.is_relative_to(root) for root in system):
                    pass  # explicitly trusted system development headers
                else:
                    raise ValueError('unsupported external include directory: ' + str(raw))
            i += 1
    return result


def verify_generated_inputs(expected, commands, build, checkout, juce):
    if expected is None:
        raise ValueError('receipt lacks generated compiler-input identity; rebuild required')
    if generated_include_manifest(commands, build, checkout, juce) != expected:
        raise ValueError('generated compiler-input directory changed')


def relevant_compile_commands(commands, target):
    main = 'state_tests.cpp' if target == 'AnamorphStateTests' else 'dsp_tests.cpp'
    wanted = ('/tests/' + main, '/src/dsp/AnamorphEngine.cpp')
    selected = [shlex.split(line) for line in commands
                if ' -c ' in line and any(line.endswith(path) for path in wanted)]
    if len(selected) != 2:
        raise ValueError('unsupported target compile commands')
    return selected


def verify_compile_dependencies(depfile, build, checkout, juce, generated, local_inputs, verified_roots=()):
    """Reject dependencies outside checked source, packet, generated or system roots."""
    text = Path(depfile).read_text().replace('\\\n', ' ')
    if ':' not in text:
        raise ValueError('compiler dependency file is missing its target')
    dependencies = shlex.split(text.split(':', 1)[1])
    if not dependencies:
        raise ValueError('compiler dependency file is empty')
    roots = [Path(checkout).resolve(), Path(juce).resolve(), *(Path(p).resolve() for p in verified_roots),
             *(Path(p).resolve() for p in generated), Path('/usr/include'),
             Path('/usr/local/include'), Path('/usr/lib')]
    allowed_local = {str(Path(p).resolve()): digest for p, digest in local_inputs.items()}
    seen = set()
    for name in dependencies:
        path = Path(name)
        path = (Path(build)/path).resolve() if not path.is_absolute() else path.resolve()
        if not path.is_file():
            raise ValueError('compiler dependency is unavailable: ' + str(path))
        seen.add(str(path))
        if str(path) in allowed_local:
            if hashlib.sha256(path.read_bytes()).hexdigest() != allowed_local[str(path)]:
                raise ValueError('compiler packet dependency changed: ' + str(path))
        elif not any(path.is_relative_to(root) for root in roots):
            raise ValueError('compiler dependency is outside verified inputs: ' + str(path))
    if not set(allowed_local).intersection(seen):
        raise ValueError('the selected compilation input is absent from dependencies')
    return dependencies


def verify_relocated_source(path):
    data = Path(path).read_bytes()
    if any(token in data for token in (b'__FILE__', b'__BASE_FILE__', b'__builtin_FILE', b'#include_next')):
        raise ValueError('relocated source can depend on its compilation location')


def prepare_artifacts(out, run):
    """Required side outputs must be newly created by this execution."""
    root = Path(out).resolve()
    for item in run.get('required_artifacts', []):
        name = Path(item['path'])
        if name.is_absolute() or '..' in name.parts or not name.parts:
            raise ValueError('invalid result artifact path')
        path = root/name
        if path.exists() or path.is_symlink():
            raise ValueError('required result artifact already exists: ' + str(name))


def validate_artifacts(out, run, stdout):
    """Hash explicitly required side artifacts; exit-only runs are labelled."""
    root = Path(out).resolve()
    markers = run.get('stdout_contains', [])
    for marker in markers:
        if not isinstance(marker, str) or marker.encode() not in stdout:
            raise ValueError('required stdout marker missing')
    artifacts = {}
    for item in run.get('required_artifacts', []):
        name = item['path']
        path = Path(name)
        if path.is_absolute() or '..' in path.parts or not path.parts:
            raise ValueError('invalid result artifact path')
        candidate = root/path
        parent = candidate.parent
        while parent != root:
            if not stat.S_ISDIR(parent.lstat().st_mode):
                raise ValueError('artifact parent is not a real directory')
            parent = parent.parent
        info = candidate.lstat()
        if not stat.S_ISREG(info.st_mode) or (item.get('nonempty', True) and not info.st_size):
            raise ValueError('required result artifact is not a nonempty regular file: ' + name)
        digest = hashlib.sha256(candidate.read_bytes()).hexdigest()
        if 'sha256' in item and item['sha256'] != digest:
            raise ValueError('required result artifact digest mismatch: ' + name)
        artifacts[name] = {'sha256': digest, 'size': info.st_size}
    return {'scope': 'declared-artifacts-and-markers' if artifacts or markers else 'exit-only',
            'artifacts': artifacts, 'stdout_contains': markers}


def verify_retained_artifacts(out, runs):
    for run in runs:
        for name, record in run['result_validation']['artifacts'].items():
            validate_artifacts(out, {'required_artifacts': [{'path': name, 'sha256': record['sha256']}]}, b'')
