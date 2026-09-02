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
    "src/dsp/SoloMonitor.h",
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

# THE GLOSS: the half of a citation a READER checks, made checkable by the tool.
#
# This file's header already prescribes the convention -- "Spelling the SYMBOL
# beside the line number is what makes the other half checkable by a reader, and
# is the convention to write new citations in". Every citation in the documents
# below already follows it. Nothing was reading it.
#
# The cost of not reading it was measured rather than argued: nine fully
# qualified anchors in this repository point at unrelated code RIGHT NOW, and all
# nine are green, because the drift test compares an anchor against a BASE and an
# anchor that was already wrong at the base stays wrong and stays green. That
# limit is stated at the top of this file. This closes it for the anchors that
# say, in the document, what they are supposed to be pointing at.
#
# A MATCH, NOT A PARSE, deliberately -- the constraint this file is written under
# is that it must not become a parser. Exactly two shapes are claimed, both
# unambiguous: one backticked identifier, or one double-quoted source string,
# alone inside the parentheses immediately after the anchor. Everything else is
# declined, which is most of what the documents actually write: `(24 Hz timer)`,
# `(VBlank)`, `(gate + rationale comment)` and `(mono->stereo upmix)` are prose
# ABOUT the reference, not a claim about the text at it, and a tool that treated
# them as one would be inventing an assertion the author did not make.
GLOSS = re.compile(r"""\A[`]?\s*\((?:`([^`]+)`|"([^"]+)")\)""")

