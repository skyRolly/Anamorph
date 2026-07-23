# JUCE 9.0.0 Migration & Dependency Hardening (v0.8.13 cycle)

Controlled dependency migration: JUCE **8.0.14 → 9.0.0**, plus the audit-roadmap
supply-chain-hardening item "pin JUCE by commit SHA". No feature work, no redesign; the diff is
the pin change, one new Linux build dependency, and documentation. A JUCE bump is a **Build
System change** → Architecture-Review-Gate item (precedent: ADR-0012 for 8.0.8 → 8.0.14);
recorded in ADR-0022 and flagged on the PR.

## 1. Dependency audit (Phase 1 — before any modification)

* **Old pin**: FetchContent `GIT_TAG 8.0.14` (mutable tag name; tag → commit `2cdfca8f`),
  `GIT_SHALLOW TRUE`. `ANAMORPH_JUCE_PATH` escape hatch for offline builds.
* **New upstream**: tag `9.0.0` = commit `f8f8864172464b9adf9eba6101e1f784838d1597`
  ("JUCE version 9.0.0", lightweight tag — verified via `git ls-remote` + clone).
* **Toolchain minimums unchanged**: CMake ≥ 3.22 (same as ours), **C++17 still supported**
  (`juce_CompilerSupport.h` — no C++20 requirement; the C++ build contract is untouched),
  macOS deployment minimum still 10.11 (our 10.13 CI setting stays valid).
* **Licence**: still dual AGPLv3 / commercial, now under the **"JUCE 9" EULA** wording
  (LICENSE.md). Structurally unchanged vs JUCE 8; the EULA-version change is an owner-level
  business note, not a build fact.

### 1.1 Breaking changes in 9.0.0 (BREAKING_CHANGES.md, full 8.0.14→9.0.0 delta —
8.0.15 recorded no breaking entries) vs Anamorph's API surface

| Upstream change | Anamorph exposure | Action |
|---|---|---|
| Windows multi-touch disabled by default (`usesWindowsMultiTouch()`) | None — mouse-only editor; no multi-touch handlers (`mouseMagnify` unused) | none |
| `Drawable::createFromSVG (XmlElement&)` removed (SVG parser now lunasvg) | No SVG use anywhere in `src/` | none |
| `DrawableShape` stroke/dash signature changes | `DrawableShape` unused | none |
| `Drawable` no longer inherits `Component` | Only use is the `const juce::Drawable* icon` parameter in the `LookAndFeel::drawPopupMenuItem` override — the JUCE 9 virtual keeps the identical signature (juce_LookAndFeel_V2.h:174-177); pointer type only, never treated as a Component | none |
| **Linux OpenGL now uses EGL, not GLX** | `juce_opengl` is linked on every platform (ADR-0011: the plugin never *attaches* a GL context on Linux, but the module still compiles) → **EGL headers become a Linux build dependency** | `libegl-dev` added to `scripts/setup-linux.sh` (used by CI and BUILD.md) |

### 1.2 Non-breaking 9.0.0 changes reviewed for regression risk (CHANGE_LIST.md)

* **New macOS CoreAudio implementation** — affects the Standalone device layer only; the
  VST3/AU plugin path does not own the device. CI macOS build + pluginval cover compile/load.
* **Software-renderer performance improvements** — Linux renders CPU-side (ADR-0011); an
  upstream renderer change cannot be pixel-guaranteed across a major bump. GUI code paths are
  unchanged; visual sameness is a Level-5 (audition) item, noted in §5.
* New SVG parser / variable fonts / Linux OpenGL-ES / multi-touch improvements — no project
  exposure (no SVG, no variable fonts, no GL on Linux, no touch).
* `juce_audio_processors_headless` module layout retained (state harness unaffected);
  `juce::exactlyEqual` retained (state tests); `copyXmlToBinary`/`getXmlFromBinary` framing
  unchanged (state-blob compatibility).

### 1.3 Known-issue cross-checks

* **KI-013** (macOS release-outside reconcile inert): JUCE 9's `getNativeRealtimeModifiers`
  still refreshes only keyboard `modifierFlags` and returns cached mouse-button state
  (juce_NSViewComponentPeer_mac.mm) — **not fixed by 9.0.0**; KI-013 stands, evidence updated.
* **KI-003/KI-007** (pluginval-side X11 XEmbed crash): lives in pluginval's own JUCE, not
  ours — unaffected by our bump.
