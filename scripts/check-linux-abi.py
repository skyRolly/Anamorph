#!/usr/bin/env python3
# ============================================================================
#  check-linux-abi.py -- the shipped Linux binaries' glibc/libstdc++ FLOOR,
#  asserted rather than assumed. With a self-test, per TESTING_POLICY rule 4.
#
#  WHAT THIS IS ABOUT. A Linux binary built on `ubuntu-latest` records, per
#  imported symbol, the oldest glibc/libstdc++ version that provides it. The
#  MAXIMUM of those is the oldest system the artifact can load on at all: below
#  it the dynamic loader refuses with `version 'GLIBC_x.y' not found` before a
#  single line of this project's code runs. Nothing about that number is chosen
#  -- it falls out of whatever the runner image happened to ship -- and nothing
#  in the build reports it, so the project's compatibility claim for Linux was
#  "it builds on ubuntu-latest", which is a statement about CI rather than about
#  users.
#
#  Measured on the artifact this repository ships (2026-08-18): GLIBC_2.38 and
#  GLIBCXX_3.4.31. That is Ubuntu 23.10 / Debian 13 and GCC 13 respectively --
#  so the VST3 does NOT load on Ubuntu 22.04 LTS (glibc 2.35), which was a live
#  LTS at the time and a normal thing for a DAW user to be running. The number
#  was not a decision anybody made; it is what `ubuntu-latest` moving from 22.04
#  to 24.04 did to the artifact, silently, in some earlier run.
#
#  SO THE FLOOR IS DECLARED HERE AND GATED. The check fails when a shipped
#  binary requires something NEWER than the floor below -- and, equally, when it
#  references one of the declared families NOT AT ALL, because comparing only the
#  families a binary happens to import makes an ABSENT one read as a satisfied
#  one. Both inspected artifacts are C++ binaries linked against the system
#  libstdc++ and glibc, so an absent family means either the wrong file was
#  inspected or the link topology changed. It is not trying to
#  lower the floor -- lowering it means an older build image or a sysroot, which
#  is a release-topology decision -- it is making the floor VISIBLE and making
#  the run that raises it the run that fails, instead of a user's DAW.
#
#  Raising the floor is therefore a deliberate, reviewable act: change the two
#  constants below in the same change as whatever raised them, and say in the
#  PR which systems just stopped being supported. `COMPATIBILITY_MATRIX.md`
#  quotes no number of its own and defers here, for the same reason CLAUDE.md
#  quotes no pluginval strictness: the copy that rots is the one nobody edits.
#
#  WHY THE MAXIMUM AND NOT THE LIST. A binary legitimately references a dozen
#  version tags -- GLIBC_2.2.5 through GLIBC_2.38 -- because each symbol names
#  the version that introduced IT. Only the newest one constrains the host, and
#  gating on the set would fail on an unrelated symbol appearing at an old
#  version. This is the same "one number, derived, not a list" shape the warning
#  baselines use.
#
#  SELF-TEST (`--self-test`). Runs the real parser and the real comparison over
#  synthetic `objdump -p` output: an ordinary case, the ordering trap that makes
#  string comparison wrong (2.9 vs 2.38), a binary that is over the floor, one
#  exactly at it, and output with no version references at all -- which must be
#  an ERROR rather than a pass, because "no requirements found" is what a
#  mis-invoked objdump looks like and it would otherwise read as clean. It also
#  drives `evaluate()`, the whole-floor verdict, over a binary MISSING a declared
#  family in each direction, over one that is both missing a family AND over
#  another's floor, and over the ordering trap a second time -- inside the
#  verdict rather than in `version_key` alone, since a raw string comparison
#  there survives every other case.
#
#  EXIT CODES follow the sibling scripts: 0 clean; 1 a shipped binary is over a
#  declared floor, which is a real artifact regression; 2 the check could not
#  run -- self-test failure, unreadable file, no versioned dependencies at all,
#  or a declared family absent. That last one is filed under 2 rather than 1
#  deliberately: its likeliest cause is a path that no longer names the artifact,
#  i.e. the gate did not measure what it was pointed at, and the other cause (a
#  link-topology change) is indistinguishable from the symbol table alone.
# ============================================================================

