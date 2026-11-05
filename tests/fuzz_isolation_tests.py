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

from state_isolation_tests import protected_snapshot


def main():
    command = sys.argv[1:]
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
        # ASan leak disabling is the harness's existing intentional-JUCE-leak contract.
        # Preserve any caller detector settings; CI supplies the configured ASan/UBSan options.
        args = command + ['-runs=2', '-seed=20260818', '-artifact_prefix=' + str(base / 'finding-')]

        def start():
            proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, text=True, env=env)
            processes.append(proc)
            messages = queue.Queue()

            def read():
                for line in proc.stdout:
                    messages.put(line)
                messages.put(None)

            threading.Thread(target=read, daemon=True).start()
            return proc, messages

        def ready(proc, messages):
            while True:
                line = messages.get(timeout=90)
                assert line is not None, 'fuzz entry did not reach its isolation witness'
                if line.startswith('FUZZ_ISOLATION_READY='):
                    root = Path(line.strip().split('=', 1)[1])
                    assert root.resolve().is_relative_to(tmp.resolve())
                    assert (root / 'presets').is_dir() and (root / 'temp').is_dir()
                    assert not list((root / 'presets').iterdir()), 'fuzz input acquired caller presets'
                    return root

        def release(proc):
            proc.stdin.write('\n')
            proc.stdin.flush()

        try:
            first, q1 = start()
            second, q2 = start()
            root1, root2 = ready(first, q1), ready(second, q2)
            assert root1 != root2
            for proc, messages, old in ((first, q1, root1), (second, q2, root2)):
                release(proc)
                new = ready(proc, messages)
                assert new != old and not old.exists(), 'previous input profile survived callback return'
                release(proc)
                _, error = proc.communicate(timeout=90)
                assert proc.returncode == 0, error
                assert not new.exists(), 'final input profile survived normal libFuzzer exit'

            eof, qe = start()
            eof_root = ready(eof, qe)
            eof.stdin.close()
            eof.stdin = None
            _, error = eof.communicate(timeout=90)
            assert eof.returncode != 0 and 'Fuzz isolation release failed' in error
            assert not eof_root.exists(), 'EOF control failed to clean its profile'

            replaced, qr = start()
            root = ready(replaced, qr)
            moved = root.with_name(root.name + '-moved')
            root.rename(moved)
            root.mkdir()
            (root / '.anamorph-state-owner').write_bytes((moved / '.anamorph-state-owner').read_bytes())
            (root / 'keep').write_text('replacement')
            release(replaced)
            _, error = replaced.communicate(timeout=90)
            assert replaced.returncode != 0 and 'Fuzz isolation cleanup failed' in error
            assert (root / 'keep').read_text() == 'replacement' and moved.is_dir()

            killed, qk = start()
            killed_root = ready(killed, qk)
            killed.kill()
            killed.communicate(timeout=90)
            assert killed_root.is_dir(), 'forced kill unexpectedly claimed cleanup'

            for control, message in (('invalid-parent', 'setup failed'), ('bypass', 'override is not active')):
                result = subprocess.run(args, env=dict(env, ANAMORPH_FUZZ_ISOLATION_CONTROL=control),
                                        capture_output=True, text=True, timeout=90)
                assert result.returncode != 0 and message in result.stderr, result
                assert 'FUZZ_ISOLATION_READY=' not in result.stdout
            assert protected_snapshot(presets) == snapshot, 'caller preset sentinel tree changed'
            # Only deliberately replaced/killed roots remain, all beneath this disposable controller.
            assert set(tmp.glob('anamorph-state-*')) == {root, moved, killed_root}
        finally:
            for proc in processes:
                if proc.poll() is None:
                    proc.kill()
                    proc.communicate(timeout=90)
    print('Fuzz isolation: repeated input/exit cleanup, concurrent roots, EOF, replacement, kill, setup and bypass controls passed')


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, lambda signum, frame: sys.exit(128 + signum))
    main()
