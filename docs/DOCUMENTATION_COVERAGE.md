# DOCUMENTATION_COVERAGE.md

Permanent documentation-coverage audit. **Future contributors/AI must update this on every
documentation-affecting change** (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`).

Coverage = how well the module/topic is documented. Confidence = strength of the evidence behind
that documentation (Verified / Partially Verified / Unverified / Not Supported).

Last updated: for the **JUCE 8.0.14 → 9.0.0 migration & dependency hardening** (v0.8.13
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
CI_CD (actions @v7), DEPENDENCY_POLICY (`JUCE_*` flags at `CMakeLists.txt:183-188`; "then-current"
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
ADR-0011). Version bump `0.8.11 → 0.8.12` (`CMakeLists.txt:14`). Synced: this file, CHANGELOG
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
| docs root | SOURCE_OF_TRUTH, HANDOVER, REPOSITORY_MAP, DOCUMENTATION_COVERAGE, POSTMORTEMS, KNOWN_ISSUES, FUTURE_RISKS | Present |
| architecture | 15 docs (incl. RELEASE_HARDENING_PLAN) + ADR_INDEX + 16 ADRs (0016–0020 reserved, see plan §8) | Present |
| worklogs | performance/ (Waves 3–6 + the v0.8.11 final-pass and crossover-glide investigations), release-hardening/ (RH program working evidence; finalized decisions live in ADRs), root-level v0.8.12 GUI-fix records (`BANDWIDTH_DRAG_FIX_v0.8.12.md`, `MOUSE_RELEASE_STATE_FIX_v0.8.12.md`) + `POST_v0.8.12_AUDIT_AND_ROADMAP.md` + `STATE_HARNESS_v0.8.13.md` | Present |
| procedures | 8 docs | Present |
| policies | 15 docs | Present |
| root | README, CHANGELOG, CLAUDE | Present |

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