* Custom workarounds audited: no JUCE patches are carried in-repo (the pluginval retry and
  ADR-0011 no-GL-attach are host-side/runtime decisions, not JUCE modifications).

## 2. Changes applied (Phase 2)

* `CMakeLists.txt`: pin → commit SHA `f8f8864…` with `ANAMORPH_JUCE_VERSION="9.0.0"` carrying
  the human-readable version (immutability: a re-pointed upstream tag can no longer change our
  dependency; fetch-by-SHA verified against github.com incl. shallow fetch, and the configure
  banner prints `9.0.0 (f8f8864…)`).
* `scripts/setup-linux.sh`: `libegl-dev` (with a rationale comment).
* Project C++ sources: **no changes required** — the whole 8.0.14→9.0.0 breaking surface has
  zero project exposure (§1.1).
* **Stale-cache trap found during validation and documented**: `ANAMORPH_JUCE_TAG` is a CACHE
  variable, so an existing `build/` silently keeps the OLD pin after a pull (observed locally:
  banner `fetching JUCE 9.0.0 (8.0.14)`). Fresh CI configures are unaffected. Remedy row added
  to TROUBLESHOOTING (delete `build/` or `-UANAMORPH_JUCE_TAG -UANAMORPH_JUCE_VERSION`).
* Docs synced: DEPENDENCY_POLICY (pin table + version-lock rule + EGL), BUILD.md, README,
  TROUBLESHOOTING (pin row + cache-trap row), REPOSITORY_MAP, COMPATIBILITY_MATRIX,
  FUTURE_RISKS RISK-001 (SHA pin + twin-dump precedent), KNOWN_ISSUES KI-011/KI-013 evidence
  (re-verified against the JUCE 9 tree; KI-013 not fixed upstream), ADR-0022 (+ index row,
  Proposed), HANDOVER, DOCUMENTATION_COVERAGE — plus a repo-wide `CMakeLists.txt:NN` citation
  sweep (+5 line shift from the pin block; every cite re-verified against the new file,
  including two pre-existing stale cites found and fixed: ARCHITECTURE.md `:62-73` and
  COMPATIBILITY_MATRIX's VST3 `:137`).

## 3. Validation

* **DSP bit-identity (twin dump)** — the core "identical DSP behavior" proof: a scratchpad tool
  (methodology recorded here; tool not committed, per the `xbench.cpp` precedent) compiles the
  8 `AnamorphDSP` sources + a deterministic scenario driver against **both** JUCE checkouts with
  identical flags, runs **32 scenarios** (Haas/Velvet/Chorus/Dim-D × OS Off/2x/4x/8x × M/S
  on/off; drive 8 dB, amount 0.7, width 1.6, mix 0.8, multiband + mono-maker + level-match on;
  120 noise + 120 silence blocks each at 48 kHz/512) and FNV-1a-hashes every output byte.
  **All 32 hashes and all reported latencies are identical 8.0.14 vs 9.0.0** — including the
  `juce::dsp::Oversampling` wet path, the upstream component most able to change numerics.
  (First run also validated the harness itself: with `algoAmount` left at its 0 default the
  parked algorithms hashed identically to each other, so the scenario set was corrected to
  engage the wet path before freezing the baseline.)
* **Build**: full fresh Linux Release build green against the SHA pin (configure banner
  `fetching JUCE 9.0.0 (f8f8864…)` — FetchContent-by-SHA proven end-to-end, `GIT_SHALLOW`
  retained). Windows/macOS builds are exercised by the CI matrix on push.
* **Suites at JUCE 9**: `AnamorphTests` **140 checks, 0 failures**; `AnamorphStateTests`
  **774 checks, 0 failures** — including the parameter-registry snapshot **frozen under
  8.0.14**, which passes byte-for-byte: parameter IDs/names/order/flags/ranges and the
  serialization schema are unchanged under JUCE 9.
* **Warnings**: `juce_recommended_warning_flags` is **byte-identical** between 8.0.14 and
  9.0.0, and a controlled recompile of the DSP TUs against both versions produced the
  **identical 7 warning instances** — the full-rebuild warnings (VelvetNoise `-Wfloat-equal`,
  AnamorphEngine `-Wswitch-enum`/`-Wmisleading-indentation`, ScopeBuffer `-Wsign-conversion`,
  PluginProcessor `-Wshadow`) are the pre-existing project baseline, newly *visible* only
  because this was the first full clean rebuild here. **No new warnings from the migration**;
  the baseline itself is untouched (no unrelated cleanup).
* **pluginval**: the release download is egress-blocked in this sandbox (HTTP 403 — same
  constraint recorded for ADR-0012); the strictness-10 both-modes ×3 gates run blocking on all
  three CI platforms on push.

## 4. Compatibility verification

| Contract | Evidence |
|---|---|
| VST3 loading | CI pluginval strictness 10 (deterministic + randomise ×3, 3 OSes, blocking) on this PR |
| Parameter IDs / names / order / automation flags | registry-snapshot fixture frozen under 8.0.14 passes unchanged under 9.0.0 (state test 2) |
| Serialization schema + raw-exact round-trip | state tests 1/3 green under 9.0.0 (`copyXmlToBinary` framing unchanged upstream) |
| Legacy sessions (v0.2 / pre-0.6.4 / pre-0.8.4) | state tests 4-6 green under 9.0.0 |
| Presets | state test 8 green under 9.0.0 |
| A/B state | state tests 3/9 green under 9.0.0 |
| DSP output + reported latency | 32/32 twin-dump hashes + latencies identical (§3) |
| Bypass / lifecycle / click-free transitions | DSP suite (bypass-null, transition, parked-path bit-exactness tests) green under 9.0.0 |

## 4b. Pre-commit verification (carried inline)

The usual 3-lens adversarial Workflow was launched but all three subagents were lost to the
org's monthly spend limit mid-run (the same failure mode previously recorded for the v0.8.11
GUI sweep), so the verification was carried **inline** against the same checklist: tag→SHA
identity re-confirmed from two independent paths (ls-remote + checkout log); every
`CMakeLists.txt:NN` cite changed in this diff opened and anchor-matched; the §1.1 table checked
1:1 against the upstream `BREAKING_CHANGES.md` 9.0.0 section (5/5 entries, none for 8.0.15);
zero-exposure greps re-run; remaining `8.0.14` doc mentions audited (all period-correct
history: the ADR-0012 compliance record, headers, KI mechanism citations); CI_CD.md confirmed
version-free (no edit needed); `juce_opengl` macOS/Windows module metadata diffed 8 vs 9
(identical — no new deps there); `libegl-dev` confirmed a real Ubuntu package;
DEPENDENCY_POLICY Upgrade rules walked item-by-item (rule 1 ADR+gate ✓, rule 2 headless parts ✓
/ audition OPEN, rule 3 latency+session-reload re-verified via twin-dump latencies + the state
suite, rule 4 unchanged, rule 5 flags unchanged at `:188-193`); and the CHANGELOG precedent
verified — the 8.0.14 bump's entry lives inside the `[0.8.8]` release section (CHANGELOG:684),
so recording this bump in the v0.8.13 release entry at release-prep matches precedent. A
compliance-log entry was appended to DEPENDENCY_POLICY (audition marked OPEN).

