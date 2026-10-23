#!/usr/bin/env python3
"""Verify product namespace and independent identities in source and generated metadata."""

import argparse
import ctypes
import json
from pathlib import Path
import plistlib
import re
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
PRODUCT = 'Anamorph'
BASE = 'tech.rolly.anamorph'
CODE = 'Anmr'
APP_ID = 'D1E3D8F8-C9CE-415C-AC73-A6AA842987BD'
CIDS = {'Audio Module Class': 'ABCDEF019182FAEB52546563416E6D72',
        'Component Controller Class': 'ABCDEF011234ABCD52546563416E6D72'}
COMPONENTS = {BASE + '.' + suffix for suffix in ('vst3', 'au', 'app')}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def source(root):
    cmake = (root / 'CMakeLists.txt').read_text()
    plugin = cmake.split(f'juce_add_plugin({PRODUCT}\n', 1)[1].split('\n)', 1)[0]
    for field, value in {'BUNDLE_ID': BASE, 'COMPANY_NAME': 'RollyTech', 'PRODUCT_NAME': PRODUCT,
                         'PLUGIN_MANUFACTURER_CODE': 'RTec', 'PLUGIN_CODE': CODE}.items():
        found = re.findall(r'^\s*' + field + r'\s+"?([^"\s]+)', plugin, re.M)
        require(found == [value], f'incorrect {field}: {found}')
    iss = (root / f'packaging/windows/{PRODUCT}.iss').read_text()
    require(re.findall(r'^AppId=\{\{([^}]+)\}', iss, re.M) == [APP_ID], 'Windows AppId changed')
    package = (root / 'packaging/macos/build-pkg.sh').read_text()
    ids = re.findall(r'--identifier\s+"?([^"\s\\]+)', package)
    require(set(ids) == {BASE + '.$id', BASE + '.probe'}, f'incorrect pkgbuild identifiers: {ids}')
    refs = re.findall(r'<pkg-ref id="([^"]+)"', package)
    require(set(refs) == COMPONENTS and len(refs) == 6, 'incorrect distribution component identifiers')
    return {'base': BASE, 'WindowsAppId': APP_ID, 'manufacturer': 'RTec', 'plugin_code': CODE,
            'package_components': sorted(COMPONENTS)}


def moduleinfo(path):
    # The pinned SDK writes JSON with trailing commas. Remove only those commas.
    info = json.loads(re.sub(r',\s*([}\]])', r'\1', path.read_text()))
    require(info['Name'] == PRODUCT and info['Factory Info']['Vendor'] == 'RollyTech', 'VST3 product/vendor changed')
    classes = {item['Category']: item['CID'] for item in info['Classes']}
    require(classes == CIDS and len(info['Classes']) == 2, f'VST3 class identity changed: {classes}')
    return classes


def windows_version(path):
    """Read native VERSIONINFO without loading executable code."""
    from ctypes import wintypes
    api = ctypes.WinDLL('version', use_last_error=True)
    api.GetFileVersionInfoSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
    api.GetFileVersionInfoSizeW.restype = wintypes.DWORD
    api.GetFileVersionInfoW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    api.GetFileVersionInfoW.restype = wintypes.BOOL
    api.VerQueryValueW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(wintypes.UINT)]
    api.VerQueryValueW.restype = wintypes.BOOL
    ignored = wintypes.DWORD()
    size = api.GetFileVersionInfoSizeW(str(path.resolve()), ctypes.byref(ignored))
    require(size, f'missing VERSIONINFO: {path}')
    data = ctypes.create_string_buffer(size)
    require(api.GetFileVersionInfoW(str(path.resolve()), 0, size, data), 'cannot read VERSIONINFO')

    def query(key):
        pointer, length = ctypes.c_void_p(), wintypes.UINT()
        require(api.VerQueryValueW(data, key, ctypes.byref(pointer), ctypes.byref(length)), f'missing version field: {key}')
        return pointer, length.value

    pointer, length = query('\\VarFileInfo\\Translation')
    require(length >= 4, 'missing VERSIONINFO translation')
    language, encoding = struct.unpack('<HH', ctypes.string_at(pointer, 4))
    result = {}
    for field in ('CompanyName', 'ProductName', 'FileDescription', 'InternalName', 'OriginalFilename'):
        pointer, length = query(f'\\StringFileInfo\\{language:04x}{encoding:04x}\\{field}')
        result[field] = ctypes.wstring_at(pointer, max(0, length - 1))
    return result


