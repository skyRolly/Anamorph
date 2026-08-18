# FUTURE_RISKS.md

Potential technical risks. Each is evidence-based (constraint C7) — no invented risks. ADRs and
postmortems may reference these IDs to close the loop. Severity: Low / Medium / High / Critical.

Version-synced to **v0.9.4** (the JUCE 9.0.0 → 9.0.1 dependency upgrade, ADR-0026 — **no new
risk**: RISK-001 is the risk this change is an instance of, and its mitigation was executed in
full (twin-dump bit-identity, both suites, pluginval strictness 10 in both modes, identical
warning set); no source, no build dependency, no serialized state, parameter or DSP behaviour
changed, and no new limitation appeared. The same version also carries the **C++ standard 17 → 23**
migration (ADR-0027) — likewise **no new risk entry**: its one `src/` change is an added
`#include`, engine output is bit-identical C++17 vs C++23, and its single open caveat (MSVC has no
stable `/std:c++23`, so CMake requests `/std:c++latest`) is carried in ADR-0027 §Consequences with
its escape hatches. The same version also moves the macOS CI job off the **deprecated** `macos-14`
image to `macos-latest` — likewise **no new risk entry**: the floating label is what the other two
jobs already use, its toolchain-drift exposure is the same shape as ADR-0027's MSVC
`/std:c++latest` caveat, and it is recorded with the measured compiler move in
`docs/procedures/CI_CD.md`. The four `-Wimplicit-int-float-conversion` diagnostics that move
surfaced are **fixed** — an explicit `(float)` cast per site, with the three translation units
verified to compile to byte-identical machine code, so no risk attaches to them either.
RISK-003's mitigation now names **v0.9.4** as the first
tag — v0.9.3 was written up but, like 0.9.0-0.9.2 before it, never cut). Prior sync: **v0.9.3** (six GUI interaction fixes plus an equal-width Widen row: the Multiband add-split preview line, the
unified pop-up dismissal shield, pop-up lifetime across a hidden editor / background application,
menu width, disabled menu items and the Tooltips on/off transition
— **no new risk**: editor-only, with no serialized state, parameter or DSP behaviour changed. The three
new limitations — KI-018, KI-019 and KI-020 — are known *issues* and live in `KNOWN_ISSUES.md`, not
here.
RISK-003's mitigation now names **v0.9.3** as the first tag). Prior sync: **v0.9.2**
(preset drop-down lifetime/crash fix, factory-preset identity, the `UI Scale` label and the installer
component titles — no new risk; the one new limitation is an OS text-input behaviour filed as
KI-017). Prior: **v0.9.1**
(manufacturer-code change, ADR-0023 — no new risk: RISK-003's
mitigation then named v0.9.1 as the first tag, and the one-time session break is a documented
known issue (KI-016), not a forward-looking risk). Prior sync: **v0.9.0** (release-prep,
2026-07-24, PR #87 — packaging/installers + user
docs + version bump — and PR #89, the installer/packaging rework: component selection, system-wide
installs, flat ZIP-only artifacts; no DSP/GUI code change in either, no new risk; the unsigned
installers inherit the existing signing/notarization gap already tracked as RH-PR-3/5). Previously verified against
repository HEAD `64e87c4` (post-v0.8.12 content re-audit), synced to the
**v0.8.12 release** (changelog-dated 2026-07-22, PR #79 performance Wave 6 + PR #80 GUI interaction
fixes — pixel-identical / message-thread-only, no new risk; the **v0.8.11 release** of 2026-07-20
likewise introduced none: PRs #60/#61 — the ADR-0015 crossover-follower fixes, behaviour-changing
by design with the trade tracked as KI-012; PRs #62/#76 — Class-A performance Waves 3–5,
twin-dump validated; PR #63 — RH-PR-2 build hardening, byte-exact). Prior sync: the **v0.8.10 release**
(finalized 2026-07-14, PR #59 — undo/redo forced-duck dry-fill, multiband
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
| RISK-006 | Undeclared licensing: no `LICENSE`/EULA, and the commercial JUCE licence required by the closed-source model is not yet obtained | High | High (already true) |

---

## RISK-001 — JUCE version bump
- **Risk:** JUCE is pinned to exactly `9.0.1` (immutable commit `e18f7f5…`, ADR-0026; previously
  `9.0.0` = `f8f8864…`, ADR-0022; before that
  tag `8.0.14`, ADR-0012). A future bump can silently change DSP behaviour (oversampling,
  Linkwitz-Riley filters, `dsp::AudioBlock`), reported latency, the parameter/state ABI, and the
  X11 editor-embedding path (the INC-006 crash lives in JUCE's host code).
- **Impact:** Audible DSP/latency drift, session/automation incompatibility, or a returning editor
  crash — none of which the headless gate fully catches.
- **Likelihood (evidence-based):** Medium — dependencies eventually need security/feature updates;
  the pin defers but does not eliminate this. The SHA pin (v0.8.13 cycle) additionally removes the
  re-pointed-tag variant of the risk.
- **Evidence [Verified]:** CMakeLists.txt:52-54 (exact commit); ADR-0011 (X11 in JUCE); `docs/policies/DEPENDENCY_POLICY.md`.
- **Mitigation:** Treat any bump as a Build System change → ADR + Architecture Review; run full DSP
  tests + pluginval (3 OSes) + a manual audition + the RELEASE_COMPATIBILITY_CHECKLIST after. The
  8.0.14→9.0.0 bump additionally proved engine output **bit-identical** via a 32-scenario twin
  dump (ADR-0022) — the pattern to repeat on future bumps, and it **was** repeated for
  9.0.0→9.0.1 (ADR-0026: 32/32 hashes and latencies identical, warning set byte-identical across
  the 18 project translation units).

## RISK-002 — Always-on banks / crossover-move cost (CPU)
- **Risk:** `SoloMonitor` runs every block even with multiband off and no solo (INC-009 invariant;
  since 0.8.9/H1 the settled passthrough goes cold, shrinking this), and `MonoMaker`,
  `MultibandWidth` and `SoloMonitor` recompute Linkwitz-Riley coefficients **per sample** while a
  cutoff glides (since 0.8.10 the multiband/solo cutoffs track under the frequency-proportional
  R(f) = 4·max(1, f/300) oct/s cap — ADR-0015 final + slow-drag fix — so the per-sample
  recompute lasts as long as the drag plus ≤ ~1 s of worst-case catch-up; a discrete step
  instead runs **two banks for one ~12 ms crossfade**, 2× the stage's filter ticks). Under heavy multiband automation or on low-power hosts this could be a
  hot path. Formal budget numbers are not yet committed (session-local Wave-3/4/5 callgrind
  measurements exist — drag scenarios −35…−50 % after Wave 3; see PERFORMANCE_BUDGET).
- **Impact:** Higher-than-necessary CPU in Simple mode and CPU spikes during fast split automation.
- **Likelihood (evidence-based):** Medium — the cost is real and constant; whether it matters
  depends on host/SR/buffer, which are unmeasured.
- **Evidence [Verified]:** src/dsp/AnamorphEngine.cpp:1254 (`soloMonitor.process`, always-on); src/dsp/MultibandWidth.cpp (glide + fade paths);
  Devin PR #50 review (efficiency note); `docs/architecture/PERFORMANCE_BUDGET.md` (TODOs).
- **Mitigation:** Formal profiling (PERFORMANCE_BUDGET numeric budgets remain TODO). The SoloMonitor
  settled-skip **shipped**: H1 (0.8.9) plus the Wave-3 gains-only cold gate, guarded by Test 33
  (`testSoloColdThroughDrag`) — the settled passthrough now goes fully cold. Correctness is
  unaffected either way.

## RISK-003 — No git release tags
- **Risk:** The repository has no tags, so version/CHANGELOG attribution relies on commit messages.
  Reconstruction is error-prone and cannot be Verified to a release artifact.
- **Impact:** CHANGELOG entries for older versions stay Partially Verified / reconstructed; harder to
  reproduce a specific shipped build.
- **Likelihood (evidence-based):** High — already the case (`git tag` is empty).
- **Evidence [Verified]:** `git tag` empty; `docs/policies/CHANGELOG_POLICY.md`; `docs/procedures/RELEASE_PROCESS.md`.
- **Mitigation:** **Infrastructure shipped (RH-PR-8, v0.8.13 cycle):** annotated `vX.Y.Z` tag
  convention + tag-triggered `release.yml` (fail-closed tag⇄version⇄CHANGELOG validation →
  reused `build.yml` gates → draft GitHub Release with versioned artifacts + SHA-256 sums +
  manifest). The risk **closes when the first release tag is cut** (planned: **v0.9.4** — 0.9.0, 0.9.1, 0.9.2 and 0.9.3 were each written up but never tagged); until
  then, cite commit SHAs. Historical entries keep SHA evidence permanently.

## RISK-004 — pluginval signal-only retry masking a real crash
- **Risk:** `run-pluginval.sh` retries on a signal-crash to absorb the external X11 flake
  (INC-006/KI-003). A genuine *new* editor crash that also exits with a signal could be retried away
  and pass on a later attempt, hiding a real defect.
- **Impact:** A real crash regression could ship if it happens to pass on retry.
- **Likelihood (evidence-based):** Low, and **lower since 2026-08-18** — the retry is now scoped by
  `uname -s` to the platform its justification names, so macOS gets exactly one attempt and this risk
  no longer applies there at all. On Linux retries stay capped at 3 and a deterministic crash still
  fails all attempts.
- **Evidence [Verified]:** scripts/run-pluginval.sh:147-197 (`run_one_pass`; retry only on exit ≥128, cap 3).
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

## RISK-006 — Undeclared licensing (no LICENSE, no approved EULA, JUCE tier unchosen)
- **Risk:** The repository root has **no `LICENSE` file** and neither installer presents an
  end-user agreement, so the terms under which Anamorph's own source and binaries are offered are
  undeclared. `EULA.md` is an **unapproved draft** (not in force, not shipped) and does not
  change that. The stated product model (owner, 2026-07-26) is **closed-source commercial**; JUCE 9
  modules are dual-licensed **AGPLv3 or commercial**, and a closed-source distribution cannot use
  the AGPLv3 arm — the commercial JUCE tier must be in place before commercial distribution. A
  third strand —
  the Steinberg VST 3 trademark/distribution review — is separate again (the SDK *code* is MIT in
  JUCE 9.0.1; the VST name and plug-in distribution terms are not covered by that grant).
- **Impact:** Blocks a commercial release outright, and leaves even a free release legally
  ambiguous for anyone who downloads, redistributes or contributes. Third-party **attribution**
  is a different obligation and is already discharged (`NOTICE` + `THIRD_PARTY_LICENSES.md`
  accompany every download as release-page assets, which since 2026-07-26 carry the IJG
  acknowledgement on their own) — this risk is specifically about Anamorph's *own* terms.
- **Likelihood (evidence-based):** High — already the case (`ls` shows no `LICENSE`/`COPYING`).
- **Evidence [Verified]:** repository root (no licence file); `THIRD_PARTY_LICENSES.md`
  §"Open licensing decisions"; the pinned JUCE tree's `LICENSE.md` (dual licence);
  `docs/KNOWN_ISSUES.md` KI-015.
- **Mitigation:** **None available to engineering** — this is an owner/legal decision, tracked as
  RH-R11 / RH-F1 (and RH-F2 for Steinberg) in `docs/architecture/RELEASE_HARDENING_PLAN.md` and
  indexed with the other open decisions in `docs/COMMERCIAL_STATUS.md` §4. It
  closes when the commercial JUCE licence is obtained and a `LICENSE` (plus an EULA, if the
  product is sold) is added. Until then, cite this risk rather than assuming any particular
  terms.
