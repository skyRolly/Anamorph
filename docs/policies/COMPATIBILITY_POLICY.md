# COMPATIBILITY_POLICY.md

**Highest compatibility authority.** The unified compatibility contract for the plugin. This
policy governs `SESSION_COMPATIBILITY_POLICY.md` and `PARAMETER_COMPATIBILITY_POLICY.md` (its
subsets) and the latency contract.

## The contract

A user's saved session — in any host, from any prior shipped version — must reload to the same
sound, with automation and presets intact. The following are **absolutely prohibited** unless an
exception (below) is satisfied:

| Prohibited change | Why it breaks the field |
|---|---|
| **Parameter ID rename or removal** | Sessions/automation key by ID. |
| **Serialization field removal** | Old sessions lose state silently. |
| **Preset schema break** | Saved/factory presets stop loading correctly. |
| **Host-visible parameter semantic change** | Automation lanes now mean something different. |
| **Reported-latency behaviour change** | Host PDC desyncs; timing shifts. |
| **Automation behaviour change** | Recorded automation plays back differently. |
| **Plugin identity change** (`PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, `PRODUCT_NAME`) | The host cannot find the plug-in at all: the manufacturer code is the AU component's manufacturer field, and JUCE derives the VST3 class UID from all three. The session reports the plug-in as missing. |

## The only exception

A prohibited change may proceed **only if all** of the following are satisfied:

1. an **ADR** records the decision (`ADR_POLICY.md`), and
2. a **migration plan** preserves old sessions (a read path / default for the old form) — see the
   identity carve-out below for the one case where no such plan can exist, and
3. the **Release Compatibility Checklist** passes (`procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`), and
4. the change clears the **Architecture Review Gate** (human review).

The reference precedent is the 0.8.4 move of view params out of the APVTS, done with
`InternalState::migrateFromLegacyApvts` (ADR-0010).

### Carve-out: plugin-identity changes (condition 2)

Condition 2 assumes the host still **resolves** the plug-in, so the plug-in can read the old form
itself. For a **plugin-identity change** that assumption fails by construction: the manufacturer
code, plugin code and product name are what the host matches on, so once they change the host
never reaches the plug-in's state-restoration code at all — there is no read path to write, and
no migration plan can exist. Requiring one would not protect users; it would only make the
condition impossible to satisfy while the change itself stayed possible to make.

For an identity change, and **only** for an identity change, condition 2 is satisfied instead by
**all** of:

- **2a.** No annotated release tag exists for any build carrying the old identity — i.e. the change
  is made before the product has ever been released. (Builds already given to testers are covered
  by 2b, not by a migration.)
- **2b.** A documented **recovery procedure** in `KNOWN_ISSUES.md`, written for the person holding
  an affected session, stating what they will see and what to do.
- **2c.** The ADR records that the identity is **frozen** afterwards, and that 2a is thereby spent.

2a is what makes this non-repeatable: it can be true at most once in a product's life. An identity
change proposed after the first release tag has no route through this policy at all — not a harder
one, none.

**Exceptions granted so far:**

| Change | ADR | Condition 2 satisfied by | Status |
|---|---|---|---|
| View params moved out of the APVTS (0.8.4) | ADR-0010 | **Migration plan** — `InternalState::migrateFromLegacyApvts`; old sessions read the legacy form | Accepted |
| Manufacturer code `Anmf` → `RTec` (0.9.1) | ADR-0023 | **The identity carve-out.** 2a: no annotated tag existed for any build carrying `Anmf`. 2b: recovery documented as KI-016 (re-insert the plug-in, re-load the preset). 2c: ADR-0023 freezes the identity and records 2a as spent. | **Accepted** (2026-07-30) |

2a is now spent for this product. From v0.9.1 the plugin identity is frozen, and a later identity
change has **no** route through this policy — the carve-out is unavailable to it, and condition 2
proper is unsatisfiable for it.

## Backward-compatibility paths that must be preserved

- v0.2 bare-APVTS session format (`setStateInformation` else-branch).
- pre-0.6.4 A/B slots (params-only `slotA`/`slotB`).
- pre-0.8.4 legacy APVTS view params (migrated to `InternalState`).

Evidence [Verified]: src/PluginProcessor.cpp:327-396; src/InternalState.h:100-122.

## Subset policies

- **Parameters:** `PARAMETER_COMPATIBILITY_POLICY.md` + ledger `PARAMETER_REGISTRY.md`.
- **Session/serialization:** `SESSION_COMPATIBILITY_POLICY.md` + ledger `SERIALIZATION_REGISTRY.md`.
- **Latency:** `docs/architecture/LATENCY_MODEL.md` (latency changes require an ADR).

## Status taxonomy (for `COMPATIBILITY_MATRIX.md`)

Verified · Partially Verified · Unverified · **Not Supported** (a deliberate exclusion, e.g.
**AAX** and **mono→mono** — these are not "unverified," they are out of scope by decision).