def build_metadata(build, platform):
    generated = build / f'{PRODUCT}_artefacts/JuceLibraryCode'
    info = dict(record.split('\x1f', 1) for record in (generated / 'Info.txt').read_text().split('\x1e') if '\x1f' in record)
    require(info['BUNDLE_ID'] == BASE, 'generated bundle ID differs')
    require(info['PLUGIN_MANUFACTURER_CODE'] == 'RTec' and info['PLUGIN_CODE'] == CODE, 'generated plugin codes differ')
    definitions = (generated / 'Release/Defs.txt').read_text()
    for item in (f'JucePlugin_CFBundleIdentifier={BASE};', 'JucePlugin_ManufacturerCode=0x52546563;',
                 f'JucePlugin_PluginCode=0x{CODE.encode().hex()};', 'JUCE_VST3_CAN_REPLACE_VST2=0;'):
        require(item in definitions, f'generated definition missing: {item}')
    cache = (build / 'CMakeCache.txt').read_text()
    standalone_key = PRODUCT.upper() + '_BUILD_STANDALONE:BOOL='
    require(standalone_key + 'ON' in cache or standalone_key + 'OFF' in cache, 'missing Standalone policy')
    standalone = standalone_key + 'ON' in cache
    products = build / f'{PRODUCT}_artefacts/Release'
    bundle = products / f'VST3/{PRODUCT}.vst3'
    result = {'generated_base': info['BUNDLE_ID'], 'VST3Classes': moduleinfo(bundle / 'Contents/Resources/moduleinfo.json')}
    if platform == 'macos':
        bundles = [bundle, products / f'AU/{PRODUCT}.component']
        if standalone:
            bundles.append(products / f'Standalone/{PRODUCT}.app')
        result['bundles'] = []
        for path in bundles:
            require((path / f'Contents/MacOS/{PRODUCT}').is_file(), f'missing compiled bundle: {path}')
            plist = plistlib.loads((path / 'Contents/Info.plist').read_bytes())
            require(plist['CFBundleIdentifier'] == BASE, f'incorrect CFBundleIdentifier: {path}')
            row = {'path': str(path), 'CFBundleIdentifier': plist['CFBundleIdentifier']}
            if path.suffix == '.component':
                components = plist['AudioComponents']
                require(len(components) == 1, 'unexpected AU components')
                row['AudioComponents'] = components
                require((components[0]['type'], components[0]['subtype'], components[0]['manufacturer']) == ('aufx', CODE, 'RTec'), 'AU component identity changed')
            result['bundles'].append(row)
    else:
        module = bundle / ('Contents/x86_64-win/' + PRODUCT + '.vst3' if platform == 'windows' else 'Contents/x86_64-linux/' + PRODUCT + '.so')
        targets = [module]
        if standalone:
            targets.append(products / ('Standalone/' + PRODUCT + ('.exe' if platform == 'windows' else '')))
        require(all(path.is_file() for path in targets), 'missing compiled product')
        if platform == 'windows':
            result['WindowsVersionInfo'] = {str(path): windows_version(path) for path in targets}
            for row in result['WindowsVersionInfo'].values():
                require(row['CompanyName'] == 'RollyTech' and row['ProductName'] == PRODUCT, 'Windows product/vendor changed')
        else:
            result['LinuxNamespace'] = 'JUCE generation metadata only; ELF has no CFBundleIdentifier; no desktop resource installed'
    return result


def package_metadata(expanded):
    paths = list(expanded.rglob('PackageInfo'))
    require(len(paths) == 3, f'expected three PackageInfo files, found {len(paths)}')
    ids = []
    for path in paths:
        info = ET.parse(path).getroot()
        ids.append(info.attrib['identifier'])
        bundles = info.findall('.//bundle')
        require(bundles and all(item.attrib.get('id') == BASE for item in bundles), f'package bundle namespace differs: {path}')
    require(set(ids) == COMPONENTS and len(set(ids)) == 3, f'incorrect package receipts: {ids}')
    distribution = ET.parse(expanded / 'Distribution').getroot()
    refs = [item.attrib['id'] for item in distribution.findall('.//pkg-ref')]
    require(set(refs) == COMPONENTS and len(refs) == 6, f'incorrect productbuild references: {refs}')
    return {'package_receipts': sorted(ids), 'distribution_refs': refs}


