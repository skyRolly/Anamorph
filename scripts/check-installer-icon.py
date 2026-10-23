#!/usr/bin/env python3
"""Check the approved installer ICO, unconditional install contract and native MAINICON."""

from contextlib import contextmanager
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import tempfile
from unittest.mock import patch
import zlib
import binascii
import argparse

ROOT = Path(__file__).resolve().parent.parent
ICO_PATH = 'resources/icons/windows/Anamorph.ico'
APPROVED_SHA = '17c2f958e07de4d0e1bcfd8d6e67f1e254368b7ea11764e12eca8da2742fbdf1'
SOURCE_COMMIT = '35cf982f9d2e33bde5c4a1bf84b17205ce1e6b80'
SIZES = {16, 20, 24, 32, 40, 48, 64, 128, 256}
COMPILER_PATH = r'{#SourcePath}..\..\resources\icons\windows\Anamorph.ico'
INSTALLED_ICON = r'{app}\resources\Anamorph.ico'
spec = importlib.util.spec_from_file_location('native_icons', ROOT / 'scripts/check-icons.py')
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)
require = native.require


def frames(data):
    require(len(data) >= 6, 'truncated installer ICO')
    reserved, kind, count = struct.unpack_from('<HHH', data)
    require((reserved, kind) == (0, 1) and count > 0 and 6 + count * 16 <= len(data), 'invalid installer ICO directory')
    result = {}
    for index in range(count):
        entry = data[6 + index * 16:22 + index * 16]
        w, h, colours, reserved, planes, bits, length, offset = struct.unpack('<BBBBHHII', entry)
        width = w or 256
        # Approved Pillow PNG ICOs have planes=0; JUCE's distinct ICO uses 1.
        # Keep the Round 1 pinned-JUCE parser strict; validate this container here.
        require(width == (h or 256) and reserved == 0 and planes in (0, 1) and bits == 32, 'invalid installer ICO entry')
        require(width not in result and offset >= 6 + count * 16 and offset + length <= len(data), 'invalid installer ICO offsets')
        payload = data[offset:offset + length]
        require(native.png_size(payload) == width, 'installer ICO PNG dimensions differ')
        result[width] = {'header': entry[:12], 'payload': payload}
    require(set(result) == SIZES, 'installer ICO frame sizes differ')
    return result


def source(root):
    data = (root / ICO_PATH).read_bytes()
    provenance = json.loads((root / 'resources/icons/windows/PROVENANCE.json').read_text())
    require(hashlib.sha256(data).hexdigest() == provenance['sha256'] == APPROVED_SHA, 'unapproved installer ICO hash')
    require(provenance['source_repository'] == 'https://github.com/skyRolly/icon-vector-reconstruction'
            and provenance['source_commit'] == SOURCE_COMMIT and provenance['selected_design'] == 'B0'
            and provenance['source_path'] == 'assets/anamorph/windows/Anamorph.ico', 'invalid installer icon provenance')
    images = frames(data)
    text = (root / 'packaging/windows/Anamorph.iss').read_text()
    directives = dict(re.findall(r'^([A-Za-z]+)=(.+)$', text, re.M))
    require(directives.get('SetupIconFile') == COMPILER_PATH, 'incorrect SetupIconFile')
    require(directives.get('UninstallDisplayIcon') == INSTALLED_ICON, 'uninstall icon must be independent of Standalone')
    require(directives.get('AppId') == '{{D1E3D8F8-C9CE-415C-AC73-A6AA842987BD}', 'Windows AppId changed')
    entries = [line for line in text.splitlines() if line.startswith('Source:') and COMPILER_PATH in line]
    require(len(entries) == 1 and 'Components:' not in entries[0] and 'Check:' not in entries[0]
            and 'DestDir: "{app}\\resources"' in entries[0] and 'ignoreversion' in entries[0], 'installer icon must be installed unconditionally')
    require('Name: "{code:GetVst3Dir}\\Anamorph.vst3"; Components: vst3; Attribs: system' in text, 'installed VST3 bundle must retain its System attribute')
    return images


def main_icon(path, expected):
    require(sys.platform == 'win32', 'MAINICON inspection requires Windows')
    require(path.is_file(), f'missing PE file: {path}')
    with native.windows_resource_api(path) as (names, read):
        require('MAINICON' in names, 'missing native MAINICON group')
        group = read(14, 'MAINICON')
        require(len(group) >= 6, 'truncated MAINICON')
        reserved, kind, count = struct.unpack_from('<HHH', group)
        require((reserved, kind, count) == (0, 1, len(expected)) and len(group) == 6 + count * 14, 'invalid MAINICON directory')
        seen = set()
        for index in range(count):
            entry = group[6 + index * 14:20 + index * 14]
            width = entry[0] or 256
            require(width in expected and width not in seen and entry[:12] == expected[width]['header'], 'MAINICON frame header differs')
            seen.add(width)
            resource_id = struct.unpack_from('<H', entry, 12)[0]
            require(read(3, resource_id) == expected[width]['payload'], f'MAINICON {width}px pixels differ')
        require(seen == set(expected), 'missing MAINICON frames')
    return {'path': str(path), 'group': 'MAINICON', 'frame_sizes': sorted(seen), 'source_sha256': APPROVED_SHA}


