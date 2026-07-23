# RELEASE_HARDENING_PLAN.md

**Status: Proposed (planning artifact — no code change accompanies this document).**
Pre-release hardening program for the commercial release of Anamorph: licensing, anti-piracy,
anti-reverse-engineering, release-build security, signing/distribution, and final QA. This is a
**plan**, not a policy: where it proposes decisions, the decision authority is the referenced
future ADR + `docs/policies/ARCHITECTURE_REVIEW_GATE.md` human sign-off, per
`docs/SOURCE_OF_TRUTH.md` (Code → Tests → ADR → Policy → Architecture → Procedures → README).

The program assumes **multiple agents working in parallel**; §9–§11 define the coordination
contract (dependency order, parallelization, branch/PR rules) that every implementation PR in
this program must follow.

---

## 1. Baseline — what exists today (evidence)

| Area | Current state | Evidence |
|---|---|---|
| Licensing | **None.** No license manager, no activation, no trial logic anywhere in `src/` | [Verified] `src/` tree |
| macOS signing | CI **ad-hoc** codesign (`--sign -`), **not notarized**; users must strip quarantine manually. ~~Sign commands `\|\| true`~~ — since RH-PR-2 a signing failure fails the job (ADR-0021) | [Verified] build.yml (Package macOS plugins step); PACKAGING.md |
| Windows signing | **None** (no Authenticode) | [Verified] build.yml (no sign step) |
| Symbol hygiene | **Closed by RH-PR-2 (ADR-0021):** retain-then-strip pipeline on all platforms — debug info generated (`-g`/`/Zi`+`/DEBUG`), captured as separate `Anamorph-<OS>-debug` artifacts (split `.debug`/dSYM/PDB), public binaries stripped (`nm: no symbols`; dynamic exports untouched). Visibility was already hidden via JUCE's plugin helpers (JUCEUtils.cmake) — the earlier "no explicit `CXX_VISIBILITY_PRESET`" row described our CMakeLists, not the effective build; recorded in the ADR instead of restated | [Verified] build.yml, CMakeLists.txt (AnamorphHardening), RH_PR2_INVESTIGATION.md |
| LTO | `juce::juce_recommended_lto_flags` linked on the plugin target; **verified applied** to compiles and the final format-target links (`-flto` measured on both, RH-PR-2) | [Verified] CMakeLists.txt; RH_PR2_INVESTIGATION.md |
| Networking | `JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0` → **no `juce::URL` backend on Linux**; macOS/Windows use native OS backends | [Verified] CMakeLists.txt compile definitions; DEPENDENCY_POLICY.md |
| Installers | None (`.pkg`/`.msi` absent); raw CI artifacts + `INSTALL.txt` | [Verified] PACKAGING.md "Not in scope" |
| Update mechanism | None | [Verified] `src/` tree |
| Crash reporting | None | [Verified] `src/` tree |
| Version management | **Annotated `vX.Y.Z` tag convention adopted (RH-PR-8)**; no tag cut yet (first: v0.8.13 — closes RISK-003 when practiced); version in `CMakeLists.txt` + About box; CI run number as build number | [Verified] RELEASE_PROCESS.md §Tagging |
| Release pipeline | `build.yml` (push/PR/dispatch/`workflow_call`; `contents: read`) + **tag-triggered `release.yml` skeleton (RH-PR-8)**: metadata validation → reused build gates → draft GitHub Release (versioned artifacts + SHA-256 + manifest; `contents: write` scoped to the draft-release job only; no signing secrets exist) | [Verified] CI_CD.md; release.yml |
| QA gate | 33 DSP self-tests + A/B guard (140 checks) + the 9-test state-compatibility suite (774 checks) + pluginval strictness 10, deterministic + randomise ×3, blocking on 3 OSes; Level-5 manual audition | [Verified] TESTING_POLICY.md, CI_CD.md |
| AAX / PACE | **Out of scope** (no Avid/PACE/iLok) — PACE licensing is therefore *not* an available protection option | [Verified] COMPATIBILITY_POLICY.md |

## 2. Release risk assessment

