#!/usr/bin/env python3
"""Linux/Clang ASan fault control for the nonshipping benchmark's actual owner scope.

Uses the existing built Ninja target, never edits tracked files. A committed
reader finishes after the destructor stop signal. An injected producer exception
must join it before engine storage is released; the old owner-scope mutation
must produce a real ASan heap-use-after-free. This is not performance evidence.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys

import native_validation as n


def fault_header(source, invalid_owner):
    if invalid_owner:
        owner = "        std::unique_ptr<anamorph::AnamorphEngine> engine;\n"
        allocation = "            engine = std::make_unique<anamorph::AnamorphEngine>();"
        if source.count(owner) != 1 or source.count(allocation) != 1:
            raise ValueError("owner-scope mutation no longer matches; review the control")
        source = source.replace(owner, "").replace(allocation, allocation.replace("engine =", "auto engine ="))
    begin = source.index("        worker = std::thread")
    end = source.index("        });", begin) + len("        });")
    worker = """        worker = std::thread ([this, &scope, a = std::move (vectorscope), b = std::move (spectrum)]
        {
            // Deterministic pending-read completion at the shutdown boundary.
            while (! stop.load (std::memory_order_acquire)) std::this_thread::yield();
            if (a->accept (a->readLatest (scope, 8000))) ++accepted;
            if (b->accept (b->readLatest (scope, 8192))) ++accepted;
        });"""
    source = source[:begin] + worker + source[end:]
    start = "                reader.start (engine->getScopeBuffer(), o.reader);"
    if source.count(start) != 1:
        raise ValueError("fault injection no longer matches; review the control")
    return source.replace(start, start + '\n                throw std::runtime_error ("injected reader lifetime failure");')


def run(build, output):
    if sys.platform != "linux" or output.is_relative_to(n.ROOT):
        raise ValueError("use Linux/Clang and an evidence directory outside the checkout")
    cache = n.cache_values(build)
    if (cache.get("CMAKE_HOME_DIRECTORY") != str(n.ROOT) or cache.get("CMAKE_GENERATOR") != "Ninja"
            or "clang" not in Path(cache.get("CMAKE_CXX_COMPILER", "")).name):
        raise ValueError("require this checkout's built Clang/Ninja benchmark")
    output.mkdir(parents=True, exist_ok=False)
    sanitizer_options = "detect_leaks=1:halt_on_error=1:symbolize=1"
    state = {"state": "started", "classification": "synthetic failure control, not performance evidence",
             "sanitizer_options": sanitizer_options, "results": [],
             "source_commit": n.required(["git", "rev-parse", "HEAD"], n.ROOT),
             "source_header_sha256": n.digest(n.ROOT / "tests/native_validation_bench.h"),
             "control_script_sha256": n.digest(Path(__file__))}
    n.save(output / "results.json", state)
    try:
        import json
        database = json.loads(n.required(["ninja", "-C", str(build), "-t", "compdb", "-x"]))
        def find(predicate):
            found = [x for x in database if predicate(x)]
            if len(found) != 1:
                raise ValueError("missing/ambiguous built benchmark command")
            return n.command_args(found[0]["command"])
        compile_base = find(lambda x: "AnamorphBench.dir" in x.get("output", "") and x.get("file", "").endswith("tests/bench.cpp"))
        link_base = find(lambda x: bool(x.get("command")) and Path(x.get("output", "")).name == "AnamorphBench")
        source = (n.ROOT / "tests/native_validation_bench.h").read_text(encoding="utf-8")
        for variant, invalid in (("fixed", False), ("invalid-owner", True)):
            directory = output / "original-overlay" / variant
            (directory / "tests").mkdir(parents=True)
            (directory / "src").symlink_to(n.ROOT / "src", target_is_directory=True)
            (directory / "tests/bench.cpp").write_bytes((n.ROOT / "tests/bench.cpp").read_bytes())
            (directory / "tests/native_validation_bench.h").write_text(fault_header(source, invalid), encoding="utf-8")
            compile = compile_base.copy()
            old_object = n.output_path(compile)
            index = next(i for i, x in enumerate(compile) if x.endswith("/tests/bench.cpp"))
            compile[index] = str(directory / "tests/bench.cpp")
            obj = directory / "bench.o"
            compile = n.with_output(compile, obj)
            compile[1:1] = ["-fsanitize=address", "-fno-omit-frame-pointer"]
            n.run_logged(compile, build, output / (variant + ".compile.stdout.log"), output / (variant + ".compile.stderr.log"))
            link = link_base.copy()
            if link.count(old_object) != 1:
                raise ValueError("owned benchmark object must appear exactly once")
            link[link.index(old_object)] = str(obj)
            exe = directory / "AnamorphBench-lifetime"
            link = n.with_output(link, exe)
            link[1:1] = ["-fsanitize=address", "-fno-omit-frame-pointer"]
            n.save(output / (variant + ".commands.json"), {"compile": compile, "link": link})
            n.run_logged(link, build, output / (variant + ".link.stdout.log"), output / (variant + ".link.stderr.log"))
            env = dict(os.environ, ASAN_OPTIONS=sanitizer_options)
            result = subprocess.run([str(exe), "--native-json", "--kind", "engine", "--reader", "active", "--paced", "--smoke", "--seconds", "1.1"],
                                    capture_output=True, text=True, timeout=30, env=env)
            (output / (variant + ".stdout.log")).write_text(result.stdout, encoding="utf-8")
            (output / (variant + ".stderr.log")).write_text(result.stderr, encoding="utf-8")
            uaf = "heap-use-after-free" in result.stderr
            valid = (result.returncode != 0 and uaf) if invalid else (
                result.returncode == 2 and "injected reader lifetime failure" in result.stderr and "AddressSanitizer" not in result.stderr)
            state["results"].append({"variant": variant, "exit": result.returncode, "heap_use_after_free": uaf,
                                     "header_sha256": n.digest(directory / "tests/native_validation_bench.h"), "binary_sha256": n.digest(exe)})
            n.save(output / "results.json", state)
            if not valid or result.stdout:
                raise RuntimeError("lifetime control did not discriminate; retain and inspect raw logs")
        state["state"] = "completed"
    except BaseException as error:
        state["state"] = "failed"
        state["error"] = str(error)
        raise
    finally:
        n.save(output / "results.json", state)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    run(args.build_dir.resolve(), args.output_dir.resolve())
