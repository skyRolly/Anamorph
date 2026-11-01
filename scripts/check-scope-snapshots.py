#!/usr/bin/env python3
"""Deterministic original/fixed ScopeBuffer regression plus live mutant control.

Historical source is hash-verified and gets scheduling hooks only. All
interleavings here execute on one thread, so the old race is never invoked as UB.
The separately instrumented state probe supplies genuine concurrent validation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def instrument(raw: str) -> str:
    for needle, after in (("            dstL[i] = left [idx];", "false"),
                          ("            dstR[i] = right[idx];", "true")):
        if raw.count(needle) != 1:
            raise ValueError("legacy scheduling seam missing or ambiguous")
        raw = raw.replace(needle, needle + f"\n            LEGACY_SCOPE_READ_HOOK(i, {after});", 1)
    publication = "        write.store (end, std::memory_order_release);"
    if raw.count(publication) != 1:
        raise ValueError("legacy publication seam missing or ambiguous")
    raw = raw.replace(publication, "        LEGACY_SCOPE_WRITE_HOOK();\n" + publication, 1)
    return raw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if not args.self_test:
        parser.error("run --self-test (regression and liveness execute together)")
    root = Path(__file__).resolve().parent.parent
    fixture = root / "tests/fixtures/scope-legacy/ScopeBuffer.h"
    raw = fixture.read_bytes()
    metadata = json.loads(fixture.with_name("provenance.json").read_text())
    blob = hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()
    assert blob == metadata["git_blob"] == "ed24ceab5ced2784d9fe96effdc46f8aeb03f472", "legacy source changed"
    assert hashlib.sha256(raw).hexdigest() == metadata["sha256"]
    generated = instrument(raw.decode())
    assert generated.count("LEGACY_SCOPE_READ_HOOK") == 2
    assert generated.count("LEGACY_SCOPE_WRITE_HOOK") == 1
    for bad in ("", raw.decode().replace("dstR[i] = right[idx];", "changed;")):
        try:
            instrument(bad)
        except ValueError:
            pass
        else:
            raise AssertionError("instrumentation accepted a broken seam")
    source = root / "src/dsp/ScopeBuffer.h"
    current = source.read_text()
    guard = "if (slot.tag.load (std::memory_order_acquire) != pos + 1) return { false, 0, end };"
    assert current.count(guard) == 2, "current validation seams changed; review the regression"
    cxx = shlex.split(os.environ.get("CXX", "c++"))
    with tempfile.TemporaryDirectory(prefix="anamorph-scope-regression-") as directory:
        work = Path(directory)
        old = work / "original-instrumented.h"
        old.write_text(generated)
        mutant = work / "current-without-validation.h"
        mutant.write_text(current.replace(guard, "if (false) return { false, 0, end };"))

        def build(header, exe):
            subprocess.run(cxx + ["-std=c++23", "-O2", "-pthread",
                "-DANAMORPH_OLD_SCOPE_HEADER=" + json.dumps(str(old)),
                "-DANAMORPH_CURRENT_SCOPE_HEADER=" + json.dumps(str(header)),
                str(root / "tests/scope_legacy_snapshot_control.cpp"), "-o", str(exe)], check=True)

        exe = work / "regression"
        build(source, exe)
        for mode, code, failures in (("old", 1, 3), ("new", 0, 0)):
            result = subprocess.run([str(exe), "--" + mode], capture_output=True, text=True, timeout=20)
            assert result.returncode == code and f"cases=3, failures={failures}" in result.stdout, result
            if mode == "old":
                assert result.stderr.count("wrong committed positions: old") == 3, result
            print(result.stdout.strip())
        build(mutant, exe)
        result = subprocess.run([str(exe), "--new"], capture_output=True, text=True, timeout=20)
        assert result.returncode == 1 and "cases=3, failures=3" in result.stdout, result
        assert result.stderr.count("wrong committed positions: new") == 3, result
        invalidation = "slot.tag.store (0, std::memory_order_release);"
        assert current.count(invalidation) == 1
        mutant.write_text(current.replace(invalidation, "/* seeded missing invalidation */", 1))
        build(mutant, exe)
        result = subprocess.run([str(exe), "--new"], capture_output=True, text=True, timeout=20)
        assert result.returncode == 1 and "cases=3, failures=1" in result.stdout, result
        assert "wrong committed positions: new in-flight" in result.stderr, result
        print("scope snapshot self-test: legacy3/3 failures; fixed3/3 pass; validation and invalidation mutants rejected")


if __name__ == "__main__":
    main()