def self_test():
    def rejects(function, *args):
        try:
            function(*args)
        except (ValueError, KeyError, OSError, struct.error, zlib.error):
            return
        raise AssertionError('installer icon gate accepted seeded invalid input')

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', binascii.crc32(kind + data) & 0xffffffff)

    entries, payload, offset = [], b'', 6 + 16 * len(SIZES)
    for size in sorted(SIZES):
        png = native.PNG + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress((b'\0' + b'\x00\x44\x88\xff' * size) * size)) + chunk(b'IEND', b'')
        entries.append(struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 0, 32, len(png), offset))
        payload += png
        offset += len(png)
    ico = struct.pack('<HHH', 0, 1, len(SIZES)) + b''.join(entries) + payload
    expected = frames(ico)
    rejects(frames, ico[:-1])
    with tempfile.TemporaryDirectory() as directory, patch.dict(globals(), APPROVED_SHA=hashlib.sha256(ico).hexdigest()):
        root = Path(directory)
        (root / 'resources/icons/windows').mkdir(parents=True)
        (root / 'packaging/windows').mkdir(parents=True)
        icon = root / ICO_PATH
        icon.write_bytes(ico)
        provenance = {'source_repository': 'https://github.com/skyRolly/icon-vector-reconstruction', 'source_commit': SOURCE_COMMIT,
                      'selected_design': 'B0', 'source_path': 'assets/anamorph/windows/Anamorph.ico', 'sha256': APPROVED_SHA}
        (root / 'resources/icons/windows/PROVENANCE.json').write_text(json.dumps(provenance))
        contract = ('SetupIconFile=' + COMPILER_PATH + '\nUninstallDisplayIcon=' + INSTALLED_ICON +
                    '\nAppId={{D1E3D8F8-C9CE-415C-AC73-A6AA842987BD}\nSource: "' + COMPILER_PATH +
                    '"; DestDir: "{app}\\resources"; Flags: ignoreversion\n'
                    'Name: "{code:GetVst3Dir}\\Anamorph.vst3"; Components: vst3; Attribs: system\n')
        iss = root / 'packaging/windows/Anamorph.iss'
        iss.write_text(contract)
        source(root)
        for invalid in (contract.replace(COMPILER_PATH, 'missing.ico', 1), contract.replace(INSTALLED_ICON, r'{app}\Anamorph.exe'),
                        contract.replace('Flags: ignoreversion', 'Components: standalone; Flags: ignoreversion'),
                        contract.replace('Attribs: system', 'Attribs: hidden')):
            iss.write_text(invalid)
            rejects(source, root)
        iss.write_text(contract)
        icon.write_bytes(ico + b'altered')
        rejects(source, root)
        icon.unlink()
        rejects(source, root)
        pe = root / 'setup.exe'
        pe.write_bytes(b'MZ synthetic API boundary fixture')
        group = struct.pack('<HHH', 0, 1, len(expected)) + b''.join(entry['header'] + struct.pack('<H', index) for index, entry in enumerate(expected.values(), 100))
        resources = {(14, 'MAINICON'): group, **{(3, index): entry['payload'] for index, entry in enumerate(expected.values(), 100)}}

        @contextmanager
        def api(path):
            yield [name for kind, name in resources if kind == 14], lambda kind, name: resources[(kind, name)]

        with patch.object(sys, 'platform', 'win32'), patch.object(native, 'windows_resource_api', api):
            main_icon(pe, expected)
            valid = resources[(14, 'MAINICON')]
            for invalid in (valid[:-1], valid[:6] + b'\x20' + valid[7:]):
                resources[(14, 'MAINICON')] = invalid
                rejects(main_icon, pe, expected)
            resources[(14, 'MAINICON')] = valid
            original = resources[(3, 100)]
            resources[(3, 100)] = b'altered pixels'
            rejects(main_icon, pe, expected)
            del resources[(3, 100)]
            rejects(main_icon, pe, expected)
            resources[(3, 100)] = original
            del resources[(14, 'MAINICON')]
            rejects(main_icon, pe, expected)
    print('installer icon self-test passed (contract, missing/altered ICO and matching-platform MAINICON good/bad fixtures)')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--setup-exe', type=Path)
    parser.add_argument('--uninstall-exe', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    expected = source(ROOT)
    result = {'ICO_sha256': APPROVED_SHA, 'frames': sorted(expected), 'unconditional_installed_icon': INSTALLED_ICON}
    for field, path in [('setup', args.setup_exe), ('uninstaller', args.uninstall_exe)]:
        if path:
            result[field] = main_icon(path, expected)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError, struct.error, zlib.error) as error:
        print(f'::error::installer icon: {error}', file=sys.stderr)
        sys.exit(1)
