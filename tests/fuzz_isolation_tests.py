#!/usr/bin/env python3
"""Linux libFuzzer filesystem lifecycle controls; no caller-profile mutation."""
import os
from pathlib import Path
import queue
import signal
import subprocess
import sys
import tempfile
import threading

from state_isolation_tests import protected_snapshot, protected_snapshot_controls

if not __debug__:
    raise SystemExit('Fuzz isolation controls require Python assertions')


class Progress:
    """Actual entry/cleanup witnesses, independent of libFuzzer budget semantics."""
    def __init__(self):
        self.roots = set()
        self.current = None
        self.count = 0

    def ready(self, sequence, root):
        assert self.current is None, 'next callback started before cleanup witness'
        assert sequence == self.count + 1, 'missing or repeated callback progression'
        assert root not in self.roots, 'callback reused a previous profile'
        assert all(not old.exists() for old in self.roots), 'previous input profile survived callback return'
        self.current = root
        self.roots.add(root)
        self.count = sequence

    def clean(self, sequence, root):
        assert sequence == self.count and root == self.current, 'cleanup witness does not match callback'
        assert not root.exists(), 'callback cleanup witness left its profile'
        self.current = None

    def finished(self):
        assert self.current is None, 'process ended before callback cleanup'
        assert self.count >= 2, 'fewer than two verified callback transitions'
        assert all(not root.exists() for root in self.roots), 'final profile survived process exit'


def progress_controls():
    with tempfile.TemporaryDirectory(prefix='anamorph-fuzz-protocol-') as directory:
        base = Path(directory)
        a, b = base / 'a', base / 'b'
        good = Progress()
        good.ready(1, a)
        good.clean(1, a)
        good.ready(2, b)
        good.clean(2, b)
        good.finished()

        def rejects(action):
            try:
                action()
            except AssertionError:
                return
            raise AssertionError('invalid lifecycle transcript was accepted')

        rejects(Progress().finished)
        one = Progress()
        one.ready(1, a)
        one.clean(1, a)
        rejects(one.finished)
        rejects(lambda: one.ready(3, b))
        rejects(lambda: one.ready(2, a))
        pending = Progress()
        pending.ready(1, a)
        rejects(pending.finished)
        rejects(lambda: pending.ready(2, b))
        rejects(lambda: pending.clean(2, a))
        a.mkdir()
        rejects(lambda: pending.clean(1, a))


