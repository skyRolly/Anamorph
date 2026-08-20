#!/usr/bin/env python3
# ============================================================================
#  check-citations.py — keep `file:line` evidence citations pointing at the code
#  they were written about.
#
#  PROVENANCE: adopted from the sibling product Anabasis
#  (`scripts/check-citations.py`). The mechanism is unchanged; `TRACKED` is
#  re-derived against this repository's source layout and `DELIBERATE_REAIMS`
#  starts empty, because every entry in such a list names one document and one
#  line number in the tree it was written for.
#
#  WHY THIS EXISTS. The documents of record cite their evidence as
#  `src/PluginProcessor.cpp:695-752` — 184 such anchors across `docs/` when this
#  gate landed. A line anchor is exact and therefore fragile: any edit ABOVE it
#  silently re-aims it at unrelated code, and the document keeps reading as
#  though it were still correct. `DOCUMENTATION_LIFECYCLE_POLICY.md` already
#  requires anchors to be re-anchored in the SAME change set that moves them;
#  this makes that rule checkable instead of remembered. In the sibling, the rule
#  did not survive being remembered — 42 of 71 anchors were stale before anyone
#  noticed — and nothing about this repository makes it likelier to.
#
#  It is worth being precise about what this can and cannot do:
#
#    * It CAN tell you that a citation no longer points at the same TEXT it
#      pointed at in a base revision, and move it back onto that text. That
#      catches the whole class of "an edit above shifted it".
#    * It CANNOT tell you a citation was aimed at the wrong code to begin with.
#      Preserving content identity faithfully preserves a pre-existing mistake —
#      and it does so INVISIBLY, in a DRIFTED line that reads like a repair. The
#      anchors in this tree are therefore ADOPTED, not audited: a clean run does
#      not mean every citation is correct, it means none of them MOVED. Spelling
#      the SYMBOL beside the line number is what makes the other half checkable
#      by a reader, and is the convention to write new citations in.
#
#  WHAT IT WILL NOT TOUCH, and why that list is the important part. This tool
#  rewrites line numbers, so every citation it misclassifies as ours is a
#  citation it CORRUPTS — it replaces a correct anchor with a wrong one and
#  reports success. It has done exactly that once already (see `classify()`), so
#  the ownership test is now deliberately narrow: a citation is ours only when
#  it names its path from THIS repository's root, with no revision or checkout
#  qualifier in front of it, and that path is one of `TRACKED` verbatim.
#  Everything else — a bare file name, a sibling-product path, a `<rev>:`-pinned
#  anchor — is left alone. Under-checking costs coverage; misclassifying costs
#  the truth of the document, which is the thing being protected.
#
#  PROSE EXAMPLES MUST NOT USE A TRACKED PATH. This tool cannot tell an
#  illustration of a citation from a citation — `DOCUMENTATION_COVERAGE.md`'s
#  own worked example of a substitution bug was silently re-anchored, changing
#  the numbers the sentence depended on. Write examples against a path outside
#  `TRACKED` (`some/file.cpp:107`) and they are left alone.
#
#  Usage:  --check (the default) reports drift · --fix re-anchors it.
#  Exit codes follow the sibling scripts: 0 clean · 1 drift found (--check only)
#  · 2 the run could not reach a trustworthy answer (nothing to check against,
#  or --fix left anchors that need a human).
# ============================================================================

import argparse
import os
import re
import subprocess
import sys

# The sources whose line anchors are worth tracking, spelled EXACTLY as a
# citation must spell them to be checked. A file absent here is not checked; a
# path that differs from the spelling here — including the SIBLING product's
# `src/gui/PluginEditor.cpp` against our `src/PluginEditor.cpp`, which is the
# same trap in the other direction — is not ours.
#
# This is EVERY root-spelled source path the documents currently cite with a line
# anchor, and each one was confirmed to exist in the tree when the list was
# written. The sibling ships a deliberately shorter list because most of its
# citations land on a handful of files; here they are spread across the DSP
# stages, so a partial list would leave most of the surface unguarded — which for
# this gate means "silently unchecked", the one outcome worse than a red build.
#
# A file added to `src/` is NOT tracked until it is added here. That is the
# correct default (under-checking costs coverage; misclassifying costs the truth
# of the document) but it does mean the list needs a line when a new source file
# starts being cited.
#
# THE GOVERNED SCRIPTS ARE IN SCOPE, and were the gate's largest hole while they
# were not. Anchors into `run-pluginval.sh`, `run-tests.sh`, `setup-linux.sh` and
# `build.sh` drift exactly like the ones into `src/` -- more, in fact, because a
# script gets rewritten wholesale more often than a DSP stage does. They were held
# out of the first round only because that round was itself rewriting them, which
# would have reported every one of their anchors as drift.
#
# ONLY FIRST-PARTY, VERSIONED SCRIPTS. What makes a path safe to list is that this
# repository authors it and `git` tracks it: a generated or fetched file could
# otherwise become a rewrite target, and this tool rewrites. `_deps/`, `build*/`
# and the fetched JUCE tree are none of this list's business, and a bare
# `run-pluginval.sh:154` still does not classify as ours -- the path must carry
# its directory, which is what tells a citation in this repository apart from the
# same file name in another checkout.
#
# THE BUILD DEFINITION AND THE CI WORKFLOW ARE IN SCOPE, for the same reason the
# scripts are and after the same lesson, learned the same way. `CMakeLists.txt`
# carries 90 anchors across the documents and `.github/workflows/build.yml` six;
# both are first-party, both are `git`-tracked, and both are cited as
# `Evidence [Verified]` by the policies. They were outside the gate until a CI
# round inserted 22 lines into one and several hundred into the other, at which
# point `docs/procedures/BUILD.md` pointed `ANAMORPH_BUILD_TESTS` at
# `JUCE_REPORT_APP_USAGE=0`, `RELEASE_POLICY.md` pointed the build-number
# evidence at a comment block and three unrelated workflow lines, and
# `KNOWN_ISSUES.md` pointed the ad-hoc-codesign evidence at a job header -- and
# this gate reported the tree clean through all of it, because it was never
# looking. That is the drift class this file exists to close, arriving in the
# two files that change most.
#
# Adding them is what makes `--fix` able to touch them, so the guard rails
# matter more here, not less: `build/_deps/juce-src/CMakeLists.txt` is a FETCHED
# file with the same basename and must never become a rewrite target, and the
# sibling product has a root `CMakeLists.txt` too. Both are declined -- the
# first because its path is not `TRACKED` verbatim, the second because a
# citation qualified by a checkout or a revision is declined outright -- and
# both are asserted in the self-test rather than left to be reasoned about.
#
# STILL OUTSIDE, and named so the gap is a decision rather than an oversight:
# `packaging/macos/INSTALL.txt` (4 anchors), `NOTICE` (3) and
# `packaging/linux/INSTALL.txt` (1). They are first-party and versioned, so
# nothing disqualifies them; they are simply not what this round measured, and a
# path is not added here until someone has confirmed its anchors are worth
# rewriting.
#
# ONE CAVEAT FOR WHOEVER ADDS THE NEXT ROOT-LEVEL PATH, because `NOTICE` above is
# exactly that shape. A path with directories has a bare escape spelling: prose
# that must NOT be rewritten can write `build.yml:288` and the parser declines
# it, which is how this document's own history sentences stay safe. A root-level
# path has no such escape -- its tracked spelling IS its bare one -- so listing
# `NOTICE` here makes `NOTICE:12` claimable from ANY prose that mentions it with
# a line number, including a sentence whose whole point is the old number. The
# remedy is the one `DOCUMENTATION_COVERAGE.md`'s history table uses: separate
# the path from the anchor (`` `NOTICE` `:12` ``) so no citation is matched.
# Weigh that against the coverage before adding one. The documents citing `.md` files by line are deliberately not
# candidates: `TRACKED` names sources, and a document is edited by prose rules
# rather than by code movement.
TRACKED = (
    "src/InternalState.h",
    "src/PluginEditor.cpp",
    "src/PluginEditor.h",
    "src/PluginParameters.cpp",
    "src/PluginParameters.h",
    "src/PluginProcessor.cpp",
    "src/PluginProcessor.h",
    "src/PresetManager.cpp",
    "src/PresetManager.h",
    "src/dsp/AnamorphEngine.cpp",
    "src/dsp/AnamorphEngine.h",
    "src/dsp/ChorusEngine.cpp",
    "src/dsp/Correlation.h",
    "src/dsp/EngineParameters.h",
    "src/dsp/HaasProcessor.cpp",
    "src/dsp/LevelMeters.h",
    "src/dsp/LoudnessMatch.cpp",
    "src/dsp/MonoMaker.h",
    "src/dsp/MultibandWidth.cpp",
    "src/dsp/MultibandWidth.h",
    "src/dsp/ScopeBuffer.h",
    "src/dsp/SoloMonitor.cpp",
    "src/dsp/VelvetNoise.cpp",
    "src/gui/LookAndFeel.cpp",
    "src/gui/Vectorscope.h",
    "tests/state_tests.cpp",
    "tests/dsp_tests.cpp",
    "scripts/build.sh",
    "scripts/run-pluginval.sh",
    "scripts/run-tests.sh",
    "scripts/setup-linux.sh",
    "CMakeLists.txt",
    ".github/workflows/build.yml",
)

# A citation is `<path>:<line>`, `<path>:<start>-<end>`, or a COMPOUND list that
# names the path once and then several anchors: `<path>:708-709, 851, 1208`. The
# trailing group is what an early version of this file missed — it re-anchored
# the first anchor and left the bare numbers behind it untouched, which produced
# `:1040, 1039, 1053` in this repository: out of order and internally
# contradictory, from a tool whose whole job is keeping anchors true.
#
# Two parts of this pattern exist ONLY to stop the tool rewriting somebody
# else's line numbers, and both are load-bearing:
#
#   * The leading lookbehind refuses to start a match in the middle of a token.
#     Without it, `<checkout>:src/PluginProcessor.cpp:485-491` simply matched
#     from `src/…` — the qualifier never reached the ownership test — and the
#     anchor was rewritten by THIS tree's code movement onto lines of another
#     product that contain something else entirely. Rejecting the match is not
#     enough on its own either: the scan would just retry one character later
#     and match `rc/PluginProcessor.cpp`. The lookbehind blocks every one of
#     those restarts, because each is preceded by a path or word character.
#   * `<prefix>` therefore CAPTURES the qualifier — a checkout name, or a pinned
#     revision such as `7686204:` — so `classify()` can see it and decline.
#
# THE DIRECTORY SEPARATOR IS NO LONGER REQUIRED BY THE PATTERN, and the reason
# it once was is worth keeping straight, because the protection it provided has
# not been given up — it has moved one step later.
#
# The original rule read "a path must contain a separator", and its stated
# purpose was to decline a bare `PluginProcessor.cpp:7`: the architecture
# documents use a bare name as shorthand for "the file I have been quoting",
# which inside a paragraph about another product means that product's file.
# That reasoning is sound and still holds. But the rule was a PROXY for the real
# test, and the proxy had a hole exactly where this repository's build lives:
# `CMakeLists.txt` sits at the root, so its root-spelled path IS its bare name
# and no separator exists to require. The pattern could therefore never match
# it, and adding it to `TRACKED` would have been inert — a listed file that is
# silently never checked, which is the one outcome this gate must not produce.
# That is not hypothetical: 90 anchors into `CMakeLists.txt` across the
# documents went unchecked the whole time, and a round that moved 22 lines of it
# left three documents pointing at unrelated code with the gate reporting clean.
#
# So the pattern now admits a bare name and `classify()` declines it, which is
# where ownership was always decided anyway: `TRACKED` spells every path from
# the repository root, so a bare `PluginProcessor.cpp` still fails to match
# `src/PluginProcessor.cpp` and is still left alone — as is a bare
# `run-pluginval.sh` and a bare `build.yml`, each asserted in the self-test.
# The only names this newly claims are root-level files that `TRACKED` lists
# verbatim. Under-checking still costs coverage and misclassifying still costs
# the truth of the document; this moves one specific file out of the first
# column without moving anything into the second.
CITATION = re.compile(
    r"(?<![\w./\\:-])"
    r"(?:(?P<prefix>[\w.@-]+):)?"
    r"(?P<path>[\w.-]+(?:[/\\][\w.-]+)*):"
    r"(?P<anchors>\d+(?:-\d+)?(?:\s*,\s*\d+(?:-\d+)?)*)")
ANCHOR = re.compile(r"(\d+)(?:-(\d+))?")

