#!/usr/bin/env python3
"""Verify approved Linux handoff bytes and the customer archive contract."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import shutil

ROOT = Path(__file__).resolve().parent.parent
SOURCE = '203bb326d02fcddeb4c9989c65cf44010f1eaff6'
SUMS = '7bd45fbb113c1d11136e20c0802824b49c8b409ae31ab1ca8fb17db3bc82e127'
SOURCE_MANIFEST = '0fa5cc7418df76e13d01187fc659bf2bbf36063f9b5285eb5de002f3f537d7b5'
NAME = 'tech.rolly.anamorph'
SIZES = (16, 24, 32, 48, 64, 128, 256, 512)
FILES = {f'hicolor/{size}x{size}/apps/{NAME}.png' for size in SIZES} | {
    f'hicolor/scalable/apps/{NAME}.svg', NAME + '.desktop'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate_assets(assets):
    require(assets.is_dir() and not assets.is_symlink(), 'missing/linked Linux assets')
    raw = (assets / 'SHA256SUMS').read_bytes()
    require(hashlib.sha256(raw).hexdigest() == SUMS, 'unapproved Linux hash manifest')
    mapping = {line.split('  ', 1)[1]: line.split('  ', 1)[0] for line in raw.decode().splitlines()}
    require(set(mapping) == FILES, 'incorrect Linux resource names')
    provenance = json.loads((assets / 'PROVENANCE.json').read_text())
    require(provenance['source_repository'] == 'https://github.com/skyRolly/icon-vector-reconstruction'
            and provenance['source_commit'] == SOURCE and provenance['selected_design'] == 'B0'
            and provenance['SHA256SUMS_sha256'] == SUMS
            and provenance['source_manifest_path'] == 'assets/anamorph/manifests/assets.json'
            and provenance['source_manifest_sha256'] == SOURCE_MANIFEST, 'incorrect Linux provenance')
    require({entry['path']: entry['sha256'] for entry in provenance['files']} == mapping,
            'provenance file inventory differs')
    require({p.relative_to(assets).as_posix() for p in assets.rglob('*') if p.is_file()}
            == FILES | {'PROVENANCE.json', 'SHA256SUMS'}, 'extra/missing Linux archive resource')
    for relative, digest in mapping.items():
        path = assets / relative
        require(path.is_file() and not path.is_symlink()
                and not any(p.is_symlink() for p in path.parents if p != assets.parent), 'missing/linked Linux resource')
        data = path.read_bytes()
        require(hashlib.sha256(data).hexdigest() == digest, 'altered approved Linux resource: ' + relative)
        if relative.endswith('.png'):
            size = int(relative.split('/')[1].split('x')[0])
            require(data[:8] == b'\x89PNG\r\n\x1a\n' and len(data) > 33
                    and struct.unpack_from('>IIBB', data, 16) == (size, size, 8, 6), 'invalid hicolor RGBA PNG')
    desktop = (assets / (NAME + '.desktop')).read_text()
    require(desktop == '[Desktop Entry]\nType=Application\nName=Anamorph\nComment=Stereo image toolkit\n'
            'Exec=Anamorph\nIcon=tech.rolly.anamorph\nTerminal=false\nCategories=AudioVideo;Audio;\n',
            'unexpected source desktop contract')
    return {'source_commit': SOURCE, 'assets': mapping, 'unchanged_approved_bytes': True}


def staged(path):
    result = validate_assets(path / 'desktop-assets')
    for file in ('install.sh', 'uninstall.sh', 'desktop-integration.sh', 'INSTALL.txt', 'Anamorph'):
        require((path / file).is_file() and not (path / file).is_symlink(), 'missing staged file: ' + file)
    require((path / 'Anamorph.vst3/Contents/x86_64-linux/Anamorph.so').is_file(), 'missing staged VST3 module')
    for file in ('install.sh', 'uninstall.sh', 'desktop-integration.sh', 'INSTALL.txt'):
        require((path / file).read_bytes() == (ROOT / 'packaging/linux' / file).read_bytes(), 'stale staged installer: ' + file)
    return result


def self_test():
    source = ROOT / 'resources/icons/linux'
    with tempfile.TemporaryDirectory() as directory:
        target = Path(directory) / 'assets'
        shutil.copytree(source, target)
        validate_assets(target)
        mutations = [target / next(p for p in sorted(FILES) if p.endswith('.png')),
                     target / (NAME + '.desktop'), target / 'SHA256SUMS', target / 'PROVENANCE.json']
        for path in mutations:
            old = path.read_bytes(); path.write_bytes(old + b'altered')
            rejected = False
            try:
                validate_assets(target)
            except (ValueError, OSError, json.JSONDecodeError):
                rejected = True
            require(rejected, 'source gate accepted altered resource')
            path.write_bytes(old)
        image = target / next(p for p in sorted(FILES) if p.endswith('.png'))
        image.unlink()
        try:
            validate_assets(target)
        except (ValueError, OSError):
            pass
        else:
            raise AssertionError('source gate accepted missing image')
    print('Linux desktop source self-test passed: valid handoff and modified/missing input negatives')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--staged-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test(); return
    report = staged(args.staged_dir) if args.staged_dir else validate_assets(ROOT / 'resources/icons/linux')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