IDs are program-local (`RH-R*`); if any is accepted as a standing repository risk it graduates to
`docs/FUTURE_RISKS.md` under the normal evidence rules.

| ID | Risk | Severity for a commercial release |
|---|---|---|
| RH-R1 | No licensing → the product cannot be sold with any entitlement enforcement at all | Critical (blocking) |
| RH-R2 | macOS not notarized → Gatekeeper blocks by default on modern macOS; the `xattr` workaround is a support wall and looks untrustworthy to paying customers | Critical (blocking) |
| RH-R3 | No Windows Authenticode → SmartScreen "unknown publisher" interstitials; some AV heuristics flag unsigned audio plugins | High |
| RH-R4 | ~~Unstripped binaries with full symbol names~~ **Mitigated by RH-PR-2 (ADR-0021)**: shipped binaries stripped, RTTI typeinfo names the only accepted residue | ~~High~~ Closed |
| RH-R5 | No installers → manual copy instructions; wrong-folder installs; no upgrade path | High |
| RH-R6 | ~~No git tags / tag-triggered release pipeline~~ **Pipeline shipped by RH-PR-8** (tag convention + `release.yml`); residual = no tag cut yet — closes with the first release tag (v0.8.13), which makes shipped bytes attributable to a source state (RISK-003) | Medium → Low |
| RH-R7 | No update notification → shipped defects persist silently in the field | Medium |
| RH-R8 | No crash reporting; ~~no retained debug symbols~~ **symbol retention shipped in RH-PR-2** (per-run `Anamorph-<OS>-debug` artifacts: split `.debug`/dSYM/PDB) — a full crash-reporter remains Phase-2 (§7) | Medium → Low (symbolication now possible) |
| RH-R9 | ~~`codesign ... \|\| true`~~ **Failure-visibility half fixed in RH-PR-2** (a sign/staging failure now fails the job); Developer ID signing itself is still RH-PR-3 | ~~Medium~~ Low |
| RH-R10 | **Third-party licence compliance is broader than the JUCE tier**: the VST3 SDK (ships inside JUCE) is dual-licensed GPLv3 / the proprietary Steinberg *VST 3 Licence Agreement* — closed-source commercial VST3 distribution requires the signed agreement (+ logo/trademark obligations); JUCE additionally vendors harfbuzz/sheenbidi/lunasvg(9)/codec libraries needing a **NOTICES/attribution file** in the distribution. Owner action (agreements) + a small engineering deliverable (NOTICES). Found in the v0.8.13 product-readiness review | High (blocking commercial sale; zero impact on free/AGPL distribution) |

## 3. Guardrails — what this program must not touch

Binding constraints inherited from the policy set; every PR in this program restates them:

1. **No DSP rewrite, no audio-algorithm change, no parameter-behaviour change.** The hard-stop
   list applies unchanged: parameter IDs, serialization schema, threading model, DSP signal
   order, reported latency (`AI_AGENT_POLICY.md`, `ARCHITECTURE_REVIEW_GATE.md`).
2. **License state is NOT session state.** Nothing licensing-related may enter
   `getStateInformation`/APVTS/`InternalState` or preset files. A session saved on a licensed
   machine must reload byte-identically on an unlicensed one (`SESSION_COMPATIBILITY_POLICY.md`;
   `SERIALIZATION_REGISTRY.md` untouched).
3. **The audio thread never sees licensing.** No file I/O, network, locks, allocation, or
   cryptography on the audio thread (`REALTIME_AUDIO_POLICY.md`). The *only* permitted audio-side
   artifact is a relaxed `std::atomic` state read, and any processing consequence must go through
   the existing click-free transition machinery (ADR-0004) — never a hard mute.
4. **The DSP core stays format-agnostic and license-free.** `AnamorphDSP` (`src/dsp/`) must not
   link or reference licensing (preserves ADR-0001; keeps the DSP self-tests license-free).
5. **CI must pass unlicensed.** pluginval strictness 10 (both modes ×3, 3 OSes) and all self-tests
   run with no license present, forever. Demo/unlicensed mode is therefore a first-class, fully
   validated state — not an error path.