# Citations this repository RE-AIMED on purpose: the anchor was pointing at the
# wrong code and was moved onto the right code, which is indistinguishable from
# drift by the base-text test and would otherwise fail the gate forever.
# Declaring one here is a reviewable act — it appears in the diff, beside the
# document it exempts, and it is the only way to make this tool accept a
# re-aim. An entry whose citation no longer differs from the base has done its
# job (the base has caught up) and is reported as removable on the next run.
#
# A DICT, and `{}` is its correct empty state. This paragraph used to say the
# opposite -- that the structure was spelled `set([...])` rather than `{...}` on
# purpose, because emptying a brace literal would leave a dict and the set
# difference below would then raise `TypeError`. That was true when it was
# written (2026-08-15) and stopped being true on 2026-08-18, when each entry
# gained the expectation described just below: the same change turned `set([`
# into `{` and routed both set operations through `.keys()` --
# `used_reaims & DELIBERATE_REAIMS.keys()` and
# `DELIBERATE_REAIMS.keys() - used_reaims`. `dict_keys` supports set algebra, so
# both work empty and non-empty alike; only a RAW `dict - set` raises, and that
# form appears nowhere in this file. What the paragraph was protecting still
# holds -- this list going empty is the expected end state of every entry in it,
# so it must be the boring case -- it is simply no longer the spelling that
# protects it.
# EACH ENTRY IS `(document, BASE anchor, CURRENT anchor) -> what a reader should
# FIND there`. The key is the TRANSITION, not the anchor, and that is the whole
# lifecycle: a declaration authorises one deliberate movement, from one spelling
# to one other spelling, and stops matching the moment either end differs.
#
# IT WAS KEYED ON A SINGLE SPELLING UNTIL 2026-08-30, matched against EITHER side
# of the change, and that made every entry a permanent exemption for its anchor
# rather than for its transition. Demonstrated on this repository's own table
# before the change: `docs/REPOSITORY_MAP.md` declared `CMakeLists.txt:114-384`
# for a transition that had long since merged, and
# `is_declared_reaim(doc, "CMakeLists.txt:114-384", "CMakeLists.txt:120-390")`
# still returned True -- a later, undeclared movement of that anchor, silenced.
# The `verify_reaim_targets` aim-check did NOT catch it either, because the
# declared span is 270 lines wide and still contained its token. Silent
# suppression of real drift, by a declaration written for something else.
#
# The `base == current` guard that used to be the only lifecycle rule stays, but
# it is now a belt-and-braces refusal rather than the mechanism: a key naming the
# same spelling twice is a declaration that nothing moved, which is not a re-aim.
#
# The VALUE is not documentation: `verify_reaim_targets()` resolves the CURRENT
# anchor against the current file on every run and fails when the substring is
# absent. It resolves the current one because that is the spelling the document
# now carries; the base spelling names a revision, not this tree, and asking this
# tree to contain it would fail every well-formed declaration.
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
    # EMPTY IS THE EXPECTED RESTING STATE, and this table reached it on
    # 2026-08-30 when the key became a TRANSITION. It held 40 entries the moment
    # before; none of them was doing any work. Measured against every base this
    # repository ever compares -- `origin/main`, the branch merge base (the same
    # commit) and `HEAD~1`, which is what CI passes as `github.event.before` --
    # emptying the table left all three green with zero drift. 37 were reported
    # by the tool itself as "not needed against origin/main, which already
    # carries the re-aimed spelling", against a base that IS the branch's merge
    # base, which is exactly the retirement condition the note below states. The
    # other 3 were written earlier in this same branch and were re-anchored out
    # of existence by `--fix` in the commit that created them.
    #
    # They are not deleted to make a problem go away; they are deleted because
    # each had completed its one transition, and under the key below a completed
    # transition can no longer match anything. Leaving them would have meant
    # inventing base spellings for transitions that merged weeks ago.
    #
    # 2026-08-31 (engineering-review round 1): two hand re-aims. The 0.9.6
    # fixes edited `reassertParameters` (ER-STATE-01 grew it) and `readSlot`
    # (ER-STATE-02's type guard), i.e. the cited lines themselves, so --fix
    # reported both UNMAPPABLE and the new spellings were re-derived from the
    # symbols the documents name. Both retire on merge, when origin/main
    # carries the re-aimed spellings.
    # 2026-09-01 (engineering-review round 4): two more hand re-aims, both because
    # the CITED LINES THEMSELVES were edited by approved work. D-1 added a
    # `private juce::Timer` base to the AnamorphAudioProcessor declaration, and the
    # KI-028 macOS fix added an APPLE-only source to the plugin source list, so
    # --fix reported both UNMAPPABLE and the new spellings were re-derived by
    # reading the spans. Both retire on merge.
    # 2026-09-01 (engineering-review round 11): three more hand re-aims. The
    # ER-STATE-14 latency split and the ER-STATE-15 sound-child guard EDITED the
    # cited lines themselves, so --fix reports them UNMAPPABLE rather than moving
    # them. Each was re-derived by reading the span for its named symbol:
    # setStateInformation still starts at :878 and now ends at :1111 (the guard is
    # inside it, so the span is unchanged and only its content moved);
    # updateLatency moved down past the new deliverLatency. All retire on merge.
    # 2026-09-02 (round 15): ER-STATE-19 grew prepareToPlay by its D-1 comment and
    # requestLatencyUpdate by its no-MessageManager branch, so every span below
    # them moved again; the same symbols, re-derived by reading the spans
    # (deliverLatency+updateLatency now :149-173, setStateInformation :901-1134,
    # the legacyKey adoption :1009-1010, the ADR-0010 sites :512 and :647-650).
    # 2026-09-02 (round 16): ER-STATE-20 added the per-slot match reset inside
    # `readSlot`, growing it by its explanatory comment, so the spans below it moved
    # again -- same symbols, re-derived by reading them: the legacyKey fallback is now
    # :1056-1057, setStateInformation :926-1181, readSlot :991-1078. Each earlier
    # transition keeps its own base key and gains one for the push predecessor.
    # Each earlier transition keeps its origin/main key and gains a HEAD~1 key,
    # since CI compares against the push predecessor. All retire on merge.
    # Round 15 also EDITED the engine's latency lines themselves (the three
    # `latencyN = ...` assignments became relaxed stores, and the jmax below them
    # wrapped onto three lines), so the spans that cite them are UNMAPPABLE and
    # were re-derived by reading: the oversampler block is now :42-56, the three
    # stores :54-56, and everything below :84 sits two lines lower.
    ("docs/architecture/LATENCY_MODEL.md",
     "src/dsp/AnamorphEngine.cpp:42-54",
     "src/dsp/AnamorphEngine.cpp:42-56"): "latency2",
    ("docs/architecture/LATENCY_MODEL.md",
     "src/dsp/AnamorphEngine.cpp:52-54",
     "src/dsp/AnamorphEngine.cpp:54-56"): "latency2",
    ("docs/architecture/design-decisions/ADR-0003-oversampling-strategy.md",
     "src/dsp/AnamorphEngine.cpp:14-23, 42-54, 313-349",
     "src/dsp/AnamorphEngine.cpp:14-23, 42-56, 338-374"): "isModAlgorithm",
    ("docs/architecture/design-decisions/ADR-0003-oversampling-strategy.md",
     "src/dsp/AnamorphEngine.cpp:14-23,42-54,293-329",
     "src/dsp/AnamorphEngine.cpp:14-23, 42-56, 338-374"): "isModAlgorithm",
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:105-108",
     "src/PluginProcessor.cpp:149-173"): "updateLatency",
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:131-137",
     "src/PluginProcessor.cpp:149-173"): "updateLatency",
    ("docs/architecture/LATENCY_MODEL.md",
     "src/PluginProcessor.cpp:131-154",
     "src/PluginProcessor.cpp:149-173"): "updateLatency",
    # 2026-09-01 (round 12): the ER-STATE-17 guard EDITED the cited lines inside
    # migrateFromLegacyApvts, so --fix reports these UNMAPPABLE rather than moving
    # them. Each end was re-derived by reading the span: origin/main's :60
    # (oversampleValue), :95 (restoreState's id loop), :100 (its trailing comment)
    # and :122 (the oversample setProperty in the migration) are now :62, :97, :102
    # and :159. All retire on merge.
    # 2026-09-01 (round 14): the ER-STATE-18 settings() table replaced the
    # constructor's six hand-written setProperty lines, so this anchor's target
    # text no longer exists. Re-derived by symbol: the tooltips default is now the
    # `iid::tooltipsOn` row of that table. Retires on merge.
    # 2026-09-02 (round 18): ER-STATE-21's Policy B added the DOMAIN to the settings()
    # table, growing it; the same symbol, re-derived by reading the new span.
    ("docs/KNOWN_ISSUES.md",
     "src/InternalState.h:57-64",
     "src/InternalState.h:68-75"): "tooltipsOn",
    # 2026-09-02 (round 18): Policy B grew settings() (the DOMAIN joins the default)
    # and added usableNumber/repairedValue above the migration, so every span below
    # them moved; the same symbols, re-derived by reading the new spans.
    ("docs/KNOWN_ISSUES.md",
     "src/InternalState.h:57-64",
     "src/InternalState.h:68-75"): "tooltipsOn",
    # 2026-09-02 (round 20): ER-STATE-22 added two exact-compare lines to
    # `repairedValue` and its comment, and ER-DSP-09 added the four module
    # `snapToTargets()` calls plus their rationale at the end of
    # `AnamorphEngine::prepare()`, so every span below each of them moved again.
    # Same symbols, re-derived by reading the new spans: `oversampleValue`
    # :165-280, `migrateFromLegacyApvts` :197-286 and :229-280, `isModAlgorithm`
    # :338-374. Only the TARGET side of each existing transition changed -- no
    # entry was added or retired, because no new document started drifting.
    ("docs/KNOWN_ISSUES.md",
     "src/InternalState.h:57-64",
     "src/InternalState.h:68-75"): "tooltipsOn",
    ("docs/architecture/API_REFERENCE.md",
     "src/InternalState.h:80-191",
     "src/InternalState.h:165-280"): "oversampleValue",
    ("docs/architecture/PARAMETER_REGISTRY.md",
     "src/InternalState.h:112-197",
     "src/InternalState.h:197-286"): "migrateFromLegacyApvts",
    ("docs/policies/COMPATIBILITY_POLICY.md",
     "src/InternalState.h:134-191",
     "src/InternalState.h:229-280"): "migrateFromLegacyApvts",
    ("docs/KNOWN_ISSUES.md",
     "src/InternalState.h:51",
     "src/InternalState.h:68-75"): "tooltipsOn",
    ("docs/KNOWN_ISSUES.md",
     "src/InternalState.h:53",
     "src/InternalState.h:68-75"): "tooltipsOn",
    ("docs/architecture/API_REFERENCE.md",
     "src/InternalState.h:60-122",
     "src/InternalState.h:165-280"): "oversampleValue",
    ("docs/architecture/PARAMETER_REGISTRY.md",
     "src/InternalState.h:95-122",
     "src/InternalState.h:197-286"): "migrateFromLegacyApvts",
    ("docs/architecture/PARAMETER_REGISTRY.md",
     "src/InternalState.h:97-159",
     "src/InternalState.h:197-286"): "migrateFromLegacyApvts",
    ("docs/policies/COMPATIBILITY_POLICY.md",
     "src/InternalState.h:100-122",
     "src/InternalState.h:229-280"): "migrateFromLegacyApvts",
    ("docs/architecture/API_REFERENCE.md",
     "src/PluginProcessor.h:20-79",
     "src/PluginProcessor.h:20-80"): "AnamorphAudioProcessor",
    ("docs/policies/RELEASE_POLICY.md",
     "CMakeLists.txt:14, 457-482",
     "CMakeLists.txt:14, 467-492"): "ANAMORPH_BUILD_NUMBER",
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:688-692",
     "src/PluginProcessor.cpp:1056-1057"): "legacyKey",
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:1009-1010",
     "src/PluginProcessor.cpp:1056-1057"): "legacyKey",
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:986-987",
     "src/PluginProcessor.cpp:1056-1057"): "legacyKey",
    ("docs/policies/COMPATIBILITY_POLICY.md",
     "src/PluginProcessor.cpp:327-396",
     "src/PluginProcessor.cpp:926-1181"): "setStateInformation",
    ("docs/policies/COMPATIBILITY_POLICY.md",
     "src/PluginProcessor.cpp:901-1134",
     "src/PluginProcessor.cpp:926-1181"): "setStateInformation",
    ("docs/policies/COMPATIBILITY_POLICY.md",
     "src/PluginProcessor.cpp:878-1111",
     "src/PluginProcessor.cpp:926-1181"): "setStateInformation",
    # 2026-08-31 (round 2): prepareToPlay grew by the priming call, shifting every
    # line below it. RISK-007's read-side citation names setStateInformation, so it was
    # re-derived from that symbol rather than mapped mechanically -- the mechanical
    # map would have carried the span past the function's own signature. (The
    # write-side reference beside it is a bare `:line` shorthand, which the gate
    # does not track as an anchor, so it needs no declaration.)
    # The same three anchors also moved relative to the PUSH PREDECESSOR (a
    # different base than origin/main, and the one CI compares). Same symbols,
    # same re-derivation; both bases are declared until this branch merges.
    ("docs/architecture/design-decisions/ADR-0010-host-hidden-internalstate.md",
     "src/PluginProcessor.cpp:328, 390-393",
     "src/PluginProcessor.cpp:512,647-650"): "setValueNotifyingHost",
    ("docs/architecture/SERIALIZATION_REGISTRY.md",
     "src/PluginProcessor.cpp:743-744",
     "src/PluginProcessor.cpp:1056-1057"): "legacyKey",
    ("docs/policies/COMPATIBILITY_POLICY.md",
     "src/PluginProcessor.cpp:645-796",
     "src/PluginProcessor.cpp:926-1181"): "setStateInformation",
    ("docs/architecture/design-decisions/ADR-0010-host-hidden-internalstate.md",
     "src/PluginProcessor.cpp:318, 380-383",
     "src/PluginProcessor.cpp:512,647-650"): "setValueNotifyingHost",
    ("docs/architecture/design-decisions/ADR-0010-host-hidden-internalstate.md",
     "src/PluginProcessor.cpp:489,624-627",
     "src/PluginProcessor.cpp:512,647-650"): "setValueNotifyingHost",
    ("docs/architecture/design-decisions/ADR-0010-host-hidden-internalstate.md",
     "src/PluginProcessor.cpp:311,345-348",
     "src/PluginProcessor.cpp:512,647-650"): "setValueNotifyingHost",
    ("docs/FUTURE_RISKS.md",
     "src/PluginProcessor.cpp:901-1134",
     "src/PluginProcessor.cpp:926-1181"): "setStateInformation",
    ("docs/FUTURE_RISKS.md",
     "src/PluginProcessor.cpp:878-1111",
     "src/PluginProcessor.cpp:926-1181"): "setStateInformation",
    ("docs/FUTURE_RISKS.md",
     "src/PluginProcessor.cpp:610-749",
     "src/PluginProcessor.cpp:926-1181"): "setStateInformation",
}

