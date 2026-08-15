#!/usr/bin/env python3
"""
Anamorph -- source portability lint (Linux-runnable, guards a macOS-only defect class).

PROVENANCE
==========
Adopted from the sibling product Anabasis (`scripts/check-portability.py`).  The
SIMD lint and the compile canary are unchanged: the hazard is a property of JUCE
and of the platform typedefs, not of either product, and this repository builds
the same pinned-JUCE `juce_dsp` on the same three platforms.  The second check
(`scratch_names_agree`) is re-derived against THIS repository's installer pair --
see its own docstring for what changed and why, because the two products'
uninstallers do not have the same shape.

This tree is CLEAN of the hazard today, so the lint lands as a regression guard
rather than a repair.  That is the point: the defect class it guards is
structurally invisible to every Linux compiler, and this project's Linux jobs are
its fast lane.

WHY THIS EXISTS
===============
In the sibling repository, between 2026-08-08 and 2026-08-10, the macOS CI job
failed to COMPILE on every push while Linux and Windows stayed green.  One line
was responsible:

    return std::sqrt (s / juce::jmax<size_t> (1, v.size()));     tests/state_tests.cpp

`juce::jmax` is not one function.  juce_dsp declares an extra overload

    template <typename Type>
    dsp::SIMDRegister<Type> jmax (dsp::SIMDRegister<Type>, dsp::SIMDRegister<Type>);
                                     -- juce_dsp/containers/juce_SIMDRegister_Impl.h:185

Supplying an EXPLICIT template argument substitutes it into every candidate, so
the compiler must form `SIMDRegister<size_t>` -- and completing that class needs
`SIMDNativeOps<size_t>`, which exists only if `size_t` happens to name one of the
ten types JUCE specialises (int8_t/uint8_t/int16_t/uint16_t/int32_t/uint32_t/
int64_t/uint64_t/float/double).  Parameters of incomplete class type must be
completed, and that completion is OUTSIDE the immediate context, so it is a hard
error rather than a quiet SFINAE removal.

    Linux (LP64, glibc):  uint64_t IS `unsigned long` IS size_t  -> complete   -> compiles
    macOS (LP64, libc++): uint64_t is `unsigned long long`,
                          size_t is `unsigned long`              -> incomplete -> ERROR

The defect is therefore INVISIBLE to a Linux compiler no matter which compiler it
is -- GCC and Clang agree, because the typedef, not the front end, is what
differs.  That is precisely why it needs a lint rather than another build job.

WHAT IT CHECKS
==============
1. lint (default)   Rejects an explicit template argument on any juce function
                    template that juce_dsp also overloads for SIMDRegister.
                    That set is CLOSED at {jmin, jmax, snapToZero} -- verified by
                    sweeping every JUCE module header for free functions taking
                    SIMDRegister<T>.  jlimit / jmap / findMinimum / findMaximum /
                    approximatelyEqual / exactlyEqual / roundToInt have NO such
                    overload and are deliberately not listed: a lint that flags
                    safe code gets switched off.

                    The deduced form is structurally immune and is what the fix
                    looks like -- deducing `Type` from a scalar against parameter
                    `SIMDRegister<Type>` FAILS, which removes the SIMD candidate
                    before the class is ever completed:

                        juce::jmax<size_t> (1, v.size())      # rejected
                        juce::jmax ((size_t) 1, v.size())     # fine, identical result

2. --compile-canary  Proves the premise above is still TRUE of the pinned JUCE,
                     rather than trusting this docstring.  Compiles two tiny
                     translation units against the JUCE modules directory given:
                     the deduced form MUST compile and the explicit form MUST
                     NOT.  `long long` is used as the probe type because on Linux
                     it is the exact structural analogue of macOS's
                     `unsigned long` -- a 64-bit integer type that is NOT what
                     <cstdint> spells int64_t there, hence unspecialised.  A JUCE
                     bump that adds or drops a SIMD overload changes this
                     result and fails the canary instead of silently voiding
                     the lint.

Usage:
    scripts/check-portability.py [--root DIR]
    scripts/check-portability.py --compile-canary JUCE_MODULES_DIR [--cxx g++]

Exit codes: 0 clean, 1 violations found, 2 usage/environment error.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

# The closed hazard set -- juce free function templates that juce_dsp ALSO
# overloads for dsp::SIMDRegister<Type>.  Keep this list in sync with
# juce_dsp/containers/juce_SIMDRegister_Impl.h; --compile-canary is what notices
# if the pin moves under it.
SIMD_OVERLOADED = ("jmin", "jmax", "snapToZero")

HAZARD = re.compile(r"(?:juce\s*::\s*)?\b(" + "|".join(SIMD_OVERLOADED) + r")\s*<")

SOURCE_SUFFIXES = (".h", ".hpp", ".cpp", ".cc", ".mm")
# `tools` is kept from the sibling's list even though this repository has no such
# directory: `rglob` over a missing directory yields nothing, so it costs a
# no-op, and a future `tools/` is linted the day it appears rather than the day
# someone remembers this line. (`check-clang-warnings.py` drops it for the
# opposite reason -- there the list is the GATE'S DECLARED SCOPE, not a scan
# list, and declaring a directory that does not exist overstates the gate.)
SOURCE_DIRS = ("src", "tests", "tools")


def blank_comments_and_literals(text: str) -> str:
    """Return `text` with comments and string/char literals replaced by spaces.

    Newlines are preserved so reported line numbers stay exact.  This is what
    keeps the lint from firing on its own explanation: prose in a comment may
    name `juce::jmax<size_t>` freely, and the repository's comments do.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
        elif c == "/" and nxt == "*":
            out.append("  ")
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append("  ")
            i += 2
        elif c == "'" and (
            (out and str(out[-1]).isalnum()) or (i + 1 < n and text[i + 1].isdigit())
        ):
            # A C++ DIGIT SEPARATOR, not a character literal: 1'000'000. Treating
            # it as a quote would blank the source from here to the next `'`,
            # which can only ever hide a hazard (a false NEGATIVE) -- the one
            # failure mode a lint must not have, because it fails by going quiet.
            # The test is local and deliberately conservative: a separator always
            # has an alphanumeric on its left (the preceding digit, already
            # emitted) or a digit on its right, and a real char literal never
            # does -- `'a'` follows an operator or whitespace, and `x'` is not
            # valid C++ otherwise.
            out.append(c)
            i += 1
        elif c in ('"', "'"):
            quote = c
            out.append(" ")
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\" and i + 1 < n:
                    out.append("  ")
                    i += 2
                    continue
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append(" ")
            i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def lint(root: Path) -> int:
    files = [
        p
        for d in SOURCE_DIRS
        for p in sorted((root / d).rglob("*"))
        if p.suffix in SOURCE_SUFFIXES and p.is_file()
    ]
    if not files:
        print(f"check-portability: no source files under {root}/{{{','.join(SOURCE_DIRS)}}}", file=sys.stderr)
        return 2

    violations = []
    for path in files:
        try:
            raw = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        for lineno, line in enumerate(blank_comments_and_literals(raw).splitlines(), 1):
            m = HAZARD.search(line)
            if m:
                violations.append((path.relative_to(root), lineno, m.group(1), raw.splitlines()[lineno - 1].strip()))

    for rel, lineno, name, src in violations:
        print(f"{rel}:{lineno}: error: explicit template argument on juce::{name} -- "
              f"instantiates JUCE's dsp::SIMDRegister overload, which fails to compile on "
              f"macOS for any type that is not one of JUCE's ten SIMD element typedefs "
              f"(size_t is such a type on macOS and is NOT on Linux).")
        print(f"    {src}")
        print(f"    fix: drop the <...> and cast the arguments instead, e.g. "
              f"juce::{name} ((size_t) 1, v.size()) -- identical result, deduction removes "
              f"the SIMD candidate before it can be instantiated.")

    scratch = scratch_names_agree(root)

    print(f"check-portability: {len(files)} file(s) scanned, {len(violations)} violation(s).")
    return 1 if (violations or scratch) else 0


