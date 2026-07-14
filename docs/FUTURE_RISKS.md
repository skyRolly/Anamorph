# FUTURE_RISKS.md

Potential technical risks. Each is evidence-based (constraint C7) — no invented risks. ADRs and
postmortems may reference these IDs to close the loop. Severity: Low / Medium / High / Critical.

Verified against repository HEAD `c605fbe` (0.8.7 content audit); version-synced to the
**v0.8.10 release** (finalized 2026-07-14, PR #59 — undo/redo forced-duck dry-fill, multiband
flat recombination, adaptive `FrameClock` GUI refresh — introduces no new risk: the engine fixes
are behaviour-preserving (single swaps byte-identical) or a documented magnitude correction
(multiband), `FrameClock` is a message-thread GUI change, and the multiband allpass adds a known
CPU cost tracked in PERFORMANCE_BUDGET, not an open risk). Prior: the v0.8.9 release (finalized
2026-07-12, PR #58 — Wave-2 performance work introduces no new risk: H6 replaces the crossover
filter with a bit-exact local clone, H15 adds two generation counters following the existing
sanctioned staleness-hint pattern, H3/H4/H11 are bounded Class-B changes); before that PR #56
(JUCE 8.0.14) and 0.8.8 (PR #54).

| ID | Risk | Severity | Likelihood |
|---|---|---|---|
| RISK-001 | JUCE version bump silently changes DSP/latency/editor behaviour | High | Medium |
| RISK-002 | Always-on monitor/crossover banks + per-sample coeff recompute → CPU | Medium | Medium |
| RISK-003 | No git release tags → fragile version/CHANGELOG attribution | Low | High (already true) |
| RISK-004 | pluginval signal-only retry could mask a real future editor crash | Medium | Low |
| RISK-005 | Manual-only audio/visual + host validation lets regressions ship green | Medium | Medium |

---

## RISK-001 — JUCE version bump
- **Risk:** JUCE is pinned to exactly `8.0.14`. A future bump can silently change DSP behaviour
  (oversampling, Linkwitz-Riley filters, `dsp::AudioBlock`), reported latency, the parameter/state
  ABI, and the X11 editor-embedding path (the INC-006 crash lives in JUCE's host code).
- **Impact:** Audible DSP/latency drift, session/automation incompatibility, or a returning editor
  crash — none of which the headless gate fully catches.
- **Likelihood (evidence-based):** Medium — dependencies eventually need security/feature updates;
  the pin defers but does not eliminate this.
- **Evidence [Verified]:** CMakeLists.txt:33 (exact tag); ADR-0011 (X11 in JUCE); `docs/policies/DEPENDENCY_POLICY.md`.
- **Mitigation:** Treat any bump as a Build System change → ADR + Architecture Review; run full DSP
  tests + pluginval (3 OSes) + a manual audition + the RELEASE_COMPATIBILITY_CHECKLIST after.

## RISK-002 — Always-on banks / crossover-move cost (CPU)
- **Risk:** `SoloMonitor` runs every block even with multiband off and no solo (INC-009 invariant;
  since 0.8.9/H1 the settled passthrough goes cold, shrinking this), and `MonoMaker` still
  recomputes Linkwitz-Riley coefficients **per sample** while its cutoff glides. Since 0.8.10,
  `MultibandWidth`/`SoloMonitor` crossover moves instead run **two fixed-coefficient banks for the
  ~12 ms crossfade** (2× the stage's filter ticks while fading, chained during a sustained
  split-frequency automation ramp) with coefficients recomputed once per fade. Under heavy
  multiband automation or on low-power hosts this could be a hot path. It is unprofiled.
- **Impact:** Higher-than-necessary CPU in Simple mode and CPU spikes during fast split automation.
- **Likelihood (evidence-based):** Medium — the cost is real and constant; whether it matters
  depends on host/SR/buffer, which are unmeasured.
- **Evidence [Verified]:** src/dsp/AnamorphEngine.cpp:845; src/dsp/MultibandWidth.cpp (fade path);
  Devin PR #50 review (efficiency note); `docs/architecture/PERFORMANCE_BUDGET.md` (TODOs).
- **Mitigation:** Profile (PERFORMANCE_BUDGET TODO); consider skipping the SoloMonitor filters when
  settled at `passGain==1` (a DSP change → ADR + Review). Correctness is unaffected either way.

## RISK-003 — No git release tags
- **Risk:** The repository has no tags, so version/CHANGELOG attribution relies on commit messages.
  Reconstruction is error-prone and cannot be Verified to a release artifact.
- **Impact:** CHANGELOG entries for older versions stay Partially Verified / reconstructed; harder to
  reproduce a specific shipped build.
- **Likelihood (evidence-based):** High — already the case (`git tag` is empty).
- **Evidence [Verified]:** `git tag` empty; `docs/policies/CHANGELOG_POLICY.md`; `docs/procedures/RELEASE_PROCESS.md` (TODO).
- **Mitigation:** Adopt annotated release tags going forward; until then, cite commit SHAs.

## RISK-004 — pluginval signal-only retry masking a real crash
- **Risk:** `run-pluginval.sh` retries on any signal-crash to absorb the external X11 flake
  (INC-006/KI-003). A genuine *new* editor crash that also exits with a signal could be retried away
  and pass on a later attempt, hiding a real defect.
- **Impact:** A real crash regression could ship if it happens to pass on retry.
- **Likelihood (evidence-based):** Low — retries are capped at 3 and a deterministic crash still
  fails all attempts.
- **Evidence [Verified]:** scripts/run-pluginval.sh:46-76 (retry only on exit ≥128, cap 3).
- **Mitigation:** Investigate any repeated crash rather than trusting the pass; keep the cap; a real
  assertion (exit <128) already fails immediately with no retry.

## RISK-005 — Manual-only audio/visual + host validation
- **Risk:** Audio quality, GUI/vectorscope appearance, and real-DAW host behaviour cannot be verified
  headlessly; a green build + pluginval pass is "ready to audition," not final.
- **Impact:** A sound/visual or host-specific regression can pass CI and reach testers.
- **Likelihood (evidence-based):** Medium — depends on diligence of the manual Level-5 sign-off.
- **Evidence [Verified]:** docs/procedures/TESTING.md ("What cannot be verified headlessly"); `docs/policies/TESTING_POLICY.md` (Level 5);
  `docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md` (host-matrix item).
- **Mitigation:** Enforce the manual audition + host-matrix line items at release; expand the
  documented host coverage as it is performed.

---

## Adding a risk

Create the next `RISK-NNN` only when a TODO/FIXME, issue, PR discussion, or concrete code limitation
supports it. State the likelihood **basis**, cite evidence with a confidence level, and give a
mitigation. Do not invent risks to fill the template.
