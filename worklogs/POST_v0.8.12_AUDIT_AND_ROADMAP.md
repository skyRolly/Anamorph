# Post-v0.8.12 repository audit & development roadmap

> Full repository-level review after the v0.8.12 release: documentation-drift audit (with the
> corrections applied in the same PR), technical state assessment (DSP / GUI / compatibility /
> release engineering), a verdict on every previously deferred candidate, and a prioritized
> phased roadmap. **Documentation-only — no code change, no version bump.**

- **Date:** 2026-07-22 · **Base:** `main` @ `64e87c4` (v0.8.12, PR #80 merged) · **Branch:** `claude/beautiful-sagan-JAUFI`.

## 1. Method

Four parallel audit passes over the whole repository, each grounded in file/line/git evidence:
(a) drift audit of every docs-root file + README + CHANGELOG against git history and the source
tree; (b) extraction of every deferred/rejected/pending item from the six performance worklogs,
the release-hardening worklog, PERFORMANCE_BUDGET and RELEASE_HARDENING_PLAN; (c) policy/ADR
consistency (all 15 policies read; all 16 ADRs + index cross-checked; terminology sweep);
(d) release-engineering maturity (all 4 CI workflows, scripts/, packaging/, tests/, CMake read).
Findings were then re-verified in-place before any edit (every claimed stale line number was
re-grepped against the current tree). Finally, the complete edit set was itself adversarially
verified by three further independent lenses before commit (citation accuracy — every added
file:line opened and confirmed, including the KI-013 JUCE-source claim; historical-falsification /
policy compliance; missed-drift completeness). That pass corrected four of this audit's own edits —
a set-wide "behaviour-neutral" mislabel of v0.8.11's PRs #60/#61 (which are behaviour-changing by
design, KI-012), an unevidenced KI-007 status upgrade, HANDOVER's Known-Blockers range missing the
new KI-013, and one claimed-but-unapplied HANDOVER cite fix — and surfaced the second-pass items
marked ▸ in the table below.

## 2. Project health summary

**Strengths.** The DSP core is mature: 33 acceptance tests + A/B guard (140 checks) green; five
bit-exact optimization waves + one GPU wave landed with twin-dump/pixel-identity proof standards;
NaN-self-heal, click-free-transition and latency contracts are tested and ADR-recorded. CI rigor is
unusually high for pre-1.0: 3-OS matrix, blocking pluginval strictness 10 in both modes ×3, symbol
retain-then-strip pipeline (ADR-0021), fail-closed artifact gating, CodeQL + MSVC analyze +
dependency-review + dependabot. The policy/ADR system is real and self-consistent (16/16 Accepted
ADRs match the index; authority order enforced).

**Weaknesses.** (1) **Compatibility automation is the big gap**: the only automated state test is
the A/B-index clamp; `getState→setState` round-trip, parameter-registry snapshot diff, the three
legacy migration read paths (v0.2 bare APVTS, pre-0.6.4 A/B, pre-0.8.4 view params) and preset
save/load have **zero** automated coverage while COMPATIBILITY_POLICY makes them the project's
highest-authority contract. (2) **Distribution engineering is unstarted**: no release pipeline/tags
(RISK-003), ad-hoc macOS signing only (KI-002), no Authenticode, no installers (KI-005), no
LICENSE/EULA anywhere, no user manual (docs are 100 % developer-facing). (3) The **host matrix is
Unverified** (KI-004): no recorded DAW evidence; AU builds in CI but is never `auval`'d or
host-tested. (4) Maintenance watch items: `PluginEditor.cpp` (1843 L) and `SpectrumImager.cpp`
(1780 L) are approaching the 2000-line risk threshold — watch, don't restructure preemptively.