# The scratch names `install.sh` creates that can OUTLIVE it. `uninstall.sh` must
# remove exactly these, and nothing but a human reading both files has ever
# enforced it.
#
# `\.probe` IS IN THE SIBLING'S SET AND IS DELIBERATELY ABSENT FROM THIS ONE.
# The name is the same in both products; the uninstallers are not, and the
# difference is exactly what decides whether the file can survive:
#
#   * `install.sh` creates `.probe` INSIDE the staging directory
#     (`$_out/.probe`) as one end of a hard-link same-filesystem test, and
#     removes it on every branch of that test. One can therefore only survive a
#     hard kill in the window between the `touch` and the `rm`.
#   * This repository's `uninstall.sh` removes each staging directory with a
#     single unconditional `rm -rf`, so anything inside it — `.probe` included —
#     goes with it. There is no path through that script which leaves a staging
#     directory standing.
#   * The SIBLING's uninstaller has one: `--discard-parked` is opt-in, so by
#     default it KEEPS a `.vst3.prev` parked by an interrupted install, and to do
#     that it must leave the directory around that copy in place. `.probe` is
#     then the one scratch file that outlives the uninstall, which is why it is
#     swept by name there and listed in the set here.
#
# Listing it anyway would be a false positive against a correct uninstaller — and
# the reflex fix for a false positive is to switch the lint off, taking the three
# real names with it. IF this repository's uninstaller ever gains a path that
# preserves a staging directory, add `|\.probe\b` back in the SAME change: the
# note in `uninstall.sh`'s `remove_install_scratch` says so at the other end of
# the coupling.
SCRATCH_NAME = re.compile(r"\.anamorph-[a-z-]+|\.Anamorph\.new")