# Citations this repository RE-AIMED on purpose: the anchor was pointing at the
# wrong code and was moved onto the right code, which is indistinguishable from
# drift by the base-text test and would otherwise fail the gate forever.
# Declaring one here is a reviewable act — it appears in the diff, beside the
# document it exempts, and it is the only way to make this tool accept a
# re-aim. An entry whose citation no longer differs from the base has done its
# job (the base has caught up) and is reported as removable on the next run.
#
# Spelled `set([...])` rather than `{...}` on purpose: emptying a brace literal
# leaves a DICT, and the set difference below then raises `TypeError` instead of
# reporting a clean run. The list going empty is the expected end state of every
# entry here, so it must be the boring case.
# EACH ENTRY IS `(document, anchor) -> what a reader should FIND there`, and
# that value is not documentation: `verify_reaim_targets()` resolves the anchor
# against the current file on every run and fails when the substring is absent.
#
# The value exists because a declaration here is the one construct in this file
# that turns a check OFF. Until 2026-08-18 nothing checked the aim at all -- the
# run printed "verify the aim by hand" and section 9 of the self-test asserted
# only that the declared STRING appeared somewhere in its document, which is a
# fact about the document rather than about the code it points at. Four anchors
# declared in one round were computed before the workflow file settled; every one
# of them pointed at unrelated lines, and every one was green, because the
# declaration is precisely what stopped the comparison that would have caught it.
# Two more were found within a minute of this check existing.
#
# Choose a substring a READER would recognise as the thing the sentence is
# about -- `codesign --force --deep --sign -`, `macos-intel:`,
# `ANAMORPH_BUILD_NUMBER` -- not a line's incidental characters. It survives
# reformatting, which is the whole reason it is a substring and not a range.
# `""` means "no stable token to name"; the run reports those as unverified
# rather than accepting them silently, so they stay countable.
DELIBERATE_REAIMS = {
    # EMPTY IS THE EXPECTED RESTING STATE. The sibling's list was NOT carried
    # over: each of its entries excuses one specific `<document>, <path>:<line>`
    # pair in ITS tree, so importing them here would exempt anchors that do not
    # exist in this repository and would silently narrow the gate on the day one
    # of those spellings ever coincided.
    #
    # This repository's anchors are ADOPTED AS-IS rather than audited: the tool
    # detects MOVEMENT, not wrongness, so a citation that was aimed at the wrong
    # code before this gate existed stays wrong and stays green. That limit is
    # stated in the header and is not a reason to delay the gate -- it closes the
    # drift class, which is the one that grows on its own.
    #
    # ---------------------------------------------------------------------
    # v0.9.4 CI/validation round: the four `scripts/` entries joined `TRACKED`
    # in the SAME change set that rewrote those scripts. Every anchor below
    # therefore names a region whose TEXT no longer exists at the merge base, so
    # `--fix` cannot repair it -- it reports UNMAPPABLE and needs a human. This
    # is the case the list exists for, and it is scoped to exactly the six
    # transitions that happened: measured against the previous default-branch
    # revision, not invented.
    #
    # Each was re-read at its new location against the sentence that cites it
    # before being declared, and the aim confirmed by the maintainer:
    #   :147-176  the signal-only retry -- comment block + `run_one_pass`
    #   :121      the pluginval release download (`curl -L ...`)
    #   :44-54    setup-linux.sh's apt package list
    #   :19-54    build.sh's artefact-path reporting block
    #
    # THESE ARE GOOD FOR ONE TRANSITION. Once the default branch carries the
    # spellings below, `is_declared_reaim`'s "the spelling actually changed"
    # test can no longer fire for them and the run reports each as removable.
    # Delete them then -- against the MERGE BASE, per the note that run prints.
    #
    # A DECLARATION MUST TRACK ITS OWN ANCHOR. The spellings here were updated on
    # 2026-08-18 because a later edit to the CITED file moved the anchor and
    # `--fix` re-anchored the citing sentence: `run-pluginval.sh` grew the
    # platform-scoped retry (:147-176 -> :147-197), `CMakeLists.txt` grew the
    # bench/fuzz options, and `build.yml` grew the GCC pin. The declaration is
    # not a second copy of the anchor -- it is a claim about a specific spelling,
    # so when the spelling changes the claim has to change with it. This is
    # exactly the staleness section 9 of the self-test exists to catch, and it is
    # what caught it here: the entries were left behind by the first `--fix` and
    # the self-test failed until they followed.
    ("docs/FUTURE_RISKS.md", "scripts/run-pluginval.sh:147-197"): "run_one_pass",
    ("docs/POSTMORTEMS.md", "scripts/run-pluginval.sh:147-197"): "run_one_pass",
    ("docs/architecture/COMPATIBILITY_MATRIX.md", "scripts/run-pluginval.sh:121"): "pluginval",
    ("docs/architecture/design-decisions/ADR-0011-linux-x11-cpu-render.md",
     "scripts/run-pluginval.sh:147-197"): "run_one_pass",
    ("docs/procedures/BUILD.md", "scripts/setup-linux.sh:44-54"): "apt-get",
    # ---------------------------------------------------------------------
    # v0.9.4 CI-performance round: `CMakeLists.txt` and
    # `.github/workflows/build.yml` joined `TRACKED` in the SAME change set that
    # CORRECTED the anchors into them -- the same shape as the `scripts/` block
    # above, arriving for the same reason. The difference is that these four
    # were not merely moved by an edit: they were already WRONG at the base,
    # naming unrelated code, because the round before this one moved the two
    # files while nothing was watching. So each is a genuine re-aim rather than
    # a re-anchor, `--fix` cannot compute it (the base anchor's text is not the
    # text the sentence is about), and each was re-read at its new location
    # against the sentence that cites it and the aim CONFIRMED BY THE MAINTAINER
    # (2026-08-16) -- the same standard the block above was declared under.
    #
    # What each now names, and what it named at the base:
    #   BUILD.md          CMakeLists.txt:27,305      `if(ANAMORPH_BUILD_TESTS)`
    #                     was :283, i.e. `JUCE_REPORT_APP_USAGE=0`
    #   RELEASE_POLICY    CMakeLists.txt:14,250-275  the versioning block, grown
    #                     by the build-number scoping change; was :250-256, which
    #                     now stops in the middle of that block's comment
    #   RELEASE_POLICY    build.yml:495,1042,1454    the linux/windows/macos
    #                     Configure steps; was :383,855,1238, three unrelated
    #                     lines after several hundred were inserted above them
    #   KNOWN_ISSUES      build.yml:1621-1623        the three `codesign
    #                     --force --deep --sign -` calls; was :1398-1400, which
    #                     is now the `macos:` job header
    #
    # GOOD FOR ONE TRANSITION, like the block above: once the default branch
    # carries these spellings the run reports each as removable, and it says so
    # against the merge base rather than against a push predecessor.
    ("docs/procedures/BUILD.md", "CMakeLists.txt:27, 361"): "ANAMORPH_BUILD_TESTS",
    ("docs/policies/RELEASE_POLICY.md", "CMakeLists.txt:14, 306-331"): "ANAMORPH_BUILD_NUMBER",
    # Same round, found by a second review pass: the compile-definition list
    # cites the block those definitions live in, and `ANAMORPH_BUILD_NUMBER`
    # left that block when it was scoped to one translation unit. The
    # `target_compile_definitions(Anamorph PUBLIC ...)` block alone no longer
    # contains it, so the sentence listed a definition its own evidence
    # disproved. The anchor was therefore WIDENED to start at the
    # `set_source_files_properties` that now carries it and run to the end of
    # that block -- ONE anchor still, not two, because a citation whose anchor
    # COUNT changes lands in the "review by hand" branch that no declaration can
    # excuse.
    #
    # ONE TRANSITION ONLY, and for the narrowing rather than for the anchor.
    # Against the MERGE BASE this citation needs no declaration at all: `:330-340`
    # is exactly `origin/main`'s `:274-284` shifted by +56, so the line map
    # computes it and the gate verifies it unaided. But CI also runs against the
    # PREVIOUS PUSH, and that push carried `:330-341` -- one line wider, taking in
    # the block's closing `)`. Narrowing it is a deliberate re-aim measured
    # against that base, which is exactly what this list is for. DELETE THIS once
    # the default branch carries `:330-340`; the run says so itself when that
    # happens.
    ("docs/procedures/BUILD.md", "CMakeLists.txt:330-340"): "ANAMORPH_BUILD_NUMBER",
    # The entry that used to sit here is GONE, and its removal is the point. The numbers moved twice:
    # the round that widened the anchor wrote `origin/main`'s `:274-284`, and the
    # round that added 178 lines to `CMakeLists.txt` re-anchored the START but
    # narrowed the END to `:330-331` -- back to two lines, dropping seven of the
    # eight definitions the sentence enumerates, and still green here because
    # line 331 carries the expected string. Re-derived 2026-08-19 as `:330-340`,
    # which is exactly `origin/main`'s `:274-284` shifted by +56 -- so it is
    # ordinary drift the line map can compute, and it needs no declaration at
    # all. (An intervening draft used `:330-341`, one line wider, taking in the
    # block's closing `)`. That single line bought no evidence and cost a
    # permanent exemption, which is a bad trade for the construct this file calls
    # the one thing that turns a check OFF.)
    # ---------------------------------------------------------------------
    # ADR-0009's NaN/Inf SELF-HEAL ANCHOR WAS WRONG BEFORE THIS GATE EXISTED, and
    # both documents that cite the block proved it by disagreeing. The audit and
    # the ADR each carried `AnamorphEngine.cpp:847-870` at the merge base; the
    # audit was re-derived to the real self-heal earlier in this change set,
    # while the ADR's copy was only SHIFTED -- to `:860-883`, which is input
    # conditioning, the M/S solo branch and the `dryScratch` copy. Unrelated code,
    # cited twice in the same file, for a decision about NaN handling.
    #
    # This is the "adopted as-is rather than audited" limit stated at the top of
    # this list, arriving exactly as predicted: an anchor already misaimed at the
    # base stays misaimed through `--fix`, because the tool detects MOVEMENT and
    # this never moved -- it was wrong where it started. So it is a re-aim, not a
    # re-anchor, and `--fix` cannot compute it.
    #
    # Re-derived 2026-08-19 as `:1269-1313`: the comment header, the branch-free
    # non-finite detector, the zeroing pass, and the stateful reset. The reset is
    # why the span runs to 1313 rather than stopping where the audit's does -- the
    # ADR's own Consequences claim the plugin "self-heals instead of needing a
    # Multiband off/on", and that sentence is about `multiband.reset()`.
    ("docs/architecture/design-decisions/ADR-0009-nan-selfheal-nyquist-clamp.md",
     "src/dsp/AnamorphEngine.cpp:1269-1313"): "Defensive NaN / Inf self-heal",
    # THE AUDIT'S COPY IS WIDENED TO MATCH, and needs a declaration only against
    # the PREVIOUS PUSH. `REALTIME_SAFETY_AUDIT.md` was re-derived earlier in this
    # change set to `:1269-1303`, which stops on the `if (nonFinite)` line -- so it
    # excluded every one of the resets its own sentence claims ("replaces only
    # non-finite samples with 0 AND RESETS STATEFUL NODES"). Against `origin/main`
    # the line was a bare filename and therefore a new citation with nothing to
    # drift from; against the previous push it is a deliberate widening. One
    # spelling for one block, in both documents. DELETE THIS once the default
    # branch carries `:1269-1313`.
    ("docs/architecture/REALTIME_SAFETY_AUDIT.md",
     "src/dsp/AnamorphEngine.cpp:1269-1313"): "Defensive NaN / Inf self-heal",
    # ---------------------------------------------------------------------
    # THE WORKFLOW ANCHORS WERE WRONG ON ARRIVAL, and the way they got there is
    # worth recording because this gate is the thing that should have caught it.
    # They were computed part-way through the round that introduced them and
    # then a LATER edit in that same round (making ccache optional, ~+70 lines)
    # moved everything below it. Nothing objected: three of them were declared
    # re-aims, and a declaration is exactly a promise that the tool must not
    # judge the aim; the two `COMPATIBILITY_MATRIX` ranges were BRAND NEW
    # citations, and a new citation "has nothing to drift from" so it is skipped
    # against the base. Both escape hatches are correct in themselves and both
    # were open at once, which is how a measured number ships stale.
    #
    # Every one below is now recomputed from the file as it stands and read back
    # line by line, not shifted by an arithmetic delta:
    #   RELEASE_POLICY  :546,1124,1546  the linux/windows/macos Configure steps
    #   KNOWN_ISSUES    :1713-1715      the three ad-hoc `codesign` calls
    #   COMPAT_MATRIX   :1482-1942      the `macos` job, key line to last step
    #   COMPAT_MATRIX   :1943-1999      its rationale block (cited alongside)
    #   COMPAT_MATRIX   :2000-2245      the `macos-intel` job, ditto
    # All five moved +4 or +9 in the Clang-22 round -- the pin rationale grew by
    # four lines above every anchor, and the two Clang jobs' install steps by
    # five more above everything from `windows:` down. Recomputed from the file
    # and read back, NOT shifted by arithmetic; section 9 below is what caught
    # the four stale declarations this round, which is the second time it has.
    # The lesson is in the header already: a declaration buys ONE transition and
    # costs the tool's opinion for that transition, so the hand-check it
    # substitutes for has to actually happen.
    #
    # FOURTH recomputation (the allocation-guard / static-lint round): the
    # release-blocking correction and the valgrind opt-out note grew the
    # `sanitizers` job's comments by 35 lines above `windows:`, so everything
    # from `windows:` down moved +35. Recomputed by `--fix` against the push
    # predecessor and read back by hand at every boundary.
    #
    # Third recomputation (the sanitizer-coverage round): the `sanitizers` job's
    # comments grew by 27 lines above `windows:` (the UBSan census + ASan
    # runtime-option rationale), so everything from `windows:` down moved +27.
    # `--fix --base <push predecessor>` recomputed all five anchors from the
    # file; each was then read back by hand at the new spelling.
    #
    # BOTH WERE MISAIMED TOO, and both are now single-line anchors rather than
    # 400-line spans. `1544-2004` claimed "the `macos` job" and landed inside a
    # PowerShell PDB helper in the `windows` job; `2062-2307` claimed
    # "the `macos-intel` job" and landed on an artifact-upload line in `macos`.
    # A span that large cannot be verified by reading it and moves whenever
    # anything above it moves, so each now names the JOB HEADER -- one line, one
    # unambiguous token (`macos:`, `macos-intel:`), which identifies the job more
    # precisely than a range ever did and which `verify_reaim_targets` can check.
    ("docs/architecture/COMPATIBILITY_MATRIX.md",
     ".github/workflows/build.yml:1667"): "macos:",
    ("docs/architecture/COMPATIBILITY_MATRIX.md",
     ".github/workflows/build.yml:2266"): "macos-intel:",
    # THESE TWO WERE MISAIMED, and are corrected here. Both were recomputed
    # early in the round that introduced them and never re-derived, so
    # `build.yml:562, 1186, 1608` pointed at a composite-action `uses:` line, a
    # comment inside the valgrind rationale and an `if-no-files-found:` key --
    # while claiming to be the three per-OS Configure steps. `1775-1777` named
    # the `Package macOS plugins` step header while claiming the three
    # `codesign` calls. Re-derived against the current file and now carrying the
    # expectation that keeps them honest.
    #
    # A declaration naming a spelling that exists in no document is dead weight
    # at best and a red default branch at worst, so section 9 of `--self-test`
    # asserts every entry here is a string its document really contains -- and
    # section 8c asserts it lands on what it claims.
    ("docs/policies/RELEASE_POLICY.md", ".github/workflows/build.yml:578, 1234, 1731"): "ANAMORPH_BUILD_NUMBER",
    ("docs/KNOWN_ISSUES.md", ".github/workflows/build.yml:1949-1951"): "codesign --force --deep --sign -",
    # THE ONE ANCHOR IN THE CLANG-22 ROUND THAT IS A RE-AIM RATHER THAN A
    # RE-ANCHOR, and the tool is right to refuse it. `DEPENDENCY_POLICY`'s Clang
    # row cites the `env:` block, whose lines both MOVED (+4) and CHANGED
    # (`ANAMORPH_CLANG_VERSION: 20` -> `22`), so there is no text to map from --
    # `--fix` cannot compute it and reports UNMAPPABLE. Verified by hand at the
    # new spelling: :78 `env:`, :79 the pluginval strictness, :80 the Clang
    # major. Recorded here rather than worked around, because the LOCAL run
    # could not see it: against `origin/main` this row does not exist yet, so the
    # citation reads as NEW and is skipped, while CI compares against the push
    # predecessor where it does exist. That is both escape hatches open at once
    # -- exactly the pair this file's header warns about, and the reason a local
    # green is not evidence until it is re-run against the base CI will use.
    ("docs/policies/DEPENDENCY_POLICY.md", ".github/workflows/build.yml:107-109"): "ANAMORPH_CLANG_VERSION",
    # PRE-EXISTING ROT CORRECTED, not fresh drift: these four anchors were
    # stale BEFORE origin/main (PluginProcessor/PluginEditor grew above them
    # long ago), so the tool -- which preserves aim-at-same-text relative to a
    # base -- can only propose re-aiming them BACK at the rotten spellings.
    # The corrected aims were measured against the working tree 2026-08-18:
    # `processBlock` spans src/PluginProcessor.cpp:117-185 with
    # `ScopedNoDenormals` at :119 (grep + body-end read); `prepare` spans
    # src/dsp/AnamorphEngine.cpp:28-113 (awk over the body); the editor's 24 Hz
    # timer starts at src/PluginEditor.cpp:672. The same rot class survives
    # un-gated in every BARE-filename evidence line (REALTIME_SAFETY_AUDIT.md's
    # table was the worked example -- its rows are now path-qualified so this
    # tool tracks them from here on).
    ("docs/architecture/THREAD_MODEL.md", "src/PluginProcessor.cpp:117-185"): "processBlock",
    ("docs/architecture/THREAD_MODEL.md", "src/PluginEditor.cpp:672"): "startTimerHz (24)",
    ("docs/policies/REALTIME_AUDIO_POLICY.md", "src/PluginProcessor.cpp:117-185"): "processBlock",
    ("docs/policies/REALTIME_AUDIO_POLICY.md", "src/dsp/AnamorphEngine.cpp:28-113"): "prepare",
    ("docs/procedures/BUILD.md", "scripts/build.sh:19-54"): "artefacts",
    # ---------------------------------------------------------------------
    # 2026-08-19, the hover-occlusion round's review: PRIVACY.md's preset-directory
    # anchor was aimed at the wrong code BEFORE this gate existed, which is the
    # limitation stated at the top of this file rather than a failure of it. The
    # sentence says the Presets folder is created when the Load Preset dialog opens;
    # `dir.createDirectory()` is in `showLoadPreset`, but the anchor named the S11
    # generation pre-gate comment in `stepMicroAnims` -- `:1455` at the merge base,
    # ~383 lines short of the `:1838` the call sat on there. `--fix` then did exactly
    # what it promises and carried the mistake to `:1524`, still the same comment.
    #
    # This is the case the list exists for, and it is worth being precise about WHY
    # a declaration is needed here when the same round re-aimed five anchors in
    # `docs/KNOWN_ISSUES.md` without one. Citations are paired per (document, path)
    # by POSITION (`zip(olds, curs)`), and the count guard above skips a whole group
    # when the counts differ. KNOWN_ISSUES went 2 -> 5, so its group is not compared
    # at all -- those anchors are unchecked, not blessed. PRIVACY.md still has
    # exactly one `src/PluginEditor.cpp` citation, so its pair IS compared, the
    # re-aim reads as drift, and without this entry `--fix` reverts the correction
    # on every run. Measured, not assumed: `--fix` reverted it once before this
    # entry was written.
    ("PRIVACY.md", "src/PluginEditor.cpp:2016"): "createDirectory",
    # ---------------------------------------------------------------------
    # 2026-08-19, the same class in the three LEGAL documents, and the last one
    # this branch carries. EULA, PRIVACY and TRADEMARKS each assert where the
    # product's ONE outbound link is declared, and each named `src/PluginEditor.h:213`
    # at the merge base -- inside `GatedTooltipWindow::getTipFor`, not the link.
    # `--fix` carried all three to `:223`, still that same `return
    # juce::TooltipWindow::getTipFor (c);`. `aboutLink` is declared at `:382`.
    #
    # Declared rather than silently renumbered because each document has exactly ONE
    # `src/PluginEditor.h` citation in both the base and the current tree, so the
    # count guard does not fire and the pair IS compared: without these three entries
    # the run reports DRIFTED and `--fix` drags all three back to `:223` every time.
    # The substring is what keeps that off-switch honest -- `verify_reaim_targets`
    # resolves `:382` against the live header on every run, so if the declaration ever
    # outlives the line it names, the run fails instead of quietly exempting it.
    ("EULA.md", "src/PluginEditor.h:478"): "aboutLink",
    ("PRIVACY.md", "src/PluginEditor.h:478"): "aboutLink",
    ("TRADEMARKS.md", "src/PluginEditor.h:478"): "aboutLink",
    # ---------------------------------------------------------------------
    # 2026-08-18, the macOS symbolication round. `AnamorphEngine::process` gained
    # `ANAMORPH_NONBLOCKING` on its DEFINITION (it was on the declaration only),
    # with the reasoning above it -- so the anchor's FIRST line is one of the
    # lines that changed, which is the one shape `--fix` cannot map: the text it
    # would search for is the text that was edited. Re-derived by hand against
    # the definition (`:673`) and the closing brace of its body (`:1352`), and
    # the expectation below is what makes that claim checkable from here on.
    ("docs/architecture/DSP_GRAPH_REFERENCE.md",
     "src/dsp/AnamorphEngine.cpp:673-1352"): "AnamorphEngine::process",
    ("docs/architecture/SIGNAL_FLOW.md",
     "src/dsp/AnamorphEngine.cpp:673-1352"): "AnamorphEngine::process",
}


