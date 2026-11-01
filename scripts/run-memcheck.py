#!/usr/bin/env python3
"""Run both existing Valgrind suites concurrently, retaining every exit status."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile

OPTIONS = ["--tool=memcheck", "--track-origins=yes", "--leak-check=no",
           "--error-exitcode=1", "--error-limit=no", "--num-callers=25"]


def run_suites(prefix, binaries, directory):
    directory.mkdir(parents=True, exist_ok=True)

    def run_one(label, binary):
        command = [*prefix, *OPTIONS, str(binary)]
        log_path = directory / f"{label}.log"
        try:
            with log_path.open("wb") as log:
                print(f"memcheck {label}: {json.dumps(command)}", flush=True)
                with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT) as child:
                    for line in child.stdout:
                        log.write(line)
                        log.flush()
                        print(f"[{label}] {line.decode('utf-8', errors='replace').rstrip()}", flush=True)
                    code = child.wait()
            return {"suite": label, "command": command, "exit_code": code, "log": log_path.name}
        except OSError as error:
            print(f"::error::memcheck {label} could not run: {error}", flush=True)
            return {"suite": label, "command": command, "exit_code": None, "error": str(error)}

    # Joining both futures is mandatory even if the first suite fails. The
    # commands/options are unchanged; no result is inferred from log text.
    with ThreadPoolExecutor(max_workers=2) as pool:
        futures = [pool.submit(run_one, label, binary) for label, binary in binaries]
        results = [future.result() for future in futures]
    (directory / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    failed = any(result["exit_code"] != 0 for result in results)
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
            with contextlib.redirect_stdout(io.StringIO()):
                code = run_suites([sys.executable, str(stub), str(left), str(right), str(case)], binaries, case)
            assert code == int(left != 0 or right != 0), (left, right, code)
            report = json.loads((case / "results.json").read_text())
            assert [r["exit_code"] for r in report] == [left, right], report
            for label, _ in binaries:
                assert (case / (label + ".finished")).exists(), label
                output = (case / (label + ".log")).read_text()
                assert "child " + label + " finished" in output
                assert "child " + label + " stderr" in output
        # An unstartable validator is an infrastructure failure, never green.
        with contextlib.redirect_stdout(io.StringIO()):
            assert run_suites([str(root / "missing-validator")], binaries, root / "missing") == 1
        missing = subprocess.run([sys.executable, __file__, "--build-dir", str(root / "absent")],
                                 capture_output=True, text=True)
        assert missing.returncode == 2 and "required suite binaries missing" in missing.stderr
    print("memcheck runner: 4 overlap/status, missing-validator and missing-binaries controls passed")


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
