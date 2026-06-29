# ADR-0002 — Parameter ID versioning & immutability (`kVersion`)

**Status:** Accepted

## Context
Host sessions, automation lanes, and presets persist by **parameter ID**. VST3 hosts cache the
parameter list; changing it can silently break recall and automation in saved projects.

## Problem
There must be a stable contract for the parameter surface and a signal to hosts when it
legitimately changes.

## Options
- **A. Unversioned string IDs.** Hosts cannot tell when the set changed.
- **B. Versioned parameter IDs (`ParameterID { id, version }`) with a single `kVersion`.** Chosen.

## Decision
Every parameter is registered as `ParameterID { id, kVersion }` with `kVersion = 1` and the
comment "Bump this when the parameter set changes so hosts re-scan automation." The **string
`id` is the immutable contract** (not the display name). Renaming a display name is allowed
(e.g. `Haas Side` → `Haas Focus` in 0.8.6, ID `haasSide` unchanged); removing/renaming an **ID**,
or changing serialization semantics, requires an ADR + migration support.

## Consequences
- Display names can be improved freely without breaking sessions.
- The parameter surface is governed by `docs/policies/PARAMETER_COMPATIBILITY_POLICY.md` and the
  ledger `PARAMETER_REGISTRY.md`.
- A genuine surface change (the 0.8.4 move of view params out of APVTS) must ship with migration
  (see ADR-0010).

## Related code
- `src/PluginParameters.cpp:13` (`kVersion = 1`), `:67-68`, `:135-136` (rename, ID unchanged)
- `src/PluginParameters.h:14-88` (`pid::` IDs + exclusion helpers)

Evidence [Verified]:
- Source: src/PluginParameters.cpp:13,135-136; src/PluginParameters.h:14-88
- History (rename) [Partially Verified]: CHANGELOG.md [0.8.6]