def reaim_target_text(whole):
    """The source lines a declared anchor actually points at, joined.

    `whole` is the anchor as the document spells it (`path:12, 30-40`), so this
    resolves exactly what a reader following the reference would land on --
    which is the thing the declaration is making a claim about.
    """
    m = CITATION.match(whole)
    if m is None:
        return None
    path = m.group("path")
    try:
        lines = read(path).split("\n")
    except OSError:
        return None
    picked = []
    for a, b in ANCHOR.findall(m.group("anchors")):
        lo = int(a)
        hi = int(b) if b else lo
        for n in range(lo, hi + 1):
            if 1 <= n <= len(lines):
                picked.append(lines[n - 1])
    return "\n".join(picked)


def reaim_stops_the_run(problems, fixing: bool) -> bool:
    """Is a misaimed declaration fatal HERE, or reported and carried past?

    This is the whole of the repair-mode / verification-mode distinction, kept
    as one testable expression because the shape of the bug it encodes is a
    one-character regression: an unconditional `return 2`.

    `--check` VERIFIES, so it stops -- a declaration switches the drift check
    OFF for its anchor, and a wrong one is invisible in every other way. The CI
    step runs `--check`, so that is where the release-blocking failure lives.

    `--fix` REPAIRS, so it must NOT stop. Declared anchors drift for the most
    ordinary of reasons -- two of them are single lines in `build.yml`, so any
    insertion above one moves them -- and stopping the whole run over that left
    the operator a "could not run" and no automated way out for the OTHER
    documents, whose drift `--fix` repairs perfectly well. Measured on a worktree
    at 33333fe: one line inserted into `build.yml` misaimed three declared
    entries, two of them single-line anchors, and `--fix` exited 2 having
    rewritten nothing at all; carrying past it re-anchors the unrelated drift and
    still exits 2, because `fix_exit_code` below keeps the run failing.

    WHAT IT DOES NOT DO, stated because the first version of this comment claimed
    otherwise: `--fix` cannot re-anchor the misaimed anchor ITSELF while its
    declaration is live, because `is_declared_reaim` excuses exactly that anchor
    from the comparison. The replacement spelling is computed only for a
    declaration whose base and current spellings already agree. So the value here
    is the rest of the run, not the anchor that caused it -- and the document
    that OWNS a misaimed declaration is skipped entirely, see the interlock in
    the rewrite branch below.
    """
    return bool(problems) and not fixing


def fix_exit_code(unmappable: int, reaim_problems) -> int:
    """`--fix` exits non-zero when it leaves work behind -- including a misaimed
    declaration it CANNOT repair, because a declaration lives in this script and
    no rewrite of a document reaches it. Letting `--fix` proceed past one must
    not turn into `--fix` reporting success over one."""
    return 2 if (unmappable or reaim_problems) else 0


def verify_reaim_targets():
    """Check every declared re-aim still lands on what it claims to.

    WHY THIS EXISTS. A `DELIBERATE_REAIMS` entry switches the drift comparison
    OFF for one anchor -- that is its whole function, and it has to be, because
    a deliberate re-aim is textually indistinguishable from drift. The cost was
    that nothing then checked the aim at all: the tool printed "verify the aim by
    hand, not by this tool" and moved on, and section 9 only asserted the
    declared STRING appeared somewhere in its document. Four anchors were
    declared in one round with line numbers computed before the file settled;
    all four pointed at unrelated lines, and all four were green, because the
    declaration is exactly what stopped anyone looking.

    So a declaration now carries what the reader is supposed to FIND there, and
    that claim is checked on every run against the current file. It is a
    substring rather than a range comparison on purpose: the aim is "this
    reference identifies the codesign calls", not "these are bytes 3-40", and a
    substring survives the reformatting that line numbers do not.

    An entry may declare `""` when there is genuinely no stable token to name --
    a single line of ordinary code. That is reported, not silently accepted, so
    the unverifiable ones stay countable.
    """
    problems, unverifiable = [], []
    for (doc, whole), expect in sorted(DELIBERATE_REAIMS.items()):
        if not expect:
            unverifiable.append((doc, whole))
            continue
        target = reaim_target_text(whole)
        if target is None:
            problems.append((doc, whole, expect, "the cited file could not be read"))
        elif expect not in target:
            first = target.split("\n")[0].strip()[:70]
            problems.append((doc, whole, expect,
                             f"the cited lines start with: {first!r}"))
    return problems, unverifiable


def is_declared_reaim(doc, whole_base, whole_cur):
    """Does a declaration excuse this citation's mismatch?

    TWO conditions, and the second is what stops the list becoming a set of
    permanent exemptions:

      1. Either spelling is declared. Which side of the change a declaration
         names should not decide whether it works — the two check paths had
         drifted apart on exactly that, one testing the current spelling and the
         other the base one, so a declaration written for one branch was inert in
         the other and nothing said so.
      2. THE SPELLING ACTUALLY CHANGED. A re-aim moves an anchor, so it always
         changes the spelling. If base and current read the same, this diff did
         not re-aim anything and a mismatch is ordinary drift — the exact case a
         spelling-keyed declaration would otherwise silence forever. Without this
         the entry survived its own transition: once the base carried the
         re-aimed spelling, `1766 == 1766` kept matching and the next commit that
         moved that code had its drift swallowed by a declaration written for
         something else.

    So a declaration is good for exactly one transition, and the run after it
    reports the entry as removable.
    """
    if whole_base == whole_cur:
        return False
    return any((doc, w) in DELIBERATE_REAIMS for w in (whole_base, whole_cur) if w)


