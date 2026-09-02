# DOCUMENTATION_COVERAGE.md

Permanent documentation-coverage audit. **Future contributors/AI must update this on every
documentation-affecting change** (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`).

Coverage = how well the module/topic is documented. Confidence = strength of the evidence behind
that documentation (Verified / Partially Verified / Unverified / Not Supported).

Last updated: for the **0.9.6 change set** (2026-09-01, matching the CHANGELOG heading) — the
**engineering-review programme, rounds 1 through 25**, newest last in the body: round 1 (the
programme's first sweep: six engine/state/GUI fixes with Tests 43–46 and two state regressions,
the engaged Test 2/38 matrices, the KI-027 and RISK-007 filings, the v0.9.6 renumbering sweep, the
NOTICE pin + AudioUnitSDK section, the CI_CD job inventory, and the new
`worklogs/engineering-review/` programme worklog + live HTML dashboard); round 2 (CI recovery and
two restore defects, whose section also carries the **rounds 3 and 4** bullets); **rounds 5 and 6**
(the Level-5 audition specified, then recorded as PASSED — entered late, in round 7, and labelled
as such); **round 7** (the compatibility checklist taken to six of eight boxes with measured
evidence, and the test-count sweep); **round 8** (the legacy A/B contamination defect confirmed
and fixed, an obsolete macOS scope comment corrected, and the realtime-lint boundary re-measured and
left alone); **round 9** (the per-slot Level-Match residual refuted on impact — mechanism real,
no code changed); **round 10** (the release notes reconciled with the KI-013 outcome, and the
two duplicated knob-readout Undo entries consolidated); **round 11** (two restore fixes, one
refutation, and a full audit of the [0.9.6] changelog); and **round 12** (undefined behaviour in the
legacy-Settings conversion fixed, the latency regression test made deterministic, and ER-STATE-13
re-run on AArch64 with no change); and **round 13** (ER-STATE-17 verified on the real frozen
pre-0.8.4 fixture, and compatibility-checklist items 5 and 7 recorded on the maintainer's
attestation, closing the gate); **round 14** (partial modern Settings inheriting the previous
project, confirmed and fixed on the opposite path from the one reported); and **round 15** (the
off-message-thread re-prepare latency race confirmed under ThreadSanitizer and closed inside D-1,
with one new risk recorded); and **round 16** (the previous project's per-slot A/B Level-Match gains
surviving a restore, confirmed and fixed on two paths, with the modern-Settings validation question
investigated and deliberately left to a contract decision); and **round 17** (that investigation
finished at the consumers: one concrete undefined conversion fixed, the contract question deferred
with complete evidence, and the drag-recovery finding refuted); and **round 18** (the maintainer's
recovery policy implemented and ER-STATE-21 closed, and RISK-008 investigated to a class-B
classification); and **round 19** (the maintainer's real-host REAPER result recorded against
RISK-008, its disposition finalised, and the settled set audited for consistency — no production
change); and **round 20** (a restored session's modules gliding into their own sound, and a
malformed numeric boolean switching a setting on — both fixed at their source, with the state race
outside the latency fields classified as the already-deferred D-2 risk); and **round 21** (the phase
meter's own `ll * rr` overflowing in float on extreme-but-finite audio, so a perfectly correlated
signal read as fully decorrelated — fixed at that operation, with the state race re-measured and
again found to be the deferred D-2 risk); and **round 22** (the `docs` CI gate, red on a line of
this file that began with a pipe character, and the filtered-preflight habit that let it reach the
push — both fixed, no production change); and **round 23** (the balance meter's own `ll + rr`
overflow, a different operation from round 21's `ll * rr`, which published dead centre for a badly
lopsided pair — fixed, with round 21's mistaken note about it corrected in place); and **round 24**
(a valid preset from another plug-in loaded as if it were ours — adopting what it named and
defaulting the rest, with both loaders reporting success — closed by a root-type acceptance test
shared by both loaders); and **round 25** (a minimal `[0.9.6]` Change Log correction pass — the
release date, and five wording claims the current implementation contradicts).
**Header correction (round 7, 2026-09-01):** this line enumerated round 1 alone while the body
carried six later rounds, and dated the change set 2026-08-31 while the CHANGELOG `[0.9.6]` heading
had been re-dated to 2026-09-01 — the same drift the C6 correction below was written about,
recurring. Rounds 5 and 6 had additionally skipped this file entirely; their entry says so rather
than pretending it was written at the time.
**Header correction (C6, 2026-08-31):** this line previously said "for the 0.9.5 change set
(2026-08-22, matching the CHANGELOG heading)" while the CHANGELOG's `[0.9.5]` heading had been
re-dated to 2026-08-30 and the file's own body already carried five later 0.9.5-set rounds the
header never enumerated — the **platform-coverage audit + MSVC adoption rounds** (ADR-0032, the
blocking Windows A/B gate), the **[0.9.5] changelog audit** (date + six corrections), the
**toolchain hold** (ADR-0033: Clang stays 22, release identity asserted), the
**missing-metadata verification bypass closure** (`setup-llvm-apt.sh --self-test`), and the
**citation-exemption lifecycle re-key** (`DELIBERATE_REAIMS` transition-scoped). Those rounds'
entries are in the body below; only this header had gone stale.
Prior: the **0.9.5 change set** — the
**A7-0 attempt** (first below, no code and no rows filled), then the **PR #127 review round**, then
the **A7-2 investigation** (no code), then the **A7-1 implementation round**, then the
**A7-2T oracle**, then **A7-2B + A7-5E + A7-9C**, then **A7-5E closed on the shipping toolchain**,
then the **four A7 decisions implemented** (ADR-0031's x86-64 ISA baseline and its floor, the
cross-architecture numerics contract, the A7-9 fixpoint gates with Test 41, A7-2B's corner accepted,
and one recorded bound corrected), then the five header-missed rounds above — **newest last within
each set**. Under it, the **0.9.4 change set** is retained in full
(2026-08-21, matching its own CHANGELOG heading — re-dated
from 2026-08-15 in the hover-occlusion round, on 2026-08-20, and again on 2026-08-21, each time
because the version took a further user-visible change) — the
**A7 performance audit**, then the
**spent re-aim declaration sweep**, then the
**SIGNAL_FLOW anchor restoration**, then the
**roadmap tail: instruments, the declined JUCE cache, gloss-checked anchors and the editor lifetime**, then the
**post-merge drift sweep**, then the
**Linux release toolchain move to Clang**, then the
**Linux installer migration and changelog-completeness audit**, then the
**tooltip source-of-truth round**, then the
**tooltip investigation that shipped no fix**, then the
**stale-anchor correction**, then the
**animation-landing round**, then the
**overlay-occlusion and idle-latch round** (closing KI-024 and KI-025), then the
**hover-occlusion round**, then the
**shared-action-input round**, then the
**mismatched-new-delete round**, then the
**allocation-family round**, then the
**lint-count round**, then the
**policy-topology round**, then the
**armed-transition round**, then the
**reachability-and-runnability round**, then the
**parser-and-evidence round**, then the
**gate-liveness round**, then the
**environment-assertion placement round**, then the
**follow-up review round**, then the
**macOS-symbolication and review round**, then the
**engineering-roadmap implementation round**, covering in two batches: the leaf-layer
`-Wfunction-effects` check, the performance-benchmark harness, `setStateInformation` fuzzing, the
GCC-only warning gate, LeakSanitizer promoted to a gate, the pluginval crash-retry scoped to its
justification, then commit-SHA pinning for every action, one composite action for the Linux setup,
the **shipped Linux ABI floor**, the Windows toolchain record, the macOS symbolication contract
corrected, the committed **DSP bit-identity harness**, and the citation gate reporting the
declarations its own `--fix` invalidates — then the
**review round that found the allocation guard was blinding the RealtimeSanitizer lane**, then the
**allocation guard + static realtime lint completing ADR-0029's three tiers, and the
release-blocking / THREAD_MODEL corrections** (first below), then the
**realtime-enforcement strategy (ADR-0029): RealtimeSanitizer at the annotated audio entry point,
its own CI lane behind a liveness canary, and the review-cleanup that preceded it** (first below),
then the **engineering-capability audit: extended UBSan coverage, the LTO validation gap, the wrapper
audio path + three DSP feature-coverage tests, `preflight.sh`, and the realtime-doc anchor rot**
(first below), then the
**dependency-update audit: Clang 18 → 22 (upstream stable, from apt.llvm.org) and the Dependabot
group split** after it, then the
**stale re-aim declaration, protected history, non-gating cache statistics** after it, then the
**citation follow-up: two missed anchors and four that were wrong on arrival** after it, then
the **citation-gate coverage for the build definition and the CI workflow** after it, then the
**CI compiler cache + job timeouts** after it, then the
**native Intel macOS job** after it, then the
**raw-string lexing fix** before it, then the **withdrawn debuglink claim** before it, then the
**unterminated block-comment invariant**
before it, then the **unterminated-literal invariant +
citation scope wording** before it, then the **`_deps` scan scope + visible FTZ relaxation**, then
the **line-splice diagnostic fix**, then the **portability-scanner false negative**
before it, then the **post-merge citation gate fix**
before it, then the **merge-result / script-anchor / strictness round**, then the **check-docs
false-green fix**, then the **lint self-verification round**,
then the **macOS Intel artifact gate**,
then the **documentation re-sync**, then the **CI review follow-up** that one follows, then the
**CI/validation round** it corrects, then the four
AppleClang 21 `-Wimplicit-int-float-conversion` fixes, the macOS CI runner move
`macos-14` → `macos-latest` that surfaced them, then the C++17 → C++23
language-standard migration, both applied on top of the JUCE 9.0.0 → 9.0.1
dependency upgrade in the same version; the JUCE entry follows them. Under it, the **0.9.3 change set** (2026-08-11) is retained in full — six editor-only GUI interaction fixes on
top of 0.9.2 (add-split preview line, unified pop-up dismissal, pop-up lifetime across a hidden,
destroyed or backgrounded window, menu width, disabled menu items, Tooltips off) plus a
**packaging round** (Linux per-user install default; the macOS re-install defect INC-012), landed
across seven rounds; the entries below run newest-first. Below them, the 0.9.2
entry (2026-08-07) is retained in full.

**A7-0 attempted and declined (2026-08-22): the budget rows stay TODO and RISK-002 stays open,
because the one thing missing is still a machine. No rows filled, no code changed.**

The harness builds and runs end to end after A7-1 — that half was never in doubt. The available
machine was measured against what this section requires and fails it four ways, each checked rather
than assumed: `/proc/cpuinfo` reports `Intel(R) Xeon(R) Processor @ 2.80GHz`, a **masked virtual
model string with no SKU**, so `AnamorphBench` prints something that satisfies constraint C2's letter
while identifying no actual processor — the one failure mode C2 exists to prevent;
`systemd-detect-virt` reports **docker**, so it is a container and not a desktop; **no `cpufreq` is
exposed**, so neither the governor nor host-side or thermal throttling can be observed, let alone
pinned; and the **load average was 0.44 / 2.10 / 1.69** while the bench ran, so it was not idle.

**Stable medians are not the test, and this run is why.** Across three consecutive invocations the
working reference measured 193.79 / 193.31 / 194.09 ns/sample — 0.4 %, far better than the 7.2 %
recorded when this was last attempted. The column RISK-002 actually turns on is the **worst single
block**, and it measured 104.0 / 128.5 / 118.2 µs over those same three runs: a **23.6 % spread** on
the one figure the open question needs. A tempting median would have bought a number that could not
answer the question it was collected for.

**One gap recorded for whoever does run it:** `AnamorphBench` prints the CPU string, the core count
and the compiler, but not the OS version or the build configuration, while this section asks for
CPU/OS/compiler beside every cell. Those two must be written down by hand alongside the table, or the
harness taught to print them. Noted rather than fixed: the roadmap's next action is measurement, and
changing the instrument is not measurement.

`PERFORMANCE_BUDGET.md` carries the disqualification checklist at the paragraph that explains why the
rows are still TODO, so the next attempt starts from it. **RISK-002 is unchanged** — its evidence
row already records what the A7 audit measured and what it explicitly could not close. [Verified]

**PR #127 review round (2026-08-22): a real hole in the version-bump exemption, and a numbers pass
that reconciled every check-count claim against the binaries rather than against arithmetic.**

**The exemption had a second door, and it was open.** `VERSIONED_LINES` was consulted in the paired
check path but not in the **count-mismatch** path — the branch reached when a document changes HOW
MANY times it cites a file. Each path carried its own copy of "is this citation still right?", and
only one of them grew the substitution. Consequence: a version bump landing in the same change set as
an added citation of `CMakeLists.txt` reported line 14 as drifted and re-blocked the release the
substitution exists to unblock. **Reproduced on the real tree before the fix** — one extra citation
appended to `RELEASE_PROCESS.md` turned `--check` red with `UNMAPPABLE CMakeLists.txt:14` — and green
after. The fix is one shared `anchor_still_right()` used by both paths, so the two cannot drift apart
again; the duplication was the defect, not a symptom of it.

**Proven not to have widened anything.** With the count mismatch AND a seeded drift on a
*neighbouring* line of the same file (`CMakeLists.txt:427`, cited alongside `:14` in `TRADEMARKS.md`),
the run still fails on that path — so the exemption still covers one declared line and nothing else.
Self-test 131 → 135 cases: three behavioural cases driving `anchor_still_right` directly on synthetic
sources (excused when the anchor did not move; a neighbouring line still compared; a MOVED anchor not
excused even on the declared line) plus a structural case asserting both paths call the shared
function. That structural case was **proven live** by re-inlining the old comparison, which fails it
1-of-135. Its literals are split so the check cannot match its own source text — a self-matching
source check counts itself and passes regardless of the code.

**Check counts reconciled from the binaries.** The coverage entry below said "174 checks (Test 39
adds 12)" while `HANDOVER.md` and the v0.9.5 worklog said 178 and 16; the worklog separately gave
178 plain against 172 ASan and called that "two fewer". Both were stale rather than wrong-in-kind:
Test 39 gained a fourth check per rate on review (the mixed-block-size run), and the ASan figure had
not been re-measured since. Re-run, not re-derived: **plain 178 / ASan 176** for `AnamorphTests`,
**920 / 920** for `AnamorphStateTests`, 0 failures and **0 sanitizer diagnostics** in every case. The
delta is 2 and always was — Test 38's malloc half compiling out under ASan and saying so. The same
pass corrected `RELEASE_HARDENING_PLAN.md` ("37 DSP self-tests … 162 checks") and
`TESTING_POLICY.md` ("the 37 DSP self-tests"), both left behind when v0.9.5 added Test 39. Historical
round entries in this file keep their own figures: they record what was true when they were written.

**Test 39 now varies the block size**, which review identified as the case it was missing:
`linHistSlide` carries the JUST-PROCESSED block's length, so consecutive gather blocks of differing
length are what the slide arithmetic is about, and two runs each at a fixed size could both be
correct with the offset confused for a constant. A third run cycles 32/128/64/256/32 — summing to
512, so events still land on a block boundary and every neighbouring pair differs — and is
bit-identical to the 512-sample reference at all four rates. Its comparison is on BITS rather than
`==`: `-Wfloat-equal` is at zero in the Clang baseline, and a float `==` is the wrong predicate for a
bit-identity claim anyway, calling +0 and −0 equal (which this module's own signed-zero algebra cares
about) and NaN unequal to itself. That warning is what turned `linux` red; `source-lint` went red on
a citation this round's own change set had invalidated, re-aimed by hand and declared for its one
transition. **`VelvetNoise.cpp` itself was reviewed and left alone** — the invalidation-on-entry,
the re-arm-on-gather-exit, the `reset()` ordering and the copy bounds were all confirmed sound.
[Verified]

**A7-2 investigation (2026-08-22): the roadmap's own proposal, prototyped and measured — and
rejected on the measurement. No product code changed. The alternative it turned up is recommended,
planned, and deliberately not implemented.**

**The residual term, attributed.** Of the 13,502 Ir/block A7-1 left at 48 kHz, **8,704 (64 %) is the
`memcpy` of the slide** — 34,624 of 39,429 (88 %) at 192 kHz. The remainder is ~4,800 Ir/block,
**identical at both sample rates**: per-block libm conversions in Level-Match and the meters, the
engine's own bookkeeping, the meter publish. That is this item's floor, and it is not VelvetNoise's.

**Two prototypes, both bit-exact over 180 configurations** (9 scenarios × 5 block sizes × 4 sample
rates, FNV-1a over every output sample): the **double-buffer** the roadmap proposed (a cursor into an
over-sized image, compacting when it runs out) and a **gather straight from the ring** with no linear
image at all — for each tap, 1–3 contiguous runs over the ring plus this block's own mids, which
keeps H5's unit-stride property while deleting the structure H5 built to get it.

**The proposal is the weaker half of the fork, and the reason is measured.** It is 1–2 points ahead on
average (−13.1 % vs −11.4 % at 48 kHz/32; −37.9 % vs −36.8 % at 192 kHz/32) and it **amortises the
copy rather than removing it**, so the worst block still pays the full 8,704 Ir. Counted in the
prototype over 4,000 gather blocks: one compaction in **81.6** blocks at 48 kHz/32, one in **20** at
128, one in **five** at 512-sample blocks. A plug-in drops a buffer on its worst block, not its
average one — so this trades a uniform per-block cost for a periodic full-size spike, which is the
wrong shape for an audio thread even though the mean falls. It also roughly **doubles** the history
buffer (+8.6 KB at 48 kHz, +34.5 KB at 192 kHz per instance) and adds a **second** cross-block
invariant to a module that gained its first in v0.9.5. The ring gather compacts **zero** times at
every setting, **frees** the buffer instead of growing it, and **deletes** both cross-block flags
rather than extending them. Both remove the sample-rate dependence of the fixed term entirely: at
32-sample blocks, 48 kHz and 192 kHz land on the same figure where the shipped engine differs by
810 Ir/sample.

**Why it stops at investigation, and neither reason is that the change is risky.** A7-2's own gate is
**A7-0** — *"gated on A7-0's evidence that small buffers still hurt"* — and A7-0 has not been done:
there is still no wall-clock datum from a named machine and RISK-002 is still open. Instruction
counts are the right unit for comparing two implementations and the wrong one for deciding whether a
user is dropping buffers. And the recommended change is **not the change that was scoped**: swapping
a rewrite of the Wave-2 H5 gather kernel in for the proposed double-buffer, unprompted, in the same
function v0.9.5 changed hours ago and before that release has had its audition, is what the
architecture review gate exists to catch. `PERF_AUDIT_A7-2_INVESTIGATION.md` carries the evidence and
a ready-to-execute plan; the A7 audit worklog and its HTML report are updated in place so the roadmap
row reads "proposal rejected" rather than "consider later". [Verified]

**A7-1 implementation, v0.9.5 (2026-08-22): the optimization the audit below sized now shipped, its
evidence re-derived on the product tree, a permanent guard added for the cross-block state it
introduces — and the release blocked by this repository's own citation gate, for a reason no release
had met before.**

**The change.** `VelvetNoise` was rebuilding its whole `round(0.045*sr)`-sample decorrelation window
from the ring on EVERY block (`src/dsp/VelvetNoise.cpp`), independent of `numSamples` — a fixed
per-block cost that grows with the sample rate. It is now slid forward from the previous block's
image. **The shipped design is deliberately not the throwaway one** the audit measured: the offset is
taken and cleared on ENTRY to `processBlock` and re-armed in exactly one place, so every other path —
and every path a later round adds — leaves the image invalid *by construction*, rather than by an
RAII guard a reader has to find. `reset()` clears it too, and that line is load-bearing: `reset()`
runs between blocks, after the offset was armed.

**The estimate held to one decimal place.** Product tree vs the audit's throwaway build: −14.3 % at
48 kHz/32, −8.5 % at 48 kHz/64, −4.7 % at 48 kHz/128, −2.5 % at 48 kHz/256, −32.3 % at 192 kHz/32,
−15.0 % at 192 kHz/128 — every figure the audit predicted, reproduced. Two rows it had not measured
land where the model says they should (−7.5 % at 44.1 kHz/128, −8.7 % at 96 kHz/128). The per-block
term fell **24,302 → 13,502 Ir** at 48 kHz while the marginal per-sample term stayed at **1596.6** —
the arithmetic signature of a change confined to the refill, and worth more than the percentages
because it says *where* the saving came from.

**Class A, with the committed instrument as the primary evidence.** `AnamorphDspDump` — the
DEPENDENCY_POLICY rule-2 twin harness — reports its 32 scenarios identical before and after, having
first self-checked that all 32 are repeatable AND distinct, so the diff is a live comparison rather
than 32 equal hashes proving nothing. Beside it: a **180-configuration** sweep (9 scenarios × 5 block
sizes × 4 sample rates) hashed over every output sample of both channels, 0 mismatches; reported
latency unchanged; no parameter, serialization or threading change.

**Test 39 is the guard, and it proves itself rather than being trusted.** The same audio at 512 and
at 32 samples must be bit-identical, at four sample rates. It **passes unchanged against the
pre-0.9.5 engine**, so it asserts a contract the module already had and nothing was checking. It
fires on both defect classes, seeded: a wrong slide fails at sample 32, a missing invalidation at the
transport-stop block. Its own premises are asserted too — the engaged stretch must really
decorrelate, and the transport stop must really flush the wet (15.4–25.2 % of the engaged figure with
the stop, **90.6–128.9 % with the stop event removed**, so the 50 % bound sits between two measured
populations instead of being a hopeful inequality).

**THE CITATION GATE BLOCKED THE VERSION BUMP, and that was not a false alarm.** Six documents cite
the `project(Anamorph VERSION x.y.z ...)` line, whose text changes by definition; the anchor never
moves. Checked rather than assumed: the 0.9.3 → 0.9.4 bump (`3ebdf69`, 2026-08-14) predates
`CMakeLists.txt` joining `TRACKED` (`129457e`, 2026-08-16), so `RELEASE_PROCESS.md` step 1 had been
un-runnable under the gate since the day the gate started watching that file, and no release had
exercised it. `DELIBERATE_REAIMS` cannot express it — `is_declared_reaim` returns False when the
spelling is unchanged, deliberately, so an entry cannot outlive its one transition, and here there is
no transition to outlive. `VERSIONED_LINES` was added for exactly this: keyed by one exact
`(path, line)` pair, the base comparison **replaced** by a permanent token check, applying only while
the anchor has not moved. Proven narrow rather than argued: a *neighbouring* cited line drifted in the
same citation still fails; an anchor moved by an inserted line **exits 2** rather than excusing
whatever is now on line 14; missing token, past-EOF and unreadable-file each report a finding rather
than a traceback. Self-test 123 → 130 cases, including a structural check that the substitution stays
gated on the anchor not having moved — the guard a later rewrite could drop invisibly on a clean tree.
Recorded where it bites: `RELEASE_PROCESS.md` step 1, `CI_CD.md`, `REPOSITORY_MAP.md`.

**One finding recorded and deliberately not acted on.** Proving Test 39 live turned up that the
**Amount one-pole never reaches zero when turned down**: with a 0 target the update is
`a -= 0.0015f * a`, and under FTZ the DECREMENT underflows first, so the glide stalls at
`7.82561114e-36` and stays there. `currentAmount > 0.0f` therefore stays true and the Wave-5 PARKED
fast path — whose own comment says the one-pole "flushes to true zero" — is unreachable after a user
turns Amount down. No audio effect (the measured contribution is one ULP of the M/S round trip, the
same as a true park) and the path is still reached from a fresh `prepare()` with Amount at its 0
default, which is the state it was written for. The repair is Class B (it moves the sample at which
the amount reaches zero), so it is filed as **A7-9** with its measurement rather than folded in.

**A7-9 is wider than that entry says, and the follow-up round measured how much.** The stall is not
a `VelvetNoise` quirk but a closed form, `≈ FLT_MIN / k` for a glide of rate `k`, and **three**
shipped parked fast paths rested on the same claim — each said the one-pole flushes to true zero —
and each gates on a **value** test that a stalled glide defeats. (**The three comments have since
been corrected under A7-9C**, so the claim no longer appears in the source; each site now states the
precondition without the false justification and records how the state is and is not reached. The
gates are unchanged — that is the A7-9 decision, not this one.) (Velvet's *density*
gate, three lines above its amount gate, tests the **fixpoint** instead and is therefore immune —
the same file carries both the defect and its remedy.) So none of the three parks is reachable after
a user turns its control down. Measured: `HaasProcessor` stalls at `1.17487956e-35` and
`ChorusEngine` at `5.63638313e-36` **at 48 kHz** — Velvet's and Haas's coefficients are compile-time
constants so their stall is rate-independent, while Chorus's `wSmooth` is `1/(0.01·sr)`
(`src/dsp/ChorusEngine.cpp:80`) so its stall scales with the rate, reaching `2.25593495e-35` at
192 kHz — and the missed park costs +203 % and +370 % of the parked module
at 48 kHz / 128 — for Velvet, +752 % at 48 kHz / 32 and +2,372 % at 192 kHz / 32, because a stalled
amount above zero also satisfies the H5 gather gate and puts the engine on its most expensive path.
Still no audio effect on real signal (0 of 102,400 samples differ in all three); the residual shows
only on digital silence, bounded at **4.476e-36** across the rate range (Chorus at 192 kHz;
1.643e-36 at 48 kHz alone). *(Two later corrections, both marked at their sources: the 4.476e-36
figure was that harness's observed maximum, not a bound — Test 41 measures 3.5× more and derives
`FLT_MIN/k` instead; and "only on digital silence" is too narrow — near-silent NONZERO samples
below ≈ 2–4e-28 of full scale move too, measured 2026-08-30 in
`PERF_AUDIT_A7-9_NEARSILENT_SCOPE.md`.)* Full evidence and the maintainer decision:
`worklogs/performance/PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md` Part III. The two additional
comments are drift reported and deliberately not edited this round — the correction rides with the
A7-9 decision.

**Verification.** `AnamorphTests` **178 checks** at the time of that round (162 before; Test 39 adds
16 — four checks at each of four sample rates; **202 since Test 40**, below) and `AnamorphStateTests` 920 checks, both 0 failures. Under ASan + UBSan + LSan
with the `sanitizers` job's own flag set: **176** and 920 checks, **0 sanitizer diagnostics** — run for `local-bounds`, which checks the new copy's
range with a tool rather than only with the argument in the source. `check-realtime` 44 files /
0 violations, `check-portability` 52 / 0, `check-docs` 104 clean, `check-citations --self-test`
130 cases and `--check --base origin/main` green, full `scripts/preflight.sh` green.
`worklogs/performance/PERF_AUDIT_v0.9.5_IMPLEMENTATION.md` is the round's record; the A7 audit below
and its HTML report are updated in place to say A7-1 shipped rather than describing it as proposed.
[Verified]

**A7 performance audit (2026-08-22): the engine re-measured after six optimization waves, and one
dominant cost found that the previous rounds recorded as absent — because they measured it in the one
configuration where it is switched off. Investigation only; no product code changed.**

**What the round produced.** `worklogs/performance/PERF_AUDIT_v0.9.4_INVESTIGATION.md` (the evidence
trail and the roadmap) and `worklogs/performance/PERF_AUDIT_v0.9.4_REPORT.html` (a self-contained
rendered companion for reading and assigning from). The HTML is a VIEW of the worklog, stated as such
in both files and in `REPOSITORY_MAP.md`, because a second copy of a decision is a second thing to
keep true.

**Instructions, not nanoseconds, and the budget document is why.** It states that a shared cloud
runner is not a wall-clock datum and that *"for attribution rather than totals, `valgrind
--tool=callgrind` … gives instruction counts that are stable across machines"*. Every figure in this
round is Ir, taken as the difference between a 3.0 s and a 1.0 s run so that process start, dynamic
linking and `prepare()` cancel exactly. **No number from this round is promoted into the
`PERFORMANCE_BUDGET.md` TODO rows**, and the round says so in three places — the container measured
spread up to 34 % on the committed bench's own cells, which is the evidence for that sentence rather
than a hypothetical.

**The finding.** Wave 5 attributed small-buffer per-block cost and closed it — *"~4.5k Ir of fixed
work per block… no single dominant item"*. That reproduces **exactly**, for the transparent default:
3,581 Ir/block measured here. In the configuration a paying user runs it is **24,287 Ir/block, 6.8×
larger**, and **20,369 of it (84 %) is `VelvetNoise::processBlock` alone**. The H5 tap-outer gather
rebuilt a `decorrSamps`-long linear history image from the ring every block, independent of
`numSamples` — 2,160 samples at 48 kHz, 8,640 at 192 kHz. That walk survived the A7-1 round as
the first-block-after-invalidation path, with the slide that replaced it directly above; **A7-2B has
since deleted both**, and the image with them — the ring is read in place, so there is no image to
refill and nothing to carry across a block (see the A7-2B entry at the end of this document). At 192 kHz / 32 it is **62.3 % of the whole engine**. The earlier closure missed it for a
mechanical reason worth recording: the gather is gated on `currentAmount > 0.0f || targetAmount >
0.0f` (`:132-135`), and the default `algoAmount` is 0 — **measuring per-block cost at the transparent
default measures the one configuration in which the dominant per-block item is switched off.**

**The fix was measured, not argued.** Implemented in a throwaway build outside the tree (§6.1 of the
worklog): the retained tail of the history image is the previous block's image shifted by that
block's length, so the masked ring walk becomes a `memmove` of the same floats behind a validity
flag. Measured **−14.3 % of whole-engine cost at 48 kHz/32, −32.3 % at 192 kHz/32, −4.7 % at the
128-sample common case**, and **bit-exact across 144 configurations** (9 scenarios × 4 block sizes ×
4 sample rates, FNV-1a over every output sample of both channels, zero mismatches). Class A by
measurement rather than by assertion — which is the standard the Wave 1–5 program set.

**Cross-validated against Wave 4 rather than trusted.** Rescaling Wave 4's idle shares out of its
own *"~14 % harness noise-fill"*: LevelMeters 26.3 % vs 28.6 % here, LoudnessMatch 27.7 % vs 30.7 %,
Correlation 3.4 % vs 3.8 %. Two independent harnesses two rounds apart agreeing to a few points.

**Two prices quoted for the first time, both maintainer decisions and neither reopened here.** A
host-bypassed instance costs **101 % of an active one** (85.1M vs 84.0M Ir/s) because the Issue-2
contract at `src/dsp/AnamorphEngine.cpp:856-862` keeps Measure + Predict running while bypassed, and
`loudness.process()` is handed the *processed* signal (`:1137`). And **59.3 % of the transparent idle
floor is metering and loudness analysis**, running with Level Match off and with no editor in
existence. W3-7 and W3-8 rejected gating those for reasons that still hold; what was missing was the
number, and the roadmap already names the queue position (*"one consolidated Review if idle-CPU ever
matters commercially"*).

**Eight candidates, one recommended.** Four were already disposed of in
`worklogs/POST_v0.8.12_AUDIT_AND_ROADMAP.md` §4 and are re-confirmed rather than re-litigated —
including the largest single consumer, the multiband LR4 bank at 41.7 % of the working reference,
which stays blocked behind the same AVX2 ADR as W5-D. The GUI was measured too (one full editor
repaint = 28.6M Ir, of which `Vectorscope::paint` itself is 2.9 % — the rest is JUCE's software
rasterizer), with its three limits stated rather than implied: Linux software renderer only, a forced
full repaint rather than the dirty-region steady state, and **the `juce::Timer`/`VBlankAttachment`
tick path not measured at all**, so the Wave 1–4 idle-GUI claims are neither confirmed nor challenged.

**Verification.** `check-docs` 103 files clean; `check-citations --self-test` 123 cases and `--check
--base origin/main` green; full `scripts/preflight.sh` green. Only `REPOSITORY_MAP.md` and this file
changed outside the two new worklog artifacts. [Verified]

**Spent re-aim declaration sweep (2026-08-22): all eight `DELIBERATE_REAIMS` entries retired after
the round that declared them merged. A declaration is good for exactly one transition; kept past it,
it is a standing licence for that anchor's next genuine drift. Removing them RESTORES drift checking
— a coverage increase, stated that way because a table shrinking to `{}` reads like the opposite.**

**Data only. No code changed.** The table is back at the empty resting state its own header
documents. `check-citations.py` is otherwise untouched: no regex, no mode, no exit-code path, and
no check was disabled, narrowed or exempted.

**Every entry was verified rather than assumed, in three independent ways.**

| Check | Result |
|---|---|
| the tool's own removability note, `--check --base origin/main` | 8 of 8 reported "not needed against origin/main" |
| the precondition that note attaches to — that base IS the branch's merge base | `git merge-base HEAD origin/main` = `origin/main` = `d44f004`, checked, not assumed |
| the transition actually landed — `git show origin/main:<doc>` carries the re-aimed spelling | 8 of 8 |
| `verify_reaim_targets()` — every declared aim still resolves | 0 misaimed, 0 unverifiable |

**The hazard a spent entry creates, measured on the real function rather than argued.**
`is_declared_reaim` returns `False` while the base and current spellings agree, so a spent entry is
inert **today** — and returns `True` the moment the cited code actually moves, which is precisely
when the gate is supposed to speak. Asked of the live function with an entry still present:
`(base :21, cur :21) -> False`, `(base :21, cur :22) -> True`; with the entry removed, `False`.
Keeping them would therefore have been the option that weakens validation.

**Proof the removal hides nothing:** `--check --base origin/main` reports **380 anchors, exit 0**
both with the entries and without them. An entry that was excusing something would have moved that
count; identical totals are the direct evidence that all eight were already inert.

**What removal costs, stated because it is not nothing.** `verify_reaim_targets` content-checks
every entry on every run, so retiring one drops that assertion.

* **Five lose nothing that matters.** The `ARCHITECTURE.md` ×2, `LATENCY_MODEL.md`,
  `PARAMETER_REFERENCE.md` and `THREAD_MODEL.md` anchors are all in `GLOSS_CHECKED_DOCS` documents
  and all carry the gloss the document itself writes — `juce::ScopedNoDenormals`,
  `isBusesLayoutSupported`, `updateLatency`, `applyAutoGain`, "Nothing is ever drawn on the audio
  thread". Each retired entry's expectation was that same symbol, so the identical claim is now
  asserted by the permanent mechanism instead of the one-shot one, base-independently, on every run.
* **Three do lose their only content check.** The `tests/state_tests.cpp:6-11` anchors in
  `POSTMORTEMS.md`, `procedures/TESTING.md` and ADR-0025 are not in `GLOSS_CHECKED_DOCS`, and opting
  those documents in would buy nothing today: measured, all three carry **zero** glossed citations,
  because those references are written as prose rather than as a parenthetical after the anchor.
  Getting them content-checked means rewriting prose in three documents — a change about those
  documents, not about this table, and left as a follow-up rather than smuggled into a housekeeping
  round. Drift checking on all three is unaffected and is now restored rather than excused.

**Self-test 131 → 123 cases, and the drop is not lost coverage.** Section 9 emits **one case per
`DELIBERATE_REAIMS` entry** ("entry is live: names a spelling its document really carries"), so an
empty table has nothing to assert — 131 − 8 = 123, exactly. Every structural case is unchanged, and
section 9 itself is intact and will emit again the next time an entry is declared.

**Verification.** `check-citations --self-test` 123 cases; `--check --base origin/main` 380 anchors;
`check-docs` 102 files clean; full `scripts/preflight.sh` green including both built suites
(`AnamorphTests` 162 checks, `AnamorphStateTests` 911 checks, 0 failures). Nothing outside
`scripts/check-citations.py` and this file is touched. [Verified]

**SIGNAL_FLOW.md anchor restoration (2026-08-22): 33 references no gate had ever seen, verified one
by one against the source, and the document restructured so they can be seen. The DSP-order
document is now the best-covered architecture document in the repository rather than the worst.**

**What was wrong.** `SIGNAL_FLOW.md` records the absolute processing order inside
`AnamorphEngine::process` — a `CLAUDE.md` hard-stop class, since changing that order needs an ADR.
It carried **35 references and the citation gate could see 2 of them.** The other 33 were written
bare (`:1108`), and `CITATION` requires a path, so no run in the gate's history had ever resolved
one. They were free to rot, and they had.

**The "uniformly 13 lines stale" summary was wrong, which is why each was checked rather than
shifted.** A Wave-4/5 performance round inserted 13 lines inside `process`, so anchors BELOW that
point were +13 — but the ones above it were not, and three references were wrong in ways no shift
would have fixed:

| Reference | Was | Is | Why |
|---|---|---|---|
| duck fade times | `:70-71` | `:72-73` | polarity smoothers, not the `switchIncOut`/`switchIncIn` constants the sentence describes |
| M/S decode | `:554-572` | `:554-567` | the range ran past the `if (p.msMode)` branch into the `else` (L/R domain) half |
| solo-agnostic Multiband | `MultibandWidth.h:29-32` | `:43-45` | 29-32 is flat-reconstruction phase compensation; the SOLO-AGNOSTIC statement is at 43-45 |
| oversampling gate | `:21-25` | `:21-25` | **correct already** — `osActiveFor`, above the insertion point. A uniform +13 would have BROKEN it |

Two more ranges (`Dry/Wet Mix`, `Output stage`) had ends that already overshot into the next stage's
comment banner and were tightened to the real section boundaries rather than shifted into a worse
version of the same error.

**Why the diagram lost its numbers.** Sixteen anchors sat in the ASCII block diagram, in an aligned
column. A path-qualified anchor is 26 characters; sixteen of them would have destroyed the alignment
that is the diagram's entire reason to exist. So the numbers moved OUT into a stage table beside it
and came back qualified — **each number now exists exactly once**, which also rules out the failure
this repository has hit twice: two copies of one line number drifting apart. The diagram kept the
order and the symbol, neither of which rots.

**No exception was added and no check was weakened.** `src/dsp/SoloMonitor.h` joined `TRACKED` (it is
first-party architecture evidence, and its sibling `MultibandWidth.h` was already there), and
`SIGNAL_FLOW.md` joined `GLOSS_CHECKED_DOCS`. Both are coverage INCREASES. The gate now sees **40
anchors where it saw 2**, and **23 of them are glossed** — content-asserted against the symbol the
document names, not merely watched for movement. All 35 citations were read back through the tool's
own resolver and land on their intended target.

**Limitation, stated rather than papered over.** A citation added in this change set has no
counterpart at the base revision, so the gate reports "the added ones are not checkable against that
base" for this run and checks all 32 from the next commit onward. That is the tool's honest
behaviour on new citations, not a gap opened here.

**Roadmap tail (2026-08-21): the two committed instruments got liveness steps, the proposed JUCE
cache was measured and declined, citation anchors began asserting what they name, and the editor
was constructed for the first time. Four items, one of them closed by NOT doing it.**

**`AnamorphDspDump` had no CI build at all.** It is the `DEPENDENCY_POLICY` rule-2 gate for a JUCE
bump, and it was the one committed harness with no protection against exactly the rot the bench's
own CI step exists to prevent. `linux-lto-tests` now runs it with `--self-check` rather than a bare
run, because the tool's own argument makes the stronger check the cheap one: a scenario set that
collapses prints 32 confident and meaningless hashes, which is what the 8.0.14 → 9.0.0 run shipped.
Measured here: 32 scenarios, all repeatable and all distinct. **Nothing is diffed in CI and no
baseline is stored** — that would be the golden-master test this repository has rejected.

**Both instruments were compiled only by GCC.** `linux-lto-tests` is a `gcc:16` container whose
configure names no compiler, so `bench.cpp` and `dsp_dump.cpp` were the only first-party TUs no
Clang job compiled — a Clang-only break in either reached no gate, and both sit behind
OFF-by-default options, which is why it was easy to miss. `linux` now compiles both
`-fsyntax-only` under the pinned Clang. Proved live in both directions locally; the first attempt
at that proof seeded against the wrong `main` signature and "passed" while checking nothing, which
is recorded because it is the failure mode the seeding was supposed to rule out.

**The JUCE cache was declined on measurement, and that is the finding.** The roadmap asked for an
`actions/cache` for "faster CI". The clone is already `GIT_SHALLOW` at a pinned commit: fetching
exactly that commit measured **5.0 s** for a 120 MB tree, while the cache replacing it is 44 MB
compressed and took **2.6 s** to decompress and extract before any download. A hit saves ~1.5 s
against jobs running 6–21 minutes, and buys a key to maintain plus a path by which the release
build could link bytes that are not the pinned ones. What the measurement found instead: one job
was fetching the same commit **three times** in one run, one `cmake -B` per build directory. Both
secondary configures now reuse `build-lto/_deps/juce-src` through the `ANAMORPH_JUCE_PATH` escape
hatch — no cache, nothing crossing runs. `sanitizers` still fetches twice and is left alone, named
so the omission is a decision.

**Citation anchors now assert what they name — nine were pointing at unrelated code.** The gate has
always compared an anchor against a BASE, so one already wrong at the base stays wrong and stays
green; its header says so and this repository's anchors were "adopted, not audited". The
expectation was already in the documents: the convention spells the symbol beside the line number,
and nothing read it. `GLOSS` extracts it and `verify_glossed_anchors` asserts the token is in the
cited lines, with no base revision. **A match, not a parse** — one backticked identifier or one
double-quoted string, alone in the parentheses; `(24 Hz timer)` asserts nothing rather than having
an assertion invented for it. **Opt-in by DOCUMENT, not by anchor**, for two reasons that were both
verified rather than assumed: an anchor-keyed table would hold a copy of the line number `--fix`
rewrites (the disease built into the cure — `invalidated_reaims` exists because that happened twice
in one change set), and a permanent entry cannot live in `DELIBERATE_REAIMS` at all because
`is_declared_reaim` reads that dict to turn the drift check OFF. Measured: 20 glossed citations
across seven architecture documents, **5 firing, all genuine, 0 false positives**; repo-wide 10
fire, 9 genuine and 1 false, which is why it is opt-in. All five corrected, each carrying a one-shot
declaration because a correction is textually indistinguishable from drift — without them `--fix`
proposes to undo all five, observed rather than inferred. Self-test 109 → 126 cases.

**The editor was constructed for the first time, and it is clean.** `AnamorphStateTests` already
compiled `PluginEditor.cpp` and already linked the GUI modules — `createEditor()` had simply never
been called, so the editor's constructor and destructor were the largest piece of first-party code
no instrument had executed. It now runs five construct/destroy cycles, headlessly, opening no
window (the null peer is asserted, not assumed). **No new CMake target**, which is both the cheaper
answer and the one that avoids a gated Build System change: the binary already had every source and
module it needed. The value is what it inherits — this suite already runs under ASan+UBSan+vptr,
LeakSanitizer, valgrind memcheck, LTO codegen and three platforms. Measured on first exposure:
940×720, 68 direct children, five cycles, **0 failures and no sanitizer report** under
`detect_leaks=1`. **Linux-only deliberately**: headless construction is unverified on Windows and
macOS where this suite is a blocking gate, KI-007 records that the Windows runner cannot host editor
GUI tests, and every instrument this feeds is a Linux job — so the scoping costs no coverage.

**CI found what the local run could not: the editor reaches vendored HarfBuzz.** The `sanitizers`
job went red on the first push. The local sanitizer run that preceded it used
`address,undefined,vptr` and the job's real set is seven groups wider — `implicit-conversion` was
never on locally, so the finding could not appear. Reproduced with the exact flags, then fixed:
constructing the editor calls `refreshPresetDisplay()` → `textWidth()` → JUCE font shaping →
`hb-face.cc`, where `face->num_glyphs = -1` assigns to an `unsigned` as HarfBuzz's documented
"not computed yet" sentinel. UBSan is right that it narrows; it is vendored code and no project
signal. `scripts/ubsan-ignorelist.txt` exempts **that one sub-check in that one tree** — the
`[implicit-conversion]` section header keeps ASan, the rest of `undefined`, `vptr` and the four
other groups instrumenting the vendored code in full, and `*juce-src/*` matches no first-party
path. This is `TESTING_POLICY`'s sanitizer procedure step 3 taken rather than step 4 violated: the
alternatives were dropping the flag build-wide or not constructing the editor under sanitizers.
Verified in both directions on clang-22 in the real build — vendored silenced, a conversion seeded
into `src/PluginProcessor.cpp` still fails the run at 920 checks → exit 1.

**Counts corrected while there.** The state suite is **15 tests / 920 checks**; the documents said
13 and 911. The test count was already stale by one before this round — the tooltip test was never
counted — which is why it is stated here rather than quietly incremented.

**Found and NOT fixed, reported rather than carried silently.** `SIGNAL_FLOW.md` — the
DSP-signal-order document, a `CLAUDE.md` hard-stop class — carries one qualified anchor and **33
bare ones, uniformly 13 lines stale**. Bare anchors carry no path and `CITATION` requires one, so no
run has ever seen them. It is deliberately not in `GLOSS_CHECKED_DOCS`: adding it would report
coverage of that evidence while 33 of its 35 anchors stayed invisible. Four more wrong anchors sit
in ADR bodies (ADR-0001 `toEngine`, ADR-0002 `kVersion`, ADR-0007 `applyAutoGain`, ADR-0009
`sanitize`); an ADR is a decision record and correcting its evidence is a separate judgement.

**Post-merge drift sweep (2026-08-21): four documents corrected against the tree, one deleted
comment block restored, and the five spent re-aim declarations retired. No code, no gate and no
release behaviour changed.**

**What this round is.** A reconciliation of the standing engineering roadmap against `main@85697de`
found most of it already shipped, and found that what remained was almost entirely documentation
that had drifted from code which moved underneath it. Four findings share one cause: **a change
corrects the paragraph it is about and leaves the summary that introduced it.** The largest was made
by the immediately preceding commit.

**`12c545d` deleted the `sanitizers` job's entire comment header, as collateral.** That header sat
between the end of `linux-clang` and `sanitizers:`, so deleting the job took it too — the rationale
for ASan+UBSan, for deliberately not using MSan, for running valgrind over *both* suites, for
`detect_leaks=1` becoming a gate, and the topology paragraph that two other job headers still refer
to by name ("see the `sanitizers` comment" at `linux-lto-tests` and at `realtime`). Two referrers,
zero referent, through every green run since. **No gate could have caught it:** the citation gate
anchors `file:line` references, and these are prose pointers naming a comment. Restored verbatim
from `be99567`; every claim in it was re-checked against the current file first, and the figure it
carries ("160 + 900 checks") is left alone as the dated 2026-08-18 measurement it is.

**`THREAD_MODEL.md` disagreed with itself about where the threading mechanisms live.** The Threads
section's evidence line put the VBlank attachment at `:675-681` and the OpenGL gate at `:295-309`,
while the same document's §"OpenGL platform gate" and §"Timers / animation cadence" said `:306-320`
and `:686-692`. The source settles it: the later two are right, and `:675-681` lands on
`applyScopePersist()`…`applyUiScale()` — unrelated code. `src/PluginEditor.cpp` grew by eleven lines
in `4fcc41c`, which re-aimed some of that document's anchors and not this line's. **Why the gate was
silent, stated because it will be silent again:** it compares an anchor against a base and only
examines anchors whose *target file* moved in the change set. A citation that was wrong before the
base stays wrong and stays green — the limit its own header declares. Correcting it therefore needed
no `DELIBERATE_REAIMS` entry either, for the same reason.

**The state suite is 911 checks, not 900.** Measured, not inferred: `AnamorphStateTests` prints
`911 checks, 0 failure(s)` and `AnamorphTests` prints `162 checks`, which is what `HANDOVER.md`
already carried for the DSP side. Corrected in `HANDOVER.md` and `RELEASE_HARDENING_PLAN.md`, the
two status-of-record documents. **Deliberately not touched:** `HANDOVER.md`'s ABI sentence, which
names `GLIBC_2.38 / GLIBCXX_3.4.31` without `CXXABI`. It sits under a dated "validation surface added
2026-08-18" heading and `CXXABI` was gated on 2026-08-21, so adding it there would falsify a record
rather than repair one.

**Three more `12c545d` residuals, all of the same shape.** `TESTING.md`'s job table told a developer
how to reproduce `linux-clang`, a job that no longer exists, and had no row for `fuzz`, which does;
its count of six was right, its membership was not. `TESTING.md`'s "CI builds all three" followed a
four-row table whose three *targets* include `AnamorphDspDump`, which no CI job builds — the claim is
true only of the benchmark, the fuzz target and the compile-only effects check, and it now names
them. `REPOSITORY_MAP.md` said "seven non-packaging jobs" and listed six, omitting `merge-check`.
`CI_CD.md` named the `linux`/`merge-check` cache lineage `ccache-ubuntu-gcc-release-` two sentences
before saying those jobs key on the pinned Clang major, and listed `build-clang` among the per-job
build directories.

**`PERFORMANCE_BUDGET.md` contradicted itself inside one section.** §"How to produce those numbers"
opened with "no repeatable benchmark is committed to this repository: `scripts/` has no bench entry
point, `tests/` measures correctness only", and twenty lines later said "**The harness now exists:
`tests/bench.cpp`**" and that the reason the TODO rows stay open "has changed". The opening is now
written as the past it describes, with a forward pointer to the current reason. The document's
header — "no benchmark/profiling *data* exists in the repository" — is left as written: it is still
true, and it is the sentence the C2 constraint rests on.

**Five spent re-aim declarations retired, and none added.** All five entries from the Clang
release-toolchain round reported "not needed against `origin/main`" once PR #122 merged, and
`origin/main` is this branch's merge base — the condition that note attaches to. A declaration is
good for exactly one transition, so keeping one past its transition converts a one-shot exemption
into a permanent hole. `DELIBERATE_REAIMS` is back at its documented empty resting state, and the
comment there records why this round added none despite re-aiming two spans.

**Line-number consequence, handled by the tool rather than by hand.** Restoring 54 comment lines
above `sanitizers:` moved every `build.yml` anchor below it. `check-citations.py --fix` re-anchored
six citations across `DOCUMENTATION_COVERAGE.md`, `KNOWN_ISSUES.md`, `COMPATIBILITY_MATRIX.md` and
`RELEASE_POLICY.md`, reporting "0 need a human"; each new target was then read back and confirmed to
land on the content its document names.

**Linux release toolchain: Clang ships, GCC verifies (2026-08-21): the compiler that builds the
Linux artifact stopped being the runner image's choice, and the ABI gate gained a family it had been
blind to. ADR-0030 records the decision.**

**What moved.** The `linux` job named no compiler at all, so the shipped bytes were whatever `g++`
`ubuntu-latest` supplied. They are now the pinned `clang-22`'s, linked by the matching `lld` that
`setup-llvm-apt.sh` installs beside it — not a preference but what the `-flto` archive link requires,
and the reason `CMakeLists.txt`'s lld block now governs the shipped link rather than a side job's.
`merge-check` follows the same toolchain because it is the only build on the same-repo PR path.
`linux-clang` — a second Release build with the same compiler — folded into `linux`; its two genuinely
own steps (the portability canary and the first-party warning gate) moved with it, so **no check was
lost**, and the gate runs after the artifact upload for the reason the ABI gate does.

**GCC stayed, with a deliberately weaker pin than anything else here.** It is now the compatibility
compiler and ships nothing, so `linux-lto-tests` tracks `gcc:16` — the floating major tag. A checker
wants the newest stable 16.x automatically; a shipping toolchain must not. An image rather than `apt`
for a concrete availability reason: no package source ships a *released* GCC 16 for any
runner-available Ubuntu — noble stops at `g++-14`, and both `ubuntu-toolchain-r/test` and Ubuntu
26.04 carry only pre-16.1 trunk snapshots.

**The finding that outlived its own step.** GCC 16 was pinned for the shipping build first, as an
intermediate state inside this same unmerged change set. That step surfaced that
`check-linux-abi.py` gated `GLIBC` and `GLIBCXX` only while the GCC 16 artifact's exception path
pulled `__cxa_call_terminate@CXXABI_1.3.15` — a silent floor rise in an undeclared family. `CXXABI`
is now gated permanently, at **GCC 13's `1.3.14`** rather than GCC 14's `1.3.15`, so the three
families describe one runtime between them; the Clang artifact needs `1.3.9` and **the supported
floor is unchanged at Ubuntu 23.10 / Debian 13**.

**Documents touched, and why each.** `CI_CD.md` (the pin-mechanism paragraph argued a supply
direction that no longer applies, the job table listed `linux-clang`, and the ABI section stated the
interim Ubuntu 24.04 floor), `DEPENDENCY_POLICY.md` (both compiler rows, plus the ADR-0028 scope
paragraph — its "ships nothing" reasoning was correct when written and is kept as it stood, with the
change of scope stated after it), `COMPATIBILITY_MATRIX.md`, `BUILD.md`, `REPOSITORY_MAP.md`,
`TESTING_POLICY.md`, `HANDOVER.md`, `CHANGELOG.md` (the 0.9.4 entry would otherwise have shipped both
a wrong compiler and a wrong system requirement) and `ADR_INDEX.md`. Four workflow line citations in
this file were re-derived and each verified to land on the invocation it names, rather than trusted
because the gate stayed quiet.

**Not rewritten:** the worklogs, `KNOWN_ISSUES.md`, the ccache timing table's `linux-clang` row and
the ADR bodies recording `gcc 13.3.0` as a past measurement environment. Those are historical
records; the migration does not make them false, and the timing row is labelled with the job's fate
rather than deleted.

**Review follow-up, same change set (2026-08-21).** Three findings, all about the containerised GCC
job rather than the release path:

  * **The dependency list is now profiled instead of assumed.** `linux-lto-tests` runs inside a
    Debian-based toolchain image but still called `setup-linux.sh`, whose list is written for a
    fresh Ubuntu machine. `setup-linux.sh` now takes `full` (the default, unchanged for developers
    and for every other job) or `headless`, and the container job asks for `headless`. The lists stay
    in that one script — the workflow says *which* profile, never *what is in it*. Measured in the
    real `gcc:16` image: the headless profile builds both LTO test targets and both suites pass
    (162 + 911, 0 failures), with `build-essential`, `xvfb` and `lld` confirmed absent.
    `build-essential` is the one that mattered — it would have installed a distribution GCC over the
    pinned compiler the container exists to provide.
  * **`python3` is named rather than inherited.** Every checker in `scripts/` is Python and the
    Ubuntu images preinstall it, so nothing ever had to ask. The `gcc:16` image carries it only
    through `python3-minimal`, pulled in by an unrelated layer — true today (measured: Python
    3.13.5) and not a promise. Both profiles now install it explicitly.
  * **The ABI floor parseability guard covers every declared family.** It named `GLIBC_FLOOR` and
    `GLIBCXX_FLOOR` literally, so the `CXXABI` family added earlier in this change set fell outside
    the only check that floor VALUES are versions at all. It now iterates `FLOORS`, so the next
    family is covered on arrival, and it runs FIRST and returns on failure — an unparseable floor
    used to raise out of an unrelated later case and report as a traceback instead of as the one
    thing actually wrong. Verified by seeding a bad value into each of the three families in turn:
    each produces its own named failure. Self-test 17 → **19 cases**.

No floor value, cache key, compiler pin or release step changed.

**Linux installer migration and changelog-completeness audit (2026-08-20): the sibling product's
`install.sh` / `uninstall.sh` hardening brought into this repository, and the whole post-0.9.3
change set re-read for user-visible items the CHANGELOG never received. No ADR: nothing is decided
or reopened — the two-mode installer is this product's own design (0.9.3), and what moved is a
strict superset of its behaviour with the defaults untouched.**

**What moved, and why each half is user-visible.** Three groups, all Linux-only:

  * **Explicit mode selection.** `--user` / `--system` on both scripts, `--discard-parked` on the
    uninstaller, `-h` on both. The prompt is gated on `[ -t 0 ]`, so before this a piped or
    provisioned run took the per-user default with no way to say otherwise, and the only
    non-interactive route to a system-wide install was handing the WHOLE script to root — a
    different transaction from answering `2`, which elevates one operation at a time through
    `priv`. A contradictory pair is refused rather than resolved (they differ in destination *and*
    in privilege), an unrecognised option stops the run, `--user` under root is refused because
    `$HOME` under `sudo` depends on the sudoers configuration, and a repeated flag is not a
    conflict.
  * **The staging directory is now trusted or refused, never assumed.** A candidate is adopted only
    when it is not a symlink, is owned by the identity whose writes land in the destination, and is
    writable by nobody else; one this run creates is `chmod 700`ed explicitly so the next run can
    adopt what this one made whatever the umask was. `choose_stage_dir` now RETURNS non-zero
    instead of printing a candidate it has just judged unusable, and both call sites check it — a
    fresh install is refused too, because staging is where the payload is assembled before the
    rename that publishes it. `reconcile`'s tests are elevated (`$SUDO test -d`), which is what
    lets the system-wide path — running as the invoking user — see the parked bundle inside a
    root-owned 0700 directory at all.
  * **A parked previous version is kept by an uninstall.** An install stopped inside the two-rename
    window parks the working plug-in as `Anamorph.vst3.prev`; `install.sh` is the only thing that
    puts it back. The uninstaller used to sweep it as scratch. It now keeps it, names it on
    **stdout**, and prints the MODE-CORRECT restore command — a plain `./install.sh` defaults to
    per-user and would never look in a system-wide staging directory. `--discard-parked` is the
    opt-in that deletes it.

**What deliberately did NOT move: the sibling's commentary.** Those files carry long
incident-narrative comments — why `/var/tmp` was removed, what a wrong `-e` cost, which run
reproduced which failure. They are correct and they are not for this audience: `install.sh` and
`uninstall.sh` ship to USERS inside the Linux zip, and this repository's copies are written for the
person running them. The reasoning that has to survive lives in `docs/procedures/PACKAGING.md` and
in this file, which is where a maintainer looks; the scripts keep their operator-facing voice. This
is a deliberate divergence from the sibling, recorded here so a later reader does not "restore" it.

**The lint coupling this forced, and the note that predicted it.** `scripts/check-portability.py`
holds installer and uninstaller to the same scratch-name set. Its `SCRATCH_NAME` regex omitted
`\.probe` on an explicit premise — *this* repository's uninstaller removed every staging directory
with one unconditional `rm -rf`, so nothing inside one could outlive it — and the comment ended:
"IF this repository's uninstaller ever gains a path that preserves a staging directory, add
`|\.probe\b` back in the SAME change." `--discard-parked` IS that path: the keep branch leaves the
directory standing, so the `.probe` hard link is now the one scratch file that can survive an
uninstall, and it is swept there by name. The regex gains `|\.probe\b` in this change, as
instructed; `.probe` does not match inside `.anamorph-probe` (the preceding character is a hyphen),
and the gate now reports the four names agreeing rather than three. The 120-case self-test is
unchanged and passes.

**Verified by running the scripts, not by reading them** (2026-08-20, stubbed payload, unprivileged
account for the per-user paths and root for the system-wide one; recorded in
`docs/procedures/TESTING.md`): `--help`; `--user`/`--system` non-interactively; both flags together
and an unrecognised option each exiting 1; a repeated flag accepted; `--user` under root refused by
both scripts; install → upgrade → uninstall round trips with **zero residue** each time; `TERM`
inside the copy and inside each of the two rename windows leaving either the previous plug-in or
the new one in place, never neither; a symlinked plus a foreign-owned candidate stopping the run
with both paths named and NOTHING installed; a group-writable candidate refused, left untouched at
its own mode, and fallen through to the second candidate; a parked copy restored by the next
install, REPORTED rather than skipped when its directory is no longer usable, kept by a plain
uninstall and removed only by `--discard-parked` (which also takes the `.probe`); and a system-wide
install as root naming the per-user copy it coexists with, resolved through `SUDO_USER`. One
honest limit: `INT` was delivered as `TERM`, because a job backgrounded by a non-interactive shell
inherits `SIGINT` **ignored** — a property of the harness, not of the script, whose three signal
traps are the same line.

**The changelog-completeness half, and its one finding.** The audit ran over `a741aa9..HEAD` — the
whole post-0.9.3 range, 95 non-merge commits — reading the diff of the user-visible surface
(`src/`, `packaging/`, `CMakeLists.txt`, the release workflow) rather than the commit subjects. Most
of that range is infrastructure that CHANGELOG_POLICY rule 3 excludes by construction: the citation
and portability lints, ADR-0029's realtime enforcement, the Clang pin, the fuzz/bench/LSan lanes,
the action SHA pinning. Three classes were checked and cleared rather than assumed:

  * The four `-Wimplicit-int-float-conversion` fixes touch `VelvetNoise.cpp`, `LookAndFeel.cpp` and
    the engine, and are **explicit casts of the same values** — no behaviour to report.
  * `-Wl,-object_path_lto` and the dSYM work change where a temporary is written; the shipped bytes
    are untouched and no release asset was added (`release.yml` is unchanged across the range).
  * No macOS deployment target and no LTO setting changed — `juce_recommended_lto_flags` predates
    this range.

**One entry was genuinely missing: the Linux runtime ABI floor.** The shipped Linux binaries
require **GLIBC_2.38 and GLIBCXX_3.4.31** — Ubuntu 23.10 / Debian 13 / GCC 13 — and therefore do
**not load on Ubuntu 22.04 LTS** (glibc 2.35). That is a system requirement a Linux user must know
before downloading, it was measured and gated during this version
(`scripts/check-linux-abi.py`, KI-023, COMPATIBILITY_MATRIX §"Linux runtime ABI floor"), and the
CHANGELOG said nothing about it. Added under `[0.9.4] Changed`, naming the floor, the distributions
it excludes, and that lowering it has not been decided. Re-measured here against the local Release
build before the entry was written: `GLIBC_2.38, GLIBCXX_3.4.31`, matching the declared constant
exactly rather than being copied from the commit message.

**Documentation synced with the code, per the lifecycle policy.**
`docs/procedures/PACKAGING.md` had three statements the migration falsified — "Two invocations skip
the prompt" (now three), "before the probe and before the recovery paths that never probe" (the
recovery scan probes now, since a parked bundle is adopted only from a candidate that passes the
same filesystem test as a fresh one) and "an interrupted install leaves nothing that survives a
deliberate uninstall" (now has one deliberate exception) — all three corrected in place, with the
trust rules for the two staging candidates written down beside them. `docs/REPOSITORY_MAP.md` names the new options
on the installer row. `INSTALL.txt`, which ships to users, documents them bilingually in the file's
existing voice, including that `--system` with no terminal needs root or cached `sudo` because there
is nothing to prompt on. `scripts/check-citations.py`'s `DELIBERATE_REAIMS` list is back to the
empty resting state its own preamble describes: all 26 entries reported "not needed against
origin/main" once PR #120 merged, and origin/main is this branch's merge base — the condition that
note attaches to. A declaration is good for exactly one transition, so the self-test count moves
135 → 109 (26 sections-8c/9 cases retire with the entries they checked) while the gate still
verifies 342 anchors. Full `preflight.sh` green afterwards: seven checker self-tests, the
citation gate across all three bases with no drift, the 162-check DSP suite and the 911-check state
suite, plus the real ABI gate against the local build.

**Tooltip source-of-truth round (2026-08-20): the third attempt at this defect, and the first with
a deterministic reproduction. The two earlier attempts are reverted (they are the two entries below
this one, retracted in place rather than deleted). Root cause is in shared JUCE code, not in this
tree's geometry and not in dwell. No ADR: no decision is made or reopened.**

**A user screenshot ended the guessing.** Cursor on the UI Scale combo; the hint box labelled with
the OVERSAMPLING text; and the box sitting in the GAP between the two rows, covering neither
control. That single frame refutes both earlier diagnoses at once — there is no covered control
under the pointer (round one) and no control being crossed (round two). It also came with the
asymmetry that turned out to be the tell: the defect reproduces on UI Scale, Vectorscope Persist and
UI Animations but NOT on Oversampling, the topmost control, where the hint merely blinks and returns
correct.

**The cause is that JUCE's tooltip tick reads TWO DIFFERENT SOURCES OF TRUTH and mixes them.**
`juce_TooltipWindow.cpp:209` takes the component from
`Desktop::getMainMouseSource().getComponentUnderMouse()`, and `:221` derives the TEXT from it.
`:223` takes the POSITION from `getScreenPosition()`, and `:239` places the box there. Those two are
not the same clock:

  * `getScreenPosition()` is **live**: `getRawScreenPosition()` calls
    `MouseInputSource::getCurrentRawMousePosition()`, an OS query, on every call
    (`juce_MouseInputSourceImpl.h:96-101`). `Desktop::getMousePosition()` is that same value
    (`juce_Desktop.cpp:167-173`).
  * `getComponentUnderMouse()` is a **cache** (`juce_MouseInputSourceImpl.h:54-56`), and the only
    thing that writes it is
    `setComponentUnderMouse (findComponentAt (lastPointerState.position, lastPeer))` — the LAST
    EVENT's position against the LAST peer (`:247-270, :292-297`).

**And that write is not driven only by OS events, which is the half that ties the defect to
REPOSITIONING and which the first version of this entry got wrong.** `TooltipWindow::updatePosition`
is `setBounds()` then `setVisible (true)` (`juce_TooltipWindow.cpp:92-96`), and BOTH of those call
`Component::sendFakeMouseMove()` (`juce_Component.cpp:1105` and `:559`) → `triggerFakeMove()` →
`handleAsyncUpdate()` → `setPointerState (lastPointerState, …, forceUpdate = true)`
(`juce_MouseInputSourceImpl.h:453-462`), which re-runs the cache write at `:297`. **So every show,
move or hide of the box re-derives "what is under the mouse" — from the last event's position, never
from the live one.** The correction matters because the earlier wording ("nothing but an event
refreshes the cache") is false as stated, and because it wrongly implied a *frozen* cache: a frozen
cache cannot move the box at all, since `tipChanged` (`:227`) would be false and neither `showTip`
gate (`:248`, `:254-256`) would open. The observed transition — box moves AND text changes — requires
a cache **write**, and the reposition is what performs it.

So the reposition rewrites the text's source at a position the pointer has already left, while the
box is placed where the pointer actually is. The box lands where you are and is labelled with where
you were; and because that rewrite is driven by the tooltip's own geometry rather than by the mouse,
nothing corrects it until the pointer moves again — one more pixel delivers an event and the correct
hint returns. Persistence and one-pixel recovery both fall out of it, and none of it is
platform-specific: it is shared JUCE code.

**Instrumented, not inferred.** A temporary trace inside the pinned `juce_TooltipWindow.cpp` and
`detail/juce_MouseInputSourceImpl.h` (diagnosis only; both files are byte-identical to the pin in
this change set) logged every mouse event with its peer, its peer-local position, the screen
position `localToGlobal` produced, and the live OS pointer beside it. It caught the divergence
directly: `[evt] local=(470,184) -> screen=(960,540) rawOS=(1426,1072)` — an event resolving the
cache at one point while the pointer was 500 px away — followed by
`[under] <null> -> Backdrop usingPos=(960,540) rawOS=(1426,1072)`.

**Then reproduced deterministically, which is what the two earlier rounds never managed.** Drive the
two sources apart on purpose: deliver a real mouse event over control A (so the cache says A), then
move the OS pointer onto control B without delivering one. Five pairs across the Settings panel —
Oversampling/UI Scale, UI Scale/Persist, Tooltips/UI Animations, UI Scale/Oversampling,
Persist/Tooltips — **5 of 5 displayed the wrong control's text, persisted while the pointer was
still, and recovered on the next event.** With the fix: **0 of 5.**

**The asymmetry — why the row ABOVE — is a geometric FIT, not a traced proof, and is labelled as
such.** What is established is that the rewrite happens at a position that is not the live pointer.
What is not established is the exact macOS event sequence that makes that position land one row up;
that would need a trace on macOS, which was not available here. What supports it is arithmetic that
accounts for all four observed controls. The box is placed `h + 8` above the cursor
(`AnamorphLookAndFeel::getTooltipBounds`, `src/gui/LookAndFeel.cpp:885-894`), so its top edge is a
**tip-dependent** offset above the pointer, and the Settings rows are at editor-local
`oversampleBox` 274–297, `uiScaleBox` 331–354, `scopePersistK` 387–411, `tooltipsToggle` 423–449,
`animToggle` 455–481 (`src/PluginEditor.cpp:2248-2273`). Taking each control's centre and
subtracting `h + 8` for a two-line tip lands inside **Oversampling** from UI Scale, inside **UI
Scale** from Vectorscope Persist, on or within a pixel or two of **Tooltips** from UI Animations,
and — from Oversampling — on `settingsTitle` (221–241), a plain `juce::Label` that never had
`setTooltip` called, so the tip is EMPTY and JUCE hides and re-shows it with the correct text. That
is precisely the exemption the reporter observed. Note the row pitch is **not** constant (57, 56,
36, 32), so no single fixed displacement fits all four; a `h + 8` displacement does, because `h`
varies with the tip. Suggestive, and consistent with everything observed — but a fit.

**The fix validates the cache against the live pointer before trusting it.** `TooltipSource::choose`
takes the cached component, the live pointer position — the same value JUCE is about to place the
box at — and what is really under that position; it keeps the cached component when the pointer is
inside it, and otherwise uses the live one. The editor supplies the live hit test as a lambda, next
to the existing `tooltipsEnabled` gate. Whichever component wins, **the base class still answers for
it**, so every suppression JUCE applies — a button down, a modal block, a backgrounded process, a
target that is not a `TooltipClient` — reaches the state machine untouched. Nothing in dwell,
timing, placement or the show/hide path is modified, and in the common case where cache and pointer
agree the tooltip RESULT is identical to before.

**Two things that claim is careful not to overstate.** First, the fix does **not repair** JUCE's
cache — it cannot, `componentUnderMouse` is private — it re-derives the answer at the one place this
editor controls, the tooltip's own `getTipFor`. That is sufficient here rather than merely
sufficient-looking, because the tooltip is the **only** consumer of `getComponentUnderMouse()` in
this tree (`grep -rn getComponentUnderMouse src/` finds nothing but this comment): the editor's own
hover is geometric, derived from `getMouseXYRelative()`, and never reads the cache. A future
consumer of that cache would inherit the hazard, and this note is where it should be looked up.
Second, the live hit test is passed as an argument and therefore **evaluated on every tick**, not
only on disagreement; `choose` ignores it when the cache agrees, so the answer is unchanged, but the
walk does run — one coordinate transform and one tree descent per 123 ms tick, on the message thread.
Keeping it a plain argument is what keeps `choose` a pure function of three values, which is what
makes it testable with no display; the earlier phrase "bit-for-bit what it was" was true of the
result and not of the work, and is corrected here.

**Four questions the follow-up review asked, answered from the source.** *Why does the original
defect preferentially select controls above?* — the geometric fit above, labelled as a fit. *How does
`choose` prevent that transition?* — the wrong text can only be produced by asking a component that
is not under the pointer; `choose` makes that unaskable, because the only two components it will
ever hand to the base class are one that geometrically contains the live pointer and the one a live
hit test at that same point returns. *Is it preventing an incorrect selection or masking a stale
state?* — preventing the selection; it does not and cannot repair the cache, and the distinction is
recorded above along with the fact that the tooltip is the only consumer of that cache here. *Are
there remaining cases where the text can detach from the component that should own it?* — four, all
narrow and all deliberate:

  1. **A stale cache that still geometrically contains the live pointer is kept.** `choose` tests
     containment, not topmost-ness, so a cache pointing at a container whose bounds still contain
     the pointer is not corrected. In practice JUCE caches the deepest `findComponentAt` leaf, so
     this needs the pointer to have moved from a container onto a smaller child-on-top with no
     intervening event. Narrower than the defect fixed, and not a regression against the old
     behaviour, which took the cache unconditionally.
  2. **The live hit test only searches THIS editor** (`src/PluginEditor.cpp`, the `componentAt`
     lambda). A live pointer over another window resolves to `nullptr`, i.e. no tip — correct, since
     this tooltip belongs to this editor, but worth knowing it is a deliberate boundary rather than
     an oversight.
  3. **A control the box is drawn on top of still owns the tip when the pointer is genuinely on it.**
     That is the separate geometric case the two reverted rounds chased; it is unchanged here and
     out of scope by design.
  4. **If `componentAt` were ever left unassigned**, `choose` would degrade to "no tip when the cache
     is stale" rather than to the wrong tip — the safe direction, and one the headless suite pins
     ("a stale cache with nothing under the pointer yields no tooltip at all"). It is assigned
     unconditionally in the editor's constructor beside `tooltipsEnabled`, and the behavioural
     harness could not reach 0/5 without it being live, so the wiring is exercised rather than
     merely asserted.

**Regression coverage is headless, because the decision was deliberately kept pure.**
`TooltipSource::choose` takes no JUCE state and needs no display, so `AnamorphStateTests` drives it
over the measured Settings geometry — the reported gesture in both directions, Vectorscope Persist,
the agreeing-cache case that must NOT be overridden, both boundary pixels, all three null
combinations, and the "moving off a control still drops the hint" case that guards against the
regression the previous attempt shipped. 11 checks; suite **900 → 911**. Mutation-tested by reverting
`choose` to "always trust the cache": **7 of the 11 checks fail and the behavioural reproduction goes
back to 5 of 5 wrong.**

**Everything else re-measured on the running editor under `xvfb`:** the KI-024/KI-025 behavioural
matrix **ALL PASS (0 failed)**; the 45-gesture walk matrix **identical to the pre-fix baseline**
(39 clean / 6, unchanged — those are the separate "the box covers a control and you park on it" case,
which this fix deliberately does not touch); the in-control walk **0 wrong**.

**Platform.** The reproduction was confirmed on macOS by the reporter. The cause is in shared JUCE
code — the cache/live split is platform-independent — and the fix is in the shared path, with no
platform conditional. What differs per platform is only how easily the two sources drift apart:
macOS's event coalescing and the tooltip window's own peer teardown produce it readily, and it was
reproducible here on Linux once driven directly. No platform-specific handling was added because no
evidence called for any.

**The two earlier attempts, retracted.** Round one anchored the tip while the cursor was inside the
box (6 → 3 failing gestures; wrong cause). Round two required the pointer to be at rest before a new
tip was adopted (0 failing gestures on that matrix, but it changed dwell — out of bounds — and kept
hints alive whenever the cursor sat in a remembered rectangle, a real regression). Both are reverted
and both entries stay below with their measurements intact.

**Sign-off status: NOT GRANTED for this round.** No maintainer confirmation was given and none is
claimed. What is owed is a Level-5 check of the gesture on macOS by the reporter, since the
behavioural evidence here is Linux-only on a synthetic display; the decision itself is now covered
headlessly on every platform the suite runs on.

**Tooltip investigation (2026-08-20): TWO fixes reverted and NO third one shipped. This entry
exists because a measurement contradicted the report and the right thing to do was to stop, not to
patch a third time.** The tree is back to the behaviour `origin/main` has; nothing user-visible
changes in this change set and `CHANGELOG.md` is untouched.

**What was reverted.** (1) The *anchor-while-inside-the-box* fix — it treated the pointer entering
the box as "the box put a component under the pointer" and held the tip. Measured over 45 scripted
gestures it took the failures from 6 to 3, i.e. it helped but did not close the case. (2) The
*wait-for-the-pointer-to-rest* fix — it took the same 45 gestures to 0, but it changed dwell
behaviour, which is out of bounds, and it kept a tip alive whenever the cursor sat inside a
remembered rectangle, which is the "tooltips sometimes stay visible after leaving the control"
regression that was reported against it. Both reverts are ordinary `git revert` commits; the
reasoning is kept here rather than deleted with the code.

**What was instrumented, and it was JUCE itself rather than this tree.** A temporary trace inside the
pinned `juce_TooltipWindow.cpp` and `detail/juce_MouseInputSourceImpl.h` (diagnosis only — both files
are byte-identical to the pin in this change set) logged, per 123 ms tick: the component under the
pointer, the tip it yields, `tipChanged`, every `updatePosition` with the box's old and new
rectangles, every `mouseEnter` on the box, and every `setPeer` / `setComponentUnderMouse` transition
with the position that produced it. Every `displayTipInternal` additionally logged the component
under the pointer **at that instant** beside the text being displayed.

**The finding that stopped the work: across 122 tooltip displays — 45 scripted gestures over 9
Settings controls, a confined random walk restricted to one control and its own box, and a
two-minute unconstrained walk — the text displayed was NEVER anything but the tooltip of the
component under the pointer at that instant. Zero mismatches.**

> **RETRACTED (2026-08-20, by the round above).** That check was a **tautology on the tree it ran
> against**. Pre-fix, `juce_TooltipWindow.cpp:221` computes the text as
> `getTipFor (*getComponentUnderMouse())` and `:239` displays it in the same tick with no message
> pumping in between, so comparing the displayed text against the tooltip of
> `getComponentUnderMouse()` compares a value with itself and can never fail. It measured nothing.
> The invariant that CAN break is the one between `:221` (the cached component) and `:223` (the live
> position), and that is what the round above measured instead. The 0/122 is left standing here only
> so the mistake is legible; it is not evidence of anything.

The reasoning it was used to support — "`newTip` is `getTipFor(newComp)` computed in the same tick,
and `newComp` is `findComponentAt (livePosition, lastPeer)`, so no branch of it can return a
component the pointer is not over" — carries the same error: `findComponentAt` is given
`lastPointerState.position`, the LAST EVENT's position, not the live one.

**Two transitions were mapped in detail because they were the obvious suspects, and both came out
clean.** Walking 1 px per tick *without ever leaving the UI Scale combo*, into the strip its own box
overlaps: the box is entered, `setPeer` switches to the box's peer, `componentUnderMouse` becomes the
box itself, `mouseEnter` hides the tip, the peer is destroyed, `componentUnderMouse` re-resolves to
the combo's label, and the box is re-placed — measured `614,599` → `375,586`, flipping sides as the
cursor crossed the display centre, and carrying the **correct** text across the whole sequence. The
stale-position re-resolve after the peer is destroyed uses the last pointer state rather than the
live position, which is a real staleness of a pixel at this speed; it was traced and it never
selected a different control.

**So the only mechanism the evidence supports is the one the FIRST investigation named**: the box is
placed 12-16 px from the cursor (`AnamorphLookAndFeel::getTooltipBounds`), so it lands on top of
neighbouring controls — measured, the UI Scale box covers the Oversampling combo and its label — and
aiming at the box therefore puts the pointer over whatever the box covers. Every foreign tip observed
in every run was that control's, with the pointer measurably on it. The reporter states the pointer
is not on the other control and does not cross one, which contradicts the measurement, and no
reproduction matching that description has been produced here.

**What is NOT claimed.** That the reporter is mistaken. The measurements are Linux/X11 on a synthetic
display at one UI scale, driven through `ComponentPeer::handleMouseEvent`; a real window manager,
a HiDPI scale factor, a different UI Scale setting or a host-owned parent window all change the
geometry and the event ordering, and any of them could expose a transition this harness cannot
produce. What is claimed is narrower and firmer: **on the evidence available, no state transition in
the tooltip machine displays text belonging to a component the pointer is not over**, and a third
patch built on a guess about the remaining possibilities would be the third patch built on a guess.

**Sign-off status: NOT GRANTED for this round, and nothing is owed** — no behaviour changed.

**Stale-anchor correction (2026-08-20): one number, in this file's own About-link entry.** The
About-link round re-aimed the three `DELIBERATE_REAIMS` declarations onto the header line where
`aboutLink` is declared and updated the prose around them, but the sentence describing what
`verify_reaim_targets` resolves still named the old `:362`. It now names the line the declarations
carry, which is the line `aboutLink` occupies in the current header — read to confirm, not shifted.
(That number has moved again since, with the header; it is `:465` at the time of writing, and the
declarations and the three legal documents move with it.) One character; no declaration, no source file and no other document touched.
**MAINTAINER SIGN-OFF RECORDED HERE, granted 2026-08-20**, for that single correction and nothing
else. No platform, pluginval or Level-5 validation was performed for it and none is claimed.

**Animation-landing round (2026-08-20): one line deleted from `stepVal`, one handover
self-contradiction, one mis-aimed post-mortem anchor. Three review corrections and nothing else. No
ADR: no decision is made or reopened, and the KI-025 requirement the round below rests on is a
constraint the fix had to satisfy rather than something to trade against it.**

**The defect is that the round below left TWO landing tests in `stepVal` where one was needed, and
the one it ADDED measured the wrong quantity.** The test that actually lands the ease —
`|next - target| < 0.004f` — predates KI-025 and measures DISTANCE. The line added beside it measured
the STEP: `|next - curr| < 0.0015f`. A step is `distance × rate` and `rate` is `1 - exp(-dt/tau)`, so
a step shrinks with the FRAME as well as with the remaining distance. The step test therefore fired
at a distance of `0.0015 / rate` — 0.0143 at 60 Hz, 0.0278 at 120 Hz, and the *whole* remaining
distance once `rate` reaches 0. `rate` is 0 when `dt` is 0, and `dt == 0` is reachable rather than
hypothetical: the vblank value is clamped `jlimit (0.0, 0.05, t - lastFrameTime)`, which bounds it at
the TOP only. On such a frame every animated property of every widget was snapped in one pass, so a
hover glow, a press glow or a switch slide that had barely begun jumped to its end state.

**The fix is the deletion of that one line, and what makes it sufficient is a property of the
surviving test, not an assumption about frame times.** `|next - target| = |curr - target| × (1 - rate)`,
so a shorter frame drives `(1 - rate)` toward 1 and makes the distance test STRICTER, never looser —
the failure mode is structurally unreachable in that form. The jump it applies is under 0.004 of the
0..1 range by construction, from at most 0.0122 away (the 50 ms clamp ceiling at the fastest tau,
`0.004 / (1 - rate)`; exactly 0.004 at `dt == 0`). The comment above `stepVal` now carries that
derivation, because the reason the surviving test is safe is the reason the deleted one was not.

**Simulated at the numbers rather than argued, on all four rates (`rIn` 0.075, `rOut` 0.150,
`rAct` 0.045, `rOn` 0.055).** The step form made the fade **frame-rate dependent** — 0.67 s on a
60 Hz display against 0.44 s at 240 Hz — so it was never merely a `dt == 0` edge case; it was
shortening every fade on every fast display. The pre-KI-025 form (`return false` when the step is
small) leaves the residue KI-025 exists to remove, and that residue GROWS with refresh rate: 0.054 at
240 Hz, 0.449 at 2000 Hz. Deletion gives 0.829–0.833 s at every rate from 30 to 2000 fps and exact
convergence — 25 writes on `rIn`, 50 on `rOut`, 15 on `rAct`, 19 on `rOn` (`rAct` and `rOn` are
byte-identical before and after, because on those taus the 0.004 test always fired first). A `dt == 0`
frame mid-fade now leaves the value unchanged and still WRITES, which is what keeps `microSettled`
false while a transition is in flight.

**The write on a zero-length frame is deliberate and must stay, which is the one thing a future
"optimisation" here would break.** A `next == curr -> return false` write-skip is the obvious way to
suppress it, and it is wrong: returning false clears `changed`, hence `anyMotion`, hence
`microSettled`, and `anyLit` deliberately does not count `onA` — so a single zero-length frame during
a host-automated toggle slide would seal the idle gate on a half-slid switch and freeze it there. The
prohibition and its reason are written above `stepVal`.

**Mutation-tested in both directions.** Removing the surviving landing test entirely: the value is
still 5.6e-45 after 50 000 000 frames — 833 333 s of animation at 60 Hz — so `microLit` never clears
and the idle gate NEVER seals, which is precisely the S11/H15 regression KI-025's second half exists
to prevent. Restoring the step test reproduces the snap at `dt == 0`. The two tests are not
interchangeable and only one of them belongs here.

**No CHANGELOG entry.** The step test was introduced and removed inside the same unreleased 0.9.4
change set, so nothing user-visible ever shipped with it, and rule 2 (no invented history) is the
governing one. The existing 0.9.4 wording — "the fade is made to land on zero instead of approaching
it forever" — describes the surviving test and stays true as written.

**The handover snapshot said both things.** The 0.9.4 operational-status row was edited to record
KI-024 and KI-025 as closed, while the later half of the same table cell still read that they were
"deliberately left open". Both halves were once true — the first hover round measured them and filed
them, the round after it closed them — so the correction preserves that history rather than deleting
it: the clause now reads that the round "measured two adjacent defects and filed rather than folded
them in … and **the round after it closed both**, as described above; neither is open." `KNOWN_ISSUES.md`
and `procedures/TESTING.md` were already consistent; only that row carried both claims.

**Two anchors were re-aimed after reading the code they name, not by shifting numbers.** In
`POSTMORTEMS.md` the geometric-hover incident cited the combo `hov` block as a bare `:1346-1348`,
which today is the pop-up housekeeping calls; it is now spelled path-qualified against the block the
sentence describes. The bare form is exactly why the gate had not caught it — a citation is only
matched when it names its path from the repository root — so re-spelling it puts the anchor UNDER the
gate for the first time. That takes the document's `src/PluginEditor.cpp` group from 2 to 3 citations,
which makes the group non-comparable against `origin/main` for this run (the tool says so on stderr)
rather than requiring a `DELIBERATE_REAIMS` entry: a declaration is needed only when a same-count
group makes a correction read as drift and `--fix` reverts it. That was the case for `PRIVACY.md`'s
`createDirectory` anchor and its entry is still required; it was re-derived against the current tree
this round, and `verify_reaim_targets` resolves it live every run. The overlay comment's own anchors
for `withTrimmedTop (46)` and for the `microSettled` motion latch were likewise re-read and corrected;
those live in a `.cpp` comment, are not path-qualified, and so are the class of anchor no tool checks —
they have to be re-read by hand every time the file moves, which is the standing cost of the bare form.

**This round's own edits moved the file again, and that was caught by the gate rather than by
memory.** Correcting a wrong numeric bound in the new comment added one line above every anchor below
it; `--check` reported 5 drifted citations, `--fix` re-anchored them, and the two bare in-comment
anchors plus the declared `PRIVACY.md` re-aim were corrected by hand. Final state: **338 anchors,
`--check` rc=0.**

**Nine informational findings were left unchanged, and none of them is a defect in the current tree.**
Three are statements about future layouts or future pop-up kinds — that `cursorOverlay()` would treat a
newly-added visible intercepting sibling drawn over a control as an overlay, that submenus and
non-modal pop-ups are not handled, that the predicate is "any intercepting child" rather than "a panel"
— and the review's own layout audit confirms no currently-reachable configuration has two visible
intercepting siblings under one point. Narrowing the predicate to `Backdrop` or to full-editor bounds
would be the future-proofing the round was told not to add, and would cost the generality KI-024 was
required to have. Two are consistency notes with no behavioural difference today: `isVisible()` versus
`isShowing()` in `cursorOverlay()` (for a direct child these differ only when the editor is not
showing, and both call sites are already inert in that state) and the parented preset menu satisfying
both the pop-up and the overlay predicate (the two terms are ANDed identically, so the answer is the
same). One is the per-frame child scan cost, already measured at 0 idle passes per second with the
pointer outside. One is the event-driven hover paths (`ABControl::hovered`, `SpectrumImager`), which a
plain intercepting child resolves through real `mouseExit` delivery — unlike the modal menu case,
which is why that reasoning had to be written down separately. One is the extra landing-frame write
and repaint, which is the bounded, intended cost of reaching the target: one frame per transition per
property, and the alternative is the write-skip rejected above. The last is a NaN hypothetical —
`exactlyEqual` would be false, both threshold tests false, and the gate would never seal again — but
`hovT`/`actT` are literal 0/1 and the rates are finite over the clamped `dt`, so nothing in the tree
can produce it. It is a note about a pre-existing shape, not a regression this round introduced.

**Validation actually run this round.** Full `preflight.sh` — the documentation and source lints, the
citation gate against all three bases, the **162-check** DSP suite and the **900-check** state suite —
**rc=0**. The behavioural matrix on the running editor under `xvfb`, relinked against the objects that
run had just built: **ALL PASS (0 failed)** — normal hover on and off; KI-025's one-sample exit,
gradual exit and hidden-editor exit all reaching 0.000; all three overlays checked three ways each;
the Bypass dim still leaving hover live; the 0.9.4 drop-down occlusion still holding. The idle counter
re-measured on a freshly instrumented build of the current source: **0 passes/s** with the pointer
outside, 102.6/s resting on a control, **0 passes/s** immediately after a hover — the previous round's
numbers exactly, so the longer landing (0.83 s against 0.67 s at 60 Hz) still finishes well inside the
harness's settle window and the S11/H15 saving is untouched. **No platform matrix, no pluginval run
and no Level-5 audition were performed for this round, and none is claimed** — the automated evidence
here is Linux-only on a synthetic display, exactly as the round below records.

**MAINTAINER SIGN-OFF RECORDED HERE, granted 2026-08-20**, scoped to the three review corrections in
this round and to nothing else: (1) removing the step-based landing test from `stepVal` and keeping
the distance-based one, (2) the `HANDOVER.md` status-row contradiction, and (3) the re-aimed
`POSTMORTEMS.md` / in-comment / `PRIVACY.md` citations with the declaration handling above. It does
**not** cover any per-platform or Level-5 validation, and it does **not** discharge the Level-5 item
the round below still owes — that entry's "NOT GRANTED" status is unchanged and is deliberately left
standing.

**Overlay-occlusion and idle-latch round (2026-08-19): KI-024 and KI-025 closed. One shared cause
with two different second halves, both fixed on evidence and both mutation-tested. No ADR: no
decision is made or reopened.**

**KI-024's root cause is that "occluded" had been answered at the wrong granularity.** The 0.9.4
pop-up term is one bool per frame, and that is right for a pop-up: it is a window over the WHOLE
editor, so if the cursor is inside it nothing this editor draws is under the cursor. An overlay is
not that shape. `Backdrop` is an editor CHILD that covers the editor *except its own contents* — the
Settings panel's combos and switches must keep hovering while a knob behind the dim must not — so a
single per-frame answer cannot express it. `cursorOverlay()` finds the overlay once per pass and
`occludes (overlay, c)` finishes the question per widget, which is the granularity the case actually
has.

**What counts as an overlay is DERIVED, not listed, which is what makes it general.** Three
properties, each load-bearing and each checked against the tree rather than assumed: a visible editor
CHILD (an overlay is drawn by this editor, which is what makes "except its own contents"
expressible); scanned front-to-back so the topmost wins when two are up; and it TAKES THE POINTER
(`getInterceptsMouseClicks`). A panel added later is covered with no edit here. The interception test
is the discriminator, and it is why `dimOverlay` is correctly not an overlay: the Bypass dim paints
over the editor at 40 % but is `setInterceptsMouseClicks (false, false)`, so every control under it
stays live and stays the thing the pointer is on — suppressing its hover would have been a
regression, not a fix. It is also the only one of these whose bounds are NOT the full editor
(`withTrimmedTop (46)`), which is exactly why the predicate tests containment instead of assuming the
geometries match. `popupShield` is skipped, and that exclusion is why this is not simply "anything
that eats the click": the shield paints nothing, and treating it as an overlay would darken the whole
editor whenever any menu is open — the coarse behaviour the 0.9.4 round measured and rejected.

**KI-025's root cause is that the idle gate's "nothing can move" test was only a MOTION latch.**
`microSettled` is `! anyMotion` — true whenever the previous pass wrote nothing — so it reads
identically with a control's `hovA` at 0.0 and at 1.0. The gate's own comment asserted "cursor
outside the editor (all hover targets 0)", and the word doing the damage is *targets*: the target is
0, but the VALUE only reaches 0 by being stepped, and the gate skipped the pass that would step it.
A pointer that left the editor inside one sample therefore sealed the gate on a control at full
brightness. `microLit` is the missing half: with the cursor outside and no button held, `hovA` and
`actA` both rest at 0, so a non-zero one proves the editor is not yet in the state the gate is about
to assume. `onA` is deliberately excluded — a switch that is ON rests at 1, and counting that as
"lit" would hold the gate open on any preset with a toggle enabled.

**The second half of the KI-025 fix is what makes the first half free, and this was measured rather
than reasoned.** An exponential ease never arrives; `stepVal` stopped writing once the step fell
under its threshold, leaving `hovA` parked around 0.014 and calling itself settled. Harmless while
"settled" only gated repaints — fatal once it also gates the idle seal, because a
settled-but-not-zero value would hold the gate open for ever. So `stepVal` now LANDS on the target
when the step is too small to be worth easing: one final write, then it returns false for good. Only
the last ~1.4 % is snapped, once per transition. Mutation-tested with the landing reverted and
`microLit` kept: the residue returned AND the idle gate stopped sealing at all — **507 passes in 5 s
with the pointer parked outside**, i.e. precisely the 68–87 %-of-idle-profile regression the H15/S11
work exists to prevent. The two halves are one fix.

**The two fixes interact in one place, and it is a benign one.** Opening an overlay changes the hover
answer without the pointer moving — the same shape as a menu opening — so the driver must not be
asleep at that moment. It cannot be: an overlay is opened by a click inside the editor, so
`mouseInside` is true and the gate's first conjunct already forbids sealing; and if the pointer is
outside instead, every `over` is false regardless. KI-025's fix makes that robust rather than
incidental — the gate now also refuses to seal while anything is lit — so no extra un-settle was
added for overlays, and `refreshPopupShield`'s existing one is left exactly as it was.

**Performance: measured, not asserted, on the path the optimisation was written for.** A temporary
counter on the full-pass path, five-second samples, baseline = `origin/main`:

| state | before | after |
|---|---|---|
| pointer parked outside the editor | 0 passes/s | **0 passes/s** |
| pointer resting on a control | 102.8/s | 102.2/s |
| pointer outside again, straight after a hover | 0 passes/s | **0 passes/s** |

The third row is the one that matters: it was already 0 before, because the gate sealed — on a
control that was still glowing. It is still 0, now with the control dark. The optimisation is not
merely preserved, it seals in exactly the same states; the added work is one compare per widget
inside a loop that was already running, plus one child scan per pass while an overlay is open.

**Verified behaviourally across the whole matrix, on the running editor under `xvfb`:** normal hover
on and off; KI-025's one-sample exit, gradual exit and the editor being HIDDEN while a control is lit
(all → 0.000, from 0.990/0.021/0.990); KI-024 for **all three** overlays — Settings, About and Save
Preset — each checked three ways (a control behind is dark, a control INSIDE the overlay still
hovers, and hover returns when the overlay closes); the Bypass dim leaving hover live; and the 0.9.4
drop-down fix still holding. Three mutation runs pin each mechanism: removing `occludes()` fails all
three overlays and nothing else; removing `microLit` fails three KI-025 cases including the plain
"pointer off the editor" one and nothing else; reverting `stepVal`'s landing fails the residue cases
and destroys the idle seal.

**Sign-off status: NOT GRANTED for this round.** No maintainer confirmation was given for it and none
is claimed. What is owed is the same Level-5 check the hover round records — the automated evidence
here is Linux-only on a synthetic display — extended to the three overlays and to the Bypass dim.

**Hover-occlusion round (2026-08-19): a control covered by an open drop-down reported itself
hovered. One predicate, two call sites, and the 0.9.4 heading re-dated to today because this is the
first user-visible change the version has taken since it was written. No ADR: no decision is made or
reopened — the geometric hover design stays exactly as it is and gains the one term it was missing.**

**The root cause is that geometry cannot express occlusion, and this editor's hover is geometry on
purpose.** Since v0.6.1 hover has been derived from the pointer's position rather than from
mouseEnter/mouseExit, because those events fired unreliably and left highlights stuck on. That is
still the right design and is untouched. But `Component::getMouseXYRelative()` is
`getLocalPoint (nullptr, Desktop::getMousePositionFloat())` (`juce_Component.cpp:3233-3236`) — a pure
coordinate transform with no hit test in it — so a control covered by a drop-down keeps containing
the pointer exactly as before and lights while the pointer is provably on the menu.
`cursorIsOverOpenPopup()` supplies the missing term, and it is geometry too: the pointer measured
against the pop-up instead of against the control. Three mechanisms were considered and rejected with
reasons recorded in the code — `isMouseOver`/`componentUnderMouse` (the enter/exit machinery v0.6.1
moved away from, and a frozen cached flag off the message thread), `reallyContains` (a per-platform
z-order syscall, 44 per vblank on the path the idle gate exists to keep quiet), and the process-global
modal stack (it would make this editor's hover a function of another instance's menu, or of the
host's dialogs — and per **KI-020** the sibling's control is genuinely clickable, so dimming it would
be a *false negative* affordance, the worse direction).

**The placement is the load-bearing part, and it is now written down where the next author will
look.** Occlusion is ANDed into the ANSWER (`over`, `hov`) and never into the GATE (`mouseInside`,
`comboCursorInside`). Folding it into the gate reads tidier and is wrong: `! mouseInside` is the
shared prefix of both idle-gate early returns, so a gate that goes false the moment a menu opens
seals the driver with `hovA` parked at 1.0 — converting a false highlight into a frozen one for the
life of the menu. The rule and the reason are on `PopupShield` in `src/PluginEditor.h`, whose
existing comment argued that raising the shield "cannot disturb hover" and treated that as a feature;
that argument was true about enter/exit churn and silent about occlusion, and it now carries both
halves. Its two code anchors had also rotted (`1331-1333`, `:1072-1073`) and are corrected.

**Verified by measurement on the running editor, not by argument.** A throw-away harness — not
committed; see the disclosure below — linked the real processor, instantiated the editor into a
window on an `xvfb` display, warped the real pointer with `Desktop::setMousePosition`, and read the
eased `"hovA"` property the LookAndFeel actually paints from. Same geometry, same probe point, three
runs each side: with the pointer at the centre of an open combo list (`702,279 125×114`) the `Knob`
underneath (`705,307 122×131`) read **0.990 → 0.000**. The combo that owns the list read ~0.02 both
ways — it is not covered, because `getOptionsForComboBoxPopupMenu` deliberately opens the list flush
*below* the box (`LookAndFeel.cpp:421-434`), so the pointer really is still on the box and its
highlight is correct. Dismissing with the pointer unmoved returned the knob to **0.990**, so nothing
is left stuck dark.

**Both branches of the predicate were mutation-tested, and one of them changed what the comment says.**
The preset menu never reaches the look-and-feel hook (its look-and-feel is null at construction), so
it is found by the modal-child scan `dismissTrackedPopupMenus` already uses. With only factory
presets that menu lands on the scope and covers no hover widget at all — the branch looked dead. With
22 saved presets it is 690 px tall and covers the A/B control: branch disabled **0.990**, restored
**0.000**. Load-bearing, and reachable by any user with a preset library. The un-settle in
`refreshPopupShield` was mutation-tested the same way (**1.000 → 0.022** with it, **0.990 → 0.990**
without) — but its first draft justified itself with a flick scenario that does not hold, since the
gate only seals when the pointer is outside the editor and `over` is false there with or without
occlusion. The comment now says what the mutation actually shows: it drains a **pre-existing** seal
at the two moments a pop-up changes the hover picture, and does not fix that seal in general.

**Two adjacent defects were found by the same measurement and deliberately left open**, each filed
rather than folded in. **KI-024**: the Settings / About / Save Preset overlays occlude identically
(measured **0.990** behind an open Settings panel) but need a *per-widget* test rather than this
fix's per-frame one, because a backdrop covers everything except its own children — three overlays
with their own child sets, plus the Persist-drag `reveal` mode. **KI-025**: the idle gate is a motion
latch, so a pointer that leaves the editor inside one frame seals it on a still-lit control
(measured **0.990**, persistent); pre-dates this change, unaffected by it, and fixing it means
editing the sealing condition of the path that removed 68–87 % of the idle profile. Filing them is
also why the placement rule above matters: this fix is the one that must not make KI-025 worse, and
by construction it cannot.

**Documentation synced:** `CHANGELOG.md` (`[0.9.4]` re-dated 2026-08-19, one **Fixed** entry
prepended — no new `## ` heading, which `check-docs.py` rejects because the release workflow slices
notes to the next `^## \[`); `README.md` §Project status; `HANDOVER.md`; `docs/procedures/TESTING.md`
§"Gaps in the automated coverage" (the ADR-0025 rule-1 exception, four disclosures written fresh per
§3); `docs/KNOWN_ISSUES.md` (KI-024, KI-025). Seven citation anchors in `EULA.md`, `PRIVACY.md`,
`TRADEMARKS.md`, `KNOWN_ISSUES.md` and `THREAD_MODEL.md` were re-anchored by
`check-citations.py --fix` after the line insertions, and one **untracked** prose reference the gate
cannot see — `KNOWN_ISSUES.md`'s "declared PluginEditor.h:85", which my edit made point at a comment
— was corrected by hand to `:178`. A **review pass** then found two more of the same untracked class
and one pre-existing mis-aim, all three verified against the tree rather than taken on the review's
word: this file's own new helper cited the modal-child test it copies at `:1082-1084` (the
pre-insertion location — corrected to `:1146-1148`, the `for`/`if`/`exitModalState` block in
`dismissTrackedPopupMenus`), and `POSTMORTEMS.md` INC-011 still made the identical geometric-hover
claim as the header against `PluginEditor.cpp:1331-1333`, `:1072-1073` — the pair the header edit
corrected. Both are now `src/`-qualified and therefore visible to the gate from here on; the second
was invisible only because it lacked the `src/` prefix `TRACKED` matching requires, which the same
document already uses for its five other source anchors.

**A second review pass found that sweep was not exhaustive, and it was right.** "Two more of the
same untracked class" was a count of what that pass happened to look at, not a search — three more
sat in `KNOWN_ISSUES.md` alone, and one of them is the worse kind. **KI-009's `focusSaveNameField`
citation was mis-aimed, then mechanically carried.** At the merge base it read
`src/PluginEditor.cpp:1603-1611`, which was the `rIn`/`rOut`/`rAct`/`rOn`/`rPos` easing block in
`stepMicroAnims` — not that function at all — and `--fix` moved it to `:1567-1575`, the same easing
block after this change's insertions. Faithful, and still wrong. `focusSaveNameField` is at
**`:1984-1992`**, and the two untracked references beside it were mis-aimed the same way: the
on-open call is at **`:1953`** (not `:1483`), and KI-017's evidence line cited **`:363-373`** for the
Save-Preset field (not `:326-355`, which is the A/B-control setup) and **`:1942-1992`** for show +
focus. Every one was verified by reading the lines, not by trusting the review's numbers.

**No `DELIBERATE_REAIMS` declaration was required for those five — but the reason first written
here was WRONG, and the correction matters more than the conclusion.** It said the gate "keys on the
anchor's spelling". It does not. Citations are grouped per `(document, tracked path)` and paired
**positionally** (`zip(olds, curs)`), and when the two counts differ the run prints "now has N
citation(s) where the base had M; the added ones are not checkable against that base" and `continue`s
— skipping the content comparison for that whole group. `docs/KNOWN_ISSUES.md` went from two
`src/PluginEditor.cpp` citations to five, so its group is **not compared at all**. Those five
anchors are therefore **unchecked, not blessed**; `--fix` leaving them alone is what "not compared"
looks like, not evidence that the gate approved them. That is inherent — a new anchor cannot be
checked against a base that does not contain it — and it resolves itself once the default branch
carries them, at which point the counts match again and they are protected like any other. Recording
the real mechanism because the wrong one would have been used to justify skipping a declaration that
*was* needed, which is exactly what happened next.

**The shape problem is closed too, without losing the labels.** Two of these anchors were invisible
not for want of the `src/` prefix but because prose sits between the path and the second anchor
(`…:363-373 (the field), :1942-1992 (show + focus)`), which the compound-anchor pattern does not
match. Collapsing them to `path:a, b` would have made both visible at the cost of the per-anchor
labels; repeating the path keeps both. `docs/KNOWN_ISSUES.md` now has **five** gate-visible
`src/PluginEditor.cpp` citations where `origin/main` had two, and **zero** references to a nested
tracked path that the gate cannot see.

**Scope of the sweep, stated so the next reader does not over-read it.** Exhaustive for
`docs/KNOWN_ISSUES.md`, measured. Repository-wide there are **63** further unqualified references to
nested tracked paths across eleven other documents (28 of them in this file, where many are
deliberate — the checker's own header forbids prose *examples* from using a tracked path, and this
ledger quotes old anchor spellings as examples). That is a pre-existing structural condition, not
something these rounds created, and closing it is its own change.

**A third review pass found the same carried-mistake class in `PRIVACY.md`, and this one needed a
declaration.** The row saying the Presets folder is created when the **Load Preset** dialog opens
cited `src/PluginEditor.cpp:1560` at the merge base — the S11 generation pre-gate comment inside
`stepMicroAnims`, about 383 lines short of the `:1838` that `dir.createDirectory()` sat on there.
`--fix` carried it to `:1524`, still the same comment. Corrected to **`:1916`**, and written the way
the checker's own header says new citations should be — with the symbol spelled beside the number
(`dir.createDirectory()` in `showLoadPreset`), because that is the half a reader can check and the
half that survives the next shift.

**Unlike the `KNOWN_ISSUES.md` five, this one is caught by the gate, which is why it is declared.**
`PRIVACY.md` still has exactly one `src/PluginEditor.cpp` citation, so the pair IS compared, the
re-aim reads as drift, and `--fix` **reverted the correction on the first run** — measured, not
predicted. `("PRIVACY.md", "src/PluginEditor.cpp:2074"): "createDirectory"` is therefore added to
`DELIBERATE_REAIMS`. It is not an inert exemption: `verify_reaim_targets` resolves the anchor against
the live file every run, and mutating the substring to a value the code does not contain makes the
run fail with `::error::` and exit 2 — checked by doing it, then reverting. A declaration turns the
drift check off for its anchor, so the aim check is the thing that keeps it honest.

**Reported and deliberately NOT corrected — the About-link anchor in the three legal documents.**
`EULA.md`, `PRIVACY.md` and `TRADEMARKS.md` cite where the product's one outbound hyperlink is
declared, and `--fix` moved all three from `src/PluginEditor.h:293` to `:223` in this change set.
That re-anchor is mechanically correct and **preserves a pre-existing mistake**: at the merge base
`:213` already read `return juce::TooltipWindow::getTipFor (c);`, and `:223` reads the identical
line today, while `aboutLink` actually lives at `src/PluginEditor.h:475`. So the rot predates this
change and was faithfully carried, not created by it — precisely the failure mode
`check-citations.py`'s own header describes ("it CANNOT tell you a citation was aimed at the wrong
code to begin with… and it does so INVISIBLY, in a DRIFTED line that reads like a repair"). It is
left for its own change because re-**aiming** three legal documents is a different act from
re-**anchoring** them, needs the `DELIBERATE_REAIMS` declaration that distinguishes the two, and has
nothing to do with hover; recording it here is what stops the paragraph above reading as though those
three anchors were verified correct.

**NOW CLOSED (2026-08-19), as its own standalone change.** The three documents cite
**`src/PluginEditor.h:475`**, where `aboutLink` is actually declared, instead of `:223` — which is
`return juce::TooltipWindow::getTipFor (c);` inside `GatedTooltipWindow`, the line `--fix` had
carried the mis-aim onto from the merge base's `:213`. Only the number changed in each document; no
wording, formatting or meaning was touched, and the correction is one anchor per file.

It needed a declaration, and the reason is the mechanism recorded above rather than a guess: each of
the three documents holds **exactly one** `src/PluginEditor.h` citation in both the base and the
current tree, so the count guard does not fire, the pair IS compared, and the re-aim reads as drift.
Measured: before the entries were written the run reported all three `DRIFTED … -> :223`, and `--fix`
would have dragged every one of them back. `("EULA.md" | "PRIVACY.md" | "TRADEMARKS.md",
"src/PluginEditor.h:475"): "aboutLink"` now covers them, and the substring is what keeps that
off-switch honest: `verify_reaim_targets` resolves `:465` against the live header every run, and
mutating one entry's substring to a value the line does not contain makes the run emit `::error::`
and exit 2 — checked by doing it, then reverting. Re-running `--fix` afterwards leaves all three
documents byte-identical.

That is the whole change: three numbers and three declarations. The checker itself is unchanged
apart from the added entries — no rule was relaxed, no path exempted, and the 63 unqualified
references recorded earlier are still untouched.

**Drift reported and corrected (C6):** `docs/KNOWN_ISSUES.md`'s summary table stopped at KI-022, so
**KI-023 had a full entry but no table row** from the day it was filed (2026-08-18). Its row is added
with the two new ones. Separately, and **not** acted on: `check-citations.py` now reports ~20
`DELIBERATE_REAIMS` declarations as unnecessary against `origin/main`, because PR #113 merged and
they described re-aims relative to the old merge base. They are notes, not failures, and pruning them
is tooling hygiene for its own round rather than part of a hover fix.

**A standing claim in the testing documentation is narrowed by evidence.** The INC-010 and v0.9.3
gap entries both state that the *behavioural* half of editor testing — a driven message loop with
synthetic pointer input — "remains out of reach". On Linux it is not: `xvfb` is already on the CI
runner for pluginval, and the harness above drove the editor, opened menus and positioned the real
pointer with no repository change at all. What is still owed is making it a **committed** target — a
CMake target, an `xvfb` wrapper in `run-tests.sh`, and the same thing proven on the Windows and macOS
runners, where no virtual display is configured. That is the harness change the INC-010 entry already
says should land on its own merits, so it is recorded there as a measured correction rather than
attempted here.

**A fourth review pass asked whether controls that fall back to JUCE's CACHED mouse-over state can
stay lit under a menu. Investigated and measured: NO, and no code change is warranted.** The concern
is well-formed — the fix's occlusion term reaches the geometric paths only, so anything drawing from
an event-driven over-state would be outside it — but every step of the path turns out to be closed,
and the two controls the review names are the two that cannot show it.

**The fallbacks are `animOr(component, "hovA", <cached state>)`, and the fallback fires only when
the property is ABSENT.** `registerAnimated` seeds `"hovA"` on every control it takes, so for a
registered control the cached argument is evaluated and then discarded. Counted on the running
editor rather than from the registration list: **43 of 45** controls carry the seeded property. The
two that do not are `titleButton` and `aboutLink`, and neither can produce the reported symptom:

  * **`titleButton` — the review's named example — has a DEAD fallback.** It carries componentID
    `"ghost"`, and `drawButtonBackground` returns at `src/gui/LookAndFeel.cpp:369` — *before* the
    `animOr (b, "hovA", highlighted)` on `:373` is reached. Its text path ends at
    `LookAndFeel_V4::drawButtonText (g, b, false, false)`, which passes `highlighted` as a literal
    `false`. So the control has no hover visual at all, registered or not; the argument the review
    traces never reaches a pixel.
  * **`aboutLink` has a live fallback but cannot be occluded by a menu.** It is the *only* child of
    `aboutBackdrop` (`src/PluginEditor.cpp:580`), so it is on screen only while the About overlay
    is. The editor's only menu-openers are `presetName.onClick` (`:350`) and the combo drop-downs —
    all of them outside that overlay and covered by it while it is up, with `Backdrop::mouseDown`
    eating the click. No pop-up menu can be open while `aboutLink` is visible.

**Measured, not merely argued.** With a combo list open (`702,279 125×114`) and the pointer inside
it, **zero** controls report `isMouseOver`, and **zero** buttons report `Button::isOver()` — the
event-driven flag `drawButtonBackground` and `drawToggleButton` actually receive. Both mechanisms
were checked because they are backed differently: `Component::isMouseOver` re-queries
`getComponentUnderMouse()` on the message thread (the cached flag is used only off it,
`juce_Component.cpp:3182-3196`), whereas `Button::isOver()` returns `buttonState != buttonNormal`
(`juce_Button.cpp:327`), which enter/exit maintain. The one shape that could strand the latter — the
pointer resting on the very control that opens the pop-up, so no `mouseExit` precedes modality — was
driven directly: pointer parked on `presetName`, its menu opened with no pointer move, menu covering
the pointer. `hovA` went **0.990 → 0.020** and the button reads correctly dark, because `presetName`
is registered and the seeded property wins.

**`SpectrumImager` and `ABControl` are covered too, by different mechanisms.** The imager's
`panelHoverA` comes from `isMouseOverOrDragging(true)`, i.e. `componentUnderMouse` — measured
**false** while the menu is open, because the raised shield is `toFront`ed over the editor and
intercepts, so JUCE resolves the component under the pointer away from the imager on its own. Its
index-based indices (`hoverHandle`/`hoverWidth`/`hoverDelete`/`hoverSolo`) are cleared by the
`mouseExit` that necessarily precedes opening a pop-up, since every menu-opener is a click on a
control outside the imager and nothing is modal at that instant. `ABControl` is registered, so its
`hovered` member is only the `animOr` fallback and never reached — a point the review concedes.

**The CHANGELOG sentence was re-checked against this and left as written.** "A control the menu
covers stays dark" is accurate for every control a menu can actually cover: those are all in the
registered 43. The two exceptions are not reachable by a menu, so the sentence is not broader than
the predicate.

**Left unchanged deliberately, and this is the smallest correct outcome.** Registering `titleButton`
or `aboutLink` would add per-frame work for a control with no hover visual and a control no menu can
reach, and would change what the About link looks like on hover for no defect. Touching
`src/PluginEditor.cpp` at all would also shift the citation anchors corrected in the previous three
rounds, several of which are new spellings the gate cannot re-map from the base — so a cosmetic edit
here would silently re-rot them.

**MAINTAINER SIGN-OFF RECORDED HERE, granted 2026-08-19**, for the hover-occlusion fix, for the
three citation-correction rounds the reviews of it produced (the third added the `PRIVACY.md`
re-aim and its `DELIBERATE_REAIMS` declaration), and for the fourth round's finding that the
cached-mouse-over fallbacks need no code change — the evidence for that conclusion is the paragraph
above, and the sign-off covers the decision to leave them unchanged, not a claim that they were
altered. Confirmed after review; recorded, not
requested again. It supersedes this entry's earlier "NOT GRANTED" status, which stood while the
confirmation was genuinely outstanding. Scope is this change set only; no approval is claimed for
the About-link re-aim left open below, or for anything else in the reviews that produced these
rounds.

**What the sign-off rests on, stated so the two are not confused.** It discharges ADR-0025 §3
disclosure 2 the way the v0.9.3 entries did — on the reasoning, the root cause and the recorded
evidence, which here is a **measurement**, not an argument: `hovA` 0.990 → 0.000 under an open combo
list, 0.990 → 0.000 under a preset menu tall enough to reach the A/B control, and both branches
mutation-tested. That evidence is **Linux-only and taken on a synthetic (`xvfb`) display**, and the
sign-off does not convert it into a per-platform Level-5 DAW check, which is a different act and is
not claimed as performed. The remaining visual confirmation — with each of the seven drop-downs and
the preset menu open, that a covered control goes dark, that the box which owns the list keeps its
highlight and open bloom, that a control merely beside the list is unaffected, and that hover
returns on dismissal, at 100 % and 150 % UI scale — rides with the release's existing Level-5
audition rather than standing as an open action item against this round.

**Implementation round (2026-08-18): six approved roadmap items landed — selective realtime
diagnostics, a benchmark harness, state fuzzing, a GCC-only warning gate, LeakSanitizer as a gate,
and the pluginval crash-retry narrowed to its own justification. No new ADR: ADR-0029's evidence is
extended in place and no decision in it is reopened.**

**`-Wfunction-effects` was narrowed rather than reversed.** ADR-0029 refused it on the strength of a
52-warning census over the annotated engine TU. Re-reading that census rather than restating it: all
52 are *transitive through JUCE*, calls whose definitions the TU cannot see. They therefore appear
only where JUCE appears, and the repository has a layer where it does not — `MidSide.h`,
`LR4Xover.h`, `ScopeBuffer.h`, `Correlation.h`, `LevelMeters.h`. Measured over that layer the flag
emits **0**, and it still fires precisely: a seeded call from the annotated driver to a helper that
grows a `std::vector` produces `error: function with 'nonblocking' attribute must not call
non-'nonblocking' function '(anonymous namespace)::canaryAllocatingHelper'`. (This entry first named
`anamorph::applyWidth` as the seeded call, which cannot be right — its definition is visible in that
TU, so its effects are inferred, and the driver calls it in the compile that must stay clean. What fails is the
EFFECT, not the missing annotation.) `tests/realtime_effects.cpp` is the driver;
it is compiled `-fsyntax-only` with `-Werror=function-effects` as a seconds-long step in the
`realtime` job, adds no target to the shipped build, and proves those bodies effect-clean *before*
any test executes them — the one thing a runtime tool structurally cannot do, since it sees only the
branches the suite takes.

**The benchmark harness answers PERFORMANCE_BUDGET's own constraint C2**, "a number without its
machine and method is not a measurement". `tests/bench.cpp` (behind `ANAMORPH_BUILD_BENCH=OFF`)
refuses to run — exit 2 — when it cannot identify the CPU and `ANAMORPH_BENCH_CPU` is unset, rather
than printing an unattributable number. It reports median ns/sample, worst-block µs, spread and
percentage of one core across reference/idle, the four algorithms, four sample rates, four block
sizes, oversampling engaged and the multiband path including the RISK-002 dragging split.
**It is deliberately NOT a CI threshold gate, and that is a measurement rather than a preference**:
across independent invocations on an otherwise idle machine the median ns/sample varied by **7.2%**
and the worst-block figure by **65.4%**. A gate on the second number would be noise, and a gate on
the first would sit inside its own variance. CI therefore builds it and smoke-runs it — which
catches the real regression risk, a harness that stops compiling against the engine it measures —
and the numbers are taken deliberately, on a named machine, into PERFORMANCE_BUDGET.

**`setStateInformation` is the one entry point that parses bytes the plug-in did not write**, and
every existing test of it was written after a human thought of the case. `tests/fuzz_state.cpp`
drives it under libFuzzer with ASan + UBSan as the oracle: a rejected blob is a **pass**, because
refusing malformed state is what the path is for. The corpus (`tests/fuzz-corpus/`, three entries) is
generated from the committed XML fixtures in JUCE's `copyXmlToBinary` framing, so the fuzzer starts
from inputs that already reach the parser rather than from noise. Two implementation facts are
load-bearing and both were found by running it: `-fsanitize=fuzzer` must be **target-scoped**, since
libFuzzer's `main` collides with CMake's compiler-probe program and configure fails at `project()`;
and the JUCE initialiser must be **leaked deliberately**, because letting `shutdownJuce_GUI()` run
under libFuzzer's `exit()` double-freed in `DeletedAtShutdown::deleteAll()` during
`__run_exit_handlers` — the fuzzer found that on the empty input within 60 s, in the harness rather
than in the product. After the fix: 5,351 execs, exit 0.

**A GCC warning gate is not redundant beside the Clang one, because "larger set" is not "superset".**
`juce_recommended_warning_flags` picks by compiler ID and Clang's set is the larger one, but Clang's
`-Wshadow-all` structurally does not report a parameter or local shadowing a member outside a
constructor, and Clang has no `-Wmisleading-indentation` at all. Three first-party sites in this tree
are visible only to GCC and are recorded as accepted debt, not fixed — the gate exists so the next
one fails the push that introduces it. The gated set is five flags; two candidates were measured and
**rejected**: `-Wnull-dereference` (four unprovable post-inlining hits — the baseline shape that
trains people to regenerate without reading) and `-Wmismatched-new-delete` (a false positive *by
construction* on `tests/AllocationGuard.h`, verified both by funnelling deallocations through
`::operator delete` and by calling `std::free` directly). The gate rides on `linux-lto-tests` rather
than a job of its own because that job's two targets already compile every first-party translation
unit; it costs one `tee`. Both directions were verified live: a seeded `-Wduplicated-branches` and a
seeded `-Wshadow` in `tests/dsp_tests.cpp` failed the gate by name at exit 1, and the same run
demonstrated the falling-count `::notice::` path on an incremental build.

**LeakSanitizer stopped being suppressed.** The previous round had already retested the stated
justification for `detect_leaks=0` and found it false — both suites run leak-clean. This round
flipped the flag: a suppressed detector with nothing left to suppress is a gate the repository was
paying for and not receiving, and a leak in a plug-in process is a leak in a host that stays open for
hours. The `fuzz` job is the one place `detect_leaks=0` remains, for the deliberate initialiser leak
above.

**The pluginval crash retry was broader than the flake that justified it.** `run-pluginval.sh`
retried a crashed pass three times on every platform, while the documented cause — the XEmbed race —
is Linux/X11 only. On Windows and macOS that turned a genuine crash into two more chances to pass.
The retry is now scoped by `uname -s`: three attempts on Linux, one everywhere else, with a distinct
message so a single-attempt failure cannot be misread as an exhausted retry. The separately
justified `.ps1` retry is untouched.

**Shared-action-input round (2026-08-19): the composite action built shell source out of its own
inputs, and the sanitizers comment quoted a count from two commits ago.**

**MAINTAINER SIGN-OFF RECORDED HERE, granted 2026-08-19**, for both corrections below. Confirmed
before the work; recorded, not requested again. No approval is claimed for anything else in the
review that produced them.

**`${{ }}` is substituted into a `run:` body before bash sees it, so an input interpolated there is
CODE.** `.github/actions/setup-linux-build/action.yml` pasted both of its inputs into the script:
`"${{ inputs.clang-version }}"` and, unquoted, `${{ inputs.extra-packages }}`. Every one of the
seven callers passes a literal or a workflow `env` constant, so nothing was exploitable — but the
seven inline blocks this action replaced had no parameters at all, and the contract it now publishes
accepts arbitrary text for a position that executes. Both inputs are now bound to the step's `env:`
and read as `"$CLANG_VERSION"` / `$EXTRA_PACKAGES`. The package list stays UNQUOTED, deliberately and
for the unchanged reason: word-splitting is how a space-separated list becomes several arguments.
Unquoted there splits a value into words; interpolated into the script it was parsed as shell.

Verified by executing the action's real script body against stub `setup-linux.sh`,
`setup-llvm-apt.sh`, `sudo`, `apt-get` and `ccache`, once per caller shape: no inputs; clang only
(`setup-llvm-apt.sh` receives the version as exactly ONE argument); clang + `valgrind`; `g++-13`
only; and a three-word list, which still reaches `apt-get` as three arguments. The ccache fallback
was exercised too — a failing `ccache` still yields the `::warning::`, an empty
`ANAMORPH_COMPILER_LAUNCHER` and step exit 0. And the property itself, both ways: with
`extra-packages` set to `valgrind; touch /tmp/PWNED`, the previous shape ran the `touch`, while the
current shape hands the whole string to `apt-get` as arguments and executes nothing.

**The sanitizers comment said 159, and 162 would have been wrong too.** It justifies `detect_leaks=1`
with "both suites run leak-CLEAN, 159 + 900 checks" — a figure from when the DSP suite was 161. The
suite is 162 now, but this job does not report 162: ASan owns `malloc`, so the allocation guard's
malloc half is compiled out, Test 38 discloses that and skips its two malloc-family assertions.
Measured by building with this job's own flag set (clang-22, the full seven-check UBSan list, vptr)
and running both suites under `ASAN_OPTIONS=detect_leaks=1`: **160 + 900 checks, 0 failures, zero
LeakSanitizer reports, exit 0**. The comment now says 160 — one character, deliberately: `build.yml`
is a citation target, so adding the explanation inline would have shifted every anchor below it and
turned a one-word correction into a re-anchoring round. The reason 160 is not 162 is recorded HERE
instead, which is where this repository keeps the reasoning that would otherwise go stale in place:
**ASan owns `malloc`, so the allocation guard's malloc half is compiled out, and Test 38 discloses
that and skips its two malloc-family assertions.** A reader who later "corrects" 160 to 162 will
find this entry.

**Left alone, deliberately:** `build.yml:2520`'s "the 156 + 900 checks" is a past-tense statement
about what had only ever run non-LTO before `linux-lto-tests` existed — history, correct as written,
and not the sanitizers figure this round was asked to fix.

**`-Wmismatched-new-delete` round (2026-08-19): the obvious fix was measured and does not work, so
the exclusion stays and now carries its numbers.**

**MAINTAINER SIGN-OFF RECORDED HERE, granted 2026-08-19**, for re-examining the exclusion and for
the documentation correction that came out of it. Confirmed before the work; recorded, not requested
again. **No approval is claimed for a gate change, because none was made** — see below for why.

**The objection is the right one to raise.** Every other accepted GCC-only diagnostic goes into
`scripts/gcc-warning-baseline.txt` with a site count, so a NEW instance fails;
`-Wmismatched-new-delete` is instead absent from `GATED_FLAGS`, which reads as a permanent hole for
the sake of one false positive in `tests/AllocationGuard.h`. Baselining it (count 1, that file)
would, on the face of it, keep the class gated everywhere else. Measured on gcc-13.3.0 / Ubuntu
24.04 — the pair `linux-lto-tests` pins — with the flag appended to the gated set and the two gated
targets built exactly as the baseline header prescribes, neither half of that holds.

**Under `-flto` the flag emits nothing at all.** The whole two-target build produced **zero**
`-Wmismatched-new-delete` lines, first-party and vendored alike. Not because the tree is clean: a
genuine `free` on `new[]` memory **seeded** into `tests/dsp_tests.cpp` also produced zero, and so did
a second seed in `tests/state_tests.cpp` called from `main()` so nothing could elide it
(`AnamorphStateTests` relinked, zero hits, exit 0). The same dsp_tests seed in the same translation
unit with `-flto` removed produces **3**. That lane compiles
everything `-flto`, which is its reason to exist, so adding the flag to `GATED_FLAGS` would add a
flag that cannot fire in the job that reads the log — the shape `TESTING_POLICY.md` rule 4 names as
indistinguishable from a gate that passes, and worse than an exclusion that says so out loud.

**And the attribution turns a per-file baseline into a mask.** Without `-flto` the diagnostic lands
on the guard's deallocator — `tests/AllocationGuard.h:351:69`, `operator delete (void*,
std::size_t)` — for the false positive AND for the seeded real mismatch alike, because that is where
the `free` is. `scan()` deduplicates by `path:line:col`, so the two collapse to ONE site and a
`1 -Wmismatched-new-delete tests/AllocationGuard.h` line would accept the real one. The property the
baseline exists for — a new instance fails — is precisely what this flag cannot have per file.

**So nothing was gated and nothing was baselined**, and the exclusion in
`scripts/check-gcc-warnings.py` now carries both measurements rather than the one-line "false
positive by construction" that invited the question. The only shape that would gate this class
honestly is a non-LTO GCC compile of the same sources, which this pipeline does not have; adding one
is a workflow decision rather than a flag-list edit, and is recorded here as the option rather than
taken. The gated set, the baseline file, the self-test (17 cases, including the case that pins this
flag as deliberately ungated) and the guard itself are unchanged.

**Allocation-family round (2026-08-19): the guard's two counters were one counter twice, and three
documents still carried the pre-Test-38 check count.**

**MAINTAINER SIGN-OFF RECORDED HERE, granted 2026-08-19**, for both corrections in this round.
Confirmed before the work; recorded, not requested again. No other approval is claimed by this entry.

**`operator new` was counted twice.** It incremented `newCount` and then called `std::malloc` — and
where `ANAMORPH_GUARD_MALLOC` is defined that name resolves to the interposer in the same
translation unit, so `mallocCount` moved as well. Nothing asserted wrongly: Test 38 requires both
counters to be zero, and `selfCheck()`'s probes read the counter each is about. What was wrong is
what the run PRINTS. `new=N malloc=M` was not a split — every `new` appeared in both halves — and
the per-`prepare()` figures the header and `REALTIME_SAFETY_AUDIT.md` quote could not be reproduced
from the code that printed them. Measured: the same `prepare()` reported **102 new + 765 malloc**,
the second being 663 + 102.

Fixed with `rawAlloc`, which the two non-aligned `operator new` forms now call: `__libc_malloc`
where the interposer exists, `std::malloc` everywhere else. That is the same real allocator the
interposer itself forwards to, so the `new` route takes the identical path with one fewer counter on
it, and its blocks stay ordinary glibc heap blocks that the unchanged `std::free` in every
`operator delete` frees — the pairing the interposer already relied on. The over-aligned forms were
already clean (`posix_memalign` is not interposed), and the malloc half's own liveness probe still
calls `std::malloc` deliberately, because it exists to prove the interposer fires. **The documented
split needed no edit: it was right and the code had drifted from it.** After the fix the same
`prepare()` reports **102 new + 663 malloc**, exactly the figures both documents carry.

**Verified in all three shapes and in both directions.** Compiled and run standalone: glibc
(all three halves live, one `new` now moves `new` only — `new=1 malloc=0`, previously both), the
ASan shape (malloc half compiled out and disclosed, `new` half live, unchanged), and
`ANAMORPH_NO_ALLOC_GUARD` (everything stands down, unchanged) — so ASan, RealtimeSanitizer and
valgrind are untouched by construction as well as by measurement. Detection is unweakened and now
attributes correctly: a seeded `operator new` in `process` fails Test 38 reporting `new=1 malloc=0`,
a seeded raw `malloc` fails it reporting `new=0 malloc=1`.

**Three documents still said 160 checks.** `REALTIME_SAFETY_AUDIT.md`, `HANDOVER.md` and
`RELEASE_HARDENING_PLAN.md` carried the count from before Test 38's assertions; the suite is at
**162**. The audit's sentence carried one current number and one stale one — "**156** of the suite's
160 checks" — and 156 is the RTSan figure, which is unchanged and was measured again here rather
than inferred: built with the guard compiled out, the suite reports **156 checks, 0 failures** and
Test 38 discloses and asserts nothing. 162 − 156 = 6 is Test 38's own assertions on the
GCC-Linux configuration.

**"37 DSP self-tests" was checked and is NOT stale**, which is why it was not touched. `main()`
invokes 38 test functions; the last is the A/B state-restoration clamp guard, which every one of
those sentences already counts separately. The printed roster reaches "Test 38" because
`testBypassNullAndLatency` prints "Test 3+4" — one function, two numbers. 38 functions − the A/B
guard = the 37 the documents claim. The `140 checks` figures in `DEPENDENCY_POLICY.md` and in
ADR-0021/0022/0026/0027 are measurements at those revisions, not current-state claims, and are left.

**Lint-count round (2026-08-19): the two places that still said four.**

**MAINTAINER SIGN-OFF RECORDED HERE, granted 2026-08-19**, for this documentation correction.
Confirmed before the work; recorded, not requested again. No other approval is claimed by this entry.

**The round below named this and left it; this round closes it.** `REPOSITORY_MAP.md`'s `scripts/`
tree summary still read "plus the four CI lints (check-docs, check-portability, check-citations,
check-clang-warnings — each with its own self-test)" while the same file's own table lists all seven
and its `preflight.sh` row already says "the **seven** checkers"; `CI_CD.md` compared the RTSan
canary to "the four lints' `--self-test`s". Both now say seven and the tree names all seven. The
summary line is the first thing a new reader hits, which is why a count there is worth more than its
size: `TESTING_POLICY.md` rule 4 enumerates seven, so a reader arriving through the map met the
divergence before the policy.

**Two more "four"s were checked and are correct, not stale.** `REPOSITORY_MAP.md`'s
`check-citations.py` row says the gate governs "the four `scripts/`", and `TRACKED` does list exactly
four — `build.sh`, `run-pluginval.sh`, `run-tests.sh`, `setup-linux.sh`. And ADR-0029 §9 says the
canary "is the maintenance the repository already performs for its four lints", which was the state
when it was decided: `check-realtime.py` was introduced by the change set that ADR authorised. An
Accepted ADR records what was decided and known then; it is not a place to re-count. Left, with the
reason, so the next reader does not re-derive it. Also left, as before: the same phrasing in
`.github/workflows/build.yml:3209` and `.github/workflows/build.yml:3294`, this round being
documentation-only. **Both are path-qualified now, and the second one earned it twice over.** It
was `:2836` and bare, which was right when written — the phrasing sat there through `a925e79` —
then went stale in `be99567` and stayed stale through `12c545d` and `31c3b1b`, because a bare
anchor carries no path and `CITATION` requires one, so no run could see it. PR #123 corrected the
number and left the spelling bare, reasoning that qualifying it would change one citation's ANCHOR
COUNT and need a `DELIBERATE_REAIMS` entry. **It went stale again in the very next change set** —
this one, which moved the phrasing to `:2888` while `--fix` moved its tracked companion and left
the bare half behind, exactly as before. Written as a SECOND citation of the same path rather than
as a second anchor on the first, the count of the existing citation is unchanged and the gate's
note about an added citation is a note rather than a failure. Two rounds of evidence that the bare
spelling is only safe for prose that must NOT be rewritten.

**Policy-topology round (2026-08-19): rule 4 described a placement three of its own seven checkers
cannot have. One report investigated and closed with no change.**

**MAINTAINER SIGN-OFF RECORDED HERE, granted 2026-08-19**, for the documentation correction below —
restating rule 4's placement requirement as the job and the order rather than adjacency. Confirmed
before the work; recorded, not requested again. No other approval is claimed by this entry.

**Rule 4 read as though all seven self-tests sit one step before their check.** They do not, and
three of them structurally cannot: the two warning gates classify a BUILD LOG, so the build has to
sit between the self-test and the gate, and `check-citations.py` compares against a BASE REVISION,
so its self-test is its own step and the comparison follows in the step that resolves the base.
Under the old wording the pipeline violated its own policy in three places while behaving exactly
as intended. The rule now states what it actually requires — the self-test runs **in the same job as
the check it vouches for, ahead of that job's use of the checker**, never in a different job,
workflow or run — names the job each of the seven pairs lives in (`docs`; `source-lint` ×3;
`linux-clang`; `linux-lto-tests`; `linux`), and says which four are adjacent and why the other three
are not. The intent is untouched: a liveness proof somewhere else proves nothing about the run whose
silence is being read.

**Read off the workflow, not off the review.** The report asserted that
`check-clang-warnings.py` and `check-gcc-warnings.py` "self-test in one job and gate in another".
They do not — `check-clang-warnings.py` self-tests at `.github/workflows/build.yml:650` and gates at
`:944`, both in one job; `check-gcc-warnings.py` self-tests at `:2530` and gates at `:2551`,
both in `linux-lto-tests`. All seven pairs are same-job. (The Clang pair was in `linux-clang` when
this round ran; ADR-0030 folded that job into `linux`, moving both lines together and leaving the
same-job property intact — which is the property the sentence is about.) What was genuinely wrong was "immediately
before", not the job placement, and that is what changed.

**One downstream copy followed, and only one.** `docs/procedures/CI_CD.md` restated the same
"immediately before" claim for `source-lint`'s three lints, where it is true of two of them; leaving
it would have left a Procedures document contradicting the Policy on the exact sentence being
corrected, which the authority order in `SOURCE_OF_TRUTH.md` does not permit. Deliberately NOT
followed: the `source-lint` comment at `.github/workflows/build.yml:463` carries the same phrasing
about the citation self-test, and the `scripts/` tree summary in `REPOSITORY_MAP.md` still
enumerates four lints where its own table lists seven. Both are real; neither is this round's
subject, and the second is a stale COUNT rather than the placement claim.

**The CI-target report was investigated and required no change.** It read the visible diff as adding
only three `option()` declarations and asked whether `AnamorphFuzzState`, `AnamorphBench` and
`AnamorphDspDump` resolve to anything. They do: `CMakeLists.txt:603-621`, `:560-577` and `:597-623`
define them, the last including the target-scoped `-fsanitize=fuzzer` the workflow comment relies
on. Verified by building rather than by reading — all three configure and compile from the same
option and compiler flags CI passes (JUCE supplied from the already-fetched checkout rather than
re-cloned): the benchmark builds and smoke-runs, `AnamorphFuzzState` builds under
clang-22 + ASan/UBSan/libFuzzer and completes 1,161 runs over the committed corpus at exit 0, and
the dump links. Two further premises of the
report were checked and are also unfounded: no CI job builds `AnamorphDspDump` at all (it is the
local instrument for a dependency bump, exactly as `TESTING.md` describes), and the `realtime` job's
`-I build-rtsan/_deps/juce-src/modules` is correct because `ANAMORPH_JUCE_PATH` is a
local-developer escape hatch that no workflow sets, so CI always takes the FetchContent path.

**Armed-transition round (2026-08-19): the portable allocation gate proved the steady state and
called it the switch.**

**MAINTAINER SIGN-OFF RECORDED HERE, granted 2026-08-19**, for the one point this round rests on:
that Test 38's coverage of mid-stream configuration transitions was missing and had to be closed in
the test rather than in its description. Confirmed before the work; recorded, not requested again.
No other approval is claimed by this entry.

**Test 38 never armed a parameter CHANGE.** The per-configuration `setParameters (p); reset();` ran
*before* the block loop, and `reset()` flushes an in-flight duck straight to its target
(`src/dsp/AnamorphEngine.cpp:175-182`) — so by the time the counters were armed the switch was over,
`switchState` was `Normal`, and the `setParameters (p)` inside the armed region hit the steady-state
no-change gate every time. The whole structural half of a switch lives in the adopt block
(`src/dsp/AnamorphEngine.cpp:750-825`: algorithm tails cleared, the three oversamplers and the
chorus reset on an oversampling-path change, the crossover cleared on a topology change) and it runs
inside `process()`, at the silent bottom of the duck. So 3,840 armed calls proved the audio path
allocation-free while nothing was changing, and `REALTIME_SAFETY_AUDIT.md` presented that gate as
the committed form of a probe whose stated coverage was "mid-stream algorithm swaps". Measured, not
inferred: one `std::vector` allocation seeded into the adopt block was **invisible** — 3,840 armed
calls, worst `new` 0, run green.

**Fixed by not applying the switch outside the armed region**, which is the whole change: the first
armed block of each configuration performs the real transition from the previous one and the armed
blocks after it carry the duck through its landing. Every configuration differs from its predecessor
in at least `msMode`, so all 32 are genuine mid-stream switches, and no `reset()` between them is
deliberate — a host does not get one either. The same seeded allocation now **fails** the test
(worst `new` 2, worst `malloc` 2, exit 1). The guard, its diagnostics, the matrix, the 3,840 armed
calls and every existing assertion are untouched.

**A gate that can silently narrow again needs to say so**, per the repository's own rule that a
checker must prove it is live. Test 38 now counts the switch landings it OBSERVES — `osEngaged`, and
therefore the reported latency, is re-latched only in that adopt block — reads the latency across
the armed scope alone rather than carried across configurations, and fails when the count is zero.
Both halves are measured: restoring the pre-loop flush drops the count to 0 and fails the new check,
and a *carried* comparison still reported 11 against that same flush, which is why the reading is
scoped. The count is a floor rather than a census, and the difference is measured too: all 32
configurations land, but the half-band polyphase IIR reports 4 samples at ×2 and 6 at both ×4 and
×8, so the four ×4 → ×8 landings move no latency. 11 of the 15 latency-visible transitions are
counted, across all four algorithms. Checks 161 → 162.

**One document needed correcting and one deliberately did not.**
`REALTIME_SAFETY_AUDIT.md` said the committed gate "runs the same counting over the same matrix" as
a probe described as covering "mid-stream algorithm swaps, bypass crossfades and crossover drags";
it now states what the gate arms, what it did not arm before this round, and what it still does not
carry over — `bypass` and `mbBands` are fixed across its matrix, so bypass crossfades and crossover
drags are the click-free-transition tests' and (on Linux/Clang) RTSan's. ADR-0029 §7 was re-read and
left alone: its 7,680-call figure is the investigation probe's, attributed to the probe, and its
coverage table already reads 3,840 armed calls per configuration of the suite. Neither number was
wrong; only the claim about what the gate arms was.

**Reachability-and-runnability round (2026-08-19): a documented command that could not do what the
paragraph beneath it requires, a lint that could not see past a template, and a portability report
that did not reproduce.**

**NO NEW MAINTAINER SIGN-OFF IS RECORDED HERE.** The two decisions confirmed 2026-08-19 in the round
directly below — the ADR-0009 re-aim and the restated leaf-layer liveness evidence — stand as
recorded and are not reopened. Nothing in this round is a decision the process asks a human to
confirm, so it claims no approval of its own.

**The documented local fuzz command ran with leak detection ON.** `TESTING.md` set
`ASAN_OPTIONS=detect_leaks=0` on a continued line and then put three comment lines between the
continuation and the binary. A `\` before a comment line splices the two, so the logical line is
`ASAN_OPTIONS=detect_leaks=0  # Note: ...` — an assignment with no command. The variable reaches
nothing, the remaining comment lines are their own no-ops, and the fuzzer starts on the line after
them with the environment untouched. Measured on text extracted from the document: the child process
saw no `ASAN_OPTIONS` at all, at exit 0 — the same silence a correct run produces, which is why this
survived. Five lines below, the same document calls `detect_leaks=0` "required, not optional" and
gives the reason (`tests/fuzz_state.cpp` leaks `ScopedJuceInitialiser_GUI` deliberately, because
letting `shutdownJuce_GUI()` run under libFuzzer's `exit()` double-frees in
`DeletedAtShutdown::deleteAll()`), so the command as printed contradicted the paragraph explaining
it and would end a local fuzz run in exactly the leak report the harness design accepts on purpose.
The three comment lines moved above the command; re-extracted from the document, the child now
receives `detect_leaks=0`. CI never had this defect — the `fuzz` job sets the variable through
`env:`, not a command prefix — and the divergence between the two is what the fix removes.

**A helper whose return type was a template was invisible to the realtime lint.**
`heads_a_definition` decides whether a name heads a definition by what precedes it: `::`, a `*`/`&`
that ends a type, or an identifier. A `>` was on neither list, so `std::vector<float> helper (` was
rejected while `float helper (` a line above was accepted. The cost is not one missing definition, it
is a missing SUBTREE: a name absent from `definition_index` is one `reachable_bodies` cannot follow
into, so the helper's allocations and everything IT calls were both unscanned — and a lint that
discards a function prints what a clean tree prints. Measured: `std::vector<float> helper (int n)
{ v.assign (n, 0.0f); return v; }` called from an annotated `process` reports **0** findings before
the fix and **1** after; the same helper returning `float` reported 1 throughout. Fixed by asking
`_blank_template_args` — the balanced-span parser already in this file, whose seven rules are
mutation-pinned — whether this `>` closes a template argument list, over a window that starts at the
enclosing statement. Asking it rather than re-deriving it is the point: a second `<`/`>` parser here
is a second one to keep in agreement, and the window is what keeps the answer about THIS declarator
rather than a comparison a statement back. Enforcement surface after the change: **470** index names
(was 467), **61 reachable spans — unchanged**, 44 files, 0 violations. The three newly indexed
definitions are `logFreqRange`, `logFreqRangeCentred` (`src/PluginParameters.cpp`) and
`LevelMeter::bar` (`src/gui/LevelMeter.h`), none of them audio-path reachable, so this widens what
the lint CAN see without changing what it currently says. Pinned by two end-to-end cases and twelve
`heads_a_definition` assertions (76 → 90 self-test cases), and every one of them is load-bearing:
a bare `return True` for the branch fails eight, removing the branch fails six, dropping the
statement window fails the previous-statement case, and dropping the "`<` opens only after a name"
rule fails four. Stated rather than implied: on `src/` as it stands the naive `return True` also
reports 470/61/0, because `_is_declarator_tail` independently rejects a comparison — a comparison's
call sits inside an enclosing paren whose `)` drives the region depth negative before any `{`. The
guard is not what keeps this tree quiet; it is what keeps "does this `>` close a template argument
list" a question with one answer.

**The `preflight.sh` Bash 3.2 report did not reproduce, and the script is unchanged.** The claim was
that `${#ABI_TARGETS[@]}` (`scripts/preflight.sh:62`) aborts under `set -u` on macOS's `/bin/bash`
when no local Linux Release build exists. Tested rather than reasoned about: GNU bash **3.2.57**, the
exact patchlevel macOS ships, built here from the FSF tarball plus all 57 official patches, runs the
block's real text through **both** branches at exit 0 and runs the whole of `preflight.sh` at exit 0
(161 + 900 checks). `ABI_TARGETS=()` creates the variable, and 3.2's `array_length_reference` errors
only when the variable is ABSENT — `var == 0 || array_p (var) == 0` — returning
`array_num_elements` (0) for an empty one; confirmed both ways, since `${#NEVERSET[@]}` does abort
there. The `set -u` hazard old Bash genuinely has is `"${arr[@]}"` on an empty array, and that
expansion appears here only inside the `-gt 0` branch, where the array is non-empty by construction.
A change with no defect behind it is a change that has to be maintained for nothing, so none was
made.

**Parser-and-evidence round (2026-08-19): one silent false negative in the realtime lint, one
architectural citation pointing at unrelated code, and one liveness claim that was never true.**

**MAINTAINER SIGN-OFF RECORDED HERE, granted 2026-08-19**, covering the two decisions in this round
that the process asks a human to confirm: re-aiming ADR-0009's evidence to
`src/dsp/AnamorphEngine.cpp:1335-1379` (a re-aim, not a re-anchor — the tool cannot compute it, so
it is declared in `DELIBERATE_REAIMS` and its aim machine-checked against
`Defensive NaN / Inf self-heal`), and restating the leaf-layer `-Werror=function-effects` gate's
liveness evidence to name the mechanism the tree actually runs.

**A definition whose signature carried a comma inside template arguments was discarded whole.**
`_is_declarator_tail` rejects a depth-zero comma because an argument list is made of one, and `<`/`>`
were not counted toward depth at all — so `-> std::pair<float,float>` and
`requires std::is_same_v<T,int>` both read as argument lists, and `_bodies` dropped the definition,
its body and every same-file callee reached only through it. Silently, which is the direction that
matters: a lint that discards a function prints what a clean tree prints. Fixed by modelling the
construct — balanced `<...>` spans are blanked before the comma test — with balance REQUIRED, because
`<` is the one bracket C++ overloads with an operator and counting it unconditionally would turn the
false negative into the false positive the `Options` case exists to catch. All seven rules of the
resulting parser are mutation-pinned; independent verification found no false positive or negative
across 3,264 files of JUCE, libstdc++-13 and LLVM-22, and the enforcement surface is unchanged (467
index entries, 61 reachable spans, 44 files, 0 violations, zero files differing).

**ADR-0009 cited unrelated code, and the documents that cite the same block disagreed.** Both the
ADR and the audit carried `AnamorphEngine.cpp:847-870` at the merge base; the audit was re-derived
earlier in this change set while the ADR's copy was only shifted, to `:860-883` — input conditioning,
the M/S solo branch and the `dryScratch` copy. That is the "adopted as-is rather than audited" limit
this repository states for its own citation gate, arriving as predicted: an anchor already misaimed
at the base stays misaimed through `--fix`, because the tool detects MOVEMENT and this never moved.
The span now runs to 1313 rather than stopping at the `if`, because the ADR's Consequences claim the
plugin "self-heals instead of needing a Multiband off/on" and that sentence is about
`multiband.reset()`. Four documents carried copies — `DSP_POLICY.md`, `DEVELOPMENT.md` and the audit
alongside the ADR — and all four now spell it the same way.

**The `applyWidth` liveness claim was false, in eight places.** ADR-0029, `ADR_INDEX`, `CI_CD`,
`HANDOVER`, `TESTING_POLICY`, this file, the workflow comment and the effects TU said the gate is
proven live by a seeded call to the non-annotated `anamorph::applyWidth` failing by name. Measured:
that compile exits 0, and it could never have been otherwise — `applyWidth` is header-defined, so
Clang infers its effects, and the driver calls it in the compile that must stay CLEAN. What fails is
the EFFECT, not the missing annotation. The real proof is the `-DANAMORPH_EFFECTS_CANARY` call to a
helper that grows a `std::vector`, and the quoted diagnostic is the one clang-22 emits. The gate
itself was not touched: the mechanism was already right, only the evidence describing it was stale.

**Known and deliberately left**, so the next reader does not re-derive it: the comment above
`DELIBERATE_REAIMS` still argues the collection is spelled `set([...])` on purpose, which stopped
being true when it became a dict keyed by `(document, anchor)`. Behaviour is unaffected — the
consumers were converted to `.keys()` and the self-test covers them — so it is a stale comment rather
than a defect, held out of a round scoped to correctness.


**Gate-liveness round (2026-08-19): four enforcement mechanisms that could pass while checking
nothing, and six evidence anchors the `CMakeLists.txt` growth left behind.**

**MAINTAINER SIGN-OFF RECORDED HERE, both granted 2026-08-19.** (1) The macOS **retained-LTO-object**
behaviour and the **placement and persistence of the LTO assertion** are confirmed against CI run
**773** (push, `33333fe`): the `macos` job's step 7 `Assert LTO ran and its objects were retained`
passed, and `Assert a validated dSYM was captured` ran last, after the installer and all three
uploads. (2) The **one-time human verification of the `function-effects` warning's availability** is
confirmed. Neither confirmation is a reason to leave a silent CI failure mode standing, and the
durable mechanism below was added regardless — which is the point of recording them separately.

**A gate whose only output is silence must be able to prove it can speak.** Four could not.

- **The leaf-layer `-Werror=function-effects` step.** Clang treats an unrecognised `-Werror=<name>`
  as a `-Wunknown-warning-option` *warning*, so a renamed or dropped option leaves the step exiting
  0 while checking nothing. Measured on the pinned Clang 22.1.8: with the option misspelled,
  `tests/realtime_effects.cpp` carrying a **real seeded violation** compiled with status **0**. It
  is now compiled twice — the gate, then the same TU with `-DANAMORPH_EFFECTS_CANARY` seeding an
  allocating non-annotated helper, asserted to fail *with* a `-Wfunction-effects` diagnostic — and
  both compiles carry `-Werror=unknown-warning-option`. **The canary is the coverage; the flag is
  the diagnosis** — the first draft of this round had that as two independent halves, which is
  measurably wrong: a name Clang no longer knows is also a diagnostic Clang no longer emits, so the
  canary compiles clean and fails the step in the renamed case too. The flag earns its place by
  failing on the *first* compile and naming the cause. The canary alone covers what the flag never
  can — an option accepted but no longer implemented — plus three cases neither was claimed to
  catch: a silently-empty `ANAMORPH_NONBLOCKING`, a `#pragma clang diagnostic ignored` planted in
  the include set, and the driver compiled out. `TESTING_POLICY.md` rule 4's "one-time manual
  verification" wording for this gate is retired.
- **The allocation guard's stand-down under RealtimeSanitizer.** `__has_feature(realtime_sanitizer)`
  was a single unverified spelling holding up the whole lane; verified on Clang 22.1.8 that
  `-fsanitize=realtime` defines **no** preprocessor macro of its own, so no non-circular check could
  be built from the compiler alone. The `realtime` job now also passes `-DANAMORPH_RTSAN_LANE=1` on
  the same flag string, and `tests/AllocationGuard.h` `#error`s if the lane is declared while the
  guard is still live. Proven on the mutation itself, not a proxy for it: with `-fsanitize=realtime`
  genuinely ON and the feature spelling renamed, the build fails — which a circular check could not
  do — while the same mutation against `33333fe` compiles silently. The four quadrants of
  (RTSan on/off) × (flag on/off) behave as intended, only flag-on + RTSan-off failing, and the real
  lane still builds and runs (`AnamorphTests` links, 156 checks, guard disclosed as compiled out).
  ADR-0029 §7 records it.
- **The Linux ABI floor, per declared family.** `check()` compared only the families a binary
  actually referenced, so an **absent** family read as a satisfied one. Demonstrated on a real
  C-only binary: the previous checker reported `within the declared floor (GLIBC_2.38,
  GLIBCXX_3.4.31)` and exited **0** for a file importing no `GLIBCXX_*` at all, and likewise for a
  real `-static-libstdc++` link. A missing declared family is now an error, on the file's own stated
  reasoning that "no requirements found" must not read as clean. Both verdicts are **collected**
  rather than returned from inside the loop: an early return stopped inspecting the remaining
  binaries and discarded the over-floor findings already gathered, so a genuine breach in the
  Standalone could be replaced by a missing-family message about the VST3 — the same
  "withholding the evidence helps least" argument the `linux` job makes for running this step last,
  applied inside the step. Both shipped artifacts import both families, so the real gate is
  unchanged.
- **The realtime lint's lexer and body extractor**, both silent false negatives rather than false
  positives — the dangerous direction for a lint whose value is its silence. `_is_digit_separator`
  tested only the two neighbouring characters, which is also the shape of the **opening** quote of an
  encoded character literal (`L'a'`, `u8'a'`, `u'a'`, `U'a'`): the opener was emitted as code, the
  closer read as an opener, and the rest of the line blanked. It now walks left to the start of the
  token and requires that token to begin a **number** — not merely to begin with a digit, which was
  the first attempt and swallowed leading-dot literals (`.5'0f`, `.000'001`, both ordinary C++ and
  both exactly how a gain constant or a DSP tolerance gets written) in the same way. A stray
  apostrophe with no partner on its line — `#error don't`, which the comment branches do not blank —
  is likewise emitted as prose rather than opening a literal.
  Separately, `_bodies` searched only 400 characters past the closing parenthesis for the opening
  brace, and blanked comments **keep their length**, so a definition with a long comment between `)`
  and `{` left the scanned set with no diagnostic. The search is unbounded now — but unbounded alone
  traded the false negative for a false **positive**: with nothing stopping it, the `)` of a *call*
  pairs with the `{` of a lambda in a later argument, which happens in this tree at
  `src/PluginEditor.cpp` (`juce::PopupMenu::Options()`, brace 759 characters later, inert only
  because `callees()` drops `::`-qualified names). So the region between `)` and `{` is now checked
  for what it is: a declarator tail may carry qualifiers, `noexcept`, attributes, a trailing return
  type or a member-initialiser list, but a comma at bracket depth zero — outside an initialiser list
  — means an argument list, not a signature. That rejects 38 candidate positions under 17 names
  (`abs`, `memcmp`, `jmax`, `std::move`, base-class initialisers, and the `Options` pairing). Two of
  those names, `isPresetExcluded` and `getValue`, *are* first-party — what is rejected is a call to
  them, and their real definitions stay indexed. (An earlier draft of this entry said "24 further
  call sites, every one verified to have no first-party definition"; both figures were measured
  loosely and the second framed the wrong property. The claim that matters is that no rejected
  position is a definition.) Verified identical afterwards where it counts: **61 reachable body
  spans**, **44 files**, **0 violations**, zero files differing from before the round.

**The citation tool was disabling its own repair path.** `verify_reaim_targets()` ran first and
returned 2 for *every* mode, so one drifted `DELIBERATE_REAIMS` declaration switched `--fix` off for
the **whole run** — including every citation that had nothing to do with it. Declared anchors drift
for the most ordinary of reasons: two are single lines in `build.yml`, so any insertion above one
moves them. Measured on a worktree at `33333fe`, with one line inserted into `build.yml` and one into
`AnamorphEngine.cpp`: `--fix` re-anchored **0** citations across **0** documents; after the fix, **31**
across **16**. The distinction is now by mode — the same one the rewriter already draws for
invalidated declarations: `--check` verifies and stops; `--fix` warns, repairs, and still exits
non-zero, because a declaration lives in the script and no document rewrite reaches it. Verification
is not weakened: CI runs `--check` only.

**What `--fix` deliberately does NOT do**, recorded because the first draft of this round claimed
more than it delivered. It cannot re-anchor the misaimed anchor *itself* while the declaration is
live — `is_declared_reaim` excuses precisely that anchor from the comparison — so the replacement
spelling is computed only for a declaration whose base and current spellings already agree. And the
document that **owns** a misaimed declaration is now withheld from the rewrite entirely: re-deriving
one anchor while its excused neighbour stays put produced a cell asserting that
`build.yml:2266` was both the `macos-intel` job header *and* the last line of that job's rationale
block. The run-wide hard stop had been acting as an interlock against exactly that; removing it
without replacing it would have traded a blocked repair for a corrupted document, on the one lint in
the repository that **writes**. The interlock is now per document, so the other fifteen are still
repaired.

**Ten evidence anchors, not the three the review named — and the first sweep found only six.**
`CMakeLists.txt` took **178 insertions and one deletion** in this change set (net +177). The shift
map is +16 for base lines 29–105, **+55 for base 106–107** — base 107 was *rewritten* into head
162–163 rather than shifted, which is the one case a single figure cannot express — and +56 from base
line 108 on. The **bare continuation** form (`:NNN` following a `path:NNN` citation) is not tracked
by `check-citations.py`, so these moved with nothing watching; demonstrated by mutating two of them
to `:900-901`, past the end of a 542-line file, and watching `--check` still exit 0 while mutating
the tracked anchor on the line above failed it.

Corrected: `ADR-0001:31` `:314-323` → `:370-379`; `REPOSITORY_MAP.md:158` `:188`/`:214` →
`:244`/`:270`; `PRIVACY.md:21` `:310-311`/`:348-349` → `:366-367`/`:404-405`; `PRIVACY.md:22`
`:107`/`:92` → `:162`/`:108`; `ADR-0023:151` `:218` → `:274`; `BUILD.md:19` `:36-38`/`:47-55` →
`:52-54`/`:63-71`; `ADR-0022:70` and `ADR-0026:82` `:47-55` → `:63-71`. Three were named by the
review, two more found by the first sweep (`PRIVACY.md`'s `:92`, `ADR-0023`'s `:218`), and **four by
the second** — including two on `BUILD.md:19`, eighty-seven lines above the line the first sweep
edited. That is the sharpest lesson available here: a sweep is worth only as much as its next pass,
because the form being swept for is the one the gate cannot see.

`BUILD.md:106` was re-anchored to the span that `origin/main`'s `:274-284` maps to under the +56
shift, restoring the coverage recorded below as deliberate — one anchor over *both* the
`set_source_files_properties` pair and the `target_compile_definitions(Anamorph PUBLIC …)` block the
sentence enumerates. Because it is exactly that mapped span, the merge-base run computes and verifies
it with **no declaration at all**, and the permanent `DELIBERATE_REAIMS` entry an intervening draft
had added is deleted. An entry survives only for the one transition CI's *previous-push* base needs,
since that push carried a span one line wider — the block's closing `)`, which bought no evidence.
That one is marked for deletion as soon as the default branch catches up.

**One statement, rather than an anchor, was made wrong by this PR.** `PRIVACY.md:21` claimed the
webview/curl definitions cover "every target — the plug-in and both test binaries", citing three
sites. The bench, DSP-dump and fuzz executables added in this change set each carry the same pair, so
the sentence and its evidence now name all six. Two further citations are *incomplete* in the same
way and deliberately left — `CODE_STYLE.md:10` and `TESTING_POLICY.md:9` cite three of six
`juce_recommended_warning_flags` sites, but each cited line is correct and neither sentence claims to
be exhaustive.

**The continuation gap is left open deliberately.** Bringing these under the gate means the
comma-list spelling (`CMakeLists.txt:496-497, 527-528, 569-570`), which the tool does accept — but
each continuation here carries its own annotation naming *which* line it is (`:162`
(`-Wl,-dead_strip`, Apple), `:108` (`/OPT:REF`, MSVC)), and the comma-list form has nowhere to put
them. Widening the recogniser to follow continuations is a change to the gate's scope rather than to
this round's defect, and would newly track dozens of anchors across the tree at once. Recorded here
so the next reader knows it was weighed rather than missed.

**Environment-assertion placement round (2026-08-19): the same defect the macOS dSYM gate had, in the
two assertions added beside it.**

**An assertion about the ENVIRONMENT must not cost the run its evidence.** The Linux ABI floor and
the Windows MSVC toolset check were both added in the round that fixed exactly this shape for the
macOS dSYM gate, and both were added with the shape unfixed. The Linux floor sat between the strip
step and `DSP + state self-tests`, so a breach left `steps.tests` at `skipped` — and both Linux
uploads gate on that being `success`. The Windows toolset check sat between `Configure` and `Build`,
so a mismatch skipped the build, both suites, both pluginval gates, the installer and every upload.

In both cases the trigger is a **runner-image move**, which is the case where withholding the
evidence helps least: a silent image change would have produced a red run with no test results and
nothing to install, i.e. no way to tell whether anything *else* was wrong at the same time.

Both are now their job's **last step**, matching the macOS precedent. Neither is weakened: the
bodies are byte-identical (verified by comparing every step's `run`/`shell`/`with`/`env` against the
previous commit — zero differ), and release eligibility is untouched, because `release.yml` depends
on this workflow's **aggregate** result, which a failed job decides wherever in the job it failed.
Each is gated on the step whose output it reads rather than on "everything succeeded": the Linux
floor on `strip` (it checks the stripped bytes, and a failed build leaves `strip` skipped anyway),
the Windows toolset on `configure` (it reads `CMakeCache.txt`) — deliberately not on the build, so
the toolset is still recorded when a build fails, which is when knowing the compiler is most useful.

**Verified structurally rather than by reading the diff.** A script compared every job's `needs`,
`if` and `runs-on` and every step's id/condition before and after: no job-level change anywhere, no
step added or removed, every artifact-upload gate byte-identical, and the `macos` job not appearing
in the comparison at all — its dSYM gate, its LTO assertion and the `-Wl,-object_path_lto`
configuration are untouched. The moved Linux step was then executed from the workflow file itself:
rc 0 against the real artifacts at the declared floor, rc 1 with the floor lowered to Ubuntu 22.04's
2.35.

One presentational consequence, not a separate change: moving the ABI block out from between the
`MALLOC_PERTURB_` rationale and the `DSP + state self-tests` step it documents restored their
adjacency.

**Follow-up review round (2026-08-19): a mandatory gate placed where it could withhold the product,
and a corpus that had quietly grown to 21× its documented size.**

**A debug-symbol failure must not cost macOS users their build.** Making zero usable dSYMs an error
was right; raising it *inside* the packaging step was not. The `exit 1` sat between the `strip -x`
loop and the customer half of the same step — codesign, the copy into `dist/`, the universal-slice
assertion and the entry-point check — so a symbol-capture regression would have left
`dist/Anamorph-macOS` unpopulated, skipped the plug-in upload, the `.pkg` and the draft release.
macOS users would have got **no build at all** rather than a build without symbols, which is the
opposite of the invariant stated three lines above it in the same comment block: the customer
pipeline is never blocked by debug-capture problems.

The assertion is now the macOS job's **final step**. The packaging step records the count into
`debug_artifacts` where it already did, at the moment it is known; the judgement happens after
packaging, both pluginval gates, the installer and all three uploads. **Release eligibility is
unchanged** — `release.yml` depends on this workflow's aggregate result, which a failed job decides
regardless of where in the job it failed — so the gate is exactly as mandatory as before, and a red
run now hands the reader the plug-in, the installer *and* the failure instead of nothing. Verified by
extracting both shell bodies and running them: with `USABLE_DSYMS=0` the packaging step reaches the
customer pipeline (rc 0) and writes `debug_artifacts=false`, and the new step exits 1 on `false` and
0 on `true`.

**The fuzz corpus was documented as three seeds and contained 65 files.** The documentation was
right and the tree was wrong, which the naming makes unambiguous: three `*.bin` files named after the
three legacy XML fixtures they derive from, and 62 files named after the SHA-1 of their contents —
libFuzzer's own convention for inputs *it* discovered. `AnamorphFuzzState tests/fuzz-corpus` passes
that directory as libFuzzer's first positional argument, which makes it the corpus libFuzzer reads
seeds from **and writes discoveries into**. On a CI checkout that is harmless. Locally it writes into
the working tree, and a `git add -A` committed them — in the very round whose documentation said the
corpus held three.

The 62 are removed and the root `.gitignore` tracks `tests/fuzz-corpus/*.bin` only, so a discovery cannot
be committed by accident while a genuine new seed still can be, deliberately. The rule is at the root
rather than in the directory because a `.gitignore` inside it is itself read as a corpus entry — measured,
libFuzzer reported `files: 4` — which would leave the directory not matching the count every document
states. Verified: a SHA-1-named file is ignored (`git check-ignore` names the rule), a `.bin` file is still
offered as untracked, and the gate runs from `files: 3`. The
three documents the review named keep their counts, which were correct; what they were missing is
that the directory is a libFuzzer *output* as well as an input, so each now says so — including
`TESTING.md`, whose local run command is the one that writes into a real working tree.

**macOS-symbolication and review round (2026-08-18): the one platform contract the previous round
could only correct is now closed, and three validation mechanisms were found to be checking less
than they claimed.**

**macOS crash symbolication exists.** The previous round could only correct the claim; this one
fixes the thing. `dsymutil` does not read DWARF out of a linked binary — it reads the debug map,
walks back to each object the linker consumed, and pulls DWARF from there. Under `-flto` ld64 does
codegen itself, writes the merged result to a temporary object and deletes it at the end of the
link, so the `N_OSO` entry named a path that was gone: `dsymutil` warned, exited 0, and emitted a
dSYM with no usable DWARF, which the packaging step correctly discarded on **every** run.
`-Wl,-object_path_lto` tells ld64 to keep that object. It changes where a temporary is written and
nothing else — the bitcode, the codegen and the shipped bytes are untouched, and **LTO is still
LTO**, which is not a detail: turning it off is the workaround this fix exists to avoid. A directory
rather than a file, because a universal build links once per architecture and one path would have
the second slice overwrite the first.

**Verified on macOS, not in source.** The build now asserts the retained-object directory is
non-empty before `dsymutil` runs — a direct witness that ld64 did LTO codegen, since it writes there
only then. The landing run produced three UUID-matched dSYMs and a **53 MB `Anamorph-macOS-debug`
artifact** where every previous run produced none. Zero usable dSYMs is now an **error** rather than
a warning: while the zero was an expected property of the build a warning was right, and now that the
build produces them a zero means something broke.

**`tests/AllocationGuard.h` would not have compiled on macOS forever.** It calls the over-aligned
allocator on every platform, and both macOS jobs configure with a 10.13 deployment target; C11
`aligned_alloc` arrived in the macOS runtime at 10.15, and libc++ honours that window by not
declaring `std::aligned_alloc` below it. `posix_memalign` has been there since 10.6, carries no
availability attribute and returns ordinary `free`-able memory, so `alignedFree` is unchanged. The
deployment target is deliberately **not** raised: 10.13 is the compatibility claim, and a test header
is not the place to move it.

**The realtime lint did not recognise this project's own allocations.** `.assign` is the idiom every
DSP module uses — `REALTIME_SAFETY_AUDIT` names it as such — and `make_unique` is how the engine
allocates its oversamplers. Neither was in the forbidden set, so the likeliest regression of all, a
line copied out of `prepare()` into a `reset()` or `process()`, was invisible to the one tier that
reads code the suites never execute.

Its scope was narrower than its subject too. Only functions whose own NAME matched the Policy list
were scanned, so `AnamorphEngine::updateDerived()` (run at the bottom of a switch duck) and
`VelvetNoise::updateWeights()` (run per block while the density glide moves) were audio-thread code
excluded by what they happened to be called. The scanned set is now the **reachable** set: seeds plus
every callee defined in the same file, transitively. **35 bodies became 61**, still 0 violations. A
hand-maintained list of extra names would have had the same defect one refactor later.

That closure needed a real definition test, and building one found a defect in the old one: the
`{`-before-`;` rule reads `if (isModAlgorithm (x)) {` as a definition, because a call in an `if`
condition is also followed by a brace. Measured on the real tree, that attributed **11 legitimate
`prepare()` allocations to a predicate**. The discriminator is what precedes the name.

**The lexer could swallow a real violation.** A raw string literal and a C++14 digit separator each
leave an unbalanced quote under a line-oriented scan, which blanks to the next quote anywhere in the
file. The danger is not a false finding but a true one hidden, so both self-test cases are MUST_FIRE
— an allocation placed after the construct, which must remain visible — and both were confirmed to
report zero under the previous stripper before being accepted.

**A DECLARED RE-AIM IS NOW A CHECKED CLAIM, and this is the round's most transferable lesson.**
`DELIBERATE_REAIMS` switches the drift comparison off for one anchor. It must — a deliberate re-aim
is textually indistinguishable from drift — but nothing then checked the aim, and the tool said so
in as many words ("verify the aim by hand, not by this tool"). Four anchors declared in the previous
round were computed before the workflow file settled and never re-derived: `build.yml:562, 1186,
1608` claimed the three per-OS Configure steps and landed on a composite-action `uses:`, a comment
inside the valgrind rationale and an `if-no-files-found:` key; `1775-1777` claimed the three
`codesign` calls and landed on a step header; two 400-line spans claimed the `macos` and
`macos-intel` jobs and landed in a PowerShell PDB helper and an artifact-upload line. All four were
green, because the declaration is precisely what stopped the comparison that would have caught them.

Each entry now records **what a reader should find there**, and `verify_reaim_targets` resolves the
anchor against the current file on every run. It found **two more** misaimed anchors within a minute
of existing — both `CMakeLists.txt` spans that the previous round had re-spelled mechanically. The
two job spans became job HEADER lines: one line, one unambiguous token (`macos:`, `macos-intel:`),
which identifies a job more precisely than a 400-line range ever did and is verifiable forever. A
substring rather than a range because the claim is "this identifies the codesign calls", not "these
are bytes 3-40", and a substring survives the reformatting that line numbers do not.

**`ANAMORPH_NONBLOCKING` is now on the definition as well as the declaration.** Clang's
redeclaration merge already carried the effect to the body — the seeded exit-43 catch proves it —
but that is a compiler behaviour to lean on rather than a property of the code,
`-Wfunction-effect-redeclarations` exists because Clang reserves the right to diagnose the split, and
a reader of the `.cpp` saw no sign the body was under a contract.

**Test 38 stopped degrading silently.** Its malloc probe now escapes through the volatile sink the
other two probes already used. Measured honestly, that is hardening rather than a bug fix: on g++-13
at `-O3 -flto` the unescaped pair survives, because this translation unit defines `malloc` and the
compiler therefore stops treating the call as the builtin `-fallocation-dce` relies on — an accident
of the configuration, not a guarantee. The substantive half is that the test now distinguishes "the
malloc half is not **compiled in**" (legitimate on MSVC, macOS and under ASan: warn and assert less)
from "compiled in and not observing its own probe" (broken: fail). Both printed the same warning
before.

**Second batch of the same round (2026-08-18): supply-chain pinning, duplication that was a
correctness hazard, two platform contracts that were claims rather than measurements, the
dependency-bump instrument, and one measured refusal.**

**Every action ref is now a commit SHA, and the argument is an internal inconsistency rather than
generic hygiene.** This repository pins JUCE to an immutable commit SHA and `DEPENDENCY_POLICY`
gives the reason in as many words — so the dependency cannot silently change under a re-pointed
tag. JUCE is source that gets compiled and never sees a credential; an action is code that executes
**on the runner with the job's token**. `actions/checkout@v7` was a mutable tag, resolving at the
time of the change to the same commit as `v7.0.1` with nothing but the tag owner's restraint keeping
it there. All 40 `uses:` across the five workflows now carry a SHA with the version in a trailing
comment. The cost is stated in `.github/dependabot.yml` rather than discovered later: a bare major
is rewritten only when the major moves, a SHA pin on every release, so four more `actions/*`
dependencies now generate updates — absorbed by the update-type groups that were already there.

**Seven copies of a policy is a policy that can hold in six places.** The Linux jobs each opened
with `setup-linux.sh` plus a ccache install behind a fallback that must not fail the job, and six of
the seven copies were byte-identical including the comment. `.github/actions/setup-linux-build`
collapses them behind two fail-closed inputs. The worked example is this round itself: it had to add
a compiler pin to exactly one of the seven. What the action deliberately does **not** absorb is the
per-job cache lineage — that is the part genuinely different in every job, and folding seven
readable explanations into one parameter would be worse than the duplication.

**The Linux compatibility claim was a statement about CI, and is now a measurement.** A Linux binary
records the oldest version providing each imported symbol; the maximum is the oldest system that can
load it at all. Measured: **GLIBC_2.38** and **GLIBCXX_3.4.31** — Ubuntu 23.10+ and GCC 13+ — so the
shipped VST3 does **not** load on Ubuntu 22.04 LTS. Nobody decided that; `ubuntu-latest` moving to
24.04 did it, silently and retroactively, with no failure in CI and no line in any diff.
`scripts/check-linux-abi.py` declares the floor and gates it on what the strip produced, as the `linux`
job's last step.
It does not attempt to *lower* the floor — that is an older toolchain or a sysroot, a release-topology
decision — it makes the run that raises it the run that fails. Its self-test covers the ordering trap
that makes the naive form wrong (2.38 outranks 2.9 numerically, not lexically) and treats "no version
references found" as an error, since that is what a mis-invoked `objdump` looks like.

**The Windows toolchain is recorded, and only its ABI series is asserted.** `windows-latest` floats
and MSVC is auto-detected, so a released `.vst3` could not be traced to the compiler that made it.
The assertion is deliberately narrow: every 14.x toolset since VS2015 is binary compatible and needs
the same redistributable, so gating on the exact version would fail on updates that change nothing a
user can observe. A cache the step cannot read is a `::warning::`, never a failure — a reporting step
must not decide whether a release ships.

**The macOS symbolication claim was false and is corrected.** `RELEASE_HARDENING_PLAN` read as
though symbol retention closed on all three platforms and "symbolication now possible" lowered RH-R8
to Low. True on Linux and Windows; on macOS not partially delivered but **never** delivered, because
`juce_recommended_lto_flags` is linked into the shipped target and under Release+LTO the DWARF lives
in ld64's temporary LTO object, deleted before `dsymutil` runs. The packaging step correctly discards
the empty dSYM — it validates the OUTPUT, not the warning text — and skips the upload every run.
macOS is the platform whose users most often submit an OS crash log. The fix is named precisely
(`-Wl,-object_path_lto=<dir>`, which moves where a temporary is written and not what is linked) and
left open rather than applied blind: it is a shipped-link change on the release job that cannot be
validated anywhere but macOS.

**`DEPENDENCY_POLICY` rule 2 finally has its instrument in the tree.** Bit-identical engine output
is the gate a JUCE bump must pass; two bumps passed it, and neither left the tool behind. So the gate
was permanent and the instrument disposable, and each bump rebuilt it — which is how the first run of
the original scratchpad tool shipped a scenario set that left `algoAmount` at its identity default,
hashed all four algorithms the same, and reported a confident nothing. `tests/dsp_dump.cpp` therefore
checks its own scenarios every run and exits 3 rather than print a table it has not shown to be
discriminating: all 32 must be repeatable **and** distinct. Verified both ways — 32 pass, two
consecutive runs are byte-identical, and restoring `algoAmount = 0` reproduces the 2026 defect
exactly (16 colliding pairs, named, exit 3). Nothing is stored: a committed set of expected hashes
would be the golden-master baseline this repository rejects, and the question is never "does this
match a stored value" but "does build A match build B".

**The fuzz gate got a fixed seed, because it is release-blocking.** A release must not be able to
fail on a lottery. The residual nondeterminism is documented instead of denied: `-max_total_time` is
wall-clock, so the tail is machine-dependent (measured 792 vs 807 executions across two identical
local runs), and `-runs=N` would trade that for a machine-dependent *duration*, which on a release
gate is the worse failure. A finding is reproduced from the uploaded artifact, which is exact.

**The citation gate now reports the declarations its own `--fix` invalidates.** A
`DELIBERATE_REAIMS` entry is a claim about a spelling, and a re-anchor can falsify it when the anchor
drifts for an unrelated reason. Section 9 of the self-test already fails on the result and that gate
holds — but it fires in CI, minutes later, in a different job, knowing only that an entry is dead,
while `--fix` killed it and is holding the replacement. Observed twice in this change set. Verified
live: shifting `run-pluginval.sh` by one line produced the warning with
`update it to scripts/run-pluginval.sh:122`.

**One approved item was measured and NOT implemented, and the measurement is the reason.** Caching
the JUCE checkout would save at most the JUCE clone's share of a **23–40 s** `Configure` step (two
observed runs), against an `Install build dependencies` step that varied **92 s to 576 s** between
those same two runs — the apt install is the dominant per-job cost by an order of magnitude, and the
clone is a fraction of a step an order of magnitude smaller. The cost side is concrete rather than
theoretical: ~100 MB per job across ~11 jobs is ~1.1 GB competing for the repository's 10 GB Actions
cache quota with the ccache lineages that already save minutes each, and GitHub evicts
least-recently-used. Spending a resource that saves minutes to buy seconds is a net loss, so the item
is declined with its numbers rather than implemented for completeness. The apt step is the target the
evidence actually points at, and reducing it means questioning build dependencies (the
`libwebkit2gtk-4.1-dev` tree, given `JUCE_WEB_BROWSER=0`) — a release-relevant change that needs its
own evaluation, not a footnote in a CI round.

**Review round (2026-08-18): the allocation guard was blinding the RealtimeSanitizer lane, and the
static lint's scope was narrower than the policy it enforces. Six confirmed findings, maintainer
approved; no new ADR — ADR-0029's evidence is corrected in place.**

**The most serious finding is that one new tier disabled another.** RTSan detects allocations by
intercepting the allocation entry points, and a definition in the program's own object files takes
precedence over the interceptor in the sanitizer runtime archive. The guard's `malloc` — and its
`operator new`, which routes through `std::malloc` — therefore reached glibc without ever passing
RTSan. Measured on the real suite with one escaping `malloc` seeded into `AnamorphEngine::process`:
guard compiled in → **RTSan reports 0, exit 1** (only the guard's own assertion); guard compiled out
→ RTSan reports the malloc at `AnamorphEngine.cpp:668`, **exit 43**. The liveness canary could not
have caught it, being compiled standalone without the guard: it kept proving the lane was live while
the binary it vouches for had lost its allocation detection.

Two measurement traps had to be cleared to see this at all, both the same class ADR-0029 §5 already
documents. A first seeded `juce::AudioBuffer` looked like a successful detection, but the report
named **`free`**, not `malloc` — the guard leaves `free` to RTSan, so the *deallocation* was caught
while the allocation was invisible; only a seed that allocates without freeing inside `process`
isolates the question. And a first "escaping" seed was **elided by the optimizer** (`objdump`: zero
`malloc` call sites in the engine object), so it proved nothing until an `asm volatile` barrier kept
it. The fix is `__has_feature(realtime_sanitizer)` in the guard itself rather than a flag on the
job, so a local `-fsanitize=realtime` build cannot reintroduce the conflict by forgetting it.

**The static lint enforced a narrower scope than the policy it cites.** The Policy binds
"`processBlock` / `AnamorphEngine::process` and every DSP module's `process`/`reset`", but the lint
scanned `src/dsp` only — omitting `AnamorphAudioProcessor::processBlock`, the first function named —
and listed `reset` in an exemption set. That exemption was also *dead code*: the extractor only
yields functions matching its audio-path regex, which never contained `reset`, so the eight module
reset bodies were never scanned while a self-test case asserted that allocation there was permitted.
Scope is now the Policy's scope (`src`, plus `reset`/`softReset` — the latter is called from
`process` at `AnamorphEngine.cpp:701`), the exemption list is gone rather than fixed, and coverage
went from **7 scanned bodies to 35**. The tree stays at 0 violations; seeded violations in a module
`reset` and in the wrapper `processBlock` are both caught at the exact line.

**Two smaller corrections.** The guard gained the **C++17 over-aligned** `operator new`/`delete`
family, paired to `aligned_alloc`/`std::free` or `_aligned_malloc`/`_aligned_free` — the route that
matters most on MSVC and macOS, where the malloc half never compiles in and JUCE's SIMD types are
over-aligned. Its self-check probes it separately (a live plain `operator new` does not imply a live
aligned one) and needed the same escape-the-optimizer treatment: `new`/`delete` of a non-escaping
object is elidable under C++14 and was being elided at `-O3`, reporting the half dead while it
worked. And the header's configuration table said the guard was live under valgrind while the
paragraph below it and the workflow both said the opposite; the table was the stale half.

**Counts re-derived from the binaries rather than from the review:** 37 DSP tests + the A/B guard,
**160** checks (the aligned-liveness assertion is the 160th), 13 state tests, 900 checks. The RTSan
build reports **156** — Test 38's assertions stand down there by design, which the run discloses.

**Allocation guard + static realtime lint (2026-08-18): ADR-0029's three tiers are complete. Plus
two review corrections. No new ADR — the decision was already made and this is its implementation.**

**The guard is the tier that reaches MSVC.** RTSan is the strongest realtime tool here and the least
portable: Clang, Linux/macOS only, while the shipped Windows binary is MSVC's. `operator new`
replacement is standard C++ on every conforming implementation, so `tests/AllocationGuard.h` +
Test 38 count allocations while `process()` runs and assert zero over **3,840 armed calls** across
the algorithm × oversampling × M/S matrix. The split between the two counters is load-bearing rather
than cosmetic: measured on this project a single `prepare()` allocates **102** times through
`operator new` and **663** through the malloc family, because JUCE's `AudioBuffer`/`HeapBlock` take
the raw-malloc route — so an `operator new`-only guard would miss the allocation JUCE actually
performs most. Both classes were seeded into the real `AnamorphEngine::process` and caught: a
`juce::AudioBuffer` (`malloc=1`) and a `std::vector` growth (`new=1`), each failing the suite exit 1.

**Two of ADR-0029's own predictions about the guard were wrong, and both are corrected in place.**
(1) It anticipated a review-gated CMake change; none was needed — the guard is a header included by
`dsp_tests.cpp`, and the one build that must exclude it does so through a compile flag on that job's
existing configure line. (2) It attributed the valgrind hazard to `vgpreload` replacing the
interposers. The real mechanism is that memcheck tracks which allocator produced each block and
intercepts the `new`/`delete` and `malloc`/`free` families **separately**, so an `operator new`
returning `std::malloc` memory reads as *"Mismatched free() / delete []"* on every later delete.
That distinction mattered practically: a small standalone probe does **not** reproduce it and
reported 0 errors, and only running the real JUCE-linked suite under the pipeline's exact invocation
exposed it. The ASan hazard was confirmed as written.

**Four configurations of the same suite, measured, because "no allocations" and "nothing was
counting" print identically.** GCC Release and RTSan: both halves live, 0 allocations. ASan: malloc
half compiled out (an exe-defined `malloc` fights ASan's allocator), `operator new` half still
asserting. valgrind: the whole guard compiled out via `-DANAMORPH_NO_ALLOC_GUARD`, memcheck 0
errors. Every inactive half is announced with a `::warning::` and the assertion skipped — never a
silent pass.

**The static lint earns its place by covering what neither runtime tier can.** RTSan and the guard
are runtime tools: they see only the code the suite executes, and measured coverage of `src/dsp` is
93.4 % of lines / 79.9 % of branches. `scripts/check-realtime.py` reads the branches the suite never
takes, on every platform, with no build. It is **function-scoped**, and that is the whole design —
the eight `setSize` calls in `AnamorphEngine.cpp` are all inside `prepare()`, where the policy
*requires* allocation, so a file-wide token scan would flag legitimate code and be switched off.
Comments and string literals are blanked before matching (both shapes exist in this tree: a
"the new-cutoff bank" comment, and diagnostic strings). Self-test: 19 cases both directions; real
tree: 23 files, **0 violations**; a `new` seeded into the real `process()` is reported at the exact
line.

**Review corrections in the same change set.** (1) `THREAD_MODEL.md` contradicted itself: its
Threads-section evidence line had been corrected to `:672` / `:675-681` / `:295-309` while five
other references still pointed at pre-growth locations (`:246-256` for the OpenGL gate, `:1151-1152`
for `triggerRepaint`, `:616-622`, `:613,917-1003`, `:627-632`). All five re-derived from the source
and verified line by line; the two table rows were path-qualified so the citation gate tracks them
from here on. (2) The claim that the new lanes "cannot withhold an artifact" was **false for
releases**: `release.yml` calls `build.yml` as one job and `draft-release` is
`needs: [validate, build]`, so a called workflow's aggregate result is what that edge observes and
any job here skips the draft release. The per-push artifact statement was true and is kept; the
release scope is now stated beside it in `build.yml`, `CI_CD.md` and ADR-0029's Consequences. No
workflow was redesigned — `RELEASE_POLICY.md` §Artifacts already says the `build.yml` gates are
reused unchanged, so the behaviour was correct and only the wording was not.

**Realtime-enforcement strategy, ADR-0029 (2026-08-18): the Priority-1 policy acquires its first
mechanical detector. Plus the PR review cleanup that preceded it. `Accepted` on maintainer approval;
amends no Policy — it implements enforcement for one that already existed.**

**`REALTIME_AUDIO_POLICY` had no gate, and the tools already in the pipeline structurally cannot
provide one.** ASan finds out-of-bounds and lifetime bugs, UBSan finds undefined behaviour, valgrind
finds uninitialised reads; a `malloc` added to `AnamorphEngine::process` is, to every one of them, a
perfectly correct allocation. RealtimeSanitizer is the only tool here that asks *where* it happened.
`AnamorphEngine::process` now carries `ANAMORPH_NONBLOCKING` and the new `realtime` job builds the
DSP suite with `-fsanitize=realtime` and runs it. Demonstrated both ways before landing: the whole
156-check matrix runs violation-free (1.6 s), and a seeded `juce::AudioBuffer` allocation inside
`process` fails the run at **exit 43**, naming `AnamorphEngine.cpp:664` through
`juce_HeapBlock.h:356` — a useful diagnostic, not just a non-zero exit.

**Four decisions in the ADR are refusals, each on measurement rather than taste** — and the first of
them has since been narrowed by a further measurement rather than reversed. `-Wfunction-effects` is
NOT enabled **on the audio path**: the annotated engine TU emits **52** warnings, dominated by JUCE
calls whose definitions the TU cannot see (`Oversampling::reset` ×9, `FloatVectorOperations::copy`
×6), and JUCE 9.0.1 carries **zero** annotations of its own — the warnings are about correct code.
Every one of those 52 is transitive through JUCE, so they appear only where JUCE does: over the
**JUCE-free leaf layer** the same flag emits **0**, and it still fires precisely (the seeded
`ANAMORPH_EFFECTS_CANARY` call to an allocating helper fails by name; this entry first named
`anamorph::applyWidth`, whose visible `inline` definition makes it inferred-clean instead). It is
therefore enabled there, as
`-Werror=function-effects -fsyntax-only` over `tests/realtime_effects.cpp` in the `realtime` job —
compile-only, seconds, no target added to the shipped build. That is the scoping recorded in
ADR-0029 §3; the refusal for the engine TU stands unchanged.
`-Wperf-constraint-implies-noexcept` is NOT enabled either, for the opposite reason: it fires on
definitions whose effects imply `noexcept`, and the entry point is already `noexcept`, so it would
gate on nothing. RTSan is NOT folded into the `sanitizers` job — the clang driver *rejects*
`realtime` alongside `address`, `undefined`, that job's actual `address,undefined,vptr` set, or
`thread`. And the lane sets **no `RTSAN_OPTIONS`**: `halt_on_error=false` makes the process print its
violation reports and still exit **0**, which is the gate-that-cannot-fail this repository's testing
policy is written against.

**The annotation is byte-identical in object code, which is what let it touch a frozen audio path.**
`src/dsp/AnamorphEngine.cpp` compiled by clang-22 at `-O3` with the project's flags produces the same
object with the attribute live and with the macro emptied. The spelling had to be corrected against
the compiler rather than from memory: it is a **type** attribute (after the parameter list), and the
prefix form is a hard error — *"'clang::nonblocking' attribute cannot be applied to a declaration"*.
The `__has_cpp_attribute` guard is what keeps GCC's `-Wattributes: scoped attribute directive
ignored` (2 lines, measured on the raw spelling) out of the Clang warning gate.

**The canary's first draft was defeated by its own error message, and running it caught that.**
`TESTING_POLICY` rule 4 demands a lane prove it can fail. The canary commits a real *escaping*
allocation — escaping because a non-escaping `malloc`/`free` pair is elided at `-O2` (measured: exit
0 instrumented at `-O2`, exit 43 at `-O0`), so the obvious canary passes while the tool works
perfectly. The workflow step then asserts a non-zero exit **and** the sanitizer's report. The first
draft grepped for the bare word `RealtimeSanitizer`, which the canary's own failure text contained,
so an uninstrumented build satisfied it — a dead lane reporting itself live. Found by running the
step's logic against a deliberately uninstrumented build before the job ever ran in CI; the message
no longer carries the token and the grep matches `ERROR: RealtimeSanitizer`.

**The cross-platform tier is scheduled, not shipped, and the ADR says so.** The allocation guard
(7,680 armed calls, zero allocations, both seeded classes caught) and the static realtime lint reach
where RTSan cannot — the `operator new` half works under **MSVC**, which RTSan never covers. They
are held back because the guard needs a CMake target-level change (review-gated Build System class)
and carries two demonstrated CI hazards: an exe-defined `malloc` **segfaults** under ASan, and
valgrind's `vgpreload` silently replaces the interposers. Landing three enforcement mechanisms at
once, two unproven in CI, is how a gate gets switched off.

**Review cleanup in the same change set.** Three findings, each verified against the tree before
acting: the CI job inventories in `CI_CD.md`, `TESTING.md` and `REPOSITORY_MAP.md` still said *four*
non-packaging jobs (now **six**, with the matrix/table rows and the ccache-lineage note synced); the
test counts in `REPOSITORY_MAP.md`, `RELEASE_HARDENING_PLAN.md` and `HANDOVER.md` still read
33/12/140/894 (**36/13/156/900** at that point in this change set, taken from the registrations in `main()` and a real run; Tests 38 and the aligned-guard check landed later in the same PR — the final figures are in the newest entry above);
and the wrapper test's RMS diagnostic said "240 blocks" while the accumulator only runs in the
120-block noise phase — corrected via a named `blocksPerPhase` constant so the literal cannot drift
from the loop again. Historical records were deliberately left alone: ADR evidence sections,
`DEPENDENCY_POLICY`'s compliance log and `CI_CD.md`'s Clang-22 verification paragraph state what a
past run measured, and those are append-only.

**Engineering-capability audit (2026-08-18): extended UBSan coverage, the LTO validation gap,
the wrapper audio path + three DSP feature-coverage tests, `preflight.sh`, and the realtime-doc
anchor rot. Six adopted improvements, every one demonstrated before landing; the RealtimeSanitizer
decision stays reserved for its own ADR (ADR-0028), as do all CMake-structure changes (gated).**

**The sanitizers job now checks five more UBSan groups, chosen by census rather than by list.**
`-fsanitize=undefined` does not contain `float-divide-by-zero`, `implicit-conversion`,
`unsigned-shift-base`, `local-bounds` or `nullability`; a census build with the candidates enabled
ran both suites and measured **zero** diagnostics under all five, so they gate for free. The full
`integer` group was measured too and is **deliberately absent**: its `unsigned-integer-overflow`
half fired 8 times on legal, intentional wraparound (JUCE string hash, `Random` LCG, `Thread` tick
arithmetic, libstdc++'s mersenne twister) and under `halt_on_error=1` would redden the job on
correct third-party code. One sharp edge is written beside the flags: `local-bounds` is trap-based
and can fail by SIGILL with no diagnostic text. `ASAN_OPTIONS` gains
`check_initialization_order=1:strict_init_order=1:strict_string_checks=1` (measured clean);
`detect_stack_use_after_return` is deliberately NOT written because it is clang-22's Linux default
— demonstrated, not assumed — and restating a default is the copy that rots. The `detect_leaks=0`
comment's factual half ("JUCE singleton teardown reports") was retested and is no longer true —
both suites run leak-clean under `detect_leaks=1` — and the flag has now been **flipped to `1`**, so
LeakSanitizer is a gate on this job rather than a suppressed detector with nothing left to suppress.
A leak in a plug-in process is a leak in a host that stays open for hours, so a report here is to be
investigated; the one place `detect_leaks=0` survives is the `fuzz` job, where the harness leaks
JUCE's `ScopedJuceInitialiser_GUI` deliberately (see below).

**The self-test suites had never executed link-time-optimized codegen, and the shipped binary is
nothing else.** The plugin alone links `juce_recommended_lto_flags`; both console suites
deliberately do not. The new `linux-lto-tests` job builds and runs both suites with the identical
plain `-flto` spelling on the same GCC that builds the shipped Linux artifact — via the CMake cache
variables, because linking the JUCE interface target into the test targets would be a
CMake-structure change and that class is review-gated (`ARCHITECTURE_REVIEW_GATE.md`). Demonstrated
before landing: the LTO build of both suites passes locally (275 s on 4 cores; the state-suite LTO
link dominates because plain `-flto` runs LTRANS serially — true of the shipped link too).

**The state suite gained the wrapper audio path, which makes an existing workflow comment true.**
`build.yml`'s valgrind rationale has long said the read that matters "runs through the real wrapper
`processBlock` — which only AnamorphStateTests drives"; measured 2026-08-18, `grep -c processBlock
tests/state_tests.cpp` was **0** — no test in either suite drove the wrapper's audio path, so the
sentence was aspirational. `testWrapperProcessBlockAudioPath` now drives the real
`AnamorphAudioProcessor::processBlock` over a denormal-provoking noise→silence matrix with **no
test-side FTZ arming**, regressing `processBlock`'s own `ScopedNoDenormals` (the DSP suite's own
denormal matrix arms FTZ in the harness, so it could never catch the wrapper losing its guard); a
liveness RMS check proves the invariant is not vacuously green. Three DSP feature gaps found by a
line/branch coverage run (93.4% lines / 79.9% branches over src/dsp, with `monoSum`, M/S input
solo and the Level-Match injection consume paths at zero) are closed by Tests 35–37 — Test 37's
first draft asserted a *frozen* injected trim and failed, which is how the actual contract
(injection is a SEED the measurement re-converges from, `LoudnessMatch.h:63-69`) got asserted
instead; the suites stood at **156 + 900 checks** after that round (were 140 + 894).

**The realtime documentation had rotted around code growth, and only the untracked spellings let
it.** `REALTIME_SAFETY_AUDIT.md` cited `processBlock` at `PluginProcessor.cpp:64-131` (actual:
117-185), the engine at `:26-113 vs :472-899` (actual: 28-113 vs 660-1339) and the NaN self-heal
at `:847-870` (actual: 1256-1290); `REALTIME_AUDIO_POLICY.md:34` and `THREAD_MODEL.md:16-17`
carried the same class. Every corrected anchor was measured against the tree, the previously BARE
filenames are now path-qualified so `check-citations.py` tracks them from here on, and the four
already-tracked corrections are declared in `DELIBERATE_REAIMS` (the tool rightly refuses a
re-aim it cannot map — its base text never moved; the aim was wrong, not drifted). The audit's
JUCE-oversampler TODO stays open but now carries the first measurement: an interposition probe
counted zero audio-path allocations through `Oversampling::processSamplesUp/Down` at ×2/×4/×8
across 7,680 armed process calls, with the expected `prepare()` allocations (102 `new` + 663
`malloc`) as the positive control.

**`scripts/preflight.sh` exists because this branch kept paying a CI round trip for lint verdicts
that cost 5 seconds locally.** It runs the four lints with their self-tests, the citation gate
against BOTH bases that can disagree (`origin/main` and the branch merge base), and the built test
suites — skipped WITH A NOTE when no built tree exists, never silently (the `build.sh` stale-binary
lesson). Measured: 5.5 s end to end on a built tree. It states its own limit out loud: the full
Clang warning gate needs a clang build log, so only that lint's self-test runs locally.

**Anchor bookkeeping for this round, same discipline as the last two.** The `sanitizers` job's
comments grew build.yml by 27 lines above `windows:` (the LTO job was appended at the END of the
file precisely so nothing below it exists to shift); the five tracked build.yml anchors were
recomputed by `--fix` against the push predecessor and read back by hand (`macos` `:1482-1942` →
`:1509-1969`, its rationale `:1943-1999` → `:1970-2026`, `macos-intel` `:2000-2245` →
`:2027-2272`, the codesign trio `:1713-1715` → `:1740-1742`, the per-OS Configure trio
`:546,1124,1546` → `:546, 1151, 1573`), and the matching `DELIBERATE_REAIMS` declarations were
updated in the same change — section 9 of `--self-test` failed 4 cases until they were, the third
time that assertion has caught exactly this.

**Dependency-update audit: Clang 18 → 22 (upstream stable, installed from apt.llvm.org), and
Dependabot split into two semver groups (0.9.4, no version bump). No `src/`, `tests/`,
`CMakeLists.txt` or packaging change; no shipped byte changes. `.github/workflows/build.yml` +
`.github/dependabot.yml` + `scripts/setup-llvm-apt.sh` (new) +
`scripts/clang-warning-baseline.txt` + ADR-0028 + `ARCHITECTURE_REVIEW_GATE.md` + five documents.**

**The audit's first result was an inventory, and it is now written down.** `DEPENDENCY_POLICY.md`
listed four dependencies (JUCE, pluginval, the C++ standard, Linux system libs) and stopped there, so
the *pinned Clang major*, the *GitHub Actions refs* and the *runner images* — three externally
maintained things the pipeline is wired to — appeared in no dependency inventory at all, only in the
workflow that consumes them. All three are now rows in the table, and a new **§Update mechanisms**
says per category which mechanism maintains it **and why that one**: automation where an automated PR
would carry actionable information, hand-maintenance where it would not. That section is the answer to
"is anything missing from Dependabot", and the answer is no ecosystem is missing — of Dependabot's 33,
none reads CMake, `apt`, Homebrew, a `releases/latest` download or a workflow `env:` value, and its
only C/C++ ecosystem (`vcpkg`) wants a `vcpkg.json` this repository does not have.

**Clang 18 → 22, and the pin stays a MAJOR pin.** The rule this settles is *upstream stable*, not
*newest* and not *whatever the distribution packages*: 22.1.8 (2026-07-10) is the newest non-rc release
— 23.1.0 is still at rc3 — and apt.llvm.org itself asserts it, `CURRENT_LLVM_STABLE=22`. **An
intermediate step of this same change set went to 20 and was superseded before merge**, and the reason
is recorded rather than tidied away: that draft rejected 21/22 as having "no noble publication at all",
a conclusion drawn from checking **Ubuntu's** archive pockets, which is accurate, and then
over-generalised. apt.llvm.org publishes noble suites for **17 through 23** on `amd64 arm64 s390x`.
Needing a different install mechanism is an implementation cost, not an availability blocker.
The granularity is deliberate: `apt` names these packages by major (`clang-22`), and the baseline's own
invariant is that diagnostics are a property of the major, so a full-version pin would be both
inexpressible in the install and self-breaking on the first rebuild — the apt.llvm.org string is
`1:22.1.8~++20260714014902+ca7933e47d3a-…`, a snapshot of the 22.x branch head. The measured result is
that **nothing moved but the recorded version**: with clang-20 as the control, clang-22 emits a
`diff`-identical 52-instance warning census, and `--write-baseline` at 22 changes exactly one line,
`# clang-major:`. That census also matches Clang 18's, so this tree's accepted set is now stable across
**three** majors. Both suites pass under the clang-22 build (140 + 894) and again under its
ASan+UBSan+vptr build; the LTO `Anamorph_VST3` link and `check_linker_flag`'s lld probe are green at 22;
the `--compile-canary` still rejects the explicit `SIMDRegister` form; and the mismatched pin/baseline
pair was **observed** to exit 2 before regeneration, which is the guard doing its job rather than an
assumption about it.

**The install mechanism is a script, and it follows the version rather than choosing it.**
`scripts/setup-llvm-apt.sh <major>` adds one apt.llvm.org suite with a `signed-by=` keyring and installs
exactly `clang-N`, `lld-N`, `libclang-rt-N-dev`; the suite codename is read from `/etc/os-release` so a
future `ubuntu-latest` move cannot silently point it at the wrong one. Deliberately **not**
`apt.llvm.org/llvm.sh`, which decides the version itself and installs more broadly — that would move
the authority out of `ANAMORPH_CLANG_VERSION`. It is **fail-closed**, unlike the optional ccache install
beside it: the pinned Clang *is* the job, so a partial install stops it rather than letting the image's
default `clang` stand in and trip the baseline guard one step later, reading as a project problem. The
three *shipping* build jobs never touch apt.llvm.org.

**One behavioural change in the pipeline, and it preserves coverage rather than adding it.** Clang 21
removed `vptr` from the `-fsanitize=undefined` group, so the bare `address,undefined` the `sanitizers`
job carried would have silently stopped checking bad downcasts and bad vtables the moment the pin passed
20. It now names `-fsanitize=address,undefined,vptr`. Reproduced rather than taken from a release note:
one bad-downcast program, clang-20 reports it from `undefined` alone, clang-22 reports **nothing**, and
naming `vptr` restores it on 22. It needs RTTI, which this project never disables. This is the trap the
first draft of ADR-0028 had recorded as a warning for a future bump; the bump happened in the same
change set, so it is now implemented instead.

**Clang is not, and should not be, bot-updated — and that is a capability finding.** Dependabot parses
no workflow `env:` key in any ecosystem, so it cannot see `ANAMORPH_CLANG_VERSION` at all. Renovate
could match it with a `custom.regex` manager, and the PR would be **red by construction**:
`check-clang-warnings.py` exits 2 whenever the pin and `# clang-major:` disagree, and re-baselining
means building with the new compiler. Renovate was evaluated for the rest of the tree too and declined
on capability: it has **no CMake manager** (its only C/C++ manager is Conan), and its runner-label
support extracts `ubuntu-latest` and then discards it as a non-numeric version. Recorded in
`DEPENDENCY_POLICY.md` so the evaluation does not have to be repeated from scratch.

**Dependabot: one `*` group became two, split by semver impact.** `github/codeql-action` releases
every week or two, so nearly all the volume is minor/patch — and in a single combined PR one action
*major* (a changed Node runtime, input or output) blocks every safe bump behind it while the whole
9-job matrix re-runs when it is finally read. Both groups keep `patterns: "*"` deliberately: a family
spread over several refs (`codeql-action/{init,analyze,upload-sarif}` is three dependency names here)
would become three independent PRs once it fell out of a group, and `init` and `analyze` are two halves
of one run. `microsoft/msvc-code-analysis-action` is now **ignored**, which is protection rather than
an exclusion for its own sake: its SHA pin carries no tag, and GitHub's documented behaviour for an
untagged pin is to follow the **latest commit, not the latest release** — so the first push to that
upstream branch (whose HEAD is still the pinned commit, untouched since 2023-03-22) would propose
replacing a pin whose reason is documented in `msvc.yml` with an untagged HEAD of a third-party action
GitHub does not certify, and SHA-pinned actions raise no Dependabot security alerts either way.
`cooldown` was considered and left unset: Dependabot already withholds a new version for 3 days by
default, so an explicit block would restate it.

**ADR-0028 is `Accepted`, and it closes the governance question rather than recording it.** The pin
sits in `DEPENDENCY_POLICY.md`'s pinned-dependency table, which is the exact condition ADR-0027 cited
when it treated a one-line `CMAKE_CXX_STANDARD` edit as a gated **Build System change**. The first draft
of this ADR applied that reading and then left the countervailing precedent — the 2026-08-15 image move
that took the **shipping** macOS compiler from AppleClang 15 to 21 as a CI change with no ADR — as an
open tension. It is now resolved, by the repository's own instrument: per `ADR_POLICY.md` rule 5 and
`SOURCE_OF_TRUTH.md` ("An ADR may change a Policy, but only by an explicit new/updated ADR"), ADR-0028
carries an **`Amends:`** header and `ARCHITECTURE_REVIEW_GATE.md` now carries the rule — the same shape
ADR-0025 used to amend `TESTING_POLICY.md` rule 1.

**The discriminator is *who chooses the version*,** not which platform it is on and not whether the
compiler's output ships. A version **this repository pins** is gated (the Clang major; the C++
standard). A version the **runner image supplies** is not gated *because it cannot be* — GitHub
re-points `macos-latest` with no commit here, so a rule demanding review for AppleClang or MSVC is one
the repository is unable to obey; what it requires instead is detection and record, which is what the
warning baseline and `CI_CD.md` §Build matrix already do. **Pinning or unpinning a label** for a
toolchain that builds shipped artifacts *is* gated. AppleClang 15 → 21 is therefore reconciled by the
second rule rather than exempted from the first: no pinned version changed, because AppleClang was never
pinned, and the compiler that followed the `macos-14` → `macos-latest` move was GitHub's choice. Under
the third rule that *label* move would be gated today. The Policy states explicitly that numeric
symmetry between platforms is **not** required — Apple's version numbers are not upstream LLVM's, and
the two are chosen by different parties. None of the six `AI_AGENT_POLICY.md` Hard Stops is touched.

**Two drift items found while doing this, both corrected in place with the evidence.** (1) The
self-coverage table below counted **18 ADRs**; there were 22 before this change (0001–0015 + 0021–0027,
with 0016–0020 reserved by `RELEASE_HARDENING_PLAN` §8) and 23 with ADR-0028, so the figure was four
stale and now reads 23. (2) `CI_CD.md`'s ccache paragraph stated the `linux-clang` figure
(**5m48s → 2m36s**) in the present tense "against the pinned Clang 18"; that was a measurement of a
configuration the bump retires, so it now reads *then-pinned* and *was*, with the reason attached. The
same figure inside this file's own dated entry is left alone — historical entries are append-only.

**One defect found while implementing, not planned for.** The `sanitizers` ccache key did not carry
the Clang major (`linux-clang`'s did), so raising the pin would have left that job restoring an 800 MB
lineage of objects the new compiler can never hit — not a wrong answer, since ccache hashes the
compiler binary's contents, but a restore that buys nothing. Both jobs now key on the major, which is
what `CI_CD.md` §Cache lineages already claimed for one of them.

**`build.yml` grew by 14 lines across this change set, so the anchors below it were recomputed — and
the guard caught the part that was missed.** The 18 → 20 step held the file at net-zero line count
precisely to avoid this; the 22 step could not, because the pin rationale needed four more lines and the two Clang
jobs' install steps five more. Six anchor values across four documents were recomputed **from the file
as it stands and read back line by line, never shifted by arithmetic**:
`RELEASE_POLICY` `build.yml:542,1110,1532` → `:546,1124,1546`; `KNOWN_ISSUES` `:1699-1701` →
`:1713-1715`; `COMPATIBILITY_MATRIX` `:1468-1928` → `:1482-1942` and `:1986-2231` → `:2000-2245` with
its rationale block `:1929-1985` → `:1943-1999`; and `DEPENDENCY_POLICY`'s own `:74-76` → `:78-80`
(that last one alone sits above every insertion point, so it moved once and stayed). Those are the
values the tree carries: the Clang-22 commit landed them five lower, and the `vptr` commit's five added
lines moved them again — recomputed and re-read both times, which is the whole claim this paragraph
makes.
Updating the citations left the four matching `DELIBERATE_REAIMS` declarations naming spellings no
document contained any more, and **section 9 of `check-citations.py --self-test` failed 4 of 85 cases**
until they were updated — the second time that assertion has caught exactly this, and the reason it
exists. Two independent confirmations that the new anchors are right rather than merely consistent: the
drift check reports no re-spelling at all (the anchors resolve to the *same text* as the base, which is
the property it actually tests), and a deliberate mutation of one anchor to a wrong value made the gate
exit 1 while **independently computing the committed value** as the correct answer — `:1708-1710` at
the time, `:1713-1715` after the `vptr` commit shifted it, each time matching the hand-computation.

**The signing-key pin was presence-only, and review caught it.** `setup-llvm-apt.sh` asserted that the
expected fingerprint appeared *somewhere* in the dearmored keyring — but `signed-by=` trusts **every**
key in that file, so a served blob carrying the genuine key concatenated with another one satisfied the
grep and both became trusted for the suite. The comment beside it claimed the stronger guarantee ("the
key this repository decided to trust"), so the check was weaker than what it advertised. It now asserts
identity: the list of **primary** fingerprints must equal the pinned one exactly. It counts `pub:`
records rather than `fpr:` ones because the genuine key has two fingerprints — a primary and one subkey
— so an "exactly one fingerprint" test would have rejected the real key. Verified against crafted
keyrings rather than argued: genuine passes; genuine + a second generated key is **rejected naming both
primaries**; attacker-only, empty and corrupt keyrings are each rejected with the same specific message;
and the real script still installs clang 22.1.8 on a clean machine and is idempotent. Testing also
exposed a diagnostic regression in the first draft of the fix — under `set -e` a corrupt keyring killed
the shell at the command substitution before the assertion could report anything — so the substitution
carries `|| true` and lets the assertion do the failing.

**A seventh anchor turned out not to be checked at all, which review surfaced and measurement
confirmed.** `COMPATIBILITY_MATRIX`'s `macos-intel` row cites two ranges in one sentence, and the
second was written in the continuation spelling — `…build.yml:1995-2240 (the job), :1938-1994 (its
rationale block)`. The `CITATION` regex requires a path before the anchors, so the bare `:1938-1994`
matched nothing: the gate never saw it. Demonstrated rather than reasoned — replacing that value with
`:100-200` left `--check` at **exit 0** against both `origin/main` and the branch tip. It was *correct*
(1938–1994 was then the rationale block and 1995 the `macos-intel:` key; both have since moved with the
file, to `:1943-1999` and `2000`), just unguarded — and it had been unguarded since it was written, not
by anything this round did. The declaration a reviewer would reach
for is the wrong instrument and provably so: adding it to `DELIBERATE_REAIMS` fails section 9, because
that assertion requires the entry to name a string the document contains and the document contains no
such string. The fix is to give the anchor its path, which is the only spelling `classify()` resolves
(`build.yml` and `workflows/build.yml` both return `None`). It reads as a new citation for one
transition and is checked from the next base onward, like any new anchor.

**One of those six anchors was a re-aim, not a re-anchor, and only CI could see it.**
`DEPENDENCY_POLICY`'s Clang row cites the `env:` block, whose lines both **moved** (+4) and **changed**
(`ANAMORPH_CLANG_VERSION: 20` → `22`), so there is no base text to map from and `--fix` reports
UNMAPPABLE. The local run could not catch it: against `origin/main` that row does not exist yet, so the
citation reads as *new* and is skipped, while CI compares against the **push predecessor**, where it
does exist — both of the checker's escape hatches open at once, which is precisely the pair its own
header warns about. `source-lint` went red on exactly one citation out of 302, the aim was re-verified
by hand (`:78` `env:`, `:79` strictness, `:80` the Clang major) and declared in `DELIBERATE_REAIMS`, and
the fix was then confirmed **against the base CI actually used** rather than the local default. The
standing lesson, now applied: a local `--check` green is not evidence until it is re-run against that
base. This round it was re-run against all five — `origin/main` and every commit on the branch.

Validated: `check-citations` 86 self-test cases and `--check` **exit 0** against `origin/main`
**and** against each branch commit — 299 anchors reported stable against `origin/main` with 2 re-spelled
beyond that run's judgement, and 303 checked against the branch predecessor; `check-docs` 68
cases / 100 files; `check-portability` 45 files / 0 violations; `check-clang-warnings --self-test` 28
cases, plus the real gate run against the clang-22 build log at the regenerated baseline (**exit 0**,
14 accepted sites in 7 entries) and the mismatched pair still **refused** (exit 2); `dependabot.yml`
validated against the SchemaStore Dependabot 2.0 schema (**0 errors**); `actionlint` 1.7.7 over all five
workflows reporting **the same two findings as `origin/main` and no new one**; `bash -n` clean on all
five scripts. Synced: `ARCHITECTURE_REVIEW_GATE.md` (the new compiler/toolchain rule), ADR-0028 +
`ADR_INDEX`, `DEPENDENCY_POLICY.md` (table row, §Update mechanisms, compliance log),
`CI_CD.md` (§Cache lineages, §The Clang warning baseline, §Reproducing CI locally — the local-repro
block now calls `setup-llvm-apt.sh`, since Ubuntu has no `clang-22` for noble), `REPOSITORY_MAP.md`
(the new script + the `scripts/` tree line), this file.

Deliberately **not** changed: `CHANGELOG.md` (rule 3 — a CI toolchain pin is not user-visible),
`BUILD.md` (its "Verified on" list is about building the product locally and nothing in it became
false), the floating `*-latest` runner labels (floating is the intent), and pluginval's
`releases/latest` download, which is already carried as `RELEASE_HARDENING_PLAN.md` **RH-F6**.

**Stale re-aim declaration, protected history, non-gating cache statistics (0.9.4, no version
bump). Three review findings. No CI behaviour change beyond making a reporting step non-fatal.
`scripts/check-citations.py` + `.github/workflows/build.yml` + four documents.**

**A declaration that would have turned the default branch red.** `DELIBERATE_REAIMS` still named the
*intermediate* workflow anchors from the commit before last (`:495,1042,1454`), not the values the
documents were corrected to. The branch stayed green because a declaration is consulted **only when a
citation mismatches**, and against this branch's own tip nothing mismatches — so `--check --base
HEAD` passed while `--check` against the **merge base**, which is what a push to the default branch
actually compares, reported `UNMAPPABLE` and exited 1. Reproduced at `1993dbe` before the fix
(exit 1) and after (exit 0), and re-checked against every other base CI can pick.

That is the second time in this PR an anchor was recomputed without its declaration following, so
the class is now closed rather than the instance: **self-test section 9** asserts every
`DELIBERATE_REAIMS` entry names a string its document really contains. An entry naming a spelling no
document carries cannot be excusing anything — either the anchor moved on without it, or it outlived
its transition and should be deleted. It earned its place immediately: editing the workflow in this
same round shifted four anchors, and the self-test failed on all four before they were re-synced
(83 cases now, up from 70).

**History is no longer rewritable by the tool it describes.** `CMakeLists.txt` joined `TRACKED` last
round, and root-level paths have no bare escape spelling, so **eleven** *historical* sentences in
this document became claimable as live evidence — including one whose entire point is that an anchor
was **wrong** (`Drift observed, not corrected`), and another that literally states "rewriting one to
match today's code would falsify the record". A future `--fix` would have silently changed the
numbers those sentences are about, which is exactly what the checker's own header forbids. Each now
separates path from anchor (`` `CMakeLists.txt` `:206,230` ``); the one that *illustrates a spelling*
uses a non-tracked path instead, since splitting it would have destroyed the illustration.

Eight were caught in the first pass and three more in a second, which is worth recording because the
three were **misjudged, not missed**: they were read as live pointers ("the version line") when each
is in fact a record of a past event *at a position*. The sharpest is the C++23 entry, whose claim is
that the standard "moved 17 → 23 **in place** on `:16`, so no downstream citation moved" — the
number is load-bearing to the argument, and rewriting it would assert a positional fact that never
happened. Another sits two lines from a sibling reference in the *same* historical sentence that had
already been protected, so a `--fix` would have rewritten half a paired record and frozen the other
half. The discriminator that survives: **is the number the subject of the sentence, or a pointer to a
thing?** Exactly one `CMakeLists.txt` citation in this document is the latter — "*it is*
`CMakeLists.txt:531-540`", present tense, where re-anchoring preserves truth — and it is deliberately
left live so the gate still demonstrably checks real evidence here.

Verified by mutation rather than by reading: a line inserted into `CMakeLists.txt` above all of them,
then the real `--fix`, leaves all eleven byte-identical and correctly re-anchors the one live
citation (`:314-323` → `:315-324`).

**Cache statistics can no longer gate a release.** The six `Compiler cache statistics` steps ran
`ccache --show-stats --verbose` unguarded under `bash -e`; a non-zero exit (an older ccache without
`--verbose`) would have failed the job and then skipped stripping, staging and every upload, because
the steps after it default to `success()`. That contradicts this file's own rule — required tools
fail loudly, the cache steps aside — and is why `--zero-stats` was already guarded. Now `|| true`,
explained once in the compiler-cache block rather than six times at the call sites.

Re-anchoring the workflow after that edit moved four citations again; all four were recomputed from
the file as it stands and read back line by line.

**Citation follow-up (0.9.4, no version bump). Six corrected anchors, one wrong workflow comment,
two unsafe table cells. No CI behaviour change. Six documents + `scripts/check-citations.py`
+ `.github/workflows/build.yml` (comment only).**

Review found two anchors the previous round missed, both below its insertion point and both invisible
to the new gate for the same reason: they are spelled as **bare continuations**
(`` `path/to/file:188-199` … `:292-301` ``), which the parser only recognises in the
`path:a,b,c` form. `ADR-0001`'s "tests link the core" pointed at `juce::juce_opengl` inside the
*plugin*'s link block; it is `CMakeLists.txt:531-540`. `BUILD.md`'s compile-definition list cited
`:277-284` while listing `ANAMORPH_BUILD_NUMBER` — a definition that range no longer contains, since
scoping moved it to `:274-275`; widened to `:274-284`, deliberately as **one** anchor, because a
citation whose anchor *count* changes lands in the "review by hand" branch no declaration can excuse.

That continuation spelling is the **house convention** — 131 instances across the ADRs — so it was
left alone rather than re-spelled for one line. The class is recorded here, not fixed: bringing it
under the gate means either teaching the parser the convention or rewriting 131 anchors, and neither
is this round's business.

**Four workflow anchors were wrong on arrival**, which matters more than the two misses. They were
computed part-way through the previous round; a later edit in that same round (making ccache
optional, ~+70 lines) moved everything below it, and nothing objected — three were *declared
re-aims*, and a declaration is precisely a promise the tool will not judge the aim, while the two
`COMPATIBILITY_MATRIX` ranges were *new* citations, which "have nothing to drift from" and are
skipped against the base. Both escape hatches are correct individually; both being open at once is
how a measured number ships stale. All four are now recomputed from the file as it stands and read
back line by line rather than shifted by an arithmetic delta: `RELEASE_POLICY` `:530,1098,1520`,
`KNOWN_ISSUES` `:1687-1689`, and the two job ranges `:1456-1916` / `:1974-2219`.

Two more corrections, both of things the previous round introduced. The `linux` job carried a comment
claiming cached objects meant `objcopy --only-keep-debug` "reads exactly what it always read" — but
that command operates on the **linked** binary, not on objects, so the dependency it described does
not exist (the macOS `dsymutil` note, which does walk back to the object files, was already
accurate). And this document's own history table spelled two cells as `CMakeLists.txt` `:27,305` /
`:14,250-275`, violating the checker's own stated rule that **prose examples must not use a tracked
path** — the rule exists because one worked example in this file was already silently re-anchored
once. `build.yml` rows escape by being spelled bare; `CMakeLists.txt` is root-level and has no bare
form, so the table now separates path from anchor (`` `CMakeLists.txt` `:27,305` ``). The same trap
is now documented beside `TRACKED` for whoever adds the next root-level path — `NOTICE` is named
there as a candidate and would inherit it.

Investigated and deliberately unchanged: the `merge-check`/`linux` shared cache lineage (verified
mutually exclusive across all five event shapes), the `github.run_id` cache keys (ordinary
lineage/eviction trade-offs, no defect), the `!cancelled()` statistics steps (a step with no `if:`
still defaults to `success()`, so a failed build still skips its producer), the relaxed
citation-matcher (correct and asserted by name in the self-test), and the `ANAMORPH_BUILD_NUMBER`
scoping (the only reader is `src/PluginEditor.cpp`, both compiling targets are in this directory
scope, product behaviour unchanged).

**Citation-gate coverage for `CMakeLists.txt` and `build.yml` (0.9.4, no version bump). Four
corrected anchors, one checker change, no CI behaviour change beyond making the cache optional.
`scripts/check-citations.py` + `.github/workflows/build.yml` + `.gitignore` + five documents.**

The round before this one reported a gap rather than fixing it: `check-citations.py` did not track
`CMakeLists.txt` or `.github/workflows/build.yml`. Review then demonstrated the gap was load-bearing
rather than theoretical — that round moved 22 lines of one file and several hundred of the other,
and **four evidence anchors went stale while the gate reported the tree clean**:

| document | claimed | actually pointed at | now |
|---|---|---|---|
| `procedures/BUILD.md` | `ANAMORPH_BUILD_TESTS` | `JUCE_REPORT_APP_USAGE=0` | `CMakeLists.txt` `:27,305` |
| `policies/RELEASE_POLICY.md` | the build-number definition | mid-comment of the new block | `CMakeLists.txt` `:14,250-275` |
| `policies/RELEASE_POLICY.md` | the per-OS Configure steps | three unrelated workflow lines | `build.yml:495,1042,1454` |
| `KNOWN_ISSUES.md` | the ad-hoc `codesign` calls | the `macos:` job header | `build.yml:1621-1623` |

Two more, detached (`build.yml macos job (:1213-1626)`) rather than spelled as citations, were
corrected in `COMPATIBILITY_MATRIX.md` and re-spelled so the gate can now see them at all.

**The checker change is two lines of behaviour and one of ownership.** `CMakeLists.txt` is a
ROOT-level file, so the pattern's "a path must contain a directory separator" rule — written to
decline a bare `PluginProcessor.cpp:7` as ambiguous across checkouts — could never match it, and
listing it in `TRACKED` alone would have been **inert**: present in the list, absent from the gate,
indistinguishable from working. The separator requirement was therefore a proxy for the real test,
and ownership now rests where it always actually rested, on `TRACKED` membership spelled from the
repository root. A bare `PluginProcessor.cpp`, `run-pluginval.sh` and `build.yml` are all still
declined, each asserted by name in the self-test.

That last one is deliberate and load-bearing: this very document records past re-anchorings as prose
(“`build.yml:288,758,1141` → the three per-OS …”), which is **history, not evidence**. The workflow
is tracked under its full path, so the bare spelling those sentences use is what keeps `--fix` away
from the numbers the sentences are about — the exact corruption the header's "prose examples" rule
exists to prevent. `CMakeLists.txt` has no such escape, being root-level: its tracked spelling *is*
its bare one. The table above therefore separates the path from the anchor
(`` `CMakeLists.txt` `:27,305` ``) so the parser sees no citation, which is the same protection by a
different route — the two `build.yml` rows need no such treatment and are left as they read.

Coverage went from **217 to 312 checked anchors**. Proven live rather than asserted: inserting one
line into `CMakeLists.txt` turns the gate red with **60 drift reports** where the previous checker,
given the identical mutation, reported "217 anchors still point at the same text" and exited 0. Six
new self-test cases assert both files by name, end to end, plus the guard rails that make them safe
to rewrite — the FETCHED `build/_deps/juce-src/CMakeLists.txt` is declined, as are revision-pinned
and sibling-checkout spellings. The four corrections are declared in `DELIBERATE_REAIMS`, good for
one transition, aims confirmed by the maintainer.

**Still outside, named so the gap is a decision:** `packaging/macos/INSTALL.txt` (4 anchors),
`NOTICE` (3), `packaging/linux/INSTALL.txt` (1).

**The cache is now genuinely optional.** A review observation, confirmed on inspection: for one
commit `brew install ninja ccache || true` followed by a bare `ccache --version` swallowed a real
Homebrew failure and then converted it into a hard failure of a release-gating job. Ninja is
**preinstalled** on the macOS images, so brew had not previously been load-bearing at all. Each
install step now resolves `ANAMORPH_COMPILER_LAUNCHER` into `$GITHUB_ENV` and the configure step
passes it through; empty is how CMake spells "no launcher", verified locally to produce a plain
compiler invocation that builds. Required tools still fail loudly. `.ccache/` joined `.gitignore`
for the reason `/dist/` and `/clang-build.log` are there.

**CI compiler cache + job timeouts (0.9.4, no version bump). No new job, no test change, no
artifact change, no product-behaviour change. `.github/workflows/build.yml` + `CMakeLists.txt`
+ three documents.**

A CI *performance* round, measured before it was implemented. Baseline: run `31952680908` (push to
`main`, 2026-08-16, the first run carrying `macos-intel`) took **29m51s** wall, and the critical
path is **`macos` at 29m44s** — not `macos-intel` (21m26s), which despite a 5m04s queue delay still
finished 3m17s earlier. Within `macos`, the **build step alone is 16m40s**, more than its four
pluginval passes combined (11m25s). Every other build job has the same shape: build is 74–83% of
`linux`, `linux-clang`, `windows` and `sanitizers`. Compilation, not validation, is this pipeline's
cost.

`.ninja_log` splits a cold Linux Release build into **1409 CPU-seconds of compilation (75%) and 468
of LTO link (25%)**, and that compilation is overwhelmingly JUCE — ~9k lines of first-party source
against a framework each of the three JUCE-linking targets compiles separately, pinned to an
immutable commit and therefore byte-identical run to run. Recompiling it on every push was the
largest avoidable cost in the workflow, so the six Ninja jobs now compile through **ccache**
persisted in `actions/cache`.

**The enabling change is in `CMakeLists.txt`, and without it the rest is nearly worthless.**
`ANAMORPH_BUILD_NUMBER` is `${{ github.run_number }}` — the one definition whose value changes every
run — and it was a *target-wide* compile definition. Measured per target from `.ninja_log`, the two
targets carrying it are **84.4% of compile time** (`AnamorphStateTests` 57.7%, `Anamorph` 26.7%), so
84.4% of every build was guaranteed to miss the cache because an About-box string had incremented.
It is now attached with `set_source_files_properties` to the single translation unit that reads it;
`src/PluginEditor.cpp` already carried an `#ifndef ANAMORPH_BUILD_NUMBER → "0"` fallback, both
consumers still compile that file from this directory, and `grep` confirms nothing else ever read
it. Verified from the generated `build.ninja`: the definition now appears on **2 of 111** C++
objects, and on exactly the two expected ones.

Measured on 4 cores (the runner's core count), same build directory name, **with the build number
deliberately changed between the two runs** so the test is the CI case and not a favourable one:
**7m41s → 3m40s (−52%)**, at **137 direct hits / 6 misses**; the `linux-clang` configuration against
the pinned Clang 18 is **5m48s → 2m36s (−55%)** at **129 hits / 5 misses**. The residual is the LTO
link, which no compiler cache touches. The warning-replay property was proven the same way rather
than asserted: that job's real build and real gate, run cold and then warm, produce **54 warning
lines each, `diff`-identical**, and the same verdict (*no new first-party warnings, 14 accepted
sites in 7 baseline entries*).

Then confirmed in CI rather than left as a local claim. Baseline run `31952680908` against warm run
`31959972853`, full matrix, all nine jobs green in both: **run wall clock 29m51s → 17m13s (−42%)**.
Per job, build step first: `macos` 16m40s → **3m09s** (job 29m44s → 17m09s), `macos-intel` 10m21s →
3m04s (21m26s → 13m12s), `sanitizers` 12m14s → 3m31s across its two builds (15m07s → 6m38s), `linux`
8m04s → 3m34s (10m57s → 6m21s), `linux-clang` 7m58s → 2m29s (9m38s → 4m13s). Restoring a cache costs
2–4s and saving one 2–4s against ~104 MB per lineage. The intervening cold run (`31958514768`, the
push that landed this) came in at 26m11s with no hits available, confirming the caching machinery
adds no measurable cost when it cannot help.

The pipeline's shape is now inverted: `macos` remains the critical path, but 12m54s of its 17m09s is
the four pluginval passes and only 3m09s is compilation. Any further wall-clock reduction would have
to be taken out of validation, which is why this round stops here.

Safety is structural rather than promised: ccache's own hash — preprocessed source, full command
line, and the compiler binary's *content* (`CCACHE_COMPILERCHECK=content`) — is the correctness
boundary, so a GitHub cache key can only ever cost a hit and never manufacture a wrong one. That is
why the keys are coarse and do not hash `CMakeLists.txt`. Two properties were verified rather than
assumed before enabling: ccache hashes the full `-arch` **list**, so a universal object cannot be
served to a thin build or the reverse (checked directly: differing `-arch` sets miss, identical sets
hit — and the `macos` job's `lipo` assertion is the backstop either way); and a cache hit replays
the compiler's stderr **verbatim**, caret lines and `[-Wflag]` included, which `linux-clang`'s
warning gate depends on absolutely, since a cache that swallowed warnings would turn that gate green
by deleting its input.

Windows is deliberately excluded: ccache's MSVC mode requires `/Z7`-style embedded debug info while
this project compiles Release with `/Zi` so the linker emits the shipped crash-symbolication PDB,
and Windows is not the critical path (12m49s against 29m44s). Trading a release artifact for build
time is not a trade this pipeline should make.

Separately, **every job now carries `timeout-minutes`** (10 for the lint jobs, 30–60 for the build
jobs, each roughly double its measured runtime). There were none. A wedged job otherwise runs to
GitHub's 6-hour default while holding its `concurrency` slot, and since `release.yml` reuses this
workflow whole it would hold a release for that long; `scripts/run-pluginval.sh` passes
`--timeout-ms 600000`, so `macos-intel`'s twelve passes have a two-hour worst case on their own.
This bounds a failure mode; it does not remove any check.

Nothing about *what* is validated changed: same jobs, same suites, same pluginval modes, passes and
strictness, same artifacts, same `if:` conditions, still no `needs:` edge anywhere. Documents synced:
`docs/procedures/CI_CD.md` (new "The compiler cache" and "Job timeouts" subsections; pipeline steps
1–2), `docs/REPOSITORY_MAP.md` (the `build.yml` row). No `CHANGELOG` entry — `CHANGELOG_POLICY`
rule 3 admits user-visible changes and this ships no product change; the plugin binary's content is
unaffected, since the build number still reaches the same translation unit with the same value.

Two documentation inaccuracies introduced by the preceding round were corrected in the same pass,
both minimal: `CI_CD.md`'s "All three runners use the floating `*-latest` label" (now four runners,
one of them pinned), and pipeline step 6's producer list, which named `strip`/`package`/`build` but
not `macos-intel`'s `thin` and `au_install`. A third observation — that the new job's paragraphs had
been inserted between a later paragraph and its "the image" antecedent — was fixed by naming the
antecedent rather than reordering the block.

**Native Intel macOS CI (0.9.4, no version bump). One new job, no source change, no artifact
change. `.github/workflows/build.yml` + six documents.**

Migrated from the sibling product Anabasis after a fresh read of its current `main`
(`d92cf21`, 2026-08-16) rather than of the copy this repository was seeded from — its `macos-intel`
job was the only CI capability there that this repository did not already have in some form. The
gap it closes is one distinction: **an x86_64 binary running under Rosetta** is not **an x86_64
binary running on Intel silicon**. The `macos` job builds universal on an Apple Silicon runner and
executes the shipped x86_64 slice under Rosetta 2, which translates the instruction stream and runs
the result on arm64 hardware; `macos-intel` (`macos-15-intel`) builds thin `x86_64` and executes it
on a real Intel CPU. Both are blocking and neither replaces the other — the first validates the
*shipped bytes* under translation, the second validates *Intel execution* of a separately compiled
build.

Four defect classes were previously ungated anywhere: Intel code generation at `-O3 + LTO`; the
no-denormal DSP invariant, which holds only because `ScopedNoDenormals` flushes **in hardware**
(MXCSR FTZ/DAZ on x86_64, FPCR FZ on arm64) and was therefore never checked against the register
the shipped Intel slice sets; the Intel macOS AudioUnit/VST3 runtime; and a second AppleClang on
the macOS side. The job runs both self-test suites and the **full** pluginval gate — VST3 and AU,
deterministic ×3 **and** randomise ×3, at `ANAMORPH_PLUGINVAL_STRICTNESS` — which is where this
implementation deliberately departs from the sibling's (that one runs deterministic only): a fourth
platform running half the gate would falsify this workflow's own stated uniformity, and a fresh
randomise seed on Intel-generated code reaches an (architecture × value) space no other job does.
It packages, signs and uploads nothing, so the shipped macOS artifact is unchanged.

Two assertions carry the claim rather than the runner label carrying it: the first step **fails**
(not warns) unless `uname -m` is `x86_64` **and** `sysctl.proc_translated` is `0` — both wrong
answers caught, since a translated shell on an arm64 runner reports `x86_64` to `uname` — and a
second asserts `lipo -archs` is exactly `x86_64` on both built bundles, asserted rather than echoed
for the same reason the packaging step stopped echoing it. `macos-15-intel` is the one **pinned**
runner label in the workflow: `macos-latest` resolves to Apple Silicon and can never mean Intel, so
there is no float to follow, and the runtime assertion is what makes the pin safe rather than
brittle. `ANAMORPH_BUILD_STANDALONE=OFF` (the `codeql.yml` / `msvc.yml` idiom) keeps the build down
for no coverage loss.

Nothing existing was weakened, and the change is a pure **append** to `build.yml` (new lines
1627-1889, no existing line moved), so every `build.yml:NNN` anchor in `RELEASE_POLICY.md`,
`KNOWN_ISSUES.md` and `COMPATIBILITY_MATRIX.md` still resolves. `merge-check`, the same-repo PR
guard, the `ANAMORPH_CLANG_VERSION: 18` pin, the warning baseline, the concurrency tag exemption
and the `workflow_call` contract are untouched; the new job carries the identical same-repo PR
guard and has **no `needs:`**, so it cannot participate in the "green run that built nothing"
failure mode. Verified by evaluating every job's `if:` from the parsed YAML across five event
shapes (same-repo PR → `merge-check` only; fork PR, branch push, release `workflow_call`,
rehearsal → full matrix incl. `macos-intel`, `merge-check` skipped). One consequence stated rather
than discovered: `release.yml` calls this workflow whole, so the Intel gate now blocks a release
too — correct for a product whose `10.13` deployment target exists to claim Intel support.

Documents synced: `procedures/CI_CD.md` (build-matrix row + intro, the macOS narrative, pipeline
steps 2 and 4, §"Validation is uniform", §"Known coverage limits" — the "no native Intel macOS
runner" bullet **replaced** with the narrower limit that actually remains, and §"Reproducing CI
locally"), `policies/TESTING_POLICY.md` (Level 4 row; the hard-gate "suites run twice" → three
times, with why the three are not interchangeable; the pluginval bullet), `procedures/TESTING.md`
(§CI integration), `architecture/COMPATIBILITY_MATRIX.md` (a new platform row, and the AU row),
`REPOSITORY_MAP.md` (the `build.yml` row) and `HANDOVER.md` (Test Status). No `CHANGELOG` entry:
`CHANGELOG_POLICY` rule 3 admits user-visible changes, and this ships no product change.

**The residual limit, stated here as well as in CI_CD.md:** neither macOS job runs *the shipped
x86_64 slice on an Intel CPU*. The Rosetta step runs the shipped slice but not on Intel; this job
runs on Intel but not the shipped slice. What is left is a toolchain gap (the two builds come from
different images and AppleClang majors), not an ISA gap, and closing it means carrying the
universal artifact to the Intel runner and revalidating there.

**The stripper did not know what a raw string is (0.9.4, no version bump). One branch, one helper,
26 cases. `scripts/check-portability.py` only.**

`blank_comments_and_literals` treated every `"` as an ordinary string delimiter. A C++ raw string
does not end at the next quote — it ends at `)<delim>"` — and it processes no escapes, so
`R"(a " b)"` terminated on the embedded quote, its contents were scanned as **code**, and its real
closing quote then opened another "string" that blanked to the next `"` in the file. That last step
is the false-negative shape this lint exists to prevent: everything after the raw string stops being
checked. Measured before the fix: `const char* s = R"(a " b)"; auto n = juce::jmax<size_t> (x, y);`
reported **zero** hazards, and `R"(/* not a comment)"` leaked its contents into the scan. No raw
string exists under `src/` or `tests/` today, so this was latent — but every other edge of this
stripper (prefixes, splices, EOF in a literal, EOF in a comment) is explicitly pinned and this one
was not mentioned at all.

**Recognised, not special-cased.** `RAW_STRING_PREFIXES` is the exact set `{R, LR, uR, UR, u8R}`,
matched against the whole token to the left via the existing `_left_token` — the same discipline
`CHAR_LITERAL_PREFIXES` already uses, so an identifier merely *ending* in `R` is not a prefix.
`_raw_string_end()` then reads the delimiter under the real rule (C++ [lex.string]: at most 16
d-chars, excluding space, `(`, `)`, `\` and control characters) and searches for `)<delim>"`. It
returns `None` when the construct is not a valid raw string, and the caller then reads the quote as
an ordinary one — exactly the behaviour that existed before, so a malformed prefix cannot change
anything. An unterminated raw string returns end-of-file: it is still a raw string, so it swallows
the rest of the file rather than resuming as code halfway through a literal.

**Every existing guarantee holds.** The body is blanked one character per character with a newline
for every newline, and no escape handling — a raw string has none, so a trailing `\` must not
swallow the delimiter after it. Ordinary strings, character literals and their prefixes, escapes,
line splices, both comment forms, both EOF guards and hazard detection are untouched; the invariant
was re-checked across all 45 real source files, character count and newline count both.

**26 cases, chosen so the implementation cannot pass the easy way.** Five prefix forms must each let
the code *after* the literal be scanned; a custom delimiter whose body contains `)"` must not end it
early; a multi-line raw string must end at its delimiter rather than at the newline; an unterminated
one must not blind the code above it; a trailing backslash must stay literal. Four "must stay
silent" cases put a hazard, comment markers and a `)"` sequence *inside* the literal. Three
line-preservation cases pin the physical line the following code lands on. And four cases pin what is
**not** a raw string — a space or a `)` before the paren, a delimiter over 16 characters, a newline
in the delimiter — each written against an input whose hazard count changes when that rule alone is
removed, because misclassifying here sends the scan into a delimiter that never closes. 94 → 120
cases.

**Validation.** The new cases fail against the pre-fix stripper (the raw-string branch removed, the
tests kept: 7 failures) and pass with it. Nineteen mutations swept — the prefix set emptied, `LR` and
`u8R` dropped individually, the terminator ignoring the delimiter, consuming to end-of-line, always
consuming to EOF, the d-char validity check removed, the 16-char bound removed, control characters
allowed in the delimiter, raw-body newlines blanked, plus the established branches (char-literal
prefixes, both EOF guards, the splice newline, a dead regex, unstripped line comments) — **all
caught**. `check-portability --self-test` 120 cases then 45 files / 0 violations; `check-docs` 68
cases / 99 files; `check-citations` 58 cases / 217 anchors; `check-clang-warnings` 28 cases;
`py_compile` clean; actionlint clean on all five workflows. No other file changed.

**Sign-off carries forward.** The maintainer's confirmations recorded in the entries below remain
applied and settled; nothing in this round reopens them and no new approval requirement was created
for this fix. [Verified]

**A `[Verified]` release note described a defect that never existed (0.9.4, no version bump). The
claim is withdrawn, not reworded. Four files.**

The CHANGELOG asserted, under `[Verified]`, that the shipped Linux `.so` and Standalone carried a
`.gnu_debuglink` written as `dist/Anamorph-Linux-debug/<file>.debug` — "a CI-workspace-relative path
that exists on no user's machine" — and that the workflow change fixed it. The premise is false.
`objcopy` records only the **basename** of the `--add-gnu-debuglink` argument: bfd's
`bfd_fill_in_gnu_debuglink_section` passes it through `lbasename`, so a directory in the argument
never reaches the section.

**Measured here, on GNU objcopy 2.42, not taken from the review.** With `sub/a.debug` extracted from
`a`: `objcopy --add-gnu-debuglink=sub/a.debug a` stores `a.debug`; the `cd`-into-the-directory form
stores `a.debug`; and the two `.gnu_debuglink` sections extracted with
`objcopy -O binary --only-section=.gnu_debuglink` are **byte-identical** under `cmp`. The old and new
workflow forms therefore produce the same shipped bytes. There was no defect, and the change fixed
nothing.

**What was done about it.**
- **CHANGELOG: the bullet is removed, not rewritten.** `CHANGELOG_POLICY` rule 3 admits only
  user-visible changes, and this one is user-visible in neither direction — the artifact is
  unchanged. Rule 2 forbids invented history, which is what a reworded entry claiming some *other*
  benefit would be. 0.9.4 is untagged (no `vX.Y.Z` tag has been cut), so this corrects a draft rather
  than rewriting published notes.
- **`build.yml`: the comment now states the actual behaviour** — objcopy stores the basename via
  `lbasename`, both forms produce a byte-identical section, and the `cd` form is written that way so
  the stored name is the one spelled at the call site rather than one the reader must know
  `lbasename` produces. It is explicitly **not** a behavioural difference. The true half of the old
  comment is kept: a debugger resolves the name relative to the stripped binary's own directory, then
  `.debug/`, then the global debug dir. **The commands themselves are unchanged** — the code was
  never wrong, only its justification, and rewriting working workflow code to match a corrected
  comment would be the same error in the other direction.
- **`CI_CD.md`: the same premise, corrected the same way**, and it now says what actually follows for
  a user — the downloaded `.debug` file must be placed next to its binary **either way**, which the
  false version implied had been fixed.
- **This file: two `[Verified]` assertions struck in place.** The CI/validation round's "Defects the
  review found" list named the debuglink among them, and its validation paragraph claimed "the new
  debuglink was read back off the stripped binaries" — which is precisely the check that would have
  shown both forms agree. Both are marked WITHDRAWN with the correction beside them rather than
  deleted, because a silently removed false claim leaves no record that it was made. This is the
  same in-place treatment the round below used for a case count that did not reproduce.

**Nothing replaces the claim.** No substitute defect is asserted and no independent justification is
manufactured for the command form; the honest statement is that the artifact never differed.

**Validation.** `objcopy` behaviour verified as above (basename stored, sections `cmp`-identical) —
that is an artifact-level result on a locally built ELF, not on a CI-produced binary, and is reported
as such. `check-docs` 68 self-test cases / 99 files clean; `check-citations` 58 cases / 217 anchors;
`check-portability` 94 cases / 45 files; `check-clang-warnings` 28 cases; actionlint clean on all
five workflows. No code path changed: `build.yml`'s two `objcopy` invocations, every other workflow
step, CMake, packaging, DSP and the checkers are untouched. The corrected comment is two lines longer
than the one it replaces, which moved three `build.yml` anchors; re-anchored against content —
`RELEASE_POLICY` `:383,855,1238`, `KNOWN_ISSUES` KI-002 `:1398-1400`, `COMPATIBILITY_MATRIX`'s macOS
job range `:1213-1626`.

**Sign-off applied.** The maintainer's confirmation for this evidence correction is treated as
already granted and recorded here; the previously confirmed accepted-risk items recorded in the
entries below (`AudioComponentRegistrar` settling, the Windows staging checkpoint, the Rosetta
carve-out, pluginval after a DSP self-test failure, and the CI/release policy implications) remain
settled and untouched. No confirmation request is outstanding. [Verified]

**The same EOF hole in the other branch (0.9.4, no version bump). One guard, five cases,
`scripts/check-portability.py` only.**

The round below guarded the string/character-literal branch against emitting a closing pad at end of
file. The **block-comment** branch has the identical shape and was not guarded: its inner loop also
exits at `i == n`, after which `out.append("  ")` and `i += 2` ran regardless, emitting two
characters for none consumed. Measured before the fix: `/* abc` → 8 characters from 6;
`int x;\n/* unterminated\nmore` → 29 from 27. Guarded now on `if i < n:`, mirroring the literal
branch exactly.

**Three ways in, one exit.** The loop condition is `not (text[i] == "*" and i + 1 < n and text[i +
1] == "/")`, so a comment whose last character is a bare `*` (`/* abc*`) does **not** satisfy the
closer test and falls through the same `i == n` exit — as does a bare `/*` at EOF, whose body loop
never runs at all. All three are pinned separately rather than assumed equivalent, because the loop
condition is what makes them equivalent and a future edit to it is exactly what would separate them.

**Latent, again, and fixed for the same reason.** The extra characters are spaces, so no reported
line moved and no such source exists in the tree. But `lint()` maps every finding back to a physical
source line through this transformation, and the invariant is what the surrounding cases assert per
case — a branch that quietly does not hold it is the one the next edit builds on. That argument was
made for the literal branch last round while this branch went on breaking it, which is the more
precise reason to close it now.

**Five cases in the existing line-preservation table**, each with a hazard on the line *before* the
unterminated comment so the mapping ahead of it is pinned too: at EOF, spanning physical lines,
ending on a bare star, the opener alone, and a terminated block comment as the control that the
guard must not change. 79 → 94 cases. Verified beyond the table: the invariant holds for all 45 real
source files, length and newline count both.

**Validation.** `check-portability --self-test` 94 cases then 45 files / 0 violations; reverting only
the guard fails exactly the four new unterminated cases (`35 != 33`, `49 != 47`, `36 != 34`,
`31 != 29`) and passes with it. Eight mutations swept: both EOF guards removed, block-comment
newlines lost, the splice newline dropped, the prefix set emptied, a dead regex and unstripped line
comments are all **caught**. One mutation is *not* caught and is reported rather than papered over —
never blanking a terminated `*/` leaves those two characters verbatim instead of two spaces, which
preserves both length and newline count and cannot match `HAZARD`, so it is semantically inert for
this checker rather than a coverage gap. `check-docs` 68 cases / 99 files, `check-citations` 58 cases
/ 217 anchors, `check-clang-warnings` 28 cases, `py_compile` clean. No other file changed.

**Sign-off carries forward.** The maintainer's confirmations recorded in the entry below —
`AudioComponentRegistrar` settling, the Windows artifact behaviour after the staging checkpoint, the
Rosetta warning-only carve-out, pluginval after a DSP self-test failure, and the CI/release policy
implications already written up here — remain applied and settled. Nothing in this round reopens
them, and no new confirmation is outstanding. [Verified]

**The stripper's own contract, held on the one branch that did not (0.9.4, no version bump). Plus
one stale sentence about a governed scope. Two files.**

1. **`blank_comments_and_literals` broke its character-for-character invariant on an unterminated
   literal.** The literal branch appended a closing space and advanced `i` unconditionally after its
   inner scan — but that scan also exits at `i == n`, an unterminated string or character literal at
   end of file. Blanking a closing quote that is not there emits one character more than it consumed,
   so the stripped text was a character longer than the source. Measured: `const char* s = "abc`
   stripped to 21 characters from 20; the same for `char c = '<` and `auto c = L'<`. Guarded now on
   `if i < n:` — the closing space is emitted only when there is a closing quote to blank.

   **Latent, and fixed anyway, because of what the invariant is for.** The extra character is a
   space, not a newline, so no line number moved and no such source exists in `src/` or `tests/`.
   But `lint()` maps every finding back to a physical source line through that transformation, the
   two defects repaired in the rounds below were both this invariant failing in a way that *did*
   move a line, and the self-test asserts the contract per case — so a branch that quietly does not
   hold it is the one the next edit builds on.

   **Coverage extended in the existing table, and the assertion sharpened.** Four cases: an
   unterminated string, an unterminated character literal, an unterminated *prefixed* character
   literal, and one spanning two physical lines — each with a hazard on the line *before* it, so the
   mapping ahead of the unterminated literal is pinned too. The per-case length assertion is now
   joined by a **newline-count** assertion: length alone cannot see a newline swapped for a space,
   which is precisely how the line-splice defect below got in. 59 → 79 cases. Verified beyond the
   table: the invariant holds for all 45 real source files, length and newline count both.

2. **`classify()`'s docstring said the tool "only ever rewrites the nine files it knows".**
   `TRACKED` now lists 31 paths (25 `src/`, 2 `tests/`, 4 governed `scripts/`). This file's whole
   premise is that a stale statement about scope is the dangerous kind, and that sentence is the one
   describing the scope. Reworded to name `TRACKED` itself rather than a count, so it cannot go stale
   again the next time the list grows; no number was substituted. Documentation only — the
   classification logic, `TRACKED` and the citation mechanism are untouched. (The "19 anchors across
   nine files" figure elsewhere in the file counts the *documents carrying script anchors*, a
   different measurement, and is accurate.)

**Validation.** `check-portability --self-test` 79 cases then 45 files / 0 violations;
`check-citations --self-test` 58 cases and 217 anchors against `HEAD`; `check-docs` 68 cases / 99
files; `check-clang-warnings` 28 cases; `py_compile` clean on both modified scripts. The four new
cases fail against the pre-fix branch (restored under the new tests) and pass against the corrected
one. Eight mutations — the guard removed, the closing quote never blanked, the splice newline
dropped, raw newlines blanked, block-comment newlines lost, the prefix set emptied, a dead regex,
unstripped line comments — are all caught, so terminated literals, escapes, prefixes, comments,
splices and line mapping are all still pinned rather than merely assumed.

**Sign-off applied, not requested.** The maintainer's existing confirmations are recorded as settled
for: `AudioComponentRegistrar` settling before the AU gate; the Windows customer artifact surviving a
failed staging step after the `public_ok` checkpoint; the Rosetta-unavailable warning-only carve-out;
pluginval running independently after a DSP self-test failure; and the CI/release policy
implications already written up in this file. Each is an accepted risk or an accepted trade with its
reasoning recorded where a reader meets the behaviour — none is an open review item, and none was
changed in this round.

**Deliberately unchanged.** Everything else in the review is informational, investigate-only or
already accepted: the same-repo PR lint/`merge-check` architecture, the clang-18 pin, release-time
lint and sanitizer gating, reusable-workflow concurrency, repeated JUCE FetchContent work, the Linux
debug-directory widening, `ANAMORPH_HAVE_LLD` caching, `run-pluginval.sh`'s `-maxdepth 8`, the
CMake/lld scope, and the `DELIBERATE_REAIMS` lifecycle (whose entries are expected to be removed once
the default branch carries the re-anchored spellings — that is the mechanism working, not a defect).
No workflow, CMake, packaging, DSP or CI change. [Verified]

**Two checkers stopped lying about their own scope (0.9.4, no version bump). A root-level `_deps`
cache was being linted as project documentation, and a deliberately relaxed DSP run looked identical
to a full one.**

1. **`check-docs.py` scanned the repository-level dependency cache.** `SKIP_DIRS` held `.git`,
   `node_modules` and `JUCE`, and `_is_build_dir` covered `build`/`build-*`/`cmake-build-*`. The
   usual FetchContent layout lands at `build/_deps/juce-src`, which the build-tree rule catches — but
   `.gitignore` also allows a **top-level** `_deps/`, and that path has no `build*` ancestor.
   Reproduced: `_deps/juce-src/README.md` carrying JUCE's own Markdown made a local run report a
   table-fragment finding against a file nobody here wrote, exit 1. `check-clang-warnings.py` already
   uses `_deps` as its vendored marker, so the two scanners disagreed about what counts as vendored;
   adding `_deps` to `SKIP_DIRS` is that convention applied consistently, and it is the whole fix —
   `_is_build_dir` is untouched, since widening it for symmetry is how the "named, not prefixed"
   rule got broken once already. Two cases: the real `_deps/juce-src/README.md` layout inside a scan
   root must not be scanned, and a checkout that *lives* under a parent named `_deps` must still be
   scanned in full — the second because the skip set is scoped below the scan root, and a new entry
   in it inherits that scoping. 66 → 68 cases.

2. **`ANAMORPH_TESTS_NO_FTZ=1` relaxed a DSP invariant in silence.** The escape hatch itself is
   sound and stays: valgrind emulates floating point and does not honour the CPU's FTZ/DAZ bits, so
   under memcheck denormals survive into the output and the check fails on a build that is correct on
   every real CPU — while memcheck reports zero errors on the same run. What was wrong is that the
   flag was read once and left no trace: a stale export or an inherited CI variable produced the same
   `ALL TESTS PASSED` line as a full run, with the denormal half of the invariant never asserted.
   That is the failure mode `TESTING_POLICY.md` rule 4 and this pipeline's own comments name as
   unacceptable, applied to the suite rather than to a lint. The suite now announces the relaxation
   **twice** — a `::warning::` at start-up naming the un-asserted half, and a line beside the verdict
   so the tail of the log is self-describing too. `::warning::` rather than a plain notice because it
   is the form this repository already uses for lost-coverage-but-not-a-product-failure (the Rosetta
   step), so a CI run surfaces it in the UI instead of burying it. No DSP or product code changed, no
   other assertion moved, and the hatch was not removed to make the warning possible.

**Verified by running both, not by reading the diff.** `AnamorphTests` rebuilt and executed: the
normal run is unchanged (`140 checks, 0 failures` / `ALL TESTS PASSED`, exit 0, no new output); the
relaxed run prints the warning as the second line and the reminder above the verdict, still exit 0;
and `ANAMORPH_TESTS_NO_FTZ=0` and `=yes` neither relax nor announce, so the literal-`1` rule is
intact. For `_deps`, the reproduction was re-run after the fix — a top-level `_deps/juce-src` with
violating Markdown is now clean at exit 0, and `build/_deps/…` remains excluded as before.

**Documentation.** One sentence in `TESTING.md`'s "`ANAMORPH_TESTS_NO_FTZ=1` is for valgrind and
nothing else" paragraph, which is where a contributor is told never to set it and now learns how they
would notice if it were set. `CI_CD.md`'s two passages describe the relaxation accurately and are
left alone.

**Signed off, not outstanding: `AudioComponentRegistrar` settling.** The macOS AU gate restarts the
registrar and proceeds without waiting for the re-scan. In practice the first `AudioComponentFindNext`
call restarts and blocks on the daemon, so the sequence holds; it is the plausible flake source for a
blocking gate and is **reviewed and accepted by the maintainer (2026-08-16)** as a known risk rather
than an oversight. No change requested and none made.

**Validation.** `check-docs` 68 self-test cases / 99 files clean; `check-portability` 59 cases / 45
files; `check-citations` 58 cases / 217 anchors; `check-clang-warnings` 28 cases; actionlint clean on
all five workflows; `AnamorphTests` 140 checks, 0 failures in both modes. No workflow, CMake,
packaging, DSP-product or CI-optimisation change. [Verified]

**Portability scanner: a line splice inside a literal shifted every diagnostic below it (0.9.4, no
version bump). One line of the escape branch, plus the regression coverage.
`scripts/check-portability.py` only.**

The stripper consumed an escape pair as two spaces. That is right for `\t` or `\"`, and wrong when
the escaped character is the **newline itself** — a C++ line splice. The splice joins two *logical*
lines but the file still has two *physical* ones, and `lint()` reads the source back by physical line
number (`raw.splitlines()[lineno - 1]`), so blanking that newline made the stripped text one line
short and every finding below it was reported against the wrong line **and echoed the wrong source
text**. Reproduced end to end before the fix: a hazard on line 3 after a spliced string literal was
reported as `src/A.cpp:2` with the source line printed as `def";` — a developer sent to a line they
did not write the problem on. Not cosmetic: pointing at the defect is the whole contract of a lint
whose subject is a compile error that only appears on another platform.

**The repair is `out.append(" \n" if text[i + 1] == "\n" else "  ")`, and it is the smallest change
that keeps the invariant rather than patching the symptom.** The stripper's contract is
character-for-character — one output character per input character, a newline for every newline —
which is what makes both the reported line and the column exact; the block-comment branch already
honours it, and the literal branch's escape pair was the one place that did not. A space for the
backslash and a newline for the newline restores it: still two characters for two, and the line
boundary survives. The reported number is not adjusted after the fact anywhere, which would have
left the echoed source line wrong.

**Coverage extended in place, not duplicated.** The existing "LINE NUMBERS MUST SURVIVE THE STRIPPER"
block was a single block-comment assertion; it is now a table over every branch that consumes a
newline — block comment, string splice, string splice with the hazard **three** lines further down
(a shift accumulates, so fixing only the immediately-following line is not fixing it), two splices in
one literal, a splice in a *character* literal, an ordinary escape, an escaped quote, and a raw
newline inside a literal. Each case also asserts the character-for-character length, because a branch
emitting the wrong *count* passes a line test whenever the loss lands on a blank stretch. Two
end-to-end cases read the actual report: the line must be `src/Splice.cpp:4` and the echoed text must
be the real source line — the number and the echo come from different places, so only reading the
diagnostic catches them desynchronising. 42 → 59 cases.

**Validation.** The six new splice cases fail against the pre-fix branch and pass against the
corrected one, checked by restoring the old emit under the new tests. Eleven mutations — the newline
dropped again, a newline emitted unconditionally, the pair emitting one or three characters, escape
handling removed, raw newlines blanked, block-comment newlines lost, plus the previous round's prefix
set and the established regressions — are **all caught**. The lint is unchanged on the real tree: 45
files, 0 violations, scratch names agree. `check-docs` 66 cases / 99 files, `check-citations` 58
cases / 217 anchors, `check-clang-warnings` 28 cases, actionlint clean on all five workflows,
`py_compile` clean. Nothing outside `scripts/check-portability.py` is touched. [Verified]

**Portability scanner: prefixed character literals made it go blind (0.9.4, no version bump).
One branch, one helper, ten self-test cases. `scripts/check-portability.py` only.**

The comment/literal stripper decided a `'` was a **digit separator** (`1'000'000`) by looking at one
character: if what it had just emitted was alphanumeric, the quote was emitted verbatim rather than
opening a literal. The stated justification — "a real char literal never has an alphanumeric on its
left" — is false for the encoding prefixes C++ allows: `L'x'`, `u'x'`, `U'x'`, `u8'x'`. The opening
quote of `L'<'` was therefore read as a separator; the **closing** quote then had no alphanumeric on
its left and opened a literal instead, blanking everything to the next `'` — in a real file, to EOF.
Measured before the fix: `wchar_t c = L'<'; auto n = juce::jmax<size_t> (a, b);` strips to
`wchar_t c = L'<` and the hazard is **not reported**. All four prefixes behave the same way. No such
literal exists in `src/` or `tests/` today, so nothing was being missed — the defect is that the
guard would have been silent the day one was added, which is the one failure mode this lint's own
docstring says it must not have.

**The fix reads the whole token, not one character.** `_left_token()` walks back over what has been
emitted and returns the identifier run immediately left of the quote; the branch then asks whether
that token is a character-literal prefix. Empty → an ordinary `'a'`; `L`/`u`/`U`/`u8` → a prefixed
literal; anything else alphanumeric → a separator, or code that is not valid C++, where continuing to
scan is the safe reading. Comment padding is pushed as multi-character entries and stops the walk,
which is correct — a comment does not continue a token. The one-character test could not have been
repaired by "the left character is not a digit" either: `u8` ends in one, which is why each prefix is
pinned separately below.

**Ten cases added, and the fix is not the only thing they caught.** Four prefixed forms must still
find a hazard after the literal; an escaped quote and an escape sequence inside a prefixed literal
must be consumed; a literal on one line must not blind the next; two hex/binary separator forms join
the decimal one; and the prefixed literals must not themselves be reported. A mutation sweep then
found a **second, pre-existing** false negative that no case covered: a character literal holding a
double quote (`char q = '"';`). Under the old "emit the quote verbatim" reading the `"` inside it
opened a *string* and blanked to the next one — the same blindness reached a different way. The
corrected branch consumes both quotes of a character literal, prefixed or not, so it cannot happen;
two cases pin it. Self-test 28 → 42 cases.

**Validation.** The five prefixed/multi-line cases fail against the pre-fix branch and pass against
the corrected one, checked by restoring the old condition under the new tests. Fifteen mutations —
the prefix set emptied, each prefix dropped individually, the empty-token arm removed, the branch
forced both ways, the token walk truncated to one character, escape handling removed, plus the
established regressions (dead regex, unstripped line comments, lost block-comment newlines, a walker
that reaches no files, a scratch check that always agrees) — are **all caught**. The lint itself is
unchanged on the real tree: 45 files, 0 violations, scratch names agree. `check-docs` 66 cases / 99
files, `check-citations` 58 cases / 217 anchors, `check-clang-warnings` 28 cases, actionlint clean on
all five workflows. Nothing outside `scripts/check-portability.py` is touched. [Verified]

**Post-merge citation gate (0.9.4, no version bump). Six declarations. The gate would have turned
the default branch red on the first build after this PR merged, for a reason that is not a defect.**

The round below added `scripts/` to `TRACKED` in the **same change set that rewrote those scripts**.
Against the branch's previous push the anchors read clean — that base already carries the rewrite —
but against the **merge base** the cited text no longer exists, so no line number satisfies the
same-text test and `--fix` cannot repair it: six anchors report `UNMAPPABLE`, exit 1. CI resolves
its base from `github.event.before`, which on the branch is the previous commit and on the default
branch after a merge is the pre-merge tip. The gate was therefore green here and would have been red
there. Reproduced exactly, against `2ce2a76`: six UNMAPPABLE across `FUTURE_RISKS`, `POSTMORTEMS`,
`COMPATIBILITY_MATRIX`, ADR-0011 and `BUILD.md` ×2.

**`DELIBERATE_REAIMS` is the mechanism, and no code changed — only data.** Before using it, the
branch each of the six takes was checked: all are on the *paired* path, where `is_declared_reaim`
is consulted **before** the movability test, so a declaration covers an UNMAPPABLE case; the
count-mismatch path deliberately consults no declaration and is unaffected. All six also satisfy the
"the spelling actually changed" precondition (`:63-96` → `:147-176`, `:34` → `:121`, `:46-76` →
`:147-176`, `:29-38` → `:44-54`, `:19-30` → `:19-54`), so the entries are good for exactly one
transition and the run after it reports each as removable.

**Signed off as the maintainer's confirmation, after re-reading each aim rather than assuming it.**
Every one was resolved against the sentence that cites it: `:147-176` → the comment block plus
`run_one_pass` (the signal-only retry); `:121` → the `curl -L …/pluginval` release download;
`:44-54` → `setup-linux.sh`'s apt package list; `:19-54` → `build.sh`'s artefact-path block. The
declaration block records which region each names and when to delete them (against the **merge
base**, not against the previous push — the run prints that distinction itself).

**Not a blanket exemption, proven in four directions.** Against `2ce2a76` the run is now green and
prints six `ACCEPTED re-aim` lines naming each document — the acceptance is audible, never silent.
Removing one entry puts that anchor straight back to `UNMAPPABLE`, exit 1. Ordinary drift is
unchanged: a line inserted above a cited region in `run-pluginval.sh` still reports **10 DRIFTED**
and exit 1. And a declaration does not shelter its neighbours — `build.sh:14-15` is undeclared in
the same document as a declared entry, and drifting it fails the run on its own.

**`merge-check` was inspected and left alone.** Read off the parsed workflow: its `if` is the exact
complement of the same-repo guard; `actions/checkout` carries **no `ref:` override**, so on a
`pull_request` event it takes the default `refs/pull/N/merge` — the merge result, not the head; it
runs one Linux configure + build + both self-test suites, with **no artifacts, no pluginval, no
packaging** and no `needs:`; and the concurrency group keys on `github.ref`, which differs between
`refs/pull/N/merge` and `refs/heads/<branch>`, so the PR run and the push run cannot cancel each
other. The five-scenario matrix is unchanged: same-repo PR runs `merge-check` only; fork PR, branch
push, release via `workflow_call` and `workflow_dispatch` rehearsal all run the full set with
`merge-check` skipped. Nothing to fix, so nothing was changed.

**Documentation.** `CI_CD.md` §Evidence anchors described only the "re-aimed onto the code it should
always have named" case; the rewrite case — the one that is green on the branch and red after the
merge — is now stated there, since a reader who finds six entries in a list documented as empty is
owed the reason. `packaging/linux/uninstall.sh` re-verified byte-identical to its pre-PR state.

**Validation.** `check-citations` 58 self-test cases; green against the merge base (206 anchors, 6
accepted re-aims), against `HEAD` (217) and against `HEAD~1` (208); the four negative paths above.
`check-docs` 66 cases / 99 files, `check-portability` 28 cases / 45 files, `check-clang-warnings`
28 cases. actionlint clean on all five workflows; `bash -n` clean on the packaging and build
scripts. [Verified]

**Merge-result CI, script-anchor coverage and one strictness authority (0.9.4, no version bump).
Three review findings, each a real gap rather than a preference.**

1. **A same-repo PR had no build signal for the tree the merge button produces.** The guard added
   earlier skips every job on a same-repo PR because `push: ["**"]` already built the SHA — but
   `push` builds the branch **tip** and `pull_request` builds `refs/pull/N/merge`, the tip merged
   with the base as it stands. A PR green on its own tip and broken by a moved base was caught by
   nothing until the merge landed. The fix is a new `merge-check` job carrying the exact complement
   of the guard: same-repo PRs only, `actions/checkout` on that event gives it the merge commit, and
   it configures, builds and runs both self-test suites. It stops there deliberately — packaging,
   pluginval and the other two platforms validate properties of the tip the push build already
   gated, so re-running them would restore the duplicate 3-OS matrix the guard exists to remove, and
   what a moved base breaks is compilation and behaviour, both platform-independent. It produces no
   artifacts. Every job's `if:` was then evaluated over the five event shapes that reach this
   workflow: same-repo PR runs `merge-check` **only**; fork PR, branch push, release
   (`workflow_call` with a tag-push caller) and `workflow_dispatch` rehearsal are **unchanged** —
   full matrix, `merge-check` skipped, which is correct because a tag has no merge result.
   Concurrency is unaffected: `refs/pull/N/merge` and `refs/heads/<branch>` are different refs, so
   the two runs do not cancel each other.

2. **The citation gate could not see the script anchors.** `TRACKED` excluded `scripts/`, which left
   **19 anchors across nine documents** into `run-pluginval.sh`, `run-tests.sh`, `setup-linux.sh`
   and `build.sh` ungated — and those drift more than the DSP stages do, because a script gets
   rewritten wholesale. The four are now tracked. Nine live citations spelled them by **bare name**
   (`run-pluginval.sh:154-176`), which `classify()` declines by design — a bare name is ambiguous
   across checkouts — so those were root-spelled, which is the spelling the tool's header already
   prescribes. The three bare-name occurrences in *this* file are records of past re-anchoring and
   were left alone. Anchor count 198 → 208 against the current base, and 217 once the newly
   root-spelled ones have a base that carries them.

   **Proven by drift, not by a green run.** A line was inserted into `run-pluginval.sh` and
   `setup-linux.sh` above the cited regions: the gate reported **12 drifted citations across 9
   documents** and exit 1 where it had previously been silent, `--fix` re-anchored 15 with 0 needing
   a human, the re-check went green, and a repaired anchor was read back at its new location. The
   tree was then restored. Self-test 45 → 58 cases: each of the four scripts is asserted **by name**
   as a citation target and as a scanned file (dropping one from `TRACKED` costs nothing visible —
   the citations still read fine and the run still prints a confident count, minus what it stopped
   checking), a live script anchor is claimed end to end, and the guard rails that made scripts safe
   to add are pinned in the negative: `build/_deps/juce-src/README.md`, `build-san/scripts/…` and
   `_deps/scripts/…` must **not** classify, and a bare script name must still be declined.

   **The `154-176` vs `147-176` divergence is not drift and was left as it is.** `154-176` is
   `run_one_pass` alone, cited where the *rule* is stated (`TESTING_POLICY` rule 3, `TESTING.md`'s
   failure table); `147-176` adds the comment block explaining why the retry exists, cited where the
   *rationale* is the point (`FUTURE_RISKS`, `KNOWN_ISSUES`, `POSTMORTEMS`, `TROUBLESHOOTING`,
   ADR-0011). Both land on the content their sentence describes. They are now gated independently,
   which is the right outcome: the gate's job is to keep each true, not to make them identical.

3. **The single-strictness-authority claim held in one file only.** `TESTING_POLICY.md` said "this
   policy states no strictness number" while four other governed documents stated `10` as the
   requirement — so a raise still meant five edits, which is the staleness the rule was written to
   remove. `RELEASE_POLICY.md`, `DEPENDENCY_POLICY.md` (upgrade rule 2),
   `COMPATIBILITY_MATRIX.md` (three platform rows + the DAW-proxy note) and
   `RELEASE_COMPATIBILITY_CHECKLIST.md` now state that the gate must pass **at the configured
   strictness** and name `ANAMORPH_PLUGINVAL_STRICTNESS` as where to read it; the checklist item
   spells its command `<n>` and tells the human to read the value from the workflow rather than from
   that line. `CI_CD.md` was contradicting itself in the same paragraph — claiming nobody restates
   the value while printing **10** — and no longer prints it.

   **Two classes of literal deliberately survive.** `DEPENDENCY_POLICY.md`'s **compliance log**, and
   the equivalent records in `HANDOVER`, `FUTURE_RISKS`, `RELEASE_HARDENING_PLAN` and six ADRs, say
   what a *past* run was verified at — a fact about that run, wrong to move, and the same rule this
   change set already applies to historical anchor pairs. `TESTING.md`'s command examples must show
   a number a reader can type; the three that labelled `10` as "(release gate)" no longer make that
   claim, and the sentence below them already names the authority. `README.md` is left as a
   descriptive overview at the bottom of the authority order.

**`packaging/linux/uninstall.sh`: verified byte-identical to its pre-PR state.** The developer-
oriented block was reverted in the round below; `git diff` against the pre-PR revision is empty, and
nothing in this round touches it.

**Signed off, not outstanding: pluginval runs after a failing DSP/state self-test by design.** The
producer step is `strip`/`build`/`package`, none of which depends on `tests`, so a red self-test
leaves the binary intact and the conformance gates still report — one run yields the whole picture,
at the cost of two extra pluginval runs after a genuinely broken build. The customer uploads are
unaffected; they gate on `tests`. **Manually reviewed and confirmed by the maintainer on
2026-08-16**, and now recorded where a reader meets the behaviour (`CI_CD.md` §pluginval) rather
than only here. It is settled, not an open review item.

**Validation.** actionlint clean on all five workflows, with the per-event job matrix enumerated
from the parsed YAML rather than read off the diff. The new job's 69 lines moved the three
`build.yml` anchors again, re-anchored against content: `RELEASE_POLICY` `:383,853,1236`,
`KNOWN_ISSUES` KI-002 `:1396-1398`, `COMPATIBILITY_MATRIX`'s macOS job range `:1211-1624`. (Those
three are still ungated — `build.yml` is not in `TRACKED`; adding it is the obvious next
candidate and is deliberately not folded into this round.) `check-citations` 58 self-test cases and 208
anchors against both `HEAD` and `HEAD~1`, plus the drift-injection run above; `check-docs` 66 cases
/ 99 files; `check-portability` 28 cases / 45 files; `check-clang-warnings` 28 cases. `bash -n`
clean on the packaging scripts. [Verified]

**check-docs false-green fix (0.9.4, no version bump). One defect, found by the review of the round
below: the documentation lint could report every file clean without opening one.**

`markdown_files()` filtered on `path.parts` — the components of the **absolute** path. `main()`
resolves the root, so `rglob` yields absolute paths and the skip set (`.git`, `node_modules`, `JUCE`,
plus `build` / `build-*` / `cmake-build-*`) was matched against every **ancestor of the checkout**,
not only the directories the scan owns. A clone at `~/build/anamorph`, `/opt/JUCE/anamorph`, or
anywhere beneath a `node_modules` therefore matched on a directory outside the repository, excluded
**every** file, and printed `0 file(s) clean` with exit 0. Reproduced both ways before and after:
copies of this tree under parents named `build`, `JUCE`, `node_modules` and `cmake-build-x` report
`0 file(s) clean` with the old checker and `99 file(s) clean` with the new one. CI never saw it —
`GITHUB_WORKSPACE` carries no such component — so the only way to meet it was the local reproduction
`CI_CD.md` documents, which is to say: the person following the instructions.

**The fix is two lines and one guard, and the guard is the durable half.** The filter now tests
`path.relative_to(root).parts[:-1]`, so only components below the scan root can say a file is
generated or vendored — the exclusions that are real are untouched, verified by planting a malformed
table, a lazy continuation and an unclosed fence inside `build-san/_deps/juce-src/`, `JUCE/docs/` and
`node_modules/pkg/` in the working tree and confirming the run still reports 99 files clean. (That
same relative test also makes the two halves of the condition consistent: `SKIP_DIRS` was being
matched against the filename as well, which was inert only because no skip name ends in `.md`.)
Separately, `main()` now **refuses to call an empty scan clean** — exit 1, matching this file's
`0 = clean, 1 = findings` contract and its two existing argument errors, rather than the `2` the
sibling scripts use for "inconclusive". The filtering bug is what made an empty set reachable on a
correct tree; the guard is what catches the next way of getting there, and it fires on a directory
holding only excluded subtrees as well as on an empty one.

**Self-test: 57 → 66 cases.** Six checkouts, one under each skip name (`build`, `build-san`,
`cmake-build-debug`, `JUCE`, `node_modules`, `.git`), must scan their own documents; the
in-repository exclusions must survive, including that `building/` and `rebuild/` are **not** build
trees (named, not prefixed — dropping them would be the same defect wearing the opposite sign) and
that a nested `docs/build/gen/` is still excluded; and `main()` over an empty directory must not
return 0. Mutation-tested rather than assumed: reverting the filter to `path.parts`, deleting the
empty-scan guard, disabling `SKIP_DIRS`, disabling the build-tree test, and widening `_is_build_dir`
to a prefix match are **all five caught**.

**`packaging/linux/uninstall.sh`: the round below's comment block is reverted in full.** That file
ships to users inside the Linux zip. The block named `scripts/check-portability.py`, described what
fails CI, referenced `SCRATCH_NAME` and cited the sibling product's uninstaller — developer content
in a user-facing script, and the two-line user-oriented comment that was already there says what a
user needs. `check-portability.py` claimed the note existed ("the other end of the coupling"), so
that one sentence is corrected in the same change: the reasoning now lives in the checker only, and
says why.

**Deliberately not changed.** Everything else in the review is informational or was manually
confirmed: the same-repo PR merge-result gap, deterministic pluginval running after a self-test
failure, the clang-18 pin, `AudioComponentRegistrar` settling, reusable-workflow concurrency, the
debuglink co-location note, Windows best-effort staging, the Rosetta `/usr/bin/true` probe, `cp -R`
vs `ditto`, the digit-separator char-literal note, the cached `ANAMORPH_HAVE_LLD` probe, the
CHANGELOG tab-indent edge case, the pluginval bundle-override format check, and the bash-3.2 array
expansions. The 184-vs-198 anchor-count observation is also left: the two figures measure different
sets (184 was `docs/` alone; 198 includes the tracked sources `doc_files()` added later), and both
statements are true of the round that wrote them. For the same reason the four `check-docs 57 cases`
figures in the entries below are **not** rewritten to 66 — each records what that round's run
reported, and is accurate for it. The current figure is the one above.

**Validation.** `check-docs` self-test 66 cases then 99 files clean; `check-portability` 28 cases /
45 files; `check-citations` 45 cases and 198 anchors against both `HEAD` and `HEAD~1`;
`check-clang-warnings` 28 cases. actionlint clean on all five workflows; `bash -n` clean on both
packaging scripts. The pre-existing argument guards (`no such path`, `not a Markdown file`) and the
explicit single-file invocation were re-run unchanged. [Verified]

**Lint self-verification round (0.9.4, no version bump). Two of the four CI lints could not fail;
both can now, and the policy says which proof is which.**

`TESTING_POLICY.md` rule 4 requires every pipeline lint to ship a self-verification running **in the
same job, immediately before the check itself** — because a checker that has stopped matching
anything is indistinguishable from a clean tree. Two of the four did not satisfy it, and one of them
was the checker that can do the most damage.

1. **`check-portability.py` had no test of its checker at all.** What the policy named as its
   self-verification, `--compile-canary`, answers a different question: it compiles two translation
   units against the pinned JUCE to assert the *hazard* still exists. That is a check on the
   **dependency**, it needs a JUCE checkout, and it runs in `linux-clang`. Nothing tested the
   **scanner** — a regex plus 60 lines of hand-written comment/literal lexing plus the
   installer/uninstaller scratch-name comparison — so a green canary over a dead regex would have
   reported the tree clean, which is exactly the shape the rule forbids. `--self-test` now runs the
   real stripper and the real pattern over 20 labelled sources in both directions, then the real
   `lint()` over a temporary tree (proving the walker reaches files and respects its declared
   suffixes), then the real `scratch_names_agree` over four installer/uninstaller pairs including
   the one where the name survives **only in a comment** — the divergence shape that already shipped
   once in the sibling product. 28 cases.

2. **`check-citations.py` had nothing, and it is the one lint that WRITES.** The other three report;
   this one re-anchors governed documents under `--fix`, so a defect does not merely miss drift — it
   replaces a correct anchor with a wrong one and prints success. Its header records four occasions
   on which it did exactly that: a `rev:`-qualified anchor reaching the ownership test (27 anchors
   rewritten across five ADRs), a compound citation left reading `:1040, 1039, 1053`, one span
   applied twice turning `:2000` into `:20000`, and a provenance sentence whose wrap put the sibling
   product's range outside the exclusion. Every one is now a case, in the direction it failed, and
   the set extends to the ownership test's whole decline list, the provenance block's two boundary
   forms, the diff line-map (including the pure-insertion off-by-one and the `None` that makes an
   edited hunk report UNMAPPABLE rather than invent a number), the span rewriter's de-duplication
   and overlap refusal, and the declared-re-aim guard that must not survive its own transition.
   45 cases, on synthetic input through the real functions — no repository, no base revision, no
   `git`, which is what lets it run beside the check.

**Why this shape rather than the alternatives.** Moving `--compile-canary` into `source-lint` was
rejected: it needs `build-clang/_deps/juce-src/modules`, so `source-lint` would have to fetch JUCE,
turning a seconds-long job into a multi-minute one to duplicate a check that already runs — and it
still would not test the scanner. Narrowing the policy text to describe the shortfall was also
rejected: the rule is right, and rewriting a rule to match an implementation that misses it is
documenting the gap rather than closing it. So the policy is **extended** instead: rule 4 now names
all four `--self-test`s, states what a self-test must do (both directions; no dependency the job
does not already have), and adds a paragraph distinguishing a **premise** check from a
**self-test** — both required, neither a substitute, with the failure mode of each spelled out. That
distinction is the durable part: it is what stops the next lint's canary being filed as its
self-test.

**One refactor, and only one.** `build_line_map` shelled out to `git diff` and parsed the hunk
headers in the same function, so the arithmetic that every re-anchor depends on could not be
reached without a repository. The parsing half is now `line_map_from_diff(diff)` and
`build_line_map` calls it. No behaviour change; the hunk shapes the self-test feeds it were taken
from real `git diff -U0` output rather than invented (a prepend is spelled `-0,0`, not `-1,0`, and
the first draft of the test asserted the wrong one).

**Validation, and the part that matters most: the self-tests were shown to FAIL on a broken
checker.** Passing on a correct one proves nothing — that is the whole premise of rule 4 — so both
were mutation-tested. Twenty-two mutations were applied to the two scripts, one at a time, each
disabling a specific guard: a dead `HAZARD` regex, an over-eager stripper, un-stripped line
comments, a lost newline in the block-comment branch, a walker that reaches no files, a scratch
check that always agrees; and on the citation side an ownership test that accepts a `rev:` prefix or
any path, a dropped compound-anchor group, a removed lookbehind, a lookbehind widened until it
swallows real citations, disabled provenance exclusion, a provenance block that never ends, the
pure-insertion off-by-one, an edited hunk returning a number instead of `None`, a misread
omitted hunk count, removed span de-duplication, a removed overlap refusal, a re-aim guard that
survives its transition, a re-aim honouring only one spelling, an empty scan set, and a scan that
drops the source half. **Every one is caught.** Three earlier drafts of the cases were NOT caught
and were rewritten until they were — the block boundary needed a bare `//` line rather than a
`#pragma once`, the re-aim case was under the wrong set state, and the lookbehind case had been
written with a qualifier the *prefix capture* declines, so it never reached the lookbehind at all.
Beyond that: actionlint clean on all five workflows, with the two new steps confirmed to sit
immediately before the lints they verify in `source-lint` (read back off the parsed job, not off the
diff); the 32 inserted lines moved the same three `build.yml` anchors the Intel-gate entry below
re-anchored, and they were re-anchored again against content — `RELEASE_POLICY` `:324,794,1177`,
`KNOWN_ISSUES` KI-002 `:1337-1339`, `COMPATIBILITY_MATRIX`'s macOS job range `:1152-1565`; all four self-tests and all four lints
green (`check-docs` 57 cases / 99 files, `check-clang-warnings` 28, `check-portability` 28 cases /
45 files, `check-citations` 45 cases / 198 anchors against both `HEAD` and `HEAD~1`); and the
citation gate exercised end-to-end by inserting a line above a cited anchor in a tracked source —
it reported 5 drifted citations, `--fix` re-anchored all 5 with 0 needing a human, the re-check went
green, and the repaired anchor was read back at its new location. [Verified]

**macOS Intel customer-artifact gate (0.9.4, no version bump). One workflow correction: the new
x86_64 self-test now blocks the customer uploads it was always supposed to.**

The CI/validation round added a second macOS self-test step that runs the universal binary's
**x86_64 slice under Rosetta 2** — coverage that did not exist before, since the runner is Apple
Silicon and the native step exercises arm64 only. The step was added without an `id:`, so its
outcome was unreferenceable, and the two customer uploads still named `steps.tests` (arm64) and
their own packaging step. A run whose **Intel** behavioural gate failed therefore went red while
still publishing `Anamorph-macOS` and the `.pkg`: the job reported the failure and shipped the
package anyway. That contradicts the invariant stated at the top of `build.yml` — customer uploads
gated on the DSP self-tests succeeding, never `if: always()`, so that "neither a partial packaging
failure nor a failed behavioural gate can ship a customer artifact." Validating half the shipped
product and then ignoring the verdict is the one failure mode that rule exists to prevent, and it
was pointed at the half that has *no* other coverage — Linux and Windows self-tests say nothing
about the macOS Intel slice, and native Intel hardware is not in the matrix at all.

**The fix, and why it is the smallest one.** The step gets `id: tests_x86_64`, and both customer
uploads add `steps.tests_x86_64.outcome == 'success'` — the same clause shape, in the same
position, as the `steps.tests.outcome == 'success'` beside it, so macOS now reads like Linux and
Windows rather than like an exception. `== 'success'` rather than `!= 'failure'` is deliberate:
`!= 'failure'` passes on `skipped`, which is the weaker reading of a fail-closed gate. The Intel
step itself is **untouched** — same `if:`, same body, same Rosetta probe, same `::warning::` — so
the validation was not weakened to satisfy the gate. Two non-failure paths stay open by design:
an absent Rosetta still exits 0 (its presence is the *image's* property, not the product's), so
the uploads proceed on compilation-only Intel coverage with the warning as the record; and a failed
**build** skips the step, where `steps.tests` already blocks the upload. The developer dSYM upload
is unchanged, per the standing rule that `-debug` artifacts survive gate failures. Nothing else
moved: no compiler pin, no pluginval ordering, no Rosetta detection, no `AudioComponentRegistrar`
handling, no debuglink, no Windows policy, no CMake, no source.

**Documentation synced with it.** `CI_CD.md` gains the gate in both places that describe it — the
Rosetta paragraph (§macOS runner) and the customer-upload rule in step 7 — including the
Rosetta-absent carve-out, so the exception is written down where a reader meets the rule.
`ADR-0021`'s "each customer artifact upload requires the DSP self-tests AND its own
strip/staging/packaging step to have SUCCEEDED" needed no edit: that policy statement was already
true, and this change makes the implementation match it. Three `build.yml` anchors moved with the
21 inserted lines and were re-anchored against content: `RELEASE_POLICY.md` `:288,758,1141` →
`:292,762,1145` (the three per-OS Configure steps), `KNOWN_ISSUES.md` KI-002 `:1290-1292` →
`:1305-1307` (the three `codesign --force --deep --sign -` lines), and `COMPATIBILITY_MATRIX.md`'s
whole-macOS-job range `:1116-1512` → `:1120-1533`.

**Validation.** actionlint clean on all five workflows — and proven live rather than assumed: a
canary copy with the id misspelled `tests_x86_65` is rejected with *"property is not defined in
object type"*, and the type it prints lists `tests_x86_64` among the macOS job's steps, so the
reference resolves. The two upload conditions were then evaluated exhaustively over every
combination of `tests` × `tests_x86_64` × `package` × `package_macos_pkg` ∈ {success, failure,
skipped} × cancelled ∈ {true, false} — 486 cases, with three properties asserted: an Intel failure
never uploads, an arm64 failure never uploads, and the all-success non-cancelled case uploads both.
No violations. Cancellation is unchanged (`!cancelled()` still leads both conditions). `check-docs`
self-test (57 cases) + 99 files, `check-portability` (45 files), `check-citations --check`
(198 anchors) and `check-clang-warnings --self-test` (28 cases) all green. [Verified]

**Documentation re-sync after the CI rounds (0.9.4, no version bump). Documentation only — no
workflow, script, CMake or source file was touched.**

The two CI entries below changed what the pipeline does and moved a large amount of code; this
round makes the prose that describes them true again. Three things, in descending order of how
wrong the tree was without them.

1. **KI-014 was still open in the register after the coverage gap it recorded had been closed.**
   The entry said the macOS AU "is built and shipped but never validated automatically —
   `run-pluginval.sh` only sees the VST3." That stopped being true in the CI/validation round: the
   macOS job installs the built `.component` into `~/Library/Audio/Plug-Ins/Components/`, restarts
   `AudioComponentRegistrar`, and puts the AU through the *same* release gate as the VST3 — same
   strictness, both modes, three consecutive passes each, against the **packaged** bundle. A known
   issue whose defect no longer exists is not a stale line, it is a false statement about the
   shipped product, and it was being read as current by four other documents. Handled per this
   repository's own fixed-item rule and following the KI-005 precedent: the summary-table row is
   **removed**, the body section is replaced by an italic tombstone recording the resolution and
   retiring the ID, and the removal is recorded in the file's version-sync header. `HANDOVER.md`'s
   Known Blockers row now says KI-014 is closed rather than listing it; its Roadmap row no longer
   reads as though AU coverage does not exist, and names `auval`-in-CI as an *addition* to a gate
   that does. `RELEASE_HARDENING_PLAN.md` RH-F3 is **narrowed, not closed** — its premise
   ("zero automated validation") is superseded, but Apple's `auval` specifically is still not run,
   and its open question (headless-runner reliability) is unchanged; closing the row would have
   claimed coverage that does not exist. `TESTING.md` §"Gaps in the automated coverage" already
   carried the closure from the earlier round and is left as the detailed account both now cite,
   with `CI_CD.md` §"Known coverage limits" as the record of the pluginval-not-`auval` residual.
   `COMPATIBILITY_MATRIX.md`'s AU row was the last carrier of the old picture and moved from
   **Verified (build)** to **Verified (build + conformance)** — its "Unverified (host)" half stays,
   because pluginval loads the AU through JUCE's `AudioUnitPluginFormat`, which is not Logic.
   KI-014's own evidence anchors are retired with it rather than re-anchored: the claim they
   supported (`run-pluginval.sh` finds only `Anamorph.vst3`) is no longer in the script.

2. **52 evidence anchors across 18 documents pointed into moved code.** `CMakeLists.txt` gained 64
   lines (the lld probe and the hardening block), `build.yml` roughly doubled, and several scripts
   were rewritten, so every `file:line` citation below the insertion points had drifted. These were
   **not** shifted by an offset. Each anchor was resolved by reading what the document *claims* and
   locating that content in the current source, which matters because an offset would have been
   wrong in both directions: `CODE_STYLE.md` cited `CMakeLists.txt` `:206,230` for the recommended
   warning flags, an anchor that was already wrong before this PR (206 was `juce::juce_opengl`) and
   that no shift could have repaired — the flags are at `:275,301,339`, three sites rather than two,
   because the tests target was added since. Conversely `TESTING_POLICY.md:67` and
   `TROUBLESHOOTING.md:23` cite the pluginval signal-only retry at two *different* spans,
   `:154-176` and `:147-176`; both are correct — one is `run_one_pass` alone, the other includes
   the comment block that explains why the retry exists — so neither was normalised to the other.
   One anchor is not a `file:line` citation at all and a mechanical sweep would have skipped it:
   `COMPATIBILITY_MATRIX.md` cites the *whole macOS job* as a bare range, `build.yml macos job
   (:355-542)`, and that job now spans `:1116-1512` — it did not move, it grew, and it is the
   growth that carried the AU gates.
   The 18 files: `PRIVACY`, `TRADEMARKS`, `HANDOVER`, `KNOWN_ISSUES`, `REPOSITORY_MAP`,
   `ARCHITECTURE`, `COMPATIBILITY_MATRIX`, ADR-0001 / ADR-0011 / ADR-0023, `CODE_STYLE`,
   `DEPENDENCY_POLICY`, `RELEASE_POLICY`, `TESTING_POLICY`, `BUILD`, `PACKAGING`, `TESTING`,
   `TROUBLESHOOTING`.

   **What was deliberately left alone, and why it is not an oversight.** Four older entries in
   *this* file quote anchor pairs in old → new form — the two "reported-then-corrected line drift
   (C6)" entries, the `setup-linux.sh` `curl`/`unzip` entry, and the post-v0.9.0 maintenance audit.
   Those are not citations; they are the historical record of *previous* re-anchoring operations,
   and **both** halves of each pair are meant to read as they did then — the old half is supposed
   to be stale, and the new half records where that operation landed it, not where the content sits
   today. Rewriting either would destroy the audit trail this file exists to keep. The same
   reasoning applies to anchors inside `worklogs/` and to `POSTMORTEMS.md` entries that quote the
   code as it stood at the time of an incident.

3. **The `check-clang-warnings` self-test count in the entry below was recorded as 24; it is 28.**
   Corrected in place rather than left as history — a case count is written down precisely so a
   reader can re-run the command and get the same number, and one that does not reproduce is worse
   than none. The four extra cases are the compiler-pin round's baseline version round-trip block.
   The other three figures in that sentence reproduce exactly as written.

**Validation.** All four lints green with their self-tests, run against this tree:
`check-docs.py --self-test` (57 cases) then `check-docs.py` over 99 files;
`check-clang-warnings.py --self-test` (28 cases); `check-portability.py` (45 files);
`check-citations.py --check` against both `HEAD` and `HEAD~1`, 198 anchors. Every anchor changed in
this round was additionally re-read at its new location and confirmed to land on the content the
citing sentence describes — for example `PACKAGING.md:177` → `CMakeLists.txt` `:217` →
`PLUGIN_MANUFACTURER_CODE RTec`, `TROUBLESHOOTING.md:25` → `run-pluginval.sh:129-131` → the
`xvfb-run -a` prefix, and `RELEASE_POLICY.md:65` → `build.yml:288,758,1141` → the three per-OS
Configure steps. No CI, source, architecture or policy behaviour changed in this round, and the
pluginval execution order was left exactly as the CI rounds landed it. [Verified]

**CI + validation round — review follow-up (0.9.4, no version bump). Five corrections, and the
first is the one worth reading twice.**

Landed on top of the migration entry below, all five from a review of it. None changes what the
pipeline is *for*; four change whether it does what it claims.

1. **`MALLOC_PERTURB_=255` provided no coverage at all — it was worse than not setting it.** glibc
   applies the variable asymmetrically (`alloc_perturb → memset(p, perturb_byte ^ 0xff, n)`,
   `free_perturb → memset(p, perturb_byte, n)`), so the value is the FREED fill and its complement
   is the FRESH one. 255 therefore wrote `0x00` into fresh allocations: exactly the benign
   zero-filled heap the step exists to defeat, and *deterministically*, where leaving the variable
   unset at least returns real recycled garbage. Corrected to `1` (fresh `0xFE`, freed `0x01`).
   Measured three ways: a `malloc` probe (255 → `00 00 00 00`, 1 → `fe fe fe fe`); a sweep of all
   255 selectable values; and a read-before-write reproduction whose peak reads `0` under both
   *unset* and `255` and `1.69e38` under `1`. The original "0xFF fills a float buffer with NaN"
   rationale could not have held at any value — a float of four identical bytes `B` has exponent
   `((B & 0x7f) << 1) | (B >> 7)`, which is `0xFF` only for `B = 0xFF`, and a fresh fill of `0xFF`
   needs `perturb_byte = 0`, glibc's "off" sentinel. NaN coverage is the `sanitizers` job's.
2. **The PE header guard was two bytes short of the read it protected.** It admitted
   `peOffset + 24 <= length` and the code then read the optional-header Magic at `peOffset + 24..25`.
   Corrected to 26 (PE signature 4 + COFF header 20 + Magic 2). Reproduced against synthetic
   truncated images: at 24 and 25 bytes of slack the old bound let `ToUInt16` throw the raw .NET
   `IndexOutOfRange` the function exists to replace; the new bound diagnoses all three of 23/24/25
   and hands ≥ 26 to the next guard.
3. **The Clang warning baseline had a floating reference point.** Its per-(path, flag) counts are a
   property of the compiler major, but `linux-clang` used whatever `ubuntu-latest` resolved `clang`
   to — so a runner-image bump could turn the gate red on a push that changed nothing, the same
   defect the "never key on line numbers" rule exists to avoid, one level up. `ANAMORPH_CLANG_VERSION`
   now pins the major in one place (18 — the version the baseline was generated with, and what
   ubuntu-24.04 resolves `clang` to, so no diagnostic moved); both Clang jobs install and use
   `clang-<n>`; the baseline records `# clang-major:` and the checker **refuses to run** on a
   mismatch (exit 2, not 1). The pin also lets `sanitizers` name `libclang-rt-<n>-dev` directly
   instead of scraping `clang --version` for it.
4. **macOS validated a bundle it did not ship.** The gates ran before the packaging step, and
   `strip -x` + `codesign --force --deep` rewrite the Mach-O immediately afterwards. Both formats'
   gates now run **after** packaging against `dist/Anamorph-macOS/`, named explicitly through
   `ANAMORPH_PLUGINVAL_BUNDLE`. Every platform now validates the bytes it ships. The trade is
   recorded rather than discovered: a packaging failure now skips validation, which is the trade
   Linux already made. The AU copy installed for the registry is removed again afterwards.
5. **The CHANGELOG fence tracker closed on a nested opener.** A backtick fence carrying an info
   string (a `cpp`-tagged opener) nested inside an untagged backtick block ended the *outer* fence,
   after which the block's body was read as
   structure and the real closer re-opened one — inverting the mask to EOF. The extractor now
   applies CommonMark §4.5 in full (same character, at least as long, nothing but trailing
   whitespace) and agrees with `scripts/check-docs.py`'s `fence_mask` on all 11 edge cases tested,
   under both mawk and gawk. All 20 existing CHANGELOG versions extract byte-identically, so the fix
   is latent-only — which is the point, since the tracker exists for the first fenced sample anyone
   adds.

Docs synced: `CI_CD` (the perturbation paragraph rewritten with the mechanism and the sweep; a new
compiler-pin paragraph in §The Clang warning baseline; §Known coverage limits' shipped-bytes bullet
rewritten now that all three platforms qualify, and its AU bullet narrowed to the `auval` point; the
pipeline's step 6 given the post-packaging ordering and its trade; the local-reproduction snippet
switched to the pinned compiler), `TESTING_POLICY` (the Level 1b value and the note that it is not
the fill byte), `TESTING` (the closed-AU-gap entry re-stated for the new ordering and the cleanup
step). Validation for this round is recorded in the round's own summary.

**CI + validation round (0.9.4, no version bump) — reviewed against the sibling product Anabasis
and migrated selectively.**

Anabasis' CI is the newer implementation of the same design, so its workflows, lints and scripts
were read against this repository's and adopted where the reasoning transfers. What landed, grouped
by what it closes:

**Coverage that did not exist.** The macOS **AU** is now validated by pluginval at the same
strictness, both modes ×3 — installed into `~/Library/Audio/Plug-Ins/Components/` with the
AudioComponent registry refreshed first, because a never-installed `.component` can report zero
plugin types however correct it is. The universal binary's **x86_64 slice is now executed**, under
Rosetta 2, via a new `ANAMORPH_TEST_RUNNER` prefix in `run-tests.sh`; it was compiled on every push
and run by nothing. Two new jobs: **`linux-clang`** (Clang's warning set is strictly larger than
GCC's, and it builds the LTO'd plugin — the configuration users install and the only one no other
job compiles with a second toolchain) and **`sanitizers`** (ASan + UBSan, then valgrind memcheck
from an unsanitized build; there was no dynamic-analysis coverage at all). Two new lint jobs,
**`docs`** and **`source-lint`**, carrying four checkers with self-tests.

**Defects the review found.** `--random-seed 0` is pluginval's *"pick a random seed"* sentinel, so
the "deterministic" gate was not deterministic — measured, not inferred (seed 0 printed a different
`Random seed:` per run against pluginval 1.0.4; seed 1 printed `0x1` every time). ~~The Linux
`.gnu_debuglink` stored a CI-workspace-relative path no user's machine has.~~ **WITHDRAWN — this one
was never true; see the head entry.** `objcopy` records only the basename of the
`--add-gnu-debuglink` argument, so the old form stored `Anamorph.vst3.so.debug` exactly as the new
one does. `lipo -archs` output was
printed rather than asserted, and it exits 0 for a thin Mach-O. `find … | head -n1` /
`Select-Object -First 1` picked whichever build enumerated first. `$SUDO DEBIAN_FRONTEND=… apt-get`
breaks when `$SUDO` is empty. `build.sh` exited 1 after a successful build whenever an optional
artefact was absent, silently breaking `build.sh && run-tests.sh`. Every same-repo PR built the
3-OS matrix a second time, and concurrent pushes raced full matrices.

**What was NOT migrated, and why** — recorded so the omissions are decisions rather than gaps:
Anabasis' `preflight` job (a pre-P1 scaffold guard; this repository has had a `CMakeLists.txt`
since long before 0.9.0, so it would be a permanent no-op adding a `needs:` edge to every build);
`cxx23-canary.yml` (it exists to pre-warn a C++20 → C++23 baseline raise, and this project is
*already* at C++23 — there is nothing to canary); the `channel_probe` / `engine_repro` host-side
tools (product code written around a specific Anabasis field report, not CI infrastructure — porting
them means authoring new C++, and pluginval already loads the built bundle through
`juce_audio_processors` at strictness 10); the `macos-*-intel` native-Intel job (a real gap, but
Rosetta execution closes the cheap 95% of it for no extra runner — recorded in `CI_CD.md` §Known
coverage limits, not silently dropped); and `.gitattributes` (Anabasis needs `text eol=lf` because
its snapshot fixture is compared **byte-wise**; this repository's comparison is line-based through
`juce::StringArray::fromLines`, which strips `\r`, so the hazard does not reach it).

**The one adaptation worth naming.** The Clang warning gate could not be adopted as-is: this tree
already carries 14 first-party Clang warning sites, and clearing them means renaming a member across
the editor, adding cases to engine switches and changing float comparisons in DSP code — source work
that belongs in its own review under `DSP_POLICY.md`. Landing the job red teaches people to ignore
it; landing it non-blocking makes a gate that cannot fail. So it asserts **no new** warnings against
a checked-in baseline keyed on `(path, flag)` with a site count — never line numbers, which drift on
unrelated edits — and a falling count is a notice asking the baseline to shrink, never a failure.
The debt list is `scripts/clang-warning-baseline.txt` and is reproduced in `CI_CD.md`.

Docs synced (`DOCUMENTATION_LIFECYCLE_POLICY` trigger map, **CI workflow → `CI_CD.md`,
`TESTING.md`**): `CI_CD` (triggers + concurrency + the same-repo-PR guard, the strictness single
source, the seven-job build matrix and what each non-packaging job is for, pipeline steps 4–7, the
AU gate, the Rosetta slice, the baseline, evidence anchors, a new **Known coverage limits** section,
and a rewritten local-reproduction section), `TESTING_POLICY` (Level 1 restated to include the
warning gate and the lints, a new Level 1b for dynamic analysis, the hard gate reworded for AU +
both formats + the nonzero seed + ambiguity, a new rule 4 requiring every lint to prove it is live,
and the strictness number **removed** in favour of the workflow's `env:` block), `TESTING` (the
pluginval section rewritten for the seed/format/bundle-override/ambiguity behaviour, the CI section
given the four new jobs and the citation-base warning, and the **AU gap struck through as closed**),
`REPOSITORY_MAP` (the tree comment, all nine script rows, the `build.yml` row). Six stale
`file:line` anchors into the two rewritten scripts were re-anchored in this same change set
(`POSTMORTEMS`, `FUTURE_RISKS`, `KNOWN_ISSUES`, `TROUBLESHOOTING`, `TESTING` ×2), which is the rule
`check-citations.py` now enforces for `src/`+`tests/`. Three pre-existing blockquote
lazy-continuation defects that `check-docs.py` found on its first run — a line-wrapped `> 1.5 oct`
landing at column 0 in `CHANGELOG`, `DSP_ALGORITHMS` and `ADR-0015` — were fixed by reflowing, with
no wording change.

**Validation.** actionlint (with shellcheck) clean on all five workflows; every pwsh step and
`run-pluginval.ps1` parsed with the PowerShell 7.4 parser; `bash -n` + shellcheck clean on all
scripts. All four lints green with their self-tests (`check-docs` 57 cases / 99 files,
`check-clang-warnings` **28** cases, `check-portability` 45 files, `check-citations` 198 anchors).
(That figure was recorded as 24 when this entry was written and is corrected here rather than left
as history: the whole point of writing a case count down is that a reader can re-run the command and
get the same number. The four additional cases are the compiler-pin round's version round-trip
block — see the CI review follow-up entry above. The other three figures reproduce unchanged.)
Both suites green under GCC, under Clang, and under ASan + UBSan (140 and 894 checks each time).
pluginval strictness 10 green, both modes ×3, with the seed observably pinned at `0x1`. The lld
probe reports `Success` under Clang and is **absent** under GCC, confirming the shipped Linux link
is unchanged. ~~The new debuglink was read back off the stripped binaries.~~ **WITHDRAWN with the
claim above: reading the section back is what would have shown both forms produce the same bytes, so
whatever was done here, it was not that.** The warning gate was
adversarially checked: it fails on a new file, fails on an extra site in an already-baselined
file+flag, and ignores a new vendored warning. [Verified]

**AppleClang 21 `-Wimplicit-int-float-conversion` × 4 (0.9.4, no version bump) — a source change
that provably changes no machine code.**

The runner move below surfaced four of these; this follow-up resolves all four. Each is an `int`
operand widened inside a float expression, and each now carries the explicit `(float)` cast that
spells out the conversion the compiler was already performing:

| Site | Before | After |
|---|---|---|
| `src/PluginEditor.cpp:246` | `roundToInt (inner.getWidth() * 0.40f)` | `roundToInt ((float) inner.getWidth() * 0.40f)` |
| `src/PluginEditor.cpp:247` | `roundToInt (getWidth() * 0.40f)` | `roundToInt ((float) getWidth() * 0.40f)` |
| `src/gui/LookAndFeel.cpp:262` | `x0 + k * (barW + gap)` | `x0 + (float) k * (barW + gap)` |
| `src/dsp/VelvetNoise.cpp:30` | `std::round (m * cell + …)` | `std::round ((float) m * cell + …)` |

**Nothing is suppressed.** No `#pragma`, no `-Wno-…`, no change to
`juce_recommended_warning_flags`; the diagnostic is resolved at each site by making the intended
conversion explicit, which is also the idiom the surrounding code already uses (the two lines
above the VelvetNoise site read `(float) decorrSamps / (float) maxTaps`).

**Behaviour is provably unchanged, not argued.** `int * float` performs the usual arithmetic
conversion of the `int` operand to `float` and then multiplies; `(float) i * f` is that same
conversion written out. Verified rather than reasoned: each of the three translation units was
compiled at the shipped flags with `-g0 -fno-lto` appended (so debug metadata and LTO bitcode
cannot mask the comparison) before and after the edit — **all three objects are byte-identical**.
That covers the whole translation unit, which is a stronger statement than the scenario-matrix
twin dump used for the JUCE and C++23 changes. `src/dsp/VelvetNoise.cpp` is DSP code, so this
matters: the velvet tap grid is bit-exact, i.e. Class A.

**Verified on the diagnosing toolchain, and swept for stragglers.** On CI run `31900529457`
(`macos-26-arm64`, AppleClang 21.0.0.21000101) the macOS job's normalised warning set is
**15 sites / 108 instances, `diff`-identical to the `macos-14` / AppleClang 15 set** — the image
change added four diagnostics and this change removed exactly those four, with nothing else moved.
A full local Clang 18 build of both self-test targets (**56 compilations**) then found **zero**
`-Wimplicit-int-float-conversion` anywhere in the project sources, so no unreported site was left
behind; only the pre-existing `-Wsign-conversion`/`-Wswitch-enum`/`-Wmissing-prototypes`/
`-Wfloat-equal`/`-Wshadow*`/`-Wunused-but-set-variable` families remain. Gates re-run on the
change: 140-check DSP + 894-check state suites green, pluginval strictness 10 green in both modes
×3 locally (no retry) and on all three CI platforms.

**A rejected alternative, for the record.** `juce::Rectangle::proportionOfWidth (0.40f)` would
read better at the `PluginEditor` sites but returns `ValueType (w * p)` — a **truncation**, where
the existing code rounds. That is a behaviour change, so the cast was preferred.

**Not gated, and no CHANGELOG entry.** No parameter, serialization, threading, DSP-order or
latency surface is touched, and the machine code is identical, so no
`ARCHITECTURE_REVIEW_GATE` item applies and no ADR is warranted. `CHANGELOG_POLICY` rule 3
excludes it: nothing a user of the plug-in can observe changed.

Docs synced: `CI_CD` (its toolchain paragraph recorded these four as unfixed and now records the
fix and the byte-identical-object evidence) and the **KNOWN_ISSUES** / **FUTURE_RISKS** v0.9.4
version-sync headers, which pointed at `CI_CD` for the same four diagnostics. No other document
named them.

**macOS CI runner `macos-14` → `macos-latest` (0.9.4, no version bump) — a CI-workflow change, one
line of YAML, no source and no build-configuration change.**

`actions/runner-images` marks the macOS 14 images **deprecated**: deprecation opened 2026-07-06,
eight brownouts run through October 2026, and the labels are **fully unsupported on 2026-11-02**,
after which a job carrying `macos-14` is terminated with an error. `macos-latest` currently
resolves to **macOS 26 Arm64**. The `macos` job now uses the floating label, matching
`ubuntu-latest`/`windows-latest` on the other two jobs of the same matrix; nothing else in the job
moved — same configure line (`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`,
`CMAKE_OSX_DEPLOYMENT_TARGET=10.13`), same self-tests, same two pluginval strictness-10 gates,
same `dsymutil` → `strip -x` → ad-hoc-codesign order, same `.pkg` packaging. `release.yml` reuses
this job via `workflow_call`, so the release pipeline inherits the new image with **no edit of its
own** — it is the single point of definition, and the workflow files were re-parsed to confirm
`macos` is the only job that names a macOS runner.

**Why the floating label rather than pinning `macos-15`/`macos-26`.** A pinned image is a
scheduled outage: it goes end-of-life and fails the entire matrix on removal day. The floating
label moves with GitHub's rollout, and this job's existing gates are what detect a bad image —
`lipo -archs` proves both universal slices are present (the x86_64 half is cross-compiled on an
arm64 runner), the DSP and state suites prove behaviour, pluginval proves format conformance, and
the packaging self-checks prove the `.pkg`. The trade is recorded rather than assumed: a floating
label can change the AppleClang/SDK under the shipped macOS binaries without a repository commit,
which is the same exposure the project already accepts on `ubuntu-latest`/`windows-latest` and the
same class as ADR-0027's MSVC `/std:c++latest` caveat.

**Validated by the runner it changes.** CI run `31895877794` on `macos-latest` resolved to image
`macos-26-arm64` `20260728.0273.1` and went green end to end: configure accepted
`CMAKE_OSX_DEPLOYMENT_TARGET=10.13`, the universal build succeeded, both self-test suites passed,
pluginval strictness 10 passed in **both** modes ×3, `lipo -archs` reported `x86_64 arm64` for all
three bundles, and `Anamorph-0.9.4-macOS.pkg` built with its three components and passed the
`installer -pkginfo` / `pkgutil --expand` self-checks. The degenerate-dSYM path (`-debug` upload
skipped under Release+LTO) behaves exactly as on `macos-14` — compared against the previous green
macOS job, not assumed. Linux and Windows were unaffected and green in the same run.

**The measured consequence: the macOS compiler moved with the image.** AppleClang
**15.0.0.15000309 (Xcode 15.4) → 21.0.0.21000101 (Xcode 26.6)**. Diffing the macOS warning sets of
the two runs, normalised, gives 15 → 19 distinct sites and 108 → 126 instances: nothing
disappeared, no category changed, and the whole delta is
**`-Wimplicit-int-float-conversion` at four pre-existing sites** —
`src/PluginEditor.cpp:246, 247` (`getWidth() * 0.40f`), `src/gui/LookAndFeel.cpp:262`
(`k * (barW + gap)`) and `src/dsp/VelvetNoise.cpp:30` (`m * cell`), each an `int` widened inside a
float expression. **Recorded, not fixed here** — the source was unchanged by this change, so these
were new diagnostics on old code, and Level 1 is not part of the `TESTING_POLICY` hard release
gate; they are **resolved in the follow-up entry above**. Bit-exact macOS output
across the two compilers is **not claimed** — it is not provable headlessly from this repository,
and compiler-level numerical differences are the Class-B changes `DSP_POLICY.md` permits (RH-F4).
The behavioural gate is what carries the claim.

**Not a gated change and not a CHANGELOG entry.** `ARCHITECTURE_REVIEW_GATE`'s Build System item
covers CMake structure, the JUCE version/pin and the dependency set; a runner label is none of
those, and `DEPENDENCY_POLICY`'s pinned-dependency table does not list it — so no ADR, unlike
ADR-0027. `CHANGELOG_POLICY` rule 3 (user-visible changes only) excludes it, matching the
precedent HANDOVER already records for the CI-side PRs #65–#75.

Docs synced (`DOCUMENTATION_LIFECYCLE_POLICY` trigger map, **CI workflow → `CI_CD.md`,
`TESTING.md`**): `CI_CD` (the build-matrix runner cell plus a paragraph on the deprecation dates
and the floating-label choice), `TESTING` (the RH-F3/auval feasibility sentence names the runner),
and the two documents that repeat that same sentence — `KNOWN_ISSUES` KI-014 and
`RELEASE_HARDENING_PLAN` RH-F3 — plus `BUILD`'s toolchain line, whose "Verified on …" record names
the macOS compiler, and the **KNOWN_ISSUES** and **FUTURE_RISKS** v0.9.4 version-sync headers
(each states what the version's changes did to that document; both record "no entry added" with
the reason). `COMPATIBILITY_MATRIX` needed no edit: its macOS row cites the workflow, not an image
label, and the deployment target and both architectures are unchanged. `HANDOVER` needed none: its
Build Status row claims all three CI platforms green, which the new image satisfies.

**Drift found and corrected while re-reading that BUILD line (C6).** It read "AppleClang 15.4",
which is the **Xcode** version of the `macos-14` image; the compiler
`CMAKE_CXX_COMPILER_VERSION` actually reported there was **15.0.0.15000309**. The line now gives
the compiler version with the Xcode version alongside it, for both the old and the new image. The
same conflation appears in `ADR-0027` §Verification and the C++23 worklog; those are dated records
of that change and are left as written.

**Dated records left alone (C6, report-don't-rewrite).** `ADR-0027` §Verification, both C++23
worklog tables, `worklogs/RELEASE_HARDENING_AUDIT_v0.9.0.md:160` and the `macos-14` run-ID comment
in `packaging/macos/build-pkg.sh` state what was measured **on the image of the day**. They are
historical evidence, not live statements, and are not retro-edited.

**C++ standard 17 → 23 (0.9.4, no version bump) — a Build System change with a one-line source diff.**

The C++ standard sits in the `DEPENDENCY_POLICY` pinned-dependency table, so raising it is an
`ARCHITECTURE_REVIEW_GATE` Build System change: it carries an ADR (**ADR-0027**, `Accepted`
2026-08-15 — Architecture Review signed off) and was flagged on the PR for that review. `CMAKE_CXX_STANDARD` moved 17 → 23 in place
on `CMakeLists.txt` `:16`, so no downstream `CMakeLists.txt:NNN` citation moved; the version stays
0.9.4 and the release date stays 2026-08-15, as commissioned.

**The one source change is a finding, not a cleanup.** `src/dsp/HaasProcessor.cpp` gains
`#include <algorithm>`: libc++ stops including `<algorithm>` transitively at
`_LIBCPP_STD_VER >= 20`, so `std::fill` lost its declaration under `-std=c++2b` and **the macOS
CI job failed** — the defect was invisible to GCC/libstdc++ and to the MSVC STL, both of which
still supply the include. A `-fsyntax-only` sweep of all 27 project translation-unit compilations
under Clang 18 + libc++ + C++23 then found no second occurrence, so no wider include-hygiene pass
was made. The **C++20 fallback was evaluated and not taken**: one missing standard include is an
ordinary compatibility adjustment.

Docs synced: CHANGELOG `[0.9.4]` (a second `### Changed` entry above the JUCE one), new ADR-0027
+ `ADR_INDEX` row, `DEPENDENCY_POLICY` (pin table + a new compliance-log entry), `CODE_STYLE`
(the language line), BUILD (toolchain + the `:16-18` evidence citation), README (Requirements
**and the 0.9.4 status bullet**), COMPATIBILITY_MATRIX (toolchain pin row), HANDOVER (the
C++-standard dependency row, the snapshot paragraph, Current Version, Build Status, Release
Status and Known Blockers), the **KNOWN_ISSUES** and **FUTURE_RISKS** version-sync headers,
`.github/workflows/codeql.yml` (its header comment named the standard), and
`worklogs/CXX23_MIGRATION_v0.9.4.md`. CI_CD needed no edit — it names no standard.
`TESTING.md`/`TESTING_POLICY` needed none: no test was added, changed or removed, and the gate
is unchanged.

**A release-status correction the first pass missed.** Four 0.9.4 statements still described the
version as JUCE-only with **no `src/` change** — the README status bullet, the HANDOVER snapshot
paragraph, and the KNOWN_ISSUES and FUTURE_RISKS version-sync headers. That was true of the JUCE
upgrade alone and stopped being true of the *version* once the C++23 migration landed in it; all
four now name both changes and describe the one-line `src/` diff. The JUCE-scoped statements that
say "no source change" **about the JUCE bump** (CHANGELOG's JUCE entry, the JUCE section below)
are correct as written and are left alone.

**Why the warning counts differ between the two 0.9.4 entries.** The C++23 records cite **27
compilations / 29 instances**; the JUCE records cite **18 / 19**. Same measurement, different
command set: the JUCE cycle measured `AnamorphStateTests` alone, this one measured both self-test
targets, so the 8 shared `AnamorphDSP` sources are counted once per target and
`tests/dsp_tests.cpp` is covered at all. Re-measuring the narrower set at C++23 returns exactly
18/19, so nothing drifted. Stated in CHANGELOG, `DEPENDENCY_POLICY`, ADR-0027 and
`worklogs/CXX23_MIGRATION_v0.9.4.md` §4.4; the numbers themselves are unchanged.

**Dated records left alone (C6, report-don't-rewrite).** `ADR-0022:39`, `ADR-0026:36` and the two
JUCE worklogs each state the toolchain contract **as of that change**, where "C++17" was true.
They are historical records, not live statements, and are not retro-edited.

**One open caveat is carried rather than buried.** MSVC ships no stable `/std:c++23`, so CMake
maps `CXX_STANDARD 23` to `/std:c++latest` on Windows — a documented moving target. It is
recorded in ADR-0027 §Consequences, the DEPENDENCY_POLICY compliance entry, BUILD, HANDOVER and
worklog §5 with the two escape hatches, and it did not block adoption: the Windows job builds and
passes strictness 10 in both modes.

**JUCE 9.0.0 → 9.0.1 dependency upgrade (0.9.4) — a Build System change with no source diff.**

A JUCE bump is an `ARCHITECTURE_REVIEW_GATE` Build System change, so it carries an ADR
(**ADR-0026**, `Accepted` 2026-08-15) and the `DEPENDENCY_POLICY` rule-2 verification, and it was
flagged on the PR for human Architecture Review. The pin moved to the tag's immutable commit
`e18f7f5…`; `project(... VERSION)` moved 0.9.3 → 0.9.4 with the release dated 2026-08-15.
**No C++ source change and no build-dependency change were required**, and that is a finding
rather than an assumption: neither of the two breaking changes upstream records for 9.0.1 reaches
the project, and no JUCE module Anamorph links altered its declared system packages — across all
fourteen module headers the only metadata difference is the `version:` field, so
`scripts/setup-linux.sh` is untouched.

Docs synced: CHANGELOG `[0.9.4]`, ADR-0026 + `ADR_INDEX`, `DEPENDENCY_POLICY` (pin table,
version-lock rule, compliance log), BUILD, TROUBLESHOOTING, README, REPOSITORY_MAP,
COMPATIBILITY_MATRIX, FUTURE_RISKS (RISK-001 + the version-sync header), HANDOVER (the snapshot
paragraph + Current Version / Build Status / Release Status / Known Blockers / the JUCE dependency
row), RELEASE_PROCESS and COMMERCIAL_STATUS (the tag/release-in-preparation statements),
THIRD_PARTY_LICENSES + TRADEMARKS + COMMERCIAL_STATUS + RELEASE_HARDENING_PLAN (the pinned
version each cites), `.github/dependabot.yml`, and `worklogs/JUCE901_UPGRADE_v0.9.4.md`. CI_CD
needed no edit — it is version-free by design.

**A tag-name consequence, recorded rather than assumed.** `git tag` is still empty, so the first
annotated tag becomes **v0.9.4** — v0.9.3 joins 0.9.0-0.9.2 as written-up-but-never-cut. That
moved three live statements: `CHANGELOG_POLICY` rule 2, the CHANGELOG preamble and RISK-003's
mitigation.

**Citation sweep.** Of the 22 JUCE source files cited by name-and-line anywhere in the docs,
exactly **three** differ between the two tags (`juce_audio_formats.h`,
`juce_MouseInputSourceImpl.h`, `juce_Windowing_linux.cpp`); the other 19 are byte-identical, so
their citations stand unchanged. The three were re-anchored where a *live* document cites them
against the current pin — `THIRD_PARTY_LICENSES` §2/§4 (the FLAC/Ogg and MP3 config defaults) and
`KNOWN_ISSUES` KI-018/KI-019 (whose mechanisms are re-stated against 9.0.1, both re-verified
byte-identical in substance). `POSTMORTEMS` and the worklogs were deliberately **not** touched:
each is a dated record that names the tree it was traced in ("from the pinned JUCE 9.0.0
source"), so its line numbers are correct for that tree — the same period-correct-history rule
the 8.0.14 → 9.0.0 migration applied.

**Drift found and reported (C6), not silently carried.** `.github/workflows/codeql.yml` still
described the dependency as "JUCE 8.0.14" — stale since the 9.0.0 migration, i.e. pre-existing,
not introduced here. The sentence's point is version-independent, so the minimal correction was
to drop the number rather than restate it, which also stops the line drifting again.

**What is verified and what is not.** Verified headlessly: 32/32 twin-dump hashes **and**
reported/predicted latencies identical across the two JUCE versions (the ADR-0022 harness re-run,
with the 32 hashes mutually distinct so the matrix discriminates); 140-check DSP and 894-check
state suites green, including the 8.0.14-frozen parameter-registry snapshot; pluginval strictness
10 green locally in both modes ×3; a byte-identical compiler-warning set (19 instances) across the
18 project translation units built against both trees; and the `RELEASE_POLICY` third-party
re-verification (JUCE's `LICENSE.md` and all twelve cited licence files byte-identical). **Not
verifiable headlessly, and therefore done by hand:** the Level-5 manual audition — 9.0.1 changes
editor-adjacent framework code (Linux message-loop scheduling, display enumeration, vblank period;
Windows Direct2D edge painting; macOS Metal-layer guards), and appearance/feel is a human
judgement. That audition was **performed against this build on 2026-08-15**, so **ADR-0026 is
`Accepted`**; the same audition discharges the one ADR-0022 had left open for the 9.0 line, which
is now `Accepted` too. Two of the four long-standing tag blockers close with it (`HANDOVER`
§Release Status): the compatibility checklist and the missing licence remain.

**Probe-state fix (0.9.3) — the staging probe could decide future installs.**

The hard-link probe that chooses the staging location writes one marker into the plug-in directory,
and `ln` refuses an existing target. A run killed between creating and removing that marker left it
there for good, and from then on the probe failed on **every** later run — pinning staging to the
in-scan-path fallback permanently, which is exactly the arrangement the round before had moved away
from. Reproduced against the tree before fixing: with a marker pre-placed in `~/.vst3`, the install
succeeded but staged inside the scan directory. `uninstall.sh` did not list the marker either, so it
survived a full uninstall and contradicted the "leaves nothing that survives a deliberate uninstall"
line in `PACKAGING.md`.

The fix is to make the marker **stateless** rather than to chase its cleanup: it is removed up front
on every run, before the probe and before the recovery paths that never probe, so a leftover can only
ever be litter. Deleting it only *after* use would not have been enough — that is the same
kill-between-two-commands window that created the bug. `-rf` rather than `-f`, so even a directory
under that name cannot pin the choice; nothing but the probe is ever written to that path. The
uninstaller's scratch list gained the marker so an interrupted install leaves nothing behind.

Verified by execution in both modes: stale marker (file and directory), `INT`/`TERM`/`HUP`/`KILL`
delivered inside the probe window itself — after which the installed plug-in is untouched, because
the probe runs before any staging, and the **next run stages outside again** — repeat installs
leaving no marker, and uninstall clearing one. The full transaction matrix was re-run unchanged, so
the stage-and-swap and recovery guarantees are intact. No new known issue: this defect is fixed, not
carried.

**Consolidated installer round (0.9.3) — the transaction finished, and the limitations registered.**

*The transaction, completed rather than patched again.* Two prior rounds each fixed the failure the
review named and left the adjacent one standing; this round took the whole lifecycle. What changed
beyond the previous fix: staging moved **out of the DAW scan path** (`.anamorph-install-stage` next
to the plug-in directory), chosen by a **hard-link probe** — the one operation that cannot cross a
filesystem, where the obvious `mv` probe is no test at all because it silently falls back to
copy-and-unlink, and `~/.vst3` may be a symlink onto another mount. A false negative falls back
inside the plug-in directory and costs only the scan-path property, never atomicity; this was
verified against a real second filesystem, which is also how the missing `mkdir` for that fallback
was found. Both modes now share one `choose_stage_dir`/`reconcile`/`arm_traps` implementation
instead of two near-copies that had already drifted once. The elevation prefix is declared once at
the top and stays empty on the per-user path.

*Sequencing bug found by testing, not reading.* `reconcile` sweeps an empty stage directory as
scratch, so creating that directory **before** the opening reconcile meant an upgrade (destination
present) had its stage directory deleted underneath it. Creating it after reconcile is the fix. The
same class as the previous round's finding: the recovery helper must not destroy what the
transaction still needs.

*Uninstall made consistent with what install owns.* `uninstall.sh` removes both possible stage
directory locations and the staged Standalone, by exact name, so an interrupted install leaves
nothing that survives a deliberate uninstall. It matches no patterns and touches no user data.

*Coexistence warning.* A per-user install now detects an existing system-wide one (`test -e`, no
elevation) and names what is still installed there plus the `sudo ./uninstall.sh` that clears it.
Detection only — the maintainer's decision, recorded here; removing it would need exactly the
elevation the per-user mode exists to avoid.

*macOS assertion strengthened from name to semantics.* Proving `<bundle-version>` is *producible*
did not prove its membership tracks `BundleIsVersionChecked`. The build now runs a controlled A/B on
one payload — same bundle, packaged with pkgbuild's defaults and with the patched plist — and
requires each list to appear with the key on and vanish with it off. `<upgrade-bundle>` gained an
assertion too, which closes the one patched key that had none. The stale "nested bundles are covered
too" claim was corrected in both the script and `PACKAGING.md`: nested bundles appear under the
parent's `ChildBundles`, are not patched, and do not need to be.

*Guarantees reconciled with what the code actually provides.* "Nothing half-installed" was too
strong: the VST3 and the Standalone are two artifacts, each replaced atomically, and a failure
between the two commits leaves a new VST3 with the previous Standalone — both valid, a mixed pair.
`PACKAGING.md` now states the guarantee per artifact with a point-of-failure table, the CHANGELOG
entry says what the user actually gets, and the macOS "every selected component is written" line
notes that components install in sequence with no rollback.

*Registered, not silently solved (DOCUMENTATION_LIFECYCLE trigger: new unresolved limitation).*
**KI-021** (Linux per-user install does not displace a system-wide one) and **KI-022** (macOS
non-relocatable packaging leaves a user-moved copy behind) now exist in `KNOWN_ISSUES.md`, the list
testers and the release checklist actually consult, with the procedure docs cross-referencing them.
Both are deliberate trades and neither had a register entry despite being documented in packaging
procedure — the gap the review named.

*Maintainer sign-off, 2026-08-11 (recorded, not re-requested).* The judgement calls in this round —
that the coexistence warning is wanted, that both limitations are documented rather than
behaviourally redesigned, and that the macOS semantic assertion is required — were approved by the
maintainer in advance. This is a **decision** sign-off; it is not manual testing and is not recorded
as any.

**Final review round (0.9.3) — two defects in the packaging round, plus the visual sign-off.**

*The fix reproduced the defect's own shape.* The INC-012 prevention assertion for version checking
was keyed on `<version-check>`, an element pkgbuild never writes — so it passed unconditionally. A
check that cannot fire is the same silent success as an install that cannot install, which makes this
worth recording rather than quietly correcting: the sibling `<relocate>` assertion was named
correctly and did work, and the asymmetry is invisible in a green build. In `PackageInfo` these
states are membership lists (`<relocate>`, `<bundle-version>`), so the name is now correct **and
proved on every build** — a throwaway component built from the same payload with the defaults left
on is relocatable and version-checked by definition and must match both patterns, and if it does not
the build stops and prints that `PackageInfo`. Verifying an assertion by construction rather than by
inspection is the transferable part; the same reasoning produced the `PackageInfo` **count** guard in
the round before, for the same class of vacuous pass.

*The replacement path destroyed what it was replacing.* `install.sh` removed the installed
`Anamorph.vst3` before copying its replacement, so a copy that failed part-way left the user with no
plug-in at all — contradicting the "nothing half-installed" guarantee this file and `PACKAGING.md`
already asserted. Both modes now stage beside the destination and swap. Two notes: the **per-user**
path had the identical defect and was fixed with it (the review cited only the system-wide path, but
per-user is now the *default* path, so fixing one and leaving the other was not a defensible release
state); and staging beside the destination rather than in `/tmp` keeps the final step a
same-filesystem rename, which additionally replaces a **running** Standalone that `cp` refuses with
`Text file busy` — verified directly, along with a control run proving the pre-fix script destroys
the install under the same injected failure.

*Wording.* The installer's title is now `Anamorph Linux Installer`, matching
`Anamorph Linux Uninstaller`; the two docs that quote the prompt were updated with it.

*Follow-up: the stage-and-swap still had an interruption window.* The swap deleted the destination
before renaming the staged copy in, so between those two commands the staged copy was the only
one — and the cleanup handler removed exactly that on the way out, turning a Ctrl-C into total
loss. The stage-and-swap shape was right; the **order** was not. This round made the old bundle move
**aside** instead of being deleted, so a complete copy exists at every instant, and made cleanup
restore rather than only delete. (The staging paths it used were superseded by the consolidated
round above, which moved them out of the DAW scan path; the ordering property is unchanged.) Three things this round established that are worth
carrying forward: `EXIT` traps are **not** enough for interruption — dash, `/bin/sh` on
Debian/Ubuntu, does not run them when the script is signalled, so `INT`/`TERM`/`HUP` are trapped
explicitly (measured, not assumed); the restore must be **ordered before** the scratch removal
*and* the parked copy kept until the destination is repopulated, a flaw the first draft of this fix
still had and the failure tests caught; and `SIGKILL` — which no handler covers — now leaves the
old bundle parked and recoverable, with the next run's opening `reconcile` restoring it. Verified
by injecting a failing commit rename and by delivering `INT`/`TERM`/`HUP` inside the window in both
modes (elevated included), each against a control run of the previous script that ends with nothing
installed. `PACKAGING.md`'s mechanism paragraph is corrected; its guarantee wording already
described the intended behaviour and stands unchanged.

*Inspected and accepted as non-blocking, with reasons.* A per-user install does not displace an
existing system-wide one — real and, for 0.9.3, the likely upgrade path, but the fix would need the
elevation that mode exists to avoid, so it is **documented** (`INSTALL.txt`, `INSTALLATION.md`,
`PACKAGING.md` §Not chased) rather than coded, mirroring the macOS stale-copy treatment. The `read`
EOF fallback discards a value only when a user types an answer and presses Ctrl-D instead of Enter on
a tty; the cited pipe repro cannot reach it at all, because a pipe fails `[ -t 0 ]` and never
prompts. `plist_put`'s Set/Add fallback is covered by the assertions now that the version key has a
live one. The remaining items (pkgbuild bundle classification, per-component postinstall
non-atomicity, the earlier pop-up/tooltip/focus/z-order/`SpectrumImager` findings) were re-read and
need no change; the macOS guarantee text was checked and does not claim atomicity. The
permission-denied message stays as written — it is maintainer-specified wording (C8), even though it
now also prints for non-permission failures.

*Sign-off recorded (2026-08-11).* The maintainer approved the **visual** items: the equal-width Widen
/ Style-Focus row is intentional, the narrower Simple-mode Widen control is acceptable, the current
pop-up/menu width behaviour is acceptable, and the remaining visual verification items are approved.
Recorded in `TESTING.md` (ADR-0025 disclosure 2) and `worklogs/GUI_INTERACTION_FIXES_v0.9.3.md` §7
and §10. **Scope of that sign-off:** visual/UI only — the behavioural per-platform checks and every
**installer** check (the macOS four-case re-install matrix, a DAW finding `~/.vst3` on Linux) are not
covered and remain owed.

**Packaging round (0.9.3) — `packaging/` only, no `src/` change.** Two independent installer items,
both requested with an explicit scope restriction to their own platform, and both verifiable only
where CI does not go.

*Linux — an install that no longer needs root.* `install.sh`/`uninstall.sh` now prompt for one of two
modes and **default to the per-user one** (`~/.vst3` + `~/.local/bin`), which matches how Linux DAWs
actually scan: `~/.vst3` is the VST3 standard's per-user folder and a default path in
REAPER/Bitwig/Ardour, so nothing is lost by not writing to `/usr`. The design decisions worth
keeping: elevation is **per operation** (`priv() { $SUDO "$@" || fail; }`), never a re-exec of the
script through `sudo`; **root skips the prompt** and installs system-wide, so the previously
documented `sudo ./install.sh` keeps its exact old behaviour rather than becoming a per-user install
into `/root`; a **non-tty stdin** takes the default instead of blocking; and every unrecognised answer
falls back to the default, as specified. The two failure paths fail **closed** — no `sudo` on `PATH`
and a `sudo` the user cannot authenticate both print their message and exit 1 with nothing
half-installed. Verified on Linux against a stubbed payload across the mode matrix, the failure paths
and install→uninstall round-trips in both modes (recorded in `TESTING.md`); what that cannot show is a
real DAW finding `~/.vst3/Anamorph.vst3`, which stays a manual check. Docs synced per the lifecycle
trigger (**Packaging** → `PACKAGING.md`, `RELEASE_PROCESS.md`) plus the user-facing carriers that
asserted the old behaviour: `packaging/linux/INSTALL.txt`, `docs/user/INSTALLATION.md`,
`USER_MANUAL.md` §2.1, `README.md`, `REPOSITORY_MAP.md`, `CI_CD.md` §8.

*macOS — INC-012, an installer that reported success without installing.* `pkgbuild` marks every
bundle it finds **relocatable by default**; Installer.app then resolves the destination by looking the
bundle identifier up in the receipt/Spotlight database and writes over **whatever copy it finds**,
using `--install-location` only when the lookup comes up empty. Move `/Applications/Anamorph.app`
elsewhere — dragging it to the Trash counts, since that is still a file on the volume and still
indexed — and the next install reports success while `/Applications` stays empty. `build-pkg.sh` now
patches the plist `pkgbuild --analyze` produces (rather than hand-writing one, so
`RootRelativeBundlePath` matches by construction and nested bundles are covered) and passes it back
via `--component-plist`. The audit-relevant point is **which** claim was wrong: the pre-fix build-time
self-check verified the *package* thoroughly — three component identifiers, `customize="allow"`, all
choices pre-selected — and that check was correct and remains; relocation is simply not a property of
the archive, only of install-time behaviour, so no amount of package inspection could have caught it.
The new assertions therefore cover the two things that *are* inspectable (no relocatable and no
version-checked bundle in any `PackageInfo`; `pkgutil --expand-full` payload completeness), and the
rest is now an explicit **coverage gap**: `TESTING.md` §"Gaps in the automated coverage" gained a
fifth bullet — *no gate ever installs anything* — carrying the owed four-case re-install matrix per
format. `PACKAGING.md` gained §"macOS reinstall behaviour (idempotency)" recording the destinations,
the guarantee, the three plist keys and the receipt assumption (receipts are still written but are
never read to decide where or whether to copy, so `pkgutil --forget` is never needed to make an
install work).

*Drift found and corrected (C6).* `HANDOVER.md`'s Current Version row still ended the menu-width
paragraph with "no layout code changed in the cycle" — true when written, contradicted by the
equal-width Widen row that landed later in the same cycle. The 2026-08-11 round corrected this
sentence in this file (see the layout entry below) but not the HANDOVER instance. Corrected to name
the Widen row as the cycle's one deliberate layout change; the entry-count in the same row (six
Fixed / two Changed) is now seven / three.

**Follow-up round on the pop-up work (0.9.3).** Four items, three of them corrections to the change
set itself. *(1)* The shield is **always visible and inert**, with only its *interception* toggled —
the shape `dimOverlay` already uses here, so it is one fewer idiom. The reason is `setVisible`'s
repaint cost, not hover: raising the shield cannot disturb hover at all, because every fake mouse
move involved is **asynchronous** and therefore dispatched once the menu is already modal, at which
point `internalMouseEnter`/`internalMouseExit` early-return for every blocked component — and this
editor derives hover **geometrically** rather than from enter/exit in any case. *(2)* The claim that
the preset menu cannot reach the look-and-feel hook was
**re-verified and holds**, with a sharper reason: `MenuWindow` binds `auto& lf = getLookAndFeel()`
*before* parenting and calls `preparePopupMenuWindow` through that bound **reference**, which
parenting cannot rebind — so the separate counter stays and the comment now carries the real
argument. *(3)* The 24 Hz backstop's comment was read twice as covering `presetMenusOpen`; it covers
`openMenus`, and the counter needs no cover because `showPresetMenu` always adds three unconditional
items, so `createWindow` can never return null — the one path that drops the callback. Comment
corrected; **no recovery machinery added for a statically unreachable state**. *(4)* **Tooltips**:
disabling them left a visible tip up and a quick move could raise another, because the setting only
lengthened `millisecondsBeforeTipAppears` and `TooltipWindow::timerCallback` bypasses that delay
entirely while a tip is showing. `getTipFor` is virtual, so tooltips are now switched off **at the
source** and JUCE's own state machine hides rather than shows; `hideTip()` makes the transition
immediate. Filed as **KI-018**, not fixed: the dismissing click is consumed by the shield but still
counts toward JUCE's multi-click run (`registerMouseDown` is component-agnostic), and every lever is
out of bounds — no reset API, a process-global double-click timeout (the KI-017 objection), or
per-control guards that undo the shield's whole point. `COMMERCIAL_STATUS.md` was the one carrier the
0.9.2 → 0.9.3 sweep missed; only its three current-release statements changed, its historical ones
and its review date stand.

**Windows CI failure: a portability defect, and a workflow guard that hid it (0.9.3).** The Windows
job reported `expected exactly one Anamorph.vst3 bundle, found 0` from its staging step. The cause was
a compile error ~90 lines earlier: `constexpr int algoGap` was a block-scope constant read from a
capture-less lambda, which GCC and Clang accept (reading a constexpr value is not an odr-use) and MSVC
19.51 rejects (`C3493`). Moved with its helper to **file scope**, where no capture question arises; a
sweep of all 22 capture-less lambdas in `src/` confirmed it was the only instance. The second defect
is the one worth remembering: `build.yml` gated the randomise-pluginval and staging steps on
`if: ${{ !cancelled() }}`, which is **true after any upstream failure**, so a compile error let both
run against a tree with no plug-in in it and the job's last error was a cascade. Every platform's build
step now carries `id: build` and every consumer of build output is gated on
`steps.build.outcome == 'success'`; `!cancelled()` is kept alongside it so a *pluginval* failure still
stages a beta artifact. The same bare guard existed on Linux (randomise) and macOS (randomise +
packaging), so all three were fixed; `release.yml` and `msvc.yml` use default `success()` semantics and
were never exposed. Documentation-visible outcome: `build.yml`'s header now states the invariant —
every step consuming build output names the step it depends on.

**CI gating completed on Linux, and release dates reconciled (2026-08-11).** The previous pass gated
every consumer of build output on `steps.build.outcome`, which was right for Windows and macOS and
one step short on Linux: there the strip/objcopy step sits between the build and pluginval *because*
the release gate is meant to validate the stripped bytes, so the producer the randomise gate must name
is `strip`, not `build`. Its deterministic sibling already had that gate for free from default
`success()` semantics; the randomise step, carrying an explicit `if:`, had to say so. `strip` subsumes
`build` (it has no `if:` of its own, so a failed build leaves it `skipped`). `build.yml`'s header
invariant is sharpened accordingly: every step names the step that **produces what it consumes** —
which is `build` on Windows and macOS, and `strip` on Linux. Separately, the 0.9.3 **release** date is
now 2026-08-11 everywhere it appears as a release or change-set date (CHANGELOG, HANDOVER, README,
this file's "Last updated", the worklog header). Dates that record an **event** — INC-011's fix-commit
date, the 2026-08-09 manual-verification and sign-off records — keep the date they happened on, per
`POSTMORTEMS.md`'s "dates are the fix commit dates" rule; conflating the two would rewrite history to
tidy a heading.

**Maintainer sign-off on the remaining 0.9.3 review items (2026-08-11).** Reviewed and accepted with
no code change, on the basis that each is a recorded observation rather than a current correctness or
user-visible problem: the tooltip gate depending on `getTipFor` being the only path that can raise a
tip; combo menus outliving the editor's look-and-feel members (pre-existing, and narrowed by the
destructor cancel); the shield z-order invariant being unenforced; `getChildren()` reordering during
`exitModalState`; the repeated idempotent cancel attempts while the editor stays hidden; preset-menu
double-tracking being benign if its premise ever changed; and `SpectrumImager::mouseExit` reaching the
repaint gate through the eased alphas rather than `frameDirty`; the menu chrome budget being +10 px
against 0.9.2 for every menu whose width is text-derived, and the new 64 px floor for a degenerate
one-glyph item (nothing in `src/` produces one) — both are the intended consequence of deriving the
budget from what the drawing spends, and the one place they meet the narrowed Widen box is a visual
check, not a defect. Verified-correct observations
(pop-up feeder coverage, listener teardown ordering, the foreground probe's self-healing sampling, the
inline-edit cancellation reaching exactly the two commit-on-focus-loss paths, the hover snapshot's
completeness) are recorded as confirmations, not actions. The **one** review item that did change code
in this pass is the Windows CI defect above; the layout and CHANGELOG items are documentation.

**Widen / Style / Focus laid out as equal halves (0.9.3, approved design intent 2026-08-11).** The
row reserved a hard-coded 100 px on the right, so WIDEN and its Style/Focus companion were visibly
unequal (156/94 in Simple, 160/94 in Advanced) and the seam between them sat right of centre. They are
now equal width with the gap centred on the column — one constraint, not two: taking the same slice
off each end leaves a gap whose midpoint is the row's midpoint for odd and even widths alike. The
Style/Focus label takes the identical slice from the identical row width, so it is left-aligned with
its box by construction rather than by a second constant kept in step; the WIDEN label keeps the
remainder and does not move. Both edges move left (−31 px Simple, −33 px Advanced). **No
look-and-feel path is involved** — an earlier round had read the request as being about pop-up list
width and introduced `useLegacyMenuWidth` / `widenCombo`, which is reverted in the same commit.

**Focus release narrowed to the application-switch branch (0.9.3, approved 2026-08-11).**
`dismissOrphanedPopupMenus` was releasing keyboard focus on both of its triggers, but only the
app-switch one needs it — suppressing `PopupMenuCompletionCallback`'s `toFront (true)` requires a
window the user has moved *away* from. On the hidden-editor branch the window being re-fronted is the
one they are still working in, so the release bought nothing and cost two things: a re-shown Save
Preset dialog came back with its name field unfocused (the KI-009 class of symptom, and
`focusSaveNameField` is not re-armed by a re-show) and an in-progress inline edit was discarded. The
cancel itself remains unconditional on both branches; only the focus handling is scoped.

**Maintainer sign-off on the residual pop-up limitations (2026-08-11).** Reviewed and **accepted as
documented limitations rather than defects**, closing them for this release: **KI-019** (Linux/X11
never observes an application switch, so that third dismissal is inert there — the platform's
foreground flag is a write-once latch; inert in the safe direction, and the hidden-editor and
destroyed-editor halves work normally) and **KI-020** (pop-up modality is process-global, so with two
Anamorph editors open the dismissing click can still reach the *other* instance's control —
pre-existing, and 0.9.3 closed only the same-instance half). Neither is to be redesigned for 0.9.3:
no broader pop-up architecture change is required. Also accepted unchanged in the same pass: the
sub-menu arrow sitting outside the named width budget (no menu in `src/` uses `addSubMenu`), the
`minimumWide` floor's comment overstating its guarantee (every combo is far wider than 64 px in both
modes), section headers measured in the item font (over-measurement can only widen), the compact
combo lists inheriting the derived budget, the tooltip gate's 42 ms tick latency, future overlay /
z-order hardening, and the cosmetic stale-hover residue after a menu closes. The sign-off covers the
**decision to document rather than change**; it is not a manual test of the implementation and
touches no release gate.

**A stranded pop-up's focus release could apply a half-typed value (0.9.3).** The app-switch dismissal
releases keyboard focus before cancelling, so JUCE's completion callback cannot re-front the host
window. The first revision asserted that was free — on the strength of a sweep that covered
`PluginEditor.{h,cpp}` and not `src/gui/`. Two inline text edits treat losing focus as *"the user
clicked away"* and **apply** what is in the box: `SpectrumImager`'s crossover-frequency chip
(`freqEditor->onFocusLost` → `commitFreqEditor`, a parameter write inside a change gesture plus a
`projectGaps` nudge to the neighbouring splits) and a slider's value box (`createSliderTextBox` builds
the `Label` with `lossOfFocusDiscardsChanges = false`). Either one turns switching application into a
parameter write the user never asked for, with an automation and undo step to match. `cancelInlineTextEdits()`
now runs first and ends both with the **Escape** outcome instead — `SpectrumImager::cancelInlineEdit()`
clears `editingHandle` so the later asynchronous `onFocusLost` finds nothing to commit, and
`Label::hideEditor (true)` is literally what `Label::textEditorEscapeKeyPressed` ends in. Normal
click-away, Return and Escape are untouched, and `saveNameEditor` is deliberately excluded — it has no
focus-loss handler, so its text stays put, which INC-011 requires.

**Review sign-off on the 0.9.3 pop-up round (2026-08-10).** Two review passes raised nine further
items; the maintainer reviewed each and **accepted the current implementation** on six, which are
therefore closed rather than open: the **pop-up width** growing on every menu (intentional visual
adjustment — kept, though the round after trimmed its discretionary half, see the Menu width entry
below); **unconditional shield
`toFront`** on every raise-path refresh (not required — the "nothing intercepting is brought to front
while the shield is raised" invariant holds today and is documented); the shield **staying frontmost**
after the first pop-up (accepted with the current overlay ordering); **`presetMenusOpen` recovery
machinery** (not required — the counter cannot leak, see *(3)* above); **`SpectrumImager::mouseExit`
setting `frameDirty`** (not required — clearing a hover index always moves an ease target, so the tick
gate already opens); and the shield **swallowing scroll and pinch** for as long as a menu is open
(part of the interaction contract, not only the dismissing event). Four were **actioned**: the
PopupShield hover explanation (corrected — the mechanism is asynchronous fake moves plus modal
blocking plus geometric hover, not raise ordering), the worklog's superseded predicate section (now
banner-marked), the **tooltip delay redundancy** (`tooltipsOn ? 600 : 0x3fffffff` removed; the 600 ms
now lives only at the member's construction), and — reversing an earlier accept — the
**hidden-editor pop-up lifetime**, promoted to a fix once the second pass traced its user-facing cost.
The sign-off covers the **direction and the accepted-as-is decisions**; it is not a manual test of the
implementation and touches no release gate.

**A pop-up could outlive the plug-in window (0.9.3, third Fixed entry).** INC-010 gave the preset menu
a parent so that hiding or destroying the editor cancels it; it could not do the same for a ComboBox
or TextEditor drop-down, which JUCE builds as a free-standing **desktop** window with no ancestor in
common with the editor. The watcher that performs that cancel — `ModalComponentManager::ModalItem`, a
`ComponentMovementWatcher` firing on `! isShowing()` and on the deletion of the component *or a
parent* — registers on the modal component and its ancestors, so for a desktop menu it only ever sees
the menu's own visibility and lifetime. `MenuWindow::windowIsStillValid` is no help either, comparing
two `WeakReference`s to the target control that both survive a hide. **Three** ways in, found across
two review rounds: the host **hides** the view, the host **destroys** the editor (the destructor
removed the component listeners but never asked the window to go away), and — maintainer-confirmed,
and the worst — an **application switch with the pointer resting on a menu item**, where JUCE's own
app-change dismissal does not fire because `MouseSourceState::checkButtonState` gates it on
`! reallyContained`. That last one is not desktop-specific: the parented preset menu has the identical
hole. Stranded, the menu is a floating always-on-top strip over a window that is gone (INC-010's exact
reported symptom, one menu type later), still modal and so still blocking every JUCE component in the
process, still counted in `openMenus` so the returning editor spends its first click dismissing it —
and in the app-switch case, clicking it pulls a background plug-in window back in front. One function
now cancels every pop-up the editor owns, in two passes because no single hook sees both kinds
(`openMenus`, plus any **modal child** — which identifies the parented preset menu exactly, since
nothing else the editor owns ever enters a modal state), called unconditionally from the destructor
and conditionally from the 24 Hz tick on `! isShowing()` or a genuine application switch. The tick is
the only observer available: an ancestor's `setVisible`, a peer change and a minimise all end at
`isShowing()` without notifying us, and an app switch has no `Component` event at all. The app-switch
half is **self-calibrating**, after a first attempt got it wrong. `Process::isForegroundProcess()` is
only half of JUCE's own test; the other half — which covers a plug-in whose editor lives in a window
owned by a different process — is module-internal, and the first revision argued it was safe to skip
because Anamorph ships VST3 / AU / Standalone rather than AUv3. That conflated the *format* with the
*hosting mode*: whether a plug-in runs inside the host's process is the **host's** choice (Bitwig
gives every plug-in a helper process by default; bridged and sandboxed hosting does the same), so a
plain VST3 hits it, the call reads `false` permanently while the editor is in active use, and every
menu was cancelled within one tick of opening — the controls were unusable with the mouse. The editor
now records what that call reads at the moment a pop-up **opens**, which only a click on one of its
own controls can produce, and treats a later `false` as an app switch only if it read `true` then;
where it never reads `true`, JUCE's own dismissal remains the only cover, which is exactly the
pre-0.9.3 position rather than a regression. Deliberately **not**
`PopupMenu::dismissAllActiveMenus()`, which is process-global and would close another instance's menu
— the objection that already ruled it out in INC-010. Nothing changes while the plug-in is in front of
the user, so the dismissal contract, the shield's z-order and the one-click behaviour are untouched.

**Menu width: the discretionary part removed (0.9.3).** The width fix summed the chrome
`drawPopupMenuItem` actually spends (12 + 14 + 12 = 38) and then added 12 px of "breathing room" on
top, for 50 against the previous flat 30. That allowance widens **every** menu drawn through the
look-and-feel — including the combo drop-downs — wherever the item text rather than
`withMinimumWidth (box.getWidth())` is the binding constraint, so the discretionary part was silently
changing the relationship between a control and its own list, and the Widen/Style/Focus layout
contract outranks pop-up padding. The margin is now 2 px and is no longer discretionary: it is a
rounding guard, because `drawPopupMenuItem` uses `Graphics::drawText`'s three-argument overload whose
`useEllipsesIfTooBig` defaults to true, so text measuring one sub-pixel over the strip would ellipsise
rather than overhang. Total 40 — still ≥ the 38 actually spent, so the *"Select All"* clipping fix is
intact and now exact. A later round briefly had the **Widen / Style / Focus** combos opt out of the
budget entirely (`useLegacyMenuWidth` plus a `widenCombo` instance); that answered the request in the
wrong dimension and was **reverted** — see the layout entry below. Every menu shares the one budget. This paragraph once ended by asserting that **no layout code changed** in
this cycle. That was true while the menu-width work was the only `resized()` edit on the branch, and
became **false** with the equal-halves commit, which rewrote `layoutAlgoRow` and both `algoOptLabel`
rows — so the claim is withdrawn rather than left contradicting the Widen/Style/Focus layout entry
above, which is the accurate record. What survives it is the narrower point it was making: the
menu-width work *itself* touched no layout code, and no `LookAndFeel` combo/label sizing or drawing
method has been changed at any point in this cycle.

**Pop-up dismissal became one mechanism instead of one predicate (0.9.3).** Verification of the
Settings fix found the same defect on the Save Preset dialog, where it *destroys typed input*: a
right-click opens `TextEditor`'s context menu, and the click that dismisses it was re-delivered to
the backdrop, which closed the dialog. The Settings predicate could not be extended — `TextEditor`'s
menu state is private, and JUCE exposes no universal "this click just dismissed a pop-up" signal
(all three candidates were read in the pinned tree and all three fail; the table is in the worklog).
So the editor now owns the state: `AnamorphLookAndFeel::preparePopupMenuWindow` catches every menu
built through our look-and-feel (ComboBox and TextEditor both set it), the preset menu is counted
directly because its own look-and-feel is null at that moment, and a single transparent
**`PopupShield`** takes the click. The Settings-only predicate was **removed**, not kept alongside —
the contract is "the dismissing click touches nothing underneath", and *underneath* includes controls
that act on the press (`ABControl` toggles A/B, `SpectrumImager` can add a band), so one shield is
both smaller and more complete than a predicate per control. The riskiest property is proved from the
source rather than left to a GUI test: the shield cannot be raised in front of a menu, because
`MenuWindow` sets `alwaysOnTop` and `Component::toFront` inserts a non-always-on-top component behind
every always-on-top sibling. **Two menu-rendering fixes rode along**, both in the shared
look-and-feel rather than patched per menu: `getIdealPopupMenuItemSize` allowed 30 px of chrome
against a layout that spends 38, so the longest item was measured narrower than it draws and JUCE
clipped *"Select All"* to *"Select ..."* — the allowance is now summed from named constants the
drawing code uses, so the two cannot drift; and `drawPopupMenuItem` was **ignoring its `isActive`
argument**, so disabled entries rendered identically to live ones — now dimmed at the 0.4 alpha this
file already uses for a disabled button. All of it is editor-only and joins the existing ADR-0025
entry in `TESTING.md` §Gaps. Maintainer sign-off (2026-08-09) covers the **problem reports and the
required contract**; it is not a manual test of the implementation and touches no release gate.

**The two interaction bugs this cycle opened with, both with non-obvious mechanisms (0.9.3).** *(1)* The Multiband **add-split
preview line** stalled under a moving pointer. The S2 repaint gate skips a frame when nothing it
watches moved — spectrum data, eased alphas, drawn split/width positions — on the stated assumption
that "mouse-driven fields [have] handlers [that] already repaint explicitly". `updateHover()` was the
handler that did not, and `addX`, the preview line's X, is none of the three things the gate watches.
So with the pointer moving *within one band's add zone* (hoverAdd unchanged, `addA` already at 1.0)
over a *settled* spectrum, nothing moved and the line froze; crossing into another hotspot moved an
alpha and it jumped to the cursor. `updateHover` now marks the frame dirty when its output changed —
`frameDirty` rather than `repaint()`, so painting stays paced at one frame per vblank, which is what
the gate is for. The gate itself is untouched: an idle view still stops repainting. *(2)* A **Settings
drop-down's dismissing click also closed Settings**, because JUCE *deliberately re-delivers* it:
`internalMouseDown` dismisses the modal menu via `internalModalInputAttempt()` and then, seeing the
modal loop has exited, passes the same mouse-down to the component underneath
(`juce_Component.cpp:2507-2544`). The **shipped** answer is the editor-level `PopupShield` described
in the entry above — a single always-visible, normally-inert overlay that starts intercepting while
any pop-up is on screen, so the dismissing click reaches no control at all. (The first attempt at
this fix was a Settings-only predicate on `Backdrop` reading `ComboBox::isPopupActive()`; it was
**removed** within the same PR once the same defect turned up on the Save Preset dialog, where
`TextEditor`'s menu state is private and the predicate could not reach it. Nothing named
`swallowsDismissClick` or `isPopupActive` survives in the editor — the shield is the mechanism.)
Reasoning, edge cases and the JUCE-signal analysis: `worklogs/GUI_INTERACTION_FIXES_v0.9.3.md`.
**Neither fix has an automated test** — both are
editor-interaction defects and the harness instantiates no editor and drives no pointer; registered
as a second **ADR-0025** exception with its four disclosures in `TESTING.md` §Gaps, beside INC-010.
The Save Preset case is also filed as **INC-011** — it destroys typed user input, which clears the
same bar INC-008 set for a pure GUI-interaction regression, and `DOCUMENTATION_LIFECYCLE_POLICY`'s
trigger map ("Fix a notable incident → `POSTMORTEMS.md`") therefore applies. Its most transferable
finding is not the JUCE mechanism but the process one: the Settings drop-down was fixed first with a
`ComboBox::isPopupActive()` predicate, a design *incapable* of expressing the `TextEditor` case, so
no amount of testing that fix could have reached its sibling — a fix scoped to what exposed a defect
rather than to its cause cannot find the rest of the class.
**Version carriers swept** for the 0.9.2 → 0.9.3 bump: `CMakeLists.txt`, `CHANGELOG.md`, `README.md`,
`HANDOVER.md`, `KNOWN_ISSUES.md`, `FUTURE_RISKS.md`, `RELEASE_PROCESS.md`, `RELEASE_HARDENING_PLAN.md`,
`CHANGELOG_POLICY.md`, ADR-0024 — every place naming the *release in preparation* or the *first
annotated tag*, which is now **v0.9.3** (0.9.0, 0.9.1 and 0.9.2 were each written up and superseded
before a tag was cut). Historical references to what 0.9.2 introduced are left as they are.

---

Previously: for the **0.9.2 change set** (2026-08-07) — the first `src/` change since 0.9.0.
Four changes, one investigation, three new regression tests, and one governance amendment.

**Governance: `TESTING_POLICY` rule 1 gains a narrow exception (ADR-0025).** The rule ("every bug fix
ships a regression test") was stated unconditionally, while the project has in practice shipped one
fix — INC-010 — without one, because no automated surface reaches a defect that only exists while a
modal child is open and its owner is destroyed. That deviation had been recorded in a Procedure and
in this ledger, both of which rank **below** Policy, so nothing at or above Policy level described
what the project actually does. **ADR-0025** closes that: the default is unchanged, the release gate
is untouched, and the exception is available **only** where the repository has no stable automated
surface reaching the defect (GUI/component lifetime, host-owned UI behaviour, OS-level asynchrony) —
never for a test that is merely hard to write. Invoking it requires four disclosures (why no test
exists, what replaced it, where the gap is tracked, whether infrastructure could close it), and the
exception lapses when the surface appears. `docs/procedures/TESTING.md` §"Gaps in the automated
coverage" is named as the register — the role it already played for the AU-conformance and
golden-audio gaps that `KNOWN_ISSUES.md` KI-014 and `RELEASE_HARDENING_PLAN.md` RH-F3 cite. Per
`ADR_POLICY` rule 5 / `SOURCE_OF_TRUTH`, the ADR is the instrument that makes the Policy change; per
rule 1 it is registered in `ADR_INDEX.md`. **A one-off waiver was explicitly rejected** — the goal
was a rule that describes the engineering reality, not an escape hatch for one entry.

**Preset drop-down lifetime + crash (`src/PluginEditor.cpp`).** Filed as **INC-010**. Three facts,
separated after an adversarial re-read of the pinned JUCE source — the first draft of this entry
(and of the code comment) got the mechanism wrong and is corrected here rather than left standing.
(1) The **leftover menu** is not an oversight in JUCE: `MenuWindow::windowIsStillValid()` dismisses when
`componentAttachedTo != options.getTargetComponent()`, but both are `WeakReference` to
`presetName`, so they null *together* and the comparison is false. (2) The **lost styling is not a
use-after-free** — the MenuWindow copies the look-and-feel into its own `Component::lookAndFeel`
slot (`juce_PopupMenu.cpp:366`), a `WeakReference` that nulls and falls back to `LookAndFeel_V4`
(the `PopupMenu` itself is a stack local, gone long before the editor). (3) The **crash** is the raw `this` in the callback.
The fix is `withParentComponent (this)` (JUCE parents the MenuWindow as a CHILD, cancelled with
result 0 by `ModalComponentManager` on destruction *or* hide; `Component::getLookAndFeel()` then
resolves our LookAndFeel by walking the tree) plus a `SafePointer` callback — which is **not**
redundant, since that cancel is asynchronous and the menu's 20 Hz timer can still emit a non-zero
result in the gap. Two side effects of parenting were neutralised in the same change:
`withMaximumNumColumns (1)` (a parented menu is budgeted against the editor, and JUCE adds COLUMNS
before it scrolls — past ~14 user presets the list would have silently gone two-column) and a
no-op `drawResizableFrame` (JUCE paints a frame over the border ring only when parented). The
"Load Preset…" file chooser, reachable from the same menu, got the same `SafePointer` guard.
No regression test: the failure is a GUI-lifetime use-after-free, which `tests/state_tests.cpp`
cannot express — the harness links the editor but never instantiates it. This is **not** a one-off
waiver: **ADR-0025** amends `TESTING_POLICY` rule 1 with a narrow, disclosure-bound exception for
defects that no automated surface reaches, the default stays "every bug fix ships a regression test",
the release gate is untouched, and INC-010 is the first invocation. Its four required disclosures —
why no test exists, what replaced it (removal of the lifetime by construction, plus a `SafePointer`
for the residual asynchronous window), where the gap is tracked, and what infrastructure would close
it — are recorded in `TESTING.md` §"Gaps in the automated coverage", which that ADR names as the
register, and summarised in INC-010's Prevention field. Synced:
`CHANGELOG.md`, `README.md`, `HANDOVER.md`, `POSTMORTEMS.md` (INC-010).
**Reported, not fixed (C6):** the combo-box popups store an editor-member LookAndFeel the same way
and would lose styling identically, but their callback is `ModalCallbackFunction::forComponent`,
i.e. already SafePointer-based — no memory-safety defect, no reported symptom, seven call sites
across two LookAndFeel subclasses. Out of scope. Likewise `SpectrumImager`'s `freqEditor`
`onFocusLost` can fire during teardown; it is owned by the editor it belongs to, so it is a
different (and lesser) class of hazard.

**Factory-preset identity (`src/PresetManager.{h,cpp}`, `src/PluginProcessor.{h,cpp}`).** The
preset list was searched by NAME and the factory block is list-front, so a user preset sharing a
factory preset's name could never hold the drop-down tick. A factory preset now carries an
immutable internal `factoryId` and a user preset is identified by its file
(`PresetManager::Selection`); the menu, the top bar and the Save Preset field still show the
**name**. The identity rides on `StateSet` through A/B and undo, and — after the maintainer
supplied the Architecture-Review approval the gate requires — **also with the session**, so
reopening a project ticks the row that produced the sound. Six additive metadata fields (3 in
`AnamorphRoot`, 3 per A/B slot); **user preset FILES are unchanged**, parameter restore is
independent of identity restore, and anything unresolvable ticks nothing rather than a same-named
substitute. Recorded as **ADR-0024** (registered in `ADR_INDEX.md`), whose original "never
serialized" clause is reversed by a dated **Amendment** that keeps the original text verbatim above
it — the reversal, its approval and its fallback table are exactly what a future agent would
otherwise re-litigate straight into a Hard Stop. Synced: `SERIALIZATION_REGISTRY.md` (six new field
rows), `SESSION_COMPATIBILITY_POLICY.md` (rule 4's round-trip list), `API_REFERENCE.md`,
`USER_MANUAL.md` §7.2, `TESTING.md`, `TESTING_POLICY.md`, `RELEASE_HARDENING_PLAN.md`,
`REPOSITORY_MAP.md`, `HANDOVER.md`, `CHANGELOG.md`, and **`PRIVACY.md`** — that document states
every claim about what reaches disk, and the session can now carry a preset **file name**, or an
absolute **path** in the one case where the selected preset was opened from outside the preset
folder. The path case is the same class as the Standalone's `lastStateFile` entry the document
already carves out, and it is now carved out alongside it, with the reason the in-folder case stores
a name instead. `ADR-0008` gained the third `StateSet` field and
re-based line anchors (a factual re-sync, not a reversal; ADRs stay append-only). State tests 10,
11 and 12 pin the live behaviour, the id integrity and the whole restore matrix including every
fallback.

Six defects found by review and fixed before merge, each with its own assertion, and every one
verified to fail with its fix disabled. Three from the first adversarial pass: the identity scan
**fell through** to the name scan when the identity was known but absent from the list, so a `.anamorph`
loaded from outside the preset folder ticked the same-named factory row — the exact mis-tick this
change exists to remove; `saveUser` never re-baselined the processor's undo snapshot, so the first
undo after a save restored the pre-save name/identity (fixed with an `onSaved` hook →
`syncCommitted()`, which creates no undo step because a save is not a sound change — and the same
gap existed for a preset switch whose sound is identical to the current one); and `readSlot` left
a stale identity on an A/B slot when a host restored a second session into one live instance.

Three more from the maintainer's follow-up review. **`saveUser` did not flush pending undo
coalescing** before re-baselining: `syncCommitted()` clears `pendingGestureCommit`, so a knob
gesture that had closed but not yet been polled was folded into the new baseline with no undo step
— the edit silently stopped being undoable. `onSaved` now does `pollUndoCoalesce(); syncCommitted();`,
matching the two other program-state jumps (`onAboutToLoad`, and `undo()`/`redo()`). **A factory id
that fails to resolve** applied the plain defaults and then adopted the factory identity anyway;
`load()` now resolves it BEFORE the undo bracket opens — the same rule the user-preset parse three
lines above already followed — asserts it, and fails as a clean no-op otherwise. State test 11 pins
the invariant that makes the assert unreachable: ids present, unique, and every one resolving.

**Two more from a third, independent review of the finished change set**, both introduced by the
plug-in-state work and both now fixed with a discriminating assertion. `encodeSelection` used
`juce::File::isAChildOf` to decide whether a preset lives in the preset folder — but JUCE implements
that **recursively**, so a preset opened from a **sub-folder** was stored by bare name and decoded to
a *different*, same-named file directly in the folder, breaking the `decode(encode(s)) == s`
invariant the header and the ADR both state. Now a **direct-child** test, which is also the honest
one: `refresh()` scans non-recursively, so only a direct child can ever be a menu row. And the new
`else` branch of `commitPresetSwitchUndoStep` did not clear redo, unlike the `if` branch one line
above whose comment states the rule — so undo, then select the same-sounding preset on the other row,
then Redo, and the tick jumped back out of an abandoned `StateSet`. (That branch then had to be
narrowed again — see below.)

**Raised and REFUTED, recorded so it is not re-raised:** a `saveUser` defect for preset names with a
leading `~`. The JUCE facts are real as far as they go — `getChildFile` short-circuits for anything
`isAbsolutePath` accepts, and on macOS/Linux a leading `~` survives `createLegalFileName` — but the
write cannot succeed. `replaceWithText` does not open the target: it writes a hidden sibling built
from `getParentDirectory()`, and for a separator-less path that is the path itself, which is not a
directory. `createLegalFileName` strips `/` and `\`, so every tilde-leading name hits the same
degenerate parent. `saveUser` therefore returns **false**, nothing is written anywhere, and the Save
dialog stays open with the text intact — the save fails *visibly*, which is exactly what the proposed
guard was meant to produce. Verified empirically against the pinned `juce_core` for `~foo`, `~/foo`,
`~` and `~root`, with a normal name as the control. **No code change; no defect.** The refutation and
its probe are in `worklogs/PRESET_MENU_AND_IDENTITY_v0.9.2.md` §7.

**Redo invalidation, narrowed after review.** The `else` branch above cleared redo unconditionally,
so *re-picking the row that is already ticked* — identical sound, identical identity — silently threw
away a redo the user was about to press. It now clears redo only when the identity actually **moved**
(`presets.selection() != committed.selection`), which is the only case where a surviving redo entry
could drag the tick off the row just chosen. The same-sound/**different**-row case still invalidates,
and its assertion is unchanged; a second assertion covers the re-select case, and both were verified
against the pre-fix behaviour.

**The encoder's second ambiguity: a direct-child name that `isAbsolutePath` accepts.** The
`isAChildOf` fix above closed the *nesting* route into a broken `decode(encode(s)) == s`; a leading
`~` was the other one. `decodeSelection` reads a bare name back through
`presetDirectory().getChildFile(name)`, and `getChildFile` short-circuits to the raw `File`
constructor for anything `isAbsolutePath` accepts, so `~foo.anamorph` sitting **directly in** the
preset folder decoded to a literal relative path and the row lost its tick on reload. This does not
contradict the `saveUser` refutation recorded above — that refutation is about the **save** path,
which genuinely cannot create such a file; `USER_MANUAL.md` tells users to manage presets as files, so
a hand-copied one reaches `refresh()` and can be loaded and encoded like any other. The encoder now
requires the bare name to be unambiguous (`! juce::File::isAbsolutePath (name)`) and otherwise takes
the absolute-path branch it already shares with outside-the-folder and sub-folder presets. No preset
file format change, no canonicalisation, no weakening of the no-name-fallback rule. State test 12
gained the round-trip case; verified to fail with the fix disabled.

**A/B slot metadata now follows "absence means default".** `readSlot` read `dst.name` and
`dst.baseline` *inside* the `hasProperty("slotAParams")` branch, so the pre-0.6.4 legacy shape — params
only — left both untouched. `abSlot[]` are processor members and a host may call
`setStateInformation` on one live instance repeatedly, so a legacy session restored after a modern one
kept the **previous** session's preset name and dirty-baseline attached to freshly restored
parameters. Both reads moved out of the branch, next to the identity read that already had this right.
The resulting defaults (`""` / `""`) are the ones `SERIALIZATION_REGISTRY.md` already documented, so
the code caught up to the ledger; no field was added, removed or renamed. **An existing assertion was
changed, not merely added:** state test 5's `slotAName == "Default"` under the comment "legacy slot
keeps pre-restore meta" *pinned the defect* — it described a fresh instance's construction snapshot as
if it were the rule. It now asserts the default, alongside a repeated-restore case that shows why.

**"No baseline recorded" is not "modified" (fifth review round).** The A/B fix above left a second
half unfinished: a pre-0.6.4 slot restores with an empty *baseline* as well as an empty name, and
`isDirty()` is `soundSig() != sigAtLoad`. `soundSig()` is never empty, so an empty baseline compares
unequal to every possible sound and the slot read as **permanently modified** — with no name, the top
bar rendered a bare ` *`: a modified-marker against a preset that does not exist. The project already
has a rule for "restored parameters, no recorded baseline": `adoptRestoredState` sets the restored
state as the clean one, which `SERIALIZATION_REGISTRY.md` documents for the root `presetBaseline` and
state test 4 pins for a v0.2 session. `setMeta` now applies that same rule, so it is one rule with one
spelling instead of two answers to the same question. Unreachable from undo, redo, A/B and copy —
every in-memory producer fills the baseline — so the branch is legacy-restore only. The *empty name*
was left as-is deliberately: the slot genuinely has no preset, and the pre-fix "Default" was a
factual error (the slot's parameters were not the defaults). Maintainer confirmation of the direction
is recorded per the review sign-off; no serialization field changed and `""` keeps its meaning
("absent"), so this is a read-path interpretation, not an `ARCHITECTURE_REVIEW_GATE` item.

**A slot must reset as a whole, or its two halves come from two projects (sixth review round).** The
"absence means default" rule was applied field by field — `dst.selection`, `dst.name` and
`dst.baseline` — but `dst.params` was still only touched inside the two params-present branches. An
`AB` node that exists while a slot's payload cannot be read (neither `slotAParams` nor the pre-0.6.4
`slotA`, or a payload that fails to parse) therefore kept the **previous restore's sound** while its
metadata was reset around it: one slot holding one project's sound under another project's label.
Before this PR both halves were inherited together — consistently stale, which is wrong but not
*mixed* — so this was a defect the earlier rounds introduced, not a pre-existing one. `readSlot` now
resets the slot to a default `StateSet` first and overlays what the node carries. The params default
is not an empty tree but **"lazily initialised from current"**, which the registry already recorded
and which `abEnsureInit()` already implements off `StateSet::isValid()` — so no new mechanism, no new
field, and the slot comes back seeded from the state just restored. The reset also covers the
present-but-unparsable payload for free. Both cases are pinned by state test 9 and were verified to
fail with the reset removed. **No `CHANGELOG.md` entry:** no shipped version writes an `AB` node
lacking both params keys, so there is no user-visible change to report under `CHANGELOG_POLICY` rule
3 — this is corrupt/truncated-state robustness, the category state test 7 covers. Maintainer
confirmation of the direction is recorded per the review sign-off.

**The root preset NAME had the same leak as the slots (seventh review round).** `readSlot`'s rule —
metadata never inherits across a repeated restore — was not applied to `AnamorphRoot`. Both adoption
paths fell back to the live `presets.currentName()`: the `haveBaseline` branch via
`restoredName.isNotEmpty() ? restoredName : presets.currentName()`, and `adoptRestoredState` via
`if (name.isNotEmpty()) current = name;`. `presets` is a processor member, so on a host's second
`setStateInformation` into one instance that is the **previous project's** label — new sound, new
identity, old name, and with no stored identity the name scan could then tick the old project's row.
This became reachable *because* of this PR: an empty preset name is now a real state (a session saved
while sitting on a nameless A/B slot stores `presetName=""`).

**Absent and empty are different answers**, and only `setStateInformation` can tell them apart — the
distinction `haveBaseline` already drew for the sibling field. Absent means a session predating the
field (< 0.6) and resolves to the new `PresetManager::defaultName()`, a **constant**, whose
name-fallback tick is the documented ADR-0024 answer for identity-less state; present-but-empty is
adopted verbatim. `adoptRestoredState` now assigns the name unconditionally, so "what the session
carried" and "what absence means" stop being decided in two places. No serialization field changed,
and no existing assertion moved — state test 4's `preset name falls back to Default` still passes,
because a v0.2 blob has no `presetName` property. Four cases (empty/absent × baseline/no-baseline)
are pinned in state test 12; all eight new assertions were verified to fail with the fix reverted.
Maintainer confirmation of the direction is recorded per the review sign-off.

**An unrecognised chunk is not a restore (eighth review round).** `setStateInformation` handles two
root shapes; anything else matched neither and *fell through* to the adoption block, which clears the
undo history and writes the restored preset name, identity and baseline. With nothing restored, that
relabelled the live sound — after the previous round, with the constant `"Default"` — dropped the
identity to `unknown` so the name scan ticked whatever shared the label, and re-baselined the
dirty-star. The name half was introduced by the previous round; the identity and baseline halves were
**pre-existing**, since `adoptRestoredState` always assigned those two unconditionally. The fix is an
`else { return; }`, which is the same answer the `getXmlFromBinary` guard at the top of the function
already gives an unparsable blob — the identical situation one layer down. It also stops the undo
history being cleared for a session that never loaded; disclosed rather than slipped in, since that
half was not named in the finding.

**`abEnsureInit` now seeds both slots the same way.** It seeded an invalid slot A from
`currentStateSet()` but an invalid slot B from a **copy of slot A**. On the path that runs every time
— construction, both slots invalid — the two are indistinguishable, so this changes nothing there.
They diverged only when slot A was valid and slot B was not, i.e. an `AB` node whose `slotBParams`
alone was missing or unparsable: slot B came back as a duplicate of slot A rather than as the state
just restored, and a later save wrote that duplicate out. The registry and `STATE_SERIALIZATION.md`
had already been written as though the rule were symmetric, so this is the code catching up to the
documented invariant rather than a new one. `currentStateSet()` builds a fresh tree per call, so the
explicit `createCopy()` for slot independence is no longer needed.

**The empty preset label gets a placeholder (ninth review round).** The blank top-bar button a
pre-0.6.4 A/B slot produced — flagged as a maintainer decision under constraint C8 — is now
**No Preset**, with sign-off dated 2026-08-08. It is a *display* substitution in
`refreshPresetDisplay`, deliberately **not** in `PresetManager::currentName()`: that accessor also
feeds the serialized `presetName` and the Save Preset pre-fill, so a placeholder there would be
written into every session saved from a nameless slot and offered as the default preset *file* name.
The stored name stays `""`, the identity stays `unknown`, and `currentIndex()` still ticks nothing.
State test 5 gained the assertion that closes the loop — a re-save must still write `presetName=""`
— and moving the substitution into the accessor as a control fails four assertions. `ADR-0024`'s
"no user-visible string was added" consequence was **false** once this landed and is corrected in
place rather than left to drift; the `CHANGELOG` entry now names the label. `TESTING.md`'s
restore-path sentence was one behind in both numbers (seven/six → eight/seven) and had missed the
tilde case in its fallback list; tests are the source of truth, so the prose moved. The `setMeta`
ordering invariant, already in the header, is now also stated at `applyStateSet` — the two lines that
*are* the order, and the place a future edit would break it.

**Declined in the same round, recorded so it is not re-raised: an `AnamorphRoot` with no `ANAMORPH`
child.** Such a chunk is *recognised*, so it restores the fields it carries and resolves the absent
ones to their documented defaults — while `params.isValid()` is false, so the parameters keep their
current values and the live sound ends up labelled `defaultName()`. It reproduces, and it is
deliberately left alone: the rule this round implements is about *unrecognised* input; field-by-field
handling of a recognised root is the existing design and state test 7's `restoreWithActive` depends
on it (an `AnamorphRoot` carrying only an `AB` child must still apply the clamped `active`); the
obvious alternative — skip adoption when there are no params — re-introduces the cross-restore
leakage the previous round removed; and `getStateInformation` always writes an `ANAMORPH` child, so no
shipped version can produce one. Reasoning in full in `worklogs/…v0.9.2.md` §13.

**`setMeta`'s identity-less overload removed.** The two-argument overload forwarded a
default-constructed `Selection`, so "forget which row produced this sound" — the mis-tick ADR-0024
exists to remove — was something a caller could do without writing it down. Its only caller was a
test, which now passes `Selection()` explicitly. The one-argument `adoptRestoredState` overload was
dead code with the identical shape and went with it. No behaviour change. The header also now records
the precondition `setMeta`'s empty-baseline fallback depends on and the signature cannot enforce: the
parameters the metadata describes must already be applied, because `soundSig()` reads the live APVTS.

**Re-raised and re-refuted: the `~foo` `saveUser` claim.** A later review reported this ledger as
still asserting that `saveUser` "writes outside the folder and still returns success". It does not,
and has not since the round recorded in `worklogs/…v0.9.2.md` §8 — the sentence was removed there and
the entry above has stated the refutation ever since (introduced `9b67b8d`, corrected `55e062d`). The
repository holds no conflicting description: `DOCUMENTATION_COVERAGE.md` and worklog §7 both say the
write fails and `saveUser` returns **false**, and §9 records that the *encode*-side sibling — a
`~`-named file a user copies in by hand — was a separate, real defect. Because the claim keeps coming
back, the refutation now also lives in the **code**, at the `getChildFile` call it is raised against;
per `SOURCE_OF_TRUTH` that outranks every document and is the first thing a reader of
`saveUser` sees.

**Documentation follow-up on the identity match (no behaviour change).** ADR-0024's Consequences now
state the three properties plainly: the match is a raw path-string compare with **no**
canonicalisation (`getLinkedTarget()` considered and rejected — it resolves symlinks but not
`/private/var`, mount aliases or UNC spellings, trading a predictable "no tick" for a partial one);
cross-machine resolution holds only for the name-encoded case, because a stored absolute path fails
`isAbsolutePath` on the other platform; and a file name that looks like a path is stored as a path.
`SERIALIZATION_REGISTRY.md` gained both encoder conditions and the raw-compare note.
`API_REFERENCE.md`, `STATE_SERIALIZATION.md` and the ADR had their `src/` citations re-anchored where
this round's edits moved them.

**Declined, with evidence: restoring `PopupMenu::setLookAndFeel (&lnf)`.** The stated goal was to
make item *measurement* use `AnamorphLookAndFeel` — but it already does. `MenuWindow` parents itself
at `juce_PopupMenu.cpp:370-372` and only then builds items (`:457`), and `ItemComponent` calls
`parent.addAndMakeVisible` *before* `getIdealSize` (`:139-146`), which resolves through
`getLookAndFeel()`. Restoring it would instead re-arm the `~LookAndFeel` assertion, which fires on
any live `WeakReference`: `lnf` is a member and so is destroyed *before* this editor's `Component`
base, i.e. before the menu is asynchronously cancelled. The only two calls that still see the
default look-and-feel are bound one line before the parenting — `setOpaque` (same answer,
`colours::bgPanel` is opaque) and `preparePopupMenuWindow` (a no-op we do not override). A **third**
resolves through it earlier still and is **load-bearing**: `getParentComponentForMenuOptions`
(`juce_PopupMenu.cpp:353`, in the member-init list), whose return value is what installs the parent —
so a process-global default look-and-feel overriding it to return `nullptr` would silently discard
the parenting. Every JUCE look-and-feel inherits `LookAndFeel_V2`'s pass-through, and the default is
not ours to control; recorded in the code as the latent trap it is.

**`focusSaveNameField`'s comment was stale, not its behaviour.** It justified the retry by the
preset menu's own desktop window owning OS focus — which parenting removed. The retry stays, because
the abort it works around is not menu-specific: `Component::takeKeyboardFocus` gives up while the
plug-in's own peer is not OS-focused, and whether it is, at that instant, is the host's call (the
failure KI-009 tracks in REAPER). Comment rewritten; the bounded 4 × 50 ms retry is untouched.

**`Window Size` → `UI Scale` (display name only).** `PARAMETER_COMPATIBILITY_POLICY` permits a
display-name change; the identifier `int_uiScale` and the pre-0.8.4 legacy APVTS id `uiScale` its
migration reads are untouched, so this is not a serialization change and needs no ADR. Recorded
with the repo's own footnote form in `PARAMETER_REGISTRY.md` (`※`, mirroring the `Haas Side` →
`Haas Focus` precedent). Synced: `PARAMETER_REFERENCE.md`, `REPOSITORY_MAP.md`, `USER_MANUAL.md`
(×3), `README.md`, the six source comments naming the control, and a **clarifying annotation** in
ADR-0010 — the ADR body is otherwise left verbatim, since ADRs are append-only.

**Installer component titles.** macOS `<choice title=…>` → *VST3 Plug-in* / *AU Plug-in* /
*Standalone Application*; the two Windows destination-page **labels** → *VST3 Plug-in folder* /
*Standalone Application folder*. Prose sentences keep lowercase "plug-in"/"application" (the
`MsgBox` strings, the `:90` parenthetical, every legal/manual use). The Windows `[Components]`
descriptions ("Install VST3" / "Install Standalone") contain neither phrase and are unchanged, so
the five doc quotes of them stay valid. No CI or self-check assertion matches a title — the macOS
self-check matches `<choice id=…>` and the package identifiers. Synced: `PACKAGING.md`,
`INSTALLATION.md` (the macOS Component table, which had drifted twice over: *AU (Audio Unit)* and
*Standalone app* never matched the installer even before this change — corrected here in the same
pass, and reported rather than silently changed).

**macOS key auto-repeat: investigated, no code change (KI-017).** Holding a letter or digit in a
text field types once and stops while punctuation repeats. Traced through the pinned JUCE: a
focused `TextEditor` makes `findCurrentTextInputTarget()` non-null, so every key-down goes to
`[inputContext handleEvent:]` first and printable characters return via `insertText:` — the path
macOS press-and-hold and the IME own — while "special" keys return via `doCommandBySelector:` and
repeat normally. Everything inside the plug-in was eliminated by inspection (the bounded focus
retry, `setSelectAllWhenFocused`, the 24 Hz timer, the VBlank attachment, the UI-scale transform,
both `getCurrentModifiersRealtime` call sites). Filed as **KI-017** with the two discriminating
checks; no `CHANGELOG` entry, since nothing user-visible changed (`CHANGELOG_POLICY` rule 3).

**First-tag renumbering, swept this time.** The 0.9.0 → 0.9.1 renumbering was recorded here as
incomplete; the 0.9.1 → 0.9.2 one repeated it and is now closed in the same pass:
`CHANGELOG.md` preamble, `CHANGELOG_POLICY.md`, `FUTURE_RISKS.md` (×2), `COMMERCIAL_STATUS.md`
(×3) and `RELEASE_HARDENING_PLAN.md` (×5) all named v0.9.1 as the first annotated tag. Neither
`FUTURE_RISKS.md` nor `COMMERCIAL_STATUS.md` had been touched by the version bump at all.

Prior: for the **third review pass on the 0.9.1 change set** (2026-07-30). Three findings
fixed, three were confirmations. No `src/` change.

**The `Unreleased` guard had a residual hole.** It rejected only a heading containing the word
`Unreleased`; a heading written as a bare `## [0.9.1]` is equally undated and would have published
undated notes. The check now requires an **ISO date** in the heading, which subsumes both cases and
matches the format every existing entry already uses. Exercised against five heading forms
(`— Unreleased`, bare, em-dash-dated, hyphen-dated, two-digit minor) plus both real CHANGELOG
sections. Synced: `release.yml` (tag branch + rehearsal warning), `RELEASE_PROCESS`, `CI_CD`,
`HANDOVER`.

**`FUTURE_RISKS` was edited for 0.9.1 but kept a v0.9.0 version-sync lead**, leaving it and
`KNOWN_ISSUES` disagreeing about which version the status documents are synced to. Re-led to
v0.9.1, recording that ADR-0023 adds no new *risk* — the one-time session break is a known issue
(KI-016), not a forward-looking one.

**More reported-then-corrected line drift (C6), in a document this change set touched.** `BUILD.md`
carried three stale `CMakeLists.txt` citations the previous pass did not report:
`ANAMORPH_BUILD_TESTS` `:27,212` → `:27,219`, `ANAMORPH_BUILD_NUMBER` `:183` → `:188`, compile
definitions `:185-194` → `:190-199`. Sweeping for the same class found a fourth, shared by
`RELEASE_PROCESS` and `RELEASE_POLICY`: the versioning citation `:181-187` → `:186-192`. All five
re-verified against the file. (`build.yml:60,180,438` for the build-number Configure steps was
checked and is correct.)

**Confirmed, no change:** the auval recipe is consistent in every maintained carrier; the guard
interacts correctly with the verbatim notes extraction; the `curl`/`unzip` fix matches what
`run-pluginval.sh` invokes and its re-based citations all resolve.

Prior: for the **second review pass on the 0.9.1 change set** (2026-07-30). Six findings
fixed, no `src/` change.

**The exception was over-claimed.** ADR-0023's status line said the `ARCHITECTURE_REVIEW_GATE` item
and the `COMPATIBILITY_POLICY` exception were "both now cleared". The exception needs all four
conditions and **condition 3 — the Release Compatibility Checklist — has never been completed for
this release**, as `HANDOVER` says in the same change set. The ADR now carries a per-condition table
marking 3 **OPEN** (a release-time gate: it blocks the tag, not the merge), and the exceptions table
in `COMPATIBILITY_POLICY` says the same. The ADR's own rule — "must not claim a green gate it did
not observe" — was what the blanket sentence broke.

**The carve-out was scoped wrongly.** 2a read "no annotated tag exists for any build carrying the
old identity", and the exceptions table called it "spent", which would have closed the only route
for a later `PLUGIN_CODE` or `PRODUCT_NAME` change (a product rename before the first tag) on the
strength of a manufacturer-code change. 2a is a **condition on the state of the world, not a token
an exception consumes**: true while no tag exists — for every identity field at once — and
permanently false from the first tag, again for every field at once. Reworded in
`COMPATIBILITY_POLICY` and ADR-0023.

**The 0.9.0 → 0.9.1 first-tag renumbering was incomplete.** Still claiming v0.9.0 as the first tag:
`RELEASE_PROCESS` (§After release), `CHANGELOG_POLICY` rule 2, `FUTURE_RISKS` RISK-003,
`RELEASE_HARDENING_PLAN` (Version-management row, RH-R6, RH-PR-8 row, RH-F3 timing), `HANDOVER`
(Branch Strategy). All corrected.

**`RELEASE_PROCESS` contradicted the new pipeline check.** It still told maintainers an undated
`Unreleased` heading "would be published with that word in it" and that "the validation only checks
that the section *exists*" — the same change set had made that a fail-closed rejection. Corrected,
with the two practical consequences spelled out (date the heading **in the tagged commit**;
rehearsals only warn).

**The guard's justification was wrong about *what* it protects.** The release **title** is set
separately (`--title "Anamorph <version>"`); the extracted section becomes the **notes body**. The
check is right and stays; the wording is fixed in `release.yml`, `CI_CD`, `RELEASE_PROCESS` and
`HANDOVER`.

**Reported-then-corrected line-number drift (C6).** The previously reported `ADR-0001` citation
`CMakeLists.txt` `:62-73` → `:124-135` (and `:149-166` → `:228-237` for the tests-link-the-core
range), plus the same drift in `TROUBLESHOOTING` (`:115-125` → `:124-135`), found by the reviewer.
Both re-verified against the file. Reporting came first, in the prior pass; this is the correction.

**Stale version snapshots refreshed:** `HANDOVER`'s snapshot preamble (v0.9.0 was never tagged;
0.9.1 is the release in preparation and the first tag) and `COMMERCIAL_STATUS` (§Last reviewed,
§2, §6). `COMMERCIAL_STATUS` keeps its **2026-07-26** review date deliberately — its substance
(product model, distribution model, open owner/legal decisions) is untouched by a version
renumbering, and a review date that moves on every version bump stops meaning anything.

Prior: for the **ADR-0023 sign-off + the pluginval dependency fix** (2026-07-30). Two
changes, neither touching `src/`.

**ADR-0023 is now `Accepted`.** The maintainer signed off the Architecture Review and performed the
Level-5 identity check on 2026-07-30 — the 0.9.1 build registers under the new identity in a host,
and `auval -v aufx Anmr RTec` was run on macOS. That was the only check that exercises the change,
and no automated gate in this repository could have stood in for it, since nothing in the suite
observes plug-in identity. Status synced in ADR-0023 (incl. its *Verification performed* section),
`ADR_INDEX`, `COMPATIBILITY_POLICY` (exceptions table), and `HANDOVER` (Current Version, Release
Status, Known Blockers — the v0.9.1 tag blockers drop from five to **four**; the remaining four are
the missing licence plus three `RELEASE_POLICY` preconditions, all carried unchanged from the
v0.9.0 audit and all still requiring a human). Deliberately **not** upgraded: the ADR's
`AnamorphTests` / `AnamorphStateTests` / pluginval rows stay `Unverified in-repo` — the sign-off
covered the identity behaviour, which is what needed a human; the machine-checkable gates are
reported by CI on the change set, and an ADR must not claim a green gate it did not observe (C2/C7).

**`scripts/setup-linux.sh` now installs `curl` and `unzip`.** `run-pluginval.sh` calls both to
fetch and extract the pluginval release, and neither was installed — `libcurl4-openssl-dev` is the
development headers, not the CLI. GitHub-hosted runners preinstall both, which is exactly why the
gap never showed in CI and would only have bitten on a fresh machine or a minimal container, i.e.
the case this script exists to cover. Found while fixing the same defect in the sibling repository.
Synced: `BUILD.md` (§Linux dependencies — package list plus a paragraph separating the three
pluginval-only packages from the build dependencies; §Network domains), `TROUBLESHOOTING.md` (a new
`command not found` row). The script gained lines, so the `setup-linux.sh:NNN` citations in both
documents were re-based: package list `:24-32` → `:29-38`, EGL note `:13-15` → `:18-20`, network
domains `:8-13` → `:8-12`, webkit `:31`/`:36` → `:37`/`:42`, `libegl-dev` `:30` → `:36`.

Prior: for the **vendor manufacturer-code change** (2026-07-30, on top of `main` @
`c0fca30`). **No `src/` change; DSP, parameter surface and serialized state bit-identical to
0.9.0.** `PLUGIN_MANUFACTURER_CODE` changes `Anmf` → `RTec` (`CMakeLists.txt` `:153`) so the vendor
code spells RollyTech rather than the first product, ahead of the second product line member
(Anabasis) adopting the same value. Version bumped to **0.9.1** (`CMakeLists.txt` `:14`). The code is
host-facing identity — the AU component's manufacturer field, and an input to JUCE's VST3 class
UID — so pre-0.9.1 sessions report the plug-in as missing; that is documented, not fixable, and
one-time. Added: **ADR-0023** (options incl. "keep `Anmf` forever" and the rejected
`Roll`/`RolT`/`RlyT` candidates; opened as `Proposed`, `Accepted` 2026-07-30 — see the head entry)
+ its `ADR_INDEX` row; **KI-016** + its summary-table row.
ADR count in the self-coverage table synced 17 → **18**.
Synced: CHANGELOG (`[0.9.1] ### Changed`, evidence = PR #97 per `CHANGELOG_POLICY` rule 2; the
preamble's "from [0.9.0] onward each release is tagged" claim corrected — 0.9.0 was written up but
never tagged, so the first annotated tag will be v0.9.1), README (§Project status), HANDOVER
(Current Version, Release Status incl. the tag name `v0.9.1`, Known Blockers), COMPATIBILITY_POLICY
(new *Plugin identity change* prohibited-row; an **identity carve-out** to exception condition 2 —
enacted by ADR-0023, because condition 2 as written is unsatisfiable by construction for an
identity change and the policy would otherwise have contradicted itself; and an
"Exceptions granted so far" table recording that the carve-out's 2a ground is spent),
RELEASE_PROCESS (§Tagging — next tag is `v0.9.1`, and date the heading before tagging),
`release.yml` + CI_CD (a fail-closed check rejecting a tag whose CHANGELOG heading is still marked
`Unreleased`, since the heading is published verbatim as the release-notes title), TRADEMARKS
(§1 — the code is a RollyTech name-bearing identifier), PACKAGING (§Plugin identifiers),
KNOWN_ISSUES (version-sync lead), and every `auval -v aufx Anmr Anmf` invocation → `RTec`
(`packaging/macos/INSTALL.txt`, `docs/user/INSTALLATION.md`, `PACKAGING.md`, `TROUBLESHOOTING.md`,
`TESTING.md`, `KNOWN_ISSUES.md` KI-014, `RELEASE_HARDENING_PLAN.md` RH-F3).
**Deliberately not changed:** `worklogs/PRODUCT_READINESS_ROADMAP_v0.8.13.md:36` still carries the
old `auval` recipe — worklogs are a historical evidence trail, not maintained documents, and
rewriting one to match today's code would falsify the record.
**Drift observed, not corrected (constraint C6):** `ADR-0001` cites `CMakeLists.txt` `:62-73` for the
`AnamorphDSP` INTERFACE library, which now lives at `:124-135` (as `ARCHITECTURE.md` correctly
states); this predates the present change and is out of its scope.
The comment added beside the new code is deliberately same-line, so no `CMakeLists.txt:NNN`
citation anywhere in the documentation shifts.

Prior: for the **product video script worklog** (2026-07-29, on top of `main` @
`82b2f61`). **No `src/` change; no product-document change.** Added (and subsequently revised,
in the same unmerged branch) `worklogs/KEYNOTE_SCRIPT_v0.9.0.md` — a session work product
(marketing draft): a locked product positioning ("width is a method"; the plugin as instrument,
not assistant) and a complete ~7-minute developer-walkthrough video script with production
guardrails, all derived from the existing developer chain. An earlier keynote-style draft in
the same file was superseded by this revision; a Chinese adaptation is deferred until the
English script is approved. It is explicitly marked derived content, may never be cited
as evidence, quotes no unmeasured performance numbers (constraint C2), uses no ™/® symbols
(`TRADEMARKS.md`), and does not alter product status (`docs/COMMERCIAL_STATUS.md`: v0.9.0
remains internal-testing, not for sale). Worklogs sit outside the four documentation classes
(`docs/REPOSITORY_MAP.md` describes `worklogs/` generically), so no
`REPOSITORY_MAP`/`SOURCE_OF_TRUTH`/README class-table change applies; this entry satisfies the
audit obligation. Not a changelog entry (no user-visible product change, `CHANGELOG_POLICY.md`
rule 3).

Prior: for the **artifact & INSTALL.txt cleanup pass** (2026-07-26, on top of `main` @
`2d0a906`). **No `src/` change; no installer or runtime behaviour change.** The three internal
`Anamorph-<OS>-release` artifacts are **removed**, along with the archive-creation steps that fed
them (`zip -ry` / `Compress-Archive` / `ditto -c -k`): each platform now uploads exactly one
customer artifact (`Anamorph-<OS>`, loose files) plus its `-debug` symbols. `release.yml`
downloads those same trees, restores the executable bits the artifact transport drops
(`Anamorph`, `install.sh`, `uninstall.sh`, `*.so` on Linux; `*/Contents/MacOS/*` on macOS;
Windows carries no Unix modes), archives each tree with its entries at the archive root, and then
**fails closed** unless every expected executable is present in the published zip with its mode —
so release assets keep their names, contents and permissions, and no nested archive is
reintroduced. The three `packaging/*/INSTALL.txt` files lose their "Testing & third-party
attribution" section entirely and now carry installation instructions, paths, platform notes and a
copyright line only; the mandatory IJG acknowledgement therefore rests solely on the
release-page `NOTICE` asset that `RELEASE_POLICY` requires on every published release. Synced:
CI_CD (pipeline step 7, artifact table, route note), PACKAGING (artifact table, routes,
attribution table), RELEASE_POLICY (§Artifacts, §Third-party attribution), RELEASE_PROCESS
(§Build the release artifacts, §Tagging step 3), REPOSITORY_MAP, HANDOVER (snapshot base,
distribution), COMMERCIAL_STATUS, KNOWN_ISSUES (KI-015), FUTURE_RISKS (RISK-006), TRADEMARKS §3,
CHANGELOG `[0.9.0]`, this file.

Prior: for the **internal-testing preparation & closed-source product documentation pass**
(2026-07-26, PR #94, on top of `main` @ `aecd448`). **No `src/` change.** The v0.9.0 **release date moved to
2026-07-26** in `CHANGELOG.md` and its two HANDOVER restatements (PR-landing and audit-run dates
left untouched). `SUPPORT.md` was **rewritten from a public support document into the internal
testing guide** — evaluation-only permission, no source-code rights, no redistribution, the
project's testing channel, and six mandatory report fields (version+build, OS, DAW/host, format,
reproduction steps, logs/screenshots); the bug-report form became "Test report — bug" and carries
the closed-source + public-tracker notice. **New:** `EULA.md` (an **unapproved draft**, not in
force and not shipped, every open owner/legal decision marked), `PRIVACY.md` (collects nothing,
sends nothing; every disk write and the one About-screen link cited to source), `TRADEMARKS.md`
(name status, third-party marks used descriptively, the naming obligations IJG/Xiph/zlib impose,
and the `Dim-D` / "Roland Dimension-D-style" review item) and `docs/COMMERCIAL_STATUS.md` (the
internal index of product model, distribution model and the eight open owner/legal decisions —
including the newly recorded fact that the GitHub repository is public with forking enabled while
the product model is closed-source). README regrouped its documentation index into **four
classes** (user / internal-testing / legal / developer). Product-model wording was then **rebalanced on owner instruction**: it is
stated once for a general audience in `README.md` §Licensing and otherwise kept only where it is
operative — the legal class, the internal/testing class (`SUPPORT.md` §1, the bug-report form) and
the developer documents that derive the JUCE-tier consequence. The user-facing set stays on using
the product: `USER_MANUAL` and `INSTALLATION` end with a plain copyright line, every `INSTALL.txt`
carries one in its own bilingual section above and separate from the mandatory third-party
(**superseded 2026-07-26 — see the head entry: `INSTALL.txt` is installation-only, so no
attribution section remains for it to sit above**)
attribution (which is unchanged), and the manual's Quick start and FAQ carry no legal wording at
all.
Synced: README, SUPPORT, REPOSITORY_MAP (root + `docs/` trees, `user/` branch), SOURCE_OF_TRUTH
(four-class scope + per-class authority), HANDOVER (snapshot base, release date, KI-015 wording),
DOCUMENTATION_LIFECYCLE_POLICY (documentation-only trigger table), THIRD_PARTY_LICENSES
(§"Open licensing decisions" #2), CHANGELOG `[0.9.0]`, issue templates, this file (self-coverage
tiers, ADR count).

Prior: for the **flat-artifact / lean-package / closed-source documentation pass**
(2026-07-26, PR #93). **No `src/` change.** The `Anamorph-<OS>` artifacts now upload loose
staged files (payload + `INSTALL.txt`; Linux adds `install.sh`/`uninstall.sh` — extract the
artifact zip once to see them directly; the transport drops exec bits on that route), new
`Anamorph-<OS>-release` artifacts carry the permission-preserving source archives that
`release.yml` publishes byte-identically (**superseded 2026-07-26 — see the head entry: those
artifacts are removed and `release.yml` archives the release zips itself**), and `NOTICE`/`THIRD_PARTY_LICENSES.md`/`SUPPORT.md`
ship **only** as version-named release-page assets — no longer inside any zip or installer
payload; each `INSTALL.txt` carries the IJG acknowledgement + pointer (**superseded 2026-07-26 —
`INSTALL.txt` is installation-only**). README now states the
product model (closed-source commercial; docs grouped user/legal/developer — the grouping
was superseded 2026-07-26, see the head entry), and the
licensing blocker set (KI-015/RISK-006/RH-R11/RH-F1, THIRD_PARTY_LICENSES, NOTICE, HANDOVER)
uniformly records that the model rules out the AGPLv3 arm, so the commercial JUCE licence is
required before commercial distribution. Synced: PACKAGING, CI_CD, RELEASE_PROCESS,
RELEASE_POLICY, REPOSITORY_MAP, SOURCE_OF_TRUTH (doc-class scope), USER_MANUAL (online link
fallbacks), CHANGELOG `[0.9.0]`, this file.

Prior: the **v0.9.0 release-hardening & commercial-readiness audit** (2026-07-25,
on `main` @ `0a98ebd`, PR #92; record: `worklogs/RELEASE_HARDENING_AUDIT_v0.9.0.md`). **No `src/`
change.** Six parallel investigation lenses + adversarial verification over the repository and
the pinned JUCE tree. **New:** `NOTICE` and `THIRD_PARTY_LICENSES.md` (verified third-party
inventory — every component classified compiled-in vs vendored-but-not-built from
`build/build.ninja` and object symbols rather than from JUCE's manifest, which is how FreeType
and stb, both vendored *inside* PlutoVG, were found; the Steinberg VST 3 SDK is **MIT** in JUCE
9.0.0, correcting RH-R10's GPLv3/proprietary claim, with the trademark/distribution review left
explicitly open); `SUPPORT.md`; `.github/ISSUE_TEMPLATE/{bug_report,config}.yml`. **Packaging
(superseded 2026-07-26 — see the head entry):** both attribution files at that point shipped
inside all three zips, installed unconditionally by the Windows installer, and were attached as
version-named release assets (covering the `.pkg` route) — several vendored licences (IJG,
FLAC, Ogg Vorbis) require the notice to accompany a binary distribution; the release-page
assets are now the sole route, with the IJG line in every `INSTALL.txt`. **User docs:** USER_MANUAL gained a Quick start, a Standalone-application
section, system requirements, a TOC and a rewritten FAQ (rescanning per DAW, Windows paths,
Gatekeeper both routes, presets, CPU, latency, automation, session compatibility), and three
defects were fixed — "set Mix to 0 %" was the manual's most-repeated instruction but `mixK` is
Advanced-only (`PluginEditor.cpp:856`), seven controls were documented under host-parameter
names rather than GUI labels, and the MULTIBAND `On` toggle was undocumented; INSTALLATION
gained the missing macOS `mkdir -p`; macOS `INSTALL.txt` dropped its "unsigned developer build
for testing" line. **Policy/plan:** RELEASE_POLICY (artifact list + a new third-party-attribution
precondition), RELEASE_HARDENING_PLAN (RH-R10 corrected, **RH-R11** added for the missing
LICENSE/EULA, §12a post-v0.9.0 follow-ups RH-F1..F6), PERFORMANCE_BUDGET (a required benchmark
procedure for RISK-002 — no infrastructure added), TESTING (a "gaps in the automated coverage"
section: the AU is never auval-validated, and no frozen golden-audio reference exists by
design), KNOWN_ISSUES (**KI-014** AU unvalidated, **KI-015** no declared licence), HANDOVER
(release status: four unsatisfied RELEASE_POLICY preconditions, none fixable by code),
REPOSITORY_MAP, PACKAGING, CI_CD, README, CHANGELOG `[0.9.0]`. Validation: Release build green,
140-check DSP + 774-check state suites green, pluginval strictness 10 green in both modes ×3.
Prior: for the **post-v0.9.0 maintenance audit** (2026-07-24, on `main` @ `4226d2c`):
a repository-wide drift/maintainability pass with **no behaviour change** — no DSP, GUI,
parameter, serialization or CI-gate change, so **no CHANGELOG entry and no version bump**
(CHANGELOG_POLICY rule 3). Fixed: `CMakeLists.txt` — the 9-file wrapper/GUI source list was
duplicated verbatim between the plugin target and `AnamorphStateTests` (a new source added to
one only would silently desync the state suite's coverage) → single `ANAMORPH_PLUGIN_SOURCES`
variable, build graph provably identical (`ninja: no work to do` after reconfigure), plus the
stale "DSP self-tests" section header for a block that builds both suites;
`.github/dependabot.yml` — comment still said JUCE **8.0.14** pinned to a **tag** (it is 9.0.0
pinned by immutable commit SHA, ADR-0022); `release.yml` — stale `v0.8.13` tag example (the
first tag is v0.9.0); `packaging/windows/Anamorph.iss` — validation note cited a CI run and step
name that predate the rewritten script; `packaging/windows/INSTALL.txt` — the zip's own notes
told the reader to run an installer `.exe` that is not in the zip (macOS INSTALL.txt already
said "from the GitHub release"); `BUILD.md` — the Linux dependency list omitted **`libegl-dev`**,
required since JUCE 9, and still called the JUCE pin a "tag" (+ a new EGL row in
TROUBLESHOOTING); `SOURCE_OF_TRUTH.md` — authority level 2 named only `tests/dsp_tests.cpp`,
not `tests/state_tests.cpp`; **KI-002 rewritten** — it claimed manual `xattr` is required for
macOS artifacts full stop, contradicting the v0.9.0 `.pkg` route (payloads are not quarantined;
what remains there is the one-time Gatekeeper approval of the unsigned package), with the same
zip-vs-pkg scope applied in `PACKAGING.md`, `RELEASE_PROCESS.md` and `TROUBLESHOOTING.md`;
`HANDOVER.md` — snapshot HEAD frozen at `86b4273` (pre-#88/#89) → `4226d2c`, and "the three
installable packages" → the two installers + the in-zip Linux scripts; `FUTURE_RISKS.md` /
`KNOWN_ISSUES.md` version-sync headers extended to PR #89. Stale `file:line` evidence
citations corrected where they pointed at unrelated code: RELEASE_POLICY (`build.yml:54,156,373`
→ `:60,180,432`), KNOWN_ISSUES KI-002 (`build.yml:495-498` → `:558-561`; macOS INSTALL.txt
ranges), DEPENDENCY_POLICY (`run-pluginval.sh:34` → `:43-48`), TROUBLESHOOTING
(`run-pluginval.sh:42-44` → `:50-53`; `setup-linux.sh:33` → `:31,36`), PACKAGING (two bare
`INSTALL.txt:` cites qualified to `packaging/macos/`). Validation: Release build green, 140-check
DSP + 774-check state suites green, no new compiler warnings. Reported but NOT fixed (need an
owner / exceed "minimal"): the CMake-version-parsing regex exists in three independent copies
(build.yml Windows PowerShell, build.yml macOS `sed`, release.yml `sed`); the Windows installer
does not remember the VST3 folder across upgrades and its `UninstallDisplayIcon` points at
`Anamorph.exe` even on a VST3-only install; HANDOVER's status cells are multi-thousand-character
single table rows. Prior: for the **v0.9.0 installer/packaging rework** (2026-07-24, PR #89): the
Windows Inno Setup installer gains a **component page** (Install VST3 / Install
Standalone, both pre-selected, ≥1 enforced) and a **single destination page with both
paths** (VST3 above Standalone; the launch-after-install checkbox is removed); the macOS
`.pkg` gains **component selection** (hand-written distribution, `customize="allow"`,
full-install default, system-wide domain); the Linux `install.sh`/`uninstall.sh` move
**into the zip** and switch to **system-wide** installs (`/usr/lib/vst3`,
`/usr/local/bin`, root required) — the separate `Anamorph-<version>-Linux.tar.gz`
package artifact is REMOVED (release archives are flat ZIPs only, payload at the archive
root); all three `INSTALL.txt` files restructured (Installer vs Manual sections, both
system-wide). Docs synced per the lifecycle triggers: PACKAGING (artifact table,
archive-contract note, §Installers rewrite, install-locations table incl. Standalone
row), CI_CD (pipeline item 8 + artifact table), RELEASE_PROCESS (asset list),
INSTALLATION.md (all three platforms + version-number placeholders replacing literal
versions), USER_MANUAL (version-agnostic wording), README (Installing section),
KNOWN_ISSUES (KI-005 wording), RELEASE_HARDENING_PLAN (installer rows/plan wording),
HANDOVER (status rows), REPOSITORY_MAP (packaging rows), CHANGELOG `[0.9.0]` packaging
bullet. Prior: for the **v0.9.0 release preparation** (2026-07-24, PR #87, on top of
`main` @ `86b4273`): version bump 0.8.12 → **0.9.0** + CHANGELOG `[0.9.0]`; **installable
packages** added to CI (Linux `Anamorph-<version>-Linux.tar.gz` + `packaging/linux/`
install/uninstall scripts, Windows `Anamorph-<version>-Windows-Installer.exe` via
`packaging/windows/Anamorph.iss`, macOS `Anamorph-<version>-macOS.pkg` via
`packaging/macos/build-pkg.sh` — all built from the same validated staging dirs, uploaded
as three NEW artifacts, staged fail-closed into the draft release by release.yml alongside
the unchanged zips); **NEW user docs area `docs/user/`** (USER_MANUAL.md — full end-user
manual, attached to releases; INSTALLATION.md — per-platform install guide) closing the
roadmap's "zero user docs" P0 gap; `INSTALL.txt` now ships in all three zips (previously
macOS only). Docs synced per the lifecycle triggers: PACKAGING (installable-packages
section replaces the "no installer" TODO; artifact table + install-locations evidence),
RELEASE_PROCESS (v0.9.0 tag examples; release-asset list), CI_CD (triggers/pipeline/
artifact table incl. the macOS-debug best-effort correction), README (version, user-docs
links, Releases distribution), HANDOVER (all status rows), KNOWN_ISSUES (KI-005 resolved —
installers exist; header re-synced), FUTURE_RISKS (header re-synced), CHANGELOG_POLICY +
CHANGELOG preamble (tags exist from v0.9.0), RELEASE_HARDENING_PLAN (RH-R5 mitigated,
RH-PR-5b/6 skeletons landed, first-tag references v0.8.13 → v0.9.0), REPOSITORY_MAP
(docs/user/ + packaging/* rows), and one stale code comment (SpectrumImager.h alt-click
solo semantics, pre-0.8.10 wording). Prior: for the **product-readiness roadmap review** (v0.8.13 cycle, 2026-07-23, on
`main` @ `dcfad73`; extended the same day with the **item-by-item re-evaluation + independent
gap hunt**: 14 carried items re-classified (Must-now / before-1.0 / nice / defer, with
reasons); NEW findings — **Steinberg VST3-SDK licence compliance + third-party NOTICES**
(recorded as RH-R10 in RELEASE_HARDENING_PLAN §2, the pass's one doc fix beyond the roadmap),
support-workflow gap (no issue templates/SUPPORT.md), the undo/gesture-coalescer test gap
(largest hand-verified-only subsystem; now cheap to cover via the state-harness target), and
a "what 1.0 commits to" policy gap; plus an explicit outdated-assumptions retirement list.
Original entry: (v0.8.13 cycle, 2026-07-23, on
`main` @ `dcfad73` — PRs #82/#83/#84/#85 all merged). Roadmap-only pass, deliberately NOT
another audit: drift review limited to correctness-affecting items (none found). NEW
`worklogs/PRODUCT_READINESS_ROADMAP_v0.8.13.md` — maturity assessment (engineering High /
release Medium-high / commercial Low / UX Medium), blockers split (pre-1.0 vs 0.9.x vs
optional), 4-phase ordered roadmap (v0.8.13 completion → user-facing readiness → commercial
infra → v1.0 prep), documentation-review verdicts (user docs MISSING → Phase-2 item 1;
developer + release docs sufficient/complete), and technical-order rationale (auval before
host matrix; presets before golden-audio; signing before installers; licensing last).
HANDOVER Roadmap row re-pointed at the new plan (the previous pointer directed the next agent
at already-finished work). Prior: for the **RH-PR-8 release-pipeline foundation + its review follow-up
(release-artifact integrity)** — the follow-up archives customer artifacts **at the source**
(`zip -ry` / `Compress-Archive` / `ditto`) because the artifact transport preserves neither
Unix permissions nor symlinks, and turns the release job's staging into a **rename-only**
step (the archives CI validated are published byte-identically; Linux round-trip proven
locally with real build output — 755 bits + `cmp`-identical; annotated-accept /
lightweight-reject tag tests replicated green; PACKAGING.md artifact-layout table updated
to the single-archive contents) — (v0.8.13 cycle, 2026-07-23,
branch `claude/beautiful-sagan-JAUFI` on `main` @ `ee82380` — PR #83 merged). Infrastructure
only, no product behaviour/version change. NEW `.github/workflows/release.yml` (annotated
`vX.Y.Z` tag → fail-closed tag⇄version⇄CHANGELOG validation → the FULL existing `build.yml`
gates reused via a new additive `workflow_call` trigger (6-line `on:`-block diff, branch/PR
behaviour byte-identical; tag pushes triggered nothing before) → **draft** GitHub Release
with versioned artifact copies + `SHA256SUMS.txt` + `RELEASE_MANIFEST.txt` + CHANGELOG-section
notes; `workflow_dispatch` rehearsal mode; `contents: write` scoped to the one release job;
no third-party actions beyond `actions/*` + `gh`; publishing stays manual per RELEASE_POLICY).
No tag created (first: the v0.8.13 release — closes RISK-003 when cut). Docs synced:
RELEASE_PROCESS (§Tagging + release pipeline; stale "no tags TODO" replaced), RELEASE_POLICY
(Artifacts note), CI_CD (Triggers + source-of-truth), FUTURE_RISKS RISK-003 mitigation,
RELEASE_HARDENING_PLAN (§1 baseline rows + §10 RH-PR-8 row per its §13 update protocol),
REPOSITORY_MAP, HANDOVER. Work record:
`worklogs/release-hardening/RH_PR8_RELEASE_PIPELINE.md` (incl. the scoped dependency/security
review: no new third-party actions; SHA-pinning of actions + the pluginval download pin remain
open supply-chain items). Validation: both workflows YAML-parse; the validate/stage shell
logic executed locally against real repo data (version parse, CHANGELOG gate, 46-line notes
extraction); end-to-end proof = the post-merge `workflow_dispatch` rehearsal. Prior: for the **JUCE 8.0.14 → 9.0.0 migration & dependency hardening** (v0.8.13
cycle, 2026-07-23, branch `claude/beautiful-sagan-JAUFI` on `main` @ `1502077` — PR #82
merged). **Dependency migration, zero C++ source changes**: the complete 9.0.0
breaking-change surface has no project exposure (audit table in
`worklogs/JUCE9_MIGRATION_v0.8.13.md` §1.1). CMake pin → the tag's **immutable commit SHA**
`f8f8864…` with new `ANAMORPH_JUCE_VERSION` (supply-chain hardening, audit roadmap item);
`scripts/setup-linux.sh` + `libegl-dev` (JUCE 9 Linux GL uses EGL, not GLX). Validation:
engine output **bit-identical** 8.0.14 vs 9.0.0 (32-scenario twin dump incl. latencies);
140 + 774 suites green under 9.0.0 with the 8.0.14-frozen registry snapshot passing
**unchanged**; `juce_recommended_warning_flags` byte-identical and DSP-TU warnings identical
under both versions (no new warnings); pluginval on the CI gates (local egress 403, ADR-0012
precedent). New **ADR-0022** (Proposed — pending Architecture-Review sign-off + the
DEPENDENCY_POLICY Level-5 audition) + index row. Docs synced: DEPENDENCY_POLICY (SHA-pin rule +
EGL), BUILD, README, TROUBLESHOOTING (pin row + the discovered stale-CMake-cache trap row),
REPOSITORY_MAP, COMPATIBILITY_MATRIX, FUTURE_RISKS RISK-001, KNOWN_ISSUES (KI-011/KI-013
evidence re-verified against the JUCE 9 tree; KI-013 not fixed upstream), HANDOVER — plus a
repo-wide `CMakeLists.txt:NN` citation sweep (+5 shift from the pin block; every cite
re-verified, two pre-existing stale cites fixed: ARCHITECTURE.md, COMPATIBILITY_MATRIX VST3
row). No version bump / CHANGELOG entry (stays inside v0.8.13; a JUCE bump is user-visible at
release time — the release-prep changelog entry will record it, per the 8.0.14 precedent
where the bump shipped inside `[0.8.8]`). Prior: for the **state-serialization & parameter-compatibility regression harness**
(v0.8.13 cycle, 2026-07-23, branch `claude/beautiful-sagan-JAUFI` on `main` @ `823bfbe` —
PR #81 merged). **Validation infrastructure only** — no parameter, serialization, DSP or
user-visible behaviour change; no version bump / CHANGELOG entry (release-prep steps; the
changelog scopes to user-visible changes). NEW: `tests/state_tests.cpp` (9 headless
state-compatibility tests exercising the real `AnamorphAudioProcessor`: schema shape vs
SERIALIZATION_REGISTRY, parameter-registry snapshot vs a frozen fixture, raw-exact
save→load→save round-trip, the v0.2 / pre-0.6.4 / pre-0.8.4 legacy-migration paths via frozen
fixture XMLs, corrupt/foreign-state robustness, user-preset round-trip + exclusion rules, A/B +
view-param preservation), `tests/fixtures/` (registry snapshot + 3 legacy session models), the
`AnamorphStateTests` CMake console target (test block only — shipped targets untouched), and the
blocking CI wiring (`scripts/run-tests.sh` runs both suites fail-closed; the Windows job runs
both exes; step ids/gating unchanged). Docs synced: TESTING.md (new suite section + snapshot
workflow), TESTING_POLICY (Level-2 row + hard gate), RELEASE_COMPATIBILITY_CHECKLIST
(automation annotations on 4 items), CI_CD.md (pipeline step 4), REPOSITORY_MAP, BUILD.md,
DEVELOPMENT.md, README, RELEASE_HARDENING_PLAN (QA-gate row), HANDOVER. The whole edit set was
adversarially verified pre-commit (3 lenses: citation accuracy, test quality, policy/scope);
the pass surfaced and fixed one missed required sync (CI_CD.md), several overstated wordings,
and four test hardenings (recorded in the worklog §4). Design + architecture
record: `worklogs/STATE_HARNESS_v0.8.13.md` (includes the honest remaining-gaps statement:
legacy fixtures are reconstructions; cross-version vN−1→vN reload stays manual). Prior: for the **post-v0.8.12 repository audit & documentation-consistency pass**
(2026-07-22, branch `claude/beautiful-sagan-JAUFI` at `main` @ `64e87c4` — PR #80 merged).
**Documentation-only.** Two things: (1) **retroactive coverage of PR #80** (v0.8.12 GUI interaction
fixes: bare-press no-write + relative Width drag with 3 px threshold in `src/gui/SpectrumImager.{h,cpp}`,
release-outside stuck-press reconcile in `src/PluginEditor.cpp`; recorded in
`worklogs/BANDWIDTH_DRAG_FIX_v0.8.12.md` + `worklogs/MOUSE_RELEASE_STATE_FIX_v0.8.12.md` — PR #80
synced CHANGELOG/HANDOVER/worklogs but missed this file, a lifecycle slip closed here); and
(2) a **full drift audit with minimal corrections**: CHANGELOG `[0.8.12]` re-dated 2026-07-22 (two
of its fixes landed that day) and "MultiBand"/"Bandwidth" normalized to the registry terms
"Multiband"/"Width"; HANDOVER snapshot-HEAD + Build/Release-Status rows refreshed to v0.8.12 (were
frozen at v0.8.11/136 checks) and RH-PR-2 marked shipped; KNOWN_ISSUES + FUTURE_RISKS headers
re-synced (were at v0.8.10) with **KI-013 added** (macOS-inert release-outside reconcile — platform
limitation of the v0.8.12 fix); stale line-number evidence citations refreshed in KNOWN_ISSUES
(KI-001/002/003/006/009/012), FUTURE_RISKS (RISK-002 incl. marking the shipped H1/Wave-3
SoloMonitor skip, RISK-004), POSTMORTEMS (INC-003/004/006/007/009), REPOSITORY_MAP (test count
23→33, `FrameClock.h` + `LR4Xover.h` rows added, CMake cites), README (3-OS pluginval gate scope),
CI_CD (actions @v7), DEPENDENCY_POLICY (`JUCE_*` flags at `CMakeLists.txt` `:183-188`; "then-current"
qualifiers), PACKAGING + COMPATIBILITY_MATRIX (CMake line cites), ADR_INDEX (130-check/23-test
wording), BUILD + TESTING_POLICY + CODE_STYLE + TROUBLESHOOTING + RELEASE_PROCESS + TESTING (the
same class of post-RH-PR-2 stale CMake/script cites, caught by the pre-commit verification pass),
PERFORMANCE_BUDGET (GUI-redraw row gained its missing Wave-6/0.8.12 record), RELEASE_HARDENING_PLAN
("then-current 136-check" qualifier). The whole edit set was adversarially verified pre-commit
(3 lenses: citation accuracy, history preservation, completeness — see the worklog §1).
Roadmap + deferred-item review recorded in `worklogs/POST_v0.8.12_AUDIT_AND_ROADMAP.md`. No code
change; no version bump. Prior: for **performance Wave 6 — GPU/GUI rendering-efficiency (v0.8.12)** (2026-07-21,
branch `claude/beautiful-sagan-JAUFI`, restarted from `main` @ `c6f3226` — PR #78 merged). **One
behaviour-neutral code change** (`src/gui/SpectrumImager.cpp`, `paintHeadphone`): the per-band solo-
headphone transparency layer was allocating a **plot-sized offscreen framebuffer every Advanced-mode
frame** (JUCE sizes a transparency-layer offscreen to the current clip, which was the whole plot
rounded-rect, not the ~18×15 px glyph); it is now clipped to the glyph (+4 px, covering the earcups +
AA → pixel-identical) and skipped entirely at full opacity. A 5-lens adversarial Workflow (14 agents)
confirmed the idle/Simple/hidden GPU paths are already ~0 and at their frontier, and that the spectrum
**cannot** be made opaque pixel-identically (it nests bottom-flush in a translucent rounded panel, so
its bottom corners straddle a two-colour arc no flat pre-fill reproduces). Build + **140-check suite
green**; no DSP/threading/parameter/serialization/latency change; GPU measurement unavailable in the
headless container (analytical estimate — the affected GL path is macOS/Windows-only, Linux is CPU per
ADR-0011). Version bump `0.8.11 → 0.8.12` (`CMakeLists.txt` `:14`). Synced: this file, CHANGELOG
(`[0.8.12]` **### Changed**), HANDOVER (Current-Version + Pending-Tasks rows), README (version line).
Evidence: `worklogs/performance/WAVE6_GPU_RENDER_INVESTIGATION.md`. Prior: for the **v0.8.11 final
performance pass & release-readiness audit** (2026-07-20,
branch `claude/beautiful-sagan-JAUFI`, restarted from `main` @ `4aac4eb` — PR #76 = Waves 4+5,
merged). **No code change:** the three remaining named candidates were closed with measured
verdicts. The long-open **GUI fresh-eyes sweep** is DONE — carried in-line after the Workflow
lens was lost to the org token limit a third time; the GUI paint + message-thread surface is
already exhaustively cached/gated across Waves 1–4, the only residual (per-call `Path`/`Font`
locals in the shared `LookAndFeel` slider draws) transient and not worth a restructure. **W3-10**
deferred as Class B (a 50 M-sample probe: `applyWidth(·,·,1.0f)` differs from identity in 15.5 %
of samples, ~1 ULP). **W5-D** prototyped (`scratchpad/kwbench.cpp`): bit-exact vs the scalar
K-weighting chains but only 1.10× at the frozen SSE2 flags — the 4-wide win needs an AVX2/`-march`
build decision (itself numerics-frozen + FMA-divergent). `loudness.process()` confirmed
intentionally unconditional (feeds the live match readout). Release-readiness audit: build +
140-check suite green, no version/test-count drift, no release blockers; documentation-only, so
**no CHANGELOG entry** per CHANGELOG_POLICY rule 3. Synced: this file, PERFORMANCE_BUDGET (final-
pass bullet), HANDOVER (Pending-Tasks + Release-Status rows). Evidence:
`worklogs/performance/FINAL_PASS_v0.8.11_INVESTIGATION.md`. Prior: for **performance Wave 5 — per-block/settled-state runtime optimisation +
v0.8.11 changelog consolidation** (2026-07-20, branch `claude/beautiful-sagan-JAUFI`, rebased
onto main @ `912a755` — the security-tooling/CodeQL-autofix PRs #65–#75; the one rebase
conflict (both sides' new head entry in THIS file) was resolved by keeping both in order).
Eight Class-A trims from a two-lens fresh-eyes Workflow sweep (per-block + per-sample; the GUI
lens was lost to the org token limit for the second time — still open): the
`sameParameters` bitwise no-change gate on per-block parameter adoption (~250 → ~91
instructions per unchanged snapshot), the VelvetNoise parked fast loop (env/gate/history kept
per W3-9), the settled-Width hoist, meter-publish db reuse + bar-fall cache, Level-Match
estBoost memo + MEASURE-coeff cache + silent-block LUFS skip. Rejected with reasons in the
worklog: atomic-exchange load-gating (THREADING_POLICY conservatism), generation-keyed
snapshot cache (incomplete contract); deferred: K-weighting SIMD bank (W5-D), lat==0
mix-ring elimination (W5-A). Callgrind A/B: transparent −4.5 %, hostlike-b64 −5.5 %; twin
dump bit-exact ×19; suite 140 checks; warning set unchanged. Also corrected Wave 4's
drift-contaminated small-buffer datum (real overhead +10–20 %, not 2×). **v0.8.11
consolidation (maintainer instruction):** the `[Unreleased]` Wave-4 entry moved into
`[0.8.11]`, now dated **2026-07-20**, with a new Wave-5 sibling entry; HANDOVER
Current-Version/Release-Status/Pending-Tasks rows re-synced (PRs #60/#61/#62/#63/#76 named;
the CI-/test-only security PRs noted as changelog-exempt per CHANGELOG_POLICY rule 3);
PERFORMANCE_BUDGET gained the Wave-5 bullet and its Wave-4 CHANGELOG citations now point at
`[0.8.11]`. Evidence: `worklogs/performance/WAVE5_INVESTIGATION.md`. Prior: for **performance Wave 4 — idle/background runtime optimisation** (2026-07-19,
unreleased cycle, branch `claude/beautiful-sagan-JAUFI`). Implements the Wave-3 handover's
remaining ranked candidates, all Class A: LevelMeter static-layer cache + opaque (the H2/H13/N2
recipe — the last of the four visualizers; −29…−31 % per meter frame, raw-pixel-identical),
SpectrumImager per-transform dB cache (−92 % of the decay-tail tick) + paint `Path` reuse,
editor 24 Hz memoisations (preset-name shaping keyed on inputs, combo-hover pre-gate, match
readout on value change), Vectorscope hidden-editor gate, Haas parked fast path (rings keep
recording; new Test 34 `testHaasParkedWarmHistory` guards the warm history), vectorized NaN-scan
detector (bit-identical healing, NaN-injection twin rows), segmented scope/bypass ring copies
(publication contract unchanged). Callgrind A/B: transparent floor −4.9 %, haas-parked −12.4 %,
bypass-on −3.0 % whole-run instructions; 19-scenario twin dump bit-exact; suite 33 DSP tests +
A/B guard, checks 136→**140**; warning set byte-stable. A 4-lens verification/discovery
Workflow was lost to an org spend limit — verification was carried in-line against primary
sources; the fresh-eyes sweep is recorded as a follow-up in the worklog. Synced:
PERFORMANCE_BUDGET (GUI row + two new Wave-4 cost bullets), CHANGELOG (`[Unreleased]`, folded
into `[0.8.11] — 2026-07-20` by the Wave-5 consolidation),
TESTING_POLICY + TESTING + README + RELEASE_HARDENING_PLAN QA row (32/136 → 33/140), HANDOVER
(Test Status / Pending Tasks); investigation + validation evidence in
`worklogs/performance/WAVE4_INVESTIGATION.md`. Prior: for the **security-tooling configuration
review** (2026-07-19, branch `security-tooling/config-review`). The four generated GitHub
security configs were optimized against the repository's actual shape: `dependabot.yml` was
**invalid as generated** (`package-ecosystem: ""` — rejected by the Dependabot schema) and now
monitors the only supported ecosystem here, `github-actions` (weekly, grouped into one PR; JUCE
stays FetchContent-pinned + review-gated per `DEPENDENCY_POLICY.md`); `codeql.yml` switched
`c-cpp` from `build-mode: none` (near-zero include resolution — JUCE is absent from the bare
checkout) to a **manual build** mirroring the Linux CI steps but compiling only `Anamorph_VST3`
+ `AnamorphTests` with `-DANAMORPH_BUILD_STANDALONE=OFF`, with alerts scoped to repo-own code
(`paths-ignore: build` excludes the FetchContent'd JUCE tree) and docs-only changes skipping
the workflow; `msvc.yml` gained the **required** build step (juceaide-generated files),
JUCE-as-external suppression (`ignoredIncludePaths`/`ignoredTargetPaths` → `build/_deps`),
path-filtered triggers, and `upload-sarif` v3→v4; `dependency-review.yml` comments on failure
only. Validated: schema (github-workflows + dependabot vendor schemas), local build of the
exact analysis targets, 136/136 self-tests. Synced: CI_CD (§Security scanning),
REPOSITORY_MAP. Prior: for **RH-PR-2 Build Hardening + review follow-up** (2026-07-18, release-hardening
program, ADR-0021, PR #63 `release-hardening/build-hardening`, rebased onto the v0.8.11 bump —
the CHANGELOG entry now lives under `[0.8.11]` **### Security**). Behaviour-neutral binary
hygiene: an `AnamorphHardening` INTERFACE target pins `-fstack-protector-strong`, section GC,
Release `-g`, full RELRO (`-z,relro,-z,now,-z,noexecstack`) on Linux, `-Wl,-dead_strip` on
macOS, and `/guard:cf` + `/DYNAMICBASE /NXCOMPAT` + Release `/Zi`+`/DEBUG /OPT:REF,ICF` on
Windows; CI runs a retain-then-strip pipeline (split `.debug`/dSYM/PDB captured as separate
`Anamorph-<OS>-debug` artifacts, public binaries stripped — Linux VST3 −19.8%, `nm: no
symbols`, dynamic exports untouched; Linux strips before pluginval so the gate validates
shipped bytes; macOS order dsymutil → strip → codesign with `|| true` swallowing removed;
`if-no-files-found: error` everywhere). **Review follow-up (artifact-safety):** customer
uploads are now gated on their strip/staging steps succeeding (`steps.<id>.outcome` — the old
`if: always()` could upload an unstripped Linux binary after a strip failure), the Windows
staging purges ALL debug material from the public copy immediately after the copy and before
any abortable validation (the old order could leak the in-bundle PDB), and both public staging
steps end with an explicit no-symtab/no-`.debug`/no-PDB self-validation. Numerics-affecting
flags untouched; proven by a byte-identical twin engine dump + a green full suite (136 checks
post-Wave-3). Baseline finding recorded: symbol visibility was ALREADY hidden via JUCE's
plugin helpers (plan §1 drift corrected). Synced: new ADR-0021 (+ ADR_INDEX row),
RELEASE_HARDENING_PLAN (§1/§2/§6.1/§10/§12 statuses + the pending QA-row 32/136 sync noted by
the version-bump entry below), CI_CD, PACKAGING, BUILD, REPOSITORY_MAP (worklogs/ entry merged
with Wave 3's), CHANGELOG (`[0.8.11]` ### Security); investigation + validation + review
evidence in `worklogs/release-hardening/RH_PR2_INVESTIGATION.md`. Prior: for the **v0.8.11 version preparation** (2026-07-18, PR
`release/v0.8.11-version-bump` — version/release metadata only, no functional change).
`CMakeLists.txt` project version 0.8.10 → **0.8.11** (single source: `ANAMORPH_VERSION_STRING`
and the JUCE plugin version derive from it); README version line; HANDOVER status rows
(Current Version / Build / Release / Pending Tasks — the completed Wave-3 candidate removed
from the backlog text). CHANGELOG: the `[Unreleased]` Wave-3 entry became **`[0.8.11] —
2026-07-18`** (evidence PR #62, merge `b2481db`), and the two post-release maintenance fixes
recorded under `[0.8.10]` after it shipped — the slow-drag follower regression (PR #60,
`3268cc2`) and the 192 kHz terminal-snap robustness fix (PR #61, `c72d3c3`) — **moved into
`[0.8.11]`** with their evidence lines updated: the released 0.8.10 binaries (PR #59,
2026-07-14) predate both, so `[0.8.10]` claiming them was recorded drift against
CHANGELOG_POLICY rule 2 (no invented history). Deliberately untouched: PR #63's build-hardening
work and files (CMake hardening/CI/ADR-0021/RELEASE_HARDENING_PLAN — including that doc's
still-pending 32/136 QA-row sync noted in the previous entry). Prior: for
**performance Wave 3 — runtime optimisation** (2026-07-18, unreleased cycle,
PR `performance/wave3-runtime-optimization`). Investigation-first wave (baselines, callgrind
attribution and the full decision record live in `worklogs/performance/WAVE3_INVESTIGATION.md`
— a new top-level `worklogs/` directory for session-local records, added to REPOSITORY_MAP).
Four DSP changes + one GUI flag: **(1)** SoloMonitor's H1 cold gate decoupled from cutoff
proximity (gains alone prove the passthrough; a no-solo split drag — ~22 % of the drag-profile
instructions — no longer wakes the bank; Class A, guarded by new `testSoloColdThroughDrag`,
Test 33, proven to fail pre-change); **(2)** per-split LR4 coefficient sharing
(`LR4Xover::copyCoefficientsFrom`): x/dx/ax/dax always share one cutoff, so the glide, the
aligned-block resync and `setBankCutoffs` compute `tan` once per split (12→3 per sample worst
case) and the never-processed `ax[0]`/`dax[0]` are not updated at all (Class A); **(3)** the
phase-compensation allpass is the ladder's first 2nd-order section computed directly
(`LR4Xover::processSampleAllpass` — the recorded 0.8.10 follow-up; Class B ≤ 1.2e-7, 2–24
samples per 204,800 in the twin dump); **(4)** settled output-stage and settled-Mix per-sample
constants hoisted per block (Class A); **(5)** SpectrumImager FFT `ignoreNegativeFreqs=true`
(consumers read bins ≤ N/2 only; identical visuals). Rejected with reasons (recorded in the
worklog): LoudnessMatch off-gating (Measure readout + Apply are live consumers with Match off),
LevelMeters editor-closed gating (held peaks must persist), velvet parked-envelope freeze.
Fair interleaved before/after (session-local, 48 kHz): drags −35…−50 %, settled multiband
−9…−17 %, transparent floor −6.6 %. Suite 32 DSP tests + A/B guard, checks 130→**136**, twin
dump bit-exact on every Class-A row. Synced: PERFORMANCE_BUDGET (allpass follow-up marked done,
H1/crossover-move/GUI rows updated, stale process() line-range corrected), CHANGELOG
([Unreleased]), README, TESTING_POLICY, TESTING, HANDOVER, REPOSITORY_MAP (worklogs/).
**Deliberately NOT touched** (a parallel release-hardening PR owns release documentation):
RELEASE_HARDENING_PLAN.md — its QA-gate row still reads "31 DSP self-tests … (130 checks)" and
needs the one-line 32/136 sync once the PRs land (recorded drift, not silently fixed).
Prior: the **high-sample-rate crossover terminal-snap robustness fix** (2026-07-17,
v0.8.10 maintenance, PR `fix/high-sr-crossover-snap`). Review of the slew-limited smoother found
a numerical edge case, confirmed by exact-float simulation: the per-sample one-pole add stalls
once its move drops below `ulp(f)/2`, and the terminal-snap eps (0.05 + 2e-4·f) out-runs that
stall only up to 96 kHz (margin ≥ 1.76×; 3.55–4.27× at 44.1/48 kHz) — at 192 kHz the margin is
**0.88–0.98×** just past every binade edge ≥ 2048 Hz (parameter-range hard-stall zones
[2049–2093] [4097–4437] [8194–9125] [16388–18500] Hz, resting gap up to 3.75 Hz, both
directions; higher binades to the 86.4 kHz DSP clamp stall too, ≤ 0.4 cents), so a moved crossover
could rest short of its target forever: audio < 0.4 cents off, but the SoloMonitor settled fast
path (H1, needs ≤ 0.05 Hz) never engaged and filters/smoothers stayed hot. Minimal fix in
`MultibandWidth.cpp`/`SoloMonitor.cpp`: the glide **also snaps exactly when the float add can no
longer move the cutoff** — eps, R(f), smoothing, fade thresholds untouched; ≤ 96 kHz
bit-identical (eps snap always fires first). Guarded by `testHighRateCrossoverSnap` (Test 32;
DSP tests 30→**31**, checks 115→**130**): bitwise-exact landing + cold-path engagement at four
rates; pre-fix fails at 192 kHz only (0.4688/0.9375/1.8750/3.75 Hz, never cold — proven by
stash-rebuild). Synced: ADR-0015 (new "High-Sample-Rate Terminal-Snap Robustness" section),
CHANGELOG, README, TESTING_POLICY, TESTING, HANDOVER, RELEASE_HARDENING_PLAN QA row. Test-only
`getLiveCutoff`/`isSettledCold` accessors added to the two headers. Prior: the **crossover
follower slow-drag regression fix** (2026-07-17, post-merge
v0.8.10 maintenance, new PR). The v0.8.10 final flat ~4 oct/s cap was calibrated at a 150 Hz
crossing, but the display maps ~10 octaves onto ~900 px, so ordinary 400–2000 px/s drags are
4–22 oct/s — every normal drag trailed by octaves and crawled after release while violent flicks
escaped via the discrete-jump fade (the reported slow-vs-fast inversion). The glide in
`MultibandWidth`/`SoloMonitor` is now a **slew-limited smoother**: a ~20 ms one-pole demand
clamped per sample to a **frequency-proportional cap R(f) = 4·max(1, f/300 Hz) oct/s** — the
swept-allpass shift stays ≤ 1.25 Hz below 300 Hz (150 Hz crossing still ~14 cents) and ~7 cents
of the crossing above; the one-pole leg de-staircases the 60 Hz UI cadence and tapers arrivals
(a bare clamp landing measured −24 dBc; fref = 300 is the measured spur knee, −41.3 dBc at the
floor). Normal drags now track 1:1 (600 px/s converges 0.01 s after release, was 0.63 s); all
prior artifact bounds hold at the same values. Test 29 gained a normal-drag tracking regression
on both paths (checks 112→**115**; flat-cap re-pin fails both — verified in both directions).
Synced: ADR-0015 (new "Crossover Follower Slow-Drag Regression" section, + ADR_INDEX row),
CHANGELOG, DSP_ALGORITHMS, DSP_GRAPH_REFERENCE, DSP_POLICY inv. 3, PERFORMANCE_BUDGET,
REALTIME_SAFETY_AUDIT, FUTURE_RISKS RISK-002, KI-012, TESTING, HANDOVER. Prior: the
**PR #59 final review fixes** (2026-07-17, two items). (1) **Forced duck
during an ordinary fade-out** — a forced request (undo/redo/A-B/preset) landing in the ~6 ms
fade-out window of a non-forced discrete duck was consumed but dropped, so the swap finished
with normal-duck semantics (no silent-bottom wholesale swap/smoother snap/clean-slate reset —
a stale Haas tail replayed at 0.494 peak against silent input). The engine now upgrades the
in-flight duck to forced in place (dry-fill stays off: never engaged mid-fade). CHANGELOG +
`testForcedSwapDuringOrdinaryFadeOut` (Test 31; DSP tests 29→**30**, checks 106→**112**;
README, TESTING_POLICY, TESTING, HANDOVER synced). (2) **Crossover fade comments corrected**
(comment-only, `MultibandWidth.cpp/.h`, `SoloMonitor.cpp`): the discrete-jump bank fade's
destination is latched at fade start — movement during the fade waits (glide paused), and after
the fade lands a NEW fade may start toward the then-current targets (skipped if within 0.1 oct);
the old wording implied the fade always (re)targets the newest cutoffs. Prior: the
**v0.8.10 final follower decision** (2026-07-17, PR #59). The
bounded-convergence follower (1.25 oct/s cap + release consolidation) was evaluated in
interactive testing and **rejected for interaction latency**; final design (ADR-0015
"v0.8.10 final decision"): the rate cap rises to a hard **~4 oct/s** (drags ≤ 4 oct/s track
exactly — zero GUI/DSP gap; faster movement keeps a controlled ~15-cent worst FM at a 150 Hz
crossing, ~half the pre-fix implementation; a 6-oct flick catches up in ~1.25 s of continuous
motion) and the **release consolidation is removed entirely** (no timers, no delayed jump);
the discrete-jump bank fade is the only special event left. Synced: ADR-0015 (+ ADR_INDEX row),
DSP_ALGORITHMS, DSP_GRAPH_REFERENCE, DSP_POLICY inv. 3, PERFORMANCE_BUDGET,
REALTIME_SAFETY_AUDIT, FUTURE_RISKS RISK-002, KI-012 (rewritten to the accepted controlled-FM
trade), CHANGELOG, TESTING, HANDOVER;
Test 29 re-thresholded to the final operating point (18-cent bound, 1.7–2.2 s convergence
window; both rejection directions re-verified; checks stay **106**). Prior: the
**pre-release hardening plan** (2026-07-17, PR #59, docs-only): new
`docs/architecture/RELEASE_HARDENING_PLAN.md` — the planning artifact for the commercial-release
program (licensing, anti-piracy posture, build hardening, signing/notarization, installers,
release pipeline, multi-agent parallelization contract). No code change; decisions it proposes
are gated on future ADR-0016..0020 + Architecture Review. Architecture self-coverage count
updated (15 docs; ADR count synced to 15 after ADR-0015). Prior: the **v0.8.10
follower refinement + investigation record** (2026-07-14, PR #59) — bounded convergence via
rate cap 1.0 → 1.25 oct/s plus release consolidation, with the complete A–H3 architecture
investigation history (including the H3 hostile-review failure on width purity and
the linear-phase roadmap direction) made permanent as **ADR-0015**. Prior: the **third v0.8.10 pre-merge correctness
round** (2026-07-14, PR #59), two
items. (1) **Split-movement final design** — pure-sine testing rejected the second round's
one-pole tracker too (it FMs at the full drag rate: ~50 cents measured at a fast crossing). A
candidate matrix (rate caps, one-pole, chained/consolidated fades) was measured against the
sine protocol; shipped: a **hard ~1 oct/s cutoff rate cap** (swept-allpass shift bounded at
~0.31 Hz, below the pure-tone JND at any drag speed — worst measured chunk 3.6 cents at a
150 Hz crossing, spurs at the −41 dBc floor) plus a **discrete-jump bank crossfade** (target
steps > 1.5 oct between consecutive blocks land in ~12 ms). The audible-position-eases-at-
~1 oct/s trade is recorded as **KI-012** (with the linear-phase escape hatch gated behind an
Architecture Review). Docs: DSP_ALGORITHMS, DSP_GRAPH_REFERENCE, PERFORMANCE_BUDGET,
REALTIME_SAFETY_AUDIT, DSP_POLICY inv. 3, FUTURE_RISKS RISK-002, CHANGELOG; Test 29 reworked to
grade the whole movement (drag + entire ease incl. the tone crossing + discrete-jump landing).
(2) **Forced-duck dry-fill output-gain latch** — the fill played the raw ring at unity while
the processed path around it was scaled by Output Gain × Balance; at −24 dB an undo/redo Mix
toggle spiked 15.8×. The fill gain is now latched at fade-out entry like `dryDuckLat`
(SIGNAL_FLOW forced-swap note, CHANGELOG); new `testDryFillRespectsOutputGain` (Test 30). DSP
test count 28→**29**, checks 97→**102** (README, TESTING_POLICY, TESTING, HANDOVER). Prior: the
**second v0.8.10 pre-merge correctness round** (2026-07-14, PR #59), two
fixes. (1) **Split-drag transition rework** — pure-sine testing of the first round's chained bank
crossfades showed modulation sidebands around the tone (−25…−28 dBc during a fast drag: a chain
of ~12 ms fades is amplitude/phase modulation and cannot preserve the magnitude response
mid-fade). Final hybrid, picked by measurement: a bounded-time per-sample one-pole cutoff glide
(τ ≈ 15 ms — flat magnitude at every instant, smooth phase, settles ~75 ms after the last move)
for continuous movement, plus a single bank crossfade only for multi-octave jumps (> 1.5 oct,
where the fade's mod-2π phase wrap beats a glide's chirp). Documented across DSP_ALGORITHMS,
DSP_GRAPH_REFERENCE, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT, DSP_POLICY inv. 3, FUTURE_RISKS
RISK-002, CHANGELOG; Test 29 gained a spectral-purity check (max spur < −31 dBc while a split
crosses a 1 kHz tone at 60 Hz UI cadence; the chained fades measure −28.5 and fail), checks
96→**97** (TESTING, HANDOVER). (2) **KI-011, Apple-Silicon-native tooltip white corners** —
juce::TooltipWindow declares itself opaque while drawTooltip leaves the capsule corners
unpainted; the undefined pixels render white on ARM-native AppKit (Intel/Rosetta showed the
stale transparent backing). The editor now marks the TooltipWindow non-opaque on macOS
(KNOWN_ISSUES KI-011, CHANGELOG; hardware re-test pending, KI-006 pattern). Prior: the first
**v0.8.10 pre-merge correctness round** (2026-07-14, PR #59), three fixes:
(a) **Split-drag pitch shift** — `MultibandWidth` and `SoloMonitor` no longer glide their
crossover cutoffs per sample (a swept LR4's allpass phase rotation audibly detuned the audio
during and after a fast split/band drag); cutoff changes are now ~12 ms fixed-coefficient bank
crossfades (state-copied idle bank at the newest targets). Documented in DSP_ALGORITHMS
(MultibandWidth + SoloMonitor), DSP_GRAPH_REFERENCE (shared crossover sub-bank), PERFORMANCE_BUDGET
(crossover-move cost + the allpass-compensation candidate's obsolete sub-item), DSP_POLICY
invariant 3 wording, CHANGELOG; guarded by `testMultibandSplitDragNoPitchShift` (Test 29 — fails
at ~24 cents on the pre-fix glide). DSP test count 27→**28**, checks 90→**96** (README,
TESTING, HANDOVER). (b) **Band Solo alt-click redesign** — alt-clicking an UNSOLOED band's icon
now solos only that band (exclusive) instead of all bands; soloed-band alt-click (clear all) and
plain click unchanged; CHANGELOG (GUI-only, same `mbSolo` single-gesture write). (c) **Option/
double-click reset undo fix** — `Knob::doReset` now wraps the value write in a host change
gesture (the imager's split/width resets already did), so a reset is one undoable step that
clears redo; `undo()`/`redo()` flush a settled-but-unpolled gesture first. Conforms to ADR-0008's
gesture-coalesced design (no ADR change); CHANGELOG. No parameter/serialization/latency/threading
change; the split-drag fix changes only the transition behaviour of moving crossovers (settled
output bit-identical). Prior: the **v0.8.10 release finalization** (2026-07-14, PR #59). The `[Unreleased]`
CHANGELOG entries (undo/redo forced-duck dry-fill + rapid-swap robustness, multiband flat
recombination, adaptive `FrameClock` GUI refresh) are folded into the `[0.8.10]` section; the
version is bumped to 0.8.10 across CMakeLists / README / HANDOVER / KNOWN_ISSUES / FUTURE_RISKS;
KI-009 (REAPER Save Preset) is carried forward as an open, host-specific issue (not fixed).
Includes the pre-merge review round: (a) Multiband
flat recombination — the crossover reconstruction now phase-compensates each lower band by the
splits above it (allpass telescoping), removing the −17.75 dB dip at close crossovers; documented
in DSP_ALGORITHMS (MultibandWidth) + CHANGELOG, guarded by `testMultibandFlatRecombination`
(Test 28). (b) Rapid forced-swap dry-fill robustness — every forced swap re-evaluates dry-fill,
never reusing a prior swap's stale offset; CHANGELOG + `testRapidForcedSwapDryFill` (Test 27).
(c) FrameClock review — the Advanced-only SpectrumImager now stops its display-rate clock while
hidden (Simple mode), mirroring the meters (no unnecessary vblank ticks). DSP test count
25→**27**, checks 77→**90** (README, TESTING_POLICY, TESTING, HANDOVER; `testRapidForcedSwapDryFill`
gained fade-in and fade-out latency-crossing retarget cases during the pre-merge verification pass).
No parameter/automation/
preset/serialization/latency change; the multiband fix changes only the multiband audio output
(the intended fix — twin dump confirms latency unchanged, non-multiband scenarios identical).
Prior: for the **post-v0.8.9 PR** (three items + a fresh profiling baseline). (1) Undo/Redo
audible-dropout fix — the forced switch duck is now dry-filled from the true-bypass ring;
documented in SIGNAL_FLOW (forced-swap note) + CHANGELOG `[Unreleased]`, guarded by the new
`testForcedSwapNoDropout` (Test 26, count 24→**25** DSP tests, 73→**77** checks). (2) Adaptive
display-rate GUI refresh — new `gui::FrameClock` (VBlank, capped ~120 Hz) replaces the four fixed
60 Hz visualizer timers, with dt-corrected ballistics; new module coverage row + THREAD_MODEL timer
table/top-row + PERFORMANCE_BUDGET GUI row + CHANGELOG `[Unreleased]`. (3) **KI-009** added — the
REAPER Save Preset focus report (host-specific, pending manual investigation), version-sync header
updated. A post-v0.8.9 DSP+GUI profiling baseline was produced (callgrind Ir + wall-clock +
EdBench A/B); per established convention the report stays in the session scratchpad and is **not**
committed (no volatile clock-dependent numbers enter the permanent budget). Prior: the **v0.8.9
release** (finalized 2026-07-12, PR #58) — the `[Unreleased]` CHANGELOG entries from Wave-2 Step-1
and Step-2 (H3/H4/H5/H6/H11/H15/ALG-4, the tooltip revert, and the `viewGenWatcher` destructor
lifecycle fix) folded into `[0.8.9]`; every `CHANGELOG [Unreleased]` evidence citation across the
docs set (PERFORMANCE_BUDGET) updated to `CHANGELOG [0.8.9]`. One new module row (`LR4Xover`, the
flat-state LR4 crossover); H3/H4/H5/H6/H15 documented across DSP_ALGORITHMS, DSP_GRAPH_REFERENCE,
SIGNAL_FLOW, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT, THREAD_MODEL/THREADING_POLICY (two new
generation counters, same staleness-hint pattern), TESTING (new `testDryAlignGateRecomb`, test
count 23→24).
Prior: Wave-2 Step-1 (PR #58) — no module-coverage change; the H11/ALG-4 DSP work documented in
DSP_ALGORITHMS + PERFORMANCE_BUDGET + CHANGELOG, and `AI_AGENT_POLICY.md` gained constraint C8
(UI text requires explicit instruction). Retro-covers PR #57 (KNOWN_ISSUES KI-008 added; no
coverage change — this header was missed in that PR). Prior: the initial 0.8.9 version bump
(PR #56) — no coverage change; the 0.8.8 idle-performance PR (#54) — threading paths
(`soundParamGen`) and the ScopeBuffer per-block publication model documented; prior full audit at
HEAD `c605fbe` (JUCE 8.0.14).

## Code-module coverage

| Module | Documented in | Coverage | Confidence |
|---|---|---|---|
| `AnamorphEngine` (chain/switch machine) | SIGNAL_FLOW, DSP_GRAPH_REFERENCE, DSP_ALGORITHMS, ADR-0004/0005/0006 | Full | Verified |
| `EngineParameters` (POD boundary) | ARCHITECTURE, API_REFERENCE, ADR-0001 | Full | Verified |
| `PluginParameters` / APVTS | PARAMETER_REGISTRY, PARAMETER_REFERENCE, ADR-0002 | Full | Verified |
| `InternalState` | PARAMETER_REGISTRY, STATE_SERIALIZATION, ADR-0010 | Full | Verified |
| `PresetManager` | API_REFERENCE, STATE_SERIALIZATION | Partial (interface + role; preset file format not exhaustively documented) | Verified |
| State save/recall | STATE_SERIALIZATION, SERIALIZATION_REGISTRY | Full | Verified |
| `MidSide` | DSP_ALGORITHMS | Full | Verified |
| `HaasProcessor` | DSP_ALGORITHMS | Full | Verified |
| `VelvetNoise` | DSP_ALGORITHMS | Full | Verified |
| `ChorusEngine` | DSP_ALGORITHMS | Full | Verified |
| `MonoMaker` | DSP_ALGORITHMS, SIGNAL_FLOW, ADR-0006 | Full | Verified |
| `MultibandWidth` | DSP_ALGORITHMS, ADR-0005/0009 | Full | Verified |
| `LR4Xover` (flat-state LR4 crossover, Wave 2 / H6) | DSP_GRAPH_REFERENCE, DSP_ALGORITHMS, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT + its own bit-exactness contract comment | Full | Verified |
| `SoloMonitor` | DSP_ALGORITHMS, ADR-0004/0006 | Full | Verified |
| `LoudnessMatch` | DSP_ALGORITHMS, ADR-0007 | Full | Verified |
| `Correlation` / `LevelMeters` / `ScopeBuffer` | DSP_ALGORITHMS, THREAD_MODEL | Full | Verified |
| Threading / OpenGL gate | THREAD_MODEL, ADR-0011 | Full | Verified |
| Latency / PDC | LATENCY_MODEL, ADR-0003 | Full | Verified |
| Real-time safety | REALTIME_SAFETY_AUDIT, REALTIME_AUDIO_POLICY | Full | Verified |
| `gui/FrameClock` (adaptive display-rate refresh, post-0.8.9) | THREAD_MODEL, PERFORMANCE_BUDGET, CHANGELOG + its own header contract | Full | Verified |
| `PluginEditor` / `gui/*` | THREAD_MODEL, REPOSITORY_MAP | Partial (threading + lifecycle documented; per-widget layout/LookAndFeel not exhaustively) | Verified |
| Build / CI / packaging | BUILD, CI_CD, PACKAGING | Full | Verified |
| Tests | TESTING, TESTING_POLICY | Full | Verified |
| Performance (numbers) | PERFORMANCE_BUDGET | Structural only | Unverified (no benchmark data — TODOs) |
| Host (DAW) compatibility | COMPATIBILITY_MATRIX | Listed | Unverified (no in-repo DAW tests) |
| AAX, mono→mono | COMPATIBILITY_MATRIX, COMPATIBILITY_POLICY | Documented as excluded | Not Supported |

## Documentation-set self-coverage (deliverables present)

| Tier | Files | Status |
|---|---|---|
| docs root | SOURCE_OF_TRUTH, HANDOVER, REPOSITORY_MAP, DOCUMENTATION_COVERAGE, POSTMORTEMS, KNOWN_ISSUES, FUTURE_RISKS, COMMERCIAL_STATUS | Present |
| user | USER_MANUAL, INSTALLATION | Present |
| architecture | 15 docs (incl. RELEASE_HARDENING_PLAN) + ADR_INDEX + 23 ADRs (0016–0020 reserved, see plan §8) | Present |
| worklogs | performance/ (Waves 3–6 + the v0.8.11 final-pass and crossover-glide investigations), release-hardening/ (RH program working evidence; finalized decisions live in ADRs), root-level v0.8.12 GUI-fix records (`BANDWIDTH_DRAG_FIX_v0.8.12.md`, `MOUSE_RELEASE_STATE_FIX_v0.8.12.md`) + `POST_v0.8.12_AUDIT_AND_ROADMAP.md` + `STATE_HARNESS_v0.8.13.md` | Present |
| procedures | 8 docs | Present |
| policies | 15 docs | Present |
| root — developer/status | README, CHANGELOG, CLAUDE | Present |
| root — legal | NOTICE, THIRD_PARTY_LICENSES, EULA (unapproved draft — not in force), PRIVACY, TRADEMARKS | Present |
| root — internal/testing | SUPPORT | Present |
| .github | ISSUE_TEMPLATE/{bug_report,config}.yml, workflows/*, dependabot.yml | Present |

## Known coverage gaps / TODOs

- **Performance numbers** — `PERFORMANCE_BUDGET.md` carries explicit TODOs; populate from profiling.
- **DAW host matrix** — `COMPATIBILITY_MATRIX.md` hosts are Unverified; populate from manual testing.
- **GUI per-widget reference** — editor layout/LookAndFeel is documented at the threading/lifecycle
  level only; a per-widget reference is not present (low priority — GUI changes don't gate releases).
- **Pre-0.6 version history** — CHANGELOG entries for early versions are Partially Verified (README
  + commits); no git tags exist for exact per-version attribution.

## Update protocol

On any change, set this file's "Last updated" to the new HEAD and adjust the affected rows. A new
module → add a row; a new doc → add to self-coverage; new perf/host data → upgrade the confidence.


## A7-2T — the path-equivalence oracle (Test 40)

**What was missing.** The A7-2 investigation recommended replacing Velvet's linear history image
with a 1–3-run split read straight from the ring, and identified the defect class that rewrite is
most likely to produce: a **uniform** error in the tap delay, every tap reading one sample too deep.
That is a pure function of the sample stream, so it gives the same wrong answer at every block
length — and Test 39's oracle compares the build under test *against itself* at different block
lengths, with no reference implementation anywhere in it.

**The correction this round made to its own record.** PR #128 stated that a seeded one-sample
tap-delay error *passes* Test 39 at all four sample rates. That was measured on a session-local
harness which reproduced Test 39's comparison but not its schedule, and reported as Test 39's
result. Re-measured against the committed test, with the seed applied to the gather's tap source
pointer — the pre-A7-2B `linHist.data() + (decorrSamps - pos[t])` line, which A7-2B has since
deleted along with the image, so it carries no line citation:

| build under test | Test 39 | first difference |
|---|---|---|
| seeded, Test 39 as committed | **FAIL** ×4 rates | block 215 — the transport stop |
| seeded, transport-stop events removed | **FAIL** ×4 rates | block 247 — the moving density |
| seeded, every path crossing removed | **PASS** ×4 rates | — |
| seeded, Test 40 | **FAIL** 20 of 20 checks | sample 3 of block 0, every block size and rate |

So Test 39 does catch this seed, but through its **schedule** rather than its oracle: detection
depends on the run crossing from the gather to the per-sample loop, and a schedule that stayed on
the gather would not see it. The oracle-level blindness is real; the claim that the committed test
was blind was not.

**The oracle, and why it costs no product change.** The module already holds two implementations of
the same arithmetic. The gather's eligibility gate ends with `numSamples <= (int) accum.size()`
(`src/dsp/VelvetNoise.cpp:166`) — a clause whose stated purpose is direct callers rather than the
engine — and `accum` is sized from `prepare()`'s `maxBlockSize` alone. An instance prepared for a
**smaller** block therefore runs the per-sample loop over the same audio, and everything else about
it is identical: ring, tap positions and signs, weights, envelope/gate coefficients and stop step
all derive from the sample rate and seed, never from the block size. `linHist`, `accum` and `midBlk`
are the only block-sized state and the per-sample loop touches none of them. No test hook, no friend
declaration, no `#ifdef`.

**Coverage it adds beyond Test 39.** Block **4096**, which exceeds `decorrSamps` at 44.1 and 48 kHz
(1985 and 2160) so that *every* tap splits into a ring run plus a same-block tail — Test 39's largest
block is 512, so that regime was unreachable. And a **density-1.0** pass: at the 0.5 default exactly
32 of 64 taps are active and they are the shallow half (`pos` spans 3–982 at 44.1 kHz against a
1985-sample window), so the deep taps — where a ring read is likeliest to cross the ring origin —
were exercised by nothing in the suite.

**What is deliberately not asserted.** There is no output-observable way to prove from outside which
path an instance took, because the two paths are required to produce identical bits — that is the
property under test. Eligibility is established structurally instead: the targets are set *before*
`prepare()`, which assigns `current := target` and calls `updateWeights()`, so the density glide is
at its fixpoint and the amount engaged from the first block; the transport plays and never stops.
Every clause of the gate holds for the gather instance on every block, and the last clause is
provably false for the reference instance. The premise check (the gather must really decorrelate,
measured on the second half of the run so the ring is past `reset()`'s zero-fill) is what stops the
comparison being vacuously true.

**Verification.** `AnamorphTests` **202 checks** (178 before; Test 40 adds 24 — six at each of four
sample rates), 0 failures. `AnamorphStateTests` unchanged. `check-docs` clean;
`check-citations --self-test` and `--check` green. No product code changed: `src/` is byte-identical
to `main` in this round.

## A7-2B implemented, A7-5E answered, A7-9 re-verified

**A7-2B (Class A, measured).** `VelvetNoise` no longer builds a linear history image. H5 built it so
each tap could read one unit-stride run and A7-1 slid it forward each block; but the ring is already
unit-stride in `i` and merely wraps, so each tap now reads `midHist` in place as 1–3 unit-stride
runs plus this block's own Mids. `linHist`, the `linHistSlide` offset, its clear-on-entry, its
re-arm and its allocation are all deleted — **the module now carries no cross-block scratch state**,
so the invalidation rule A7-1 had to defend no longer has anything to defend.

Bit-identical on both committed instruments: **32/32 twin-dump scenarios** and **0 mismatches over
180 configurations**, with Test 40 comparing the gather against the per-sample loop directly — the
axis Test 39 cannot see, and the reason A7-2T had to land first. Engine cost **−12.2 % at
48 kHz/32, −37.2 % at 192 kHz/32, −12.8 % at 96 kHz/64**; the fixed per-block term fell 12,957 →
5,546 Ir at 48 kHz and **39,373 → 6,104 Ir at 192 kHz**. The shape is the result: A7-1's term was
proportional to `decorrSamps` and grew with the sample rate, and this one does not — at a 32-sample
block the 48→192 kHz penalty falls from **39.8 % to 0.04 %**.

**One corner is worse and is recorded rather than left to be found:** at 44.1 kHz / 32 with Density
at maximum the change is **+1.0 %**, because the image A7-1 slid is smallest at the lowest rate
while the new per-tap preamble is rate-independent and paid once per active tap. A no-wrap fast path
in the split holds it there — without it the same point measures +3.0 %, and the default-density
point is −0.2 % rather than +0.9 %.

Test 39 was **renamed** `testVelvetLinearImageInvariance` → `testVelvetBlockLengthInvariance` and its
rationale rewritten: it was named for a structure this change deletes. The assertion is unchanged and
still earns its place — it is about the module's contract rather than that mechanism — but a name
that outlives its subject is the drift this repository is written against.

**A7-5E — the cross-slice question, answered by inference.** The proposed experiment needed a macOS
runner; this container is x86-64 Linux with no aarch64 emulator, so the question was decided in two
measured halves instead. Cross-compiling the shipped DSP sources to aarch64 yields **308 FMA
instructions across the eight modules against 0 on the frozen x86-64 baseline** — AArch64's base ISA
has `FMLA` unconditionally, so `a*b+c` contracts with no flag, while x86-64 has no FMA for the same
permissive default to use. And with the ISA held fixed at `-march=haswell`, **contraction alone
changes 32/32 dump scenarios** while **vectorization alone changes nothing** (0/32, and 0 of 180
configurations). So the two shipped slices already produce different DSP output, and the numerics
contract is already architecture-dependent — ADR-0021's freeze protects the x86-64 side only.
The claim rests on codegen inspection, not execution; the confirming one-step CI diff stays open.

That separation **corrects the A7-5 investigation's attribution** — it named contraction as the
mechanism without separating it from vectorization — and it changes the AVX2 decision's shape:
**66 % of the win is Class A**, `-march=haswell -ffp-contract=off` measuring −17.2 % engine-wide
with 32/32 and 180/180 identical, against −25.8 % for the Class-B variant. An earlier `-mfma`-only
attempt is recorded as vacuous: GCC emitted 0 FMA instructions at that flag, so it reported
IDENTICAL for the wrong reason.

**A7-9 re-verified, and its economics changed.** The stall mechanism is unchanged (`≈ FLT_MIN / k`,
value-test gates in all three modules, Chorus's threshold scaling with the rate), and the residual
bound is unchanged at **4.476e-36**. But the cost of a missed park is the cost of the path the module
is stuck on, and A7-2B made that path cheap: Velvet's penalty at 192 kHz/32 fell from **+2,372 % to
+252 %**, i.e. 37,951 → 4,032 Ir/block, an **89 % reduction**. A7-9 is therefore **no longer the
largest item on the roadmap**, and the priority inside it has moved to `ChorusEngine` (+14,220
Ir/block at 48 kHz/128, now the largest contributor). The three false "flushes to true zero" comments
are filed separately as **A7-9C**: that correction is Class A, is required whether or not the
optimization is approved, and should not wait on a numerics decision it does not need.

**Verification.** `AnamorphTests` **202 checks** / 0 failures (Test 39 renamed, Test 40 unchanged);
`AnamorphStateTests` 920 / 0; ASan + UBSan + `local-bounds` + `pointer-overflow` with
`detect_leaks=1`: 200 checks, **0 diagnostics**; `AnamorphDspDump --self-check` passed; `check-docs`
107 files clean; `check-citations` self-test and `--check` green. Wall-clock is quoted nowhere in
this round: the same binary and workload measured 292 ms in the previous round and 196 ms in this
one, so only Ir is treated as a datum.

## A7-5E confirmed by execution · A7-9C landed

**A7-5E is no longer an inference.** `qemu-user-static` and `g++-aarch64-linux-gnu` turned out to be
installable here, so the previous round's codegen argument was replaced by a run of the **committed
harness itself**: `tests/dsp_dump.cpp` unmodified, with its eight DSP translation units and four JUCE
modules, cross-built for `aarch64-linux-gnu` and — from the same sources and flags — for `x86_64`,
then both executed and their 32 scenario hashes diffed. Both pass `--self-check`, so both instruments
are live.

**At default flags the two architectures differ in 32 of 32 scenarios.** The decomposition is what
makes it useful: aarch64 default vs aarch64 `-ffp-contract=off` differs in 32/32 (529 FMA
instructions in the DSP objects at default, **0** with the flag), while x86-64 default vs x86-64
`-ffp-contract=off` is **identical** — the frozen baseline has no FMA for the flag to affect. So
contraction is real, active and architecture-intrinsic.

**But a flag does not close the gap.** With contraction disabled on both sides, **24 of 32 scenarios
still differ**, and the split is perfectly clean: **all 8 scenarios at oversampling ×1 agree; all 24
at ×2/×4/×8 differ.** The residual is confined to the oversampling path, whose polyphase coefficients
are derived at runtime through transcendental libm calls, and libm is a different implementation per
architecture. Anamorph's own DSP is architecture-portable once contraction is off; JUCE's
oversampling filter design is not.

That **corrects a claim this programme made one round earlier** — that `-ffp-contract=off` would make
the two shipped macOS slices bit-identical for the first time. It would not; it would do so only with
oversampling off. Full cross-architecture identity would additionally require the oversampling
coefficients to stop coming from libm, which is a far larger question than a compiler flag and is not
proposed.

**Limits, stated rather than glossed:** GCC 13 cross-compiler rather than Apple Clang, `qemu-user`
rather than arm64 silicon, glibc rather than Apple libc. The contraction half is ISA-level and
transfers; the oversampling half was measured against glibc and its magnitude on macOS is not known.
The one-step macOS CI diff stays worth running — now as platform-exact confirmation of a measured
result rather than as the experiment that decides the question.

**A7-9C landed.** The three parked fast paths said their one-pole "flushes to true zero"; it does
not. Each site now states the precondition without the false justification and records how the state
is and is not reached — the closed form `≈ FLT_MIN / k`, that a fresh `prepare()` does reach the
parked state and turning the control down does not, and for `ChorusEngine` that its threshold scales
with the sample rate because `wSmooth` is `1/(0.01·sr)`. **The gates are deliberately untouched**:
changing them is the Class-B A7-9 decision, and this correction is Class A and required whether or
not that is approved. Verified behaviour-neutral — the 32-scenario twin dump is **identical** across
the change.

**Verification.** `AnamorphTests` 202/0, `AnamorphStateTests` 920/0, twin dump identical and
`--self-check` passed, `check-docs` 107 clean, `check-citations` green, `check-realtime` 44/0,
`check-portability` 52/0. The CHANGELOG entry added for A7-2B was also brought to the
`CHANGELOG_POLICY` entry template, which requires an `Evidence:` source and a verification marker —
it had neither.

## A7-5E closed — confirmed on the shipping toolchain

The cross-slice question has been answered by execution on the hardware and toolchain that ship the
product, not by inference. A CI job of its own, `macos-crossslice`, builds the committed
`AnamorphDspDump` for `arm64` and for `x86_64` from the same sources and flags on an Apple Silicon
runner, runs the arm64 slice natively and the x86_64 slice under Rosetta 2 (both passing
`--self-check`), and diffs the 32 scenario hashes.

**At shipped flags the two slices differ in 32 of 32 scenarios.** With `-ffp-contract=off` on both
sides, 24 still differ and 8 agree — and the 8 are every scenario at oversampling ×1 and only those.
That is the Linux measurement reproduced exactly: same counts, same split, same scenario names, now
against Apple Clang and Apple's libm rather than GCC and glibc. So both mechanisms are properties of
the architectures and of the libm boundary rather than of any one toolchain: contraction is
flag-removable, and the oversampling path's runtime-derived polyphase coefficients are not, because
**Apple's libm does not agree with itself across Apple's own two architectures**.

**The job is reporting-only and gates nothing**, deliberately: no policy, ADR or document claims
cross-architecture bit identity, so failing a run on it would invent a contract by CI rather than
decide one. It exits 0 on every path. Whoever decides the contract can promote it to a gate in the
same change, and it can be deleted outright once the question stops being interesting.

**Two process notes worth keeping, because both cost a full CI cycle.** The experiment first ran
inside the packaging `macos` job and produced correct numbers that were *unreadable*: that job emits
a dSYM warning flood of several hundred thousand lines afterwards, and every log-retrieval path caps
by character budget — 30,000 lines of tail reached ten seconds past the step's end. Routing the
result to `$GITHUB_STEP_SUMMARY` and `::notice::` did not fix it either, since Actions job summaries
are not exposed through the check-runs API. Moving the experiment to its own short-log job did, and
it was the right structure anyway: an experiment has no business inside the job that signs and
uploads customer artifacts. Separately, the first run's "scenarios agree" count included a blank
header line and printed 1 and 9 where the true figures are 0 and 8; the counting now ignores blank
lines, and the named scenarios were never affected.

**Verification.** All jobs green on the confirming run: `linux`, `linux-lto-tests`, `sanitizers`,
`realtime`, `fuzz`, `docs`, `source-lint`, `windows`, `macos` (pluginval VST3+AU, both modes ×3, plus
the Rosetta suites), `macos-intel` (native Intel), and `macos-crossslice`.

## The four A7 decisions implemented — ADR-0031, the ISA floor, A7-9, and a bound corrected

The investigation phase closed and all four approved items landed in one change set. Worklog:
`worklogs/performance/PERF_AUDIT_A7-9_AVX2_IMPLEMENTATION.md`.

**The order was forced by the approval and is worth recording as a pattern.** The compatibility
documentation had to land *before* the flag it describes, so `COMPATIBILITY_POLICY` gained the ISA
floor and the cross-architecture statement first, ADR-0031 recorded the decision and amended
ADR-0021 second, and `CMakeLists.txt` carried `-march=haswell` last. A user-visible contract with a
`SIGILL` failure mode documented after the fact is a period in which the product's supported
hardware is whatever the build files happen to say.

**ADR-0031 — the x86-64 ISA baseline.** `-march=haswell -ffp-contract=off`, on the GCC/Clang x86-64
builds only: Linux x86-64 and the macOS `x86_64` slice via `-Xarch_x86_64`, with **arm64 and MSVC
carrying nothing**, each for its own reason. arm64 has `FMLA` in its base ISA and contracts today, so
`-ffp-contract=off` there would be a Class-B change to shipped arm64 numerics for no user-visible
gain. MSVC's `/arch:AVX2` is not the equivalent pair — MSVC controls contraction separately — and no
twin-dump instrument runs on Windows, so the Class-A property could not be *demonstrated* there,
only assumed. Both exclusions are recorded in the ADR rather than left to be inferred from the
CMake, because "the flags are not on this platform" reads as an oversight unless something says it
is a decision.

> **The MSVC half of that paragraph is SUPERSEDED — ADR-0032, Accepted 2026-08-30.** It described
> ADR-0031's scope correctly and is kept for that, but MSVC no longer carries nothing: the Windows
> x64 build compiles `/arch:AVX2`. The blocker was the *missing instrument*, not the flag — the
> R-round built the Windows twin dump, it returned **0 of 32 scenarios moved** on toolset
> 14.51.36231 across two runs, and the Class-A property is demonstrated on Windows rather than
> assumed. "MSVC controls contraction separately" is what makes the flag safe there, not what blocks
> it: `/fp` is left at the default non-contracting `/fp:precise`, and `/fp:contract` — which moves
> 32 of 32 — stays off. The arm64 half stands unchanged. Every shipped x86-64 binary now carries an
> AVX2 baseline; only Apple Silicon has no ISA floor.

**Why the change is Class A, stated once so it is not re-derived.** The frozen SysV baseline has no
FMA instruction at all, which is why the permissive `-ffp-contract=fast` default has been inert on
this target for the project's whole life. `-march=haswell` introduces the instruction;
`-ffp-contract=off` forbids its use. The flag does not move the numerics, it *states* what the
missing instruction was enforcing by accident. Measured on this tree: **0 of 32 twin-dump scenarios
differ from the baseline**, **0 FMA instructions emitted** (707 with contraction left at its
default), **780 `%ymm` operands** so the flag is demonstrably doing work, −17.2 % engine-wide. The
GCC/Clang cross-check survives for the same reason it is Class A: neither toolchain can contract.

**A build option exists to turn it off, and that is deliberate.** `ANAMORPH_X86_ISA_BASELINE` (ON =
shipped) was added because a Class-A claim is worth what re-verifying it costs, and the flags live on
a target — CMake places target compile options *after* `CMAKE_CXX_FLAGS`, so a `-march` passed on the
command line is the *earlier* one and loses. Without the switch, reproducing the frozen baseline for
the `DEPENDENCY_POLICY` rule-2 twin dump means editing `CMakeLists.txt`, which is how a verification
stops being run. Turning it off emits a `WARNING` naming what was given up; nothing in CI turns it
off.

**Two CI consequences, both from Rosetta 2, which does not translate AVX2 by default.** The `macos`
job's Intel coverage and the `macos-crossslice` experiment both execute the `x86_64` slice under
translation, and both would now `SIGILL`. Each sets `ROSETTA_ADVERTISE_AVX=1` and **probes** with a
three-line AVX2 program built by the same compiler before running anything, degrading to a
`::warning::` when the probe faults — the same shape as the existing "Rosetta unavailable" path, and
for the same reason: it is a property of the image, not of the product. No gate is lost, because the
blocking Intel coverage is `macos-intel` on native hardware, which is Haswell+ by Apple's own
requirements for macOS 15. A7-5E's recorded finding is unaffected: part 1 now builds the x86_64 slice
at the ADR-0031 pair, but contraction was already impossible there, so part 2 still changes only the
arm64 side.

**A7-9 — three gates, from a value test to a fixpoint test.** `VelvetNoise`, `HaasProcessor` and
`ChorusEngine` each gated a cheap Amount-0 path on the wet glide reaching **exactly** 0, which under
FTZ it never does: with a 0 target the update is `a -= k*a` and the DECREMENT underflows before `a`
does, stalling the glide just under `FLT_MIN/k`. Every one of those paths was dead after a user
turned Amount down — the only route to the state they were written for. The gates now ask whether the
glide *can still move*, which is the test `VelvetNoise` has always used for its density glide three
lines above the one that was wrong. `VelvetNoise` computes `amountParked` **once** and uses it in
both directions, so its gather gate and its parked gate stay exact complements and no state can be
eligible for neither or for both. Each gate keeps the pre-A7-9 condition as a second disjunct for the
one input the fixpoint test does not subsume — a NaN target with the current value already 0 — so the
gates can only ever admit more than before.

**Test 41, and why the suite needed a new shape of test.** The oracle is a second instance of the
same module: one driven the way a user drives it, one parked from `prepare()`, both fed identical
input. All three modules record the *input* in their delay lines rather than their own output, so the
rings match and any difference is the residual. Three claims, three checks: real signal exactly
equal, silence within the derived `FLT_MIN/k` ceiling, and **silence exactly 0** — the last being the
gate, which fails on all four cases against the pre-A7-9 sources. `ChorusEngine` runs at 48 kHz *and*
192 kHz because its coefficient is the only rate-dependent one, so the worst case is asserted rather
than extrapolated. The committed twin dump reports 0 of 32 scenarios different for A7-9, and that
proves nothing: `tests/dsp_dump.cpp` holds `algoAmount` at 0.7 and never ramps down, which is exactly
how the defect survived from Wave 4 to now.

**A figure was corrected, and the correction is the point.** The investigation recorded 4.476e-36 as
the A7-9 residual bound and the approval carried it forward as a requirement. It is the maximum *that
harness* observed, not a stimulus-independent bound: driving ±0.7 noise, Test 41 measures 1.563e-35
(Chorus, 192 kHz), 8.043e-36 (Haas, 48 kHz) and 7.145e-36 (Velvet, 48 kHz) against the same pre-fix
sources — 3.5× the recorded figure. What actually bounds the residual is `FLT_MIN/k` times the
module's wet gain, which is derived per module inside the test rather than quoted. The direction of
the change is not in question — the residual is what the fix *removes*, and after it the silence
output is an exact zero, which is stronger than any bound — but a number quoted in policy-adjacent
documents should be the one that is true. Corrected in the decision packet and both A7 worklogs,
with the investigation's own §17 table left as the historical measurement it is, under a forward
reference.

**Documentation synced.** `COMPATIBILITY_POLICY` (ISA floor + cross-architecture numerics),
ADR-0031 + `ADR_INDEX` + the ADR-0021 amendment note, `COMPATIBILITY_MATRIX` (three platform rows
plus two new sections), `PERFORMANCE_BUDGET` (the A7-9 and AVX2 entries, and A7-2B's corner recorded
as accepted), `KNOWN_ISSUES` (**KI-026**, the first entry here recording a limitation that was
*chosen* rather than discovered), `BUILD.md` (the baseline, the three ways to get it wrong, and the
opt-out), `TESTING.md` + `TESTING_POLICY` + `README` (**39 → 40 DSP tests**, 202 → 214 checks),
`USER_MANUAL` + `INSTALLATION` (the processor requirement, stated before install rather than after a
crash), `CHANGELOG` (one **Changed** and one **Fixed**), `HANDOVER`.

## The R-round: the audit's recommendations executed as evidence and documentation

The platform-coverage audit's R-1…R-6 were executed with the standing decisions preserved — no
shipped flag changed, no DSP behaviour changed, no platform policy reversed. Worklog:
`worklogs/performance/PERF_AUDIT_PLATFORM_COVERAGE.md` §"The R-round". **The R-round left the
decisions alone; the round after it did not — ADR-0032 accepted the MSVC evidence and adopted the
flag. Where the paragraphs below describe an R-round state that ADR-0032 changed, the supersession
is marked in place.**

**The Windows instrument now exists** (R-1): the ~~reporting-only~~ `windows-avx2-ab` job A/B/Cs the
twin dump against `/arch:AVX2` and `/arch:AVX2 /fp:contract`, records the MSVC toolset against the
14.30 contraction boundary, and ~~gates nothing~~ — it converts ADR-0031 option 5's "could not be
demonstrated" into data, while adoption still requires its own ADR. **[Superseded — ADR-0032,
Accepted 2026-08-30: adoption got its ADR, and the job is now a BLOCKING gate — `continue-on-error`
removed, a flag-liveness self-proof and a hard 32-scenario requirement added so it fails closed.
The A vs B result it produced, 0/32 on toolset 14.51.36231 across two runs, is the evidence the ADR
rests on. Note the contrast with `macos-crossslice` above, which remains reporting-only by design
and gained job-level `continue-on-error` so that an infrastructure failure in a reporting experiment
cannot block a release.]** **The scope is now recorded**
(R-3): `COMPATIBILITY_MATRIX` carries Not Supported rows for Linux arm64 and Windows arm64 and a
toolchain-validation section placing clang-cl outside ADR-0031's demonstrated set; the
`CMakeLists.txt` scope comment agrees. **The F-1/F-2 comment defects are corrected** (R-4): the
three A7-9 blocks carry the three-terminal-state architecture note, and Test 41's no-FTZ paragraph
now records its own prior error and the measured subnormal stall. **The UCRT FMA3 caveat is in the
policy** (R-5), scoping Windows bit-identity to the machine class. **The Rosetta probe was found to
be testing AVX1, not AVX2** — the review finding was re-verified rather than assumed addressed, and
was real; both probes now execute `vpaddd ymm`. **The Clang-22 measurement gap is closed** (R-6):
GCC-13 at the frozen baseline reproduced the historical figure to 0.06 % (1705.9 vs 1704.9
Ir/sample), the AVX2 win holds on the shipped toolchain (−19.6 % on clang-22.1.8 vs −18.4 % on
GCC 13.3 on the same tree), and the shipped Clang binary executes −27.6 % fewer instructions than
the GCC binaries every historical figure came from — deltas transfer, absolutes do not. Found in
passing and diagnosed to a minimal repro (**F-8**): GCC 13 emits value-exact FMA in its vectorized
unsigned-int32→float lowering even under `-ffp-contract=off` — instruction selection, not
contraction; dead code in shipped binaries; recorded so a future nonzero FMA census is diagnosed
rather than read as a broken pin.

## Consistency pass + the MSVC adoption packet (decision boundary held)

The platform-coverage worklog and its report page carried audit-time statements the R-round had
overtaken — "no bit-exactness instrument on Windows", "nothing was ever measured on Windows", the
Clang-22 gap described as open, the vectorizer question as unanswered, R-1…R-6 as pending. Each is
now struck-and-marked **[Superseded — R-round]** in place rather than silently rewritten, a status
banner at the top separates the three registers (current evidence · current adopted policy · open
maintainer decisions), and the §9/§6 recommendation tables carry executed statuses. The historical
record is preserved; nothing current contradicts it. The corrected `windows-avx2-ab` parser's own
run is recorded as replicating the verdict: **A vs B "IDENTICAL (all 32 scenarios agree)"** on
toolset 14.51.36231.

**`worklogs/performance/MSVC_AVX2_ADOPTION_PACKET.md`** prepares the one open decision without
taking it: the verbatim draft ADR-0032, the exact `COMPATIBILITY_POLICY` / `COMPATIBILITY_MATRIX` /
KI-026 / user-guide edits (documentation ordered before the flag, per the ADR-0031 rule), the exact
`windows` job toolset ≥ 14.30 assertion, the exact promotion of `windows-avx2-ab` to a blocking
A == B gate, and the CMakeLists change that makes `ANAMORPH_X86_ISA_BASELINE` govern MSVC too. It
also writes the decline path, so rejecting costs one sentence. **Nothing in it is applied**: the
shipped Windows build still compiles at MSVC defaults, ADR-0032 is not registered, and the packet's
own header names `ADR_POLICY` and the Architecture Review Gate as the reason it stops there.

## ADR-0032: the MSVC `/arch:AVX2` adoption, applied on explicit approval

The maintainer approved the prepared packet verbatim, and the whole chain landed in one change —
documentation and flag together, per the ADR-0031 ordering rule. **ADR-0032** is registered
Accepted (amending ADR-0031's option-5 deferral, which is cross-linked as superseded); the Windows
build compiles **`/arch:AVX2` at the default non-contracting `/fp:precise`** with no `/fp` flag
added and no runtime dispatch; `ANAMORPH_X86_ISA_BASELINE` now governs MSVC too (OFF drops the
flag — the gate's baseline half uses exactly that). Floor documentation: `COMPATIBILITY_POLICY`'s
Windows row rewritten to the same-floor state, `COMPATIBILITY_MATRIX`'s platform row / floor
section / toolchain-validation section updated (clang-cl stays outside the validated set — it would
now structurally *inherit* the flag, which the matrix records as one more reason it needs its own
ADR), KI-026 widened to Windows, both user guides' Windows carve-outs removed, and a CHANGELOG
entry that quotes no Windows performance figure because none exists.

**Two CI semantics changed, in opposite directions and for stated reasons.** `windows-avx2-ab` is
now a **blocking gate**: baseline (`ANAMORPH_X86_ISA_BASELINE=OFF`) vs the shipped configuration
must agree on all 32 scenarios, with **"NUMERICAL:"** and **"INFRASTRUCTURE:"** failures distinctly
labelled — both fail the job, fail-closed, because no other job verifies the property; the gate
also self-proves the flag is live (present in B's project files, absent from A's) so 0/32 can never
be vacuously true, and the `/fp:contract` build is informational only, its failures degrading to
warnings. The `windows` job's toolset step asserts **≥ 14.30** (the VS2022 contraction-default
boundary) and now **fails on an unreadable version** where it used to warn. In the opposite
direction, `macos-crossslice` — reporting-only, gating nothing — carries **job-level
`continue-on-error`**, closing the review finding that a checkout failure or timeout in a
reporting experiment could block a release through `release.yml`'s aggregate result: its failures
stay visible on the job, they just no longer propagate. The platform-coverage worklog, report page
and adoption packet are updated to the adopted state, with the superseded audit-time statements
marked rather than erased.

## The A7-9 near-silent scope correction + the cross-slice record parser (2026-08-30)

A review pass against `src/dsp/VelvetNoise.cpp:158` asked what happens to **near-silent NONZERO**
input at the stalled fixpoints, and the measured answer corrected a claim every A7-9 record carried:
"the residual appears only on digital silence" was this programme's *observation*, never a property.
The absorption `x + residual == x` needs `|x| >= 2^24 × |residual|`; a twin-binary A/B against the
actual pre-A7-9 sources (`c04096d^`, same flags, FTZ as shipped) measured tails at 1e-25…1e-37 of
full scale with warm loud history differing by up to **1.204e-35** (Chorus, 192 kHz) inside each
module's delay-history window — and every tail at 1e-20 and above bit-exact, which is the boundary
the old "real signal" claim silently rested on. Under no-FTZ the window shrinks to ~6e-36 with
subnormal-scale deltas, per F-1's stall model. Verdict: **expected behaviour, Class B stands, the
scope wording was wrong** — corrected in the three DSP comment blocks, Test 41's comment,
`CHANGELOG` `[0.9.5]`, `HANDOVER`, `PERFORMANCE_BUDGET`, `TESTING.md` (whose Test 41 paragraph was
also still carrying the F-2-corrected no-FTZ falsehood; fixed against measurement), and dated
markers on the three historical worklogs and the decision packet. **Test 42**
(`testA79ParkedNearSilentIdentity`, +12 checks → **41 tests / 226 checks**) pins the accepted side —
parked paths are bit-exact identity on 1e-30/1e-35 tails — and is proven to fire against the
pre-fix sources in both FTZ postures (the two-amplitude design exists because the discriminating
window is posture-dependent). No DSP code changed. Record:
`worklogs/performance/PERF_AUDIT_A7-9_NEARSILENT_SCOPE.md`.

The same review flagged the `macos-crossslice` job's counting: `grep -c .` counted every nonblank
line — the dump's self-check verdict and `#` headers included — so a wording change could move the
reported totals with no DSP change. The `compare` function now parses **scenario records** (a row
whose second field is a 16-hex FNV hash — the `windows-avx2-ab` `Read-Dump` acceptance rule),
derives the expected count from what parses (both slices must yield the same nonzero count or it
prints **NO VERDICT** instead of a bogus total), and compares the full records exactly (name, hash,
latency). Verified against real dump output: a reworded self-check line no longer changes the
count; a single altered hash reports 1-of-32 with the agree list intact; an unparsable side yields
no verdict. Reporting-only semantics unchanged — every path still exits 0.

## ADR-0033: LLVM 23 evaluated and NOT adopted; "stable" becomes an assertion (2026-08-30)

ADR-0028's revisit trigger fired — LLVM **23.1.0** released 2026-08-25 — and the move to 23 was
prepared, measured, and then **stopped on availability**. `ANAMORPH_CLANG_VERSION` stays **22**.
ADR-0033 supersedes nothing; it **amends** ADR-0028 with the assertion that closes the gap the attempt
exposed, and ADR-0028 and ADR-0030 keep their values and their `Accepted` status unchanged.

**The gap, stated as a question the old rule could not ask.** ADR-0028 enforced *upstream stable* by
checking the **major**, because nothing then suggested the two could come apart. They can:
apt.llvm.org publishes rolling **branch** builds, so a `-N` suite serves whatever commit of
`release/N.x` it last built, under a version string that already reads like N's release. *"A release
exists"* and *"this mirror serves it"* are different questions, and only the second decides what
compiles the shipped bytes.

**The measurement.** `llvmorg-23.1.0^{}` is `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4`, committed
2026-08-25T19:15:21Z. Every apt.llvm.org `-23` suite — noble, resolute, bookworm and trixie, all
indexed 2026-08-18 — was built from the same commit `55feb0a3b6b7` (noble's package is
`1:23.1.0~++20260818083557+55feb0a3b6b7`; the others differ only in build timestamp), a commit that is an
**ancestor of the tag by 49 commits**. No Ubuntu series publishes `clang-23` at all (Launchpad: 22
reaches `1:22.1.6-1ubuntu2` in stonking; 23 is unpublished; noble stops at `clang-20`). Upstream's own
release binaries could not be checked — GitHub non-git HTTP returns 403 through this environment's
proxy — so their availability is recorded as **unevaluated**, not assumed.

**The control is what made it decisive.** noble-22 serves
`1:22.1.8~++20260714014902+ca7933e47d3a`, and `ca7933e47d3a` **is** `llvmorg-22.1.8^{}`. The 22 pin has
been a build of the release **commit** all along. So the withdrawn draft would have traded a
release-commit build for a pre-release branch build while believing it was moving stable → stable.

**What landed instead.** `scripts/setup-llvm-apt.sh` carries an upstream **release identity** per
major — the full version plus that release's `llvmorg-<version>` tag commit — and asserts both before
the job proceeds: an **unrecorded major exits 2** (the property that would have caught the draft: a pin
bump to a major nobody verified now stops before installing anything), and a **build commit that is not
the tag's exits 1**, naming both. The commit is read from **four** sources — `clang --version` and the
installed version of each of the three packages — every source that carries one must agree, and if
**none** carries one the install fails closed. That last rule is a correction inside the same round:
the first implementation accepted a missing commit as "release-versioned by construction", which was a
bypass rather than a lenience, and it is what a compiler with no `+<sha>` would have walked through.
A `--self-test` drives the decision function with recorded strings and runs in `source-lint` and
`preflight.sh`. The 23 line is
deliberately absent, and says so at the point of absence. Proven in all three directions against the
real packages: 22 passes printing the matching commit; an unrecorded major exits 2; 23 with its true
identity recorded exits 1 naming `55feb0a3b6b7` against `ea7d852a`.

**Two documents were corrected by this round rather than by opinion.** ADR-0028's reproducibility
bullet said apt.llvm.org publishes *"branch snapshots, never the tagged `llvmorg-*` build"* — wrong,
and wrong in this project's favour, since `ca7933e47d3a` is the tag. It is left in place with a dated
correction note, because that belief is precisely what made the reverse error possible on `-23`. And
`DEPENDENCY_POLICY.md` §Update mechanisms, whose trigger was already corrected from
`CURRENT_LLVM_STABLE` (it lagged the release by five days) to upstream's release page, now also carries
the second question: a release existing is not a release being installable.

**Rules 2–3 have nothing to act on.** No shipping toolchain moved, so no twin dump, no Level-5 audition
and no compatibility re-verification are owed — ADR-0028's position, reached for a different reason.
The evidence gathered while 23 was on the table (32/32 twin-dump scenarios bit-identical, a
`diff`-identical first-party warning census, 226+920 and 224+920 checks green, an unmoved ABI floor,
and the RTSan/libFuzzer/lld/function-effects detectors all live on 23) is kept in ADR-0033 because it
is where the next attempt starts — and it is explicitly **not** a licence to adopt, having been taken
against the pre-release build the release is 49 commits ahead of.

**The condition that lifts the block is single and checkable:** an apt.llvm.org `-23` suite rebuilt
from `ea7d852a`, or from a later 23.x release tag. Then the identity line is added, the pin moves, and
the assertion proves the move instead of the reader trusting it.

## The `DELIBERATE_REAIMS` lifecycle: a declaration is a transition, not an anchor (2026-08-30)

An INVESTIGATE finding on `scripts/check-citations.py` — *"these entries only cover intermediate
pushes; after merge, later anchor movement can activate them and silence genuine drift"* —
**reproduced, including the silent case.**

**What the lifecycle actually was.** The table was keyed `(document, anchor)` and
`is_declared_reaim(doc, base, cur)` returned true when **either** spelling was declared, guarded only
by a refusal when `base == cur`. That guard retires an entry for the *no-movement* case, which is
what its comment claimed and what the procedure docs repeated. It does nothing for the case that
matters: once the base revision carries the declared spelling, every later movement of that anchor
arrives as `declared → something new`, matches through the **base** arm, and is excused by a
declaration written for a transition that merged weeks earlier.

**Reproduced against the live table**, not a model:

```
is_declared_reaim(D, A, B)  -> True    # the declared transition
is_declared_reaim(D, B, B)  -> False   # after merge, nothing moved
is_declared_reaim(D, B, C)  -> True    # a LATER, undeclared movement — the finding
is_declared_reaim(D, C, B)  -> True    # and the reverse
```

**And the second gate did not catch it.** `verify_reaim_targets` resolves the declared anchor and
requires its token, which usually turns the stale case red — but on `docs/REPOSITORY_MAP.md`'s
`CMakeLists.txt:114-384`, a 270-line span, the token stays inside the span through almost any edit.
So `114-384 → 120-390` was excused **and** the aim-check passed: real drift suppressed with no output
at all. That is the case that makes this a defect rather than a message-quality problem.

**The fix is the key.** `DELIBERATE_REAIMS` is now `(document, BASE anchor, CURRENT anchor) → expected
token`. A declaration authorises exactly one movement and stops matching the moment either end
differs — the brief's *"expire naturally once its declared transition is no longer the transition
being verified"*, expressed as the lookup itself rather than as a second rule bolted beside it. The
`base == current` refusal is kept as a belt-and-braces check on a malformed key. `verify_reaim_targets`
resolves the **current** end (the base end names a revision this tree is not, so resolving it would
fail every correct declaration), and the self-test now asserts which end is resolved. Two
simplifications fall out: `used_reaims` holds the same 3-tuple the table is keyed on, so the
"record both spellings" workaround and the reason the report had to intersect both disappear.

**The table is now empty, and that is a measurement rather than a cleanup.** It held 40 entries;
emptying it left `--check` green against **every** base this repository compares — `origin/main`, the
branch merge base (the same commit here) and `HEAD~1`, which is what CI passes as
`github.event.before`. 37 were reported by the tool itself as *"not needed against origin/main, which
already carries the re-aimed spelling"* against a base that IS the merge base, which is precisely the
retirement condition it documents; the other 3 were written earlier in this branch and re-anchored out
of existence by `--fix` in the commit that created them. None was load-bearing. They are retired
rather than converted because converting would have meant inventing base spellings for transitions
that merged weeks ago.

**Regression coverage, in the self-test that runs in `source-lint`.** Section 5 now drives the real
`is_declared_reaim` through the declared transition (accepted), a later `B → C` (refused — the
finding), the reverse `C → B` (refused — the hole a "current spelling only" key would have left), an
unrelated anchor in the same document (still checked), the post-merge no-movement case, a declaration
for a *different* transition (does not excuse this one), an undeclared re-spelling, and another
document's declaration. Section 8c gained a case asserting that `verify_reaim_targets` resolves the
current end and not the base end. `--self-test` passes 138 cases; the drop from 174 is the 40 retired
entries' per-entry liveness checks.

**One current-state claim was wrong in the docs and is corrected:** `CI_CD.md` stated that because
`is_declared_reaim` returns false when the spelling is unchanged, *"an entry cannot outlive its
transition"*. That is the belief the finding disproved, and it is now recorded as such beside the
rule that makes it true.

---

## Engineering-review programme, round 1 (2026-08-31, the 0.9.6 change set)

**What the round is.** The first pass of the standing engineering-review programme: a broad,
adversarially-verified sweep of the whole repository (8 lenses; 17 findings confirmed, 2 refuted,
~75 areas ruled out), a fix-now set implemented with regression coverage, two gate-blocked defects
FILED instead of fixed, and a persistent programme record created. Records:
`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` (the worklog of record) with
`ENGINEERING_REVIEW_REPORT.html` beside it (the live dashboard — a VIEW of the worklog, updated
and re-committed whenever the programme state materially changes; same companion rule as the
performance reports).

**Code changes and their doc syncs (trigger map applied):**
- `src/dsp/AnamorphEngine.cpp` (chunk guard ER-DSP-01, prepare-snap ER-DSP-02),
  `src/dsp/Correlation.h` (sanitize ER-DSP-04) → CHANGELOG `[0.9.6]` (three Fixed entries),
  Tests 43–45, HANDOVER Test/Version rows. No stage-order, latency-value, parameter or
  serialization change — no ADR needed (behaviour changes only in the defective windows; the
  Mix-0 null and identity invariants are what the fixes RESTORE).
- `src/PluginProcessor.cpp` (absent-PARAM defaults ER-STATE-01, slot type guard ER-STATE-02,
  the KI-027 comment correction) → `SERIALIZATION_REGISTRY.md` (two dated annotations; the
  serialized SCHEMA is untouched — restore semantics for absent/corrupt payloads now match the
  registry's own recorded rules), two state-suite regressions, CHANGELOG (two Fixed entries).
- `src/gui/LookAndFeel.cpp` (ValueBox gesture ER-GUI-01) → KI-010 dated correction, CHANGELOG,
  USER_MANUAL left as-is (its wording is now accurate again).
- `tests/dsp_tests.cpp` (engaged matrices ER-TST-01 + Tests 43–46), `tests/state_tests.cpp`
  (two regressions) → TESTING.md counts, README:64, HANDOVER Test row, REPOSITORY_MAP test rows,
  RELEASE_HARDENING_PLAN QA row (45 tests / 241 checks; 15 tests / 924 checks).
- `scripts/check-realtime.py` (AUDIO_FN seeds ER-RT-02) → REALTIME_AUDIO_POLICY scoping sentence
  (the "anywhere in the chain" claim now states the annotated-entry boundary and the lint/guard
  complements).
- `scripts/run-pluginval.ps1` + `run-pluginval.sh` comment (ER-CI-01) → TESTING.md §Signal-only
  retry (Windows paragraph), FUTURE_RISKS RISK-004 (Windows analog closed).
- `.github/workflows/build.yml` one-line pin comment (ER-DEP-05).

**Registry filings (defects found, fixes gated):** KI-027 (audio-thread latency re-report,
ER-RT-01 — table row + full entry; the LATENCY_MODEL/THREAD_MODEL drift is recorded IN the entry
per C6 and deliberately left in the two architecture docs for the gated fix to reconcile) and
RISK-007 (off-main-thread state calls, ER-RT-03/ER-STATE-05 — plus THREADING_POLICY's new §Host
state calls, and a dated KNOWN-VIOLATION cross-reference under its PDC rule).

**Version renumbering (ER-DOC-01) and the 0.9.6 bump:** `CMakeLists.txt:14` → 0.9.6; CHANGELOG
`[0.9.6] — 2026-08-31` (six Fixed entries, evidence PR #134); the forward-looking "release in
preparation / first tag" claims renumbered in HANDOVER (:76 and the Branch/Release/Blockers rows),
CHANGELOG:9, CHANGELOG_POLICY:12, RELEASE_PROCESS (§Tagging commands + :112), COMMERCIAL_STATUS
(:11-12/:30/:115-118), FUTURE_RISKS (RISK-003), RELEASE_HARDENING_PLAN (:29/:46/:300/:346 + the QA
row counts). The **Level-5 precondition is restated OPEN** wherever it was asserted "against the
build that ships" (HANDOVER:91, COMMERCIAL_STATUS §5) — the 2026-08-15 audition covered v0.9.4,
the shipping build is now v0.9.6, and no carry-forward was ever decided; this is maintainer
decision D-3, not something a doc edit can clear. HANDOVER's open-KI enumeration completed
(KI-018–KI-023, KI-026 were missing). Historical v0.9.4 narrative (HANDOVER:49/:89/:94/:102,
worklog filenames, FUTURE_RISKS prior-sync chain) deliberately untouched — Class B.

**Shipped-attribution corrections:** NOTICE:24 pin line 9.0.0/f8f8864 → 9.0.1/e18f7f5
(ER-DOC-02; NOTICE added to DEPENDENCY_POLICY's bump re-verification list so the next bump cannot
miss it) and a new NOTICE AudioUnitSDK Apache-2.0 section (ER-DEP-01; © 2000-2021 Apple Inc.,
read from the pinned tree's `AudioUnitSDK/LICENSE.txt` and the `@copyright` header;
THIRD_PARTY_LICENSES §mandatory-notices names it beside SheenBidi).

**Other Class-A drift corrected:** CI_CD.md §Build matrix (nine non-packaging jobs; rows for
`macos-crossslice` reporting-only and `windows-avx2-ab` blocking; the release-blocking paragraph's
carve-out), REPOSITORY_MAP build.yml row (same two jobs) and its worklogs entry (the "ONE worklog
has a rendered companion" sentence — three existed before this round, four now; corrected with a
dated note), REPOSITORY_MAP test-count rows (37→45 DSP, 13→15 state), TESTING.md:18 (41→45),
TESTING.md's twin-dump bullet ("session-local and not committed" — false since 2026-08-18,
ER-TST-05), a new TESTING.md §Gaps entry writing down the twin-dump's coverage boundary
(ER-TST-02) with the KI-026 status line now carrying the scope qualifier, FUTURE_RISKS' missing
0.9.5 sync note (ER-DOC-04, corrected inside the new 0.9.6 sync note), and this file's own stale
"Last updated" header (ER-DOC-05, corrected in place at the top).

**New documents:** `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` +
`ENGINEERING_REVIEW_REPORT.html`. Registered per the documentation-only trigger: REPOSITORY_MAP's
`worklogs/` entry describes companions generically and now names this one; `SOURCE_OF_TRUTH.md`
and README §Documentation class tables are unchanged because worklogs are already a classified
directory there (the KEYNOTE_SCRIPT precedent at :5289 applies); self-coverage is this entry.

**Refuted findings recorded (not drift, prior art):** ER-DSP-03 (the duck's block-boundary dwell
is the documented [0.8.10] behaviour) and ER-TST-03 (JUCE state chunks are plain XML, not
deflate — the fuzzer reaches the parser). [Verified]

---

## Engineering-review programme, round 2 (2026-08-31, still the 0.9.6 change set)

**What the round is.** CI recovery plus the carried round-1 roadmap: two warning-gate blockers
fixed at source, three confirmed defects fixed with regression coverage, RISK-007 measured rather
than reasoned about, decision D-1 materially corrected, and one new issue filed. Records:
`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 2 with the dashboard
`ENGINEERING_REVIEW_REPORT.html` beside it, both updated in this change set.

**Code changes and their doc syncs (trigger map applied):**
- `src/dsp/AnamorphEngine.h` (`primeParameters`) + `src/PluginProcessor.cpp` (`prepareToPlay`
  ordering) → CHANGELOG `[0.9.6]` (two Fixed entries: the activation duck and the restored-level
  ramp), State test 16, HANDOVER Version + Test rows, and the count sweep below. No stage-order,
  latency-value, parameter or serialization change — the fix RESTORES the intended settled state,
  so no ADR is triggered.
- `src/PluginProcessor.cpp` (`reassertParameters`: non-finite guard + the inverted write gate) and
  `src/PresetManager.cpp` (`applySoundTree` fallback) → CHANGELOG (one Fixed entry), State test 17.
  The serialized SCHEMA is untouched: this changes what a MALFORMED value restores to, which
  `SERIALIZATION_REGISTRY.md` already specifies as "per-parameter defaults" for the absent case.
- **Round 4 (2026-09-01).** `src/PluginProcessor.{h,cpp}` (D-1: `requestLatencyUpdate`, the
  processor-owned 20 Hz timer, and the restore-time re-report), `src/gui/PhysicalMouseButtons.{h,cpp}`
  + `_mac.mm` and `src/PluginEditor.{h,cpp}` (the macOS physical-button query feeding both editor
  predicates), `CMakeLists.txt` (the Apple-only `.mm`) -> CHANGELOG (three Fixed entries),
  KNOWN_ISSUES (**KI-028 and KI-013 both RESOLVED**), `docs/architecture/LATENCY_MODEL.md` (the
  delivery-thread rule and the up-to-50 ms deferral, plus the restore re-report),
  `scripts/check-gcc-warnings.py` (ER-CI-04 gcc-16 measurement, closing the round-3 gap), README /
  TESTING / TESTING_POLICY / REPOSITORY_MAP / RELEASE_HARDENING_PLAN / HANDOVER counts (47 / 245;
  24 tests / 1057 checks), and State tests 22-24. D-4's Architecture-Review gate is **APPROVED**, so
  the round-3 malformed-value and repair-serialization changes are cleared to merge; no schema field
  was added, removed or renamed and no well-formed file loads differently. D-2 is DEFERRED with no
  code change; D-3 remains a release blocker.
- **Round 3 (2026-09-01).** `src/SerializedNumber.h` (new shared malformed-value predicate),
  `src/PresetManager.cpp` + `src/PluginProcessor.cpp` (both restore paths call it; the repair now
  reaches the live tree), `src/dsp/AnamorphEngine.h` (`primeParameters` clears `duckRequest`),
  `src/gui/LookAndFeel.h` + `.cpp` and `src/PluginEditor.h` + `.cpp` (`DragGestureOwner` and the
  abandoned-gesture sweep) -> CHANGELOG (four Fixed entries), KNOWN_ISSUES (KI-028 narrowed to a
  macOS residual, with the refuted editor-teardown path recorded), `scripts/check-gcc-warnings.py`
  (ER-CI-04 re-measurement 13.3.0-15.2.0 plus the gcc-16 command), `scripts/check-realtime.py`
  (ER-RT-05 cross-file census), README / TESTING / TESTING_POLICY / REPOSITORY_MAP /
  RELEASE_HARDENING_PLAN / HANDOVER counts (47 / 245; 21 tests / 1040 checks), and Tests 48 and
  State tests 19-21. **GATED:** the malformed-value and repair-serialization changes alter
  malformed-value recovery and saved-state contents, so they need ARCHITECTURE_REVIEW_GATE approval
  before merge (worklog D-4). No schema field is added, removed or renamed.
- `tests/state_tests.cpp` (State tests 16, 17 and 18, State test 4's de-vacuumed InternalState
  assertions, and the `--state-thread-probe` + `--latency-restore-probe` instruments) →
  `docs/procedures/TESTING.md` (both probes documented beside the state-suite description; test count
  15 → 16 → 17 → 18), REPOSITORY_MAP test rows, README:64, TESTING_POLICY, RELEASE_HARDENING_PLAN QA
  row, HANDOVER Test row (45 / 241; 18 tests / 941 checks). Neither probe is run by the suite; the
  latency one asserts nothing at all, because the reported latency is a gate category.
- `src/PluginProcessor.cpp` (v0.2 branch of `setStateInformation` now calls
  `internal.migrateFromLegacyApvts`, ER-STATE-08) → CHANGELOG (one Fixed entry, replacing round 1's
  absent-PARAM entry — see below), State test 4. No schema change: this reaches an EXISTING
  migration from a branch that was skipping it.
- `src/PresetManager.cpp` (`applySoundTree` keys presence off `hasProperty("value")`, ER-STATE-06) →
  CHANGELOG (one Fixed entry), State test 18. The serialized schema is untouched; this changes what
  a value-less node restores to, which `SERIALIZATION_REGISTRY.md` already specifies as the
  per-parameter default for the absent case.
- **Correction (ER-STATE-07 / ER-STATE-01, 2026-08-31).** Round 1 recorded that
  `reassertParameters`' default branch is what makes absent PARAM nodes reset on a reused live
  instance. Measurement (`--latency-restore-probe` step 0b) refuted that: `apvts.replaceState`
  already does it, with host notification. Corrected in `src/PluginProcessor.cpp` (two comments),
  `docs/architecture/SERIALIZATION_REGISTRY.md` (the `<ANAMORPH>` row), CHANGELOG (the user-facing
  entry replaced by ER-STATE-08's, which is the real instance of that leak class),
  `tests/state_tests.cpp` (the State test 4 comment; the assertion itself stands — it pins the
  contract regardless of which layer satisfies it) and the programme worklog. The shipped code is
  kept as a backstop.
- `tests/dsp_tests.cpp` (`juce::exactlyEqual` at four sites; two engine pairs moved to the heap) and
  `src/PluginProcessor.cpp` (lambda parameter renamed) → no doc trigger: neither changes behaviour,
  and CHANGELOG_POLICY rule 3 excludes them.
- `scripts/check-gcc-warnings.py`, `.github/workflows/{build,codeql,release}.yml` → the comment and
  diagnostic corrections listed below; `GATED_FLAGS` and every gate's behaviour are unchanged except
  release.yml's tag check, which now distinguishes an infrastructure failure from a verdict (both
  still exit 1).

**Registry changes:** **KI-028** filed (the value-box gesture leaked by a lost release — a residual
of round 1's own fix, with both candidate designs recorded); **KI-027 corrected on four points**
after re-verification (the expensive branch needs oversampling selected by hand, since it is not a
host parameter and defaults to Off; the rate is bounded at one dispatch per parameter per block;
`PTHREAD_PRIO_INHERIT` mitigates the inversion on POSIX but not Windows; and two of D-1's candidate
fixes are refuted); **RISK-007** gains the TSan measurement (four named races) and the D-2 pointer.
`KNOWN_ISSUES.md`'s version-sync header now covers both rounds.

**Class-A drift corrected, each verified before editing:** the `reassertParameters` rationale
comment (ER-STATE-04 — `replaceState` does propagate, so the comment now states the real residual
the function exists for, preserving its necessity); one `PluginEditor.cpp` comment clause that
over-claimed the 0.9.3 half-typed-value guarantee (ER-GUI-02 — narrowed, code deliberately NOT
widened, and the published documents were already correct); `build.yml`'s header block, which
described the pre-2026-08-15 macOS ordering and contradicted both its own in-job comment and
`CI_CD.md` (ER-CI-02 — rewritten line-count-neutral so no citation moved); `codeql.yml`'s toolchain
claim and its "src/ + tests/" coverage claim (ER-CI-03 / ER-CI-06 — comments corrected, the compiler
deliberately not pinned since that would be a Build System change for no analysis benefit);
`check-gcc-warnings.py`'s "this job's pinned pair" label (ER-CI-04 — scoped to the compiler it was
measured on, with re-measurement a round-3 item); `release.yml`'s annotated-tag diagnostic
(ER-CI-05). Round 1's own batching rationale for the CI items is also corrected in the worklog: only
`build.yml` is citation-tracked of the four files.

**Citation re-anchoring.** `prepareToPlay` and the `<cmath>` includes shifted every anchor below
them in `PluginProcessor.cpp`; 51 citations were re-anchored mechanically by `--fix` and five were
re-derived BY SYMBOL because the cited lines themselves changed — `setStateInformation`,
`getStateInformation`, `readSlot`'s legacy-key fallback, and ADR-0010's two sites. Each is declared
in `DELIBERATE_REAIMS` as a transition, and three carry a second entry for the push-predecessor base
(a different base than `origin/main`, and the one CI compares). The self-test's liveness check
caught one bogus declaration — a bare `:line` shorthand the gate does not track as an anchor — which
was removed rather than worked around. [Verified]


## Engineering-review programme, rounds 5 and 6 — the Level-5 audition specified, then recorded (2026-09-01, still the 0.9.6 change set) — ENTERED LATE

**Recorded retroactively in round 7.** Rounds 5 (`ba34d64`) and 6 (`9063b49`) both changed
documentation and neither updated this file, which is the sync
`DOCUMENTATION_LIFECYCLE_POLICY.md` requires. The gap is stated rather than back-dated: the
entry below is written in round 7 from those two commits' diffs, and the rounds themselves are
recorded in `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 5 / §Round 6.

**Round 5 — release finalisation.** NEW document `docs/procedures/LEVEL5_AUDITION.md`: the Level-5
manual audition had never been written down, so its scope lived in whoever remembered the release.
It now exists as a twelve-item protocol in five groups, **derived from the CHANGELOG `[0.9.6]`
entries** rather than invented, with an invalidation rule and a record format. Synced with it:
`docs/procedures/RELEASE_PROCESS.md` and `docs/REPOSITORY_MAP.md` (the new procedure listed),
`docs/HANDOVER.md` and `CHANGELOG.md`. Two review items were dispositioned by measurement, not
argument, and both changed a document: `scripts/check-realtime.py` gained the cross-file lint
coverage boundary and `docs/policies/REALTIME_AUDIO_POLICY.md` +
`docs/architecture/REALTIME_SAFETY_AUDIT.md` record what the lint does and does not see;
`scripts/check-gcc-warnings.py` carries the AllocationGuard/gcc-16 result — `-Wmismatched-new-delete`
is a **static pairing diagnostic, not an allocation counter**, so `AllocationGuard.h`'s replaced
global operators are a false positive by construction. `docs/policies/TESTING_POLICY.md` gained rule
3a, the state-mutation stress-pattern rule.

**Round 6 — D-3 recorded.** The maintainer performed the audition against the final v0.9.6 build and
it **PASSED**, discharging `RELEASE_POLICY.md` precondition 7. Recorded in
`LEVEL5_AUDITION.md` §Recorded auditions, and synced to `docs/HANDOVER.md` (Release Status, Known
Blockers, Roadmap), `docs/COMMERCIAL_STATUS.md` §6 and
`docs/architecture/RELEASE_HARDENING_PLAN.md` (QA-gate row). **Seven fields of that record —
audition date, DAW, OS/architecture, plugin format, session, per-item outcomes for groups A–E, and
the exact artifact identity — are marked NOT RECORDED and were left blank deliberately**, because
the protocol says what *should* be exercised, not what *was*. The correspondence between what was
auditioned and the final build rests on the maintainer's attestation, no artifact identity having
been supplied; that basis is written down rather than glossed. [Verified]

## Engineering-review programme, round 7 — the compatibility checklist completed to 6/8 (2026-09-01, still the 0.9.6 change set)

**What the round is.** `RELEASE_POLICY.md` precondition 2 — the hard compatibility gate — went from
never completed for this release to **six of eight boxes checked with measured evidence**. Records:
`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 7 and the dashboard beside it,
both updated in this change set.

**No `src/` change.** The round adds one test and one fixture pair and syncs documents; no
ARCHITECTURE_REVIEW_GATE category is touched, and `CHANGELOG_POLICY.md` rule 3 excludes both a test
addition and a checklist completion from the CHANGELOG.

**Changes and their doc syncs (trigger map applied):**
- `tests/state_tests.cpp` (State test 25, the cross-version field capture) and
  `tests/fixtures/field_capture_v0_9_5.session` + `.manifest` (NEW) →
  `docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md` (item 8 ticked, its "reconstruction, not
  field capture" caveat closed, evidence appendix), `docs/REPOSITORY_MAP.md` (the state-suite row
  and the `tests/fixtures/` row, which now distinguishes the one fixture that is **not** a
  reconstruction), and the count sweep below. The fixture was WRITTEN by a v0.9.5 binary rebuilt
  from the tree at `2c5e760^` — the three legacy XMLs are built by current code and can only contain
  what today's understanding says an old format held; this one cannot.
- `docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md` (§Completion record, per-item PASS/OPEN
  prefixes, §Evidence for the v0.9.6 completion) → `docs/HANDOVER.md` (Release Status, Known
  Blockers and Roadmap rows: "three preconditions" → two, and "has never been completed" → the 6/8
  position), `docs/COMMERCIAL_STATUS.md` §6 (same correction; it had been the last document still
  asserting the checklist was uncompleted).

**Counts corrected, measured not inferred.** The state suite is **25 tests / 1077 checks** and the
DSP suite **47 tests + the A/B clamp guard / 245 checks**. `docs/policies/TESTING_POLICY.md` was the
worst drifted — its Level-2 row still read *41 DSP tests* and *15 state-compatibility tests*, and its
hard-release-gate paragraph *41* and *24-test*, so the policy that defines the gate understated the
gate by ten tests. Also corrected: `README.md:64`, `docs/procedures/TESTING.md` (state-suite
description), `docs/REPOSITORY_MAP.md`, `docs/architecture/RELEASE_HARDENING_PLAN.md` (QA-gate row)
and `docs/HANDOVER.md` (Test Status row). Two stale release-mechanics facts in the HANDOVER Release
Status row were corrected while there, both verified first: the tag-order sentence still named
`git tag -a v0.9.4` where the same row already says the tag is `v0.9.6`, and it still cited the
`[0.9.4]`-era re-dating deadline of 2026-08-19 where the CHANGELOG `[0.9.6]` heading is dated
2026-09-01.

**What was deliberately NOT recorded.** Checklist items 5 (Host matrix) and 7 (Automation playback)
stay **OPEN**. The Level-5 audition exercised a DAW and its protocol group C covers automation, but
that record's per-item outcomes are marked NOT RECORDED — so ticking either item from it would infer
item-level results from a verdict-level record. Round 6 refused to fill those rows in from the
protocol; reading them back out here would be the same fabrication one step removed. [Verified]


## Engineering-review programme, round 8 — the legacy A/B contamination fix (2026-09-01, still the 0.9.6 change set)

**What the round is.** A supplied review's three actionable items, dispositioned by execution:
one CONFIRMED defect fixed, one obsolete comment rewritten, one enforcement boundary left alone.
Records: `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 8 and the dashboard.

**Code changes and their doc syncs (trigger map applied):**
- `src/PluginProcessor.{h,cpp}` (`abResetToDefaults()`, called from the v0.2 branch and from the
  new `AB`-absent branch of `setStateInformation`) → CHANGELOG `[0.9.6]` (one Fixed entry — it IS
  user-visible: pressing A or B after loading an old session recalled the previous project's
  sound), `docs/architecture/SERIALIZATION_REGISTRY.md` (a new paragraph under the `AB` child
  extending the existing "absent means the default" rule to a blob that carries no `AB` node at
  all), State test 26, and the count sweep below. **No ARCHITECTURE_REVIEW_GATE category is
  touched and no compatibility decision was required**: no schema field is added, removed or
  renamed; the semantics applied are the ones the registry's `AB` table already records (`active`
  → 0, slot params → "lazily initialised from current"); and every blob that carries an `AB` node
  restores exactly as before, which State test 26 leg 5 pins from the other direction.
- `src/PluginEditor.cpp` + `src/PluginEditor.h` (the obsolete "the caller's predicate is inert on
  macOS / narrows KI-028 to a macOS residual" scope comment) → no other document: `KNOWN_ISSUES.md`
  already carries KI-028 and KI-013 as RESOLVED, and the source was the only place still saying
  otherwise. Comment-only, no behaviour change, so CHANGELOG_POLICY rule 3 excludes it.
- `tests/state_tests.cpp` (State test 26 + the `--legacy-ab-probe` instrument) →
  `docs/procedures/TESTING.md` (the probe documented beside the two existing opt-in instruments,
  with the pre-fix numbers that are the evidence behind the test), `docs/REPOSITORY_MAP.md`, and
  the counts.

**Nothing changed for ER-RT-05.** The cross-file lint boundary is described accurately in all four
places that describe it (`REALTIME_AUDIO_POLICY.md`, `REALTIME_SAFETY_AUDIT.md`, `CI_CD.md`,
ADR-0029) — none implies whole-program or automatic cross-file coverage — so there was nothing to
correct. The policy's own instruction to re-measure the census before relying on it was followed:
**83 FORBIDDEN-class matches across 12 files**, identical to the round-3 record, with the three
cross-file-reachable DSP units unchanged and every match still inside that module's `prepare()`.

**Counts.** The state suite is **26 tests / 1096 checks** (was 25 / 1077); the DSP suite is
unchanged at 47 + the A/B clamp guard / 245. Swept through `docs/policies/TESTING_POLICY.md`,
`docs/procedures/TESTING.md`, `README.md`, `docs/REPOSITORY_MAP.md`,
`docs/architecture/RELEASE_HARDENING_PLAN.md` and `docs/HANDOVER.md`.

**Citation re-anchoring.** `abResetToDefaults` and the two new call sites shifted every anchor below
them in `PluginProcessor.cpp` and `PluginProcessor.h`; 33 citations were re-anchored by `--fix`
across the two bases, and **three were re-derived BY SYMBOL** because the cited lines themselves
moved past what they named — `legacyKey` (the pre-0.6.4 fallback inside `readSlot`),
`StateSet::isValid`, and `setStateInformation`, whose span is declared in `DELIBERATE_REAIMS` for
both the `origin/main` and push-predecessor bases. One over-correction was caught and undone: the
`readSlot` span was hand-derived when `--fix` could map it mechanically, which made the anchor drift
against its own base; the mechanical map is authoritative wherever it applies, and hand derivation is
for the anchors it reports unmappable. [Verified]


## Engineering-review programme, round 9 — the Level-Match residual refuted on impact (2026-09-01, still the 0.9.6 change set)

**What the round is.** A reported per-slot Level-Match contamination bug, investigated to a
disposition and **not fixed**, because its mechanism is real and its impact is not. Records:
`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 9 and the dashboard.

**No `src/` change, so no trigger fires from the code side.** The only tree change is the
`--legacy-match-probe` instrument in `tests/state_tests.cpp` and its description in
`docs/procedures/TESTING.md`. Suite counts are unchanged (**26 tests / 1096 checks**): the probe is
opt-in and the suite never runs it, so no count document needed touching. No CHANGELOG entry — no
user-visible behaviour changed (`CHANGELOG_POLICY.md` rule 3).

**What was established, and why it did not become a fix.** `abMatchGain[2]` is written and read only
in `abSwitchTo`, initialised in the header, and **never reset on any path and never serialized** —
so the `AB`-present restore leaves it as stale as the no-A/B one, and a fix scoped to
`abResetToDefaults` would close a third of the class while appearing complete. The injected figure
is measurably the previous project's, but the output level does not move under a matched
same-instance counterfactual (identical parameters across the switch, so the injection is the only
variable), a fresh-instance control, or the worst case of switching before the loudness module has
converged. The mechanism: the injection lands at the silent bottom of the switch duck and
`setParameters` re-targets from the live measurement every block. Level Match is a continuously
re-derived measurement, not stored state. The residual is a readout excursion of ≈65–85 ms.
Additionally, the value that *should* be written is undocumented — `abMatchGain` appears in no
registry, ADR or policy, unlike round 8's fix, whose semantics `SERIALIZATION_REGISTRY.md` already
specified — so choosing one would be legislating rather than conforming.

**ER-RT-05 re-verified, nothing changed.** Same boundary, same four accurate documents, census
re-measured at **83 matches across 12 files**, identical to rounds 3 and 8. Recorded with it: the
first re-measurement returned 205/29 because it ran the regexes over raw text instead of applying
the script's own `strip_comments_and_strings` — caught and re-run before being reported, since a
census is only comparable to one run the same way. [Verified]


## Engineering-review programme, round 10 — release notes reconciled with the KI-013 outcome (2026-09-01, still the 0.9.6 change set)

**What the round is.** Documentation only, no code. A reported contradiction between the `[0.9.6]`
release notes and KI-013's RESOLVED status was one of three sites still describing the pre-round-4
world. Records: `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 10.

**Corrected:**
- `CHANGELOG.md` — the *"Known limit: this reconcile is inert on macOS … so the macOS half of the
  issue remains open"* sentence, flatly contradicted by round 4 and by `KNOWN_ISSUES.md`'s own
  registry rows. Removed as part of the consolidation below.
- `docs/KNOWN_ISSUES.md`, KI-013 detail section — present-tense throughout while the registry row
  said RESOLVED, and its closing bullet offered as a hypothetical the fix that shipped (*"Fixable
  only via a JUCE-side change or a platform-specific `pressedMouseButtons` query"*). Given a dated
  RESOLVED banner, moved to past tense, closing bullet corrected to say round 4 did exactly that.
- `docs/KNOWN_ISSUES.md`, KI-028 detail section — the round-3 banner still read *"The macOS residual
  is why this entry stays open."* Given a `CLOSED 2026-09-01 (round 4)` banner.

Both KNOWN_ISSUES sections keep their diagnosis text under an explicit "everything below is the
round-2/3 record" banner rather than being rewritten, per the no-rewriting-history rule.

**Consolidated.** `[0.9.6]` carried two Fixed entries for one bug path — round 3's sweep (Linux and
Windows) and round 4's signal (macOS) — both inside one unreleased version, so no user ever saw the
intermediate state and presenting them separately implied macOS had shipped broken. Merged into one
entry describing the final state on all three platforms, preserving the half that *did* ship broken
(the stuck visual press on macOS, KI-013, present since v0.8.12) and both regression-test citations
(State tests 21 and 23). The Fixed count goes 19 → 18, which restores `docs/HANDOVER.md`'s
"eighteen Fixed entries" to accuracy — that line had gone stale by one when round 8 added its entry.
Counted, not assumed.

**Deliberately not changed**, being accurate history rather than live claims: `KNOWN_ISSUES.md:97`
(the dated v0.8.12 version-sync header recording KI-013 as *added* at that release), the dated round
entries in this file, and the round-3/4 sections of the programme worklog and older audit worklogs.
The third readout entry — the round-1 fix for the drag path opening no gesture at all — is a
different defect and was left alone.

**ER-RT-05 verified again, no change.** Third consecutive round. AUDIO_FN is a manual registry and
the script says so; `REALTIME_AUDIO_POLICY.md`, `REALTIME_SAFETY_AUDIT.md`, `CI_CD.md` and ADR-0029
all describe the same-file boundary correctly, and none claims automatic cross-file discovery.
Recorded as stable so a later round cites this rather than re-deriving it. [Verified]


## Engineering-review programme, round 11 — two restore fixes, one refutation, and a changelog audit (2026-09-01, still the 0.9.6 change set)

**What the round is.** Three reported restore/latency items plus a full staleness audit of the
`[0.9.6]` release notes. Records: `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md`
§Round 11 and the dashboard.

**Code changes and their doc syncs (trigger map applied):**
- `src/PluginProcessor.{h,cpp}` — `deliverLatency()` split out so the request flag is cleared exactly
  once per delivery, and the flag's ordering raised from `relaxed` to release/acquire (ER-STATE-14)
  → `docs/architecture/LATENCY_MODEL.md` (a new bullet stating the clear-once rule, why a dropped
  request is permanently stale, and that the fix rests on inspection rather than on the test), State
  test 27. No CHANGELOG entry: no user-visible behaviour changed on the platforms measured.
- `src/PluginProcessor.cpp` — an `AnamorphRoot` with no `ANAMORPH` child now returns before the
  adoption block (ER-STATE-15) → CHANGELOG `[0.9.6]` (one Fixed entry: a damaged file could relabel
  the sound you already had), `docs/architecture/SERIALIZATION_REGISTRY.md` (a new paragraph beside
  the existing "a chunk of neither recognised shape is not a restore at all", which states the same
  principle this branch was missing), State test 27. No schema field added, removed or renamed, and
  `getStateInformation` writes the child unconditionally, so no session the plug-in has written
  reaches the branch.
- `tests/state_tests.cpp` — State test 27, and a repair to State test 7: its out-of-range `active`
  guard built a root from an `AB` node alone, which ER-STATE-15 makes not-a-restore, so the clamp
  was no longer reached. Rebuilt from a genuine save. → `docs/procedures/TESTING.md`,
  `docs/REPOSITORY_MAP.md`, and the counts below.

**Reverted after measurement (ER-STATE-16).** Gating `readSlot`'s metadata reads on
`params.isValid()` was implemented and changed no test outcome: `abEnsureInit()` assigns the whole
`StateSet`, metadata included, and every reader calls it first. The reasoning is recorded in
`SERIALIZATION_REGISTRY.md` beside the `AB` child so a later round re-reads rather than re-derives.

**Changelog audit.** All 18 `[0.9.6]` entries were verified against the tree independently, and each
staleness claim put to an adversarial refuter. 16 accurate; one refuted on verify; **one confirmed,
in round 8's own entry** — it attributed the A/B contamination entirely to instance reuse while its
own State test 26 leg 3 measured a *fresh* instance failing at 0.5 against a restored 0.75.
Rewritten to name both cases.

**Counts.** The state suite is **27 tests / 1111 checks** (was 26 / 1096); the DSP suite is unchanged
at 47 + the A/B clamp guard / 245. The `[0.9.6]` Fixed count is **19**. Swept through
`docs/policies/TESTING_POLICY.md`, `docs/procedures/TESTING.md`, `README.md`,
`docs/REPOSITORY_MAP.md`, `docs/architecture/RELEASE_HARDENING_PLAN.md` and `docs/HANDOVER.md`.

**Citation re-anchoring.** The two source fixes edited cited lines rather than merely shifting them,
so five anchors were re-derived BY SYMBOL — `legacyKey`, `readSlot`, `StateSet::isValid`,
`setValueNotifyingHost` and `updateLatency` — and the `setStateInformation` span, unchanged at
878-1111 but with edited content, needed its declarations retargeted. `DELIBERATE_REAIMS` carries
each for both bases. [Verified]


## Engineering-review programme, round 12 — legacy-Settings UB, a deterministic latency test, and ER-STATE-13 on AArch64 (2026-09-01, still the 0.9.6 change set)

**What the round is.** One confirmed production defect fixed, one regression-test gap closed
deterministically, two stale decision records corrected, and an AArch64 investigation that changed
nothing. Records: `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 12.

**Code changes and their doc syncs (trigger map applied):**
- `src/InternalState.h` (`migrateFromLegacyApvts`: the shared usability predicate on every legacy
  value, and domain clamping in double before the integer conversion — ER-STATE-17) → CHANGELOG
  `[0.9.6]` (one Fixed entry; the corruption was user-visible — an unselectable Settings menu, and
  the bad value persisted on the next save), `docs/architecture/SERIALIZATION_REGISTRY.md` (a new
  paragraph stating the rule and the measured pre-fix behaviour on both architectures), State test
  28, and the count sweep below. **No serialization field added, removed or renamed**, and valid
  legacy sessions migrate exactly as before (State tests 5 and 6 unchanged).
- `tests/state_tests.cpp` — State test 28; State test 27's first leg rewritten as a deterministic
  barrier test; `--legacy-settings-probe` added → `docs/procedures/TESTING.md` (both instruments
  documented, with the pre-fix numbers), `docs/architecture/LATENCY_MODEL.md` (the round-11 bullet's
  claim about what the test proves, corrected now that it discriminates).
- `tests/dsp_tests.cpp` — `--match-inject-probe`, written to cross-build against AnamorphDSP alone
  so it runs under `qemu-aarch64-static` → `docs/procedures/TESTING.md`.

**Stale records corrected (no production change).** `docs/KNOWN_ISSUES.md`'s KI-027 row still read
"fix gated … awaiting maintainer sign-off" and the entry was still open, three rounds after D-1 was
approved and implemented; `docs/policies/THREADING_POLICY.md` still listed it as a known violation
awaiting Architecture Review. Both corrected, with the round-1/2 diagnosis kept under a dated
banner rather than rewritten.

**ER-STATE-13 on AArch64: no change.** Six scenarios, both architectures, identical to three
decimals; same atomics (`is_always_lock_free = 1`) and same layout. Recorded with one
architecture-independent refinement: the stale-value transient is proportional to the gap between
the stale value and the settled measurement at injection, and self-corrects, so round 9's "inert"
was too strong for the general case even though its own measurement stands.

**Counts.** The state suite is **28 tests / 1201 checks** (was 27 / 1111); the DSP suite is unchanged
at 47 + the A/B clamp guard / 245. The `[0.9.6]` Fixed count is **20**. Swept through
`TESTING_POLICY.md`, `TESTING.md`, `README.md`, `REPOSITORY_MAP.md`, `RELEASE_HARDENING_PLAN.md`
and `HANDOVER.md`.

**Citation re-anchoring.** The ER-STATE-17 guard edited cited lines inside `migrateFromLegacyApvts`,
so three anchors into `src/InternalState.h` were re-derived BY SYMBOL (`oversampleValue`,
`migrateFromLegacyApvts` ×2) and declared in `DELIBERATE_REAIMS` for both bases; eleven more moved
mechanically. [Verified]


## Engineering-review programme, round 13 — ER-STATE-17 on the real fixture; the compatibility gate closes (2026-09-01, still the 0.9.6 change set)

**What the round is.** Round 12's legacy-Settings fix verified against the repository's real frozen
pre-0.8.4 fixture rather than a synthetic shape, and compatibility-checklist items 5 and 7 recorded
on the maintainer's attestation. Records: `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md`
§Round 13 and the dashboard.

**Code changes and their doc syncs.** `tests/state_tests.cpp` only — State test 28 gains three
restores of `tests/fixtures/legacy_pre_0_8_4_view_params.xml` mutated in place (untouched, every
Setting malformed, every Setting out of domain), each asserting the surrounding session intact →
`docs/procedures/TESTING.md`, `docs/architecture/SERIALIZATION_REGISTRY.md` (the ER-STATE-17
paragraph's coverage sentence), and the counts below. Verified discriminating on the real file: the
mutated legs fail without the fix; the untouched leg passes either way, which is the point. No
`src/` change.

**Checklist items 5 and 7 — recorded on attestation, not inferred.** `docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`
(completion record 8/8, items ticked, evidence entries), `docs/HANDOVER.md` (Release Status: one
precondition left; Known Blockers; Roadmap; the tag-order sentence) and `docs/COMMERCIAL_STATUS.md`
§6 (engineering/process list empty). The record carries what the maintainer supplied — verdict,
date, performer — and marks hosts, operating systems, plug-in formats and automation lanes NOT
RECORDED, the same rule the Level-5 audition record follows. `RELEASE_POLICY` precondition 2 is
satisfied; the one remaining tag blocker is KI-015.

**Counts.** The state suite is **28 tests / 1237 checks** (was 28 / 1201); the DSP suite is
unchanged at 47 + the A/B clamp guard / 245. Swept through `RELEASE_HARDENING_PLAN.md` and
`HANDOVER.md`; the test count itself did not change, so the six documents that carry it were not
touched. [Verified]


## Engineering-review programme, round 14 — partial modern Settings inherited the previous project (2026-09-01, still the 0.9.6 change set)

**What the round is.** One confirmed state-isolation defect fixed, and two investigation-only items
verified with no change. Records: `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md`
§Round 14.

**Code change and its doc syncs (trigger map applied):**
- `src/InternalState.h` (`restoreState` writes all six host-hidden Settings unconditionally, absent
  → the documented default; the six defaults consolidated into one `settings()` table the
  constructor also seeds from — ER-STATE-18) → CHANGELOG `[0.9.6]` (one Fixed entry; the leak was
  user-visible and persisted on the next save), `docs/architecture/SERIALIZATION_REGISTRY.md` (a new
  paragraph stating that "Required: No" means the Default is APPLIED, with the 6-of-6 vs 0-of-6
  measurement), State test 29, `docs/procedures/TESTING.md` (the probe), and the count sweep below.
  **No serialization field added, removed or renamed**, and a session that carries a field restores
  it exactly as before.

**Where the finding was, versus where it was filed.** The review located it in
`migrateFromLegacyApvts` (`PluginProcessor.cpp:1038`, the v0.2 branch). Measurement put it the other
way round: the modern path inherited 6 of 6, the legacy path 0 of 6. The legacy function has always
written all six unconditionally. Recorded in both the worklog and the probe's own output so the two
paths cannot be confused again. It is also **not** a contradiction with the informational item
"missing nodes use normalized defaults" — that covers `applyNorm` and APVTS parameters, a different
subsystem.

**Verified and NOT acted on:** a malformed value on the modern Settings path carries no undefined
conversion (`syncAtomics` clamps through `jlimit`; `juce::var`→`int` on a string is a safe parse, not
the float cast that made the legacy path undefined in round 12).

**Investigation-only items, both unchanged.** ER-RT-05: the realtime-lint boundary is described
correctly in all four documents and none claims automatic cross-file discovery — fifth consecutive
round verified. D-1: the approval is recorded correctly in KI-027's row, its detail banner,
`THREADING_POLICY.md` and `LATENCY_MODEL.md`; round 12 fixed the two that had been stale and nothing
has regressed. The attribution is role-level ("the maintainer"), the same convention as every other
gate sign-off here; that is noted, not independently verified.

**Counts.** The state suite is **29 tests / 1270 checks** (was 28 / 1237); the DSP suite is unchanged
at 47 + the A/B clamp guard / 245. The `[0.9.6]` Fixed count is **21**. Swept through
`TESTING_POLICY.md`, `TESTING.md`, `README.md`, `REPOSITORY_MAP.md`, `RELEASE_HARDENING_PLAN.md`
and `HANDOVER.md`. [Verified]

## Engineering-review programme, round 15 — the off-message-thread re-prepare race (2026-09-02, still the 0.9.6 change set)

**What the round is.** One concurrency defect confirmed under ThreadSanitizer and fixed inside the
approved D-1 architecture; two investigation-only items verified with no change; one new risk
recorded. Records: `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 15.

**Code change and its doc syncs (trigger map applied):**
- `src/PluginProcessor.cpp/.h` (`prepareToPlay` calls `requestLatencyUpdate()` instead of
  `updateLatency()`; `requestLatencyUpdate()` also delivers synchronously when no `MessageManager`
  exists) and `src/dsp/AnamorphEngine.cpp/.h` (`latency2/4/8` become relaxed `std::atomic<int>`)
  — ER-STATE-19 → CHANGELOG `[0.9.6]` (one Fixed entry; a host could be left holding a stale
  latency), `docs/architecture/LATENCY_MODEL.md` (the D-1 section: which hosts prepare off the
  message thread, what the race was, the 50 ms cost applied to activation),
  `docs/policies/THREADING_POLICY.md` (the D-1 row added to the communication table; the round-15
  extension; the atomic-usage rule for the flag and the engine figures; `prepareToPlay` added to
  the host-call assumption), `docs/architecture/THREAD_MODEL.md` (the D-1 row — the table had never
  carried it), `docs/KNOWN_ISSUES.md` (KI-027 banner and row), `docs/FUTURE_RISKS.md` (RISK-007
  round-15 note narrowing the pluginval argument to VST3; **new RISK-008**),
  `docs/architecture/API_REFERENCE.md` (`prepareToPlay` row), State test 30 and the
  `--reprepare-race-probe` (`docs/procedures/TESTING.md`), and the count sweep below.
  **No serialization change, no parameter change, no reported-latency VALUE change** — only the
  thread on which one caller delivers it, which is the decision D-1 already made.

**Threading-model note.** No new communication path and no new ordering: the flag and its
release/acquire pair are D-1's, approved in round 4; `prepareToPlay` is one more producer on it,
and `latency2/4/8` are the payload it publishes (relaxed, no ordering role). Recorded here because
`THREADING_POLICY.md` says a new shared-state path or ordering is a gate item — this is neither,
and the worklog says why.

**Drift found and corrected:** the `THREAD_MODEL.md` and `THREADING_POLICY.md` communication tables
omitted the D-1 path entirely; `HANDOVER.md`'s Test-Status row still read 28 / 1237 after round
14's sweep claimed to have updated it; RISK-007's "pluginval structurally cannot produce the
window" held for VST3 `setState` only.

**Investigation-only items, both unchanged.** ER-RT-05: sixth consecutive verification; the new
relaxed loads in `getLatencySamples()` are not a forbidden class and `prepare()` stays out of
scope. D-1: the approval record is correct in all four places; this round extends the mechanism
and does not touch the decision.

**Counts.** The state suite is **30 tests / 1278 checks** (was 29 / 1270); the DSP suite is unchanged
at 47 + the A/B clamp guard / 245. The `[0.9.6]` Fixed count is **22**. Swept through
`TESTING_POLICY.md`, `TESTING.md`, `README.md`, `REPOSITORY_MAP.md`, `RELEASE_HARDENING_PLAN.md`
and `HANDOVER.md` (both rows this time). [Verified]

## Engineering-review programme, round 16 — the previous project's A/B Level-Match gains (2026-09-02, still the 0.9.6 change set)

**What the round is.** One confirmed session-isolation defect fixed on two paths, one investigation
closed with evidence and no code change, and two record checks. Records:
`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 16.

**Code change and its doc syncs (trigger map applied):**
- `src/PluginProcessor.cpp/.h` (`abResetToDefaults()` and `readSlot` now reset `abMatchGain[]`
  alongside the slot they reset — ER-STATE-20) → CHANGELOG `[0.9.6]` (one Fixed entry; the leak was
  user-visible in the Level Match readout and in what the matcher re-converged from),
  `docs/architecture/SERIALIZATION_REGISTRY.md` (a new paragraph in the `AB` section: the per-slot
  match gain is part of the slot and resets with it, with the measured figures and the reason it is
  the one field nothing overwrote), State test 31, `docs/procedures/TESTING.md` (the test's
  observation method and the new probe), and the count sweep below. **No serialization field added,
  removed or renamed** — the cache has never been in the format — and a session that carries valid
  A/B data restores exactly as before.

**Why the fix is in two places.** The finding named `abResetToDefaults`, which covers the two restore
paths with no `AB` node. Tracing found a third: an `AB` node that exists but carries no usable slot
payload resets its slots inside `readSlot` and never reaches that function. Confined to the named
function the fix still leaked −2.405 dB on that path, which is also the only one that exposes slot A.

**Investigation recorded, no production change (ER-STATE-21).** Malformed *modern* host-hidden
Settings, 19 inputs measured through `--modern-settings-probe`: no crash, no undefined behaviour,
and every DSP-facing read clamped at source. 19 of 19 persist into the next save, 8 leave an
out-of-domain ComboBox id and 3 a non-finite scope persistence; opening the editor repairs 4.
Ingress is a hand-edited or corrupted file only. What a malformed *present* value should mean is a
serialization-contract decision the `ANAMORPH_INTERNAL` table does not state — it states defaults for
ABSENCE — so it is filed rather than invented. Documented in `TESTING.md` and the worklog.

**Counts.** The state suite is **31 tests / 1301 checks** (was 30 / 1278); the DSP suite is unchanged
at 47 + the A/B clamp guard / 245. The `[0.9.6]` Fixed count is **23**. Swept through
`TESTING_POLICY.md`, `TESTING.md`, `README.md`, `REPOSITORY_MAP.md`, `RELEASE_HARDENING_PLAN.md`
and `HANDOVER.md`. [Verified]

## Engineering-review programme, round 17 — malformed Settings followed to their consumers (2026-09-02, still the 0.9.6 change set)

**What the round is.** Two investigations resolved. One produced a concrete defect with a minimal
fix; the other is refuted. Records: `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md`
§Round 17.

**Code change and its doc syncs (trigger map applied):**
- `src/gui/Vectorscope.h` (`setPersistence` substitutes the default for a non-finite input, closing a
  reachable `(int)`-of-NaN in `windowFrames()`; a `getPersistence()` accessor added so the guard is
  testable through the real editor — ER-STATE-21) → CHANGELOG `[0.9.6]` (one Fixed entry; a damaged
  stored value drove an undefined afterglow length), `docs/architecture/SERIALIZATION_REGISTRY.md`
  (a new `ANAMORPH_INTERNAL` paragraph recording the malformed-value measurement, the fixed
  consequence, and the still-open contract question), State test 32, `docs/procedures/TESTING.md`
  (the probe's second table and the test), and the count sweep below. **No serialization change**,
  and the stored value itself is untouched.

**Why the fix is at the consumer and not at the restore.** Sanitising in `restoreState` would define
what a malformed *present* value MEANS, which is the contract question the round was asked to
resolve with evidence rather than invent. The guard sits where the "0..1" promise is declared, is
correct for every caller, and leaves the contract decision to the maintainer.

**Refuted, no change (ER-GUI-05).** The abandoned-gesture sweep's direct-child traversal cannot strand
a gesture: `ValueBox` opens one only through `rotaryParent (getParentComponent())`, so a wrapper would
stop the gesture existing rather than hide it, and State test 21 already asserts the press registers
before testing the reconcile. All thirteen sliders are registered and JUCE parents every value box
directly. Documentation was not misstated, so none changed.

**Counts.** The state suite is **32 tests / 1323 checks** (was 31 / 1301); the DSP suite is unchanged
at 47 + the A/B clamp guard / 245. The `[0.9.6]` Fixed count is **24**. Swept through
`TESTING_POLICY.md`, `TESTING.md`, `README.md`, `REPOSITORY_MAP.md`, `RELEASE_HARDENING_PLAN.md`
and `HANDOVER.md`. [Verified]

## Engineering-review programme, round 18 — the approved Settings recovery policy (2026-09-02, still the 0.9.6 change set)

**What the round is.** One maintainer-approved production change implemented and closed, and one risk
investigated to a classification with no code change. Records:
`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 18.

**Code change and its doc syncs (trigger map applied):**
- `src/InternalState.h` (the `settings()` table carries each field's DOMAIN beside its default;
  `usableNumber()` is the shared number predicate the legacy migration now also calls instead of its
  own duplicate; `repairedValue()` resolves a present-but-invalid value and `restoreState` writes it
  back — ER-STATE-21, "Policy B") → CHANGELOG `[0.9.6]` (one Fixed entry; a damaged setting is
  repaired on open and the repair is what gets saved), `docs/architecture/SERIALIZATION_REGISTRY.md`
  (the decision recorded verbatim with the per-field resolution table, replacing the open question —
  and a duplicated paragraph from a round-17 partial edit collapsed), State test 33,
  `docs/procedures/TESTING.md`, and the count sweep below. **No schema change, no property renamed**,
  and a valid session restores exactly as before.

**Why the repair is at restore and not at the reads.** That is the decision: the alternative
("clamp at the read") leaves the damage in the file to be re-interpreted on every load. Round 17's
finiteness guard at `Vectorscope::setPersistence` is kept as the defensive backstop the decision asks
for, not removed.

**RISK-008, class B, no production change.** The mechanism is confirmed from the pinned wrapper —
`messageThread->start()` exists in exactly one place, the `EventHandler` destructor at unload, while
`stop()` runs whenever a host run loop registers — and its cost is measured by a clearly-labelled
synthetic probe: a request is deferred, not dropped, and the host is stale for exactly the unserviced
window. No Linux VST3 host was obtainable, so no host-visible failure was demonstrated and the
classification stops short of actionable. `docs/FUTURE_RISKS.md` records the evidence and the
limitation; D-1 is untouched.

**Counts.** The state suite is **33 tests / 1406 checks** (was 32 / 1323); the DSP suite is unchanged
at 47 + the A/B clamp guard / 245. The `[0.9.6]` Fixed count is **25**. Swept through
`TESTING_POLICY.md`, `TESTING.md`, `README.md`, `REPOSITORY_MAP.md`, `RELEASE_HARDENING_PLAN.md`
and `HANDOVER.md`. [Verified]

## Engineering-review programme, round 19 — RISK-008's real-host half (2026-09-02, still the 0.9.6 change set)

**What the round is.** A disposition and documentation round. **No production code changed, and none
was justified.** Records: `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 19.

**Evidence recorded.** The maintainer ran the predicted-failure workflow on a real host — Linux,
REAPER, the real Anamorph VST3 — and the reported latency updated with the Anamorph editor both open
and closed. That is the observable RISK-008 predicts would fail with the editor closed. Recorded as
manual real-host evidence and kept distinct from the round-18 synthetic probe, which measures the
cost of an unserviced queue rather than whether any host produces one.

**Documents touched:** `docs/FUTURE_RISKS.md` (the RISK-008 register row's likelihood moved from
Unknown to Low; the likelihood bullet rewritten; the real-host result and its limits added; round
18's "no host available" note marked superseded rather than deleted; the mitigation's pending host
census replaced by its first data point; a round-19 line in the sync header),
`docs/procedures/TESTING.md` (the probe's scope note now points at the real-host half),
`tests/state_tests.cpp` (the probe's own printed EVIDENCE LIMIT line, a `printf` only — no test
logic, no count change), the programme worklog and the dashboard.

**Limitation recorded exactly, not glossed.** The repository contains no evidence of how REAPER
supplies `Linux::IRunLoop`; every REAPER reference in the tree concerns unrelated matters. The route
is therefore not established and not guessed at, and no Linux-wide claim is made — the residual is
every other Linux VST3 host, and it does not justify a production change.

**Settled set audited against the live documents** (registries, policies, procedures, architecture,
README, HANDOVER — not the worklog, which is historical by construction): ER-STATE-21 FIXED, drag
recovery REFUTED, the realtime-lint boundary accurate for a ninth consecutive round, D-1 approved and
implemented, D-2 deferred, KI-015 still the single release blocker.

**Counts.** Unchanged: the state suite is 33 tests / 1406 checks and the DSP suite 47 + the A/B clamp
guard / 245. The `[0.9.6]` Fixed count is unchanged at 25 — nothing user-visible changed. [Verified]

## Engineering-review programme, round 20 — the restored-session glide and the malformed boolean (2026-09-02, still the 0.9.6 change set)

**What the round is.** Two production fixes and one disposition. Records:
`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 20.

**ER-DSP-09 — a restored non-default session GLIDED into its own sound.** Reproduced with the
product's own signal before anything was changed (`AnamorphStateTests --restore-fade-probe`): block 1
against the settled figure read 0.17 (Haas), 0.09 (Velvet), 0.29 (Chorus), 0.39 (Dimension-D), 0.35
(Mono Maker crossover). Cause: each module's own `prepare()` snaps its smoothers, but it runs before
`updateDerived()` installs the restored snapshot, and `reset()` then re-zeroed the chorus blend.
Fixed at that ordering — four `snapToTargets()` calls at the END of `AnamorphEngine::prepare()`,
deliberately not in `reset()` or `snapSmoothers()`, which also run at a switch duck's silent bottom,
so live edits keep smoothing unchanged. `ChorusEngine`'s depth snap is deferred one block because its
target is expressed in samples at the per-block working rate.

**ER-STATE-22 — a malformed numeric boolean switched a setting ON, durably.** An implementation
correction to the approved ER-STATE-21 policy, not a new decision: `v != 0.0` is the C coercion, not
the field's domain, so `-1`, `-2`, `2` and `0.5` all resolved to `true` and the repair persisted
that; `0` was the only value on the real line that could not turn a setting on. Anything outside
`{0, 1}` now takes the documented default. `juce::exactlyEqual` is used for the two exact compares,
so the `-Wfloat-equal` gate is not widened.

**ER-STATE-23 — no production change.** Classified as entirely covered by the deferred D-2 decision.
The latency atomics carry the latency request and nothing else, so the finding's premise is a
category error; the restore/A-B/preset tail is RISK-007's four already-measured races; the engine's
plain state has no concurrent writer; and the one thread pairing D-2's scope does not name was
measured for this round (`--state-prepare-race-probe`, three threads under TSan) and produced the
same four reports and no new ones. Nothing was added to suppress the report.

**Documents touched:** `CHANGELOG.md` (two Fixed entries; and the round-17 Scope-Persistence entry,
which a double-apply in that round had left in the file **twice**, verbatim — one copy removed, no
wording changed), `docs/architecture/SERIALIZATION_REGISTRY.md` (the three boolean rows of the
recovery table, plus a footnote recording why a boolean's domain is `{0, 1}` and the re-measured
pre-policy failure count), `docs/architecture/API_REFERENCE.md` (the `AnamorphEngine::prepare` row
now states the snap contract instead of "allocates; resets"), `docs/FUTURE_RISKS.md` (a round-20
bullet under RISK-007 recording the ER-STATE-23 disposition and its three-way split),
`docs/procedures/TESTING.md` (Test 49, the State test 33 extension, and the two new probes as the
tenth and eleventh opt-in instruments), `docs/policies/TESTING_POLICY.md`, `README.md`,
`docs/architecture/RELEASE_HARDENING_PLAN.md`, `docs/HANDOVER.md` (counts and the version row), the
programme worklog and the dashboard.

**One test-instrument correction, recorded rather than quietly applied.** `--restore-fade-probe`
printed "NOT settled: the module glides in" as a verdict. Its ratio cannot support that after the
fix: it sees a smoother glide AND empty delay-line/filter history filling up, and the second is not
a defect — `prepare` clears that history by contract, and Haas's own 28 ms line outlasts the block
the first point covers. So the ratio rises without reaching 1.0 (0.17→0.72, 0.09→0.18, 0.29→0.68,
0.39→0.90, 0.35→0.58) and the probe now says which two causes it does not separate. DSP Test 49 is
the discriminating instrument, because its reference cancels the history term exactly.

**Counts.** The DSP suite is **48 tests + the A/B clamp guard / 256 checks** (was 47 / 245; Test 49
adds 11). The state suite is **33 tests / 1431 checks** (was 1406; State test 33 went from thirty
cases to thirty-nine). The `[0.9.6]` Fixed count is **27** — two added this round,
and the duplicate removal means the file's bullet count and its distinct-entry count now agree.
Measured, not inferred: `AnamorphTests` prints `256 checks, 0 failures` and `AnamorphStateTests`
prints `1431 checks, 0 failure(s)`. [Verified]

## Engineering-review programme, round 21 — the phase meter's overflow on extreme-but-finite audio (2026-09-02, still the 0.9.6 change set)

**What the round is.** One DSP correctness fix and one re-measurement. Records:
`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 21.

**ER-DSP-10 — extreme BUT FINITE audio made the phase meter read "fully decorrelated".** Reproduced
before any change, with the module's own code: at `l = r = 1e10` the meter published **0** for a
perfectly correlated mono signal (**−0** for its anti-phase twin) where the same arithmetic in double
gives **1**. Every value the existing guard can see is finite — samples 1e10, per-sample products
1.00000002e20, all six accumulators 9.99746693e19 — so `publish()`'s `sanitize()` accepts the state
and never fires. The overflow is one step later and is the only one: `ll * rr` is a **float** multiply
of two mean-square values, which leaves float above `√FLT_MAX = 1.84467435e19`, i.e. from steady input
above `|l| = 4.29496723e9`. `sqrt(+Inf)` is `+Inf`, `+Inf` is not below the 1e-12 small-signal floor,
and `lr / +Inf` is a perfectly finite **0.0**. Fixed at that operation: on that overflow alone, the
denominator is recomputed in double, where the product of two finite floats is exact (48 significand
bits into 53) and at most ~1.16e77.

**The normal range is bit-for-bit unchanged, and that was measured rather than asserted.** The
pre-fix and post-fix expressions were compared over **19,995,466** randomised finite-product triples
spanning `ll`/`rr` from 1e-40 to 1e19: **zero differing bit patterns**. The float-only alternative
(`sqrt(ll)*sqrt(rr)`) was swept as well — every representable top-binade pair against `FLT_MAX` and
against itself, 2 × 8,388,608 pairs, none overflowing — and rejected in favour of the exact double
product, with that measurement recorded in the source comment rather than an unsupported claim.

**ER-STATE-23 re-raised — no disposition change, so no new finding was created.** Per this file's own
rule against duplicate records, the re-measurement was appended to the existing `RISK-007` entry
rather than filed again. `src/PluginProcessor.cpp` and `.h` are unchanged since round 16, and the
three TSan probes were re-run on the current build: `--state-thread-probe` and
`--state-prepare-race-probe` report the same four races and no others, `--reprepare-race-probe` is
silent. Each report maps one-to-one onto a row the register already carries.

**Documents touched:** `CHANGELOG.md` (one Fixed entry), `docs/architecture/DSP_ALGORITHMS.md` (the
`CorrelationMeter` section now separates the two non-finite contracts — accumulator poisoning versus
denominator overflow — which is the distinction the defect lived in), `docs/procedures/TESTING.md`
(Test 50, and the DSP test count), `docs/FUTURE_RISKS.md` (a round-21 bullet under RISK-007),
`docs/policies/TESTING_POLICY.md`, `README.md`, `docs/architecture/RELEASE_HARDENING_PLAN.md`,
`docs/HANDOVER.md` (counts and the version row), the programme worklog and the dashboard.

**Inspected and left alone, recorded so it is not re-derived:** `balance` and `energy` are built from
`llSlow + rrSlow` and `llFast + rrFast` — SUMS, not products — so they overflow only above an input
magnitude of ≈ 1.3e19, three orders beyond the regime that breaks the phase reading, and there
`energy = +Inf` still reads "playing" and `balance = 0` still reads "centred", which is correct for
an equal-energy pair. No demonstrated defect; not changed.
**Corrected in round 23 (ER-DSP-11):** the `energy` half stands — its only consumer is a `< 6e-9`
silence predicate, which `+Inf` answers correctly — but the `balance` half was wrong. "Balance = 0
still reads centred" holds only for channels that really are equal, and the meter exists for the
case where they are not: an overflowing sum published `-0.0` for a 1.8e19/1.0e19 pair whose true
balance is −0.53. Reproduced and fixed in round 23; this entry is left as written, with the error
named rather than edited away.

**Counts.** The DSP suite is **49 tests + the A/B clamp guard / 269 checks** (was 48 / 256; Test 50
adds 13). The state suite is unchanged at **33 tests / 1431 checks**. The `[0.9.6]` Fixed count is
**28**. Measured, not inferred: `AnamorphTests` prints `269 checks, 0 failures` and
`AnamorphStateTests` prints `1431 checks, 0 failure(s)`. [Verified]

## Engineering-review programme, round 22 — the `docs` gate, and the filtered preflight that hid it (2026-09-02, still the 0.9.6 change set)

**What the round is.** A CI round. **No production code changed, and none was justified.** Records:
`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 22.

**ER-CI-06 — the `docs` job failed on this file.** Step "Lint documentation structure",
`docs/DOCUMENTATION_COVERAGE.md:7365`: *"table fragment with no header/separator (1 pipe line(s))
-- a block was inserted mid-table, or the separator row is missing"*, `check-docs: 1 finding(s)
across 117 file(s)`, exit 1. Round 21's entry had wrapped the absolute-value notation for an input
magnitude onto the START of a line, and a line beginning with a pipe character is a table row in
Markdown — so the checker was right and a renderer would have mis-parsed it too. Reflowed to name
the quantity in words instead. **The gate was not touched, no exclusion widened, no workflow
changed**, and a repo-wide sweep found no second instance.

**Classified with evidence, not by assumption.** It reproduces on a bare local checkout at exit 1,
so it is repository content — not a stale run, not an environment difference, not citation drift,
not a workflow fault. The passing `Build & Validate` run on the same commit is the `pull_request`
event, in which `docs` is **skipped** together with eleven other jobs; only the `push` run executes
the matrix, so a green PR run is not evidence about `docs`.

**ER-CI-07 — why round 21 reported a green preflight while this was failing.** `scripts/preflight.sh`
is `set -euo pipefail` and `check-docs.py` is its **second** command, so that invocation aborted
there and every later stage — the portability and realtime lints, the four warning-gate self-tests,
the ABI floor, the citation gate on all three bases, both suites — **never ran**. It read as green
because the run was piped through a `grep` whose pattern could not match the finding's wording,
which also replaced the script's exit status with grep's, under an unconditional trailing `echo`.
The round-21 report's other validation lines stand, because those checks were also run as separate
direct commands; its "preflight" line does not.

**Documents touched:** this file (the round-21 reflow, and this entry), `scripts/preflight.sh` (a
header paragraph beside its existing "NO SILENT SKIPS" rule: the script fails fast, so a non-zero
exit means the stages below it are an unknown result, and a filtered view of its output is not a
result), `docs/procedures/CI_CD.md` (the same rule in §preflight, with the run that cost it), the
programme worklog and the dashboard. **No source file, no test, no workflow, no baseline.**

**Settled-record sweep — nothing to correct.** ER-DSP-10 FIXED, ER-STATE-21 FIXED under Policy B,
drag recovery REFUTED and presented as open nowhere, D-1 approved and implemented, D-2 / RISK-007
deferred with round 21's re-measurement recorded, RISK-008 carrying its real-host REAPER result with
the host-specific residual stated. The three surviving "no host available" strings are explicitly
dated round-18 history or scoped to the review harness, sitting beneath the round-19 bullet that
supersedes them — the disposition round 19 chose deliberately, and not rewritten here.

**Counts.** Unchanged: the DSP suite is 49 tests + the A/B clamp guard / 269 checks and the state
suite 33 tests / 1431 checks; the `[0.9.6]` Fixed count stays 28 — nothing user-visible changed.
[Verified]

## Engineering-review programme, round 23 — the balance meter's own overflow (2026-09-02, still the 0.9.6 change set)

**What the round is.** One DSP correctness fix, in the same file and regime as round 21's but at a
**different operation**. Records: `worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md`
§Round 23.

**ER-DSP-11 — extreme but FINITE levels erased the channel imbalance.** Reproduced before any
change: at `l = 1.8e19`, `r = 1.0e19` the meter published a balance of **−0.0** — perfectly centred —
for a pair whose true balance is **−0.5283**, and the mirrored input published **+0.0** for **+0.5283**.
Every value the existing guard can see is finite: the per-sample squares (3.24000014e38,
9.99999968e37), both accumulators (`llSlow` 3.23707947e38, `rrSlow` 9.98539635e37) and even the
numerator `rrSlow − llSlow` (−2.23853994e38, carrying the whole imbalance). The only overflow is
`llSlow + rrSlow`, a float **add**, which is +Inf where the exact double sum is 4.2356191e38 against
`FLT_MAX` 3.40282347e38 — and `+Inf` sails past the 1e-12 small-signal guard, so the division is
`finite / +Inf`, a well-formed **0**. Threshold: input above **1.30438174e19** for equal channels,
anywhere an unequal pair sums past `FLT_MAX`, up to the **1.84467435e19** at which `l*l` would itself
stop being finite. Fixed by redoing the **sum** in double on that overflow alone.

**Kept distinct from the two neighbouring contracts, deliberately.** It is not ER-DSP-10 — that is
the phase meter's `ll * rr` **product** in `correlation()`, and the round-21 build reproduces this
defect at full strength, so fixing the product did nothing for the sum. It is not the Test 45 poison
class either — every accumulator is finite and `sanitize()` never fires, which Test 51 asserts rather
than assumes. Test 45 owns the poisoned accumulator, Test 50 the product, Test 51 the sum.

**Normal range preserved bit-for-bit, and scale invariance restored across the edge — both measured.**
Pre- and post-fix expressions compared over **19,671,802** randomised finite-sum energy pairs
spanning 1e-40 to 1e38: **zero differing bit patterns**. At a fixed 3:1 energy ratio the true balance
is −0.5 at every scale; the pre-fix build holds −0.5 up to `s = 8.5e37` and drops to −0.0 at
`s = 8.6e37`, the first point where the sum stops being finite, while the fixed build holds −0.5
across the whole sweep.

**A round-21 record corrected rather than rewritten.** Round 21 inspected this same sum and wrote
that `balance = 0` "still means centred, which is what an equal-energy pair should read". The
threshold was right and the conclusion was wrong — it holds only when the channels really ARE equal,
which is the one case a balance meter is not for. Both copies of that note (the round-21 worklog
section and its entry in this file) now carry the correction beside them, kept as written with the
error named rather than edited away. The `energy` half of the same note stands, and this round
grounded it: its only consumer is `gui/CorrelationMeter.cpp`'s `< 6e-9` silence predicate, which
`+Inf` answers correctly as "not silent".

**Documents touched:** `CHANGELOG.md` (one Fixed entry), `docs/architecture/DSP_ALGORITHMS.md` (the
`CorrelationMeter` section now carries all three non-finite contracts and says which test owns each),
`docs/procedures/TESTING.md` (Test 51 and the DSP test count), `docs/policies/TESTING_POLICY.md`,
`README.md`, `docs/architecture/RELEASE_HARDENING_PLAN.md`, `docs/HANDOVER.md` (counts and the
version row), this file, the programme worklog and the dashboard.

**Carried unchanged, with no duplicate finding filed.** D-2 / RISK-007 stays deferred — this round
produced no new evidence, `src/PluginProcessor.cpp` and `.h` are still unchanged since round 16, and
the only source touched is a static computation inside `publish()` with no shared state. RISK-008
keeps its real-host REAPER result and its host-specific residual; **no host test was performed**.
ER-STATE-21 FIXED, drag recovery REFUTED, D-1 approved and implemented, the cross-file realtime-lint
boundary unchanged.

**Counts.** The DSP suite is **50 tests + the A/B clamp guard / 282 checks** (was 49 / 269; Test 51
adds 12). The state suite is unchanged at **33 tests / 1431 checks**. The `[0.9.6]` Fixed count is
**29**. Measured, not inferred: `AnamorphTests` prints `282 checks, 0 failures` and
`AnamorphStateTests` prints `1431 checks, 0 failure(s)`. [Verified]

## Engineering-review programme, round 24 — the foreign-preset acceptance test (2026-09-02, still the 0.9.6 change set)

**What the round is.** One fix, closing the last actionable Review Bug. Records:
`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 24.

**ER-STATE-24 — a valid preset from another plug-in replaced the current sound.** Reproduced before
any change, against a five-parameter non-default sound and a well-formed `<SomeOtherPluginPreset>`
carrying two `PARAM` children with our own `id` spellings: `loadFile` returned **true**, `drive` and
`width` took **the foreign file's values** (0.0396 and 0.0250 normalised, from 0.95 and 0.05 plain),
and `algorithm`, `monoMakerFreq` and `chorusRate` were reset to their defaults. **The review's
wording covered half the mechanism**: `applySoundTree` resolves each parameter with
`getChildWithProperty ("id", …)`, which searches by property and ignores the root, so a foreign
document both adopts what it names and defaults what it does not. Neither loader validated the root;
the only validation on the path guarded the value *inside* a matched child.

**The contract was taken from the repository, not invented.** ER-STATE-02 already settled the same
question for A/B slot payloads — `readSlot`'s `adoptIfAnamorph` accepts only `apvts.state.getType()`
and refuses a foreign-typed tree precisely as it refuses an unparsable one. Applied here:
`loadFile` returns `false`, `load(index)` is a clean no-op, and sound, preset name and menu tick are
all untouched. **No new preset API, and no maintainer decision was needed.**

**One shared choke point.** Both loaders already did parse → null-check → apply, so the acceptance
test *replaces* the null check inside one new private helper, `PresetManager::parseSoundFile`. In
`load(int)` it resolves before `onAboutToLoad()`, which is where that function's own comment already
requires every failure to land. The check cannot live in `applySoundTree`: that function looks
parameters up by property and so genuinely cannot tell a foreign tree from ours, and it returns
`void`, so it could not report the rejection to `loadFile`'s `bool`. Its declaration now states the
precondition rather than carrying an unreachable second check.

**Documents touched:** `CHANGELOG.md` (one Fixed entry), `docs/architecture/SERIALIZATION_REGISTRY.md`
(the `ANAMORPH` section now records the preset file's root as its acceptance test, with the measured
failure and the ER-STATE-02 precedent), `docs/procedures/TESTING.md` (State test 34 and the suite
count), `docs/policies/TESTING_POLICY.md`, `README.md`,
`docs/architecture/RELEASE_HARDENING_PLAN.md`, `docs/HANDOVER.md`, this file, the programme worklog
and the dashboard. **Unchanged:** every other architecture document, all workflows, both warning
baselines.

**A count correction, measured rather than carried.** The state suite's *test* count was one ahead in
every document. Counted from `main`'s registered test functions: **32 at `HEAD`** against a
documented 33, so State test 34's arrival makes the true figure **33**, not 34 — corrected in five
documents with the counting method recorded beside it in `HANDOVER.md`. The *check* count was never
wrong.

**No new race class.** The change is a file parse and a type test on the message thread, in a `const`
helper with no shared state, and `applySoundTree`'s behaviour for accepted trees is byte-identical.
No probe was re-run and **no duplicate D-2 finding was filed**; D-2 / RISK-007 stays deferred with
its four measured race classes unchanged. RISK-008 keeps its real-host REAPER result and its
host-specific residual; **no host test was performed**.

**Counts.** The state suite is **33 tests / 1460 checks** (was 32 / 1431; State test 34 adds 29). The
DSP suite is unchanged at **50 tests + the A/B clamp guard / 282 checks**. The `[0.9.6]` Fixed count
is **30**. Measured, not inferred: `AnamorphStateTests` prints `1460 checks, 0 failure(s)` and
`AnamorphTests` prints `282 checks, 0 failures`. [Verified]

## Engineering-review programme, round 25 — a minimal `[0.9.6]` Change Log correction (2026-09-03)

**Documentation only; no source, test, workflow, CMake or baseline file changed.** Records:
`worklogs/engineering-review/ENGINEERING_REVIEW_PROGRAMME.md` §Round 25.

The `[0.9.6]` release date moves to **2026-09-03**, and four claims are corrected because the current
implementation contradicts them: ER-DSP-11's "the addition now stays in range" and ER-DSP-10's "the
calculation now stays in range" both described the fixes as preventing an overflow when what they
actually do is **recover in double when the overflow happens**; ER-DSP-10's "that energy calculation
ran out of range" named the wrong operation (each energy stays finite — it is their **product** that
overflows); and the foreign-preset entry's "both accept any file you point them at" was present-tense
and false after round 24.

**A fifth correction the audit turned up, and an intra-section contradiction rather than a drift.**
The round-17 Scope Persistence entry ended "the stored value itself is untouched" — true when
written, and contradicted by round 18's Policy B entry *higher in the same section*, which repairs
`int_scopePersist` on restore and persists the repaired value. Corrected to point at that repair.
The consumer-side guard the entry is about is unchanged.

**Audited and left alone:** every test number the section cites resolves in the current tree, and the
measured figures cross-check against the worklog and `HANDOVER.md`. Historical figures were checked
for contradiction, not re-measured. D-2 / RISK-007, RISK-008 and KI-015 remain absent from the
Change Log — none is a fixed user-visible change, and the section carries no Known Issues structure.

**Counts.** Unchanged: DSP 50 tests + the A/B clamp guard / 282 checks, state 33 tests / 1460 checks,
`[0.9.6]` Fixed count 30. [Verified]
