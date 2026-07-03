# STATE_SERIALIZATION.md

How session state is saved and restored. The field-level ledger is in
`SERIALIZATION_REGISTRY.md`; binding rules are in
`docs/policies/SESSION_COMPATIBILITY_POLICY.md`.

Evidence [Verified]: src/PluginProcessor.cpp (`getStateInformation` / `setStateInformation`).

## On-disk schema (`getStateInformation`)

A root `ValueTree` "AnamorphRoot" serialized to binary via `copyXmlToBinary`:

```
AnamorphRoot
├─ (property) presetName      : String   -- live preset name (survives reload, F2)
├─ (property) presetBaseline  : String   -- clean-signature for the dirty-star (#6)
├─ <ANAMORPH>           (APVTS state — all host parameters; each PARAM node carries id, value,
│                        AND an additive `raw` attribute = exact normalised getValue(), see below)
├─ <ANAMORPH_INTERNAL>  (host-hidden InternalState: oversample, uiScale, scopePersist,
│                        metersOn, tooltipsOn, uiAnimations)
└─ <AB>
   ├─ (property) active       : int       -- 0 = A, 1 = B
   ├─ (property) slotAParams  : String    -- XML of slot A's APVTS tree
   ├─ (property) slotAName    : String
   ├─ (property) slotABase    : String
   ├─ (property) slotBParams  : String
   ├─ (property) slotBName    : String
   └─ (property) slotBBase    : String
```

Evidence [Verified]: src/PluginProcessor.cpp (`getStateInformation`).

## `getStateInformation` logic

1. `abEnsureInit()` — lazily materialise both A/B slots.
2. Build "AnamorphRoot"; attach preset name + baseline as properties.
3. Append **`copyStateWithRawValues()`** — `apvts.copyState()` with each PARAM node additively
   stamped with its exact normalised `getValue()` as a **`raw`** attribute (see the `raw`-attribute
   note + `SERIALIZATION_REGISTRY.md`; ADR-0013). The A/B slot snapshots (via `currentStateSet()`)
   use the same helper, so their XML also carries `raw`.
4. Append `internal.copyState()` ("ANAMORPH_INTERNAL").
5. Append the "AB" child with both slots' params (as XML strings) + name + baseline.
6. `copyXmlToBinary`.

## `setStateInformation` logic

1. `getXmlFromBinary` → root tree.
2. **If `AnamorphRoot`:**
   - `apvts.replaceState` from the `ANAMORPH` child, **then `reassertParameters(params)`** —
     synchronously re-apply each parameter from the restored tree, preferring the exact `raw`
     attribute (falling back to the denormalised `value` for legacy sessions). This makes a
     wholesale `replaceState` deterministic + idempotent and round-trips discrete params exactly.
   - Restore InternalState from `ANAMORPH_INTERNAL` **if present**, **else**
     `migrateFromLegacyApvts(params)` (pre-0.8.4 sessions had these as APVTS params).
   - Restore preset name + baseline (dirty-star reproduced).
   - Restore A/B slots; the `active` index is **clamped** to a valid slot (`clampAbSlotIndex`,
     `src/AbSlotIndex.h`); per-slot reader falls back to pre-0.6.4 "slotA"/"slotB" (params-only) keys.
3. **Else if the root is the bare APVTS state type:** backward-compat path for v0.2 sessions
   (`apvts.replaceState` + `reassertParameters`).
4. Clear undo history; adopt preset metadata; `syncCommitted()`.

Evidence [Verified]: src/PluginProcessor.cpp (`getStateInformation` / `setStateInformation` /
`reassertParameters` / `copyStateWithRawValues`).

## Backward-compatibility paths (all must be preserved)

| Legacy format | Handling | Source |
|---|---|---|
| **v0.2**: root *is* the APVTS tree | `setStateInformation` else-branch `apvts.replaceState` | :381-384 |
| **pre-0.6.4**: A/B slots stored params only (`slotA`/`slotB`) | `readSlot` legacy-key fallback | :371-375 |
| **pre-0.8.4**: Oversampling/view were APVTS params (no `ANAMORPH_INTERNAL`) | `migrateFromLegacyApvts` | :345-348; InternalState.h:100-122 |

## View-parameter preservation on restore

`applyStatePreservingView` restores a snapshot but **keeps the current** shared view params
(`pid::viewParams` = `bypass`) so an A/B / undo / preset apply never flips the view state.
Evidence [Verified]: src/PluginProcessor.cpp:198-209.

## Invariants

- **Serialization fields are immutable** — see `SERIALIZATION_REGISTRY.md`. Removal is
  prohibited; deprecation requires a migration path (the 0.8.4 InternalState migration is the
  reference precedent).
- A round-trip save→load must reproduce the exact sound, preset name, dirty-star, A/B slots,
  and active slot.
- Changing the schema (new child, renamed property, changed semantics) requires an ADR +
  migration support + Architecture Review (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`).