# Lines whose CONTENT is expected to change on its own schedule, keyed by the
# exact `(tracked path, line)` the citation names.
#
# WHY THIS EXISTS, and it was found the way these things are found: the first
# version bump after `CMakeLists.txt` came under this gate FAILED it. Six
# documents cite `CMakeLists.txt:14` -- the `project(Anamorph VERSION x.y.z ...)`
# line -- and its text changes by definition on every release, so the base
# comparison reports six drifted citations for a change that moved nothing. The
# release procedure this repository documents (`RELEASE_PROCESS.md` step 1) was
# therefore un-runnable while the gate was green on every other kind of change.
#
# WHY `DELIBERATE_REAIMS` CANNOT COVER IT. That table excuses a citation whose
# SPELLING changed, and `is_declared_reaim` returns False when the base and
# current spellings agree -- deliberately, so an entry cannot outlive its one
# transition. This is the opposite case: the spelling is right, unchanged, and
# will stay right; it is the cited LINE that was rewritten. There is no
# transition for an entry to outlive, which is also why the mechanism has to be
# separate rather than a loosened guard on the other one.
#
# WHAT IT COSTS, stated because it is not free. For a declared line the base
# comparison -- "does this still say what it said?" -- is REPLACED by a
# permanent content assertion: does the line still contain the stable token
# named here. On `CMakeLists.txt:14` that token is the part a citation is really
# about (`project(Anamorph VERSION`), and the part it is not about is exactly
# the version. So the gate stops watching the version and keeps watching that
# line 14 is still the project declaration. That is weaker than the base
# comparison and stronger than an exemption, and unlike an exemption it is
# checked on EVERY run by `verify_versioned_lines`, against the current file,
# with no base revision involved.
#
# THREE THINGS KEEP IT NARROW. It is keyed by one exact line, not a file and not
# a document, so it cannot spread. It applies only when the anchor did NOT move
# -- a re-aimed anchor still goes through the ordinary drift path and still
# needs a `DELIBERATE_REAIMS` declaration. And an entry whose token has gone
# missing is a hard failure, not a warning, so a line that stops being what it
# claims takes the build with it.
VERSIONED_LINES = {
    ("CMakeLists.txt", 14): "project(Anamorph VERSION",
}


