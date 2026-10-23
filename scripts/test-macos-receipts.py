#!/usr/bin/env python3
"""Real installed-receipt smoke test; mutations never authorize a host install."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import subprocess
import sys
import tempfile

PRODUCT = 'Anamorph'
BASE = 'tech.rolly.anamorph'
SIBLING_BASE = 'tech.rolly.anabasis'
CODE = 'Anmr'
COMPONENTS = {
    'vst3': ('/Library/Audio/Plug-Ins/VST3', 'Anamorph.vst3'),
    'au': ('/Library/Audio/Plug-Ins/Components', 'Anamorph.component'),
    'app': ('/Applications', 'Anamorph.app'),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def disposable_guard(platform, environment):
    require(platform == 'darwin' and environment.get('GITHUB_ACTIONS') == 'true'
            and environment.get('RUNNER_ENVIRONMENT') == 'github-hosted'
            and environment.get('RUNNER_OS') == 'macOS'
            and environment.get('GITHUB_REPOSITORY') == 'skyRolly/' + PRODUCT,
            'installation requires this repository\'s disposable GitHub-hosted macOS VM')


def tree(path):
    require(path.is_dir() and not path.is_symlink(), 'missing/linked bundle: ' + str(path))
    result = {}
    for item in sorted(path.rglob('*')):
        key = item.relative_to(path).as_posix()
        if item.is_symlink():
            result[key] = {'symlink': os.readlink(item)}
        elif item.is_file():
            result[key] = hashlib.sha256(item.read_bytes()).hexdigest()
    require(bool(result), 'empty bundle')
    return result


def validate_component(suffix, receipt, files, bundle, staged, version):
    location, name = COMPONENTS[suffix]
    require(receipt.get('pkgid') == BASE + '.' + suffix, 'wrong receipt identity')
    require(receipt.get('pkg-version') == version, 'wrong receipt version')
    require(receipt.get('volume') == '/', 'wrong receipt volume')
    require('/' + str(receipt.get('install-location', '')).strip('/') == location,
            'wrong receipt installation location')
    paths = [PurePosixPath(line) for line in files.splitlines() if line]
    require(bool(paths) and all(not p.is_absolute() and '..' not in p.parts
            and p.parts[0] == name for p in paths), 'receipt owns a foreign bundle/path')
    require({f'{name}/Contents/Info.plist', f'{name}/Contents/MacOS/{PRODUCT}'}
            <= {p.as_posix() for p in paths}, 'receipt misses installed metadata/executable')
    info = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
    require(info.get('CFBundleIdentifier') == BASE, 'wrong installed CFBundleIdentifier')
    if suffix == 'au':
        components = info.get('AudioComponents', [])
        require(len(components) == 1 and all(components[0].get(k) == v for k, v in
                {'type': 'aufx', 'subtype': CODE, 'manufacturer': 'RTec'}.items()),
                'installed AU identity changed')
    installed = tree(bundle)
    require(installed == tree(staged), 'installed bundle differs from staged customer payload')
    return {'receipt': receipt, 'bundle': str(bundle), 'CFBundleIdentifier': BASE,
            'file_count': len(paths), 'payload_sha256': installed}


def validate_separation(before, after, sibling_before, sibling_after):
    require(after - before == {BASE + '.' + key for key in COMPONENTS},
            'unexpected installed receipt additions')
    require(before <= after, 'an existing receipt disappeared')
    require(sibling_before == sibling_after, 'sibling product receipts changed')


def self_test():
    def rejects(call, message):
        try:
            call()
        except (ValueError, OSError, plistlib.InvalidFileException) as error:
            require(message in str(error), 'fixture failed for the wrong reason: ' + str(error))
            return
        raise AssertionError('receipt gate accepted: ' + message)

    environment = {'GITHUB_ACTIONS': 'true', 'RUNNER_ENVIRONMENT': 'github-hosted',
                   'RUNNER_OS': 'macOS', 'GITHUB_REPOSITORY': 'skyRolly/' + PRODUCT}
    disposable_guard('darwin', environment)
    for key in environment:
        bad = dict(environment); bad[key] = 'untrusted'
        rejects(lambda: disposable_guard('darwin', bad), 'disposable')
    rejects(lambda: disposable_guard('linux', environment), 'disposable')
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        for suffix, (location, name) in COMPONENTS.items():
            bundle = root / 'installed' / name; staged = root / 'staged' / name
            info = {'CFBundleIdentifier': BASE}
            if suffix == 'au':
                info['AudioComponents'] = [{'type': 'aufx', 'subtype': CODE, 'manufacturer': 'RTec'}]
            for path in (bundle, staged):
                (path / 'Contents/MacOS').mkdir(parents=True)
                (path / 'Contents/MacOS' / PRODUCT).write_bytes(b'synthetic payload, not native installation')
                (path / 'Contents/Info.plist').write_bytes(plistlib.dumps(info))
            receipt = {'pkgid': BASE + '.' + suffix, 'pkg-version': '1.2.3',
                       'volume': '/', 'install-location': location}
            files = f'{name}/Contents/Info.plist\n{name}/Contents/MacOS/{PRODUCT}\n'
            def check(r=receipt, f=files):
                return validate_component(suffix, r, f, bundle, staged, '1.2.3')
            check()
            for key, value, message in [('pkgid', BASE + '.other', 'identity'),
                    ('pkg-version', '9.9.9', 'version'), ('volume', '/other', 'volume'),
                    ('install-location', '/Applications/Wrong', 'location')]:
                bad = dict(receipt); bad[key] = value
                rejects(lambda: check(bad), message)
            for bad in (files + 'Foreign.app/Contents/Info.plist\n', files + '../foreign\n', files + '/absolute\n'):
                rejects(lambda: check(f=bad), 'foreign')
            rejects(lambda: check(f=name + '/Contents/Info.plist\n'), 'misses')
            payload = bundle / 'Contents/MacOS' / PRODUCT
            payload.write_bytes(b'altered'); rejects(check, 'differs')
            payload.unlink(); rejects(check, 'differs')
            payload.write_bytes((staged / 'Contents/MacOS' / PRODUCT).read_bytes())
            bad_info = dict(info); bad_info['CFBundleIdentifier'] = 'wrong'
            (bundle / 'Contents/Info.plist').write_bytes(plistlib.dumps(bad_info))
            rejects(check, 'CFBundleIdentifier')
            if suffix == 'au':
                bad_info = dict(info); bad_info['AudioComponents'] = []
                (bundle / 'Contents/Info.plist').write_bytes(plistlib.dumps(bad_info))
                rejects(check, 'AU identity')
            (bundle / 'Contents/Info.plist').unlink(); rejects(check, 'Info.plist')
    expected = {BASE + '.' + key for key in COMPONENTS}
    validate_separation({'existing'}, {'existing'} | expected, {'sibling': 'before'}, {'sibling': 'before'})
    rejects(lambda: validate_separation(set(), expected | {'unexpected'}, {}, {}), 'additions')
    rejects(lambda: validate_separation({'existing'}, expected, {}, {}), 'disappeared')
    rejects(lambda: validate_separation(set(), expected, {'other': 'before'}, {'other': 'changed'}), 'sibling')
    print('installed-receipt self-test passed: 3 valid components, metadata/payload/ownership/separation/guard negatives')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--package', type=Path)
    parser.add_argument('--staged-dir', type=Path)
    parser.add_argument('--report', type=Path, default=Path('build/installed-receipts-macos.json'))
    args = parser.parse_args()
    if args.self_test:
        self_test(); return
    disposable_guard(sys.platform, os.environ)
    require(args.package and args.package.is_file() and args.staged_dir and args.staged_dir.is_dir(),
            'real package and staged customer tree required')
    root = Path(__file__).resolve().parent.parent
    version = re.search(r'^project\(' + PRODUCT + r' VERSION (\d+\.\d+\.\d+) ',
                        (root / 'CMakeLists.txt').read_text(), re.M).group(1)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    evidence = args.report.parent / (PRODUCT.lower() + '-receipt-logs')
    evidence.mkdir(exist_ok=True)
    report = {'product': PRODUCT, 'package': str(args.package.resolve()),
              'package_sha256': hashlib.sha256(args.package.read_bytes()).hexdigest(),
              'version': version, 'components': {}, 'failure': None, 'installation_started': False,
              'scope': 'real default full installation on disposable GitHub macOS VM; no GUI/DAW certification'}

    def command(*argv):
        return subprocess.run(argv, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout

    def sibling_snapshot(ids):
        return {identifier: {'info': plistlib.loads(command('/usr/sbin/pkgutil', '--pkg-info-plist', identifier)),
                'files': command('/usr/sbin/pkgutil', '--files', identifier).decode()}
                for identifier in sorted(ids) if identifier.startswith(SIBLING_BASE + '.')}

    try:
        before = set(command('/usr/sbin/pkgutil', '--pkgs').decode().splitlines())
        require(not any(identifier == BASE or identifier.startswith(BASE + '.') for identifier in before),
                'existing product receipt: refusing installation')
        for location, name in COMPONENTS.values():
            path = Path(location) / name
            require(not path.exists() and not path.is_symlink(), 'existing product bundle: ' + str(path))
        sibling_before = sibling_snapshot(before)
        sibling_product = SIBLING_BASE.rsplit('.', 1)[-1].capitalize()
        sibling_paths = [Path(location) / (sibling_product + extension) for location, extension in
                         [('/Library/Audio/Plug-Ins/VST3', '.vst3'),
                          ('/Library/Audio/Plug-Ins/Components', '.component'), ('/Applications', '.app')]]
        sibling_payload = {str(path): tree(path) for path in sibling_paths if path.exists()}
        (evidence / 'receipts-before.txt').write_text('\n'.join(sorted(before)) + '\n')
        report['installation_started'] = True
        # No receipt forgetting or filesystem cleanup. Discarding the authorized
        # disposable VM is cleanup; a refusal never deletes pre-existing files.
        result = subprocess.run(['sudo', '-n', '/usr/sbin/installer', '-pkg', str(args.package.resolve()), '-target', '/'],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (evidence / 'installer.log').write_bytes(result.stdout)
        require(result.returncode == 0, 'native installer failed: ' + result.stdout.decode(errors='replace'))
        after = set(command('/usr/sbin/pkgutil', '--pkgs').decode().splitlines())
        (evidence / 'receipts-after.txt').write_text('\n'.join(sorted(after)) + '\n')
        validate_separation(before, after, sibling_before, sibling_snapshot(after))
        require(sibling_payload == {str(path): tree(path) for path in sibling_paths if path.exists()},
                'sibling installed payload changed')
        for suffix, (location, name) in COMPONENTS.items():
            identifier = BASE + '.' + suffix
            plain = command('/usr/sbin/pkgutil', '--pkg-info', identifier)
            raw = command('/usr/sbin/pkgutil', '--pkg-info-plist', identifier)
            files = command('/usr/sbin/pkgutil', '--files', identifier).decode()
            (evidence / (suffix + '.info.txt')).write_bytes(plain)
            (evidence / (suffix + '.plist')).write_bytes(raw)
            (evidence / (suffix + '.files.txt')).write_text(files)
            report['components'][suffix] = validate_component(suffix, plistlib.loads(raw), files,
                    Path(location) / name, args.staged_dir / name, version)
        report['preexisting_receipts_preserved'] = True
        report['sibling_receipts_unchanged'] = True
        report['sibling_receipt_count'] = len(sibling_before)
        # An empty sibling snapshot is explicitly not co-installation evidence.
        report['both_product_coinstallation_tested'] = False
        print(json.dumps(report, indent=2))
    except Exception as error:
        report['failure'] = str(error)
        raise
    finally:
        args.report.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
