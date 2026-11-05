#!/usr/bin/env python3
"""Exercise direct State launches using only a disposable caller profile."""
import os
from pathlib import Path
import queue
import re
import signal
import subprocess
import sys
import tempfile
import threading

if not __debug__:
    raise SystemExit("State isolation controls require Python assertions")


def ready_process(command, env):
    proc = subprocess.Popen(command + ["--filesystem-isolation-probe", "hold"],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True, encoding="utf-8",
                            errors="replace", env=env)
    messages = queue.Queue()

    def read_ready():
        output = []
        for line in proc.stdout:
            output.append(line)
            if line.startswith("STATE_ISOLATION_READY="):
                messages.put((Path(line.strip().split("=", 1)[1]), ""))
                return
        messages.put((None, "".join(output)))

    threading.Thread(target=read_ready, daemon=True).start()
    try:
        root, output = messages.get(timeout=90)
    except queue.Empty:
        proc.kill()
        proc.wait()
        raise AssertionError("State isolation probe did not become ready")
    except BaseException:
        proc.kill()
        proc.wait()
        raise
    if root is None:
        error = proc.stderr.read()
        proc.wait()
        raise AssertionError(f"State isolation probe failed: {output}\n{error}")
    return proc, root


def finish(proc):
    proc.stdin.write("\n")
    proc.stdin.flush()
    _, error = proc.communicate(timeout=90)
    if proc.returncode:
        raise AssertionError(f"State isolation probe exited {proc.returncode}: {error}")
    return error


def main():
    def stop_on_term(signum, _frame):
        raise SystemExit(128 + signum)

    signal.signal(signal.SIGTERM, stop_on_term)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    command = sys.argv[1:]
    full_suite = bool(command and command[0] == "--full-suite")
    if full_suite:
        command = command[1:]
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        raise SystemExit("usage: state_isolation_tests.py [--full-suite] -- [runner ...] StateExecutable")

    processes = []
    with tempfile.TemporaryDirectory(prefix="anamorph-state-controls-") as directory:
        base = Path(directory)
        home, tmp = base / "caller-home", base / "shared-temp"
        home.mkdir()
        if os.name == "nt":
            tmp.mkdir()
        else:
            target = base / "shared-temp-target"
            target.mkdir()
            tmp.symlink_to(target, target_is_directory=True)
        default_presets = home / ".config" / "RollyTech" / "Anamorph" / "Presets"
        redirected_presets = home / "redirected" / "RollyTech" / "Anamorph" / "Presets"
        sentinels = []
        for folder in (default_presets, redirected_presets):
            folder.mkdir(parents=True)
            for name in ("__isolation_control__.anamorph", "__AnamorphStateHarness__.anamorph"):
                preset = folder / name
                preset.write_text("caller preset sentinel", encoding="utf-8")
                sentinels.append(preset)
        (home / ".config" / "user-dirs.dirs").write_text('XDG_CONFIG_HOME="$HOME/redirected"\n', encoding="utf-8")
        sentinel = tmp / "AnamorphAdr0055Harness" / "sentinel"
        sentinel.parent.mkdir()
        sentinel.write_text("shared temp sentinel", encoding="utf-8")
        # JUCE adds the executable name beneath TMPDIR on macOS.
        mac_sentinel = tmp / Path(command[-1]).stem / "AnamorphAdr0055Harness" / "sentinel"
        mac_sentinel.parent.mkdir(parents=True, exist_ok=True)
        mac_sentinel.write_text("mac temp sentinel", encoding="utf-8")
        env = os.environ.copy()
        env.update(HOME=str(home), TMPDIR=str(tmp), TMP=str(tmp), TEMP=str(tmp),
                   XDG_CONFIG_HOME=str(home / "other-config"))
        try:
            if sys.platform.startswith("linux"):
                # The fake JUCE mapping resolves to redirected_presets. Even a
                # symlinked TMPDIR pointing there must fail before a test runs.
                overlap = base / "overlapping-temp"
                overlap.symlink_to(redirected_presets, target_is_directory=True)
                overlap_env = dict(env, TMPDIR=str(overlap))
                refused = subprocess.run(command + ["--filesystem-isolation-probe"], env=overlap_env,
                                         capture_output=True, text=True, timeout=90)
                assert refused.returncode == 2 and "cannot establish" in refused.stderr, refused
                assert all(p.read_text(encoding="utf-8") == "caller preset sentinel" for p in sentinels)
                print("Linux symlinked preset/temp overlap rejected before test execution")

            if full_suite:
                suite = subprocess.Popen(command, stdout=subprocess.PIPE, text=True,
                                         encoding="utf-8", errors="replace", env=env)
                processes.append(suite)
                saw_preset_test = saw_summary = False
                for line in suite.stdout:
                    print(line, end="", flush=True)
                    saw_preset_test |= "State test 8:" in line
                    match = re.fullmatch(r"\s*(\d+) checks, 0 failure\(s\)\s*", line)
                    saw_summary |= match is not None and int(match.group(1)) > 0
                assert suite.wait() == 0 and saw_preset_test and saw_summary, "full State suite failed or did not run"
                assert all(p.read_text(encoding="utf-8") == "caller preset sentinel" for p in sentinels)
                assert sentinel.read_text(encoding="utf-8") == "shared temp sentinel"
                assert mac_sentinel.read_text(encoding="utf-8") == "mac temp sentinel"
                assert not list(tmp.rglob("anamorph-state-*")), "full State suite left an isolated profile"

            invalid = subprocess.run(command + ["--test-invalid-isolation-root"], env=env,
                                     capture_output=True, text=True, timeout=90)
            assert invalid.returncode == 2 and "cannot establish" in invalid.stderr, invalid

            first, root1 = ready_process(command, env)
            processes.append(first)
            second, root2 = ready_process(command, env)
            processes.append(second)
            assert root1 != root2 and root1.resolve().is_relative_to(tmp.resolve()) and root2.resolve().is_relative_to(tmp.resolve())
            assert (root1 / "presets" / "__isolation_control__.anamorph").is_file()
            assert (root2 / "presets" / "__isolation_control__.anamorph").is_file()
            finish(first)
            finish(second)
            assert not root1.exists() and not root2.exists(), "normal cleanup left a profile"

            replaced, root3 = ready_process(command, env)
            processes.append(replaced)
            moved = root3.with_name(root3.name + "-moved")
            root3.rename(moved)
            root3.mkdir()
            # Even a copied ownership marker must not make a replacement root
            # look like the original directory.
            (root3 / ".anamorph-state-owner").write_bytes((moved / ".anamorph-state-owner").read_bytes())
            replacement_sentinel = root3 / "do-not-delete"
            replacement_sentinel.write_text("replacement", encoding="utf-8")
            assert "cleanup refused" in finish(replaced)
            assert replacement_sentinel.read_text(encoding="utf-8") == "replacement"

            interrupted, root4 = ready_process(command, env)
            processes.append(interrupted)
            interrupted.terminate()
            interrupted.communicate(timeout=90)
            assert root4.is_dir(), "terminated process unexpectedly removed its profile"
            assert all(p.read_text(encoding="utf-8") == "caller preset sentinel" for p in sentinels)
            assert sentinel.read_text(encoding="utf-8") == "shared temp sentinel"
            assert mac_sentinel.read_text(encoding="utf-8") == "mac temp sentinel"
        finally:
            for proc in processes:
                if proc.poll() is None:
                    proc.kill()
                    proc.communicate(timeout=90)
    print("State filesystem isolation: save/load, concurrent roots, fail-closed setup, replacement and interruption controls passed")


if __name__ == "__main__":
    main()
