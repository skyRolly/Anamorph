# ADR-0037 — A legacy A/B slot is migrated at the decode boundary; no baseline is ever a live read

**Status:** Accepted (maintainer instruction 2026-09-06: investigate the pre-0.6.4 `applyStateSet`
path and decide between retaining, migrating or removing it — recorded here as **migrate**).

**Amends [ADR-0036](ADR-0036-program-state-ownership.md).** Closes the residual its decision 22
recorded under *"Recorded, not changed"* — the one surviving live-read baseline — and makes the
predictor decision 22 introduced (`restoredSoundSig`) exact by construction on the path it is used
on. Nothing in ADR-0036 is superseded: ownership, publication, generations, the replacement lock and
the announce-before-install order all stand.

**Not an Architecture Review Gate item, and stated so deliberately.** No serialization field is
added, removed or re-interpreted (the pre-0.6.4 read path stays, per `SESSION_COMPATIBILITY_POLICY`
rule 3); no parameter, thread, cross-thread path, DSP order or reported latency changes. The one
observable format effect is that a re-saved pre-0.6.4 session now records a per-slot baseline where
it used to write an empty string — a field that was already written, in its documented meaning.

## Context

A session saved before 0.6.4 stores its A/B slots as parameter values alone (`AB@slotA` /
`AB@slotB`); the per-slot name, baseline and identity fields came later (0.6.4 "#6", 0.9.2).
`decodeRestore`'s `readSlot` reads the legacy key when the modern one is absent — two lines — and
produces the canonical `StateSet` with an **empty baseline**. That emptiness is the last legacy
*semantic* in the program-state model: `PresetManager::setMeta` resolved it by reading the live
parameters (`sigAtLoad = soundSig()`), which is correct only because `applyStateSet` applies the
parameters first, in the statement before.

Every other baseline in the model has been decided from bytes since D-2 rounds 9, 10 and 15
(ADR-0036 §17 save, §18 preset load, §22 host restore). §22 left this one open because closing it
means deriving the baseline from the slot's tree, and that tree is applied through
`replaceState` + `reassertParameters` rather than the preset loader — so the equality between the
bytes-only predictor and the sound that path installs had to be **measured** first. Round 10's error
(§19) was assuming exactly such an equality.

The product has never been tagged; the format predates the repository's visible history; no field
capture of a pre-0.6.4 session exists (the fixture is a reconstruction). Support is nonetheless a
written rule (`SESSION_COMPATIBILITY_POLICY` rule 3), tested (State test 5), cited by the release
checklist and by the 0.9.2 changelog. Nothing generates the format any more.

## Problem

1. **One baseline rule too many.** Thirty-two producers decide the baseline from bytes; one decides
   it from a live read, protected by a call-order convention that only a comment enforces.
2. **The defect class the programme removed three times survives at one site.** An audio-thread
   automation write landing between `applyStatePreservingView` and `setMeta` is absorbed into the
   clean baseline — the read-back window ADR-0036 §18 (KI-029) called a defect, narrower and
   legacy-only, but the same class.
3. **The migration is not finished at the boundary.** A re-saved legacy session writes
   `slotABase=""`, carrying the legacy semantic forward through every modern save until the slot is
   switched into and out of.
4. **The equality the fix depends on was unmeasured** — and, once measured, turned out not to hold
   by construction: the session-shaped apply path renders the four log-mapped frequency parameters
   through the store/report pass a *variable* number of times (`worklogs/LEGACY_AB_SLOT_BASELINE_v0.9.7.md`
   §3), because `replaceState` writes the parameter's rendered value back into the very tree
   `reassertParameters` then reads, and `repairSerializedValues` writes repaired text that is
   re-normalised the same way. The prediction agreed with the live signature in 86 062 of 86 062
   cases at the signature's five-decimal resolution — and could differ at a rare boundary. That
   already describes the round-15 root-restore baseline, not only the slot.

## Options

- **A — retain.** Keep the live read; assert the precondition; document the window. Leaves 1–4 in
  place and the round-15 prediction non-constructive. Rejected.
- **B — migrate at the boundary** (chosen). Decode the legacy payload into a slot that is
  indistinguishable from a modern one — baseline derived from its own bytes — and make the
  session-shaped apply path assert the resolved value exactly once, so the derivation is exact by
  construction.
- **C — remove.** Delete the read path; policy amendment, gate review, fixture and test removal.
  Loses a documented contract for ~4 lines, and does **not** remove the empty-baseline semantic (a
  truncated modern blob reaches the same fallback). Rejected — being unreleased makes the break
  possible, not useful.

## Decision

1. **A slot's clean baseline is decided where the slot is decoded, from the slot's own bytes.**
   `readSlot` fills an absent or empty baseline of a valid slot with
   `PresetManager::soundSignatureAfterRestoring (apvts, params)` — on whichever thread decodes,
   as `restoredSoundSig` already is. The legacy key stays; what leaves the decode is canonical.
   Name (`""`) and identity (`unknown`) were already canonical "No Preset" values (ADR-0024).

