#!/usr/bin/env python3
"""Run both existing Valgrind suites concurrently, retaining every exit status."""
import argparse
from concurrent.futures import ThreadPoolExecutor, wait
import contextlib
import io
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import threading
import time

OPTIONS = ["--tool=memcheck", "--track-origins=yes", "--leak-check=no",
           "--error-exitcode=1", "--error-limit=no", "--num-callers=25"]


def run_suites(prefix, binaries, directory):
    directory.mkdir(parents=True, exist_ok=True)
    output_lock = threading.Lock()
    state_lock = threading.Lock()
    children = {}
    results = [{"suite": label, "command": [*prefix, *OPTIONS, str(binary)],
                "exit_code": None, "log": f"{label}.log", "state": "pending"}
               for label, binary in binaries]
    interrupted = 0

    def checkpoint():
        # Called with state_lock held. Readers/upload never see half-written JSON.
        temporary = directory / "results.json.tmp"
        temporary.write_text(json.dumps(results, indent=2) + "\n")
        temporary.replace(directory / "results.json")

    def cancel(signum, _frame):
        nonlocal interrupted
        interrupted = interrupted or signum

    checkpoint()
    previous_handlers = {sig: signal.signal(sig, cancel) for sig in (signal.SIGINT, signal.SIGTERM)}

    def emit(message):
        # One complete line at a time, including GitHub workflow commands.
        with output_lock:
            print(message, flush=True)

    def run_one(result):
        label, command = result["suite"], result["command"]
        log_path = directory / result["log"]
        try:
            with log_path.open("wb") as log:
                emit(f"memcheck {label}: {json.dumps(command)}")
                with state_lock:
                    if interrupted:
                        result.update(state="interrupted", interrupted_by=signal.Signals(interrupted).name)
                        checkpoint()
                        return
                with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                      start_new_session=(os.name == "posix")) as child:
                    with state_lock:
                        children[label] = child
                        result.update(state="running", pid=child.pid)
                        checkpoint()
                    for line in child.stdout:
                        log.write(line)
                        log.flush()
                        text = line.decode("utf-8", errors="replace").rstrip("\r\n")
                        # Runner v2 commands must begin the line after whitespace.
                        # Prefixing coverage warnings would hide their annotations.
                        if line.lstrip().startswith((b"::", b"##[")):
                            emit(text)
                        else:
                            emit(f"[{label}] {text}")
                    code = child.wait()
            with state_lock:
                result.update(exit_code=code, state="interrupted" if "interrupted_by" in result else "completed")
                checkpoint()
        except OSError as error:
            emit(f"::error::memcheck {label} could not run: {error}")
            with state_lock:
                result.update(state="start_failed", error=str(error))
                checkpoint()

    # Joining both futures is mandatory even if the first suite fails. The
    # commands/options are unchanged; no result is inferred from log text.
    try:
        with ThreadPoolExecutor(max_workers=2) as pool:
            futures = [pool.submit(run_one, result) for result in results]
            pending = set(futures)
            termination_started = None
            terminated = set()
            killed = set()
            while pending:
                _, pending = wait(pending, timeout=0.05)
                if interrupted:
                    termination_started = termination_started or time.monotonic()
                    with state_lock:
                        for result in results:
                            label = result["suite"]
                            child = children.get(label)
                            if child is None or result["state"] == "completed":
                                continue
                            if label not in terminated:
                                result["interrupted_by"] = signal.Signals(interrupted).name
                                terminated.add(label)
                                sig = signal.SIGTERM
                            elif label not in killed and time.monotonic() - termination_started >= 1:
                                # A child/grandchild may ignore TERM or retain the
                                # pipe after its parent exits. Kill the whole group.
                                killed.add(label)
                                sig = signal.SIGKILL
                            else:
                                continue
                            try:
                                if os.name == "posix":
                                    os.killpg(child.pid, sig)
                                elif sig == signal.SIGTERM:
                                    child.terminate()
                                else:
                                    child.kill()
                            except ProcessLookupError:
                                pass
                        checkpoint()
            for future in futures:
                future.result() # propagate infrastructure exceptions after joining both
    finally:
        for sig, handler in previous_handlers.items():
            signal.signal(sig, handler)
    failed = bool(interrupted) or any(result["state"] != "completed" or result["exit_code"] != 0
                                     for result in results)
    if failed:
        print("::error::memcheck suite results: " + json.dumps(results), flush=True)
    return int(failed)