def main():
    protected_snapshot_controls()
    progress_controls()
    command = sys.argv[1:]
    if command == ['--self-test']:
        return
    if not command:
        raise SystemExit('usage: fuzz_isolation_tests.py FuzzExecutable')
    processes = []
    with tempfile.TemporaryDirectory(prefix='anamorph-fuzz-controls-') as directory:
        base = Path(directory)
        home, tmp = base / 'home', base / 'tmp'
        presets = home / '.config' / 'RollyTech' / 'Anamorph' / 'Presets'
        presets.mkdir(parents=True)
        tmp.mkdir()
        (presets / '__isolation_control__.anamorph').write_text('caller preset sentinel')
        snapshot = protected_snapshot(presets)
        env = dict(os.environ, HOME=str(home), TMPDIR=str(tmp), TMP=str(tmp), TEMP=str(tmp),
                   ANAMORPH_FUZZ_ISOLATION_CONTROL='hold')
        # This only bounds the run. At least two actual entry/cleanup pairs are
        # required; additional callbacks are verified and released until exit.
        args = command + ['-runs=8', '-seed=20260818', '-artifact_prefix=' + str(base / 'finding-')]

        def start():
            error_file = tempfile.TemporaryFile(mode='w+t')
            proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                    stderr=error_file, text=True, env=env)
            messages = queue.Queue()
            processes.append((proc, error_file))

            def read():
                for line in proc.stdout:
                    messages.put(line)
                messages.put(None)

            threading.Thread(target=read, daemon=True).start()
            return proc, messages, Progress(), error_file

        def message(messages, marker):
            line = messages.get(timeout=90)
            assert line is not None, 'fuzz process ended before its next lifecycle witness'
            assert line.startswith(marker + '='), f'unexpected lifecycle witness: {line}'
            sequence, root = line.strip().split('=', 1)[1].split('\t', 1)
            return int(sequence), Path(root)

        def ready(messages, progress, first=None):
            sequence, root = first or message(messages, 'FUZZ_ISOLATION_READY')
            progress.ready(sequence, root)
            assert root.resolve().is_relative_to(tmp.resolve())
            assert (root / 'presets').is_dir() and (root / 'temp').is_dir()
            assert not list((root / 'presets').iterdir()), 'fuzz input acquired caller presets'
            return root

        def release(proc):
            proc.stdin.write('\n')
            proc.stdin.flush()

        def error(proc, stream):
            proc.wait(timeout=90)
            stream.seek(0)
            return stream.read()

        def drain(proc, messages, progress, stream):
            # The first callback is held by the caller. Every further callback is
            # acknowledged from the transcript, never from an assumed -runs count.
            while True:
                release(proc)
                sequence, root = message(messages, 'FUZZ_ISOLATION_CLEAN')
                progress.clean(sequence, root)
                line = messages.get(timeout=90)
                if line is None:
                    break
                assert line.startswith('FUZZ_ISOLATION_READY='), line
                sequence, root = line.strip().split('=', 1)[1].split('\t', 1)
                ready(messages, progress, (int(sequence), Path(root)))
            failure = error(proc, stream)
            assert proc.returncode == 0, failure
            progress.finished()
            print(f'Fuzz lifecycle verified {progress.count} actual callbacks', flush=True)

        try:
            first, q1, p1, e1 = start()
            second, q2, p2, e2 = start()
            root1, root2 = ready(q1, p1), ready(q2, p2)
            assert root1 != root2
            drain(first, q1, p1, e1)
            drain(second, q2, p2, e2)

            eof, qe, pe, ee = start()
            eof_root = ready(qe, pe)
            eof.stdin.close()
            eof.stdin = None
            failure = error(eof, ee)
            assert eof.returncode != 0 and 'FUZZ_HARNESS_FAILURE=release' in failure
            assert not eof_root.exists(), 'EOF control failed to clean its profile'

            replaced, qr, pr, er = start()
            root = ready(qr, pr)
            moved = root.with_name(root.name + '-moved')
            root.rename(moved)
            root.mkdir()
            (root / '.anamorph-state-owner').write_bytes((moved / '.anamorph-state-owner').read_bytes())
            (root / 'keep').write_text('replacement')
            release(replaced)
            failure = error(replaced, er)
            assert replaced.returncode != 0 and 'FUZZ_HARNESS_FAILURE=cleanup' in failure
            assert 'ERROR: AddressSanitizer:' not in failure, 'cleanup control also hit memory corruption'
            assert (root / 'keep').read_text() == 'replacement' and moved.is_dir()

            killed, qk, pk, ek = start()
            killed_root = ready(qk, pk)
            killed.kill()
            error(killed, ek)
            assert killed_root.is_dir(), 'forced kill unexpectedly claimed cleanup'

            for control, kind in (('invalid-parent', 'setup'), ('bypass', 'override')):
                result = subprocess.run(args, env=dict(env, ANAMORPH_FUZZ_ISOLATION_CONTROL=control),
                                        capture_output=True, text=True, timeout=90)
                assert result.returncode != 0 and f'FUZZ_HARNESS_FAILURE={kind}' in result.stderr, result
                assert 'FUZZ_ISOLATION_READY=' not in result.stdout
            assert protected_snapshot(presets) == snapshot, 'caller preset sentinel tree changed'
            assert set(tmp.glob('anamorph-state-*')) == {root, moved, killed_root}
        finally:
            for proc, stream in processes:
                if proc.poll() is None:
                    proc.kill()
                    proc.wait(timeout=90)
                stream.close()
    print('Fuzz isolation lifecycle and fail-closed harness controls passed')


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, lambda signum, frame: sys.exit(128 + signum))
    main()
