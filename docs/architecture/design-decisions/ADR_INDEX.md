# ADR Index

Mandatory registry of all Architecture Decision Records. An ADR not listed here is invalid.
ADRs are created **only** when supported by repository evidence (constraint C1); the set below
is the evidence-confirmed result, not a predefined quota. New decisions append the next number.

Status values: Proposed · Accepted · Deprecated · Superseded.

| ID | Title | Status | Evidence confidence |
|---|---|---|---|
| [ADR-0001](ADR-0001-format-agnostic-dsp-core.md) | Format-agnostic DSP core via `EngineParameters` POD | Accepted | Verified |
| [ADR-0002](ADR-0002-parameter-id-versioning.md) | Parameter ID versioning & immutability (`kVersion`) | Accepted | Verified |
| [ADR-0003](ADR-0003-oversampling-strategy.md) | Oversampling wraps nonlinear stages only; minimum-phase IIR; exact PDC | Accepted | Verified |
| [ADR-0004](ADR-0004-clickfree-transition-strategy.md) | Click-free transition strategy (duck / crossfade / warm monitor) | Accepted | Verified |
| [ADR-0005](ADR-0005-phase-matched-dry-reconstruction.md) | Phase-matched dry reconstruction `A(dry)` for the Multiband Mix | Accepted | Verified |
| [ADR-0006](ADR-0006-strict-serial-signal-chain.md) | Strict serial chain: Mono Maker post-Mix, Band Solo post-everything | Accepted | Verified (code) / Partially Verified (history) |
| [ADR-0007](ADR-0007-levelmatch-measure-predict.md) | Level Match = BS.1770 Measure + absolute Predict | Accepted | Verified |
| [ADR-0008](ADR-0008-custom-per-ab-undo.md) | Custom per-A/B-slot Undo/Redo (replaces JUCE UndoManager) | Accepted | Verified |
| [ADR-0009](ADR-0009-nan-selfheal-nyquist-clamp.md) | Crossover Nyquist clamp + engine-wide NaN/Inf self-heal; no output clipper | Accepted | Verified |
| [ADR-0010](ADR-0010-host-hidden-internalstate.md) | Host-hidden `InternalState` for non-musical parameters | Accepted | Verified (code) / Partially Verified (history) |
| [ADR-0011](ADR-0011-linux-x11-cpu-render.md) | Linux/X11 CPU rendering — no OpenGL attach | Accepted | Verified (code) / Partially Verified (history) |
| [ADR-0012](ADR-0012-juce-8.0.14-upgrade.md) | JUCE dependency upgrade 8.0.8 → 8.0.14 | Accepted | Verified (CI build + the then-current 23 tests + pluginval) |
| [ADR-0013](ADR-0013-raw-normalised-serialization-attribute.md) | Additive `raw` normalised value attribute (exact discrete-param state round-trip) | Accepted | Verified (CI `--randomise` state restoration) |
| [ADR-0014](ADR-0014-multiband-bands-solo-automatable.md) | Expose `mbBands`/`mbSolo` to host automation (remove `withAutomatable(false)`) | Accepted | Verified (code) |
| [ADR-0015](ADR-0015-split-drag-zero-latency-follower.md) | Split-movement transitions: zero-latency LR4 retained, rate-capped follower — v0.8.10 final + slow-drag fix: slew-limited smoother under R(f) = 4·max(1, f/300) oct/s, controlled FM over latency (full A–H3 investigation history) | Accepted | Verified (measurements + code + Test 29) |
| [ADR-0021](ADR-0021-build-hardening-strategy.md) | Build Hardening Strategy (RH-PR-2): retain-then-strip symbol pipeline + separate debug artifacts, full RELRO/CFG/stack-protector pinned, artifact/signing failure hygiene; numerics-affecting flags frozen (0016–0020 reserved by RELEASE_HARDENING_PLAN §8) | Accepted | Verified (twin-dump byte-exact + the then-current 130-check suite + binary audit; Windows/macOS steps confirmed by CI) |
| [ADR-0022](ADR-0022-juce-9.0.0-upgrade-sha-pin.md) | JUCE dependency upgrade 8.0.14 → 9.0.0 + immutable-commit (SHA) pinning; Linux EGL build dep | **Accepted** (Architecture Review + Level-5 audition signed off 2026-08-15, against the 9.0.1 build that succeeded this pin) | Verified headlessly (32-scenario twin-dump bit-identical incl. latencies; 140 + 774 suites green; registry snapshot unchanged) / Verified — manual (Level-5 audition; not headlessly reproducible) |
| [ADR-0023](ADR-0023-vendor-manufacturer-code.md) | Vendor manufacturer code `Anmf` → `RTec` (product-line identity; AU component + VST3 UID change, pre-0.9.1 sessions report the plug-in as missing — KI-016); also adds the plugin-identity carve-out to `COMPATIBILITY_POLICY` exception condition 2 | **Accepted** (Architecture Review + Level-5 identity check signed off 2026-07-30) | Verified (code) / Verified — manual (host registration + `auval -v aufx Anmr RTec`; not headlessly reproducible) |
| [ADR-0024](ADR-0024-preset-identity.md) | Factory-preset identity is an immutable internal id (user presets are identified by their file), carried in **plug-in state** so the indicator survives a session reload — 3 additive root fields + 3 per A/B slot; user preset FILES are unchanged, parameter restore is independent of identity restore, and anything unresolvable ticks nothing rather than a same-named substitute | **Accepted** (amended 2026-08-07 pre-merge: the original "never serialized" clause is reversed with maintainer Architecture-Review approval) | Verified (code + state tests 10/11/12; state test 1 pins the new schema shape) |
| [ADR-0025](ADR-0025-regression-test-exception.md) | Documented exception to `TESTING_POLICY` rule 1 for defects with **no stable automated test surface** (GUI/component lifetime, host-owned UI behaviour, OS-level asynchrony) — the default is unchanged and the release gate untouched; invoking it requires four disclosures and registers the gap in `TESTING.md` §Gaps, and the exception lapses when the surface appears | **Accepted** (maintainer instruction 2026-08-07) | Verified (the amended Policy rule; the register and its two pre-existing entries; applied to INC-010) |
| [ADR-0026](ADR-0026-juce-9.0.1-upgrade.md) | JUCE dependency upgrade 9.0.0 → 9.0.1 (immutable-commit pin retained; no source or build-dependency change) | **Accepted** (Architecture Review + Level-5 audition signed off 2026-08-15) | Verified headlessly (32-scenario twin-dump bit-identical incl. latencies; 140 + 894 suites green; registry snapshot unchanged; pluginval 10 both modes ×3) / Verified — manual (Level-5 audition; not headlessly reproducible) |
| [ADR-0027](ADR-0027-cxx23-language-standard.md) | C++ language standard 17 → 23 (`CMAKE_CXX_EXTENSIONS OFF` retained; one `<algorithm>` include added; no version bump; C++20 fallback evaluated and not taken) | **Accepted** (Build System change — Architecture Review signed off 2026-08-15) | Verified (three-platform CI builds incl. AppleClang + MSVC, plus a local libc++ build; 32-scenario twin-dump bit-identical incl. latencies C++17 vs C++23; 140 + 894 suites green under libstdc++ and libc++; identical 29-instance warning set; pluginval 10 both modes ×3) |

## How to add an ADR

1. Confirm the decision is backed by code/test/commit/PR/README evidence.
2. Copy the field structure of an existing ADR (Status, Context, Problem, Options, Decision,
   Consequences, Related code, Evidence + confidence).
3. Assign the next sequential number; add a row here.
4. If the ADR changes a Policy or another ADR, mark the superseded record `Superseded`/`Deprecated`
   and cross-link. See `docs/policies/ADR_POLICY.md`.
