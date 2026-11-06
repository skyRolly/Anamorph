#!/usr/bin/env python3
"""Exercise direct State launches using only a disposable caller profile."""
import os
import hashlib
import json
from pathlib import Path
import queue
import re
import signal
import stat
import subprocess
import sys
import tempfile
import threading

if not __debug__:
    raise SystemExit("State isolation controls require Python assertions")


def protected_snapshot(path):
    """Read-only; never follow links or create a missing production directory."""
    result = {}

    def reject_redirect(item, info):
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise AssertionError(f"unsupported protected redirect: {item}")

    # A link in an ancestor redirects access just as a linked root does. Check
    # from the filesystem root down before reading any protected tree contents.
    for item in reversed((path, *path.parents)):
        try:
            reject_redirect(item, item.lstat())
        except FileNotFoundError:
            pass

    def visit(item):
        try:
            info = item.lstat()
        except FileNotFoundError:
            result[str(item.relative_to(path))] = None
            return
        key = str(item.relative_to(path))
        reject_redirect(item, info)
        if stat.S_ISDIR(info.st_mode):
            payload = None
            for child in sorted(item.iterdir()):
                visit(child)
        elif stat.S_ISREG(info.st_mode):
            payload = hashlib.sha256(item.read_bytes()).hexdigest()
        else:
            raise AssertionError(f"unsupported protected entry: {item}")
        # Reading may change atime; content, identity, mode and mtime must stay.
        result[key] = (info.st_dev, info.st_ino, info.st_mode, info.st_size,
                       info.st_mtime_ns, payload)

    visit(path)
    return result


def protected_snapshot_controls():
    """Prove refusal before trusting a snapshot, using only disposable targets."""
    with tempfile.TemporaryDirectory(prefix="anamorph-snapshot-controls-") as directory:
        base = Path(directory).resolve()
        target, other = base / "target", base / "outside-approved-boundary"
        target.mkdir()
        other.mkdir()
        content = target / "preset"
        content.write_text("before", encoding="utf-8")
        before = protected_snapshot(target)
        assert protected_snapshot(target) == before
        content.write_text("after", encoding="utf-8")
        assert protected_snapshot(target) != before, "regular target mutation went undetected"
        content.write_text("before", encoding="utf-8")

        link = base / "link"
        junction = False

        def redirect(destination):
            nonlocal junction
            try:
                link.symlink_to(destination, target_is_directory=True)
                junction = False
            except OSError:
                if os.name != "nt":
                    raise
                # Hosted Windows may lack symbolic-link privilege. A directory
                # junction exercises the same fail-closed reparse-point rule.
                subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(destination)],
                               check=True, capture_output=True, text=True)
                junction = True

        def unlink():
            if junction:
                link.rmdir()
            else:
                link.unlink()

        def refused(path):
            try:
                protected_snapshot(path)
            except AssertionError as error:
                assert "unsupported protected redirect" in str(error), error
            else:
                raise AssertionError("protected redirect was accepted")

        redirect(target)
        identity = link.lstat()
        refused(link)                         # unchanged link and target
        content.write_text("linked mutation", encoding="utf-8")
        assert link.lstat().st_mtime_ns == identity.st_mtime_ns
        refused(link)                         # unchanged link, changed target
        refused(link / "preset")              # ancestor redirect
        refused(base)                         # descendant redirect
        unlink()
        redirect(other)
        refused(link)                         # changed link / outside boundary
        unlink()
        redirect(target)
        content.unlink()
        target.rmdir()
        refused(link)                         # broken link still fails closed
        unlink()


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
                root = Path(line.strip().split("=", 1)[1])
                paths = dict(entry.strip().split("=", 1) for entry in output if entry.startswith("STATE_ISOLATION_"))
                messages.put((root, paths))
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
    try:
        assert Path(output["STATE_ISOLATION_PRESETS"]) == root / "presets"
        assert Path(output["STATE_ISOLATION_TEMP"]) == root / "temp"
    except BaseException:
        proc.kill()
        proc.communicate(timeout=90)
        raise
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
    protected_snapshot_controls()
    command = sys.argv[1:]
    if command == ["--self-test"]:
        return
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
        witness = subprocess.run(command + ["--filesystem-path-witness"], env=env,
                                 capture_output=True, text=True, encoding="utf-8", timeout=90, check=True)
        paths = json.loads(witness.stdout)
        product = Path(paths["productionPresets"])
        assert product == Path(paths["applicationData"]) / "RollyTech" / "Anamorph" / "Presets"
        assert Path(paths["temporaryParent"]).resolve().is_relative_to(tmp.resolve()), paths
        assert not product.resolve().is_relative_to(tmp.resolve()), paths
        assert not tmp.resolve().is_relative_to(product.resolve()), paths
        if os.name == "nt":
            # Independent read-only shell API witness, not an APPDATA/HOME guess.
            import ctypes
            buffer = ctypes.create_unicode_buffer(32768)
            status = ctypes.windll.shell32.SHGetFolderPathW(None, 0x001a, None, 0, buffer)
            assert status == 0 and buffer.value, f"CSIDL_APPDATA resolution failed: {status}"
            assert Path(paths["applicationData"]).resolve() == Path(buffer.value).resolve(), paths
        protected = protected_snapshot(product)
        print("Production path witness: " + json.dumps(paths), flush=True)
        try:
            overlap = subprocess.run(command + ["--test-overlapping-isolation-root"], env=env,
                                     capture_output=True, text=True, timeout=90)
            assert overlap.returncode == 2 and "cannot establish" in overlap.stderr, overlap
            bypass = subprocess.run(command + ["--test-bypass-isolation-override"], env=env,
                                    capture_output=True, text=True, timeout=90)
            assert bypass.returncode == 2 and "override is not active" in bypass.stderr, bypass
            assert not list(tmp.rglob("anamorph-state-*")), "bypass control left a profile"
            omitted = subprocess.run(command + ["--filesystem-isolation-probe", "omit-return-load"], env=env,
                                     capture_output=True, text=True, timeout=90)
            assert omitted.returncode == 3 and "return load failed" in omitted.stderr, omitted
            assert "STATE_ISOLATION_READY=" not in omitted.stdout, omitted
            assert not list(tmp.rglob("anamorph-state-*")), "return-load control left a profile"
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
            for root in (root1, root2):
                temporary = list((root / "temp").glob("*.probe"))
                assert len(temporary) == 1 and temporary[0].read_text() == "isolated temporary file"
            finish(first)
            finish(second)
            assert not root1.exists() and not root2.exists(), "normal cleanup left a profile"

            disconnected, disconnected_root = ready_process(command, env)
            processes.append(disconnected)
            disconnected.stdin.close()
            disconnected.stdin = None
            _, error = disconnected.communicate(timeout=90)
            assert disconnected.returncode == 3 and "isolation probe release failed" in error
            assert not disconnected_root.exists(), "disconnected probe left a profile"

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
            assert protected_snapshot(product) == protected, "resolved production preset tree changed"
        finally:
            for proc in processes:
                if proc.poll() is None:
                    proc.kill()
                    proc.communicate(timeout=90)
    print("State filesystem isolation: path witnesses, protected snapshot, A-B-C-B/omitted return, override bypass, overlap, concurrent roots, fail-closed setup, disconnected release, replacement and interruption controls passed")


if __name__ == "__main__":
    main()
