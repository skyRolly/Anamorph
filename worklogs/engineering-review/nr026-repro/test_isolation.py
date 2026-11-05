#!/usr/bin/env python3
"""Exercise the real Linux path witness and disposable-profile failure controls."""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

from isolation import IsolationError, Profile, install_signal_handlers, run_child


def wait_for(paths, seconds=20):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if all(path.exists() for path in paths):
            return
        time.sleep(0.02)
    raise AssertionError(f'timed out waiting for {paths}')


def process_is_running(pid):
    try:
        status = (Path('/proc') / str(pid) / 'stat').read_text()
    except FileNotFoundError:
        return False
    return status.rsplit(')', 1)[1].split()[0] != 'Z'


def concurrent_child(probe, shared, name):
    with Profile() as profile:
        profile.validate(probe)
        own = profile.preset / 'same-name.anamorph'
        own.write_text(name)
        (shared / ('ready-' + name)).write_text(json.dumps({'root': str(profile.root), 'preset': str(own)}))
        wait_for([shared / 'release'])
        if own.read_text() != name:
            raise AssertionError('concurrent profile data crossed')
    return 0


def interrupt_child(probe, shared):
    install_signal_handlers()
    try:
        with Profile() as profile:
            profile.validate(probe)
            (shared / 'interrupt-ready').write_text(str(profile.root))
            code = '''import os, subprocess, sys, time
grandchild = 'import os,sys,time;open(sys.argv[1],"w").write(str(os.getpid()));time.sleep(120)'
subprocess.Popen([sys.executable, '-c', grandchild, sys.argv[2]])
open(sys.argv[1], 'w').write(str(os.getpid()))
print('partial evidence', flush=True)
time.sleep(120)
'''
            with (shared / 'partial.stdout').open('wb') as out, (shared / 'partial.stderr').open('wb') as err:
                run_child([sys.executable, '-u', '-c', code, str(shared / 'child-pid'), str(shared / 'grandchild-pid')],
                          cwd=shared, env=profile.env, stdout=out, stderr=err)
    except InterruptedError:
        return 73
    return 1