import argparse
import re
import subprocess
import sys

# ---------------------------------------------------------------------------
#  THE FLOOR. One place. Raising either of these drops systems -- say which.
# ---------------------------------------------------------------------------
GLIBC_FLOOR = "2.38"        # Ubuntu 23.10+, Debian 13+; excludes Ubuntu 22.04 LTS (2.35)
GLIBCXX_FLOOR = "3.4.31"    # GCC 13+

VERREF = re.compile(r"^\s*0x[0-9a-f]+\s+0x[0-9a-f]+\s+\d+\s+(?P<tag>[A-Za-z_]+)_(?P<ver>[0-9][0-9.]*)\s*$")


def version_key(text: str):
    """`2.38` sorts ABOVE `2.9`. Lexicographic comparison gets that backwards,
    which is the whole reason this function exists rather than a `max()` on
    strings: the first floor this project would have needed was 2.9 vs 2.38."""
    return tuple(int(part) for part in text.split("."))


def parse_version_refs(objdump_p_output: str):
    """Return {family: highest required version} from `objdump -p` output.

    Families are whatever the binary actually references -- `GLIBC`, `GLIBCXX`,
    `GCC`, `CXXABI`. The caller decides which ones it has a floor for; parsing
    does not filter, so a family appearing for the first time is visible in the
    report rather than silently dropped.
    """
    highest = {}
    for line in objdump_p_output.splitlines():
        m = VERREF.match(line)
        if m is None:
            continue
        fam, ver = m.group("tag"), m.group("ver")
        if fam not in highest or version_key(ver) > version_key(highest[fam]):
            highest[fam] = ver
    return highest


def inspect(path: str):
    out = subprocess.run(["objdump", "-p", path], capture_output=True, text=True, check=True).stdout
    return parse_version_refs(out)


FLOORS = {"GLIBC": GLIBC_FLOOR, "GLIBCXX": GLIBCXX_FLOOR}


def evaluate(highest):
    """The verdict for ONE binary's parsed version references.

    Returns `(over, missing)`: the declared families it requires MORE than the
    floor of, and the declared families it does not reference AT ALL.

    THE SECOND HALF IS NOT A FORMALITY. Comparing only the families a binary
    happens to reference makes an ABSENT family read as a satisfied one, which
    is the same "silence means clean" failure this file's `not highest` branch
    already rejects -- only per family instead of per binary. A shipped artifact
    that stopped referencing `GLIBCXX_*` entirely would have passed the
    libstdc++ half of the gate vacuously, and the two things that produce that
    are exactly the two the gate exists to notice: the wrong file being
    inspected (a path that still resolves to SOME ELF with glibc references),
    and a link-topology change such as `-static-libstdc++`. The second is a
    legitimate way to lower the floor, which this file's header calls a
    release-topology decision -- not something a gate should absorb silently
    while reporting clean.
    """
    over = [(fam, FLOORS[fam], highest[fam])
            for fam in FLOORS
            if fam in highest and version_key(highest[fam]) > version_key(FLOORS[fam])]
    missing = [fam for fam in FLOORS if fam not in highest]
    return over, missing


