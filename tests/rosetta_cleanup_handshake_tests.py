#!/usr/bin/env python3
"""Exercise the real cleanup fixture with a delayed identity-writing parent."""
import ctypes
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

from rosetta_cleanup_process_tests import FIXTURE


def observe(source):
    with tempfile.TemporaryDirectory(prefix='rosetta-handshake-') as private:
        root = Path(private)
        child_pid = None
        proc = subprocess.Popen([sys.executable, '-c', source, private, 'leader_exit'],
                                start_new_session=True, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        try:
            deadline = time.monotonic() + 3
            while not (root/'ready').exists():
                if time.monotonic() >= deadline:
                    raise RuntimeError('fixture readiness timed out')
                time.sleep(0.001)
            try:
                observed = json.loads((root/'identity.json').read_text())
                complete_at_ready = observed['leader'] == proc.pid and observed['pgid'] == proc.pid
            except (OSError, ValueError, KeyError):
                complete_at_ready = False
            # Wait for the known child identity before cleanup, including in the
            # deliberately broken control. Only this new process group is killed.
            while child_pid is None:
                try:
                    identity = json.loads((root/'identity.json').read_text())
                    child_pid = identity['child']
                except (OSError, ValueError, KeyError):
                    if time.monotonic() >= deadline:
                        raise RuntimeError('fixture identity timed out')
                    time.sleep(0.001)
            proc.wait(timeout=2)
            return complete_at_ready
        finally:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait(timeout=2)
            # The standalone harness owns only these fixture descendants.
            # Reap even when failure occurred before the child PID was published.
            until = time.monotonic() + 2
            while True:
                try:
                    pid, _ = os.waitpid(-1, os.WNOHANG)
                except ChildProcessError:
                    break
                if pid == 0:
                    if time.monotonic() >= until:
                        raise RuntimeError('fixture child did not reap before deadline')
                    time.sleep(0.005)


def main():
    if sys.platform != 'linux' or os.getuid() == 0:
        raise RuntimeError('requires an ordinary-user Linux host')
    libc = ctypes.CDLL(None, use_errno=True)
    old = ctypes.c_int()
    if libc.prctl(37, ctypes.byref(old), 0, 0, 0) != 0 or libc.prctl(36, 1, 0, 0, 0) != 0:
        raise OSError(ctypes.get_errno(), 'fixture child-subreaper setup')
    try:
        delayed = FIXTURE.replace('    if child:\n', '    if child:\n        time.sleep(0.25)\n', 1)
        assert delayed != FIXTURE
        marker = "        (root/'identity-written').write_text('ready')\n"
        wait = "    until=time.monotonic()+1\n    while not (root/'identity-written').exists() and time.monotonic()<until: time.sleep(0.005)\n    if not (root/'identity-written').exists(): raise RuntimeError('fixture identity publication timed out')\n"
        assert marker in delayed and wait in delayed
        old_fixture = delayed.replace(marker, '').replace(wait, '')
        assert not observe(old_fixture), 'legacy fixture failed to expose its ready-before-identity race'
        assert observe(delayed), 'fixed fixture exposed readiness before complete identity'
        print('cleanup fixture handshake: delayed legacy control detected; fixed fixture passed')
    finally:
        if libc.prctl(36, old.value, 0, 0, 0) != 0:
            raise OSError(ctypes.get_errno(), 'fixture child-subreaper restore')


if __name__ == '__main__':
    main()