def self_test():
    def rejects(fn, *args):
        try:
            fn(*args)
        except (ValueError, KeyError, IndexError, OSError, ET.ParseError):
            return
        raise AssertionError('identity checker accepted seeded invalid metadata')

    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / 'packaging/windows').mkdir(parents=True)
        (root / 'packaging/macos').mkdir()
        cmake = f'juce_add_plugin({PRODUCT}\n COMPANY_NAME "RollyTech"\n PRODUCT_NAME "{PRODUCT}"\n BUNDLE_ID "{BASE}"\n PLUGIN_MANUFACTURER_CODE RTec\n PLUGIN_CODE {CODE}\n)\n'
        (root / 'CMakeLists.txt').write_text(cmake)
        (root / f'packaging/windows/{PRODUCT}.iss').write_text('AppId={{' + APP_ID + '}\n')
        package = '--identifier "' + BASE + '.$id"\n--identifier ' + BASE + '.probe\n' + ''.join(f'<pkg-ref id="{item}"/>\n' for item in sorted(COMPONENTS) for _ in range(2))
        (root / 'packaging/macos/build-pkg.sh').write_text(package)
        source(root)
        for field in (BASE, 'RTec', CODE, PRODUCT + '"'):
            (root / 'CMakeLists.txt').write_text(cmake.replace(field, 'invalid.identity'))
            rejects(source, root)
        (root / 'CMakeLists.txt').write_text(cmake)
        (root / f'packaging/windows/{PRODUCT}.iss').write_text('AppId={{00000000-0000-0000-0000-000000000000}\n')
        rejects(source, root)
        (root / f'packaging/windows/{PRODUCT}.iss').write_text('AppId={{' + APP_ID + '}\n')
        (root / 'packaging/macos/build-pkg.sh').write_text(package.replace(BASE, 'invalid.identity'))
        rejects(source, root)
        module = root / 'moduleinfo.json'
        original = {'Name': PRODUCT, 'Factory Info': {'Vendor': 'RollyTech'}, 'Classes': [{'Category': key, 'CID': value} for key, value in CIDS.items()]}
        module.write_text(json.dumps(original))
        moduleinfo(module)
        original['Classes'][0]['CID'] = '0' * 32
        module.write_text(json.dumps(original))
        rejects(moduleinfo, module)
        expanded = root / 'expanded'
        for index, identifier in enumerate(sorted(COMPONENTS)):
            path = expanded / str(index) / 'PackageInfo'
            path.parent.mkdir(parents=True)
            path.write_text(f'<pkg-info identifier="{identifier}"><bundle id="{BASE}"/></pkg-info>')
        distribution = expanded / 'Distribution'
        distribution.write_text('<installer-gui-script>' + ''.join(f'<pkg-ref id="{item}"/>' for item in sorted(COMPONENTS) for _ in range(2)) + '</installer-gui-script>')
        package_metadata(expanded)
        valid = distribution.read_text()
        distribution.write_text(valid.replace(BASE, 'invalid.identity'))
        rejects(package_metadata, expanded)
        distribution.write_text(valid)
        component = expanded / '0/PackageInfo'
        valid = component.read_text()
        component.write_text(valid.replace(BASE, 'invalid.identity'))
        rejects(package_metadata, expanded)
        component.unlink()
        rejects(package_metadata, expanded)
    # Generated metadata uses the production selection and parsing paths on all
    # platforms. Only native Windows VERSIONINFO access is stubbed.
    from unittest.mock import patch
    for platform in ('linux', 'windows', 'macos'):
        for standalone in (False, True):
            with tempfile.TemporaryDirectory() as directory:
                build = Path(directory)
                generated = build / f'{PRODUCT}_artefacts/JuceLibraryCode'
                (generated / 'Release').mkdir(parents=True)
                info = {'BUNDLE_ID': BASE, 'PLUGIN_MANUFACTURER_CODE': 'RTec', 'PLUGIN_CODE': CODE}
                info_path = generated / 'Info.txt'
                info_bytes = ''.join(key + '\x1f' + value + '\x1e' for key, value in info.items())
                info_path.write_text(info_bytes)
                definitions = (f'JucePlugin_CFBundleIdentifier={BASE};JucePlugin_ManufacturerCode=0x52546563;'
                               f'JucePlugin_PluginCode=0x{CODE.encode().hex()};JUCE_VST3_CAN_REPLACE_VST2=0;')
                defs_path = generated / 'Release/Defs.txt'
                defs_path.write_text(definitions)
                (build / 'CMakeCache.txt').write_text(PRODUCT.upper() + '_BUILD_STANDALONE:BOOL=' + ('ON' if standalone else 'OFF'))
                products = build / f'{PRODUCT}_artefacts/Release'
                bundle = products / f'VST3/{PRODUCT}.vst3'
                module = bundle / 'Contents/Resources/moduleinfo.json'
                module.parent.mkdir(parents=True)
                module.write_text(json.dumps({'Name': PRODUCT, 'Factory Info': {'Vendor': 'RollyTech'},
                                             'Classes': [{'Category': key, 'CID': value} for key, value in CIDS.items()]}))
                if platform == 'macos':
                    bundles = [bundle, products / f'AU/{PRODUCT}.component']
                    if standalone:
                        bundles.append(products / f'Standalone/{PRODUCT}.app')
                    for path in bundles:
                        (path / 'Contents/MacOS').mkdir(parents=True)
                        (path / f'Contents/MacOS/{PRODUCT}').write_bytes(b'synthetic build fixture')
                        plist = {'CFBundleIdentifier': BASE}
                        if path.suffix == '.component':
                            plist['AudioComponents'] = [{'type': 'aufx', 'subtype': CODE, 'manufacturer': 'RTec'}]
                        (path / 'Contents/Info.plist').write_bytes(plistlib.dumps(plist))
                else:
                    binary = bundle / ('Contents/x86_64-win/' + PRODUCT + '.vst3' if platform == 'windows' else 'Contents/x86_64-linux/' + PRODUCT + '.so')
                    binary.parent.mkdir(parents=True)
                    binary.write_bytes(b'synthetic build fixture')
                    if standalone:
                        app = products / ('Standalone/' + PRODUCT + ('.exe' if platform == 'windows' else ''))
                        app.parent.mkdir(parents=True)
                        app.write_bytes(b'synthetic build fixture')
                with patch.dict(globals(), windows_version=lambda path: {'CompanyName': 'RollyTech', 'ProductName': PRODUCT}):
                    result = build_metadata(build, platform)
                    assert result['generated_base'] == BASE and result['VST3Classes'] == CIDS
                    info_path.write_text(info_bytes.replace(BASE, 'invalid.identity'))
                    rejects(build_metadata, build, platform)
                    info_path.write_text(info_bytes)
                    defs_path.write_text(definitions.replace(BASE, 'invalid.identity'))
                    rejects(build_metadata, build, platform)
                    defs_path.write_text(definitions)
                    if platform == 'macos':
                        plist = bundle / 'Contents/Info.plist'
                        original = plist.read_bytes()
                        plist.write_bytes(plistlib.dumps({'CFBundleIdentifier': 'invalid.identity'}))
                        rejects(build_metadata, build, platform)
                        plist.write_bytes(original)
                        au = products / f'AU/{PRODUCT}.component/Contents/Info.plist'
                        original = au.read_bytes()
                        broken = plistlib.loads(original)
                        broken['AudioComponents'][0]['subtype'] = 'Bad!'
                        au.write_bytes(plistlib.dumps(broken))
                        rejects(build_metadata, build, platform)
                        au.write_bytes(original)
                        au.unlink()
                        rejects(build_metadata, build, platform)
                    else:
                        binary.unlink()
                        rejects(build_metadata, build, platform)
                    module.unlink()
                    rejects(build_metadata, build, platform)
    print('product identity self-test passed (source, generated Linux/Windows/macOS, AU, VST3, packages; good/bad fixtures)')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--platform', choices=('linux', 'windows', 'macos'))
    parser.add_argument('--expanded-package', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    result = {'source': source(ROOT)}
    if args.build_dir:
        platform = args.platform or {'win32': 'windows', 'darwin': 'macos'}.get(sys.platform, 'linux')
        result['build'] = build_metadata(args.build_dir, platform)
    if args.expanded_package:
        result['package'] = package_metadata(args.expanded_package)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, IndexError, OSError, ET.ParseError) as error:
        print(f'::error::product identity: {error}', file=sys.stderr)
        sys.exit(1)