## 5. Remaining migration risks

1. **Level-5 manual audition (OPEN, human-gated)** — required by DEPENDENCY_POLICY rule 2 for
   any JUCE bump; must compare the 9.0.0 build against 8.0.14 in a DAW (ADR-0012 precedent).
   The twin dump proves engine numerics; it cannot prove editor **visual** sameness: JUCE 9
   reworked the SVG parser, font handling (variable fonts) and improved the software renderer,
   so subtle text/AA rendering differences on the CPU-rendered Linux path (ADR-0011) are
   possible and only a human can judge them. Until attested, ADR-0022 stays **Proposed**.
2. **New macOS CoreAudio implementation** — Standalone-app device layer only (the plugin does
   not own the device); CI proves build+pluginval, a macOS Standalone smoke test is prudent at
   the next audition.
3. **Windows/macOS compile** — not locally provable here; the CI matrix on this PR is the
   verification (fail-closed).
4. **EGL packaging drift** — distro variations of the `libegl-dev` package name (older Ubuntu:
   `libegl1-mesa-dev`) may need the same fallback note as webkit; TROUBLESHOOTING covers the
   class, revisit only if CI or a user hits it.
5. **KI-013 unchanged** — JUCE 9 does not fix the macOS realtime-modifiers limitation
   (re-verified); no regression, the known issue simply persists.
