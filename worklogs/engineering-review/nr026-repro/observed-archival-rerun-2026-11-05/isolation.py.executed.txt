#!/usr/bin/env python3
"""Linux-only disposable user state for the nonshipping NR-026 diagnostics."""
import os
from pathlib import Path
import shutil
import signal
import stat
import subprocess
import sys
import tempfile


class IsolationError(RuntimeError):
    pass


def _private_directory(path):
    info = path.lstat()  # Do not follow a symlink, including a dangling one.
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.geteuid() or info.st_mode & 0o077:
        raise IsolationError(f'not a private owned directory: {path}')
    if path.resolve(strict=True) != path:
        raise IsolationError(f'symlink or redirected directory: {path}')
    return info


class Profile:
    """Own exactly one fresh 0700 tree; never clean up a tree with a changed identity."""
    def __init__(self, temp_parent=Path('/tmp')):
        self.parent = Path(temp_parent).resolve(strict=True)
        self.root = None

    def __enter__(self):
        if sys.platform != 'linux' or not shutil.rmtree.avoids_symlink_attacks:
            raise IsolationError('requires Linux and fd-safe shutil.rmtree')
        self.root = Path(tempfile.mkdtemp(prefix='anamorph-nr026-', dir=self.parent))
        self.identity = _private_directory(self.root)
        try:
            self.home = self.root / 'home'
            self.config = self.home / '.config'
            self.preset = self.config / 'RollyTech' / 'Anamorph' / 'Presets'
            self.tmp = self.root / 'tmp'
            for directory in (self.home, self.config, self.config / 'RollyTech',
                              self.config / 'RollyTech' / 'Anamorph', self.preset,
                              self.tmp, self.root / 'data', self.root / 'cache',
                              self.root / 'state', self.root / 'runtime'):
                directory.mkdir(mode=0o700)
            self.env = os.environ.copy()
            self.env.update(HOME=str(self.home), XDG_CONFIG_HOME=str(self.config),
                            XDG_DATA_HOME=str(self.root / 'data'), XDG_CACHE_HOME=str(self.root / 'cache'),
                            XDG_STATE_HOME=str(self.root / 'state'), XDG_RUNTIME_DIR=str(self.root / 'runtime'),
                            TMPDIR=str(self.tmp))
            for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'ICEAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
                self.env.pop(name, None)
            return self
        except BaseException:
            self.cleanup()
            raise

    def validate(self, probe, env=None):
        env = self.env if env is None else env
        expected_env = {'HOME': self.home, 'XDG_CONFIG_HOME': self.config, 'TMPDIR': self.tmp,
                        'XDG_DATA_HOME': self.root / 'data', 'XDG_CACHE_HOME': self.root / 'cache',
                        'XDG_STATE_HOME': self.root / 'state', 'XDG_RUNTIME_DIR': self.root / 'runtime'}
        for key, value in expected_env.items():
            if env.get(key) != str(value):
                raise IsolationError(f'missing or altered isolation variable: {key}')
        info = _private_directory(self.root)
        if (info.st_dev, info.st_ino) != (self.identity.st_dev, self.identity.st_ino):
            raise IsolationError('temporary root identity changed')
        for directory in (self.home, self.config, self.config / 'RollyTech',
                          self.config / 'RollyTech' / 'Anamorph', self.preset, self.tmp,
                          self.root / 'data', self.root / 'cache', self.root / 'state', self.root / 'runtime'):
            _private_directory(directory)
        # JUCE reads this file, not XDG_CONFIG_HOME from the environment. Refuse
        # even a harmless mapping, so a path cannot change after our witness.
        if os.path.lexists(self.config / 'user-dirs.dirs'):
            raise IsolationError('user-dirs.dirs redirection is forbidden')
        result = subprocess.run([str(probe)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True, check=False)
        actual = result.stdout.splitlines()
        expected = [str(self.home), str(self.config), str(self.preset), str(self.tmp)]
        if result.returncode != 0 or result.stderr or actual != expected:
            raise IsolationError(f'JUCE/PresetManager path witness failed: {actual!r}')
        return dict(zip(('home', 'application_data', 'preset', 'temporary'), actual))

    def cleanup(self):
        if self.root is None:
            return
        try:
            info = self.root.lstat()
        except FileNotFoundError:
            return
        if (not stat.S_ISDIR(info.st_mode) or info.st_uid != os.geteuid()
                or (info.st_dev, info.st_ino) != (self.identity.st_dev, self.identity.st_ino)):
            raise IsolationError(f'refusing cleanup of replaced temporary root: {self.root}')
        shutil.rmtree(self.root)
        self.root = None

    def __exit__(self, exc_type, exc, tb):
        self.cleanup()


def run_child(argv, *, cwd, env, stdout, stderr):
    """Forward interruption to the entire child group before profile cleanup."""
    child = subprocess.Popen(argv, cwd=cwd, env=env, stdout=stdout, stderr=stderr, start_new_session=True)
    try:
        return child.wait()
    except BaseException:
        try:
            os.killpg(child.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        child.wait()
        raise


def install_signal_handlers():
    def interrupted(signum, _frame):
        raise InterruptedError(f'interrupted by signal {signum}')
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
