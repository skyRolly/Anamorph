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
PACKAGES = {PRODUCT + 'VST3.pkg': BASE + '.vst3', PRODUCT + 'AU.pkg': BASE + '.au',
            PRODUCT + 'App.pkg': BASE + '.app'}
COMPONENTS = set(PACKAGES.values())


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
    for field in ('CompanyName', 'ProductName', 'FileDescription', 'FileVersion', 'ProductVersion'):
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


def receipt_owner(path, expanded, identifier):
    # pkgutil --expand preserves productbuild's named component directories.
    # A correct set of IDs does not prove that each package owns its own ID.
    require(path.parent.parent == expanded and path.parent.name in PACKAGES,
            f'unknown package component: {path.relative_to(expanded)}')
    require(identifier == PACKAGES[path.parent.name],
            f'incorrect receipt ownership: {path.parent.name} owns {identifier}')


def package_metadata(expanded):
    paths = list(expanded.rglob('PackageInfo'))
    require(len(paths) == 3, f'expected three PackageInfo files, found {len(paths)}')
    directories = list(expanded.glob('*.pkg'))
    require({path.name for path in directories} == set(PACKAGES) and all(path.is_dir() for path in directories),
            f'unknown package component directories: {[path.name for path in directories]}')
    ids, owners = [], {}
    for path in paths:
        info = ET.parse(path).getroot()
        identifier = info.attrib['identifier']
        receipt_owner(path, expanded, identifier)
        ids.append(identifier)
        owners[path.parent.name] = identifier
        bundles = info.findall('.//bundle')
        require(bundles and all(item.attrib.get('id') == BASE for item in bundles), f'package bundle namespace differs: {path}')
    require(set(ids) == COMPONENTS and len(set(ids)) == 3, f'incorrect package receipts: {ids}')
    distribution = ET.parse(expanded / 'Distribution').getroot()
    refs = [item.attrib['id'] for item in distribution.findall('.//pkg-ref')]
    require(set(refs) == COMPONENTS, f'incorrect productbuild references: {refs}')
    # productbuild may add metadata-only pkg-ref nodes. References are not
    # components: require one choice and one payload URI per unique receipt.
    choices = distribution.findall('./choice')
    suffixes = {'vst3', 'au', 'app'}
    require(len(choices) == 3 and {item.attrib['id'] for item in choices} == suffixes,
            'incorrect productbuild choices')
    outline = distribution.findall('./choices-outline/line')
    require(len(outline) == 3 and {item.attrib['choice'] for item in outline} == suffixes,
            'incorrect productbuild choice outline')
    for choice in choices:
        refs_in_choice = choice.findall('./pkg-ref')
        require(len(refs_in_choice) == 1 and refs_in_choice[0].attrib['id'] == BASE + '.' + choice.attrib['id'],
                'incorrect productbuild choice references')
    payloads = [item for item in distribution.findall('./pkg-ref') if (item.text or '').strip()]
    filenames = {identifier: name for name, identifier in PACKAGES.items()}
    require(len(payloads) == 3 and {item.attrib['id'] for item in payloads} == COMPONENTS,
            'incorrect productbuild payload references')
    for item in payloads:
        name = filenames[item.attrib['id']]
        require(item.text.strip() in (name, '#' + name), 'incorrect productbuild payload URI')
    require(all(item.attrib.get('id') == BASE for item in distribution.findall('.//bundle')),
            'incorrect productbuild bundle namespace')
    return {'package_receipts': sorted(ids), 'package_owners': dict(sorted(owners.items())),
            'distribution_refs': refs}


