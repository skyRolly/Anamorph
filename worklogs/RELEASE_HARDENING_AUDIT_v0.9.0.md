# v0.9.0 Release-Hardening & Commercial-Readiness Audit

> Final pre-tag hardening pass over the whole repository: licensing/compliance readiness, user
> documentation quality, maintenance quality, and release-blocker discovery. **No DSP, GUI,
> parameter, serialization or CI-gate behaviour was changed.**

- **Date:** 2026-07-25 · **Base:** `main` @ `0a98ebd` (v0.9.0 RC; PRs #89/#90/#91 merged) ·
  **Branch:** `claude/beautiful-sagan-JAUFI` → PR #92.
- **Method:** six parallel investigation lenses (licence forensics, release blockers, user-doc
  quality, roadmap triage, maintenance sweep, support infrastructure) over the repository *and*
  the fetched JUCE tree, each finding then put through an adversarial verifier instructed to
  refute it from files on disk. Licence claims were additionally re-verified by the orchestrator
  against the real licence files and the compiled object symbols.

---

## 1. Verdict

**Not yet taggable — but nothing engineering can fix.** Every remaining blocker needs a human
decision or a DAW; the mechanical state of the repository is green (Release build, 140-check DSP
suite, 774-check state suite, pluginval strictness 10 in both modes ×3).

Four `RELEASE_POLICY` preconditions are provably unsatisfied on disk:

| # | Precondition | Why it fails | Who closes it |
|---|---|---|---|
| 2 | Compatibility checklist passed | `RELEASE_COMPATIBILITY_CHECKLIST.md` has **8 unchecked boxes and 0 checked**, and there is no per-release instance of it anywhere in the repo — it has never been recorded as completed. The host-matrix and "presets sound identical" items require a DAW. | maintainer, with a DAW |
| 5 | Architecture Review cleared | **ADR-0022 is still `Proposed`.** A JUCE pin change is an explicit `ARCHITECTURE_REVIEW_GATE` Build System change, so the release contains an ungated gated-change. | maintainer sign-off |
| 7 | Manual audition acknowledged | No Level-5 audition is recorded — for v0.8.12 **or** the JUCE 9 build. | maintainer, with a DAW |
| — | (new) Licensing | The repository declares **no licence of its own**: no `LICENSE`, no EULA in either installer. Coupled to the unmade JUCE 9 AGPLv3-vs-commercial tier choice. Filed as **KI-015 / RH-R11**. | owner/legal |

Precondition 1 (tests green) holds locally and in CI; preconditions 3, 4 and 6 hold.

The **engineering** half of third-party compliance — which *was* a genuine blocker — is now
closed by this pass: `NOTICE` and `THIRD_PARTY_LICENSES.md` exist, are verified against the
pinned JUCE tree, and now ship with the binaries.

---

## 2. Third-party licence compliance (Part 2)

### What was wrong

1. **Nothing shipped.** The repository had no `NOTICE`, no `THIRD_PARTY_LICENSES.md`, and no
   licence text in *any* customer artifact — the three zips, the Inno Setup installer and the
   macOS `.pkg` all staged binaries + `INSTALL.txt` only. Three of the vendored licences
   (libjpeg/IJG, FLAC, Ogg Vorbis) require their notice to accompany a **binary** distribution,
   so this was a live obligation, not a formality.
2. **The plan described the wrong VST3 licence.** `RELEASE_HARDENING_PLAN.md` RH-R10 asserted the
   VST3 SDK is "dual-licensed GPLv3 / the proprietary Steinberg VST 3 Licence Agreement". The SDK
   in the pinned JUCE 9.0.0 tree is **MIT** (`VST3_SDK/LICENSE.txt`, "Copyright (c) 2025,
   Steinberg Media Technologies GmbH"); the same text appears in `base/`, `pluginterfaces/` and
   `public.sdk/`. The old description matched an earlier SDK release. Corrected.
3. **Two components are invisible from JUCE's own manifest.** `LICENSE.md` in the JUCE tree is
   the natural place to enumerate dependencies, and it misses two that Anamorph compiles:
   - **FreeType** — PlutoVG vendors FreeType's rasteriser, stroker and fixed-point maths as
     `plutovg-ft-raster.c` / `-stroker.c` / `-math.c`, with `FTL.TXT` beside them.
     `juce_graphics_lunasvg.c` `#include`s all three, and the compiled object carries **30
     `PVG_FT_*` symbols**. The FTL asks distributors to credit it and supplies the wording.
   - **stb_truetype / stb_image / stb_image_write** — vendored in the same directory and pulled
     in by `plutovg-font.c` (`STB_TRUETYPE_IMPLEMENTATION`) and `plutovg-surface.c`
     (`STB_IMAGE_IMPLEMENTATION`, `STB_IMAGE_WRITE_IMPLEMENTATION`). All three define the
     `*_STATIC` macro, so the symbols are file-local and do not appear in the object's symbol
     table — reading `nm` output alone would have missed them too.

   This is the audit's most transferable lesson: **the inventory has to be derived from the
   compiled translation units, not from an upstream manifest.** `THIRD_PARTY_LICENSES.md` records
   the method so the next JUCE bump can repeat it.

### What was verified, and how

Every component was classified COMPILED-IN vs PRESENT-BUT-NOT-BUILT from `build/build.ninja`
plus `nm` on the produced objects, not from reputation:

- **Compiled in:** JUCE, Steinberg VST3 SDK (1675 `Steinberg` symbol references in the VST3
  wrapper TU), HarfBuzz (`hb_*`), SheenBidi (`SBAlgorithmCreate`), LunaSVG + PlutoVG (`PVG_FT_*`),
  FreeType, stb, libpng (`png_*`), libjpeg (`jcopy_block_row`, `jdiv_round_up`), zlib (`_tr_*`),
  FLAC (`FLAC__*`), Ogg Vorbis (`juce::OggVorbisNamespace::*`), the GLEW/Mesa/Khronos block in
  `juce_gl.h`; AudioUnitSDK on macOS only.
- **Not built, with the gate identified:** the JUCE **MP3 decoder** (`JUCE_USE_MP3AUDIOFORMAT`
  defaults to 0 — notable because JUCE's own header warns the code is *"NOT guaranteed to be free
  from infringements of 3rd-party intellectual property"*, so Anamorph ships no MP3 decoder); the
  **LV2 SDK** (behind `JUCE_INTERNAL_HAS_LV2`; zero `lv2_`/`lilv_`/`serd_`/`sord_`/`sratom_`
  symbols); **AAX** (not in `FORMATS`); **ASIO** (headers + licence only, `JUCE_ASIO` off);
  **Oboe** (Android); **CHOC/QuickJS**, **Box2D** (modules not linked).

FLAC and Ogg Vorbis are in the binary purely because `JUCE_USE_FLAC` / `JUCE_USE_OGGVORBIS`
default to `1` and Anamorph never overrides them — it reads no audio files. Turning them off
would remove two BSD binary-attribution obligations, but it touches the build configuration and
was left alone for the release.

### Steinberg — the part that is *not* closed

The MIT grant covers the SDK code. It does not cover the **VST** name/logo or the terms for
developing and distributing VST 3 plug-ins: the SDK ships `VST3_Usage_Guidelines.pdf`, and its
`README.md` states the full SDK from Steinberg contains *"the Steinberg VST 3 Plug-In SDK
Licensing Agreement that you have to sign if you want to develop or host VST 3 plug-ins."*

> **Commercial VST3 distribution requires reviewing Steinberg's licensing requirements
> separately.** No determination is made in this repository. Tracked as RH-F2.

---

## 3. Release blockers found

| Severity | Finding | Disposition |
|---|---|---|
| **Blocker** | No `LICENSE` for Anamorph itself; no EULA in either installer | KI-015 / RH-R11 — owner decision, coupled to the JUCE tier |
| **Blocker** | No attribution shipped with the binaries | **Fixed** — `NOTICE` + `THIRD_PARTY_LICENSES.md` staged into all three zips, installed by the Windows installer, attached as version-named release assets (covering the `.pkg` route) |
| **Blocker** | Compatibility checklist never completed | Reported — needs a DAW |
| **Blocker** | ADR-0022 `Proposed` while its change ships | Reported — needs maintainer sign-off |
| **Blocker** | No Level-5 audition recorded | Reported — needs a DAW |
| High | The release-tag code path in `release.yml` has never executed (`workflow_dispatch` rehearsal skips the entire tag branch), and its annotated-tag check depends on a `git fetch` whose failure is swallowed by `|| true` | Reported. The failure mode is benign in the normal case — the tag being pushed is already present in the checkout — but the first real tag is also this path's first test. Left unchanged: altering release-gate logic immediately before the release is the wrong risk |
| High | All artifacts unsigned; macOS not notarized | Pre-existing, tracked: KI-002, RH-PR-3/5 |
| Medium | No SBOM, no build provenance, `SHA256SUMS.txt` unsigned (integrity, not authenticity) | Deferred — post-tag hardening |
| Medium | `pluginval` is fetched from `releases/latest`, unpinned and unverified, on every gate run | RH-F6 — the release gate's own tool can change under the project |
| Medium | GitHub Actions pinned to mutable major tags (`@v7`, `@v4`, `@v5`) while `msvc-code-analysis-action` *is* SHA-pinned — inconsistent with the project's own stated supply-chain rationale for pinning JUCE by SHA | Reported, not changed: re-pinning all actions before a release trades one risk for another |
| Medium | `RELEASE_POLICY` §Artifacts was stale (no installers, no user manual) | **Fixed** |
| Medium | No support or security contact path anywhere | **Fixed** — `SUPPORT.md` + issue templates |
| Low | The AU is shipped but never validated (pluginval sees only the VST3) | **KI-014** / RH-F3 |

---

## 4. User documentation (Part 3)

The manual was already unusually strong for a pre-1.0 project; the audit found three defects that
matter for a commercial release, all now fixed:

- **The single most-repeated instruction in the docs was unreachable.** "Set Mix to 0 %" appeared
  in the Quick Start, the FAQ and `SUPPORT.md` as *the* way to compare against unprocessed audio —
  but `mixK` is in the Advanced-only visibility list (`PluginEditor.cpp:856`), so a new user in
  Simple view cannot find it. All three now lead with **Bypass** (always visible) and name Mix as
  the Advanced-mode bit-exact reference.
- **Seven controls were documented under host-parameter names, not the labels the plug-in shows**
  — the manual said "Haas Delay", "Velvet Density", "Output Gain", "Input Balance" where the GUI
  shows "Delay", "Density", "Output", "Balance". Now written as **GUI label (host: *Parameter
  Name*)**, which fixes the mismatch and doubles as the automation name map the FAQ was missing.
- **The MULTIBAND `On` toggle was documented nowhere** — the control that decides whether the
  whole multiband section reaches the audio.

Added: a **Quick start** (install → first launch with per-DAW rescan → load on a track → first
sound → where to go next), a **Standalone application** section (a shipped format that had no user
documentation at all — including the fact that the Windows build has no ASIO), a **system
requirements** paragraph, a table of contents, and an **FAQ** covering the topics a commercial
plug-in is expected to answer: not appearing in the DAW, rescanning, Windows paths, macOS
Gatekeeper (both routes), presets (location, sharing, no in-plugin rename), CPU, latency,
automation and session compatibility.

Also corrected: `INSTALLATION.md`'s macOS manual-copy block was missing the `mkdir -p` that the
shipped `INSTALL.txt` includes, and the macOS `INSTALL.txt` closed with "This is an unsigned
developer build for testing" — accurate once, wrong on a release artifact.

---

## 5. Roadmap triage (Part 5)

| Item | Verdict |
|---|---|
| **Licensing decisions** | Owner/legal. Two distinct blockers, both stated precisely in `THIRD_PARTY_LICENSES.md` §"Open licensing decisions": the JUCE 9 tier (AGPLv3 vs commercial) and Anamorph's own LICENSE/EULA, plus the separate Steinberg review. Recorded as RH-R11 / RH-F1 / RH-F2. No terms chosen here. |
| **`auval` in macOS CI** | **Defer, land non-blocking after the tag.** The gap is real (KI-014) — but `auval` only sees a *registered* component, so CI must copy the bundle into `~/Library/Audio/Plug-Ins/Components/` and `killall -9 AudioComponentRegistrar` first, and whether that is reliable on a headless `macos-14` runner cannot be established from this repository. Adding an unproven **blocking** gate immediately before a release tag is the wrong trade. Recipe recorded in `TESTING.md` and RH-F3. |
| **Golden-audio harness** | **Do not build it.** A committed golden waveform would freeze bit-exact output, colliding with the Class-B numerical changes `DSP_POLICY.md` explicitly permits. `tests/fixtures/` holds a parameter-registry snapshot and three legacy XMLs — metadata, no audio — and the 140-check suite already pins the behavioural invariants. The real gap is that the **twin dump** (build before/after, compare hashes + latencies across a scenario matrix) is re-created per investigation instead of living in `scripts/`. That, not a golden file, is the useful version. RH-F4. |
| **Factory presets** | Documented, not redesigned: **ten** presets defined as overrides on the default sound in `PresetManager.cpp` (Default, Gentle Width, Mono To Stereo, Vocal Air, Synth Dimension, Drum Spread, Bass Guard, Tape Chorus, Wide Master, Super Wide); user presets are `.anamorph` XML in a per-user folder. The user-doc gap (sharing, portability, no in-plugin rename) is closed in the FAQ. |
| **KI-009/010/011/013 status** | All four **verified accurate** against the code. KI-011's fix is present (`tooltips.setOpaque(false)`, `PluginEditor.cpp:262`) with the hardware re-test still pending as stated; KI-013's reconcile exists (`getCurrentModifiersRealtime`, `PluginEditor.cpp:1062-1068`) and is inert on macOS as stated; KI-010 has no gesture wrapping on the typed-value path, as stated; KI-009's one-time focus retry runs only on dialog open, as stated. No status changes needed. |
| **RISK-002 / performance budget** | **No repeatable benchmark exists** — `PERFORMANCE_BUDGET.md` says so itself, `scripts/` has no bench entry point, and every number in the Wave 3-6 worklogs came from a session-local harness that was never committed. Added a **required benchmark procedure** to `PERFORMANCE_BUDGET.md`: what to build (Release, with the shipped hardening/LTO flags), the sample-rate × block × algorithm × oversampling × multiband matrix, ns/sample **and worst single block**, median of ≥5 runs with the machine recorded, and the pass/fail question the numbers must answer. No infrastructure added — closing RISK-002 needs measurements, not a framework. |

---

## 5a. Maintenance sweep (Part 6) — and the audit auditing itself

The maintenance lens found a **systemic** problem the earlier mechanical range-check could not
see: ~20 `file:line` "Evidence [Verified]" anchors across the architecture and procedure docs now
point at *unrelated code*. They are all still inside their files, so nothing flags them — the
sources simply grew underneath them. The worst was the canonical one: `AnamorphEngine::process`
cited identically as `:493-949` in both `SIGNAL_FLOW.md` and `DSP_GRAPH_REFERENCE.md` when the
function actually spans **660-1339**. Fixed here: that pair, `toEngine` (`:241-300`/`:201-300` →
`:326-389`), `ScopedNoDenormals` (`:66` → `:109`), the three legacy state-restore paths in
`STATE_SERIALIZATION.md` (v0.2 `:381-384` → `:596-600`; `readSlot` `:371-375` → `:576-593`;
`migrateFromLegacyApvts` `:345-348` → `:557-560`), and KI-009's two anchors, which were off by
~100 lines. Also: `PARAMETER_REGISTRY.md` carried a `‡` footnote with no `‡` anywhere in the
table (now attached to `mbBands`/`mbSolo`'s Auto-Safe column, which is what ADR-0014 changed), and
the README quick-start still called the JUCE pin a "tag".

A second sweep, after the verify phase corrected the finder's own proposed values, fixed **18
more** across ARCHITECTURE, THREAD_MODEL, COMPATIBILITY_MATRIX, TROUBLESHOOTING,
STATE_SERIALIZATION, SERIALIZATION_REGISTRY, LATENCY_MODEL, PARAMETER_REGISTRY and
PARAMETER_REFERENCE — every replacement value spot-checked against the source before applying.
Notable: `COMPATIBILITY_MATRIX`'s three I/O rows all pointed into an unrelated A/B snapshot
comment rather than `isBusesLayoutSupported`, and `TROUBLESHOOTING`'s NaN self-heal anchor was
~400 lines off.

**A line-range citation convention that survives edits is a genuinely open problem** — anchoring
on symbol names would fix it, but `SOURCE_OF_TRUTH.md:48-55` mandates the `path:lines` form for
`Source:` lines, so changing it is a governing-convention change, not a release fix. Recorded,
not attempted.

Two lenses then turned on this audit's *own* output, which is where they earned their keep:

- The **benchmark procedure I wrote documented an API that does not exist** — I specified
  `process(L, R, n)`; the real signature is `process(juce::AudioBuffer<float>&)`
  (`AnamorphEngine.h:60`). A future contributor following it would have hit a compile error on
  line one. Fixed, with a note to hoist the buffer allocation out of the timed region.
- The **`HANDOVER` "Known Blockers" row still said "KI-001…KI-013, all Low/Medium, none
  release-blocking"** — written before this same audit added KI-014 (Medium) and KI-015 (High).
  Fixed.
- The **bug-report form never asked for Oversampling** — the single biggest influence on CPU and
  on whether latency is reported, and (being host-hidden session state) not recoverable from
  anything a user can attach. Added, along with an Apple-Silicon-native-vs-Rosetta distinction
  (the exact axis KI-011 turns on) and a "does the Standalone reproduce it?" discriminator, since
  host-specific defects dominate the confirmed-issue list.
- **Nothing a user downloads pointed at `SUPPORT.md`.** All three `INSTALL.txt` files now do —
  and `SUPPORT.md` ships beside them in the zips and the Windows installer, so the pointer
  resolves offline.
- **`USER_MANUAL` §7.3 described preset *file contents* wrongly.** It said a preset "stores sound
  parameters only", but `PresetManager::saveUser` writes `apvts.copyState()` — the whole tree, so
  the `.anamorph` file does contain Bypass / band-solo / Advanced state. The exclusion is applied
  on **load**, not on write. The user-visible behaviour was described correctly; the claim about
  the file was not. Reworded.
- `SUPPORT.md` **overstated crash symbolication** (CI artifacts expire, and macOS dSYM capture is
  best-effort under Release+LTO) and **assumed GitHub private vulnerability reporting is enabled**
  when that is a repository setting nothing here can confirm. Both softened to what is true, and
  the one real untrusted-input surface — XML preset/session parsing — is now named.

## 6. What changed

**New files:** `NOTICE`, `THIRD_PARTY_LICENSES.md`, `SUPPORT.md`,
`.github/ISSUE_TEMPLATE/bug_report.yml`, `.github/ISSUE_TEMPLATE/config.yml`.

**Packaging (attribution and the support path now travel with the binaries):** `build.yml` stages
`NOTICE` + `THIRD_PARTY_LICENSES.md` + `SUPPORT.md` into all three platform staging dirs;
`Anamorph.iss` installs all three unconditionally; `release.yml` publishes the two attribution
files as version-named assets so `SHA256SUMS.txt` covers them. All three `INSTALL.txt` files point
at `SUPPORT.md`.

**Docs:** `USER_MANUAL` (Quick start, Standalone, system requirements, TOC, FAQ rewrite, control
names, MULTIBAND `On`), `INSTALLATION`, `README` (licensing status, support/compliance links,
both-mode pluginval, JUCE-pin wording), `PACKAGING`, `CI_CD`, `TESTING` (coverage-gaps section),
`RELEASE_POLICY` (artifacts + a new attribution precondition), `RELEASE_HARDENING_PLAN` (RH-R10
corrected, RH-R11 added, §12a follow-ups), `PERFORMANCE_BUDGET` (benchmark procedure + the API
fix), `FUTURE_RISKS` (**RISK-006** — undeclared licensing, graduated from RH-R11 per the plan's
own rule), `KNOWN_ISSUES` (KI-014, KI-015, KI-009 citations), `HANDOVER` (release status + Known
Blockers), `REPOSITORY_MAP`, `SIGNAL_FLOW`/`DSP_GRAPH_REFERENCE`/`ARCHITECTURE`/`THREAD_MODEL`/
`STATE_SERIALIZATION`/`PARAMETER_REFERENCE`/`PARAMETER_REGISTRY` (stale evidence anchors), all
three `INSTALL.txt`.

**No `src/` change.** Per `CHANGELOG_POLICY` rule 3 this is not a user-visible change set in the
changelog sense; the packaging additions are recorded there because they alter what ships.

---

## 7. Validation

- Release build (Linux, Ninja, LTO): green, no new warnings.
- `scripts/run-tests.sh`: **140 checks / 0 failures** (DSP), **774 checks / 0 failures** (state).
- `scripts/run-pluginval.sh 10 deterministic` and `10 randomise`: both **PASSED ×3**.
- YAML parse of every edited workflow and issue template; `bash -n` / `sh -n` over the packaging
  and script files.
- No DSP, GUI, parameter, serialization or CI-gate behaviour changed; the only CI edits are three
  `cp` lines and two release assets.

---

## 8. Recommendation

**Do not tag yet.** The sequence, in dependency order:

1. **ADR-0022 → Accepted** (maintainer Architecture-Review sign-off). Blocks precondition 5.
2. **Level-5 audition** of the JUCE 9 build, covering the outstanding v0.8.12 items in the same
   session. Blocks precondition 7 and feeds item 3.
3. **Complete `RELEASE_COMPATIBILITY_CHECKLIST.md`** and record the completed instance. Blocks
   precondition 2.
4. **Licensing decision** (JUCE tier → Anamorph `LICENSE`/EULA → Steinberg review). Blocks a
   *commercial* release; a free/AGPL release still needs the `LICENSE` file.

Then `git tag -a v0.9.0` → the draft GitHub Release → manual publish. Steps 1-3 gate the tag;
step 4 gates selling it.

---

## Addendum (2026-07-26) — attribution route superseded by owner decision

The in-package attribution route this audit implemented (NOTICE / THIRD_PARTY_LICENSES.md /
SUPPORT.md staged into all three zips and the Windows installer) was **superseded**: the owner
stated Anamorph is a closed-source commercial plugin and directed that the packages stay lean.
The files now accompany every download as **version-named release-page assets**
(`Anamorph-<version>-{NOTICE.txt,THIRD_PARTY_LICENSES.md,SUPPORT.md}`), and every `INSTALL.txt`
carries the mandatory IJG acknowledgement plus a pointer to them. Additionally, the stated
closed-source model rules out the AGPLv3 arm of the JUCE dual licence, so RH-F1 is now
specifically "obtain the commercial JUCE tier + write LICENSE/EULA". Current state:
`docs/procedures/PACKAGING.md` §"Third-party attribution & support files";
`docs/policies/RELEASE_POLICY.md` §"Third-party attribution". This section records the change of
route only — the audit body above is the historical record and is left unedited.
