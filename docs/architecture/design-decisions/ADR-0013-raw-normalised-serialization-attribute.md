# ADR-0013 — Additive `raw` normalised value attribute in serialized state

**Status:** Accepted

## Context
pluginval's *Plugin state restoration* test (strictness 10, `--randomise`) sets a **raw** normalised
value on each parameter and checks `getValue()` back within `0.1` after a save/restore. JUCE's
discrete parameters (`AudioParameterBool`/`Choice`/`Int`) quantise `getValue()` to the nearest legal
step; for few-step params a fuzzed mid-step value (e.g. `0.807` on a 3-choice → `1.0`) is `>0.1` from
the raw value, so it is intermittently reported "not restored" (seed-dependent, all platforms). APVTS
serialises the **denormalised (snapped)** value, so the raw value is lost across a round-trip.

## Problem
Make state restoration round-trip **every** parameter exactly (raw normalised value preserved),
without breaking session compatibility and without a hard-stop serialization-schema removal/rename.

## Decision
1. **Additively** stamp each `PARAM` node with a **`raw`** attribute = the parameter's exact
   normalised `getValue()` (`copyStateWithRawValues()`); the APVTS `value` attribute is unchanged.
2. On restore, `reassertParameters()` re-applies each parameter from `raw` when present, else from the
   denormalised `value`.
3. Discrete parameters use `RawValued<>` subclasses whose `getValue()` returns the exact raw value
   (the DSP still reads the snapped value via `getRawParameterValue()`/`getIndex()`/`get()`).

This is an **additive, backward- and forward-compatible** schema change (`SERIALIZATION_REGISTRY.md`
invariant: "adding a field is allowed only if absence is handled" — satisfied by the `value`
fallback). No field is removed or renamed; older sessions load unchanged; older plugins ignore `raw`.

## Options
- **A. Do nothing / pin the pluginval seed.** Rejected — leaves the gate intermittently red on a real
  round-trip gap and defers the fix.
- **B. Change the serialization to store raw values instead of denormalised.** Rejected — a schema
  *change* (not additive) that breaks existing sessions (a hard stop).
- **C. Additive `raw` attribute + `reassertParameters` + `RawValued` params.** Chosen — additive,
  compatible, deterministic, and does not alter DSP, parameter IDs, ranges, defaults, or choice order.

## Consequences
- State restoration is deterministic + idempotent and passes pluginval's raw-value tolerance for
  discrete params. The same `raw` mechanism covers A/B slots and undo (via `currentStateSet()`).
- `getValue()` for discrete params now reports the exact automation position (snapped only for the
  actual choice/bool/int used by the DSP and host text) — a deliberate, documented behaviour.
- Serialization gains one optional attribute per PARAM node; the field-level ledger is updated.

## Governance
Per `ARCHITECTURE_REVIEW_GATE.md`, a Serialization Registry change requires review + an ADR — this
ADR + the PR is that review. The change is additive (invariant-compliant), so it is not a prohibited
schema removal/rename.

## Related
- `docs/architecture/SERIALIZATION_REGISTRY.md` (`PARAM/@raw` row), `docs/architecture/STATE_SERIALIZATION.md`.
- Source: `src/PluginProcessor.cpp` (`copyStateWithRawValues` / `reassertParameters`);
  `src/PluginParameters.cpp` (`RawValued<>`).
- Evidence: CI runs `28356632727`, `28388176607`, `28429546091` (the `--randomise` discrete-param
  "not restored" failures this closes).