def check(paths) -> int:
    failures = []
    absent = []
    saw_any = False
    for path in paths:
        try:
            highest = inspect(path)
        except (OSError, subprocess.CalledProcessError) as exc:
            print(f"check-linux-abi: cannot read {path}: {exc}", file=sys.stderr)
            return 2
        if not highest:
            # NOT a pass. A binary with no version references is what a wrong
            # path or a stripped-to-nothing file looks like, and reporting it
            # clean is the gate-that-cannot-fail this repository writes against.
            print(f"check-linux-abi: {path} declares NO versioned dependencies -- "
                  f"that is not a clean result, it is a result that means the "
                  f"binary was not read. Check the path.", file=sys.stderr)
            return 2
        saw_any = True
        summary = ", ".join(f"{fam}_{ver}" for fam, ver in sorted(highest.items()))
        print(f"  {path}: {summary}")
        over, missing = evaluate(highest)
        # BOTH VERDICTS ARE COLLECTED, NEVER RETURNED FROM INSIDE THE LOOP. An
        # early return here would stop inspecting the REMAINING binaries and
        # discard the over-floor findings already gathered -- so a genuine floor
        # breach in the Standalone could be replaced by a missing-family message
        # about the VST3, and the run would report neither the breach nor the
        # artifact it was in. That is the same "withholding the evidence helps
        # least" argument the `linux` job makes for running this step last, and
        # it applies inside the step too.
        if missing:
            absent.append((path, missing))
        failures.extend((path, fam, floor, got) for fam, floor, got in over)

    if not saw_any:
        print("check-linux-abi: no binaries were inspected.", file=sys.stderr)
        return 2

    if absent:
        for path, missing in absent:
            print(f"::error::{path} references NO {'/'.join(missing)} symbol at all, so that "
                  f"declared floor was not checked against anything.")
        # Reported alongside, not instead: a run can carry both, and the one that
        # names a real breach is the one a reader needs.
        for path, fam, floor, got in failures:
            print(f"::error::{path} requires {fam}_{got}, above the declared floor "
                  f"{fam}_{floor}.")
        print("\ncheck-linux-abi: a declared ABI family is missing from a shipped binary.\n"
              "This is not a pass: every artifact this gate inspects is a C++ binary linked\n"
              "against the system libstdc++ and glibc, so an absent family means either the\n"
              "wrong file was inspected, or the link topology changed (e.g.\n"
              "-static-libstdc++). Filed as 2 rather than 1 because the first cause -- a path\n"
              "that no longer names the artifact -- means the gate did not measure what it was\n"
              "pointed at, and the two are indistinguishable from the symbol table alone.\n"
              "The second is a real way to LOWER the floor, which this file's header calls a\n"
              "release-topology decision: make it in the same change as the FLOORS above, and\n"
              "say in the PR what it changes for users.", file=sys.stderr)
        return 2

    if failures:
        for path, fam, floor, got in failures:
            print(f"::error::{path} requires {fam}_{got}, above the declared floor "
                  f"{fam}_{floor}.")
        print("\ncheck-linux-abi: the shipped Linux artifact just stopped loading on systems it "
              "used to load on.\nThis is almost always a runner-image move rather than a source "
              "change. Either build\nagainst an older toolchain/sysroot, or raise the floor in "
              "scripts/check-linux-abi.py --\nin the SAME change, naming in the PR which systems "
              "that drops.", file=sys.stderr)
        return 1

    print(f"check-linux-abi: within the declared floor "
          f"(GLIBC_{GLIBC_FLOOR}, GLIBCXX_{GLIBCXX_FLOOR}).")
    return 0


