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

- **2a.** **No annotated release tag exists at all** — i.e. the product has never been released.
  (Builds already given to testers are covered by 2b, not by a migration.)
- **2b.** A documented **recovery procedure** in `KNOWN_ISSUES.md`, written for the person holding
  an affected session, stating what they will see and what to do.
- **2c.** The ADR records that the changed field is **frozen** afterwards.

**2a is a condition on the state of the world, not a token an exception consumes.** It is true
while no tag exists — for *every* identity field at once — and becomes permanently false the moment
the first annotated tag is cut, again for every field at once. So:

- Before the first tag, the carve-out is available for **any** identity field, and using it for one
  field does not use it up for another. (A product rename before the first tag would be governed
  by the same three conditions, not blocked by an earlier manufacturer-code change.)
- After the first tag, the carve-out is available for **none** of them. An identity change proposed
  then has no route through this policy at all — not a harder one, none.

This is what makes the carve-out non-repeatable across a product's life without making it
artificially scarce before release.

**Exceptions granted so far:**

| Change | ADR | Condition 2 satisfied by | Status |
|---|---|---|---|
| View params moved out of the APVTS (0.8.4) | ADR-0010 | **Migration plan** — `InternalState::migrateFromLegacyApvts`; old sessions read the legacy form | Accepted |
| Manufacturer code `Anmf` → `RTec` (0.9.1) | ADR-0023 | **The identity carve-out.** 2a: no annotated tag exists. 2b: recovery documented as KI-016 (re-insert the plug-in, re-load the preset). 2c: ADR-0023 freezes the manufacturer code. | Decision **Accepted** (2026-07-30); **condition 3 still open** — the Release Compatibility Checklist has not been completed for this release. It gates the tag, not the merge. |

Note that "Accepted" in this table refers to the ADR, i.e. the *decision*. An exception is only
**fully satisfied** when all four conditions are met, and condition 3 is a **release-time** check
by construction — so an entry can legitimately sit here with the decision approved and the
checklist outstanding. What must never happen is a release shipping in that state
(`RELEASE_POLICY.md` precondition 3).

2a remains true only until the first annotated tag is cut. From that moment the identity — every
field of it — is frozen, and a later identity change has **no** route through this policy: the
carve-out is unavailable and condition 2 proper is unsatisfiable for it.

## Backward-compatibility paths that must be preserved

- v0.2 bare-APVTS session format (`setStateInformation` else-branch).
- pre-0.6.4 A/B slots (params-only `slotA`/`slotB`).
- pre-0.8.4 legacy APVTS view params (migrated to `InternalState`).

Evidence [Verified]: src/PluginProcessor.cpp:1226-1485 (`decodeRestore` + `setStateInformation`: the AnamorphRoot read path, the pre-0.6.4 `readSlot` legacy-key fallback at :1334-1335, and the v0.2 else-branch at :1388); src/InternalState.h:252-306.

## Runtime compatibility: the x86-64 ISA floor

The contract above is about **state**. This one is about the **CPU**, and it belongs in the same
policy because a plug-in the host cannot execute is as broken to a user as a session that will not
reload — worse, in fact, because the failure is not diagnosable from inside the plug-in.

**The floor is Haswell (Intel, 2013) / Excavator (AMD, 2015) on every shipped x86-64 binary** —
all three toolchains. GCC/Clang (the Linux x86-64 build and the `x86_64` slice of the macOS
universal build) compile `-march=haswell -ffp-contract=off` (ADR-0031); MSVC (the Windows build)
compiles `/arch:AVX2` at its default non-contracting `/fp:precise` (ADR-0032). Either way the
compiler may emit AVX2, FMA, BMI, F16C, LZCNT and MOVBE anywhere in the image. Below that floor
the binaries are **not supported**.

| | |
|---|---|
| **Failure mode on an older CPU** | `SIGILL` — an illegal-instruction fault raised **inside the host process** at whatever point the first unsupported instruction is reached. The host reports a crash, or vanishes; it does not report an incompatible plug-in. |
| **Why the plug-in cannot diagnose it** | The fault can be raised by code the dynamic loader runs before any Anamorph entry point does — static initialisers are compiled under the same flags. A `__builtin_cpu_supports` check would have to live in a separately compiled baseline translation unit gating the whole plug-in, which is a different build design, not a message. |
| **Who is actually exposed** | **macOS, materially:** `CMAKE_OSX_DEPLOYMENT_TARGET` is 10.13, and High Sierra runs on Macs back to 2009 — Nehalem, Sandy Bridge and Ivy Bridge, all pre-Haswell. **Linux:** the declared glibc/libstdc++ ABI floor (`scripts/check-linux-abi.py`) already implies a distribution far newer than 2013, so in practice the ISA floor binds on hardware, not on distributions. **Windows:** any x64 machine with a pre-2013 Intel / pre-2015 AMD CPU — Windows 10/11 both install on such hardware, so the floor binds on hardware there too, surfacing as `STATUS_ILLEGAL_INSTRUCTION` (0xC000001D) at plug-in load. |
| **Windows** | **The same floor, since ADR-0032.** The MSVC build compiles `/arch:AVX2` at the default (non-contracting) `/fp:precise`; `/fp:contract` is NOT enabled, and no runtime dispatch exists. The Class-A property this rests on (0/32 twin-dump scenarios moved by the flag, measured on toolset 14.51.36231 across two runs) is a toolset-version behaviour, so it is **asserted on every push**: the `windows` job requires toolset ≥ 14.30 — the boundary at which `/fp:precise` stopped contracting by default — and the `windows-avx2-ab` gate blocks on baseline == `/arch:AVX2`. No Windows performance figure exists; the benefit is mechanism-shared with the measured GCC/Clang result (−18.4 %/−19.6 % engine Ir/sample on Linux), never quoted as a Windows number. |
| **arm64** | Not affected: no ISA flag is applied to the arm64 slice, and none is implied. |