6. **Protections must never crash or destabilize the host.** Any check that can fail must fail
   *open* into the defined unlicensed state. A DAW crash caused by our protection is strictly
   worse for the business than piracy.

## 4. Licensing (Phase 2 — design only; decisions go to ADR-0016/0017)

### 4.1 Model evaluation

| Model | User experience | Piracy resistance | Implementation complexity |
|---|---|---|---|
| **Online-only activation** (server check every launch) | Poor — studios are often offline/air-gapped; server outage = product outage; community hostility is well documented | High until cracked (then zero — the check is client-side either way) | Medium + permanent server-uptime liability |
| **Offline license file** (signed blob issued at purchase, no server contact from the plugin) | Excellent — works air-gapped, nothing phones home | Medium — no keygen without the private key, but one shared file unlocks everyone (deterred only by watermarking the file with the buyer's identity) | Low — signature verification + file parsing only |
| **Machine-bound activation** (license locked to hardware fingerprint) | Medium — breaks on hardware upgrades/reinstalls; the #1 licensing support burden in this industry | Medium-high — stops casual file sharing | Medium — fingerprinting + activation counting + self-service deactivation UI |
| **Account-based** (login once, entitlements server-side) | Good if login is once-per-machine, not per-session | Medium — same client-side limits; server-side gating of downloads/updates is the durable part | Medium-high — account system + vendor integration |
| **Perpetual license** | Expected norm for audio plugins in this price class | Orthogonal (an entitlement policy, not a mechanism) | Low |
| **Subscription** | Divisive in this market; requires periodic revalidation → needs the online path + grace logic | Orthogonal | Medium (recurring billing + expiry handling) |

### 4.2 Recommendation

**Hybrid: account-based online activation that *issues* a signed offline license file, with loose
machine binding.** Concretely:

- Purchase creates an account entitlement (perpetual license). Activation (in-plugin if online,
  or via a copy-a-file flow from the account page for air-gapped machines) delivers a
  **signed license file** the plugin verifies **offline forever after** (Ed25519 signature over a
  canonical payload; public key embedded in the binary).
- The license file records licensee identity (name + hashed email — this is the watermark) and
  optionally a **loose machine fingerprint**: `k`-of-`n` matching over stable identifiers, N
  activations allowed (e.g. 3–5), self-service deactivation. Loose on purpose: a fingerprint
  mismatch after a RAM upgrade must degrade to "re-activate", never "locked out".
- The payload schema carries an **entitlement type + expiry field from day one** (perpetual =
  no expiry) so a future subscription/rent-to-own offering is a data change, not a schema break.
  The schema is versioned like the parameter system (`kVersion` discipline, ADR-0002 spirit).
- **Trial**: time-limited (e.g. 14 days), full-featured, driven by the same signed-file mechanism
  (a trial license), so there is exactly one validation path to test and to crack — not two.

Why not the alternatives: online-only fails the offline-studio reality and adds a permanent
availability liability for zero durable protection (any client-side check is patchable);
PACE/iLok is already declared out of scope [COMPATIBILITY_POLICY]; pure unwatermarked offline
files invite frictionless sharing.

### 4.3 Plugin-side architecture

New module `src/licensing/` (wrapper layer, **not** `src/dsp/` — guardrail §3.4):

| Component | Responsibility | Thread |
|---|---|---|
| `LicenseStore` | Locate/read/write the license + cached-authorization files in the per-user app-data dir | Message thread (or one short-lived background thread; never audio) |
| `LicenseVerifier` | Canonical-payload parsing + Ed25519 signature check (pure function, unit-testable headlessly) | Any non-audio thread |
| `ActivationClient` | Talks to the activation endpoint; strictly optional at runtime (offline path never needs it) | Background thread |
| `LicenseState` | The one shared artifact: `enum class { Licensed, Trial, GracePeriod, Unlicensed }` in a `std::atomic`, plus message-thread listeners for the GUI | Written message thread, read anywhere |

**Startup flow:** the `PluginProcessor` constructor reads the cached authorization synchronously
(one small local file — microseconds, and constructors don't run on the audio thread), publishes
the resulting `LicenseState`, and defers *everything else* (full re-verification, any network
re-validation) to an async message-thread/background step. Hard rules learned from host-scan
behaviour: **no dialogs, no blocking I/O, no network during instantiation** — hosts instantiate
plugins headlessly during scans (and pluginval does at strictness 10), and a licensing dialog in
a scan is a classic plugin-blacklisting bug.

**Cached authorization:** successful validation writes a signed cache token (state + timestamp)
so subsequent loads are instant and offline. Perpetual licenses never require revalidation once
activated; only trial/subscription entitlements carry expiry, checked against the token.

**Offline grace period:** applies only to entitlements that need periodic revalidation
(subscription-type). Recommended ~14 days of failed-reachability grace with a visible countdown
in the editor before degrading. Perpetual licenses have no revalidation and therefore no grace
concept — offline forever is a supported state.

**Failure behaviour (the most user-visible decision — goes to ADR-0017):**
- Verification failure of any kind fails **open into `Unlicensed`**, never into a crash, hang,
  or undefined behaviour (guardrail §3.6).
- `Unlicensed` behaviour recommendation: the plugin loads, the editor shows an authorization
  panel + persistent banner, and processing runs **dry pass-through via the existing true-bypass
  path** (delay-aligned, click-free — reuses ADR-0004 machinery, no new transition system).
  Rationale: never noise-burst or hard-mute a paying customer whose license file got corrupted
  mid-session; a bypassed insert keeps their session listenable and recoverable. Session state
  still saves/loads fully (guardrail §3.2) — no user data is ever hostage.
- A license state change mid-session transitions through the forced-duck/crossfade machinery
  exactly like a bypass toggle. No abrupt gain steps.

**Audio-thread contract:** `processBlock` reads one relaxed atomic enum already snapshotted into
the existing parameter-snapshot flow; zero additional branches inside the per-sample loops. The
gating decision maps onto the existing bypass input the engine already supports.

**Linux networking constraint:** `JUCE_USE_CURL=0` means `juce::URL` has no backend on Linux
[Verified: CMakeLists.txt]. Options for `ActivationClient`: (a) offline-file activation only on
Linux (recommended — matches the Linux audio audience and avoids a new dependency), (b) re-enable
curl on Linux only (a Build System change → Architecture Review per DEPENDENCY_POLICY rule 5).
Decision recorded in ADR-0016. macOS/Windows native backends are unaffected.

## 5. Anti-piracy (Phase 3) — effective protections vs security theater

Threat-model honesty first: a native client-side plugin **cannot be made uncrackable** — the
machine executing the check belongs to the attacker. The realistic objectives are: (1) no
keygen (asymmetric signatures achieve this outright), (2) raise patching cost so cracks lag
releases, (3) make sharing traceable (watermarks), (4) keep the durable enforcement server-side
(downloads/updates), and (5) **zero cost to legitimate users and zero host-stability risk**.

### Effective (adopt)

| Protection | Why it works | Cost / tradeoff |
|---|---|---|
| Ed25519-signed license payload | Forging a license requires the private key; eliminates keygens as a class | Near zero; key custody becomes a real operational duty (offline key, never in CI) |
| Licensee watermark in the license file (name + hashed email) | Social deterrent against sharing the file; leaked licenses are attributable and revocable server-side for updates | Zero runtime cost; needs a privacy note |
| Server-side entitlement gating of **downloads and updates** | The only check the attacker's machine can't patch; cracked users self-exclude from fixes | Requires account infrastructure (already in the recommended model) |
| Multiple small inlined check sites (verify at load + on editor open + on state transition), not one `isLicensed()` function | One-site patches are one-byte cracks; distributed inlined checks multiply the patch surface | Modest code discipline; must stay off the audio thread |
| OS code signing + hardened runtime/notarization (§6) | A patched binary breaks the signature: Gatekeeper re-quarantines, SmartScreen re-flags — the crack itself becomes harder to distribute and to trust | Cert costs; pipeline work (RH-PR-3/5) |
| Release-cadence + value (honest pricing, smooth updates, demo that converts) | The only measure with proven long-term effect on revenue in this market | Business, not code |

### Security theater (reject, with reasons)

| Rejected measure | Why it's theater here |
|---|---|
| Debugger/attach detection | Trivially patched (it's just another client-side branch); false-positives against DAW/host debuggers and profilers; a detection-triggered kill inside a host process risks killing the user's *session* — guardrail §3.6 violation |
| Commercial VM packers / whole-binary obfuscation | Breaks or flags under macOS notarization + hardened runtime; classic AV false-positive source; frequently destabilizes plugin scanning in hosts; pluginval strictness 10 becomes unreliable — all for a delay measured in days on popular targets |
| Home-grown code encryption / self-modifying code | Incompatible with W^X + hardened runtime; same AV/notarization problems; enormous maintenance risk against a determined-attacker win anyway |
| Aggressive machine binding (exact-match fingerprint, low activation count, no self-service) | Punishes only paying customers (hardware upgrades, reinstalls); the resulting support load and reputation damage exceed any piracy delta |
| Noise-burst / degraded-audio "demo punishment" in a licensed product's failure path | A false-positive (corrupt file, clock skew) then sabotages a paying user's session — the worst possible outcome |
| Binary self-hashing integrity checks | Marginal at best: the hash check is itself patchable, and signing already provides OS-enforced integrity on macOS/Windows. Optional low-priority extra on Linux only; **warn-only** if ever added, never a kill switch |

## 6. Anti-reverse-engineering (Phase 4) — build hardening

### 6.1 Compiler/build measures (all platforms — RH-PR-2)

| Measure | Detail | Status |
|---|---|---|
| Symbol visibility | JUCE's plugin helpers already set `CXX_VISIBILITY_PRESET hidden` + `VISIBILITY_INLINES_HIDDEN` on the shared-code and every format target (JUCEUtils.cmake; 16 dynamic exports measured) — recorded in ADR-0021 rather than restated in our CMakeLists | **Verified satisfied-by-JUCE (RH-PR-2)** |
| Strip | Release artifacts stripped (`strip -x` mac / `strip --strip-unneeded` Linux / PDB separated on MSVC), **with dSYM/PDB/split-debuginfo generated first and retained as CI artifacts** for crash symbolication (pairs with RH-R8) | **Implemented (RH-PR-2, ADR-0021)** |
| LTO | Linked via `juce::juce_recommended_lto_flags`; `-flto` measured on Release compiles AND the final format-target links (PUBLIC propagation from the shared-code target) | **Verified applied (RH-PR-2)** |
| Optimization | Keep `-O3`/Release as-is — **no flag change that could alter DSP numerics without a twin-dump check** (`DSP_POLICY` Class rules apply to build flags too; `-ffast-math` stays off unless separately ADR'd) | Existing |
| Assert/log hygiene | Audited: zero assertion-message and zero source-path strings in the Release binary; `NDEBUG` effective | **Audited clean (RH-PR-2)** |
| String hygiene | License-check-adjacent literals (server URLs, state strings) built at runtime or lightly encoded so `strings` doesn't hand a crack roadmap over; **no blanket string encryption** (theater §5) | To implement with licensing |
| Windows hardening | `/guard:cf` (CFG) added (compile+link — not an MSVC default); `/DYNAMICBASE /NXCOMPAT` pinned explicitly; `/GS` remains the compiler default | **Implemented (RH-PR-2)** |
| Linux hardening | `-Wl,-z,relro,-z,now,-z,noexecstack` (full RELRO measured via `BIND_NOW`), PIC (inherent), `-fstack-protector-strong` explicit | **Implemented (RH-PR-2)** |

**Regression discipline:** RH-PR-2 must prove the flags are *behaviour-neutral*: full DSP
self-test suite + a twin engine dump (byte-exactness methodology from the 0.8.9/0.8.10 rounds)
before/after, plus pluginval both modes ×3 on all three OSes. Any numeric drift → the flag is
reverted or ADR'd, not shipped silently.

### 6.2 Platform-specific

**macOS (RH-PR-3):**
- **Developer ID Application** certificate signing of every bundle (VST3, AU component,
  Standalone app), signing **nested code first** (the `--deep` flag in today's ad-hoc lines is
  deprecated practice for distribution signing); drop the `\|\| true` (RH-R9) — a sign failure
  must fail the job.
- **Hardened runtime** (`--options runtime`) with the minimal entitlement set a JUCE plugin
  needs; **timestamped** signatures.
- **Notarization** via `notarytool` (App Store Connect API key in CI secrets) + **stapling**;
  removes the `xattr` quarantine wall entirely (kills RH-R2). AU + `auval` behave correctly only
  with proper signing on modern macOS.
- Prerequisite (human): Apple Developer Program membership for RollyTech.

**Windows (RH-PR-5):**
- **Authenticode** signing of the VST3, Standalone exe, and the installer, with timestamping.
  Since 2023, OV/EV code-signing keys must live in HSMs — practical CI options are a cloud
  signing service (e.g. Azure Trusted Signing or an eSigner-type service) rather than a `.pfx`
  in GitHub secrets. SmartScreen reputation accrues to the signed publisher over time.
- Prerequisite (human): certificate/service procurement under the RollyTech legal entity.

**Linux (in RH-PR-2/6):**
- No platform signing/notarization ecosystem exists; ship stripped, hardened binaries with
  **published SHA-256 checksums** in the release notes, packaged as a tarball + install script.
  Accept the weaker protection floor explicitly (the Linux audio market rewards openness;
  heavier DRM here costs more goodwill than it protects).

## 7. Release engineering (Phase 5)

| Area | Plan |
|---|---|
| Installers | macOS: signed + notarized `.pkg` (`productbuild`) installing to the standard `/Library/Audio/Plug-Ins/` paths (PACKAGING.md table), selectable VST3/AU/Standalone components. Windows: Inno Setup (or WiX) `.exe`/`.msi` → `%CommonProgramFiles%\VST3\`, Authenticode-signed. Linux: versioned tarball + `install.sh` + SHA-256 sums |
| Version management | **Adopt annotated git tags** (`v0.8.10` style) — closes RISK-003; the tag is the release trigger and the CHANGELOG entry cites it (upgrades CHANGELOG evidence from "commit SHA" to "tag") |
| Release pipeline | New separate `release.yml` triggered by tag push: build (reusing the build.yml matrix) → sign → notarize/staple → package installers → generate SHA-256 sums → draft GitHub Release with CHANGELOG excerpt. Secrets scoped to a protected GitHub *environment* with required review; `build.yml` keeps `contents: read` and **never** sees signing secrets (PR builds from agents must not be able to exfiltrate them) |
| Update mechanism | In-plugin **check only** (never self-update a plugin binary): message-thread fetch of a signed version manifest over HTTPS, compare, show a non-modal notice linking to the account page. Opt-out in Settings (host-hidden `InternalState`, ADR-0010 pattern — *not* a new APVTS parameter). Same Linux networking caveat as §4.3 |
| Crash reporting | Phase-2 (post-first-commercial-release) unless trivial: at minimum retain dSYM/PDB per release (RH-PR-2) so user-submitted OS crash logs are symbolizable. A full opt-in crash reporter (Crashpad/Sentry-class) is a new dependency → DEPENDENCY_POLICY + ADR; explicitly **not** in the launch-critical path |
| Final QA | Extend `RELEASE_COMPATIBILITY_CHECKLIST.md` with a **commercial-release section**: licensed/trial/unlicensed/grace state matrix (each state: load, process, save/reload session, pluginval), signed+notarized artifact verification on a clean VM/machine (Gatekeeper + SmartScreen), installer install/upgrade/uninstall, update-check behaviour, checksum publication |

## 8. Required ADRs

Per `ADR_POLICY.md` an ADR is written when the decision is made (with its evidence), not
speculatively — these are the decisions this program will force, with their gate triggers:

| ADR | Decision | Why it gates |
|---|---|---|
| ADR-0016 | Licensing model + license-file schema (payload fields, canonical encoding, Ed25519, schema versioning, Linux activation path) | New persistent file format + long-term compatibility contract (schema is forever, like parameter IDs) |
| ADR-0017 | License runtime architecture (states, startup flow, cached auth, failure/grace behaviour, audio-thread contract, unlicensed = true-bypass reuse) | Threading-adjacent + user-facing failure semantics; touches the processor wrapper |
| ADR-0018 | Protection posture (adopt §5 "effective" set, reject the theater set — recorded so future contributors don't re-add anti-debug "improvements") | Binding negative decision; prevents regression into theater |
| ADR-0019 | Signing/notarization/distribution pipeline (cert custody, CI secret handling, environment protection, release.yml design) | Build System change (ARCHITECTURE_REVIEW_GATE) + key-custody security |
| ADR-0020 | Installer + update-notification + version-tagging scheme | Distribution contract; InternalState addition for the opt-out |

Numbering continues after ADR-0015 [Verified: ADR_INDEX.md].

## 9. Dependency order

```
(human) Business decisions: pricing/trial policy, activation server vendor,
        Apple Developer Program, Authenticode service, privacy notice
   │
   ├─► ADR-0016/0017 (licensing)  ─► RH-PR-4 license core ─► RH-PR-7 plugin integration
   │
   ├─► ADR-0019 (pipeline)        ─► RH-PR-3 macOS sign+notarize ─► RH-PR-6 macOS installer
   │                                 RH-PR-5 Windows sign        ─► RH-PR-5b Windows installer
   │
   RH-PR-1 this plan (docs)       — no dependencies
   RH-PR-2 build hardening        — no dependencies (flag verification only)
   RH-PR-8 tags + release.yml skeleton — after RH-PR-2 lands (shares CI files with 3/5)
   RH-PR-9 update check + QA checklist extension — after 4 and 8
```

**Everything above waits for PR #59 to merge** where it touches files PR #59 touches
(CMakeLists.txt, build.yml are safe; `src/` wrapper files are not — see §10).

## 10. Parallelization plan (multi-agent)

| Task | Can parallel? | Files affected | Dependencies |
|---|---|---|---|
| RH-PR-1 Plan + doc syncs | Yes (docs only) | `docs/architecture/RELEASE_HARDENING_PLAN.md`, coverage/map rows | None |
| ADR-0016..0020 drafting | Yes (each its own file) | `docs/architecture/design-decisions/ADR-00NN-*.md`, ADR_INDEX (1 row each — append-only, low conflict) | Human decisions |
| RH-PR-2 Build hardening | **Implemented — ADR-0021** (behaviour-neutral by twin dump + the then-current 136-check self-test suite) | `CMakeLists.txt`, `.github/workflows/build.yml` | — (landed) |
| RH-PR-3 macOS sign+notarize | After RH-PR-2 (same build.yml region); parallel vs licensing | `.github/workflows/*`, `packaging/macos/*`, new scripts | Apple account; ADR-0019 |
| RH-PR-4 License core lib | **Yes — fully parallel** (new files only) | new `src/licensing/*`, new `tests/license_tests.cpp`, CMake target append | ADR-0016/0017 accepted |
| RH-PR-5 Windows signing (+installer) | Parallel vs 3/4 (disjoint files) after RH-PR-2 | new `packaging/windows/*`, release.yml section | Cert service; ADR-0019 |
| RH-PR-6 macOS installer | After RH-PR-3 | `packaging/macos/*` | RH-PR-3 |
| RH-PR-7 Plugin/GUI license integration | **No — serialize with all other `src/` GUI/processor work** | `src/PluginProcessor.*`, `src/PluginEditor.*`, new `src/gui/AuthPanel.*` | RH-PR-4; a quiet window on PluginEditor |
| RH-PR-8 Tags + release.yml | **Implemented — skeleton shipped (v0.8.13 cycle)**: annotated `vX.Y.Z` tag convention, tag-triggered `release.yml` (fail-closed tag⇄version⇄CHANGELOG validation → reused `build.yml` gates via `workflow_call` → draft GitHub Release with versioned artifacts + SHA-256 sums + manifest; `workflow_dispatch` rehearsal mode). Signing/notarization/installer sections remain for RH-PR-3/5/5b/6; first tag cut at the v0.8.13 release closes RISK-003/RH-R6 | `.github/workflows/release.yml` (new), `build.yml` (`workflow_call` trigger only), `docs/procedures/RELEASE_PROCESS.md` | RH-PR-2 (landed) |
| RH-PR-9 Update check + QA matrix | After 4, 8 | `src/licensing/UpdateCheck.*`, editor hook, `RELEASE_COMPATIBILITY_CHECKLIST.md` | RH-PR-4/7/8 |
| DSP changes (any) | **Do not parallelize with this program's `src/` PRs**; DSP-only PRs (e.g. the multiband allpass rework) stay parallel-safe vs licensing/CI work | `src/dsp/*` | Existing roadmap |

**High-contention shared files** (every agent must treat as append-only or serialize):
`CHANGELOG.md`, `docs/HANDOVER.md`, `docs/DOCUMENTATION_COVERAGE.md` (header), `CMakeLists.txt`,
`.github/workflows/build.yml`, `src/PluginEditor.*`, `src/PluginProcessor.*`, `tests/dsp_tests.cpp`.

## 11. GitHub workflow conventions for this program

- **Branch naming:** `rh/<pr-id>-<slug>` (e.g. `rh/pr2-build-hardening`, `rh/pr4-license-core`).
  Agent-assigned branches keep their harness-designated names but map 1:1 to one RH work package.
- **PR scope:** exactly one work package from §10 per PR. No drive-by formatting, no unrelated
  doc edits beyond the DOCUMENTATION_LIFECYCLE triggers of the change itself. Every PR body
  lists: files touched, which §10 row it implements, its dependencies' merge state, and the
  guardrail (§3) checklist.
- **Commits:** conventional prefixes (`feat(licensing):`, `build:`, `ci:`, `docs:`), evidence in
  the message body per CHANGELOG_POLICY spirit; docs synced **in the same PR** as the change.
- **Cherry-pick strategy:** none by default — merge order follows §9. If a release-blocking fix
  must jump the queue, cherry-pick onto a branch cut from `main`, never from another open PR
  (avoids inheriting unreviewed work). Long-lived branches rebase onto `main` rather than
  merge-back, keeping PR diffs reviewable.
- **Conflict avoidance:** before starting, an agent lists open PRs and their touched files;
  the §10 contention list is checked; a work package whose files overlap an open PR waits or
  coordinates explicitly in the PR thread. Hard-stop conditions (AI_AGENT_POLICY) always apply.

## 12. Recommended implementation order (summary)

1. **RH-PR-1** — this plan (docs only, immediately).
2. **Human gate** — business decisions + cert/account procurement + ADR-0016/0017/0019 review.
   Nothing else is truly blocked meanwhile:
3. **RH-PR-2** — build hardening + symbol retention — **done (ADR-0021)**; behaviour-neutrality
   proven by a byte-exact twin engine dump + the full self-test suite.
4. **RH-PR-4** — license core library (new files only; headless unit tests; can start the moment
   ADR-0016/0017 are accepted, fully parallel with 3/5).
5. **RH-PR-3 → 6** (macOS chain) and **RH-PR-5** (Windows) as credentials arrive.
6. **RH-PR-8** — tags + release pipeline skeleton.
7. **RH-PR-7** — plugin/GUI integration (the only `src/` contention point — schedule in a quiet
   window after PR #59 and any GUI work merges).
8. **RH-PR-9** — update check + commercial QA checklist; then a full release rehearsal
   (tag → signed installers → clean-machine validation) before the first commercial version.

## 13. Update protocol

This plan is living: each RH-PR updates its §10 row status on merge; decisions move out into
their ADRs (which then outrank this plan per SOURCE_OF_TRUTH authority order); §1/§2 rows are
re-verified when the baseline changes. When the program completes, this document is archived by
marking the header **Implemented** with pointers to the ADRs and procedures that superseded it.