def anchor_still_right(tracked, base_src, now_src, a, b, a2, b2):
    """Is this citation still pointing at what it pointed at? ONE decision.

    BOTH check paths ask this question and they used to answer it separately.
    The paired path (base and current spell the citation the same number of
    times) grew the `VERSIONED_LINES` substitution; the count-mismatch path --
    reached when a document changes HOW MANY times it cites a file -- kept a
    bare text comparison, so a version bump landing in the same change set as a
    new citation of `CMakeLists.txt` reported line 14 as drifted and re-blocked
    the release the substitution exists to unblock. Reproduced before the fix:
    one added citation in `RELEASE_PROCESS.md` turned `--check` red with
    "UNMAPPABLE CMakeLists.txt:14".

    The two paths differ only in what they compare against: the count-mismatch
    path reaches citations whose SPELLING is unchanged, so it passes `a2 == a`.
    Everything else -- including which lines are allowed to change content -- is
    the same question and is now answered in one place, which is the property
    that stops the two drifting apart again.

    A `VERSIONED_LINES` entry substitutes "still contains its stable token" for
    the base comparison, and only for an anchor that did NOT move; a re-aimed
    anchor falls through to the ordinary drift path and still needs a
    `DELIBERATE_REAIMS` declaration.
    """
    if (b is None) != (b2 is None):
        return False
    for lo, hi in ((a, a2), (b, b2)):
        if lo is None:
            continue
        if lo == hi and (tracked, lo) in VERSIONED_LINES:
            if VERSIONED_LINES[(tracked, lo)] in line_of(now_src[tracked], hi):
                continue
            return False
        if line_of(base_src[tracked], lo) != line_of(now_src[tracked], hi):
            return False
    return True


def verify_versioned_lines():
    """Every declared line still contains its stable token. No base revision.

    The sibling of `verify_reaim_targets` and built from the same parts, for the
    same reason: a declaration that turns a comparison off is only safe while
    something else says it still names what it claims to name.
    """
    problems = []
    for (path, line), token in sorted(VERSIONED_LINES.items()):
        try:
            text = read(path)
        except OSError as exc:
            problems.append((path, line, token, f"the file could not be read ({exc.strerror})"))
            continue
        lines = text.split("\n")
        if line < 1 or line > len(lines):
            problems.append((path, line, token, f"the file has only {len(lines)} lines"))
            continue
        if token not in lines[line - 1]:
            problems.append((path, line, token,
                             f"the line reads: {lines[line - 1].strip()[:70]!r}"))
    return problems


# Documents whose GLOSSED citations are checked against the text they name.
#
# BESIDE `DELIBERATE_REAIMS` ON PURPOSE: that table turns a check OFF for one
# anchor, this one turns a check ON for a document, and the two belong where a
# reader meets them together.
#
# IT NAMES DOCUMENTS, NOT ANCHORS, and that is the whole reason it is safe to
# keep. An anchor-keyed table would hold a copy of the line number `--fix`
# rewrites -- the disease built into the cure, and not hypothetically:
# `invalidated_reaims` below exists because exactly that happened twice in one
# change set. A document name cannot go stale when code moves.
#
# WHY THESE EIGHT. They are the architecture set -- the documents whose evidence
# lines a reader follows to check a claim about the system rather than about a
# procedure. They carry 43 glossed citations between them, and the tree is clean.
# THE MEASUREMENT THAT PUT THEM HERE, over the seven listed when this was written:
# 20 glossed citations, 5 firing, all 5 genuine defects, 0 false positives -- and
# those 5 were corrected in the same change set, which is why the count of firing
# anchors is 0 now and says nothing about whether the check works.
# Repo-wide the same extraction fires 10 times, 9
# genuine and 1 false -- `RELEASE_PROCESS.md:45` cites `CMakeLists.txt:14`
# (`project VERSION`), a prose gloss that happens to be backticked. That one
# false positive is the reason this is an opt-in tuple and not the whole scan:
# the noise rate is a property of how a given document writes its parentheticals,
# so a document joins this list when someone has read its citations.
#
# `SIGNAL_FLOW.md` JOINED ON 2026-08-22, and how it got here is the argument for
# the whole mechanism. It carried two qualified anchors and 33 BARE ones, all
# invisible for the reason bare anchors always are -- `CITATION` requires a path --
# in the document that records DSP signal order, a `CLAUDE.md` hard-stop class.
# Sixteen of them sat inside an ASCII diagram where a full path would have
# destroyed the alignment the diagram exists for, so the numbers moved OUT of the
# diagram into a table beside it and came back qualified: 40 anchors the gate can
# see, 23 of them glossed and content-checked. The diagram kept the order and the
# symbol, which is what it is for and what does not rot.
GLOSS_CHECKED_DOCS = (
    "docs/architecture/ARCHITECTURE.md",
    "docs/architecture/LATENCY_MODEL.md",
    "docs/architecture/PARAMETER_REFERENCE.md",
    "docs/architecture/PARAMETER_REGISTRY.md",
    "docs/architecture/SERIALIZATION_REGISTRY.md",
    "docs/architecture/SIGNAL_FLOW.md",
    "docs/architecture/STATE_SERIALIZATION.md",
    "docs/architecture/THREAD_MODEL.md",
)



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


def fix_exit_code(unmappable: int, reaim_problems, gloss_problems=()) -> int:
    """`--fix` exits non-zero when it leaves work behind -- including a misaimed
    declaration it CANNOT repair, because a declaration lives in this script and
    no rewrite of a document reaches it. Letting `--fix` proceed past one must
    not turn into `--fix` reporting success over one.

    A MISAIMED GLOSS COUNTS THE SAME WAY and for a sharper reason: `--fix` moves
    anchors by the line map, so running it over one carries the wrong aim to a
    new line and changes nothing about the aim. The repair is a human's."""
    return 2 if (unmappable or reaim_problems or gloss_problems) else 0


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
    # THE CURRENT SPELLING IS THE ONE RESOLVED. The base spelling in the key
    # names a revision this tree is not; resolving it here would fail every
    # well-formed declaration, since a re-aim exists precisely because the two
    # differ.
    for (doc, _whole_base, whole), expect in sorted(DELIBERATE_REAIMS.items()):
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


