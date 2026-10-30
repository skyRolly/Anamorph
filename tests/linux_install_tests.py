#!/usr/bin/env python3
"""Actual VST3 installer entry points in disposable Linux filesystem roots."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parent.parent
STAGED = None
LEGACY_ARCHIVE = None
NAME = 'tech.rolly.anamorph'
DESKTOP = f'applications/{NAME}.desktop'
PNG = f'icons/hicolor/16x16/apps/{NAME}.png'
MANAGED = (DESKTOP, *(f'icons/hicolor/{size}x{size}/apps/{NAME}.png'
                      for size in (16, 24, 32, 48, 64, 128, 256, 512)),
           f'icons/hicolor/scalable/apps/{NAME}.svg')
MODULE = 'Contents/x86_64-linux/Anamorph.so'
REQUIRED_METHODS = (
    'test_fresh_user_reinstall_and_repeated_uninstall',
    'test_system_user_coexistence_and_root_selection',
    'test_vst3_install_without_standalone_payload',
    'test_legacy_upgrade_without_permission_repair',
    'test_legacy_uninstall_without_permission_repair',
    'test_unverified_legacy_resources_are_preserved',
    'test_interrupted_legacy_cleanup_retries',
    'test_plugin_swap_failure_and_kill_recover',
    'test_module_permission_failure_preserves_previous_plugin',
    'test_parked_and_unknown_scratch_preservation',
    'test_symlink_destinations_preserve_unrelated_files',
    'test_xdg_roots_and_missing_sources',
    'test_clean_archive_roundtrip',
    'test_permission_and_cleanup_guards_are_live',
)


class InstallCase(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='anamorph-install-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.sandbox = self.root / 'sandbox'; self.sandbox.mkdir()
        self.home = self.sandbox / 'home'; self.home.mkdir()
        self.system_local = self.sandbox / 'system-local'; self.system_local.mkdir()
        self.system_vst = self.sandbox / 'system-vst'; self.system_vst.mkdir()
        self.package = self.root / 'package'
        if STAGED:
            shutil.copytree(STAGED, self.package)
        else:
            self.package.mkdir()
            for name in ('install.sh', 'uninstall.sh', 'INSTALL.txt'):
                shutil.copy2(ROOT / 'packaging/linux' / name, self.package / name)
            (self.package / 'Anamorph').write_bytes(b'\x7fELF portable fixture; never executed')
            (self.package / 'Anamorph').chmod(0o755)
            module = self.package / 'Anamorph.vst3' / MODULE
            module.parent.mkdir(parents=True); module.write_bytes(b'\x7fELF VST3 fixture; never executed')
            module.chmod(0o755)
        self.fault = self.root / 'fault'; self.fault.mkdir()
        self.control = self.sandbox / 'control'; self.control.mkdir()

    @property
    def data(self): return self.home / '.local/share'

    @property
    def plugin(self): return self.home / '.vst3/Anamorph.vst3'

    def invoke(self, script='install.sh', mode='user', root=False, xdg=None,
               fault=None, command=None, extra=()):
        uid = '0' if root else str(os.getuid())
        gid = '0' if root else str(os.getgid())
        argv = ['bwrap', '--unshare-user', '--uid', uid, '--gid', gid,
                '--unshare-pid', '--die-with-parent', '--cap-drop', 'ALL',
                '--ro-bind', '/usr/bin', '/usr/bin', '--ro-bind', '/usr/share', '/usr/share',
                '--ro-bind', '/etc', '/etc',
                '--symlink', 'usr/bin', '/bin', '--symlink', 'usr/lib', '/lib',
                '--ro-bind', '/lib64', '/lib64', '--proc', '/proc', '--dev', '/dev',
                '--tmpfs', '/tmp', '--bind', str(self.sandbox), '/sandbox',
                '--ro-bind', str(self.package), '/package', '--ro-bind', str(self.fault), '/fault-bin',
                '--bind', str(self.system_local), '/usr/local',
                '--bind', str(self.system_vst), '/usr/lib/vst3', '--chdir', '/tmp',
                '--setenv', 'HOME', '/sandbox/home', '--unsetenv', 'XDG_DATA_HOME',
                '--setenv', 'PATH', '/fault-bin:/usr/bin:/bin' if fault else '/usr/bin:/bin']
        for path in Path('/usr/lib').iterdir():
            if path.name != 'vst3': argv += ['--ro-bind', str(path), str(path)]
        if Path('/usr/libexec').exists(): argv += ['--ro-bind', '/usr/libexec', '/usr/libexec']
        if xdg is not None: argv += ['--setenv', 'XDG_DATA_HOME', xdg]
        if fault: argv += ['--setenv', 'ANAMORPH_FAULT', fault]
        argv += command or ['/bin/sh', '/package/' + script, '--' + mode, *extra]
        return subprocess.run(argv, capture_output=True, text=True, timeout=40)

    def good(self, **options):
        result = self.invoke(**options)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def bad(self, message=None, **options):
        result = self.invoke(**options)
        self.assertNotEqual(result.returncode, 0, 'unsafe operation succeeded')
        if message: self.assertIn(message, result.stdout + result.stderr)
        return result

    def seed_plugin(self, system=False):
        plugin = self.system_vst / 'Anamorph.vst3' if system else self.plugin
        module = plugin / MODULE; module.parent.mkdir(parents=True)
        module.write_bytes(b'previous plug-in'); return module

    def seed_legacy(self, data=None):
        data = data or self.data
        state = data / '.tech.rolly.anamorph'; state.mkdir(parents=True); state.chmod(0o700)
        (state / 'format-v1').mkdir(mode=0o700)
        receipt = ['tech.rolly.anamorph desktop resources v1']
        for relative in MANAGED:
            target = data / relative; target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(('old owned resource: ' + relative).encode())
            receipt.append(hashlib.sha256(target.read_bytes()).hexdigest() + '  ' + relative)
        (state / 'receipt').write_text('\n'.join(receipt) + '\n')
        return state

    def assert_installed(self, system=False):
        plugin = self.system_vst / 'Anamorph.vst3' if system else self.plugin
        module = plugin / MODULE
        self.assertEqual(module.read_bytes(), (self.package / 'Anamorph.vst3' / MODULE).read_bytes())
        self.assertEqual(module.stat().st_mode & 0o777, 0o755)
        app = self.system_local / 'bin/Anamorph' if system else self.home / '.local/bin/Anamorph'
        self.assertFalse(app.exists())

    def test_fresh_user_reinstall_and_repeated_uninstall(self):
        parent = self.data / Path(PNG).parent; parent.mkdir(parents=True); parent.chmod(0o775)
        unrelated = parent / 'Other.png'; unrelated.write_bytes(b'keep icon')
        settings = self.home / '.config/Anamorph/settings'; settings.parent.mkdir(parents=True)
        settings.write_bytes(b'keep settings')
        self.good(); self.assert_installed(); self.good(); self.assert_installed()
        self.assertFalse((self.data / DESKTOP).exists())
        self.good(script='uninstall.sh'); self.good(script='uninstall.sh')
        self.assertFalse(self.plugin.exists())
        self.assertEqual(parent.stat().st_mode & 0o777, 0o775)
        self.assertEqual(unrelated.read_bytes(), b'keep icon')
        self.assertEqual(settings.read_bytes(), b'keep settings')

    def test_system_user_coexistence_and_root_selection(self):
        self.good(); self.good(mode='system', root=True); self.assert_installed(system=True)
        self.good(mode='system', root=True)
        self.good(script='uninstall.sh', mode='system', root=True)
        self.good(script='uninstall.sh', mode='system', root=True)
        self.assert_installed()
        self.bad(root=True, message='--user cannot')
        self.bad(root=True, script='uninstall.sh', message='--user cannot')
        self.bad(extra=('--system',), message='different installs')
        self.good(script='uninstall.sh')

    def test_vst3_install_without_standalone_payload(self):
        (self.package / 'Anamorph').unlink()
        self.good(); self.assert_installed(); self.good(script='uninstall.sh')

    def test_legacy_upgrade_without_permission_repair(self):
        previous = self.seed_plugin(); self.seed_legacy()
        parent = (self.data / PNG).parent; parent.chmod(0o775)
        unrelated = parent / 'Other.png'; unrelated.write_bytes(b'keep')
        legacy_app = self.home / '.local/bin/Anamorph'; legacy_app.parent.mkdir(parents=True)
        if LEGACY_ARCHIVE:
            with zipfile.ZipFile(LEGACY_ARCHIVE) as archive: legacy_app.write_bytes(archive.read('Anamorph'))
        else: legacy_app.write_bytes(b'unverified standalone')
        self.good()
        self.assertEqual(previous.read_bytes(), (self.package / 'Anamorph.vst3' / MODULE).read_bytes())
        for relative in MANAGED: self.assertFalse((self.data / relative).exists())
        self.assertFalse((self.data / '.tech.rolly.anamorph').exists())
        self.assertEqual(parent.stat().st_mode & 0o777, 0o775)
        self.assertEqual(unrelated.read_bytes(), b'keep')
        self.assertEqual(legacy_app.exists(), not bool(LEGACY_ARCHIVE))
        self.good(script='uninstall.sh')

    def test_legacy_uninstall_without_permission_repair(self):
        self.seed_plugin(); self.seed_legacy()
        parent = (self.data / PNG).parent; parent.chmod(0o777)
        self.good(script='uninstall.sh'); self.good(script='uninstall.sh')
        self.assertFalse(self.plugin.exists())
        self.assertFalse((self.data / '.tech.rolly.anamorph').exists())
        for relative in MANAGED: self.assertFalse((self.data / relative).exists())
        self.assertEqual(parent.stat().st_mode & 0o777, 0o777)

    def test_unverified_legacy_resources_are_preserved(self):
        for kind in ('changed', 'no-receipt', 'malformed', 'symlink', 'hardlink', 'journal'):
            with self.subTest(kind=kind):
                case = InstallCase(); case.setUp(); self.addCleanup(case.doCleanups)
                case.seed_plugin(); state = case.seed_legacy(); resource = case.data / PNG
                if kind == 'changed': resource.write_bytes(b'unrelated edited icon')
                elif kind == 'no-receipt': (state / 'receipt').unlink()
                elif kind == 'malformed': (state / 'receipt').write_text('unrecognized receipt\n')
                elif kind == 'symlink':
                    resource.unlink(); resource.symlink_to('/sandbox/home/keep')
                elif kind == 'hardlink': os.link(resource, case.home / 'hardlink.keep')
                else: (state / 'transaction').mkdir(); (state / 'transaction/foreign.keep').write_bytes(b'keep')
                keep = case.home / 'keep'; keep.write_bytes(b'keep')
                result = case.good(script='uninstall.sh')
                self.assertIn('warning: legacy Anamorph resources preserved', result.stderr)
                self.assertFalse(case.plugin.exists()); self.assertTrue(state.exists())
                self.assertEqual(keep.read_bytes(), b'keep')
                if kind != 'journal': self.assertTrue(resource.exists() or resource.is_symlink())
                else: self.assertEqual((state / 'transaction/foreign.keep').read_bytes(), b'keep')

    def fault_tool(self, tool, body):
        path = self.fault / tool
        path.write_text('#!/bin/sh\n' + body + '\nexec /usr/bin/' + tool + ' "$@"\n')
        path.chmod(0o755)

    def test_interrupted_legacy_cleanup_retries(self):
        self.seed_legacy(); self.seed_plugin()
        self.fault_tool('rm', '''case "$*" in *hicolor/32x32/apps/tech.rolly.anamorph.png*)
 if [ ! -e /sandbox/control/cut ]; then
  : > /sandbox/control/cut
  exit 1
 fi;; esac''')
        result = self.good(script='uninstall.sh', fault='fail')
        self.assertIn('warning:', result.stderr)
        self.assertTrue((self.control / 'cut').exists())
        self.assertTrue((self.data / '.tech.rolly.anamorph/receipt').exists())
        self.good(script='uninstall.sh')
        self.assertFalse((self.data / '.tech.rolly.anamorph').exists())

    def test_plugin_swap_failure_and_kill_recover(self):
        for action in ('fail', 'kill', 'TERM', 'HUP', 'INT'):
            with self.subTest(action=action):
                case = InstallCase(); case.setUp(); self.addCleanup(case.doCleanups)
                previous = case.seed_plugin()
                case.fault_tool('mv', '''case "$1" in */.anamorph-install-stage/Anamorph.vst3)
 if [ ! -e /sandbox/control/cut ]; then
  : > /sandbox/control/cut
  case "$ANAMORPH_FAULT" in
   kill) kill -KILL "$PPID" ;;
   fail) ;;
   *) kill -"$ANAMORPH_FAULT" "$PPID" ;;
  esac
  exit 1
 fi;; esac''')
                case.bad(fault=action)
                self.assertTrue((case.control / 'cut').exists())
                if action != 'kill': self.assertEqual(previous.read_bytes(), b'previous plug-in')
                case.good(); case.assert_installed(); case.good(script='uninstall.sh')

    def test_module_permission_failure_preserves_previous_plugin(self):
        previous = self.seed_plugin()
        self.fault_tool('chmod', 'case "$*" in *Anamorph.so*) exit 1 ;; esac')
        self.bad(fault='fail')
        self.assertEqual(previous.read_bytes(), b'previous plug-in')
        self.good(); self.assert_installed(); self.good(script='uninstall.sh')

    def test_parked_and_unknown_scratch_preservation(self):
        stage = self.home / '.anamorph-install-stage'; stage.mkdir(mode=0o700)
        stage.chmod(0o2700)
        saved = stage / 'Anamorph.vst3.prev' / MODULE
        saved.parent.mkdir(parents=True); saved.write_bytes(b'previous')
        self.good(script='uninstall.sh'); self.assertEqual(saved.read_bytes(), b'previous')
        self.good(script='uninstall.sh', extra=('--discard-parked',)); self.assertFalse(stage.exists())
        stage.mkdir(mode=0o700); (stage / 'foreign.keep').write_bytes(b'keep')
        self.good(script='uninstall.sh', extra=('--discard-parked',))
        self.assertEqual((stage / 'foreign.keep').read_bytes(), b'keep')

    def test_symlink_destinations_preserve_unrelated_files(self):
        outside = self.home / 'keep'; outside.mkdir(); sentinel = outside / 'foreign.keep'
        sentinel.write_bytes(b'keep')
        self.plugin.parent.symlink_to('/sandbox/home/keep')
        self.bad(message='unsafe plug-in destination'); self.bad(script='uninstall.sh')
        self.plugin.parent.unlink(); self.plugin.parent.mkdir()
        self.plugin.symlink_to('/sandbox/home/keep')
        self.bad(message='unsafe plug-in destination'); self.bad(script='uninstall.sh')
        self.assertEqual(sentinel.read_bytes(), b'keep')

    def test_xdg_roots_and_missing_sources(self):
        data = self.sandbox / 'custom data'; self.seed_legacy(data)
        self.good(xdg='/sandbox/custom data/')
        for relative in MANAGED: self.assertFalse((data / relative).exists())
        self.assertFalse(self.data.exists()); self.good(script='uninstall.sh', xdg='relative')
        shutil.rmtree(self.package / 'Anamorph.vst3')
        self.bad(message='Anamorph.vst3 not found')

    def test_clean_archive_roundtrip(self):
        source = self.package; archive_path = self.root / 'customer.zip'
        with zipfile.ZipFile(archive_path, 'w') as archive:
            for path in source.rglob('*'):
                if path.is_file(): archive.write(path, path.relative_to(source))
        self.package = self.root / 'extracted'
        with zipfile.ZipFile(archive_path) as archive: archive.extractall(self.package)
        self.good(); self.assert_installed(); self.good(script='uninstall.sh')

    def test_permission_and_cleanup_guards_are_live(self):
        # Execute a private mutant: removing the hash comparison must expose
        # the exact edited-resource preservation assertion, not merely status.
        self.seed_legacy(); resource = self.data / PNG; resource.write_bytes(b'foreign bytes')
        for name in ('install.sh', 'uninstall.sh'):
            path = self.package / name; original = path.read_text()
            mutated = original.replace('[ "$(hash "$target")" = "$digest" ]', 'true')
            self.assertNotEqual(original, mutated); path.write_text(mutated)
        self.good(script='uninstall.sh')
        with self.assertRaises(AssertionError): self.assertEqual(resource.read_bytes() if resource.exists() else None, b'foreign bytes')


def main():
    global STAGED, LEGACY_ARCHIVE
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged-dir', type=Path)
    parser.add_argument('--legacy-archive', type=Path)
    parser.add_argument('--report', type=Path)
    args, names = parser.parse_known_args()
    if not shutil.which('bwrap'): parser.error('Bubblewrap is required; no live-path fallback is permitted')
    STAGED = args.staged_dir.resolve() if args.staged_dir else None
    LEGACY_ARCHIVE = args.legacy_archive
    suite = (unittest.defaultTestLoader.loadTestsFromNames(names, sys.modules[__name__]) if names else
             unittest.defaultTestLoader.loadTestsFromTestCase(InstallCase))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps({'tests_run': result.testsRun, 'failures': len(result.failures),
            'errors': len(result.errors), 'native_real_uid_gate': False,
            'isolation': 'Bubblewrap private user/mount namespaces',
            'official_legacy_archive_tested': bool(LEGACY_ARCHIVE)}, indent=2) + '\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__': main()
