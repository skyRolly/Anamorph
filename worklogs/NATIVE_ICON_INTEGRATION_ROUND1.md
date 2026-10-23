# Native icon integration — Round 1

2026-10-23 (Asia/Shanghai). Baseline Anamorph main:
`098884e7fe12049010f2eaf8379e1514efdad0ab`; branch `feature/native-product-icons-round1`.
Owner Architecture Review explicitly approved the proposed two-input JUCE change before
implementation; [ADR-0062](../docs/architecture/design-decisions/ADR-0062-native-juce-icon-resources.md)
records the decision. The PR must remain unmerged. This record does not authorize shipping.

## Provenance and implementation

Source: [skyRolly/icon-vector-reconstruction](https://github.com/skyRolly/icon-vector-reconstruction),
commit **`35cf982f9d2e33bde5c4a1bf84b17205ce1e6b80`**. Accepted design: **Candidate B0 unchanged**,
merged in source PR #10 at `30349f34973366b7824700f2c212037780b9cdad`. No source-repository files
were edited. [PROVENANCE.json](../resources/icons/PROVENANCE.json) pins source and destination
paths, digests, dimensions, byte counts, consumers, source manifest digest and approval scope.

| Source below `assets/anamorph/` | Imported destination | SHA-256 | Consumer |
|---|---|---|---|
| `master/anamorph-rounded-1024.png` | `resources/icons/anamorph-1024.png` | `b7dd6efd377125a08360f1ce2bf5c82450bf05932d70a7c957f329b73bc29b0e` | ICON_BIG |
| `portable/png/anamorph-32.png` | `resources/icons/anamorph-32.png` | `f2a80389e5f47c23b69a0e8956f0492c5f1194a6970df0726c172bf633fb2002` | ICON_SMALL |

Before copying, every current source manifest asset (89 records, 91 files with the two indices)
and all 11 source-file digests were checked against Git objects. Both imported files match the
manifest and accepted-merge bytes/decoded RGBA. Destination hashes were checked after copying.
The 1024 input is RGBA/sRGB with alpha bounds `(32,32,992,992)`; the 32 input has bounds
`(1,1,31,31)`. Both have alpha range 0..255. The unchanged canonical SVG digest is
`9d944b2b04abeb92dddc6840cf949d126162169159e937f23a384160d0db56b4`.

The source manifest/inventory still contain **brand approval pending** wording, and its dated
integration audit describes Anamorph `c4f390f` / 0.9.9. These labels are stale relative to the
owner's B0 decision and this 0.9.10 branch. They were disclosed before implementation and left
unchanged in the read-only source repository. Native certification remains pending. Its
`source_revision` is a content fingerprint, not the import Git commit.

The only application build change is inside `juce_add_plugin(Anamorph)`:

```cmake
    ICON_BIG                "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/anamorph-1024.png"
    ICON_SMALL              "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/anamorph-32.png"
```

No adjacent checkout, build-time download, custom generator, post-signing resource patch or
runtime image dependency is introduced. No ICON_COMPOSER_BUNDLE is configured.

## Actual pinned JUCE behavior

JUCE is still **9.0.4**, immutable commit `3fb5cbc0340758596dac39f7c8de47d562b4896e`.
Evidence: `extras/Build/CMake/JUCEUtils.cmake`, `extras/Build/juceaide/Main.cpp`,
`extras/Build/juce_build_tools/utils/juce_Icons.cpp` and `juce_PlistOptions.cpp` at that pin.

| Platform | Expected generated resources and actual frame policy | Validation surface |
|---|---|---|
| macOS | `build/Anamorph_artefacts/JuceLibraryCode/AppIcon.icns`, copied into each built `.app`, `.vst3` and `.component` under `Contents/Resources`; CFBundleIconFile set in each plist. ICNS **icp5/32 and ic10/1024** with these two inputs. | Native plutil, iconutil extraction, sips decode, container parsing and exact generated-to-bundle copy checks, before and after existing signing/staging. Intel job checks VST3/AU; Standalone is disabled there. |
| Windows | Same generated directory: `icon.ico`, `Anamorph_resources.rc`; linked RC object embeds icons into EXE and VST3 PE. VST3 root has `Plugin.ico` and `desktop.ini`. ICO **16/32/48/256**, DIB+mask below 256, PNG at 256. | Native Windows loader resource APIs enumerate RT_GROUP_ICON and read RT_ICON from actual EXE/VST3; every embedded payload must match generated ICO. Check VST3 sidecars and repeat against staged output. |
| Linux | Existing x86-64 Standalone ELF and VST3 `.so`; no conventional ICO/ICNS generation or portable embedded ELF icon. | Native Linux outputs/header checks and all existing audio/host/staging gates. No global icon installation. |

Windows selects the smaller source whose actual dimensions meet the request: 32 for 16/32,
1024 for 48/256. CMake passes the big argument first while juceaide names the arguments small
then big; selection is dimension-based, so do not invert the configured files to compensate.
Raster resizing halves in stages while larger than twice the target, then draws centered with
reduction only. Equal-size inputs are decoded directly. DIB alpha <=5 becomes transparent.

macOS writes one supported-size entry per supplied source, not a full iconset. Its forced-512
fallback only runs if the smallest source is >512, so does not apply here. No native 16/20/24/
48/64/128/256/512 entry is synthesized. The OS must choose/resample entries for those display
sizes. Neither missing slots nor encoder differences alone establish a visual defect; actual
Finder/Dock behavior remains an explicit manual test. Do not claim the full independent
iconset is what JUCE embeds.

## Validation and reproduction

The no-icon baseline was captured from a clean `git archive` of `098884e` before application
integration and built separately, using the existing pinned Clang 22.1.8 installer and normal
Release options. Both self-test suites and the committed 32-scenario `AnamorphDspDump` harness
are compared with the integration build on the same machine/architecture/flags. Source identity,
parameter/state/latency code and supported formats are also compared with that baseline.
Concrete local results and any limitations are recorded below before the branch is submitted.

The PR's exact-head **push** run is authoritative for native outcome; the same-repository PR
run additionally executes the existing merge-check. Existing DSP/state/pluginval, sanitizer,
compatibility, static-analysis and package gates are retained. No test tolerance is relaxed.

`scripts/check-icons.py` uses only Python's standard library plus native macOS utilities.
Its `--self-test` exercises valid synthetic inputs and corrupted PNG/ICO/ICNS/group/hash/CMake
fixtures, with no build, download or source artwork dependency. CI runs it in each job before
that job's icon checks. It rejects running platform checks on another operating system.

```sh
python3 scripts/check-icons.py --self-test
python3 scripts/check-icons.py
python3 scripts/check-icons.py --platform linux --report build/icon-validation-linux.json
```

Native CI build reports and raw generated containers are uploaded as
`Anamorph-macOS-icon-validation` and `Anamorph-Windows-icon-validation`. JSON records actual
frame sizes, bundle plist values/paths or native PE groups. The normal customer artifacts
contain the actual bundled/embedded resources. Build outputs are evidence, not a release.

Pending/manual commands on macOS after the existing universal configure/build:

```sh
python3 scripts/check-icons.py --self-test
python3 scripts/check-icons.py --platform macos --report build/icon-validation-macos.json
python3 scripts/check-icons.py --platform macos --staged-dir dist/Anamorph-macOS
iconutil --convert iconset --output /tmp/Anamorph-JUCE.iconset build/Anamorph_artefacts/JuceLibraryCode/AppIcon.icns
```

Inspect `build/Anamorph_artefacts/Release/Standalone/Anamorph.app`,
`VST3/Anamorph.vst3` and `AU/Anamorph.component`, each with `Contents/Info.plist` and
`Contents/Resources/AppIcon.icns`; repeat against the signed staging tree. Native utility
checks are not Finder/Dock screenshots. A sparse-ICNS rejection is a real failure to investigate,
not a reason to label prepared files verified. Conventional precomposed rounded resources are
not unmasked Composer layers; no second mask or inset is applied in this implementation.

Windows, following the existing x64 configure/build:

```powershell
python scripts/check-icons.py --self-test
python scripts/check-icons.py --platform windows --report build/icon-validation-windows.json
python scripts/check-icons.py --platform windows --staged-dir dist/Anamorph-Windows
```

The checker opens `build/Anamorph_artefacts/Release/Standalone/Anamorph.exe` and
`VST3/Anamorph.vst3/Contents/x86_64-win/Anamorph.vst3` as data/image resources, never executing
the DLL. Explorer, Start Menu/taskbar and 100/125/150/200%+ DPI need a real Windows session.
Folder system attributes/caches and archive transport remain separate shell-display questions.

### Local result snapshot

- Clean no-icon baseline, Ubuntu 24.04 / Clang 22.1.8 / Release: **944 DSP checks, zero
  failures; 5,942 state checks, zero failures** (75 DSP tests plus the clamp guard, 142 state
  tests). The committed DSP dump self-check passed all 32 repeatable/distinct scenarios.
- State-test labels reach 145, with 13–15 unassigned: **142 cases**, as the existing testing
  procedure states. No test or compatibility contract changed.
- The fresh integration build passes the same **944 DSP + 5,942 state checks**, and all **32
  DSP dump scenarios match the baseline byte for byte**, including reported latency. Linux
  VST3/Standalone ELF and icon source/resource checks pass. Both builds use the same pinned
  JUCE revision and Clang 22.1.8; the integration build uses the documented local JUCE path.
- Seven existing JUCE-cache tests pass. Source/resource checker liveness and existing doc,
  portability, realtime, dispatch and citation checks pass. Linux VST3 pluginval passes **strictness 10**, deterministic **3/3** and randomise **3/3**.
  The existing Clang warning gate finds no new first-party diagnostics and the ABI gate passes.
  [The compatibility audit](native-icons-round1/compatibility-audit.json) records protected Git
  objects, identical raw VST3 metadata and the shared DSP-dump digest/latency set. Native CI
  and manual shell display remain separate evidence.
- [Portable frame comparison](native-icons-round1/portable-frame-comparison.png) uses original
  approved direct renders and the **pinned portable juceaide encoder**, at actual 16/32/48/256
  pixels on dark/light backgrounds. It is **not** a Windows shell screenshot, PE validation or
  macOS native rendering. [Measurements](native-icons-round1/portable-frame-comparison.json)
  record decoded RGBA/composited differences and alpha bounds; these are resampling/encoding
  measurements, not a product-design score. Alpha bounds agree at each Windows size; both
  curves/compact interaction remain readable. 16 px is slightly softer, without a material
  defect requiring a workaround. 32 px differs negligibly when composited. 48/256 retain
  the regular motif without clipping or a conspicuous corner matte.
- The portable ICNS encoder writes exactly icp5/32 and ic10/1024; alpha is identical to the
  corresponding source PNGs. Maximum per-channel mean RGB difference is <0.04 at 32 and
  <0.001 at 1024, consistent with pixel rounding. Native output must still be checked by CI.

### Actual native CI evidence and discovery regression

[Build & Validate run 39951551477](https://github.com/skyRolly/Anamorph/actions/runs/39951551477)
at `46f677c20d068588fe460fc12e994d7d85b470a5` built the actual native resources.
[The native audit](native-icons-round1/native-platform-audit.json) records artifact digests,
native reports and decoded-pixel comparisons. Windows resource APIs found IDI_ICON1 and
IDI_ICON2 in both EXE and VST3, each containing **16/32/48/256**; every embedded payload
matched the generated ICO. Decoded frames are byte-identical to the portable study already
visually inspected. macOS app/VST3/AU plists reference AppIcon.icns, with native
plutil/iconutil/sips checks passing; the native 32/1024 alpha channels match the approved
inputs exactly. Intel macOS VST3/AU checks also passed. These are resource results, not a
claim that the entire initial workflow passed.

That run exposed a real Windows discovery regression: JUCE writes `desktop.ini` into all four
Visual Studio configuration bundle directories during configure, while only Release contains
a compiled module. The old directory count rejected the tree before pluginval or staging.
The shared `scripts/find-windows-vst3.ps1` now requires
`Contents/x86_64-win/Anamorph.vst3` and still refuses zero or multiple built bundles.
Its `-SelfTest` reproduces all four shells, accepts one compiled configuration, and rejects
two; Windows additionally checks the System attribute. The filesystem portion passed in
PowerShell on Linux; only native Windows CI validates the Windows attribute and actual PE.
No validation retry/strictness/pass-count policy or installer script changed.

The initial push-only source-lint citation gate used an earlier commit with stale anchors as
its event base. References were corrected against main, without relaxing the checker. The
discovery correction is checked against both `46f677c` and the starting main before push.
Current-head native pluginval/staging and the full matrix must pass before review readiness.

Native platform results are scoped to the CI reports above; no Finder/Dock/Explorer/DAW display
result is inferred. Missing native 20/24/64/128/512 slots are not silently simulated as actual frames.

## Compatibility, deferred work and rollback

Version **0.9.10**, C++23, CMake floor, JUCE pin, formats, architectures and platform minimums
are unchanged. Product `Anamorph`, vendor `RollyTech`, manufacturer `RTec`, code `Anmr` and
bundle ID `com.rollytech.anamorph` remain frozen. All `src/` and existing audio/state tests,
parameter/serialization contracts, real-time paths and reported latency are unchanged.
No runtime network/filesystem image load is added. Installer/release configurations are untouched.

Prioritize a separate **Windows installer PR** next: add SetupIconFile and a stable installed
icon resource for `UninstallDisplayIcon`. Its current `{app}\Anamorph.exe` target is absent
when optional Standalone is not installed. Test VST3-only, Standalone-only and combined installs,
shortcuts, Explorer attributes, uninstall and rollback before claiming installer branding.

Then implement Linux desktop entry/hicolor PNG and scalable SVG installation, honoring
XDG_DATA_HOME / per-user applications/icons and system-local `/usr/local/share`, existing
executable destinations (`~/.local/bin/Anamorph`, `/usr/local/bin/Anamorph`), path resolution,
theme lookup/cache, symmetric uninstall and real GNOME/KDE/window-manager identity. Preserve
the current atomic replacement/recovery semantics; install no global theme files in CI.

Apple Composer remains a separate genuine `.icon` project, six-appearance, Xcode/actool and
older-OS fallback validation round. No Developer ID/notarization policy changes here.
DAW plug-in lists remain host-controlled for every format.

Rollback: revert the resource/build/check changes on a corrective branch and rebuild/re-sign
using existing policy. Keep ADR-0062 and the dated evidence append-only; if the decision itself
is reversed, record its replacement/deprecation per ADR_POLICY instead of deleting the ADR.
Do not patch signed bundles, rewrite main, change IDs, remove presets or alter sessions.
A source-design rollback is not needed in the icon repository.