def invalidated_reaims(doc, text, rewritten, edits):
    """Which DELIBERATE_REAIMS entries for `doc` this rewrite is about to kill.

    Returns [(declared spelling, suggested replacement or None)].

    A declaration is a claim about a SPELLING, and a re-anchor can invalidate it:
    the anchor an entry names drifts for an unrelated reason -- an edit to the
    CITED file -- `--fix` re-anchors it correctly, and the entry is left naming a
    string the document no longer contains. Section 9 of the self-test calls that
    a defect and fails on it, correctly. The problem is WHERE it fires: in CI,
    minutes later, in another job, saying only that an entry is dead -- while the
    tool that killed it knows the replacement.

    Observed twice in one change set (2026-08-18), from edits to
    `run-pluginval.sh` and `CMakeLists.txt`.

    The replacement is matched by SPAN, not by index and not by tracked path.
    Both weaker forms were tried and both suggest the wrong string:

      * by index -- `edits` is in span order and has no positional relationship
        to the sorted declaration list at all;
      * by tracked path -- correct only while a document rewrites that path
        ONCE. `docs/procedures/BUILD.md` carries two `CMakeLists.txt` anchors,
        and the path form confidently offered the first one's replacement for
        the second. Caught by this warning firing on real drift the same day it
        was added, which is the argument for the warning as much as for the fix.

    The span is exact: the declared spelling occupies a known offset range in
    `text`, and the edit that replaced it is the one whose own range overlaps it.

    EVERY occurrence is tried, not just the first. A document may spell the same
    anchor twice -- `check-citations` itself records that case ("spelled 2x in
    this document; all move together") -- and a rewrite can touch the second
    while leaving the first alone. Stopping at `text.index(whole)` then found no
    overlapping edit and printed the warning with no replacement, which is the
    one thing this warning exists to provide. Weaker than suggesting the wrong
    string, but the guarantee is supposed to be unconditional.
    """
    out = []
    for whole in sorted(w for (d, w) in DELIBERATE_REAIMS if d == doc):
        if whole not in text or whole in rewritten:
            continue
        replacement = None
        at = text.find(whole)
        while at != -1 and replacement is None:
            end = at + len(whole)
            replacement = next((new for (a, b, new) in edits if a < end and at < b), None)
            at = text.find(whole, at + 1)
        out.append((whole, replacement))
    return out


def classify(prefix, path):
    """The tracked path this citation names, or None if it is not ours.

    A citation qualified by anything — another checkout, or a pinned revision
    such as `7686204:` — is NOT about this working tree's current line numbers,
    and rewriting it is destroying evidence rather than maintaining it. That is
    not a hypothetical: it happened, to 27 anchors across `DESIGN.md`, five ADRs
    and `OPEN_QUESTIONS.md`, including the ADR-0016 table whose heading says in
    as many words that it was read from the pre-change tree.

    Requiring the path to be `TRACKED` verbatim is what makes an explicit
    foreign-root test unnecessary: a path from another checkout does not match
    this repository's own layout, so it is declined for the same reason a typo
    would be — this tool only ever rewrites the files listed in `TRACKED`, and
    nothing else in the repository or outside it.
    """
    if prefix:
        return None
    if path.startswith(("/", "\\")):
        return None
    norm = path.replace("\\", "/")
    return norm if norm in TRACKED else None


def git_show(ref, path):
    r = subprocess.run(["git", "show", f"{ref}:{path}"],
                       capture_output=True, text=True)
    return r.stdout.split("\n") if r.returncode == 0 else None


