# PARAMETER_COMPATIBILITY_POLICY.md

Subset of `COMPATIBILITY_POLICY.md`. Governs the host parameter surface and automation. Ledger:
`docs/architecture/PARAMETER_REGISTRY.md`. Decision basis: ADR-0002.

## Rules

1. **Parameter IDs are immutable.** The string `id` in `ParameterID { id, kVersion }` is the
   persistent contract. No rename, no removal without an ADR + migration.
2. **Display names may change.** Renaming the user-facing name is allowed (e.g. `Haas Side` →
   `Haas Focus`, ID `haasSide` unchanged). Update `PARAMETER_REGISTRY.md` + `CHANGELOG.md`.
3. **Ranges, defaults, and choice orderings of host-visible params are semantic.** Changing them
   in a way that alters recall or automation playback requires an ADR.
4. **`kVersion` is bumped only on a deliberate parameter-set change**, so hosts re-scan automation.
5. **Automation-flag changes** (`withAutomatable`) are allowed but must be recorded — note that
   `withAutomatable(false)` does **not** hide a param in all hosts (REAPER); true hiding means
   moving it out of the APVTS (see ADR-0010, `InternalState`).
6. **Exclusion lists are part of the contract.** `pid::viewParams` (A/B/undo/preset exclusion) and
   `pid::isPresetExcluded` (`mbSolo`, `advancedMode`) changes affect recall behaviour → ADR.

## Required verification before release

- `[ ] Parameter IDs unchanged` (diff the registry).
- `[ ] Automation playback verified`.

Evidence [Verified]: src/PluginParameters.cpp:13,114-198; src/PluginParameters.h:64-87.

## Enforcement

A Parameter Registry change is an **Architecture Review Gate** item and an **AI Agent Hard Stop**.
Changing this policy requires an ADR.