**Changing this floor is a compatibility change.** Raising it (a newer `-march`), extending it to a
platform that does not carry one today, or lowering it, each requires an ADR and a row here — the
same treatment as the ABI floor, and for the same reason: the user-visible consequence is a product
that does not run.

## Numerical compatibility: within an architecture, not across architectures

**Numerical bit identity is guaranteed within the same architecture and build configuration, and
not across different CPU architectures.** Two builds of the same source for the same architecture
at the same flags produce bit-identical output; that is what the `AnamorphDspDump` twin-dump gate
asserts at a dependency bump (`DEPENDENCY_POLICY` rule 2), and it is what every Class-A claim in
this repository means. **The twin-dump gate compares builds within one architecture only.**

**One Windows-specific caveat narrows the "same architecture" half there.** The x64 UCRT selects
FMA3 or non-FMA3 implementations of its transcendental functions **at process start, by CPU
capability** — a documented Microsoft runtime behaviour, independent of any compile flag and
predating ADR-0031 — and the engine calls CRT transcendentals at runtime (the per-block chorus LFO
seed; the oversampling coefficient derivation at `prepare()`). So on Windows the same binary can
produce different bits on machines of different CPU classes, and a Windows bit-identity claim is
scoped to **the same machine class**, not just the same architecture. This is an existing property
of the platform, not a consequence of (or argument about) the `/arch:AVX2` build flag ADR-0032
adopted; the `windows-avx2-ab` gate controls for it by comparing builds on one machine. On Linux and macOS
no equivalent runtime dispatch is in play for this code, and the plain within-architecture statement
stands unqualified.

The `arm64` and `x86_64` slices of the shipped macOS universal binary do **not** produce identical
bits. This is a measured property of the platforms, established by the A7-5E experiment on an Apple
Silicon runner with Apple Clang and Apple libm, and reproduced independently on Linux with GCC 13,
glibc and qemu-user — identical counts, identical split, identical scenario names
(`worklogs/performance/PERF_AUDIT_A7-2B_A7-5E_IMPLEMENTATION.md` §5, §5c):

| | shipped flags | contraction disabled on both slices |
|---|---|---|
| scenarios that differ, of 32 | **32** | **24** — the 8 that agree are every oversampling-×1 scenario and only those |

Two mechanisms, only one of which a flag can reach:

1. **FP contraction.** AArch64 has `FMLA` in its base ISA and contracts with no flag; the x86-64
   baseline could not contract at all until ADR-0031, and now carries `-ffp-contract=off` so it
   still does not. This half is flag-reachable and is *not* being equalised: disabling contraction
   on arm64 would be a Class-B change to the shipped arm64 numerics, taken for no user-visible
   reason.
2. **Oversampling coefficients.** The polyphase coefficients are derived at runtime through
   transcendental libm calls, and **Apple's libm does not agree with itself across Apple's own two
   architectures**. No flag reaches this; removing it would mean replacing JUCE's oversampling
   coefficient derivation with a pinned portable one.

**This difference is accepted and is not to be removed.** Do not disable contraction on arm64 to
close it, and do not replace JUCE's or libm's coefficient generation. Cross-architecture bit
identity is **not a project goal**; if it ever becomes one it needs a user-visible reason, an ADR,
and a row in the table above — not an incidental flag change.

## Subset policies

- **Parameters:** `PARAMETER_COMPATIBILITY_POLICY.md` + ledger `PARAMETER_REGISTRY.md`.
- **Session/serialization:** `SESSION_COMPATIBILITY_POLICY.md` + ledger `SERIALIZATION_REGISTRY.md`.
- **Latency:** `docs/architecture/LATENCY_MODEL.md` (latency changes require an ADR).

## Status taxonomy (for `COMPATIBILITY_MATRIX.md`)

Verified · Partially Verified · Unverified · **Not Supported** (a deliberate exclusion, e.g.
**AAX** and **mono→mono** — these are not "unverified," they are out of scope by decision).