def self_test():
    # A child cannot finish successfully until BOTH start. This proves actual
    # overlap, not just two sequential calls with successful exit codes.
    # Frozen argv from 00de8c7's serial CI step, independent of OPTIONS above.
    legacy_options = ["--tool=memcheck", "--track-origins=yes", "--leak-check=no",
                      "--error-exitcode=1", "--error-limit=no", "--num-callers=25"]
    stub_source = '''import json,sys,time
from pathlib import Path
left,right,root=sys.argv[1:4]
root=Path(root)
assert sys.argv[4:-1]==json.loads(EXPECTED_OPTIONS)
label='state' if 'State' in sys.argv[-1] else 'dsp'
(root/(label+'.started')).touch()
other='dsp' if label=='state' else 'state'
deadline=time.monotonic()+5
while not (root/(other+'.started')).exists():
 if time.monotonic()>deadline:sys.exit(90)
 time.sleep(.01)
print('::warning::seeded-'+label+'-annotation')
print('child '+label+' stderr', file=sys.stderr)
print('child '+label+' finished')
(root/(label+'.finished')).touch()
sys.exit(int(right if label=='state' else left))
'''.replace("EXPECTED_OPTIONS", repr(json.dumps(legacy_options)))
    with tempfile.TemporaryDirectory(prefix="anamorph-memcheck-control-") as temporary:
        root = Path(temporary)
        stub = root / "validator.py"
        stub.write_text(stub_source)
        for index, (left, right) in enumerate([(0, 0), (31, 0), (0, 32), (31, 32)]):
            case = root / str(index)
            case.mkdir()
            binaries = [("dsp", case / "AnamorphTests"), ("state", case / "AnamorphStateTests")]
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                code = run_suites([sys.executable, str(stub), str(left), str(right), str(case)], binaries, case)
            assert code == int(left != 0 or right != 0), (left, right, code)
            report = json.loads((case / "results.json").read_text())
            assert [r["exit_code"] for r in report] == [left, right], report
            assert all(r["state"] == "completed" for r in report), report
            for label, _ in binaries:
                assert (case / (label + ".finished")).exists(), label
                raw = (case / (label + ".log")).read_text()
                assert "child " + label + " finished" in raw
                assert "child " + label + " stderr" in raw
                annotation = "::warning::seeded-" + label + "-annotation"
                assert annotation in raw
                assert annotation in output.getvalue().splitlines(), output.getvalue()
        # An unstartable validator is an infrastructure failure, never green.
        with contextlib.redirect_stdout(io.StringIO()):
            assert run_suites([str(root / "missing-validator")], binaries, root / "missing") == 1
        missing = subprocess.run([sys.executable, __file__, "--build-dir", str(root / "absent")],
                                 capture_output=True, text=True)
        assert missing.returncode == 2 and "required suite binaries missing" in missing.stderr
        if os.name == "posix":
            cancellation_self_test(root)
    print("memcheck runner: overlap/status, missing inputs and interruption/checkpoint controls passed")


def cancellation_self_test(root):
    # Real parent signals, not mocked return values: one completed suite must
    # survive cancellation of its still-running peer. TERM-ignoring children
    # exercise bounded KILL escalation; the raw partial logs must already exist.
    stub = root / "interrupt-validator.py"
    stub.write_text('''import os,signal,sys,time
from pathlib import Path
root=Path(sys.argv[1])
label='state' if 'State' in sys.argv[-1] else 'dsp'
if label=='state':
 signal.signal(signal.SIGTERM, signal.SIG_IGN)
 signal.signal(signal.SIGINT, signal.SIG_IGN)
print('partial '+label, flush=True)
(root/(label+'.started')).write_text(str(os.getpid()))
if label=='dsp': sys.exit(0)
while True: time.sleep(.05)
''')
    driver = '''import runpy,sys
from pathlib import Path
runner,stub,root=sys.argv[1:]
root=Path(root)
binaries=[('dsp',root/'AnamorphTests'),('state',root/'AnamorphStateTests')]
sys.exit(runpy.run_path(runner)['run_suites']([sys.executable,stub,str(root)],binaries,root))
'''
    for sig in (signal.SIGINT, signal.SIGTERM):
        case = root / sig.name
        case.mkdir()
        with subprocess.Popen([sys.executable, "-c", driver, str(Path(__file__).resolve()),
                               str(stub), str(case)], stdout=subprocess.DEVNULL,
                              stderr=subprocess.PIPE) as parent:
            try:
                deadline = time.monotonic() + 5
                while True:
                    path = case / "results.json"
                    report = json.loads(path.read_text()) if path.exists() else []
                    if (len(report) == 2 and report[0]["state"] == "completed"
                            and report[1]["state"] == "running" and (case / "state.started").exists()):
                        break
                    assert parent.poll() is None and time.monotonic() < deadline, report
                    time.sleep(.01)
                assert report[0]["exit_code"] == 0 and report[1]["exit_code"] is None, report
                assert "partial state" in (case / "state.log").read_text()
                parent.send_signal(sig)
                _, error = parent.communicate(timeout=4)
                assert parent.returncode != 0, error
                report = json.loads(path.read_text())
                assert report[0]["state"] == "completed" and report[0]["exit_code"] == 0, report
                assert (report[1]["state"] == "interrupted" and report[1]["exit_code"] == -signal.SIGKILL
                        and report[1]["interrupted_by"] == sig.name), report
                for result in report:
                    try:
                        os.kill(result["pid"], 0)
                    except ProcessLookupError:
                        pass
                    else:
                        raise AssertionError(f"unreaped validator: {result}")
            finally:
                if parent.poll() is None:
                    parent.kill()
                    parent.wait()
                # Also clean up if a broken/mutated runner loses its children.
                path = case / "results.json"
                for result in json.loads(path.read_text()) if path.exists() else []:
                    if result.get("pid"):
                        try:
                            os.killpg(result["pid"], signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                for marker in case.glob("*.started"):
                    try:
                        os.kill(int(marker.read_text()), signal.SIGKILL)
                    except ProcessLookupError:
                        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--build-dir", type=Path, default=Path("build-vg"))
    parser.add_argument("--log-dir", type=Path, default=Path("build/valgrind-logs"))
    parser.add_argument("--valgrind", default="valgrind")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    binaries = [(label, args.build_dir / f"{name}_artefacts/Release/{name}")
                for label, name in [("dsp", "AnamorphTests"), ("state", "AnamorphStateTests")]]
    missing = [str(path) for _, path in binaries if not path.is_file()]
    if missing:
        parser.error("required suite binaries missing: " + ", ".join(missing))
    return run_suites([args.valgrind], binaries, args.log_dir)


if __name__ == "__main__":
    sys.exit(main())