class IsolationTests(unittest.TestCase):
    probe = None

    def test_legacy_xdg_only_profile_still_resolves_caller_home(self):
        # This read-only control runs the old environment shape against a fake
        # caller home. It must expose the very path the new wrapper refuses.
        with tempfile.TemporaryDirectory(prefix='nr026-legacy-', dir='/tmp') as raw:
            fake_home = Path(raw) / 'caller-home'
            config = fake_home / '.config'
            presets = config / 'RollyTech/Anamorph/Presets'
            presets.mkdir(parents=True)
            sentinel = presets / '__AnamorphStateHarness__.anamorph'
            sentinel.write_bytes(b'caller preset must survive\n')
            alternate = Path(raw) / 'xdg-only-profile'
            alternate.mkdir()
            env = os.environ.copy()
            env.update(HOME=str(fake_home), XDG_CONFIG_HOME=str(alternate))
            result = subprocess.run([str(self.probe)], env=env, text=True, capture_output=True, check=True)
            paths = result.stdout.splitlines()
            self.assertEqual(paths[1], str(config))
            self.assertEqual(paths[2], str(presets))
            self.assertNotEqual(paths[1], str(alternate))
            self.assertEqual(sentinel.read_bytes(), b'caller preset must survive\n')

    def test_real_paths_and_preset_round_trip_do_not_touch_caller_home(self):
        with tempfile.TemporaryDirectory(prefix='nr026-sentinel-', dir='/tmp') as raw:
            fake_home = Path(raw) / 'caller-home'
            real_preset = fake_home / '.config/RollyTech/Anamorph/Presets/__AnamorphStateHarness__.anamorph'
            real_preset.parent.mkdir(parents=True)
            real_preset.write_bytes(b'preexisting caller preset\n')
            with patch.dict(os.environ, {'HOME': str(fake_home), 'XDG_CONFIG_HOME': str(fake_home / 'other-config')}):
                with Profile() as profile:
                    paths = profile.validate(self.probe)
                    self.assertEqual(paths['preset'], str(profile.preset))
                    self.assertNotEqual(paths['home'], str(fake_home))
                    fixture = profile.preset / '__AnamorphStateHarness__.anamorph'
                    code = 'import pathlib,sys; p=pathlib.Path(sys.argv[1]); p.write_bytes(b"test preset"); assert p.read_bytes()==b"test preset"'
                    result = subprocess.run([sys.executable, '-c', code, str(fixture)], env=profile.env)
                    self.assertEqual(result.returncode, 0)
                    self.assertEqual(fixture.read_bytes(), b'test preset')
                    root = profile.root
                self.assertFalse(root.exists())
            self.assertEqual(real_preset.read_bytes(), b'preexisting caller preset\n')

    def test_missing_or_altered_isolation_variables_fail_before_work(self):
        with Profile() as profile:
            for key in ('HOME', 'XDG_CONFIG_HOME', 'TMPDIR'):
                env = profile.env.copy()
                env.pop(key)
                with self.assertRaises(IsolationError):
                    profile.validate(self.probe, env)
            env = profile.env.copy()
            env['HOME'] = '/tmp'
            with self.assertRaises(IsolationError):
                profile.validate(self.probe, env)

    def test_unusable_parent_fails(self):
        with tempfile.TemporaryDirectory(prefix='nr026-parent-', dir='/tmp') as raw:
            not_directory = Path(raw) / 'file'
            not_directory.write_text('not a directory')
            with self.assertRaises(OSError):
                with Profile(not_directory):
                    self.fail('an unusable parent must fail closed')

    def test_juce_user_dirs_escape_is_detected_and_refused(self):
        with tempfile.TemporaryDirectory(prefix='nr026-outside-', dir='/tmp') as raw:
            outside = Path(raw) / 'outside'
            outside.mkdir()
            with Profile() as profile:
                (profile.config / 'user-dirs.dirs').write_text(f'XDG_CONFIG_HOME="{outside}"\n')
                result = subprocess.run([str(self.probe)], env=profile.env, text=True, capture_output=True, check=True)
                self.assertEqual(result.stdout.splitlines()[1], str(outside))
                with self.assertRaises(IsolationError):
                    profile.validate(self.probe)

    def test_symlink_escape_is_rejected_and_cleanup_does_not_follow_it(self):
        with tempfile.TemporaryDirectory(prefix='nr026-symlink-', dir='/tmp') as raw:
            outside = Path(raw)
            sentinel = outside / 'sentinel'
            sentinel.write_bytes(b'untouched')
            with Profile() as profile:
                profile.preset.rmdir()
                profile.preset.symlink_to(outside, target_is_directory=True)
                with self.assertRaises(IsolationError):
                    profile.validate(self.probe)
            self.assertEqual(sentinel.read_bytes(), b'untouched')

    def test_cleanup_refuses_a_replaced_root(self):
        with tempfile.TemporaryDirectory(prefix='nr026-root-swap-', dir='/tmp') as raw:
            with Profile(Path(raw)) as profile:
                original = profile.root
                parked = original.with_name(original.name + '-original')
                original.rename(parked)
                original.mkdir(mode=0o700)
                sentinel = original / 'sentinel'
                sentinel.write_bytes(b'belongs to replacement')
                try:
                    with self.assertRaises(IsolationError):
                        profile.cleanup()
                    self.assertEqual(sentinel.read_bytes(), b'belongs to replacement')
                finally:
                    # Both paths were created by this control, not by another run.
                    if original.exists():
                        shutil.rmtree(original)
                    parked.rename(original)
                    profile.root = original
            self.assertFalse(original.exists())

    def test_parallel_invocations_are_separate(self):
        with tempfile.TemporaryDirectory(prefix='nr026-concurrent-', dir='/tmp') as raw:
            shared = Path(raw)
            children = [subprocess.Popen([sys.executable, __file__, '--concurrent-child', str(self.probe), raw, name],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                        for name in ('one', 'two')]
            try:
                wait_for([shared / 'ready-one', shared / 'ready-two'])
                entries = [json.loads((shared / ('ready-' + name)).read_text()) for name in ('one', 'two')]
                self.assertNotEqual(entries[0]['root'], entries[1]['root'])
                self.assertEqual([Path(x['preset']).read_text() for x in entries], ['one', 'two'])
                (shared / 'release').touch()
                for child in children:
                    _, err = child.communicate(timeout=20)
                    self.assertEqual(child.returncode, 0, err)
                self.assertTrue(all(not Path(x['root']).exists() for x in entries))
            finally:
                for child in children:
                    if child.poll() is None:
                        child.kill()
                        child.wait()

    def test_interruption_stops_child_and_preserves_external_output(self):
        with tempfile.TemporaryDirectory(prefix='nr026-interrupt-', dir='/tmp') as raw:
            shared = Path(raw)
            child = subprocess.Popen([sys.executable, __file__, '--interrupt-child', str(self.probe), raw],
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                wait_for([shared / 'interrupt-ready', shared / 'child-pid', shared / 'grandchild-pid'])
                root = Path((shared / 'interrupt-ready').read_text())
                pid = int((shared / 'child-pid').read_text())
                grandchild_pid = int((shared / 'grandchild-pid').read_text())
                child.send_signal(signal.SIGTERM)
                _, err = child.communicate(timeout=20)
                self.assertEqual(child.returncode, 73, err)
                self.assertFalse(root.exists())
                self.assertIn('partial evidence', (shared / 'partial.stdout').read_text())
                with self.assertRaises(ProcessLookupError):
                    os.kill(pid, 0)
                # A killed orphan may briefly remain as a zombie until init reaps it.
                deadline = time.monotonic() + 3
                while process_is_running(grandchild_pid) and time.monotonic() < deadline:
                    time.sleep(0.02)
                self.assertFalse(process_is_running(grandchild_pid), 'a live grandchild survived interruption')
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait()
                if (shared / 'grandchild-pid').exists() and (shared / 'child-pid').exists():
                    grandchild_pid = int((shared / 'grandchild-pid').read_text())
                    try:
                        # This process was created by this control; guard against PID reuse.
                        if os.getpgid(grandchild_pid) == int((shared / 'child-pid').read_text()):
                            os.kill(grandchild_pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', type=Path)
    parser.add_argument('--concurrent-child', nargs=3)
    parser.add_argument('--interrupt-child', nargs=2)
    args = parser.parse_args()
    if args.concurrent_child:
        probe, shared, name = args.concurrent_child
        return concurrent_child(Path(probe), Path(shared), name)
    if args.interrupt_child:
        probe, shared = args.interrupt_child
        return interrupt_child(Path(probe), Path(shared))
    if args.probe is None or not args.probe.is_file():
        parser.error('--probe requires the linked path witness')
    IsolationTests.probe = args.probe.resolve()
    return 0 if unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(IsolationTests)).wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(main())