def verify_glossed_anchors():
    """Check every GLOSSED citation in an opted-in document lands on what it names.

    The sibling of `verify_reaim_targets`, and deliberately built from the same
    parts: the same `reaim_target_text` resolver, the same
    `(doc, whole, expect, why)` problem tuple, the same "reads no base revision"
    property. The difference is where the expectation comes from -- a declaration
    in this file for that one, the DOCUMENT'S OWN PROSE for this one.

    WHY THE DOCUMENT'S PROSE IS THE RIGHT SOURCE. An expectation written here
    would be a second copy of a claim the document already makes, and the two
    would drift from each other exactly as documents drift from code. The
    parenthetical is not restating the anchor; it is the author saying what a
    reader should find there. Reading it costs nothing and asserts precisely the
    thing the line number is for.

    THE `::` FALLBACK IS REQUIRED, not a convenience. A gloss written as a
    qualified name (`StateSet::isValid`) will not appear in the source, which
    spells only the last component at the definition site -- measured on three
    citations, all of which fired falsely without this and none of which is
    wrong. Testing the full gloss OR its last `::` component removed all three
    and cost no true positive.

    IT WEAKENS THE CLAIM AND THAT IS SAID OUT LOUD: this proves the token is
    somewhere in the cited span, not that the span is the right span. On a wide
    anchor that is a weak claim. It is still strictly stronger than the nothing
    that was there before, and it is the strongest claim available without
    turning a substring test into a parser.
    """
    problems = []
    for doc in GLOSS_CHECKED_DOCS:
        # `read()` returns the contents or RAISES -- it has no None path, so the
        # guard this replaced was dead and a document deleted from the tree while
        # still listed here would have come out as an uncaught traceback rather
        # than as a finding. Caught the way `reaim_target_text` catches it, and
        # reported in the same 4-tuple every other problem here uses.
        try:
            text = read(doc)
        except OSError as exc:
            problems.append((doc, "-", "-", f"the document could not be read ({exc.strerror})"))
            continue
        problems += glossed_problems_in(doc, text)
    return problems


def glossed_problems_in(doc, text):
    """The per-document half, split out so the self-test can drive it.

    Taking the TEXT rather than reading it is what lets section 8e exercise both
    directions on synthetic input, the way every other section here does. A
    self-test that could only run over the real tree would go quiet the day the
    tree is clean -- which is the state this check is supposed to hold it in, so
    that is exactly when it would stop proving anything.
    """
    problems = []
    for whole, _tracked, span, _anchors in citations(text):
        m = GLOSS.match(text[span[1]:span[1] + 200])
        if not m:
            continue
        expect = m.group(1) or m.group(2)
        target = reaim_target_text(whole)
        if target is None:
            problems.append((doc, whole, expect, "the cited file could not be read"))
            continue
        tail = expect.rsplit("::", 1)[-1]
        if expect in target or tail in target:
            continue
        first = target.split("\n")[0].strip()[:70]
        problems.append((doc, whole, expect,
                         f"the cited lines start with: {first!r}"))
    return problems