def self_test():
    def rejects(fn, *args, match=None):
        try:
            fn(*args)
        except (ValueError, KeyError, IndexError, OSError, ET.ParseError) as error:
            if match is not None:
                assert match in str(error), f'wrong rejection reason: {error}'
            return
        raise AssertionError('identity checker accepted seeded invalid metadata' +
                             (f' (expected {match})' if match else ''))

    def distribution_text(metadata=False):
        names = {BASE + '.vst3': PRODUCT + 'VST3.pkg', BASE + '.au': PRODUCT + 'AU.pkg',
                 BASE + '.app': PRODUCT + 'App.pkg'}
        choices = ''.join(f'<choice id="{key.rsplit(".", 1)[-1]}"><pkg-ref id="{key}"/></choice>'
                          for key in sorted(COMPONENTS))
        payloads = ''.join(f'<pkg-ref id="{key}">#{names[key]}</pkg-ref>' for key in sorted(COMPONENTS))
        extra = ''.join(f'<pkg-ref id="{key}"><bundle-version><bundle id="{BASE}"/></bundle-version></pkg-ref>'
                        for key in sorted(COMPONENTS)) if metadata else ''
        outline = '<choices-outline>' + ''.join(f'<line choice="{suffix}"/>' for suffix in ('vst3', 'au', 'app')) + '</choices-outline>'
        return '<installer-gui-script>' + outline + choices + payloads + extra + '</installer-gui-script>'

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
        # Expected names are independent of PACKAGES, so an incorrect production
        # mapping cannot make both the checker and its fixtures agree wrongly.
        components = [('VST3', 'vst3'), ('AU', 'au'), ('App', 'app')]
        for label, suffix in components:
            identifier = BASE + '.' + suffix
            path = expanded / (PRODUCT + label + '.pkg') / 'PackageInfo'
            path.parent.mkdir(parents=True)
            path.write_text(f'<pkg-info identifier="{identifier}"><bundle id="{BASE}"/></pkg-info>')
        distribution = expanded / 'Distribution'
        distribution.write_text(distribution_text())
        package_metadata(expanded)
        # Native productbuild normalization adds a third reference per component.
        distribution.write_text(distribution_text(metadata=True))
        package_metadata(expanded)
        # Keep Distribution and all bundle IDs valid while exchanging ONLY the
        # two owners. Each of the three transpositions must fail for ownership.
        for left, right in ((0, 1), (0, 2), (1, 2)):
            paths = [expanded / (PRODUCT + components[i][0] + '.pkg') / 'PackageInfo'
                     for i in (left, right)]
            originals = [path.read_bytes() for path in paths]
            trees = [ET.fromstring(data) for data in originals]
            identifiers = [tree.attrib['identifier'] for tree in trees]
            for path, tree, identifier in zip(paths, trees, reversed(identifiers)):
                tree.set('identifier', identifier)
                path.write_bytes(ET.tostring(tree))
            rejects(package_metadata, expanded, match='incorrect receipt ownership')
            for path, data in zip(paths, originals):
                path.write_bytes(data)
        component = expanded / (PRODUCT + 'VST3.pkg') / 'PackageInfo'
        original = component.read_bytes()
        tree = ET.fromstring(original)
        tree.set('identifier', BASE + '.au')
        component.write_bytes(ET.tostring(tree))
        rejects(package_metadata, expanded, match='incorrect receipt ownership')
        component.write_bytes(original)
        known = component.parent
        unknown = expanded / 'Unknown.pkg'
        known.rename(unknown)
        rejects(package_metadata, expanded, match='unknown package component')
        unknown.rename(known)
        unknown.mkdir()
        rejects(package_metadata, expanded, match='unknown package component directories')
        unknown.rmdir()
        nested = expanded / 'nested'
        nested.mkdir()
        known.rename(nested / known.name)
        rejects(package_metadata, expanded, match='unknown package component')
        (nested / known.name).rename(known)
        nested.rmdir()
        valid = distribution.read_text()
        for duplicate in (False, True):
            broken = ET.fromstring(valid)
            outline = broken.find('./choices-outline')
            if duplicate:
                outline.append(ET.fromstring(ET.tostring(outline[0])))
            else:
                outline[0].set('choice', 'unknown')
            distribution.write_bytes(ET.tostring(broken))
            rejects(package_metadata, expanded, match='incorrect productbuild choice outline')
        broken = ET.fromstring(valid)
        payload = next(item for item in broken.findall('./pkg-ref') if (item.text or '').strip())
        payload.text = '#Unknown.pkg'
        distribution.write_bytes(ET.tostring(broken))
        rejects(package_metadata, expanded, match='incorrect productbuild payload URI')
        # Even a permutation of the correct choices must not select the wrong
        # component while preserving the overall receipt-ID set.
        broken = ET.fromstring(valid)
        choice_refs = broken.findall('./choice/pkg-ref')
        first, second = [item.attrib['id'] for item in choice_refs[:2]]
        choice_refs[0].set('id', second)
        choice_refs[1].set('id', first)
        distribution.write_bytes(ET.tostring(broken))
        rejects(package_metadata, expanded, match='incorrect productbuild choice references')
        broken = ET.fromstring(valid)
        broken.remove(broken.findall('./pkg-ref')[0])
        distribution.write_bytes(ET.tostring(broken))
        rejects(package_metadata, expanded)
        broken = ET.fromstring(valid)
        broken.append(ET.fromstring(ET.tostring(broken.findall('./pkg-ref')[0])))
        distribution.write_bytes(ET.tostring(broken))
        rejects(package_metadata, expanded)
        broken = ET.fromstring(valid)
        broken.find('./pkg-ref/bundle-version/bundle').set('id', 'invalid.identity')
        distribution.write_bytes(ET.tostring(broken))
        rejects(package_metadata, expanded)
        distribution.write_text(valid)
        distribution.write_text(valid.replace(BASE, 'invalid.identity'))
        rejects(package_metadata, expanded)
        distribution.write_text(valid)
        component = expanded / (PRODUCT + 'VST3.pkg') / 'PackageInfo'
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
    # Use the real VERSIONINFO reader with a stub only at the Win32 API. The
    # pinned juce_ResourceRc.cpp emits these five keys, not InternalName or
    # OriginalFilename. Requiring invented keys rejected valid native binaries.
    from ctypes import wintypes
    version_values = {'CompanyName': 'RollyTech', 'ProductName': PRODUCT,
                      'FileDescription': PRODUCT, 'FileVersion': '1.2.3', 'ProductVersion': '1.2.3'}
    buffers = [ctypes.create_string_buffer(struct.pack('<HH', 0x409, 1252))]
    class ApiCall:
        def __init__(self, function):
            self.function = function
        def __call__(self, *args):
            return self.function(*args)
    def query_version(data, key, pointer, length):
        if key == '\\VarFileInfo\\Translation':
            buffer, count = buffers[0], 4
        else:
            field = key.rsplit('\\', 1)[-1]
            if field not in version_values:
                return 0
            buffer = ctypes.create_unicode_buffer(version_values[field])
            buffers.append(buffer)
            count = len(version_values[field]) + 1
        ctypes.cast(pointer, ctypes.POINTER(ctypes.c_void_p))[0] = ctypes.addressof(buffer)
        ctypes.cast(length, ctypes.POINTER(wintypes.UINT))[0] = count
        return 1
    class VersionApi:
        GetFileVersionInfoSizeW = ApiCall(lambda *args: 1)
        GetFileVersionInfoW = ApiCall(lambda *args: 1)
        VerQueryValueW = ApiCall(query_version)
    with patch.object(ctypes, 'WinDLL', return_value=VersionApi(), create=True):
        assert windows_version(Path('synthetic-version.exe')) == version_values
    print('product identity VERSIONINFO API-boundary fixture passed')

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
