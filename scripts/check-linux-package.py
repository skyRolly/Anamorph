#!/usr/bin/env python3
"""Validate VST3-only Linux installation and the portable customer ZIP contract."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parent.parent
ENTRIES = {'Anamorph.vst3', 'Anamorph', 'install.sh', 'uninstall.sh', 'INSTALL.txt'}
MODULE = 'Anamorph.vst3/Contents/x86_64-linux/Anamorph.so'


def require(condition, message):
    if not condition: raise ValueError(message)


def block(text, name):
    start = text.index('# BEGIN ' + name)
    return text[start:text.index('# END ' + name, start)]


def source_contract(root):
    packaging = root / 'packaging/linux'
    install = (packaging / 'install.sh').read_text()
    uninstall = (packaging / 'uninstall.sh').read_text()
    for name in ('legacy desktop cleanup', 'plugin destination checks'):
        require(block(install, name) == block(uninstall, name), 'entry-point safety blocks differ')
    require('APP_SRC=' not in install and 'desktop_integration ' not in install,
            'Linux installer still installs standalone/desktop resources')
    require(not (packaging / 'desktop-integration.sh').exists(), 'obsolete desktop helper remains')
    require(not (root / 'resources/icons/linux').exists(), 'obsolete Linux desktop assets remain')
    return {'vst3_only_installation': True, 'standalone_portable': True,
            'legacy_cleanup': 'embedded receipt/hash checks; no archive helper execution'}


def staged_contract(stage, root=ROOT):
    require({path.name for path in stage.iterdir()} == ENTRIES, 'incorrect Linux archive members')
    for name in ('install.sh', 'uninstall.sh', 'INSTALL.txt'):
        require((stage / name).read_bytes() == (root / 'packaging/linux' / name).read_bytes(),
                'staged instruction/script differs: ' + name)
    for path in stage.rglob('*'):
        require(not path.is_symlink(), 'linked customer payload')
        require(not (path.name.endswith(('.desktop', '.metainfo.xml', '.appdata.xml')) or
                     path.name in ('desktop-assets', 'hicolor', 'metainfo', 'appdata')),
                'obsolete desktop/AppStream payload')
    for name in ('Anamorph', MODULE):
        path = stage / name
        require(path.is_file() and path.read_bytes()[:4] == b'\x7fELF', 'missing Linux ELF: ' + name)
    for name in ('Anamorph', MODULE, 'install.sh', 'uninstall.sh'):
        require((stage / name).stat().st_mode & 0o777 == 0o755, 'incorrect executable mode: ' + name)
    return {'files': {str(path.relative_to(stage)): hashlib.sha256(path.read_bytes()).hexdigest()
                      for path in stage.rglob('*') if path.is_file()}, **source_contract(root)}


def self_test():
    source_contract(ROOT)
    with tempfile.TemporaryDirectory(prefix='anamorph-package-') as directory:
        stage = Path(directory)
        for name in ('install.sh', 'uninstall.sh', 'INSTALL.txt'):
            shutil.copy2(ROOT / 'packaging/linux' / name, stage / name)
        for name in ('Anamorph', MODULE):
            path = stage / name; path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'\x7fELF inert contract fixture')
        for name in ('Anamorph', MODULE, 'install.sh', 'uninstall.sh'):
            (stage / name).chmod(0o755)
        staged_contract(stage)
        for name in ('desktop-assets/foreign.png', 'extra.keep',
                     'Anamorph.vst3/Contents/tech.rolly.anamorph.metainfo.xml'):
            path = stage / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'obsolete')
            try: staged_contract(stage)
            except ValueError: pass
            else: raise AssertionError('accepted foreign/desktop payload: ' + name)
            path.unlink()
            if (stage / 'desktop-assets').exists(): (stage / 'desktop-assets').rmdir()
        script = stage / 'install.sh'; original = script.read_bytes(); script.write_bytes(original + b'\n# changed\n')
        try: staged_contract(stage)
        except ValueError: pass
        else: raise AssertionError('accepted changed installer')
        script.write_bytes(original)
        for name in ('install.sh', 'uninstall.sh', 'Anamorph', MODULE):
            path = stage / name; path.chmod(0o644)
            try:
                staged_contract(stage)
            except ValueError as error:
                require(str(error) == 'incorrect executable mode: ' + name,
                        'mode negative failed for an unrelated reason: ' + str(error))
            else:
                raise AssertionError('accepted non-executable payload: ' + name)
            finally:
                path.chmod(0o755)
            staged_contract(stage)
    print('Linux package self-test: valid archive and metadata/script/mode negatives checked')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--staged-dir', type=Path)
    args = parser.parse_args()
    if args.self_test: self_test()
    else:
        result = staged_contract(args.staged_dir) if args.staged_dir else source_contract(ROOT)
        print(json.dumps(result, indent=2))


if __name__ == '__main__': main()