def read(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read()


def line_of(lines, n):
    return lines[n - 1] if lines is not None and 1 <= n <= len(lines) else None


# A cross-product ATTRIBUTION line, which `ADR-0009` mandates in exactly this
# spelling at the head of every adapted file. It carries an anchor into the
# SIBLING product spelled with a path this repository also owns:
#
#     // Provenance (ADR-0009): adapted from <the sibling product> src/gui/LookAndFeel.cpp:1-912 @ <sha>.
#
# The ownership test cannot see the difference. `<prefix>` catches a `rev:`
# qualifier and the lookbehind catches a path glued to another token, but here
# the product name is a separate word followed by a space, so the citation
# classifies as OURS and a re-anchor would rewrite the sibling's line range using
# THIS tree's code movement — the exact corruption this file's header says is the
# one it must never commit. Harmless while only `docs/` was scanned, and live the
# moment source files joined the scan, which is why the two changes are one
# change.
PROVENANCE_MARKER = "Provenance (ADR-0009)"


def citations(text):
    """(whole matched string, tracked path, span, [(start, end|None), ...])"""
    out = []
    # Byte offsets of every provenance BLOCK, so a match can be tested against
    # the block it sits in without re-splitting the text per match.
    #
    # THE BLOCK, NOT THE LINE, and the difference is a live corruption hazard
    # rather than tidiness. This exclusion was line-scoped, which happens to
    # cover `LookAndFeel.h:1` and `LookAndFeel.cpp:1` — marker and anchor share
    # a line there. It does NOT cover `src/gui/PluginEditor.h`, where the marker
    # sits on one line and the sibling's anchor (`src/PluginEditor.h:36-175 /
    # .cpp:672-800`) wraps onto the line two below. That file is safe today only
    # by ACCIDENT of ownership: `src/PluginEditor.h` is the sibling's spelling
    # and is not in `TRACKED`, this tree's editor being `src/gui/PluginEditor.h`.
    # The day a provenance sentence wraps a path this repo also owns, a re-anchor
    # would rewrite the SIBLING's range using this tree's movement — the one
    # corruption this file's header says it must never commit, reached by a
    # line break.
    #
    # A block is the marker's line plus every following CONTINUATION line: one
    # whose body, after a leading `//`, `#`, `*` or `>` and whitespace, is not
    # empty. That ends the block at the `//` separator below the sentence in
    # `PluginEditor.h`, at a blank line in Markdown, and at the first line of
    # real content anywhere else — without this function needing to know which
    # comment syntax it is reading.
    provenance_spans = []
    lines = text.splitlines(keepends=True)
    offsets, pos = [], 0
    for line in lines:
        offsets.append(pos)
        pos += len(line)

    # A continuation must wear the SAME comment prefix as the marker line, and
    # that requirement is the fix for a boundary that was merely heuristic. The
    # first form stripped `//`, `#`, `*` and `>` interchangeably, so in
    # `src/gui/LookAndFeel.h` — marker on line 1, `#pragma once` on line 2 —
    # `#pragma once` stripped to `pragma once`, read as a continuation, and the
    # block silently swallowed a line of real code. Under-checking rather than
    # corruption, since the effect is to DECLINE citations, but a block that can
    # extend over arbitrary following content is not a boundary.
    def _prefix_of(line):
        s = line.lstrip()
        for p in ("//", "#", "*", ">"):
            if s.startswith(p):
                return p
        return ""                      # Markdown prose: no comment marker

    def _is_continuation(line, prefix):
        s = line.lstrip()
        if prefix == "":
            return s != ""             # a wrapped sentence; a blank line ends it
        if not s.startswith(prefix):
            return False
        return s[len(prefix):].strip() != ""

    i = 0
    while i < len(lines):
        if PROVENANCE_MARKER in lines[i]:
            prefix = _prefix_of(lines[i])
            j = i + 1
            while j < len(lines) and _is_continuation(lines[j], prefix):
                j += 1
            provenance_spans.append((offsets[i], offsets[j - 1] + len(lines[j - 1])))
            i = j
            continue
        i += 1

    for m in CITATION.finditer(text):
        tracked = classify(m.group("prefix"), m.group("path"))
        if tracked is None:
            continue
        s, _e = m.span()
        if any(a <= s < b for a, b in provenance_spans):
            continue        # the sibling product's anchor — never ours to move
        anchors = [(int(a.group(1)), int(a.group(2)) if a.group(2) else None)
                   for a in ANCHOR.finditer(m.group("anchors"))]
        out.append((m.group(0), tracked, m.span(), anchors))
    return out


def line_map_from_diff(diff):
    """old line -> new line, from the hunk headers of a `-U0` unified diff.

    SPLIT OUT FROM `build_line_map` SO IT CAN BE TESTED. The arithmetic here is
    the whole tool: every re-anchor this script writes is this function's answer,
    and it has been wrong once already in a way no tree-level run would have
    shown (the pure-insertion case below). Feeding it hunk headers directly needs
    no repository, no commits and no `git`, which is what lets the self-test run
    in the same job as the check.
    """
    edits = [(int(h.group(1)), int(h.group(2) or 1), int(h.group(4) or 1))
             for h in re.finditer(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@",
                                  diff, re.M)]

    def m(n):
        off = 0
        for start, old_count, new_count in edits:
            if old_count == 0:
                # A PURE INSERTION (`@@ -9,0 +10,2 @@`) sits AFTER old line
                # `start`; that line does not move. `start + old_count - 1` is
                # `start - 1` here, so the general test below would treat line
                # `start` as shifted — one line too far.
                if start < n:
                    off += new_count
                continue
            if start + old_count - 1 < n:
                off += new_count - old_count
            elif start <= n:
                return None
            else:
                break
        return n + off
    return m


def build_line_map(base, path):
    """old line -> new line for `path` between `base` and the working tree."""
    diff = subprocess.run(["git", "diff", "-U0", base, "--", path],
                          capture_output=True, text=True).stdout
    return line_map_from_diff(diff)


# `worklogs/` is deliberately OUT of scope. Those files are research records
# about the SIBLING product, and they cite it both fully qualified (an absolute
# path into that checkout, `…/src/PluginEditor.cpp:797-800`) and, in the same
# bullet, by bare file name as shorthand for the same file. The ownership test
# above now declines both shapes anyway; the exclusion stays as the cheap,
# explicit statement of intent, so a future loosening of that test cannot reach
# these files by accident.
EXCLUDED_PREFIXES = ("worklogs/",)


def doc_files():
    """Every file whose citations are checked — Markdown AND the tracked SOURCE.

    THE SOURCE HALF WAS ADDED AFTER A COMMENT DRIFTED 24 LINES INSIDE THE ROUND
    THAT BUILT THIS TOOL. `closePresetUndoBracket`'s argument named `:1482` and
    `:1538` as the two sites that seed `presetBaseline`; a comment block inserted
    above them moved both by 24, and because this scan covered only `docs/` and
    the root Markdown, the gate could not see it. A tool that keeps documents
    honest about the code while the code lies about itself is checking the
    smaller half: these anchors are read by whoever is editing the function, at
    the moment they are deciding whether the argument still holds.

    The files in `TRACKED` are exactly the ones already treated as citable
    targets, so this makes the set self-checking rather than widening it. A
    source file citing ITSELF is fine: a re-anchor rewrites digits inside one
    line and never changes a line COUNT, so the numbering the citation is
    measured against survives its own repair.
    """
    # `splitlines`, not `split()`: a tracked path may legally contain a space,
    # and whitespace-splitting would shatter it into fragments that match
    # nothing — silently dropping the file from the scan rather than failing.
    # No such path exists here today, which is exactly why the bug would ship.
    tracked = subprocess.run(["git", "ls-files"],
                             capture_output=True, text=True).stdout.splitlines()
    out = []
    for p in tracked:
        if p.startswith(EXCLUDED_PREFIXES):
            continue
        if p in TRACKED:
            out.append(p)          # the source half — see the docstring
            continue
        if not p.endswith(".md"):
            continue
        if p.startswith("docs/") or "/" not in p:
            out.append(p)
    return out


def apply_edits(text, edits):
    """Substitute by SPAN, right to left.

    Never by string. `str.replace` matches a prefix: with `src/PluginProcessor.cpp:107`
    drifted to `:130` and `src/PluginProcessor.cpp:1076` still correct, replacing
    the first by text turns the second into `src/PluginProcessor.cpp:1306` —
    corrupting a citation that had nothing wrong with it. Spans come from the
    match that produced them, so each rewrite lands on exactly its own citation.

    DEDUPED AND CHECKED FOR OVERLAP, because a span is only safe once. The
    count-mismatch branch pairs a base citation against ALL current spans
    carrying its spelling, so a document that spells one citation twice queued
    that span-list once per base occurrence — and applying an identical
    `(start, end, new)` twice does not repeat a rewrite, it destroys one: the
    first pass changes the text's length, so `end` no longer bounds what it
    bounded and the second splices the replacement into the middle of itself
    (`…cpp:2000` became `…cpp:20000`). A set fixes the duplicate; the overlap
    check is there because any FUTURE way of generating two different rewrites
    for one region fails the same way, and this function must not corrupt a
    governed document in silence when that happens.
    """
    uniq = sorted(set(edits), key=lambda e: -e[0])
    for (a_start, a_end, _), (b_start, b_end, _) in zip(uniq, uniq[1:]):
        if b_end > a_start:
            raise ValueError(
                f"check-citations: refusing to rewrite overlapping spans "
                f"[{b_start},{b_end}) and [{a_start},{a_end}) — this would corrupt the "
                f"document rather than re-anchor it")
    for start, end, new in uniq:
        text = text[:start] + new + text[end:]
    return text


def self_test():
    """Assert this tool still recognises, declines, maps and rewrites correctly.

    WHY THIS IS NOT OPTIONAL HERE, of all the lints. The other three REPORT; this
    one also REWRITES governed documents, so a defect does not merely fail to
    catch drift — it replaces a correct anchor with a wrong one and prints
    success. The header lists four occasions on which it did exactly that: 27
    anchors rewritten across five ADRs because a `rev:` qualifier reached the
    ownership test, a compound citation left `:1040, 1039, 1053` behind, one
    span applied twice turning `:2000` into `:20000`, and a provenance sentence
    that wraps a line carrying the sibling product's range. Every one of those is
    a case below, in the direction it failed.

    All of it runs on pure functions with synthetic input — no repository, no
    commits, no `git` — which is what lets this run in the same job immediately
    before the check it verifies, as TESTING_POLICY rule 4 requires.
    """
    failures = checked = 0

    def check(label, got, want):
        nonlocal failures, checked
        checked += 1
        if got != want:
            failures += 1
            print(f"self-test FAIL: {label}: got {got!r}, want {want!r}", file=sys.stderr)

    # --- 1. OWNERSHIP: what this tool may rewrite, and what it must not -----
    # `citations()` returns only the ones it claims, so an empty list IS the
    # "declined" verdict. Each declined case below is a spelling that, once
    # rewritten, corrupts something this tool does not own.
    def cited(text):
        return [(w, t, [a for a in anchors]) for (w, t, _s, anchors) in citations(text)]

    check("plain anchor is ours",
          cited("see src/PluginProcessor.cpp:695-752 for the mixer"),
          [("src/PluginProcessor.cpp:695-752", "src/PluginProcessor.cpp", [(695, 752)])])
    check("single-line anchor is ours",
          cited("src/dsp/AnamorphEngine.h:38"),
          [("src/dsp/AnamorphEngine.h:38", "src/dsp/AnamorphEngine.h", [(38, None)])])
    # THE COMPOUND FORM. An early version rewrote the first anchor and left the
    # bare trailing numbers alone, producing an internally contradictory citation
    # from the tool whose job is keeping them true.
    check("compound anchor list is one citation with three anchors",
          cited("src/PluginProcessor.cpp:708-709, 851, 1208"),
          [("src/PluginProcessor.cpp:708-709, 851, 1208", "src/PluginProcessor.cpp",
            [(708, 709), (851, None), (1208, None)])])
    # A PINNED REVISION IS NOT ABOUT THIS WORKING TREE. This is the shape that
    # rewrote 27 anchors across five ADRs.
    check("revision-qualified anchor is declined",
          cited("7686204:src/PluginProcessor.cpp:485-491"), [])
    check("checkout-qualified anchor is declined by the prefix capture",
          cited("othertree:src/PluginProcessor.cpp:485-491"), [])
    # THE LOOKBEHIND, and it is a SEPARATE guard from the prefix capture — this
    # is the exact spelling that rewrote 27 anchors. `<checkout>:` cannot be
    # captured as a prefix (`<` and `>` are outside that class), so without the
    # lookbehind the match simply starts at `src/…`, classifies as OURS because
    # `prefix` is None, and the sibling's line numbers are rewritten using this
    # tree's code movement. Blocking the start is not enough on its own either:
    # the scan would retry at `rc/PluginProcessor.cpp`, which the lookbehind also
    # refuses because a word character precedes it.
    check("an angle-bracketed checkout qualifier is declined by the lookbehind",
          cited("<checkout>:src/PluginProcessor.cpp:485-491"), [])
    # ...while the punctuation the documents ACTUALLY put in front of a citation
    # must NOT block it. A lookbehind wide enough to catch every qualifier
    # catches the gate itself, and the loss is silent — the anchors simply stop
    # being checked, and a run over a document full of them still prints a
    # confident count. The two below are the spellings that sit DIRECTLY against
    # the path in `docs/` (a backtick, 18 occurrences; a bare `(`, 1) and are
    # therefore the ones a widened lookbehind would swallow; the commoner `; `
    # and `| ` forms put a space in between and are not at risk.
    for lead, label in (("`", "backtick"), ("(", "bare parenthesis"),
                        ("; ", "semicolon and space"), ("| ", "table cell")):
        check(f"a citation preceded by a {label} is still ours",
              [c[1] for c in cited(f"{lead}src/PluginProcessor.cpp:485")],
              ["src/PluginProcessor.cpp"])
    check("a foreign root is declined",
          cited("other/checkout/src/PluginProcessor.cpp:12"), [])
    check("the sibling product's editor path is declined",
          cited("src/gui/PluginEditor.cpp:1-912"), [])
    check("a bare file name is declined (ambiguous across checkouts)",
          cited("PluginProcessor.cpp:7"), [])
    check("an untracked path is declined",
          cited("src/NotTracked.cpp:7"), [])
    # The documented spelling for a PROSE EXAMPLE of a citation. If this ever
    # starts being claimed, `DOCUMENTATION_COVERAGE.md`'s worked examples get
    # rewritten and the sentences stop meaning what they say — which happened.
    check("the prose-example path is declined",
          cited("write examples as some/file.cpp:107"), [])
    check("an absolute path is declined",
          cited("/abs/src/PluginProcessor.cpp:7"), [])

    # --- 2. PROVENANCE BLOCKS: the sibling's anchors, never ours ------------
    check("a tracked anchor on the provenance line is declined",
          cited("// Provenance (ADR-0009): adapted from Anabasis src/PluginProcessor.cpp:1-912 @ abc."),
          [])
    # THE WRAP IS THE POINT. Line-scoped exclusion covered the marker's own line
    # and nothing else; a sentence that wraps onto the next line put the
    # sibling's range outside the exclusion.
    check("a tracked anchor on a wrapped continuation line is declined",
          cited("// Provenance (ADR-0009): adapted from the sibling product\n"
                "// src/PluginProcessor.cpp:36-175 @ abc123.\n"),
          [])
    check("markdown provenance wraps without a comment marker",
          cited("Provenance (ADR-0009): adapted from the sibling\n"
                "src/PluginProcessor.cpp:36-175 @ abc123.\n"),
          [])
    # ...AND THE BLOCK MUST END. `#pragma once` under a `//` marker stripped to
    # `pragma once`, read as a continuation, and swallowed real content — so a
    # genuine citation below it went unchecked.
    check("a differently-prefixed line ends the block, so the next citation is ours",
          [c[1] for c in cited("// Provenance (ADR-0009): adapted from the sibling.\n"
                               "#pragma once\n"
                               "// see src/PluginProcessor.cpp:42\n")],
          ["src/PluginProcessor.cpp"])
    # ...AND SO DOES AN EMPTY-BODIED LINE WEARING THE SAME PREFIX. That is the
    # `//` separator under the sentence in the sibling's editor header, and it is
    # the boundary a same-prefix block relies on; without it the block runs to
    # the end of the file and every citation below goes silently unchecked.
    check("a bare `//` separator ends a same-prefix block",
          [c[1] for c in cited("// Provenance (ADR-0009): adapted from the sibling.\n"
                               "//\n"
                               "// see src/PluginProcessor.cpp:42\n")],
          ["src/PluginProcessor.cpp"])
    check("a blank line ends a markdown provenance block",
          [c[1] for c in cited("Provenance (ADR-0009): adapted from the sibling.\n"
                               "\n"
                               "see src/PluginProcessor.cpp:42\n")],
          ["src/PluginProcessor.cpp"])

    # --- 3. THE LINE MAP: every re-anchor this tool writes is its answer -----
    def hunks(*hs):
        return line_map_from_diff("".join(f"@@ -{h} @@\n" for h in hs))

    # A PURE INSERTION SITS AFTER `start`, so `start` itself does not move. The
    # general test read `start + 0 - 1 < n` and shifted it one line too far.
    m = hunks("9,0 +10,2")
    check("pure insertion: the line above it does not move", m(9), 9)
    check("pure insertion: the line below it moves by the inserted count", m(10), 12)
    # A PREPEND IS SPELLED `-0,0`, not `-1,0` — git counts the insertion as
    # sitting after old line ZERO. The hunk shapes here are the ones `git diff
    # -U0` actually emits, verified against it; a self-test written against a
    # plausible-looking spelling proves the parser handles input it never sees.
    m = hunks("0,0 +1,3")
    check("prepend shifts the whole file", m(1), 4)
    # The OMITTED-COUNT form: `+3` with no `,n` means exactly one line, and the
    # regex has to default it rather than read None.
    m = hunks("2,0 +3")
    check("single-line insertion: the line above does not move", m(2), 2)
    check("single-line insertion: the line below moves by one", m(3), 4)
    # A line INSIDE an edited hunk has no honest answer — the text it named may
    # not exist any more — and `None` is what makes the caller report UNMAPPABLE
    # instead of inventing a number.
    m = hunks("10,5 +10,7")
    check("a line inside an edited hunk is unmappable", m(12), None)
    check("a line above an edited hunk is unchanged", m(9), 9)
    check("a line below an edited hunk shifts by the delta", m(20), 22)
    m = hunks("10,5 +10,2")
    check("a shrinking hunk shifts later lines back", m(20), 17)
    m = hunks("5,0 +6,1", "50,0 +52,10")
    check("multiple hunks accumulate", m(60), 71)
    check("only hunks above the line count", m(10), 11)
    check("an empty diff is the identity map", line_map_from_diff("")(42), 42)

    # --- 4. THE REWRITE: spans, not strings ---------------------------------
    # `str.replace` matches a PREFIX: repairing `:107` also mangles `:1076`.
    check("edits apply right-to-left by span",
          apply_edits("a:1 b:2", [(1, 3, ":10"), (5, 7, ":20")]), "a:10 b:20")
    # ONE SPAN IS SAFE ONCE. The same edit queued twice spliced the replacement
    # into the middle of itself: `…cpp:2000` became `…cpp:20000`.
    check("a duplicate edit is applied once, not twice",
          apply_edits("x:2000", [(1, 6, ":3000"), (1, 6, ":3000")]), "x:3000")
    try:
        apply_edits("abcdefgh", [(1, 5, "X"), (3, 7, "Y")])
        check("overlapping spans are refused", "no exception", "ValueError")
    except ValueError:
        check("overlapping spans are refused", "ValueError", "ValueError")

    # SPANS ARE NOT CITATIONS, which is the pair of units the `--fix` summary
    # used to subtract one from the other. A document that spells ONE citation
    # twice yields one spelling with TWO spans, so the count-mismatch branch
    # queues one edit per occurrence while counting one drifted citation -- and
    # the withheld branch's `fixable -= len(edits)` then took 2 off a total that
    # had gained 1, driving the closing "re-anchored N citation(s)" line
    # NEGATIVE and over-reporting `withheld`. Built through the real parser
    # rather than asserted about it, because the divergence is a property of
    # what `citations()` returns for a repeated spelling, not of the arithmetic
    # that consumes it.
    twice = ("see `src/PluginProcessor.cpp:10-20` and again "
             "`src/PluginProcessor.cpp:10-20` here.")
    by_spelling = {}
    for (whole, _t, span, _a) in citations(twice):
        by_spelling.setdefault(whole, []).append(span)
    check("one citation spelled twice is ONE spelling with TWO spans",
          [(w, len(sp)) for w, sp in by_spelling.items()],
          [("src/PluginProcessor.cpp:10-20", 2)])
    # ...and both spans really do carry the same rewrite, which is WHY the
    # branch queues one edit each rather than one for the citation.
    rebuilt = "src/PluginProcessor.cpp:30-40"
    moved = apply_edits(twice, [(a, b, rebuilt)
                                for (a, b) in by_spelling["src/PluginProcessor.cpp:10-20"]])
    check("both occurrences move together", moved.count(rebuilt), 2)

    # --- 5. DECLARED RE-AIMS: good for one transition, then reported --------
    doc = "docs/EXAMPLE.md"
    old, new = "src/PluginProcessor.cpp:100", "src/PluginProcessor.cpp:200"
    saved = dict(DELIBERATE_REAIMS)
    try:
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS[(doc, new)] = ""
        check("a declaration naming the current spelling is honoured",
              is_declared_reaim(doc, old, new), True)
        # THE ENTRY MUST NOT SURVIVE ITS OWN TRANSITION. Once the base carries
        # the re-aimed spelling, `200 == 200` kept matching and swallowed the
        # drift of every later commit that moved that code. The declaration is
        # left naming `new` here ON PURPOSE: with the guard removed this case
        # returns True, which is what makes it a test of the guard rather than
        # of the set lookup.
        check("an unchanged spelling is drift, not a re-aim, even when declared",
              is_declared_reaim(doc, new, new), False)
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS[(doc, old)] = ""
        check("a declaration naming the base spelling is honoured too",
              is_declared_reaim(doc, old, new), True)
        DELIBERATE_REAIMS.clear()
        check("an undeclared re-spelling is drift",
              is_declared_reaim(doc, old, new), False)
        check("a declaration for another document does not apply",
              is_declared_reaim("docs/OTHER.md", old, new), False)
    finally:
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS.update(saved)

    # --- 6. THE SCANNED SET still contains what the gate claims -------------
    # `doc_files()` shells out to `git ls-files`, so this one needs the
    # repository — but only to confirm the scan is non-empty and reaches BOTH
    # halves. A tool that scans nothing reports every anchor as verified.
    scanned = doc_files()
    check("the scan reaches the documents", any(p.endswith(".md") for p in scanned), True)
    check("the scan reaches the tracked source too",
          any(p in TRACKED for p in scanned), True)
    check("worklogs stay out of scope",
          any(p.startswith("worklogs/") for p in scanned), False)

    # --- 7. THE GOVERNED SCRIPTS ARE IN SCOPE -------------------------------
    # They were outside it once, and the documents kept citing them the whole
    # time -- 19 anchors across nine files that the gate could not see. Dropping
    # one from `TRACKED` costs nothing visible: the citations still read fine and
    # the run still prints a confident count, minus the anchors it stopped
    # checking. So each is asserted by name rather than by "at least one script".
    for script in ("scripts/run-pluginval.sh", "scripts/run-tests.sh",
                   "scripts/setup-linux.sh", "scripts/build.sh"):
        check(f"{script} is a citation target",
              classify(None, script), script)
        check(f"{script} is scanned for its own citations too",
              script in scanned, True)
    # A live anchor into one of them classifies end to end, not just in isolation.
    check("a script anchor is claimed by the parser",
          [c[1] for c in citations("see `scripts/run-pluginval.sh:154-176` for the retry")],
          ["scripts/run-pluginval.sh"])
    # ...and the guard rails that made scripts safe to add still hold: a fetched
    # or generated file must never become a rewrite target, and a bare file name
    # is still ambiguous across checkouts however governed the file is.
    for foreign in ("build/_deps/juce-src/README.md", "build-san/scripts/run-tests.sh",
                    "_deps/scripts/build.sh"):
        check(f"{foreign} is not a citation target", classify(None, foreign), None)
    check("a bare script name is still declined",
          citations("run-pluginval.sh:154-176"), [])

    # --- 8. THE BUILD DEFINITION AND THE CI WORKFLOW ARE IN SCOPE -----------
    # These joined `TRACKED` after 90 + 6 anchors sat unchecked through a round
    # that moved both files. Asserted by name for the reason section 7 gives:
    # dropping one costs nothing visible -- the citations still read fine and the
    # run still prints a confident count, minus the anchors it stopped checking.
    for governed in ("CMakeLists.txt", ".github/workflows/build.yml"):
        check(f"{governed} is a citation target", classify(None, governed), governed)
        check(f"{governed} is scanned for its own citations too",
              governed in scanned, True)
    # END TO END, not just through `classify()`. `CMakeLists.txt` is the case the
    # old separator-requiring pattern could not express at all: it is a ROOT-level
    # file, so it has no directory to carry, and the pattern rejected it before
    # ownership was ever consulted. Listing it without this assertion would put a
    # file in `TRACKED` that is never matched -- present in the list, absent from
    # the gate, and indistinguishable from working.
    check("a root-level CMakeLists anchor is claimed by the parser",
          [c[1] for c in citations("the option is `CMakeLists.txt:27,305` today")],
          ["CMakeLists.txt"])
    check("a compound CMakeLists anchor keeps every anchor",
          [c[3] for c in citations("CMakeLists.txt:14,250-252,274-275")],
          [[(14, None), (250, 252), (274, 275)]])
    check("a workflow anchor is claimed by the parser",
          [c[1] for c in citations("see `.github/workflows/build.yml:1621-1623`")],
          [".github/workflows/build.yml"])
    # The guard rails that made these two safe to add. The FETCHED CMakeLists is
    # the one that matters most: `--fix` rewrites what it claims, and the JUCE
    # tree lands inside the workspace at a path whose basename is identical.
    for foreign in ("build/_deps/juce-src/CMakeLists.txt",
                    "build-clang/_deps/juce-src/CMakeLists.txt"):
        check(f"{foreign} is not a citation target", classify(None, foreign), None)
    check("a revision-pinned CMakeLists anchor is declined",
          citations("7686204:CMakeLists.txt:14"), [])
    check("a sibling-checkout CMakeLists anchor is declined",
          citations("anabasis:CMakeLists.txt:14"), [])
    # A BARE `build.yml:` MUST STAY DECLINED, and this is load-bearing rather
    # than incidental. `DOCUMENTATION_COVERAGE.md` records past re-anchorings as
    # prose -- "`build.yml:288,758,1141` -> the three per-OS ..." -- which are
    # HISTORY, not evidence: rewriting them would change the numbers the sentence
    # is about, the exact corruption the header's "prose examples" rule exists to
    # prevent. The workflow is tracked under its full path, so the bare spelling
    # those sentences use is what keeps them out of reach.
    check("a bare workflow name is still declined",
          citations("build.yml:288,758,1141"), [])
    # THE ESCAPE HATCH FOR WORKED EXAMPLES MUST STAY AN ESCAPE HATCH. Tracking
    # the workflow made it a scanned document as well as a rewrite target, and
    # its own comment block illustrates what an evidence anchor looks like. An
    # illustration spelled with a governed path is indistinguishable from
    # evidence to this tool, so that example names a path outside `TRACKED` --
    # the remedy the header prescribes. Asserted here so the spelling those
    # examples rely on cannot quietly become claimable by a future addition.
    check("the example path used for illustrations is declined",
          classify(None, "some/file.cpp"), None)
    check("an example anchor is not claimed end to end",
          citations("Evidence anchors (`some/file.cpp:695-752`) are exact"), [])

    # --- 8b. `--fix` REPORTS THE DECLARATIONS IT IS ABOUT TO INVALIDATE ------
    # Section 9 below already fails on a dead declaration, and that gate holds.
    # What it cannot do is tell the person who caused it: it runs in CI, minutes
    # later, in a different job, and knows only that an entry is dead -- while
    # `--fix`, which killed it, is holding the replacement spelling. These cases
    # assert that half, in both directions, because a warning that fires on
    # every rewrite would be as useless as one that never fires.
    #
    # THE FIXTURE IS SYNTHETIC AND IS INSTALLED OVER AN EMPTIED TABLE, for the
    # same reason the span cases below are synthetic and one more besides. A
    # declaration reaches its terminal state by being DELETED -- the header calls
    # the list going empty "the expected end state of every entry here", and a
    # run that finds one retired prints "delete it" -- so a fixture built by
    # looking a REAL entry up stops being runnable on the day the tool's own
    # advice is followed. This block used to open with
    # `next(w for (d, w) in DELIBERATE_REAIMS if d == "docs/FUTURE_RISKS.md")`,
    # and `next()` on an exhausted generator raises `StopIteration`: the whole
    # self-test would die with a traceback rather than report a case, and the
    # checker whose job is to prove it can still fail would be unrunnable for a
    # reason that is not a defect. Emptying the table first is the second half:
    # these cases then run in exactly that retired state on every run, so the
    # coupling cannot come back unnoticed. `invalidated_reaims` is purely
    # textual -- it reads no file -- so nothing here needs to exist on disk.
    saved = dict(DELIBERATE_REAIMS)
    DELIBERATE_REAIMS.clear()

    doc = "docs/Synthetic.md"
    declared = "synthetic/One.txt:147-197"
    path = declared.split(":", 1)[0]
    moved = f"{path}:900-950"

    before = f"Evidence [Verified]: {declared} (the retry)."
    at = before.index(declared)
    edit = [(at, at + len(declared), moved)]
    after = f"Evidence [Verified]: {moved} (the retry)."

    # THE RETIRED STATE ITSELF, asserted before anything is declared: with the
    # table empty there is nothing to invalidate, and the answer is an empty
    # list rather than an exception. This is the state the block above is
    # written to survive, named here so it is a case and not an assumption.
    check("with every declaration retired, a rewrite invalidates nothing",
          invalidated_reaims(doc, before, after, edit), [])

    DELIBERATE_REAIMS[(doc, declared)] = "run_one_pass"
    check("--fix reports a declaration its rewrite invalidates, with the replacement",
          invalidated_reaims(doc, before, after, edit),
          [(declared, moved)])
    check("...and stays silent when the declared spelling survives the rewrite",
          invalidated_reaims(doc, before, before, [(0, 5, "src/PluginProcessor.cpp:1-2")]),
          [])
    check("...and stays silent for a document that declares nothing",
          invalidated_reaims("docs/NOT_DECLARED.md", before, moved, edit),
          [])

    # THE CASE THE FIRST IMPLEMENTATION GOT WRONG. It matched the replacement by
    # TRACKED PATH, which is correct only while a document rewrites that path
    # once. `docs/procedures/BUILD.md` carries two `CMakeLists.txt` anchors, and
    # the path form offered the FIRST one's replacement for the SECOND -- on real
    # drift, the same day the warning was added. Matching by span is exact.
    #
    # The strings below are SYNTHETIC on purpose -- `synthetic/Two.txt`, a path
    # this repository does not have. The first draft of this case spelled them
    # with the real `CMakeLists.txt` anchors it was modelled on, and a later
    # bulk edit of the DELIBERATE_REAIMS table rewrote the test's own literals
    # along with the table's, so the case declared one spelling and asserted
    # another. A fixture that a search-and-replace over real data can reach is
    # not a fixture.
    A_OLD, A_NEW = "synthetic/Two.txt:27, 317", "synthetic/Two.txt:27, 321"
    B_OLD, B_NEW = "synthetic/Two.txt:286-296", "synthetic/Two.txt:290-300"
    two = f"first {A_OLD} then {B_OLD} done"
    a1, a2 = two.index(A_OLD), two.index(B_OLD)
    DELIBERATE_REAIMS.clear()
    DELIBERATE_REAIMS.update({("docs/procedures/BUILD.md", B_OLD): ""})
    check("with two same-path rewrites, the replacement is the one whose span matches",
          invalidated_reaims("docs/procedures/BUILD.md", two,
                             f"first {A_NEW} then {B_NEW} done",
                             [(a1, a1 + len(A_OLD), A_NEW),
                              (a2, a2 + len(B_OLD), B_NEW)]),
          [(B_OLD, B_NEW)])

    # THE SAME SPELLING TWICE, with only the SECOND occurrence rewritten. A
    # lookup that stops at the first occurrence finds no edit overlapping it and
    # reports the entry dead with no replacement -- the one thing this warning
    # exists to supply. `check-citations` already handles documents that spell an
    # anchor more than once ("spelled 2x in this document; all move together"),
    # so this is a shape the tree can produce, not a hypothetical.
    twice = f"see {B_OLD} and again {B_OLD} here"
    second = twice.rindex(B_OLD)
    DELIBERATE_REAIMS.clear()
    DELIBERATE_REAIMS.update({("docs/procedures/BUILD.md", B_OLD): ""})
    check("a declaration spelled twice finds the replacement at the LATER occurrence",
          invalidated_reaims("docs/procedures/BUILD.md", twice,
                             f"see {B_NEW} and again {B_NEW} here",
                             [(second, second + len(B_OLD), B_NEW)]),
          [(B_OLD, B_NEW)])
    DELIBERATE_REAIMS.clear()
    DELIBERATE_REAIMS.update(saved)

    # --- 8c. EVERY DECLARATION STILL LANDS ON WHAT IT CLAIMS ----------------
    # A declaration switches the drift comparison OFF for its anchor. That is
    # its purpose and it cannot be otherwise -- a deliberate re-aim is textually
    # indistinguishable from drift. The consequence, until 2026-08-18, was that
    # NOTHING checked the aim: the run printed "verify the aim by hand" and
    # section 9 asserted only that the declared string appeared somewhere in its
    # document. Four anchors declared in one round with line numbers computed
    # before the file settled all pointed at unrelated lines and all stayed
    # green, because the declaration was exactly what stopped anyone looking.
    # Two MORE were found the moment this check existed.
    #
    # So the cases below drive the real resolver over the real declarations, and
    # then over a deliberately wrong one.
    real_problems, _unverifiable = verify_reaim_targets()
    check("every declared re-aim in this tree lands on what it claims",
          real_problems, [])

    saved = dict(DELIBERATE_REAIMS)
    try:
        DELIBERATE_REAIMS.clear()
        # A real file, a real line, an expectation that is NOT there.
        DELIBERATE_REAIMS[("docs/EXAMPLE.md", "CMakeLists.txt:14")] = "this-is-not-on-line-14"
        wrong, _ = verify_reaim_targets()
        check("a declaration whose expectation is absent from the cited lines is reported",
              len(wrong), 1)

        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS[("docs/EXAMPLE.md", "CMakeLists.txt:14")] = "project(Anamorph"
        right, _ = verify_reaim_targets()
        check("...and one whose expectation IS there is not", right, [])

        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS[("docs/EXAMPLE.md", "no/such/file.cpp:1")] = "anything"
        missing, _ = verify_reaim_targets()
        check("an unreadable cited file is a problem, not a pass", len(missing), 1)

        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS[("docs/EXAMPLE.md", "CMakeLists.txt:14")] = ""
        none_declared, unver = verify_reaim_targets()
        check("an entry declaring no expectation is reported as unverifiable, not clean",
              (none_declared, len(unver)), ([], 1))
    finally:
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS.update(saved)

    # --- 8d. THE AIM CHECK MUST NOT DISABLE THE REPAIR PATH ------------------
    # Section 8c gave this tool a check on its own declarations. It was wired
    # in ahead of everything else and returned 2 for EVERY mode, which stopped
    # `--fix` from repairing the citations that had nothing to do with the
    # declaration. Measured on a worktree at 33333fe: inserting ONE line into
    # `build.yml` misaimed three declared entries, two of them single-line
    # anchors, and `--fix` exited 2 having rewritten nothing at all. With the
    # split below it re-anchors the unrelated drift and still exits 2.
    #
    # Three decisions come out of this, and they fail in different directions:
    # stopping `--fix` is the defect being fixed; letting `--fix` report SUCCESS
    # over a misaimed declaration would be a worse one; and rewriting a document
    # that OWNS a misaimed declaration corrupts it. The cases below pin the first
    # behaviourally and the other two at their call sites -- see the note there
    # for why, and for what that does not cover.
    P = [("docs/EXAMPLE.md", "CMakeLists.txt:14", "x", "why")]
    check("--check STOPS on a misaimed declaration", reaim_stops_the_run(P, False), True)
    check("--fix does NOT stop on one -- the rest of the run is still repairable",
          reaim_stops_the_run(P, True), False)
    check("no misaimed declaration, no stop (--check)", reaim_stops_the_run([], False), False)
    check("no misaimed declaration, no stop (--fix)", reaim_stops_the_run([], True), False)
    check("--fix that carried past a misaimed declaration still FAILS",
          fix_exit_code(0, P), 2)
    check("...and still fails when it also left unmappable citations",
          fix_exit_code(3, P), 2)
    check("a clean --fix succeeds", fix_exit_code(0, []), 0)
    check("an unmappable citation alone still fails --fix", fix_exit_code(1, []), 2)

    # ...AND THE WIRING, not only the two helpers. Pinning the helpers alone left
    # the actual defect reachable: restoring `if reaim_problems:` at the call
    # site in `main()` -- the one-character regression this section is named for
    # -- passed every case above. So the cases below drive `main()` itself,
    # through a stubbed `verify_reaim_targets`, and assert the OUTCOME: `--check`
    # stops before reading a base revision, `--fix` does not.
    #
    # The tell is which failure comes back. With no base revision reachable,
    # `--fix` gets as far as `git show` and reports "nothing to check against";
    # a run that stopped at the aim check never gets there and says so instead.
    import io                       # local: the tool does not otherwise capture output
    import contextlib
    saved_argv = sys.argv[:]
    saved_reaims = dict(DELIBERATE_REAIMS)
    try:
        def stub_verify():
            return [("docs/EXAMPLE.md", "CMakeLists.txt:14", "x", "wrong on purpose")], []
        real_verify = globals()["verify_reaim_targets"]
        globals()["verify_reaim_targets"] = stub_verify
        for mode, must_reach_base in (("--check", False), ("--fix", True)):
            sys.argv = ["check-citations.py", mode, "--base", "no/such/revision"]
            out, err = io.StringIO(), io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                rc = main()
            text = out.getvalue() + err.getvalue()
            reached = "nothing to check against" in text
            check(f"{mode}: rc is 2 over a misaimed declaration", rc, 2)
            check(f"{mode}: {'reaches' if must_reach_base else 'stops before'} the base "
                  f"revision", reached, must_reach_base)
            check(f"{mode} reports the misaimed declaration as "
                  f"{'a warning' if mode == '--fix' else 'an error'}",
                  ("::warning::" in text) if mode == "--fix" else ("::error::" in text),
                  True)
    finally:
        globals()["verify_reaim_targets"] = real_verify
        sys.argv = saved_argv
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS.update(saved_reaims)

    # THE OTHER TWO DECISIONS ARE PINNED AT THEIR CALL SITES, and this is a
    # weaker instrument than the cases above -- stated plainly rather than
    # dressed up. Both need a run that reaches the END of `--fix`, which needs a
    # real base revision and would REWRITE documents; `TESTING_POLICY` rule 4
    # forbids a self-test that depends on a base revision, and a self-test that
    # edits the tree would be worse than the defect. So what is asserted is that
    # each decision is still consulted where it has to be. It catches the
    # realistic regression -- someone re-simplifying the condition, which is how
    # the original defect was written -- and it deliberately does not pretend to
    # be behavioural. If a refactor renames these, this test is supposed to fail
    # and be updated with them.
    # SEARCHED IN `main()` ONLY, not in the whole file -- the strings below also
    # appear a few lines up as the literals being searched FOR, so a whole-file
    # search matches itself and passes however `main()` is mutated. (Caught by
    # mutating this file and watching all three cases stay green.)
    own_source = read(__file__).split("\ndef main(")[-1]
    for label, call in [
        ("--fix's exit code still asks fix_exit_code, so a run that warned cannot "
         "report success", "return fix_exit_code(unmappable, reaim_problems)"),
        ("the rewrite still skips a document that owns a misaimed declaration",
         "if args.fix and edits and doc in misaimed_docs:"),
        ("...and the mode split is still consulted rather than re-simplified",
         "if reaim_stops_the_run(reaim_problems, args.fix):"),
    ]:
        check(f"call site intact: {label}", call in own_source, True)

    # THE UNIT THE WITHHELD SUMMARY COUNTS IN, pinned the same way and for the
    # same reason: reaching it needs a run that gets to the END of `--fix`, with
    # a real base revision and a document to rewrite. The case above proves the
    # two units diverge; these prove the branch still spends the right one.
    # `len(edits)` is asserted ABSENT from both, because that spelling is the
    # defect and it is the one a re-simplification would reintroduce.
    for label, call in [
        ("the withheld total gives back CITATIONS, not edit spans",
         "fixable -= doc_fixable"),
        ("...and `withheld` accumulates the same unit", "withheld += doc_fixable"),
        ("...and the warning it prints counts that unit too",
         "NOT re-anchored ({doc_fixable} citation(s) left "),
    ]:
        check(f"call site intact: {label}", call in own_source, True)
    for gone in ("fixable -= len(edits)", "withheld += len(edits)",
                 "NOT re-anchored ({len(edits)} citation(s) left "):
        check(f"the span-count spelling is gone: {gone}", gone in own_source, False)
    # ...and the counter has to be FED, which the three cases above cannot see:
    # a `doc_fixable` that is never incremented subtracts nothing and reports
    # nothing withheld, which is wrong in the other direction. Both drifted
    # branches increment `fixable`, so both must increment its per-document
    # counterpart. `doc_fixable += 1` CONTAINS `fixable += 1`, hence the
    # subtraction. A third branch is expected to fail this and be added to it.
    paired = own_source.count("doc_fixable += 1")
    check("each drifted-citation increment has its per-document counterpart",
          (own_source.count("fixable += 1") - paired, paired), (2, 2))

    # --- 9. EVERY DECLARATION NAMES A SPELLING ITS DOCUMENT REALLY CARRIES ---
    # A declaration excuses a mismatch, so it is consulted ONLY when one occurs.
    # That makes a stale entry invisible from the branch it was written on: the
    # citation it names has already been corrected, so nothing mismatches, so
    # nothing looks at the entry -- while against the MERGE BASE, which is what a
    # push to the default branch compares, the real mismatch finds no matching
    # declaration and the run exits 1. That is not hypothetical; it shipped
    # twice in one PR, both times as an anchor recomputed in a later commit
    # without its declaration following. An entry naming a string no document
    # contains cannot be excusing anything, so it is always a defect -- either
    # the anchor moved on and the entry should have moved with it, or the entry
    # outlived its transition and should be deleted.
    for doc, whole in sorted(DELIBERATE_REAIMS):
        try:
            body = read(doc)
        except OSError:
            check(f"DELIBERATE_REAIMS names a readable document ({doc})", False, True)
            continue
        check(f"DELIBERATE_REAIMS entry is live: {doc} :: {whole}",
              whole in body, True)

    if failures:
        print(f"\ncheck-citations: {failures} of {checked} self-test case(s) failed.",
              file=sys.stderr)
        return 1
    print(f"check-citations: self-test passed ({checked} cases).")
    return 0


def main():
    ap = argparse.ArgumentParser(description="Verify documentation evidence anchors.")
    ap.add_argument("--base", default="origin/main",
                    help="revision the citations were last verified against")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true",
                      help="report drifted citations (the default)")
    mode.add_argument("--fix", action="store_true",
                      help="re-anchor drifted citations instead of only reporting")
    mode.add_argument("--self-test", action="store_true",
                      help="verify this tool's own recognition, mapping and rewriting")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    # EVERY DECLARED RE-AIM IS CHECKED BEFORE ANYTHING ELSE, and it needs no base
    # revision because it is not a question about drift: it asks whether each
    # entry still lands on the thing it says it lands on, in the tree as it is
    # now. This runs first so a misaimed declaration is reported as itself rather
    # than as a silence somewhere downstream -- a declaration is the one
    # construct in this file that turns a check OFF, so it is the one that most
    # needs a check of its own.
    #
    # IT MUST NOT DISABLE `--fix`, and until 2026-08-19 it did. Returning 2 here
    # for EVERY mode meant that the one thing which repairs drift refused to run
    # whenever a declaration had drifted -- and drifting is what these anchors
    # do: two are single lines in `build.yml` (`:1667` expecting `macos:`), so
    # any insertion above one moves them. The whole run then stopped, including
    # the re-anchoring of every citation that had nothing to do with the
    # declaration, and the `invalidated_reaims` warning below -- which prints a
    # replacement spelling during a rewrite -- became unreachable along with it.
    #
    # So the DISTINCTION IS BY MODE, and it is the same one the rewriter already
    # draws a few hundred lines down for invalidated declarations: `--check`
    # VERIFIES, so a misaimed declaration is an error and stops the run;
    # `--fix` REPAIRS, so it says the same thing as a warning, does its work,
    # and then still exits non-zero, because the declaration is in this script
    # and no rewrite of a document can correct it. Nothing is weakened: the CI
    # step runs `--check` (there is no `--fix` in any workflow), which is where
    # the hard failure lives and where it stays.
    reaim_problems, reaim_unverifiable = verify_reaim_targets()
    misaimed_docs = {doc for doc, _whole, _expect, _why in reaim_problems}
    for doc, whole in reaim_unverifiable:
        print(f"check-citations: note — DELIBERATE_REAIMS entry {doc}: {whole} declares no "
              f"expected content, so its aim is NOT verified. Give it a substring a reader "
              f"following the reference should find.")
    if reaim_problems:
        for doc, whole, expect, why in reaim_problems:
            if args.fix:
                print(f"::warning::{doc}: the declared re-aim {whole} does not contain "
                      f"{expect!r} — {why}")
            else:
                print(f"::error::{doc}: the declared re-aim {whole} does not contain "
                      f"{expect!r} — {why}", file=sys.stderr)
        if reaim_stops_the_run(reaim_problems, args.fix):
            print(f"\ncheck-citations: {len(reaim_problems)} declared re-aim(s) point at "
                  f"something other than what they claim. A declaration switches the drift "
                  f"check OFF for its anchor, so a wrong one is invisible in every other way — "
                  f"re-derive the line numbers against the current file and fix BOTH the "
                  f"document and the declaration.", file=sys.stderr)
            return 2
        print(f"check-citations: {len(reaim_problems)} declared re-aim(s) point at something "
              f"other than what they claim. Re-anchoring continues below — a declaration lives "
              f"in this script, not in a document, so no rewrite can repair it and this run "
              f"will still exit non-zero. Re-derive those line numbers against the current "
              f"file afterwards.")

    base_src, now_src = {}, {}
    for path in TRACKED:
        if not os.path.exists(path):
            continue
        b = git_show(args.base, path)
        if b is None:
            print(f"check-citations: {path} does not exist at {args.base}; skipping",
                  file=sys.stderr)
            continue
        base_src[path] = b
        now_src[path] = read(path).split("\n")

    if not base_src:
        print(f"check-citations: nothing to check against {args.base}", file=sys.stderr)
        return 2

    maps = {p: build_line_map(args.base, p) for p in base_src}

    total = drifted = fixable = unmappable = unchecked = withheld = 0
    used_reaims = set()
    base_doc_cites = {}                # doc -> the citation spellings the BASE carries
    for doc in doc_files():
        base_text = subprocess.run(["git", "show", f"{args.base}:{doc}"],
                                   capture_output=True, text=True)
        if base_text.returncode != 0:
            continue                       # new document: nothing to drift from
        old_cites = citations(base_text.stdout)
        base_doc_cites[doc] = {c[0] for c in old_cites}
        text = read(doc)
        cur_cites = citations(text)

        # Paired PER PATH, in order of appearance. Neither global position nor
        # the citation string works on its own: a change set may add or remove
        # citations (position breaks), and once a citation has been re-anchored
        # its base spelling is gone from the file, so looking for that string
        # makes every LATER shift invisible — which is precisely how this round's
        # anchors went stale twice. Pairing the Nth `PluginProcessor.cpp`
        # reference in the base with the Nth in the current file survives both.
        by_path_old, by_path_cur = {}, {}
        for c in old_cites:
            by_path_old.setdefault(c[1], []).append(c)
        for c in cur_cites:
            by_path_cur.setdefault(c[1], []).append(c)

        # TWO UNITS, KEPT SIDE BY SIDE. `edits` counts SPANS and `doc_fixable`
        # counts CITATIONS, and they are not the same number: a document that
        # spells one citation twice queues one span per occurrence for one
        # citation (the count-mismatch branch below, which prints "spelled 2x in
        # this document; all move together"). The withheld branch has to give
        # back what the loop added, in the unit it was added in -- it subtracted
        # `len(edits)`, so a twice-spelled citation in a withheld document took 2
        # off a total that had gained 1, and the closing "re-anchored N
        # citation(s)" line could go NEGATIVE while `withheld` over-reported.
        edits, doc_fixable = [], 0
        for tracked, olds in by_path_old.items():
            if tracked not in base_src:
                continue
            curs = by_path_cur.get(tracked, [])
            if len(curs) != len(olds):
                # A change set is allowed to ADD or REMOVE citations, so a count
                # change is not itself a failure. Ordinal pairing is no longer
                # meaningful, though, so fall back to the conservative check:
                # every base citation whose spelling is still present must still
                # be correct. A citation that was re-spelled or removed is beyond
                # what this can judge, and a NEW one has nothing to drift from.
                still = {}
                for (whole_c, _t, span_c, _a) in curs:
                    still.setdefault(whole_c, []).append(span_c)
                # A spelling the BASE carries more than once. `still` is keyed by
                # spelling, so every base occurrence of the same citation resolves
                # to the same span list and the same rewrite — counting each of
                # them would report more citations drifted and fixed than the
                # document contains. `apply_edits` de-duplicates the spans, so
                # this was only ever a reporting defect; it is still the class of
                # imprecision this tool exists to remove from documents.
                counted = set()
                for (whole_o, _t, _span_o, anchors_o) in olds:
                    # `total` counts every base anchor on BOTH paths, and the ones
                    # this path cannot judge are counted again into `unchecked`
                    # and reported. Counting only the judged ones here made the
                    # closing "N of M" mean a different thing per document
                    # depending on which branch it took — the summary of a tool
                    # whose subject is documents saying what they mean.
                    total += len(anchors_o)
                    spans = still.get(whole_o)
                    if not spans:
                        unchecked += len(anchors_o)
                        continue
                    if all(line_of(base_src[tracked], a) == line_of(now_src[tracked], a)
                           and (b is None or line_of(base_src[tracked], b) == line_of(now_src[tracked], b))
                           for (a, b) in anchors_o):
                        continue
                    # Past this point the citation is DRIFTED and will be counted
                    # and queued. A second base occurrence of the same spelling
                    # reaches here with the same spans and the same rebuild, so it
                    # contributes nothing but inflated numbers — its anchors are
                    # already in `total` above, which is the figure that must stay
                    # per-occurrence.
                    if whole_o in counted:
                        continue
                    counted.add(whole_o)
                    # No DELIBERATE_REAIMS check here on purpose: this branch
                    # only reaches citations whose spelling is UNCHANGED (that is
                    # the `still.get(whole_o)` filter above), and a re-aim always
                    # changes the spelling. Anything mismatching here is drift.
                    mapped, movable = [], True
                    for (a, b) in anchors_o:
                        na, nb = maps[tracked](a), (maps[tracked](b) if b else None)
                        if na is None or (b is not None and nb is None):
                            movable = False
                            break
                        mapped.append((na, nb))
                    drifted += 1
                    if not movable:
                        unmappable += 1
                        print(f"  UNMAPPABLE {doc}: {whole_o} "
                              f"(the cited lines were themselves edited — re-aim by hand)")
                        continue
                    rebuilt = f"{tracked}:" + ", ".join(
                        f"{na}-{nb}" if nb else f"{na}" for na, nb in mapped)
                    # One citation, however many times the document spells it.
                    # Identical spellings carry identical anchors, so the same
                    # rewrite is right for each occurrence — but they are ONE
                    # drifted citation, and counting the spans inflated `fixed`
                    # past the number of citations there were to fix.
                    if len(spans) > 1:
                        print(f"    (spelled {len(spans)}× in this document; all move together)")
                    print(f"  DRIFTED {doc}: {whole_o} -> {rebuilt}")
                    edits += [(s, e, rebuilt) for (s, e) in spans]
                    fixable += 1
                    doc_fixable += 1
                print(f"check-citations: note — {doc} now has {len(curs)} `{tracked}` "
                      f"citation(s) where {args.base} had {len(olds)}; the added ones are "
                      f"not checkable against that base.")
                continue

            for (whole_o, _to, _so, anchors_o), (whole_c, _tc, span_c, anchors_c) in zip(olds, curs):
                total += len(anchors_o)
                if len(anchors_o) != len(anchors_c):
                    # COUNTS AS UNMAPPABLE, not merely drifted, and the difference
                    # is the exit code. `--check` returns 1 on any drift; `--fix`
                    # returns `2 if unmappable else 0`. This branch declines to
                    # touch the citation — a `:10` that became `:10-20` is a
                    # judgement about what the prose means, not a line shift — so
                    # counting it as drift alone made `--fix` exit 0 while
                    # promising a clean repair, and the very next `--check` (CI's)
                    # went red on what it left behind. The documented workflow is
                    # "run `--fix` in the SAME change set", so `--fix` saying 0 is
                    # the last word a contributor hears before pushing.
                    print(f"  ANCHOR COUNT {doc}: {whole_o} -> {whole_c}; review by hand")
                    drifted += 1
                    unmappable += 1
                    continue

                # Correct means: the text at the CURRENT anchors is the text the
                # BASE anchors named.
                same = all(line_of(base_src[tracked], a) == line_of(now_src[tracked], a2)
                           and (b is None) == (b2 is None)
                           and (b is None or line_of(base_src[tracked], b) == line_of(now_src[tracked], b2))
                           for (a, b), (a2, b2) in zip(anchors_o, anchors_c))
                if same:
                    continue

                if is_declared_reaim(doc, whole_o, whole_c):
                    # BOTH spellings, because the declaration may have been
                    # written with either (that is what `is_declared_reaim`
                    # accepts). Recording only the current one left an entry
                    # declared with the BASE spelling honoured here and then
                    # ALSO listed in `DELIBERATE_REAIMS - used_reaims`, which
                    # prints "was not needed … delete it" — advice that, taken,
                    # re-breaks the gate. Latent today (every entry happens to be
                    # a current spelling) and cheaper to close than to remember.
                    used_reaims.add((doc, whole_c))
                    used_reaims.add((doc, whole_o))
                    continue

                mapped, movable = [], True
                for (a, b) in anchors_o:
                    na, nb = maps[tracked](a), (maps[tracked](b) if b else None)
                    if na is None or (b is not None and nb is None):
                        movable = False
                        break
                    mapped.append((na, nb))

                drifted += 1
                if not movable:
                    unmappable += 1
                    print(f"  UNMAPPABLE {doc}: {whole_c} "
                          f"(the cited lines were themselves edited — re-aim by hand)")
                    continue
                rebuilt = f"{tracked}:" + ", ".join(
                    f"{na}-{nb}" if nb else f"{na}" for na, nb in mapped)
                print(f"  DRIFTED {doc}: {whole_c} -> {rebuilt}")
                edits.append((span_c[0], span_c[1], rebuilt))
                fixable += 1
                doc_fixable += 1

        if args.fix and edits and doc in misaimed_docs:
            # THE INTERLOCK THE HARD STOP USED TO PROVIDE, kept at DOCUMENT
            # granularity instead of run granularity.
            #
            # Letting `--fix` past a misaimed declaration is the point of the
            # mode split above, but it must not half-rewrite the one document
            # whose declaration is known wrong. A declaration EXCUSES its anchor
            # from the drift comparison, so while it is misaimed that anchor
            # stays where it is while its neighbours in the same sentence move --
            # and the cell ends up asserting two different things about one line.
            # Observed: re-anchoring `build.yml:2209-2265 -> :2210-2266` beside
            # the excused `:2266` left COMPATIBILITY_MATRIX claiming 2266 is both
            # the `macos-intel` job header and the last line of its rationale
            # block. This is the only lint in the repository that WRITES, so a
            # defect here corrupts rather than merely misses.
            #
            # Every OTHER document is still repaired, which is what makes this a
            # narrower interlock than the run-wide `return 2` it replaces.
            print(f"::warning::{doc}: NOT re-anchored ({doc_fixable} citation(s) left "
                  f"alone) because this document owns a misaimed DELIBERATE_REAIMS "
                  f"declaration. Re-deriving one anchor while its excused neighbour "
                  f"stays put is how a sentence ends up naming one line twice. Fix "
                  f"the declaration, then re-run --fix.")
            # NOT COUNTED AS RE-ANCHORED, because nothing was written. `fixable`
            # is incremented per DRIFTED citation above, which is right for
            # `--check` (where it means "repairable") and wrong here. Give back
            # CITATIONS, which is what was added -- `len(edits)` is spans, and
            # the difference is every occurrence past the first.
            fixable -= doc_fixable
            withheld += doc_fixable
        elif args.fix and edits:
            # WHAT THE REWRITE DOES TO THIS DOCUMENT'S OWN DECLARATIONS, checked
            # here rather than left to the self-test.
            #
            # A `DELIBERATE_REAIMS` entry is a claim about a SPELLING, and the
            # rewrite about to happen can invalidate that claim: the anchor an
            # entry names drifts for an unrelated reason -- an edit to the CITED
            # file -- `--fix` re-anchors it correctly, and the entry is left
            # naming a string the document no longer contains. It then excuses
            # nothing, which section 9 of the self-test calls a defect and fails
            # on. That is a real gate and it holds; the problem is WHERE it
            # fires. The self-test runs in CI, minutes later, in a different job,
            # and says only that an entry is dead -- while the tool that killed
            # it was this one, right here, and knows the replacement spelling.
            #
            # Observed twice in one change set (2026-08-18): edits to
            # `run-pluginval.sh` and `CMakeLists.txt` moved anchors that six
            # entries named. So the rewriter reports it itself, naming the new
            # spelling to paste in. A ::warning:: rather than an error --
            # `--fix`'s job is to repair drift, and refusing to do so because a
            # declaration will need an edit would leave BOTH problems in place.
            rewritten = apply_edits(text, edits)
            for whole, replacement in invalidated_reaims(doc, text, rewritten, edits):
                print(f"::warning::{doc}: this re-anchor invalidates the DELIBERATE_REAIMS "
                      f"entry naming `{whole}`"
                      + (f" -- update it to `{replacement}`" if replacement else "")
                      + ". An entry naming a string no document contains excuses nothing, and "
                        "the citation self-test fails on it.")

            with open(doc, "w", encoding="utf-8") as fh:
                fh.write(rewritten)
            # `now_src` IS THE WORKING TREE, and it stopped being so the moment a
            # TRACKED SOURCE joined the scanned set. The snapshot is taken once,
            # before this loop; a `--fix` that rewrites `src/PluginProcessor.cpp`
            # leaves the cached copy holding pre-rewrite lines, and any document
            # processed LATER that cites one of the rewritten lines is then judged
            # against text no longer on disk. Narrow — it needs a citation whose
            # own line is a citation — but it is an invariant, and an invariant
            # this tool holds about a file it just edited is exactly the kind it
            # cannot afford to be casually wrong about.
            if doc in now_src:
                now_src[doc] = read(doc).split("\n")

                # …AND THE REFRESH IS ONLY HALF OF IT. Keeping the cache in step
                # with disk fixes what this run judges NEXT; it does nothing about
                # what the rewrite DID. Re-anchoring a citation that lives inside a
                # tracked source line changes that line's TEXT, and every anchor
                # aimed at that line is verified by text identity — so a document
                # elsewhere pointing there now compares the base's old wording
                # against the new one and reports drift, or gets re-aimed away from
                # a line that never moved. The repair is indistinguishable from the
                # damage, which is this tool's recurring shape.
                #
                # Not silently, then. The lines are named, because a human can
                # settle in seconds what the tool cannot settle at all: whether
                # anything aims at them. Narrow by construction — it needs an
                # anchor whose target line is itself a citation — but that is
                # exactly the shape of the comment in `closePresetUndoBracket`
                # that motivated scanning source in the first place.
                touched = sorted({text.count("\n", 0, a) + 1 for a, _b, _r in edits})
                print(f"check-citations: NOTE — {doc} is a tracked SOURCE file and "
                      f"line(s) {', '.join(str(n) for n in touched)} were rewritten. "
                      f"Any citation elsewhere aimed at those lines is now measured "
                      f"against changed text; re-check them by hand.")

    # A declared re-aim is never silent: it is announced when it is honoured, and
    # announced again when it has stopped being needed, so the list cannot quietly
    # become a set of permanent exemptions.
    # INTERSECTED, because `used_reaims` deliberately holds BOTH spellings of an
    # honoured re-aim (see the `is_declared_reaim` branch) and only one of them is
    # a declaration. Reporting the set raw announced the UNDECLARED spelling too,
    # so a run printed twice as many accepted re-aims as `DELIBERATE_REAIMS` has
    # entries — a tool whose subject is documents saying exactly what they mean,
    # not saying exactly what it means. The intersection is never empty for an
    # honoured re-aim: `is_declared_reaim` returns true only when at least one of
    # the two spellings is literally in the set, so the "never silent" property
    # below survives the narrowing.
    for (doc, whole) in sorted(used_reaims & DELIBERATE_REAIMS.keys()):
        print(f"check-citations: ACCEPTED re-aim {doc}: {whole} "
              f"(declared in DELIBERATE_REAIMS; its aim is checked against "
              f"{DELIBERATE_REAIMS[(doc, whole)]!r} by verify_reaim_targets)")
    # "DELETE IT" IS ADVICE, so it is only given when it is SAFE, and the two
    # cases below were one message until 2026-08-14. An entry is unused against a
    # given base for two entirely different reasons:
    #
    #   * The base already CARRIES the re-aimed spelling — the transition this
    #     entry excused is behind that base, and `is_declared_reaim`'s "the
    #     spelling actually changed" test can never fire for it again from there.
    #   * The base is simply not the one that needed it, and does not carry the
    #     spelling either. It is a declaration for a DIFFERENT base.
    #
    # Neither is an instruction, and the old single message ("delete it once the
    # base carries the re-aim") read as one on both. CI compares against the
    # previous PUSH (`github.event.before`); on that base ~20 of 23 entries have
    # nothing to excuse, because they were written for the merge base. Acting on
    # that log re-breaks the gate against `origin/main` — the tool destroying what
    # it protects, in a green run. Even the first case is only safe when the base
    # in hand is the branch's MERGE BASE: an entry retired against the previous
    # push is very often still live against `main`, which is exactly how this
    # round's `PluginEditor.h:604` declaration was still doing real work.
    for (doc, whole) in sorted(DELIBERATE_REAIMS.keys() - used_reaims):
        if whole in base_doc_cites.get(doc, set()):
            print(f"check-citations: note — DELIBERATE_REAIMS entry {doc}: {whole} was not "
                  f"needed against {args.base}, which already carries the re-aimed "
                  f"spelling. Safe to delete ONLY if that base is the branch's merge "
                  f"base; re-run against that before removing it.")
        else:
            print(f"check-citations: note — DELIBERATE_REAIMS entry {doc}: {whole} was not "
                  f"exercised against {args.base}, which does not carry the re-aimed "
                  f"spelling either — the entry belongs to a different base. Keep it.")

    # Every number below counts base ANCHORS, and `unchecked` is stated rather
    # than netted off, because the difference between "17 verified" and "17
    # verified, 4 beyond what this could judge" is the whole value of the run.
    checked = total - unchecked
    tail = f" ({unchecked} re-spelled or removed, beyond what this run can judge)" if unchecked else ""

    if args.fix:
        print(f"\ncheck-citations: re-anchored {fixable} citation(s) across {checked} "
              f"checked anchor(s){tail}; {unmappable} need a human"
              + (f"; {withheld} withheld from document(s) owning a misaimed declaration"
                 if withheld else "") + ".")
        if reaim_problems:
            # The repair ran, which is the point of letting it. It is still not a
            # clean run: a misaimed declaration keeps a drift check switched off,
            # and only an edit to this script can turn it back on.
            print(f"check-citations: {len(reaim_problems)} declared re-aim(s) still point at "
                  f"something other than what they claim — fix DELIBERATE_REAIMS, then re-run "
                  f"--check.", file=sys.stderr)
        return fix_exit_code(unmappable, reaim_problems)

    if drifted:
        print(f"\ncheck-citations: {drifted} citation(s) across {checked} checked "
              f"anchor(s){tail} no longer point at the text they did at {args.base}. "
              f"Re-anchor them in THIS change set (scripts/check-citations.py --fix), "
              f"then re-read the ones marked UNMAPPABLE.", file=sys.stderr)
        return 1

    print(f"check-citations: {checked} anchor(s) still point at the same text as "
          f"{args.base}{tail}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
