#!/usr/bin/env python3
"""Observe both sides of the fixture's identity-publication handshake."""
import ctypes
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

from rosetta_cleanup_process_tests import FIXTURE, independent_finally_cleanup


def wait_for(path, deadline):
    while not path.exists():
        if time.monotonic() >= deadline:
            raise TimeoutError('waiting for ' + path.name)
        time.sleep(0.005)


def instrumented_fixture(legacy):
    # The parent cannot publish until the observer explicitly permits it. The
    # child reports reaching its gate independently; neither ordering relies on
    # the observer receiving CPU time within an arbitrary sleep interval.
    parent = "    if child:\n"
    pause = ("        (root/'parent-blocked').write_text('ready')\n"
             "        until=time.monotonic()+4\n"
             "        while not (root/'observer-ack').exists() and time.monotonic()<until: time.sleep(0.005)\n"
             "        if not (root/'observer-ack').exists():\n"
             "            (root/'parent-timeout').write_text('timeout')\n"
             "            raise RuntimeError('observer acknowledgement timed out')\n")
    gate = ("    until=time.monotonic()+1\n"
            "    while not (root/'identity-written').exists() and time.monotonic()<until: time.sleep(0.005)\n"
            "    if not (root/'identity-written').exists(): raise RuntimeError('fixture identity publication timed out')\n")
    marker = "        (root/'identity-written').write_text('ready')\n"
    assert FIXTURE.count(parent) == FIXTURE.count(gate) == FIXTURE.count(marker) == 1
    # The larger bound is for the deliberately paused parent/observer control;
    # publication ordering comes from the markers, not the deadline.
    gated = "    (root/'child-at-gate').write_text('ready')\n" + gate.replace('+1\n', '+6\n')
    source = FIXTURE.replace(parent, parent + pause, 1).replace(gate, gated, 1)
    if legacy:
        source = source.replace(marker, '', 1).replace(gated,
            "    (root/'child-at-gate').write_text('ready')\n", 1)
    return source


def observe(legacy, pause_observer=False, fail_sync=False, fail_observer=False):
    with tempfile.TemporaryDirectory(prefix='rosetta-handshake-') as private:
        root = Path(private)
        proc = subprocess.Popen([sys.executable, '-c', instrumented_fixture(legacy), private, 'leader_exit'],
                                start_new_session=True, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        try:
            deadline = time.monotonic() + 5
            wait_for(root/'parent-blocked', deadline)
            wait_for(root/'child-at-gate', deadline)
            if pause_observer:
                # Stress scheduling; this delay does not establish causality.
                time.sleep(0.35)
            assert not (root/'identity.json').exists()
            if fail_observer:
                try:
                    wait_for(root/'never-published', time.monotonic()+0.05)
                except TimeoutError:
                    return 'observer timeout bounded'
                raise AssertionError('observer timeout control did not time out')
            if fail_sync:
                wait_for(root/'parent-timeout', deadline)
                assert proc.wait(timeout=1) != 0
                return 'missing acknowledgement bounded'
            if legacy:
                wait_for(root/'ready', deadline)
                try:
                    json.loads((root/'identity.json').read_text())
                except (OSError, ValueError):
                    pass
                else:
                    raise AssertionError('legacy identity unexpectedly available before acknowledgement')
            else:
                # The child has reached the gate and the parent is blocked
                # before writing the marker on which the child waits.
                assert not (root/'ready').exists(), 'child passed its gate before publication'
            (root/'observer-ack').write_text('observed')
            wait_for(root/'ready', deadline)
            assert proc.wait(timeout=2) == 0
            identity = json.loads((root/'identity.json').read_text())
            assert identity['leader'] == proc.pid == identity['pgid']
            assert os.getpgid(identity['child']) == proc.pid
            return 'legacy defect observed' if legacy else 'complete identity observed'
        finally:
            result = independent_finally_cleanup(proc, time.monotonic()+2)
            if not result['complete']:
                raise RuntimeError('fixture cleanup failed: ' + repr(result))


def main():
    if sys.platform != 'linux' or os.getuid() == 0:
        raise RuntimeError('requires an ordinary-user Linux host')
    libc = ctypes.CDLL(None, use_errno=True)
    old = ctypes.c_int()
    if libc.prctl(37, ctypes.byref(old), 0, 0, 0) != 0 or libc.prctl(36, 1, 0, 0, 0) != 0:
        raise OSError(ctypes.get_errno(), 'fixture child-subreaper setup')
    try:
        results = [observe(True), observe(False), observe(True, pause_observer=True),
                   observe(False, pause_observer=True), observe(False, fail_sync=True),
                   observe(False, fail_observer=True)]
        print('cleanup fixture handshake: ' + '; '.join(results))
    finally:
        if libc.prctl(36, old.value, 0, 0, 0) != 0:
            raise OSError(ctypes.get_errno(), 'fixture child-subreaper restore')


if __name__ == '__main__':
    main()
