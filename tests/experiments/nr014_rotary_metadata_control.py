#!/usr/bin/env python3
"""Run the NR-014 regression against current and original editor objects.

Nonshipping Linux/Ninja control. Reuse the built state target's actual flags,
tests and other objects; compile a pinned original editor in temporary storage.
No checkout source, CMake target, focus policy or assertion is rewritten.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def command_args(line):
    args = shlex.split(line)
    if args[:2] == [":", "&&"]:
        args = args[2:]
    if args[-2:] == ["&&", ":"]:
        args = args[:-2]
    if any(x in ("&&", ";", "||", "|") for x in args):
        raise ValueError("unexpected compound build command")
    return args


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--baseline", default="425539385ebfcd1ca2bab1befe3de37618ce02d2")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    build, output = args.build_dir.resolve(), args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    baseline = subprocess.check_output(
        ["git", "rev-parse", "--verify", args.baseline + "^{commit}"], cwd=root, text=True).strip()
    original = subprocess.check_output(
        ["git", "show", baseline + ":src/PluginEditor.cpp"], cwd=root)
    with (output / "build.log").open("w") as log:
        subprocess.run(["cmake", "--build", str(build), "--target", "AnamorphStateTests", "-j", "3"],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
        lines = subprocess.check_output(
            ["ninja", "-C", str(build), "-t", "commands", "AnamorphStateTests"], text=True).splitlines()
        matches = [command_args(x) for x in lines if "src/PluginEditor.cpp" in x and " -c " in x]
        if len(matches) != 1:
            raise ValueError("require exactly one editor translation-unit command")
        compile_args, link = matches[0], command_args(lines[-1])
        editor_object = compile_args[compile_args.index("-o") + 1]
        current_binary = build / link[link.index("-o") + 1]
        results = {}
        with tempfile.TemporaryDirectory(prefix="nr014-control-") as temporary:
            temp = Path(temporary)
            source, obj, binary = temp / "PluginEditor.cpp", temp / "editor.o", temp / "original"
            source.write_bytes(original)
            compile_args[compile_args.index("-c") + 1] = str(source)
            compile_args[compile_args.index("-o") + 1] = str(obj)
            for flag, name in (("-MF", "editor.d"), ("-MT", "editor.o")):
                if flag in compile_args:
                    compile_args[compile_args.index(flag) + 1] = str(temp / name)
            subprocess.run(compile_args, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
            if link.count(editor_object) != 1:
                raise ValueError("require exactly one editor object in the state link")
            link[link.index(editor_object)] = str(obj)
            link[link.index("-o") + 1] = str(binary)
            link = ["--dependency-file=" + str(temp / "link.d")
                    if x.startswith("--dependency-file=") else x for x in link]
            subprocess.run(link, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
            for name, executable in (("original", binary), ("corrected", current_binary)):
                env = dict(os.environ, XDG_CONFIG_HOME=str(temp / (name + "-config")), MALLOC_PERTURB_="1")
                with (output / (name + ".log")).open("w") as run_log:
                    result = subprocess.run([str(executable), "--gui-accessibility-probe"],
                                            env=env, stdout=run_log, stderr=subprocess.STDOUT)
                text = (output / (name + ".log")).read_text()
                summary = re.findall(r"(\d+) checks, (\d+) failure\(s\)", text)
                if len(summary) != 1:
                    raise ValueError(name + ": missing or ambiguous regression summary")
                failures = [x.strip() for x in text.splitlines() if "[FAIL]" in x]
                results[name] = {"exit": result.returncode, "checks": int(summary[0][0]),
                                 "failures": int(summary[0][1]), "failed_assertions": failures}
        hashes = {p: hashlib.sha256((root / p).read_bytes()).hexdigest()
                  for p in ("src/PluginEditor.cpp", "tests/state_tests.cpp", "tests/gui_accessibility_tests.h")}
        record = {"baseline": baseline, "baseline_editor_sha256": hashlib.sha256(original).hexdigest(),
                  "current_sha256": hashes, "results": results}
        (output / "metadata.json").write_text(json.dumps(record, indent=2) + "\n")
        old, fixed = results["original"], results["corrected"]
        expected = "[FAIL] GUI metadata: rotary accessibility title matches its existing caption"
        if not (old["exit"] == 1 and old["failures"] == 36
                and old["failed_assertions"] == [expected] * 36):
            raise ValueError("original control must fail only the 12 titles in three editor lifetimes")
        if not (fixed["exit"] == 0 and fixed["failures"] == 0 and not fixed["failed_assertions"]
                and fixed["checks"] == old["checks"] and fixed["checks"] > 36):
            raise ValueError("corrected object must pass the same non-vacuous regression")
    print(f"original {old['failures']}/{old['checks']} failed; corrected {fixed['checks']} checks passed")


if __name__ == "__main__":
    main()
