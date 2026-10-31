#!/usr/bin/env python3
"""Build the nonshipping NR-012 probe with an existing Ninja DSP-dump build.

Reuses that target's compiled engine/JUCE objects and actual flags. No CMake,
product target or source file is rewritten. Linux/Clang research utility only.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess


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
    args = parser.parse_args()
    build, output = args.build_dir.resolve(), args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parent.parent
    source = root / "tests/experiments/nr012_aliasing_study.cpp"
    subprocess.run(["cmake", "--build", str(build), "--target", "AnamorphDspDump", "-j", "3"], check=True)
    lines = subprocess.check_output(["ninja", "-C", str(build), "-t", "commands", "AnamorphDspDump"], text=True).splitlines()
    matches = [command_args(x) for x in lines if "tests/dsp_dump.cpp" in x and " -c " in x]
    if len(matches) != 1:
        raise ValueError("require exactly one DSP-dump translation-unit command")
    compile_args = matches[0]
    old_source = compile_args[compile_args.index("-c") + 1]
    old_object = compile_args[compile_args.index("-o") + 1]
    compile_args[compile_args.index(old_source)] = str(source)
    compile_args[compile_args.index("-o") + 1] = str(output / "study.o")
    for flag, filename in (("-MF", "study.o.d"), ("-MT", "study.o")):
        if flag in compile_args:
            compile_args[compile_args.index(flag) + 1] = str(output / filename)
    link_args = command_args(lines[-1])
    if link_args.count(old_object) != 1 or "-o" not in link_args:
        raise ValueError("require the original dump object exactly once in the link")
    link_args[link_args.index(old_object)] = str(output / "study.o")
    link_args[link_args.index("-o") + 1] = str(output / "alias-study")
    link_args = ["--dependency-file=" + str(output / "study-link.d")
                 if x.startswith("--dependency-file=") else x for x in link_args]
    (output / "commands.json").write_text(json.dumps({"compile": compile_args, "link": link_args}, indent=2) + "\n")
    subprocess.run(compile_args, cwd=build, check=True)
    subprocess.run(link_args, cwd=build, check=True)
    with (output / "measurements.csv").open("w") as report:
        subprocess.run([str(output / "alias-study")], stdout=report, check=True)
    print(output / "measurements.csv")


if __name__ == "__main__":
    main()