2. **No baseline is a live read.** `setMeta` stores the baseline it is given and asserts that it
   is non-empty. Every producer now fills it: construction, `load`/`loadFile`, `saveUser`,
   `currentStateSet` (hence undo, redo, A/B, Copy), `adoptRestoreTail` via `baselineOfRestore`,
   and `readSlot`. `applyStateSet`'s statement order is no longer load-bearing.

3. **One resolver for a session-shaped node.** `anamorph::sessionNormalisedValue (rp, node)` —
   `raw` if usable (clamped to 0..1), else `value` if usable (`convertTo0to1`), else the default —
   is the single definition read by `repairSerializedValues`, by `reassertParameters` and by the
   predictor. The preset resolver (`value` only, `normalisedFromSavedTree`) stays what it is: a
   preset file never carries `raw`, and its loader writes the resolved value once already.

4. **The session-shaped apply path asserts the resolved value, once.** `reassertParameters`
   computes each parameter's target through the resolver from the tree the caller passed — the
   original, not the copy handed to JUCE — so nothing `replaceState` flushes back into its own tree,
   and nothing the repair rewrote as text, can change what is asserted. `applySoundTree` and
   `applyStatePreservingView` hand JUCE the repaired copy and this function the original. With
   that, `getValue()` after the apply is exactly one `normalisedAsRendered` of the resolved value,
   for every parameter kind, and the predictor — `signatureAfterApplying` over the session
   resolver — equals the live signature by the same arithmetic on the same inputs, as the preset
   path has since round 11.

5. **`restoredSoundSig` uses the session predictor.** Both fills in `decodeRestore` switch from
   `soundSignatureAfterLoading` (the preset resolver) to `soundSignatureAfterRestoring`, which
   makes decision 22's baseline exact by construction rather than by measurement.

## Consequences

- **Compatibility: migrated, not dropped.** Every legacy read path stays; the pre-0.6.4 fixture and
  State test 5 stay. A re-saved pre-0.6.4 session now writes the derived per-slot baseline, so the
  modernised file carries no legacy semantic at all. A legacy slot switched into reads clean — as
  before — and an automation write in the apply window now reads dirty instead of being absorbed.
- **Sound.** For the four log-mapped frequency parameters the value a session-shaped apply leaves
  in the parameter is now `nAR(n)` always, where it was `nAR^k(n)`, k ∈ {1,2,3} (≈1.3 % of values
  reached k ≥ 2). The difference is ≤ 6·10⁻⁸ normalised — below the signature's resolution and far
  below anything audible — and in the direction the function's own contract already claimed
  ("force every parameter to its exact value from the just-restored tree").
- **Realtime.** Nothing on the audio thread changes; the resolver is called on message and host
  threads only, where the same work was already done.
- **Threading.** No new path, lock, generation or ordering. `readSlot`'s derivation runs on the
  decoding thread exactly like `restoredSoundSig`.
- **Residual, measured and recorded.** One case remains outside the by-construction argument: the
  parameter already holds a *different* denormalised float whose normalised report equals the
  resolved value exactly (the probe's "1 ulp from target" leg reached it in 10 of 23 133 cases).
  `reassertParameters` then skips its write and the live signature is `nAR(n)` while the prediction
  is `nAR(nAR(n))`. No path in this plug-in produces such a neighbour after this change (every
  session and preset apply stores `F(n)`); only a user gesture landing on that exact float can.
  Left as it is: changing the write condition would touch the host-restore notification contract
  for a case with no known producer.

## Related code

- `src/PluginParameters.h` — `anamorph::sessionNormalisedValue` (decision 3).
- `src/PluginProcessor.cpp` — `repairSerializedValues`, `reassertParameters`, `applySoundTree`,
  `applyStatePreservingView` (decision 4); `decodeRestore`'s `readSlot` (decision 1) and its two
  `restoredSoundSig` fills (decision 5); `applyStateSet` (decision 2).
- `src/PresetManager.{h,cpp}` — `setMeta` (decision 2); `soundSignatureAfterRestoring`
  (decisions 3, 5).
- `tests/state_tests.cpp` — State test 5 (re-save writes the derived baseline); State test 65
  (legacy decode, switch-in, repeated restore, partial and malformed payloads, the modern-shape
  control, and the one-pass equivalence sweep over the log-mapped ranges).

Evidence [Verified]: `worklogs/LEGACY_AB_SLOT_BASELINE_v0.9.7.md` §3 (86 062 restore-and-switch
cycles through the real path: 0 signature mismatches; the pass-count histogram; the isolated
mechanism — `replaceState` rewrites the shared tree in 316–1 189 of 2 001 values per range and
`reassertParameters` re-normalises it) and §8 (the implementation's own measurements and the
mutation results). Confidence: Verified for the decision and the mechanism; the by-construction
claim of decision 4 is Verified by State test 65's sweep on the changed tree and by that sweep
failing on the unchanged one.
