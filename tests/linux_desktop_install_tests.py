#!/usr/bin/env python3
"""Exercise actual customer shell entry points in disposable Linux namespaces."""
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
NAME = 'tech.rolly.anamorph'
PNG = f'icons/hicolor/32x32/apps/{NAME}.png'
DESKTOP = f'applications/{NAME}.desktop'


class InstallCase(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='anamorph-desktop-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.sandbox = self.root / 'sandbox'; self.sandbox.mkdir()
        self.home_name = 'home'
        self.home = self.sandbox / self.home_name; self.home.mkdir()
        self.system_local = self.sandbox / 'system-local'; self.system_local.mkdir()
        self.system_vst = self.sandbox / 'system-vst'; self.system_vst.mkdir()
        self.package = self.root / 'package'
        self.package.mkdir()
        for file in ('install.sh', 'uninstall.sh', 'desktop-integration.sh', 'INSTALL.txt'):
            shutil.copy2(ROOT / 'packaging/linux' / file, self.package / file)
        shutil.copytree(ROOT / 'resources/icons/linux', self.package / 'desktop-assets')
        (self.package / 'Anamorph').write_text('#!/bin/sh\nprintf "%s\\n" "$0" > "$HOME/launcher-ran"\n')
        module = self.package / 'Anamorph.vst3/Contents/x86_64-linux/Anamorph.so'
        module.parent.mkdir(parents=True); module.write_bytes(b'fixture VST3 payload, not a native ELF')
        self.fault = self.root / 'fault'; self.fault.mkdir()

    @property
    def data(self):
        return self.home / '.local/share'

    def invoke(self, script='install.sh', mode='user', xdg=None, root=False, extra=(), command=None, fault=None):
        uid = '0' if root else str(os.getuid())
        gid = '0' if root else str(os.getgid())
        argv = ['bwrap', '--unshare-user', '--uid', uid, '--gid', gid, '--unshare-pid', '--die-with-parent', '--cap-drop', 'ALL',
                '--ro-bind', '/usr/bin', '/usr/bin', '--ro-bind', '/usr/share', '/usr/share',
                '--ro-bind', '/etc/alternatives', '/etc/alternatives',
                '--ro-bind', '/etc/ld.so.cache', '/etc/ld.so.cache',
                '--symlink', 'usr/bin', '/bin', '--symlink', 'usr/lib', '/lib',
                '--ro-bind', '/lib64', '/lib64', '--proc', '/proc', '--dev', '/dev', '--tmpfs', '/tmp',
                '--bind', str(self.sandbox), '/sandbox', '--ro-bind', str(self.package), '/package',
                '--ro-bind', str(self.fault), '/fault-bin', '--chdir', '/tmp',
                '--setenv', 'HOME', '/sandbox/' + self.home_name, '--unsetenv', 'XDG_DATA_HOME',
                '--setenv', 'PATH', '/fault-bin:/usr/bin:/bin' if fault else '/usr/bin:/bin']
        # Build a private /usr/lib parent, retaining runtime modules read-only;
        # only the product VST3 directory and /usr/local are disposable writable mounts.
        for item in Path('/usr/lib').iterdir():
            if item.name != 'vst3':
                argv += ['--ro-bind', str(item), str(item)]
        argv += ['--bind', str(self.system_vst), '/usr/lib/vst3', '--bind', str(self.system_local), '/usr/local']
        if xdg is not None:
            argv += ['--setenv', 'XDG_DATA_HOME', xdg]
        if fault:
            argv += ['--setenv', 'ANAMORPH_FAULT', fault]
        argv += list(command) if command else ['/bin/sh', '/package/' + script, '--' + mode, *extra]
        return subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=35)

    def good(self, **options):
        result = self.invoke(**options)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def bad(self, message=None, **options):
        result = self.invoke(**options)
        self.assertNotEqual(result.returncode, 0, 'unsafe operation succeeded: ' + result.stdout)
        if message:
            self.assertIn(message, result.stdout + result.stderr)
        return result

    def icons_match(self, data):
        for source in (self.package / 'desktop-assets/hicolor').rglob('*'):
            if source.is_file():
                self.assertEqual(source.read_bytes(), (data / 'icons/hicolor' / source.relative_to(self.package / 'desktop-assets/hicolor')).read_bytes())

    def test_fresh_user_reinstall_uninstall(self):
        self.good(); self.icons_match(self.data)
        desktop = self.data / DESKTOP
        self.assertIn('Exec=/usr/bin/env -- "/sandbox/home/.local/bin/Anamorph"', desktop.read_text())
        self.assertEqual(desktop.stat().st_mode & 0o777, 0o644)
        self.assertEqual((self.home / '.local/bin/Anamorph').stat().st_mode & 0o777, 0o755)
        before = desktop.read_bytes(); self.good(); self.assertEqual(before, desktop.read_bytes())
        # Existing binary update behavior remains exercised through the real scripts.
        (self.package / 'Anamorph').write_text('#!/bin/sh\nexit 0\n')
        self.good(); self.assertEqual((self.package / 'Anamorph').read_bytes(), (self.home / '.local/bin/Anamorph').read_bytes())
        foreign = self.data / 'icons/hicolor/32x32/apps/Other.png'; foreign.write_bytes(b'other product')
        presets = self.home / '.config/RollyTech/Anamorph/Presets/user.anamorph'
        presets.parent.mkdir(parents=True); presets.write_bytes(b'user preset')
        self.good(script='uninstall.sh'); self.good(script='uninstall.sh')
        self.assertFalse(desktop.exists()); self.assertFalse((self.data / PNG).exists())
        self.assertFalse((self.home / '.local/bin/Anamorph').exists()); self.assertFalse((self.home / '.vst3/Anamorph.vst3').exists())
        self.assertEqual(foreign.read_bytes(), b'other product'); self.assertEqual(presets.read_bytes(), b'user preset')

    def test_system_and_user_coexistence(self):
        self.good(); user = (self.data / DESKTOP).read_bytes()
        self.good(mode='system', root=True, xdg='/sandbox/never-use-root-xdg')
        self.good(mode='system', root=True, xdg='relative-root-data')
        system_data = self.system_local / 'share'
        self.icons_match(system_data)
        self.assertIn('Exec=/usr/bin/env -- "/usr/local/bin/Anamorph"', (system_data / DESKTOP).read_text())
        self.assertFalse((self.sandbox / 'never-use-root-xdg').exists())
        self.good(script='uninstall.sh', mode='system', root=True)
        self.assertEqual((self.data / DESKTOP).read_bytes(), user)
        self.assertFalse((system_data / DESKTOP).exists())
        self.good(mode='system', root=True); self.good(script='uninstall.sh')
        self.assertTrue((system_data / DESKTOP).is_file()); self.assertTrue((self.system_local / 'bin/Anamorph').is_file())
        self.good(script='uninstall.sh', mode='system', root=True)

    def test_xdg_absolute_relative_and_root_user(self):
        self.good(xdg='/sandbox/custom data/')
        self.assertTrue((self.sandbox / 'custom data' / DESKTOP).is_file()); self.assertFalse(self.data.exists())
        self.good(script='uninstall.sh', xdg='/sandbox/custom data/')
        result = self.good(xdg='relative/data')
        self.assertIn('relative XDG_DATA_HOME ignored', result.stderr); self.assertTrue((self.data / DESKTOP).is_file())
        self.bad(message='--user cannot', root=True)
        self.assertFalse((self.sandbox / 'custom data' / DESKTOP).exists())

    def test_missing_or_modified_source_fails_before_binary_writes(self):
        image = self.package / 'desktop-assets/hicolor/32x32/apps' / (NAME + '.png')
        image.unlink(); self.bad(message='missing/linked source asset')
        self.assertFalse((self.home / '.local/bin/Anamorph').exists())
        shutil.copy2(ROOT / 'resources/icons/linux/hicolor/32x32/apps' / (NAME + '.png'), image)
        image.write_bytes(image.read_bytes() + b'corrupt'); self.bad(message='source asset hash mismatch')
        self.assertFalse((self.home / '.vst3').exists())

    def foreign_probe(self):
        desktop = self.data / DESKTOP; desktop.parent.mkdir(parents=True); desktop.write_bytes(b'unrelated user launcher')
        self.bad(message='foreign resource')
        self.assertEqual(desktop.read_bytes(), b'unrelated user launcher')
        self.assertFalse((self.home / '.local/bin/Anamorph').exists())

    def test_foreign_destination_preserved(self):
        self.foreign_probe()

    def test_modified_owned_resource_preserved(self):
        self.good(); (self.data / PNG).write_bytes(b'user modification')
        self.bad(message='modified/foreign resource', script='uninstall.sh')
        self.assertTrue((self.home / '.local/bin/Anamorph').is_file())
        self.assertEqual((self.data / PNG).read_bytes(), b'user modification')
        self.bad(message='modified/foreign resource')

    def test_destination_and_parent_symlinks(self):
        self.good()
        icon = self.data / PNG; keep = self.home / 'keep.png'; keep.write_bytes(icon.read_bytes())
        icon.unlink(); icon.symlink_to(keep)
        self.bad(message='regular owned file'); self.bad(message='regular owned file', script='uninstall.sh')
        self.assertTrue(icon.is_symlink()); self.assertEqual(keep.read_bytes(), (self.package / 'desktop-assets/hicolor/32x32/apps' / (NAME + '.png')).read_bytes())

    def parent_link_probe(self):
        self.data.mkdir(parents=True)
        outside = self.home / 'outside'; outside.mkdir()
        (self.data / 'icons').symlink_to('/sandbox/home/outside')
        self.bad(message='symlink in destination ancestry')
        self.assertEqual(list(outside.iterdir()), [])

    def test_parent_symlink_preserved(self):
        self.parent_link_probe()

    def test_source_ancestry_symlink_and_private_state_guards(self):
        source = self.package / 'desktop-assets/hicolor/32x32/apps'
        source.rename(source.with_name('original-apps'))
        source.symlink_to('original-apps')
        self.bad(message='linked source asset ancestry')
        source.unlink(); source.with_name('original-apps').rename(source)
        self.good()
        state = self.data / '.tech.rolly.anamorph'
        state.chmod(0o777)
        self.bad(message='private directory ownership/mode')
        state.chmod(0o700)
        receipt = state / 'receipt'
        receipt.write_text(receipt.read_text().replace(DESKTOP, '../unrelated.keep'))
        self.bad(message='ownership receipt path set differs', script='uninstall.sh')
        self.assertTrue((self.home / '.local/bin/Anamorph').is_file())

    def test_unwritable_xdg_fails_before_binary_writes(self):
        data = self.sandbox / 'readonly'; data.mkdir(); data.chmod(0o555)
        self.bad(xdg='/sandbox/readonly', message='unwritable')
        self.assertFalse((self.home / '.local/bin/Anamorph').exists())

    def make_fault(self):
        wrapper = self.fault / 'mv'
        wrapper.write_text('''#!/bin/sh
for last do :; done
case "$last" in
 */icons/hicolor/32x32/apps/tech.rolly.anamorph.png)
  if [ ! -e /sandbox/fault-fired ]; then
   : > /sandbox/fault-fired
   if [ "$ANAMORPH_FAULT" = kill ]; then kill -KILL "$PPID"; fi
   exit 73
  fi ;;
esac
exec /usr/bin/mv "$@"
'''); wrapper.chmod(0o755)

    def test_mid_install_failure_and_killed_transaction_recovery(self):
        xdg = '/sandbox/shared-data'
        data_root = self.sandbox / 'shared-data'
        self.good(xdg=xdg)
        old = {p.relative_to(data_root): p.read_bytes() for p in data_root.rglob('*') if p.is_file() and p.suffix in ('.png','.svg','.desktop')}
        # Reusing one explicit XDG root from another HOME generates a different
        # valid launcher. Old and new bytes must discriminate for rollback proof.
        self.home_name = 'home-next'; self.home = self.sandbox / self.home_name; self.home.mkdir()
        self.make_fault()
        self.bad(fault='fail', xdg=xdg)
        for path, data in old.items():
            self.assertEqual((data_root / path).read_bytes(), data)
        self.assertFalse((data_root / '.tech.rolly.anamorph/transaction').exists())
        (self.sandbox / 'fault-fired').unlink()
        self.bad(fault='kill', xdg=xdg)
        self.assertTrue((data_root / '.tech.rolly.anamorph/transaction/ready').is_file())
        self.assertNotEqual((data_root / DESKTOP).read_bytes(), old[Path(DESKTOP)])
        result = self.good(xdg=xdg)
        self.assertIn('Recovered previous', result.stdout)
        self.assertFalse((data_root / '.tech.rolly.anamorph/transaction').exists())
        self.assertIn('/sandbox/home-next/.local/bin/Anamorph', (data_root / DESKTOP).read_text())
        self.good(script='uninstall.sh', xdg=xdg)

    def test_first_install_killed_then_uninstall(self):
        self.make_fault(); self.bad(fault='kill')
        self.good(script='uninstall.sh')
        self.assertFalse((self.data / DESKTOP).exists()); self.assertFalse((self.data / PNG).exists())

    def test_system_killed_transaction_recovery(self):
        self.make_fault(); self.bad(fault='kill', mode='system', root=True)
        state = self.system_local / 'share/.tech.rolly.anamorph'
        self.assertTrue((state / 'transaction/ready').is_file())
        self.good(mode='system', root=True)
        self.icons_match(self.system_local / 'share')
        self.assertFalse((state / 'transaction').exists())
        self.good(script='uninstall.sh', mode='system', root=True)
        self.assertFalse((self.system_local / 'share' / DESKTOP).exists())

    def test_unknown_journal_file_is_preserved(self):
        self.make_fault(); self.bad(fault='kill')
        foreign = self.data / '.tech.rolly.anamorph/transaction/unrelated.keep'
        foreign.write_bytes(b'foreign transaction file')
        self.bad(message='unknown transaction entries preserved')
        self.assertEqual(foreign.read_bytes(), b'foreign transaction file')
        self.assertTrue((self.data / '.tech.rolly.anamorph/transaction/ready').is_file())

    def test_foreign_change_blocks_interrupted_recovery(self):
        self.good(); self.make_fault(); self.bad(fault='kill')
        desktop = self.data / DESKTOP; desktop.write_bytes(b'foreign change after interruption')
        self.bad(message='foreign change blocks resource recovery')
        self.assertEqual(desktop.read_bytes(), b'foreign change after interruption')

    def test_gio_exec_escaping_and_hicolor_lookup(self):
        self.home_name = 'a space%f "quote" $dollar `tick` \\slash'
        self.home.rename(self.sandbox / self.home_name); self.home = self.sandbox / self.home_name
        self.good()
        self.good(command=['desktop-file-validate', '/sandbox/' + self.home_name + '/.local/share/' + DESKTOP])
        script = self.package / 'lookup.py'
        script.write_text('''import os, time, gi
gi.require_version('Gtk','3.0')
from gi.repository import Gio, Gtk
data=os.path.join(os.environ['HOME'],'.local/share')
app=Gio.DesktopAppInfo.new_from_filename(os.path.join(data,'applications/tech.rolly.anamorph.desktop'))
assert app is not None
assert app.launch([],None)
marker=os.path.join(os.environ['HOME'],'launcher-ran')
for _ in range(100):
 if os.path.exists(marker):break
 time.sleep(0.02)
assert open(marker).read().strip()==os.path.join(os.environ['HOME'],'.local/bin/Anamorph')
theme=Gtk.IconTheme.new()
theme.set_search_path([os.path.join(data,'icons'),'/usr/share/icons'])
theme.set_custom_theme('hicolor')
for size in [16,24,32,48,64,128,256,512]:
 for scale in [1,2]:
  info=theme.lookup_icon_for_scale('tech.rolly.anamorph',size,scale,0)
  assert info is not None,(size,scale)
  assert info.get_filename().startswith(data+'/icons/hicolor/'),info.get_filename()
  assert info.load_icon() is not None
print('GIO launched exact escaped executable; hicolor raster/SVG lookup 8 sizes x 2 scales')
''')
        self.good(command=['/usr/bin/python3', '/package/lookup.py'])
        self.assertFalse((self.sandbox / 'a space').exists())

    def test_clean_archive_and_required_assets(self):
        zipped = self.root / 'customer.zip'
        source = STAGED or self.package
        with zipfile.ZipFile(zipped,'w') as archive:
            for p in source.rglob('*'):
                if p.is_file(): archive.write(p, p.relative_to(source))
        extracted = self.root / 'extracted'; extracted.mkdir()
        with zipfile.ZipFile(zipped) as archive: archive.extractall(extracted)
        self.assertFalse(self.data.exists()); self.assertFalse((self.home / '.local/bin').exists())
        self.package = extracted
        self.good(); self.icons_match(self.data)
        self.assertEqual((self.home / '.local/bin/Anamorph').read_bytes(), (source / 'Anamorph').read_bytes())
        self.assertEqual((self.home / '.vst3/Anamorph.vst3/Contents/x86_64-linux/Anamorph.so').read_bytes(), (source / 'Anamorph.vst3/Contents/x86_64-linux/Anamorph.so').read_bytes())
        if STAGED:
            self.assertEqual((self.home / '.local/bin/Anamorph').read_bytes()[:4], b'\x7fELF')
            self.assertEqual((self.home / '.vst3/Anamorph.vst3/Contents/x86_64-linux/Anamorph.so').read_bytes()[:4], b'\x7fELF')
        self.good(script='uninstall.sh')

    def test_safety_gate_mutation_liveness(self):
        helper = self.package / 'desktop-integration.sh'
        original = helper.read_text()
        # The same negative probe must fail when the production ownership gate
        # is disabled. This actually invokes install.sh, including publication.
        mutated = original.replace('check_destinations() {', 'check_destinations() { return 0;\n')
        self.assertNotEqual(original, mutated); helper.write_text(mutated)
        with self.assertRaisesRegex(AssertionError, 'unsafe operation succeeded'):
            self.foreign_probe()
        self.assertNotEqual((self.data / DESKTOP).read_bytes(), b'unrelated user launcher')

    def test_parent_gate_mutation_liveness(self):
        helper = self.package / 'desktop-integration.sh'
        original = helper.read_text()
        mutated = original.replace('safe_parent() {', 'safe_parent() { return 0;\n')
        self.assertNotEqual(original, mutated); helper.write_text(mutated)
        with self.assertRaisesRegex(AssertionError, 'unsafe operation succeeded'):
            self.parent_link_probe()
        self.assertTrue(any((self.home / 'outside').rglob(NAME + '.png')))


def main():
    global STAGED
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged-dir', type=Path)
    parser.add_argument('--report', type=Path)
    args, extra = parser.parse_known_args()
    STAGED = args.staged_dir.resolve() if args.staged_dir else None
    if not shutil.which('bwrap'):
        raise SystemExit('Bubblewrap required: tests must never run installation on live host paths')
    suite = unittest.defaultTestLoader.loadTestsFromNames(extra, module=sys.modules[__name__]) if extra else unittest.defaultTestLoader.loadTestsFromTestCase(InstallCase)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps({'tests_run': result.testsRun,'failures': len(result.failures),
            'errors': len(result.errors),'real_customer_tree': str(STAGED) if STAGED else None,
            'GUI_desktop_certification': False,'isolation': 'actual shell entry points under Bubblewrap private filesystem roots'},indent=2)+'\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()