def scratch_names_agree(root: Path) -> int:
    """The installer's scratch names and the uninstaller's removal list must match.

    THIS PAIR DIVERGED ONCE IN THE SIBLING PRODUCT, and the failure was silent in
    the direction that matters: a staging candidate was added to `install.sh` and
    not to `uninstall.sh`, so an interrupted install's staged bundle survived a
    DELIBERATE uninstall — against what `INSTALL.txt` and the CHANGELOG promise.
    Both files carried a comment telling the next editor to keep them in step;
    that comment was present for the divergence. This repository's pair agrees
    today, and this is what keeps it agreeing: the two files here are a port of
    the same design and carry the same coupling.

    The two scripts are separate files in a zip, with no shared library to
    source and no build step that could generate one, so the coupling cannot be
    removed — only checked. Comparing the SET OF NAMES is the whole of it: the
    directories they hang off differ by design (the installer picks a staging
    parent, the uninstaller is told the install directories), but a name that
    exists in one and not the other is always a defect in one of them.
    """
    inst, uninst = root / "packaging/linux/install.sh", root / "packaging/linux/uninstall.sh"
    if not (inst.is_file() and uninst.is_file()):
        return 0                       # not a packaging checkout; nothing to compare

    def names(path):
        # CODE ONLY. Both scripts explain themselves at length and name every
        # scratch file while doing it — including in the paragraph about the
        # candidate that was REMOVED — so scanning the raw text would let a
        # mention satisfy the check. An edit that deleted the removal from
        # `uninstall.sh` but left the comment explaining it behind would keep
        # this gate green while reopening the exact divergence it exists for:
        # the comment survives the code, which is the failure mode the whole
        # repository is written against. Full-line comments are the only form
        # these scripts use for prose; verified that stripping them leaves both
        # sets unchanged today, so this is strictly a tightening.
        code = "\n".join(l for l in path.read_text(encoding="utf-8").splitlines()
                         if not l.lstrip().startswith("#"))
        return set(SCRATCH_NAME.findall(code))

    a, b = names(inst), names(uninst)
    if a == b:
        print(f"check-portability: installer/uninstaller scratch names agree "
              f"({', '.join(sorted(a))}).")
        return 0

    for name in sorted(a - b):
        print(f"packaging/linux/uninstall.sh: error: install.sh creates '{name}' and this "
              f"script never removes it — an interrupted install would survive a deliberate "
              f"uninstall.")
    for name in sorted(b - a):
        print(f"packaging/linux/install.sh: error: uninstall.sh removes '{name}' and this "
              f"script never creates it — a stale name, or a rename applied to one file only.")
    return 1


CANARY_BASELINE = """
#include <juce_dsp/juce_dsp.h>
int main() { return 0; }
"""

CANARY_DEDUCED = """
#include <juce_dsp/juce_dsp.h>
int main() { long long a = 1, b = 7; return (int) juce::jmax (a, b); }
"""

CANARY_EXPLICIT = """
#include <juce_dsp/juce_dsp.h>
int main() { return (int) juce::jmax<long long> (1, 7); }
"""