def is_declared_reaim(doc, whole_base, whole_cur):
    """Does a declaration excuse this citation's mismatch?

    THE DECLARATION NAMES A TRANSITION, so this is one lookup of the pair. A
    declaration written for `A -> B` excuses `A -> B` and nothing else: not
    `B -> C`, not `C -> B`, not the same anchor moving again next year for an
    unrelated reason.

    WHY THAT IS THE RULE AND NOT "either spelling is declared", which is what
    this function did until 2026-08-30. Keying on one spelling and matching it
    against either side made the entry a permanent exemption for its ANCHOR. Once
    the base revision caught up and carried the declared spelling, every
    subsequent movement of that anchor arrived as `declared -> something new` and
    was excused by an entry written for a transition that had already merged.
    Reproduced on the live table before the change (see the header): a 270-line
    declared span was still excusing movements years after its own transition,
    and the aim-check could not see it because the token had not left the span.

    The `base == current` refusal is kept even though a well-formed transition
    key can no longer trip it. A key naming one spelling twice would be a
    declaration that nothing moved, which is not a re-aim but ordinary drift, and
    the cheapest place to say so is here rather than in review.
    """
    if whole_base == whole_cur:
        return False
    if not whole_base or not whole_cur:
        return False
    return (doc, whole_base, whole_cur) in DELIBERATE_REAIMS


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
    # The CURRENT spelling of each of this document's declared transitions --
    # the one the document actually contains and the rewrite can therefore
    # invalidate.
    for whole in sorted(cur for (d, _base, cur) in DELIBERATE_REAIMS if d == doc):
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

    # --- 5. DECLARED RE-AIMS: one transition, and ONLY that transition ------
    #
    # THE REGRESSION THIS SECTION EXISTS FOR (2026-08-30). The table was keyed on
    # a single spelling matched against EITHER side of the change, which made a
    # declaration a permanent exemption for its ANCHOR: once the base caught up,
    # every later movement arrived as `declared -> something new` and was excused
    # by an entry written for a transition that had already merged. Case 3 below
    # is that exact shape and returned True before the key became the pair.
    doc = "docs/EXAMPLE.md"
    a1, b1 = "src/PluginProcessor.cpp:100", "src/PluginProcessor.cpp:200"
    c1 = "src/PluginProcessor.cpp:300"
    other = "src/AnotherFile.cpp:10"
    saved = dict(DELIBERATE_REAIMS)
    try:
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS[(doc, a1, b1)] = ""

        # 1. THE DECLARED TRANSITION IS ACCEPTED.
        check("the declared transition A -> B is honoured",
              is_declared_reaim(doc, a1, b1), True)

        # 2. THE SAME ANCHOR, MOVING AGAIN LATER, IS NOT. The declaration named
        #    A -> B; B -> C is a different movement and nobody authorised it.
        check("a later movement B -> C is NOT excused by the A -> B declaration",
              is_declared_reaim(doc, b1, c1), False)

        # 3. NOR IS THE REVERSE. A movement that lands back ON the declared
        #    spelling is still a movement the declaration does not name — the
        #    hole a "current spelling only" key would have left open.
        check("a later movement C -> B is NOT excused either",
              is_declared_reaim(doc, c1, b1), False)

        # 4. AND NOT A DIFFERENT ANCHOR IN THE SAME DOCUMENT.
        check("an unrelated anchor in the same document stays checked",
              is_declared_reaim(doc, other, "src/AnotherFile.cpp:20"), False)

        # 5. THE ENTRY MUST NOT SURVIVE ITS OWN TRANSITION. Once the base carries
        #    the re-aimed spelling there is no movement left to excuse, and the
        #    equal-spelling refusal says so before the lookup is even reached.
        check("an unchanged spelling is drift, not a re-aim, even when declared",
              is_declared_reaim(doc, b1, b1), False)

        # 6. HALF A TRANSITION IS NOT A TRANSITION. Declaring only one end used
        #    to be the whole entry; it must now match nothing on its own.
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS[(doc, b1, c1)] = ""
        check("a declaration for B -> C does not excuse A -> B",
              is_declared_reaim(doc, a1, b1), False)

        DELIBERATE_REAIMS.clear()
        check("an undeclared re-spelling is drift",
              is_declared_reaim(doc, a1, b1), False)
        DELIBERATE_REAIMS[(doc, a1, b1)] = ""
        check("a declaration for another document does not apply",
              is_declared_reaim("docs/OTHER.md", a1, b1), False)
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

    # A transition whose CURRENT end is the spelling this document carries --
    # that end is what a rewrite can invalidate.
    DELIBERATE_REAIMS[(doc, f"{path}:1-2", declared)] = "run_one_pass"
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
    DELIBERATE_REAIMS.update({("docs/procedures/BUILD.md",
                              "synthetic/Two.txt:1", B_OLD): ""})
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
    DELIBERATE_REAIMS.update({("docs/procedures/BUILD.md",
                              "synthetic/Two.txt:1", B_OLD): ""})
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
        DELIBERATE_REAIMS[("docs/EXAMPLE.md", "CMakeLists.txt:1", "CMakeLists.txt:14")] = \
            "this-is-not-on-line-14"
        wrong, _ = verify_reaim_targets()
        check("a declaration whose expectation is absent from the cited lines is reported",
              len(wrong), 1)

        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS[("docs/EXAMPLE.md", "CMakeLists.txt:1", "CMakeLists.txt:14")] = \
            "project(Anamorph"
        right, _ = verify_reaim_targets()
        check("...and one whose expectation IS there is not", right, [])

        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS[("docs/EXAMPLE.md", "no/such/file.cpp:9", "no/such/file.cpp:1")] = \
            "anything"
        missing, _ = verify_reaim_targets()
        check("an unreadable cited file is a problem, not a pass", len(missing), 1)

        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS[("docs/EXAMPLE.md", "CMakeLists.txt:1", "CMakeLists.txt:14")] = ""
        none_declared, unver = verify_reaim_targets()
        check("an entry declaring no expectation is reported as unverifiable, not clean",
              (none_declared, len(unver)), ([], 1))

        # WHICH END IS RESOLVED, asserted rather than assumed. The base end names
        # a revision this tree is not, so resolving it would fail every correct
        # declaration: here the CURRENT end carries the token and the base end
        # does not, and the entry must come back clean.
        DELIBERATE_REAIMS.clear()
        # `CMakeLists.txt:1` is `cmake_minimum_required(...)` and `:14` is the
        # `project(...)` line, so the token below is on the CURRENT end only.
        DELIBERATE_REAIMS[("docs/EXAMPLE.md", "CMakeLists.txt:14", "CMakeLists.txt:1")] = \
            "cmake_minimum"
        end_choice, _ = verify_reaim_targets()
        check("verify_reaim_targets resolves the CURRENT end, not the base end",
              end_choice, [])
    finally:
        DELIBERATE_REAIMS.clear()
        DELIBERATE_REAIMS.update(saved)

    # --- 8e. A CITATION MUST CONTAIN WHAT IT SAYS IT CONTAINS ---------------
    # The check that closes the "wrong at the base" hole. Section 8c proves a
    # DECLARED expectation is verified; this proves the same for the expectation
    # a document writes inline, which is the only one most anchors have.
    #
    # DRIVEN ON SYNTHETIC TEXT against a REAL cited file, the way section 8c is.
    # A case that could only run over the real tree would go quiet the day the
    # tree is clean -- and holding the tree clean is this check's whole purpose,
    # so that is precisely when it would stop proving anything.
    check("every glossed citation in the opted-in documents lands on what it names",
          verify_glossed_anchors(), [])

    def glossed(body):
        return glossed_problems_in("docs/EXAMPLE.md", body)

    # CMakeLists.txt:14 is `project(Anamorph VERSION ... )`.
    check("a backticked gloss that IS on the cited line passes",
          glossed("see CMakeLists.txt:14 (`project`)"), [])
    check("...and one that is NOT there is reported",
          len(glossed("see CMakeLists.txt:14 (`definitelyNotOnLine14`)")), 1)
    check("a double-quoted gloss is read the same way",
          glossed('see CMakeLists.txt:14 ("Anamorph VERSION")'), [])
    check("...and fires when absent",
          len(glossed('see CMakeLists.txt:14 ("not that string at all")')), 1)

    # THE DECLINED SHAPES, which are most of what the documents write. A gloss
    # this tool does not recognise must assert NOTHING -- inventing a claim the
    # author did not make is the false-positive engine that gets a gate switched
    # off, and `(24 Hz timer)` is prose ABOUT a reference, not a claim about the
    # text at it.
    check("a prose parenthetical asserts nothing", glossed("see CMakeLists.txt:14 (24 Hz timer)"), [])
    check("...nor does a mixed one", glossed("see CMakeLists.txt:14 (`project` and more)"), [])
    check("...nor does an absent one", glossed("see CMakeLists.txt:14 and then prose"), [])

    # A qualified name is spelled with its scope in the document and without it
    # at the definition site; testing the last `::` component is what keeps three
    # correct citations from firing.
    check("a `::`-qualified gloss is satisfied by its last component",
          glossed("see CMakeLists.txt:14 (`Fictional::project`)"), [])
    # OWNERSHIP RUNS FIRST: `citations()` yields only TRACKED paths, so a gloss on
    # anything else is never looked at. Asserted as a case because "it did not
    # fire" and "it was never examined" are indistinguishable in a passing run.
    check("a gloss on an untracked path is not this tool's business",
          glossed("see no/such/file.cpp:1 (`anything`)"), [])

    # ...which is also why reaching the unreadable-file branch needs the path to
    # be TRACKED and absent -- the state a deleted-but-still-cited file leaves
    # behind. Constructed the way section 8c constructs a wrong declaration,
    # rather than by deleting a file.
    saved_tracked = TRACKED
    try:
        globals()["TRACKED"] = TRACKED + ("no/such/file.cpp",)
        check("a cited file that cannot be read is a problem, not a pass",
              len(glossed("see no/such/file.cpp:1 (`anything`)")), 1)
    finally:
        globals()["TRACKED"] = saved_tracked
    check("...and TRACKED is restored afterwards", "no/such/file.cpp" in TRACKED, False)

    # A LISTED DOCUMENT THAT IS GONE is a finding, not a traceback. `read()`
    # returns contents or raises, so this branch is the only thing standing
    # between a deleted-but-still-listed entry and an uncaught FileNotFoundError.
    saved_docs = GLOSS_CHECKED_DOCS
    try:
        globals()["GLOSS_CHECKED_DOCS"] = GLOSS_CHECKED_DOCS + ("docs/NO_SUCH_DOC.md",)
        check("a listed document that cannot be read is reported, not raised",
              len(verify_glossed_anchors()), 1)
    finally:
        globals()["GLOSS_CHECKED_DOCS"] = saved_docs
    check("...and the document list is restored afterwards",
          "docs/NO_SUCH_DOC.md" in GLOSS_CHECKED_DOCS, False)

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
         "report success", "return fix_exit_code(unmappable, reaim_problems, gloss_problems)"),
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

    # --- 8f. A VERSIONED LINE IS EXCUSED, AND ONLY THAT LINE -----------------
    # `VERSIONED_LINES` turns the base comparison off for one exact line, which
    # is the only construct in this file that can make a REAL difference in a
    # tracked file invisible. So the four things that keep it narrow are checked
    # here rather than trusted to the comment that describes them.
    saved_vl = dict(VERSIONED_LINES)
    try:
        VERSIONED_LINES.clear()
        VERSIONED_LINES[("CMakeLists.txt", 14)] = "project(Anamorph VERSION"
        check("the declared line is live on the real tree", verify_versioned_lines(), [])

        # It must FAIL when the line stops being what it claims -- otherwise the
        # entry is an exemption rather than a substituted assertion.
        VERSIONED_LINES.clear()
        VERSIONED_LINES[("CMakeLists.txt", 14)] = "this-token-is-not-on-line-14"
        check("a token that is not there is reported", len(verify_versioned_lines()), 1)

        # Out-of-range and unreadable are findings, never tracebacks -- the same
        # rule section 8c applies to the gloss list.
        VERSIONED_LINES.clear()
        VERSIONED_LINES[("CMakeLists.txt", 10 ** 7)] = "x"
        check("a line past the end of the file is reported", len(verify_versioned_lines()), 1)
        VERSIONED_LINES.clear()
        VERSIONED_LINES[("no/such/file.txt", 1)] = "x"
        check("an unreadable file is reported, not raised", len(verify_versioned_lines()), 1)
    finally:
        VERSIONED_LINES.clear()
        VERSIONED_LINES.update(saved_vl)
    check("...and the table is restored afterwards",
          VERSIONED_LINES.get(("CMakeLists.txt", 14)), "project(Anamorph VERSION")

    # THE SUBSTITUTION IS KEYED ON `(path, line)`, so a declaration for one line
    # cannot excuse its neighbour. Asserted structurally, because the alternative
    # -- a table keyed by file -- is the mistake this shape exists to avoid.
    check("the table is keyed by a (path, line) pair, not by a path",
          all(isinstance(k, tuple) and len(k) == 2 and isinstance(k[1], int)
              for k in VERSIONED_LINES), True)

    # AND IT APPLIES ONLY TO AN ANCHOR THAT DID NOT MOVE. The substitution sits
    # behind `lo == hi` in `anchor_still_right`, so a re-aimed anchor -- the case
    # `DELIBERATE_REAIMS` is for -- still goes through the base comparison. That
    # guard is the difference between "this line's content is allowed to change"
    # and "this line is not checked", and a rewrite that drops it would be
    # invisible on a clean tree.
    # The literal is split so this line cannot match itself; a self-matching
    # source check counts its own text and passes no matter what the code does.
    check("the versioned substitution is gated on the anchor not having moved",
          read(__file__).count("if lo == hi and (tracked, lo) in " + "VERSIONED_LINES:"), 1)

    # BOTH CHECK PATHS ASK THE SAME QUESTION, and this is the case that proves
    # it. `anchor_still_right` is driven directly on synthetic sources, the way
    # section 8e drives `glossed_problems_in`, because the defect it fixes was
    # invisible from the real tree: the count-mismatch path -- reached only when
    # a document changes HOW MANY times it cites a file -- had its own bare text
    # comparison, so a version bump landing in the same change set as an added
    # citation reported `CMakeLists.txt:14` as drifted and re-blocked the
    # release. Reproduced on the real tree before the fix, green after, and a
    # drift on a NEIGHBOURING line of the same file still fails on that path.
    saved_vl2 = dict(VERSIONED_LINES)
    try:
        VERSIONED_LINES.clear()
        VERSIONED_LINES[("F", 1)] = "VERSION"
        # `line_of` takes a LIST of lines, which is the shape both check paths
        # hand it (`base_src`/`now_src` hold split sources).
        base = {"F": ["project(X VERSION 1.0)", "name X"]}
        now  = {"F": ["project(X VERSION 2.0)", "name X"]}
        # The version line changed and the anchor did not: excused, on the same
        # call shape the count-mismatch path uses (`a2 == a`).
        check("a versioned line is excused when the anchor did not move",
              anchor_still_right("F", base, now, 1, None, 1, None), True)
        # A different line of the same file is NOT excused.
        now2 = {"F": ["project(X VERSION 2.0)", "name Y"]}
        check("a neighbouring line of the same file is still compared",
              anchor_still_right("F", base, now2, 2, None, 2, None), False)
        # A MOVED anchor is not excused either, even on the declared line.
        base3 = {"F": ["a", "project(X VERSION 1.0)"]}
        check("a moved anchor is not excused by the declaration",
              anchor_still_right("F", base3, now, 2, None, 1, None), False)
    finally:
        VERSIONED_LINES.clear()
        VERSIONED_LINES.update(saved_vl2)

    # ONE FUNCTION, NOT TWO. The defect above existed because the two paths each
    # carried their own copy of the decision and only one of them grew the
    # substitution. Asserted structurally, because a future edit that re-inlines
    # either copy would pass every behavioural case above on the day it landed.
    own = read(__file__)
    check("both check paths call the shared decision",
          own.count("all(anchor_still" + "_right(tracked, base_src, now_src"), 2)

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
    for doc, _whole_base, whole in sorted(DELIBERATE_REAIMS):
        try:
            body = read(doc)
        except OSError:
            check(f"DELIBERATE_REAIMS names a readable document ({doc})", False, True)
            continue
        # The CURRENT spelling is the one the document must contain; the base
        # spelling belongs to a revision, and requiring it here would fail every
        # correct declaration.
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

    # THE GLOSSED ANCHORS, CHECKED THE SAME WAY AND FOR THE SAME REASON. Like a
    # declared re-aim this needs no base revision, because it is not a question
    # about drift: it asks whether an anchor lands on the thing its own document
    # says it lands on, in the tree as it is now. That is the one question the
    # drift test structurally cannot ask -- an anchor already wrong at the base
    # stays wrong and stays green, which is stated at the top of this file and
    # was true of nine anchors when this check was written.
    #
    # SAME MODE SPLIT, and it is `reaim_stops_the_run` itself rather than a copy
    # of its condition: `--check` VERIFIES so it stops; `--fix` REPAIRS so it
    # warns, continues and still exits non-zero. The reason `--fix` must not try
    # to repair one is sharper here than for a declaration: `--fix` re-anchors by
    # the LINE MAP, which faithfully carries a wrong aim to its new line. Moving
    # an anchor that points at the wrong code leaves it pointing at the wrong
    # code. Only a human who reads the gloss can re-derive it.
    gloss_problems = verify_glossed_anchors()
    if gloss_problems:
        for doc, whole, expect, why in gloss_problems:
            if args.fix:
                print(f"::warning::{doc}: the citation {whole} names {expect!r}, "
                      f"which is not there — {why}")
            else:
                print(f"::error::{doc}: the citation {whole} names {expect!r}, "
                      f"which is not there — {why}", file=sys.stderr)
        if reaim_stops_the_run(gloss_problems, args.fix):
            print(f"\ncheck-citations: {len(gloss_problems)} citation(s) do not contain what "
                  f"they say they contain. This is not drift and `--fix` cannot repair it — it "
                  f"re-anchors by the line map, which would carry the wrong aim to a new line. "
                  f"Re-derive each line number from the symbol its own document names.",
                  file=sys.stderr)
            return 2
        print(f"check-citations: {len(gloss_problems)} citation(s) do not contain what they "
              f"say they contain. Re-anchoring continues below, but those anchors are NOT "
              f"repaired by it and this run will still exit non-zero.")

    # A `VERSIONED_LINES` entry turns the base comparison off for one line, so
    # the thing that keeps it honest -- that the line is still what it claims to
    # be -- has to run before anything leans on it, and has to be a hard failure
    # in every mode. Unlike a gloss this is not repairable by re-anchoring: the
    # declaration names a line by number, and if that line is no longer the
    # project declaration the fix is to move the declaration, which only a human
    # reading both can do.
    versioned_problems = verify_versioned_lines()
    if versioned_problems:
        for path, line, token, why in versioned_problems:
            print(f"::error::VERSIONED_LINES {path}:{line} should contain {token!r} — {why}",
                  file=sys.stderr)
        print(f"\ncheck-citations: {len(versioned_problems)} declared versioned line(s) no "
              f"longer contain the token they name. That declaration turns the base comparison "
              f"off for that line, so it must not be left pointing somewhere else — re-derive "
              f"the line number or delete the entry.", file=sys.stderr)
        return 2

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
                    # The SAME decision the paired path makes below -- a
                    # version-bumped line is excused here too. This branch sees
                    # only unchanged spellings, so the anchors are their own
                    # counterparts.
                    if all(anchor_still_right(tracked, base_src, now_src, a, b, a, b)
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
                # BASE anchors named. `anchor_still_right` carries that rule and
                # the `VERSIONED_LINES` substitution for both check paths.
                same = all(anchor_still_right(tracked, base_src, now_src, a, b, a2, b2)
                           for (a, b), (a2, b2) in zip(anchors_o, anchors_c))
                if same:
                    continue

                if is_declared_reaim(doc, whole_o, whole_c):
                    # THE TRANSITION, which is now literally the key. This used
                    # to add both spellings separately, to paper over a lookup
                    # that accepted either side; the report below then had to
                    # intersect with the declaration set to avoid announcing the
                    # undeclared half. Keying on the pair removes both problems
                    # at once — what is recorded here is exactly what was
                    # declared, so `used_reaims` and the table are the same shape.
                    used_reaims.add((doc, whole_o, whole_c))
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
    # STILL INTERSECTED, though the reason has changed. `used_reaims` now holds
    # the same 3-tuple the table is keyed on, so the two sets cannot disagree in
    # shape; the intersection is kept because it makes that invariant explicit at
    # the one place a mismatch would print a lie. (It used to be load-bearing:
    # `used_reaims` held BOTH spellings of an honoured re-aim, and reporting it
    # raw announced the undeclared half, printing twice as many accepted re-aims
    # as the table has entries.)
    for (doc, whole_o, whole_c) in sorted(used_reaims & DELIBERATE_REAIMS.keys()):
        print(f"check-citations: ACCEPTED re-aim {doc}: {whole_o} -> {whole_c} "
              f"(declared in DELIBERATE_REAIMS for THAT transition only; its aim is "
              f"checked against {DELIBERATE_REAIMS[(doc, whole_o, whole_c)]!r} by "
              f"verify_reaim_targets)")
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
    for (doc, whole_o, whole_c) in sorted(DELIBERATE_REAIMS.keys() - used_reaims):
        if whole_c in base_doc_cites.get(doc, set()):
            print(f"check-citations: note — DELIBERATE_REAIMS entry {doc}: "
                  f"{whole_o} -> {whole_c} was not needed against {args.base}, which "
                  f"already carries the re-aimed spelling. Its transition is behind that "
                  f"base and can never match from there again. Safe to delete ONLY if "
                  f"that base is the branch's merge base; re-run against that first.")
        else:
            print(f"check-citations: note — DELIBERATE_REAIMS entry {doc}: "
                  f"{whole_o} -> {whole_c} was not exercised against {args.base}, which "
                  f"carries neither spelling — the entry belongs to a different base. "
                  f"Keep it.")

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
        return fix_exit_code(unmappable, reaim_problems, gloss_problems)

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