**Remaining risks.** RH-R1/R2/R3/R5 (licensing/notarization/Authenticode/installers — all
human-gated business decisions, ADR-0016..0020 reserved); RISK-002 (multiband CPU on low-power
hosts — formal budget numbers still TODO); KI-013 (new this audit: the v0.8.12 release-outside
reconcile is inert on macOS — JUCE platform limitation, Low); the v0.8.12 **Level-5 manual
audition is not yet recorded** (the one open RELEASE_POLICY precondition); supply-chain soft spots:
pluginval fetched from `releases/latest` (unpinned) inside the release gate, JUCE pinned by tag
(mutable) rather than commit SHA; macOS crash symbolication currently degrades under Release+LTO
(dSYM gap, needs `-Wl,-object_path_lto` — recorded RH-PR-3 candidate).

## 3. Documentation audit result (fixed in this PR)

All findings below were verified against git/current source before editing; corrections were the
smallest possible and preserve historical statements (period-correct figures got a "then-current"
qualifier instead of a rewrite).

| File | Drift found → fix applied |
|---|---|
| CHANGELOG.md | `[0.8.12]` dated 2026-07-21 but two of its fixes landed 2026-07-22 → re-dated; "MultiBand"/"Bandwidth" (6 lines) → registry terms "Multiband"/"Width" |
| docs/HANDOVER.md | Snapshot-HEAD ledger frozen at `c605fbe` (0.8.7-era) → `64e87c4`; "two items" vs three listed; Build Status frozen at v0.8.11/136 checks → v0.8.12/140; Release Status missing v0.8.12 (RELEASE_POLICY precondition 6 was in violation) → added, with the unrecorded Level-5 audition noted; RH-PR-2 still "recommended" though shipped → marked shipped (PR #63, ADR-0021); `CMakeLists.txt:89` → `:146`; `scratchpad/xbench.cpp` qualified as session-local |
| docs/KNOWN_ISSUES.md | Header version-sync two releases stale (v0.8.10) → re-synced to v0.8.12; KI-007 "pending CI" → CI-confirmed; dead line cites in KI-001/002/003/006/009 → refreshed; KI-012 evidence cite → `[0.8.10] + [0.8.11]`; **KI-013 added** (macOS-inert reconcile, Low, external) |
| docs/FUTURE_RISKS.md | Header stale (v0.8.10) → re-synced; RISK-002 mitigation listed the SoloMonitor settled-skip as future work though it shipped (H1 0.8.9 + Wave 3, Test 33) → marked shipped; "It is unprofiled" → accurate statement (session-local Wave-3/4/5 measurements exist; formal budgets TODO); dead cites in RISK-002/004 → refreshed |
| docs/POSTMORTEMS.md | Dead line cites in INC-003/004/006/007/009 (code moved under Waves 1–6 / RH-PR-2) → refreshed; SHAs/dates/tests all verified correct |
| docs/REPOSITORY_MAP.md | "23 headless DSP acceptance tests" → 33; missing rows for `FrameClock.h` + `LR4Xover.h` → added; stale CMake cite → refreshed |
| docs/DOCUMENTATION_COVERAGE.md | **PR #80 never updated this file** (lifecycle slip) → retroactive entry added; worklogs self-coverage row listed only release-hardening/ → now lists performance/, the v0.8.12 fix records and this file |
| README.md | Validation-gate scope understated ("headless Linux") → "both modes ×3, blocking on all three CI platforms" |
| docs/procedures/CI_CD.md | `checkout@v5`/`upload-artifact@v5` → `@v7` (matches build.yml) |
| docs/policies/DEPENDENCY_POLICY.md | `JUCE_*` flags cite `:123-132` → `:183-188`; compliance-log "23 DSP self-tests" → "then-current 23" (historically correct figure, qualified) |
| docs/procedures/PACKAGING.md · docs/architecture/COMPATIBILITY_MATRIX.md | Stale CMakeLists line cites for identifiers/formats → `:137-158` block |
| docs/architecture/design-decisions/ADR_INDEX.md | "130 self-tests" → "then-current 130-check suite" (checks ≠ tests; ADR-0021 body already said "130 checks" and is untouched — ADRs are append-only) |
| worklogs/BANDWIDTH_DRAG_FIX_v0.8.12.md · MOUSE_RELEASE_STATE_FIX_v0.8.12.md | "MultiBand" → "Multiband" (7 lines; content untouched); ▸ MOUSE_RELEASE date 2026-07-21 → 2026-07-22 (its commits are `9ff597b`/`6777a69`, both 2026-07-22 — the same evidence behind the CHANGELOG re-date) |
| ▸ docs/procedures/BUILD.md | Five stale CMakeLists cites (tests/Standalone options, build-number, formats, `JUCE_*` block) → `:27,207` / `:28,141-143` / `:178` / `:137-143` / `:180-189` |
| ▸ docs/policies/TESTING_POLICY.md · docs/policies/CODE_STYLE.md | Warning-flags cite `:143,165` → `:201,225` |
| ▸ docs/procedures/TROUBLESHOOTING.md | INTERFACE-lib cite `:54-73` → `:110-120` |
| ▸ docs/procedures/RELEASE_PROCESS.md | Version-block cite → `:14,176-182`; "`docs/CHANGELOG.md`" → root `CHANGELOG.md` |
| ▸ docs/procedures/TESTING.md | Retry-cap cite `run-pluginval.sh:75` → `:70-90` (`run_one_pass`) |
| ▸ docs/architecture/PERFORMANCE_BUDGET.md | GUI-redraw row had no Wave-6 (0.8.12) record — the release's only perf change — → one-sentence addition + `[0.8.12]`/WAVE6 evidence refs |
| ▸ docs/architecture/RELEASE_HARDENING_PLAN.md | "full 136-check suite" → "then-current 136-check" qualifier (suite is 140 today) |

**Reported, not fixed (needs owner/decision):** (1) the v0.8.12 Level-5 audition record; (2) a
**retroactive ADR for the 0.8.10 multiband flat-recombination change** — it audibly changed the
multiband summation maths (removed a −17.75 dB dip) yet has no ADR, and ADR-0015 presupposes "flat
recombination preserved" as a constraint no record decided; ADR_POLICY's "DSP algorithm
replacement" trigger arguably applies. Recommended as a small 0.8.13 docs task, not silently done
here (writing an ADR is a decision record, not a drift fix). (3) Optionally one lightweight ADR
for the GUI rendering architecture (FrameClock cadence contract + static-layer/N2 opacity rules) —
not policy-mandated (no GUI trigger in ADR_POLICY), rationale currently lives in header comments
and worklogs. (4) Terminology: `CorrelationMeter.{h,cpp}` contains GUI class `StereoMeter` while
`src/dsp/Correlation.h` contains a *different* class `CorrelationMeter`; "Spectrum
analyser/Imager/spectral editor" alias for one component. Renames are code churn — recorded here
as a naming decision for the maintainer, not applied.

## 4. Deferred-item review (every open candidate)

| Item | Verdict | Reason |
|---|---|---|
| W5-A lat==0 mix-ring round-trip | **Defer** | Low-single-digit % of the transparent floor vs the highest blast radius proposed in Wave 5 (restructures the H9/H4/KI-1 invariant region). Revisit only on evidence of real small-buffer host pain; the fix sketch survives only as the Wave-5/final-pass summaries. |
| W5-D K-weighting SIMD | **Defer (coupled to an AVX2 ADR)** | Prototype was bit-exact but only 1.10× under the frozen SSE2 flags (~0.5–1 % of floor). Worth it only with `-march=haswell`+ — a numerics-frozen build-contract change (own ADR + Architecture Review, FMA divergence re-check). Do not do standalone. |
| W3-10 Width==1 identity gate | **Abandon** (unless a Class-B tier is ever adopted) | Measured NOT bit-exact: 15.5 % of samples differ (~1 ULP) — breaks the program's Class-A standard for a micro win on an already-vectorised loop. |
| W3-12 residue: bypass read-back segmentation | **Abandon** | Correctness hazard (read/write overlap when `readLat < n`); permanent unless the ring is restructured. |
| W3-7 LoudnessMatch gate / W3-8 LevelMeters gate / H7 | **Defer — Category C, maintainer Architecture Review** | All change live product contracts (Measure-readout freshness; held peaks persisting while the editor is closed). Correct queue position: one consolidated Review if idle-CPU ever matters commercially. |
| Multiband LR4 SIMD · OSD-2 (OS-only) | **Defer** | Named in the final-pass queue behind the Category-C review; no elaboration/measurement exists. Only worth opening alongside the AVX2 decision (LR4) or an OS-path profiling case (OSD-2). |
| LookAndFeel per-paint `Path`/`Font` locals | **Abandon** | Gesture-transient only; caching in a shared stateless LookAndFeel needs per-component keying — complexity > benefit. (Final-pass verdict; unchanged.) |
| Crossover glide speed (< 0.5 s ask) | **Remains escalated — product decision** | Pareto-proven: every in-architecture lever violates Test 29 / ADR-0015 / spur bounds. Only levers: amend ADR-0015 (`4.8 oct/s, kRateRefHz=360` → ~13 % faster, ~17.7 c, "not recommended") or linear-phase H2 (latency change → ADR + PDC rework). Both Hard Stops. |
| Linear-phase H2 crossovers | **Defer (product decision)** | "Deferred, not rejected" in ADR-0015; +30–80 ms reported latency; the only path to both fast dividers and zero FM. A 0.9.x candidate *only* if the maintainer wants it as a user-selectable mode. |
| Wave-6 GPU rejects (opaque imager, ε idle gate, bottomLayer rebuild, sub-rect repaint, GL config, FPS cap) | **Closed/Abandon** | Each proven not-pixel-identical, a behaviour change, or interaction-transient. The render frontier is reached; do not reopen without a user-visible complaint + measurement. |
| macOS dSYM under Release+LTO (`-object_path_lto`) | **Do in 0.8.13** | Accepted RH-PR-2 consequence, recorded RH-PR-3 candidate; CI-only, small, restores crash symbolication for supporting real users. |
| RH-PR-8 tags + release.yml | **Do in 0.8.13** | Planned, unblocked, closes RISK-003/RH-R6; prerequisite for every later distribution step. |
| RH-PR-3/4/5/5b/6/7/9 (notarize, licensing, Authenticode, installers, QA matrix) | **Defer to pre-1.0 phase — human-gated** | Blocked on Apple Developer Program / cert procurement / business decisions + ADR-0016..0020. Engineering should stage the groundwork (RH-PR-8) but cannot start these unilaterally. |
| KI-011 tooltip hardware re-test · KI-009 REAPER focus · KI-010 typed-entry undo | **0.9.x backlog** | Small, real UX items; KI-009/KI-010 need investigation/design (undo semantics), KI-011 needs hardware. |
| PERFORMANCE_BUDGET numeric TODOs | **Do in 0.9.x (procedure + numbers)** | Deliberate C2 placeholders; fill from a documented repeatable measurement procedure, not container numbers. |

## 5. Recommended roadmap

### Phase 1 — 0.8.13 "Compatibility hardening" (immediate; all unblocked, low risk)

1. **State-serialization & parameter-compatibility regression harness** — Priority **P0** ·
   Impact: protects every future release against the highest-authority contract in the repo ·
   Risk: none to product (test-only; needs a second console target linking the processor, since
   `AnamorphTests` links only the DSP core + JUCE modules, not the plugin/processor objects) ·
   Scope: **medium**. Contents: `getState→setState`
   round-trip (APVTS + InternalState + A/B slots + `raw` attrs), parameter-registry snapshot diff
   (IDs, ranges, defaults, order, automatable flags — fails on any unrecorded change), versioned
   session fixtures for the three legacy read paths, preset save/load round-trip, wired into CI as
   a blocking step. Converts RELEASE_COMPATIBILITY_CHECKLIST items 1–3 from manual to automated
   **before** 1.0 freezes the schema.
2. **RH-PR-8: first git tag (v0.8.12) + tag-triggered `release.yml` skeleton** — P1 · closes
   RISK-003/RH-R6; reuses the existing build/stage steps; artifacts + checksums on tag · Risk: CI-only
   · Scope: small.
3. **Supply-chain pins** — P1 · pin pluginval to an exact release (it is a *blocking release gate*
   fetched from `releases/latest`), pin JUCE by commit SHA (tag is mutable; DEPENDENCY_POLICY
   intent) · Risk: none · Scope: small.
4. **macOS dSYM restoration** (`-Wl,-object_path_lto`) — P2 · Risk: CI-only · Scope: small.
5. **Docs decisions**: retroactive ADR for multiband flat recombination; optional GUI-rendering ADR;
   record the v0.8.12 Level-5 audition once performed — P2 · Scope: small.
6. **CI hygiene (optional)**: concurrency-cancel groups; ccache — P3 · Scope: small.

### Phase 2 — 0.9.x "Host reality & product surface"

1. **DAW host-matrix verification program** — P0 · KI-004; define the evidence procedure
   (host/version/OS, session reload, automation playback, latency check) and populate
   COMPATIBILITY_MATRIX from real DAWs; add `auval` to the macOS job and decide the AU support
   claim · Risk: none (verification) · Scope: medium, partly human/hardware-gated.
2. **User manual / quickstart** — P0 for a commercial product · Scope: medium.
3. **Factory preset bank + golden-audio regression** — P1 · product value + a compat guard that
   locks the sound of shipped presets · Scope: medium.
4. **UX backlog**: KI-009 REAPER focus, KI-010 typed-entry undo, KI-011 re-test — P2 · small each.
5. **PERFORMANCE_BUDGET numbers** via a documented repeatable procedure — P2 · small/medium.
6. **Deferred-DSP gate**: only reopen W5-A / AVX2+W5-D / Category-C gates on measured evidence
   (a real host complaint or budget breach) — default is *no* further optimization work.
7. (Product-decision option) linear-phase crossover mode — only if the maintainer wants the fast-glide
   capability; full ADR + PDC/dry-align rework + latency-change gate.

### Phase 3 — pre-1.0 "Commercial readiness" (human-gated program, order per RELEASE_HARDENING_PLAN)

RH-PR-3 (Developer ID + hardened runtime + notarization + stapling, ADR-0019) → RH-PR-5/5b
(Authenticode + Windows installer) → RH-PR-6 (macOS .pkg) → RH-PR-4/7 (licensing core + GUI
integration, ADR-0016/0017, unlicensed = true-bypass via ADR-0004 machinery) → RH-PR-9 (update
notice + QA matrix). Plus: LICENSE/EULA files + JUCE commercial-license compliance note; support-matrix
declaration; schema freeze (kVersion audit + full checklist run against fixtures from every released
0.8.x); crash-reporter decision (explicitly Phase-2/optional); 1.0 Level-5 audition + tagged release.

## 6. Recommended next task

**"Anamorph v0.8.13 — State-serialization & parameter-compatibility regression harness."**
First because: it guards the project's highest-authority contract (COMPATIBILITY_POLICY) exactly
where audit found the largest gap between policy and automation; it is fully headless (fits the
sandbox + CI, unlike host/signing work); it has zero product-behaviour risk; and it must exist
*before* 1.0 freezes the schema — every later phase (presets, licensing state, installers) builds
on serialized state being provably stable. Suggested scope: a `tests/state_tests.cpp` console
target linking the plugin objects, fixtures under `tests/fixtures/` for v0.2 / pre-0.6.4 /
pre-0.8.4 / current blobs, a parameter-registry snapshot file with a diff test, preset round-trip,
CI wiring as a blocking step, plus the RELEASE_COMPATIBILITY_CHECKLIST cross-references.
