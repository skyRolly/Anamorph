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
| [ADR-0022](ADR-0022-juce-9.0.0-upgrade-sha-pin.md) | JUCE dependency upgrade 8.0.14 → 9.0.0 + immutable-commit (SHA) pinning; Linux EGL build dep | Proposed (pending Architecture-Review sign-off + Level-5 audition) | Verified headlessly (32-scenario twin-dump bit-identical incl. latencies; 140 + 774 suites green; registry snapshot unchanged) |

## How to add an ADR

1. Confirm the decision is backed by code/test/commit/PR/README evidence.
2. Copy the field structure of an existing ADR (Status, Context, Problem, Options, Decision,
   Consequences, Related code, Evidence + confidence).
3. Assign the next sequential number; add a row here.
4. If the ADR changes a Policy or another ADR, mark the superseded record `Superseded`/`Deprecated`
   and cross-link. See `docs/policies/ADR_POLICY.md`.