def compile_canary(modules: Path, cxx: str) -> int:
    if not (modules / "juce_dsp" / "juce_dsp.h").is_file():
        print(f"check-portability: {modules} does not look like a JUCE modules directory", file=sys.stderr)
        return 2

    common = [
        cxx, "-std=c++20", "-fsyntax-only", "-I", str(modules),
        "-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1",
        "-DJUCE_MODULE_AVAILABLE_juce_dsp=1",
        "-DJUCE_STANDALONE_APPLICATION=1",
    ]

    def compile_tu(name: str, body: str):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / f"{name}.cpp"
            src.write_text(body)
            proc = subprocess.run(common + [str(src)], capture_output=True, text=True)
            return proc.returncode, (proc.stdout + proc.stderr)

    # STEP 0 -- prove the ENVIRONMENT works before drawing any conclusion from it.
    # Without this, a missing header, a bad -I, a compiler that does not know
    # -std=c++20, or any other setup problem makes the deduced form fail and gets
    # reported as "the recommended fix is wrong for the pinned JUCE" -- a
    # confident, specific and completely false diagnosis. A canary that cannot
    # tell a broken bench from a broken part is worse than no canary.
    baseline_rc, baseline_log = compile_tu("baseline", CANARY_BASELINE)
    if baseline_rc != 0:
        print("check-portability: CANARY INCONCLUSIVE -- the environment cannot compile a "
              "trivial juce_dsp translation unit, so nothing can be concluded about the SIMD "
              f"overload set. This is a SETUP failure, not a JUCE regression. Compiler: {cxx}; "
              f"modules: {modules}")
        print("  first lines of the compiler output:")
        for line in baseline_log.splitlines()[:8]:
            print(f"    {line}")
        return 2      # environment, deliberately not the 1 that means "lint failed"

    deduced_rc, deduced_log = compile_tu("deduced", CANARY_DEDUCED)
    explicit_rc, explicit_log = compile_tu("explicit", CANARY_EXPLICIT)

    ok = True
    if deduced_rc != 0:
        print("check-portability: CANARY FAILED -- a trivial juce_dsp TU compiles but the "
              "DEDUCED form does not, so the fix this lint recommends is wrong for the pinned "
              "JUCE. The baseline above rules out an environment problem.")
        for line in deduced_log.splitlines()[:8]:
            print(f"    {line}")
        ok = False

    # The explicit form must fail, AND it must fail for the RIGHT REASON. Any
    # compile error would otherwise satisfy "is rejected" and the canary would
    # keep reporting success long after the hazard changed shape.
    expected = ("SIMDNativeOps" in explicit_log) or ("SIMDRegister" in explicit_log)
    if explicit_rc == 0:
        print("check-portability: CANARY FAILED -- the EXPLICIT form now compiles, so the lint "
              "is guarding a hazard this JUCE revision no longer has. Re-derive SIMD_OVERLOADED "
              "from juce_SIMDRegister_Impl.h before relaxing the lint: the macOS typedef "
              "divergence is what makes the hazard real, and it is not observable from a Linux "
              "compile.")
        ok = False
    elif not expected:
        print("check-portability: CANARY INCONCLUSIVE -- the explicit form was rejected, but "
              "NOT by the SIMDRegister/SIMDNativeOps instantiation this lint exists for. Some "
              "other compile error is masking the check, so its success would be an accident.")
        for line in explicit_log.splitlines()[:8]:
            print(f"    {line}")
        return 2

    if ok:
        print("check-portability: compile canary -- trivial TU compiles (environment sound), "
              "deduced form compiles, explicit form rejected by SIMDNativeOps as expected.")
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description="Anamorph source portability lint.")
    ap.add_argument("--root", default=str(Path(__file__).resolve().parent.parent),
                    help="repository root (default: the parent of scripts/)")
    ap.add_argument("--compile-canary", metavar="JUCE_MODULES_DIR",
                    help="instead of linting, verify the pinned JUCE still has the hazard")
    ap.add_argument("--cxx", default="g++", help="compiler for --compile-canary (default: g++)")
    args = ap.parse_args()

    if args.compile_canary:
        return compile_canary(Path(args.compile_canary), args.cxx)
    return lint(Path(args.root))


if __name__ == "__main__":
    sys.exit(main())