# ---------------------------------------------------------------------------
#  Self-test
# ---------------------------------------------------------------------------
def self_test() -> int:
    fails = cases = 0

    def expect(label, got, want):
        nonlocal fails, cases
        cases += 1
        if got != want:
            print(f"self-test FAIL: {label}\n  want {want}\n  got  {got}", file=sys.stderr)
            fails += 1

    SAMPLE = """
Version References:
  required from libm.so.6:
    0x069691b8 0x00 33 GLIBC_2.38
    0x06969187 0x00 25 GLIBC_2.27
    0x09691a75 0x00 05 GLIBC_2.2.5
  required from libstdc++.so.6:
    0x0297f841 0x00 35 GLIBCXX_3.4.31
    0x02297f89 0x00 32 GLIBCXX_3.4.9
  required from libgcc_s.so.1:
    0x09265f61 0x00 38 GCC_3.3.1
"""
    expect("highest per family, not the first or the last seen",
           parse_version_refs(SAMPLE),
           {"GLIBC": "2.38", "GLIBCXX": "3.4.31", "GCC": "3.3.1"})

    # THE ORDERING TRAP. String comparison says "2.9" > "2.38"; component
    # comparison says the opposite, and the opposite is correct.
    expect("2.38 outranks 2.9 numerically, not lexically",
           parse_version_refs("    0x1 0x00 1 GLIBC_2.9\n    0x2 0x00 2 GLIBC_2.38\n"),
           {"GLIBC": "2.38"})
    cases += 1
    if not (version_key("2.38") > version_key("2.9")):
        print("self-test FAIL: version_key ordering", file=sys.stderr)
        fails += 1

    # Lines that must NOT be read as version references.
    expect("NEEDED lines are not version references",
           parse_version_refs("  NEEDED               libc.so.6\n"), {})
    expect("a 'required from' header is not itself a reference",
           parse_version_refs("  required from libc.so.6:\n"), {})

    # The comparison itself, in both directions and at the boundary.
    cases += 1
    over = version_key("2.39") > version_key(GLIBC_FLOOR)
    at = version_key(GLIBC_FLOOR) > version_key(GLIBC_FLOOR)
    under = version_key("2.35") > version_key(GLIBC_FLOOR)
    if not (over and not at and not under):
        print(f"self-test FAIL: floor comparison over={over} at={at} under={under}",
              file=sys.stderr)
        fails += 1

    # THE VERDICT ITSELF, over the whole declared floor rather than one family
    # at a time -- this is what `check()` calls, so it is what has to be tested.
    # The missing-family cases are the point: until 2026-08-19 a family the
    # binary did not reference was simply skipped, so the case below labelled
    # "a MISSING libstdc++ family is reported, not skipped" reported CLEAN while
    # half the declared floor went unexamined. Named rather than numbered so the
    # comment survives a reordering -- the same reason nothing here cites a line.
    expect("a binary within both floors is clean and complete",
           evaluate({"GLIBC": "2.35", "GLIBCXX": "3.4.30", "GCC": "3.3.1"}),
           ([], []))
    expect("exactly at both floors is still clean",
           evaluate({"GLIBC": GLIBC_FLOOR, "GLIBCXX": GLIBCXX_FLOOR}),
           ([], []))
    expect("over one floor is reported for THAT family only",
           evaluate({"GLIBC": "2.39", "GLIBCXX": GLIBCXX_FLOOR}),
           ([("GLIBC", GLIBC_FLOOR, "2.39")], []))
    expect("a MISSING libstdc++ family is reported, not skipped",
           evaluate({"GLIBC": "2.35"}),
           ([], ["GLIBCXX"]))
    expect("...and a missing glibc family likewise",
           evaluate({"GLIBCXX": "3.4.30"}),
           ([], ["GLIBC"]))
    expect("a missing family is reported even while another is over its floor",
           evaluate({"GLIBC": "2.39"}),
           ([("GLIBC", GLIBC_FLOOR, "2.39")], ["GLIBCXX"]))
    # THE ORDERING TRAP AGAIN, INSIDE THE VERDICT. `version_key` is tested in
    # isolation above, but swapping it for a raw string comparison *within*
    # `evaluate` survives every other case here: `2.9` > `2.38` lexically, so a
    # binary comfortably under the floor would be reported as over it.
    expect("2.9 is under the 2.38 floor in the verdict too, not over it",
           evaluate({"GLIBC": "2.9", "GLIBCXX": GLIBCXX_FLOOR}),
           ([], []))
    expect("families with no declared floor neither satisfy nor break one",
           evaluate({"GLIBC": "2.35", "GLIBCXX": "3.4.30", "CXXABI": "1.3.13"}),
           ([], []))

    # The declared floors must themselves be parseable, or the gate compares
    # against nothing.
    cases += 1
    try:
        version_key(GLIBC_FLOOR), version_key(GLIBCXX_FLOOR)
    except ValueError:
        print("self-test FAIL: a declared floor is not a version", file=sys.stderr)
        fails += 1

    if fails:
        print(f"check-linux-abi: {fails} of {cases} self-test case(s) failed", file=sys.stderr)
        return 1
    print(f"check-linux-abi: self-test passed ({cases} cases).")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Assert the shipped Linux binaries stay within the declared glibc/libstdc++ floor.")
    ap.add_argument("binaries", nargs="*", help="ELF files to inspect")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--print-floor", action="store_true",
                    help="print the declared floor (docs defer to this rather than restating it)")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    if a.print_floor:
        print(f"GLIBC_{GLIBC_FLOOR} GLIBCXX_{GLIBCXX_FLOOR}")
        return 0
    if not a.binaries:
        ap.error("give at least one binary, or --self-test / --print-floor")
    return check(a.binaries)


if __name__ == "__main__":
    sys.exit(main())
