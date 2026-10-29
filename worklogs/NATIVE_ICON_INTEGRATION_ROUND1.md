# Native icon integration — Round 1

## Current governance status — record updated 2026-10-24 (Asia/Shanghai)

ADR-0062 is **Accepted**; the owner explicitly confirms final architecture/decision approval
for this completed scope. Architecture review and owner-decision blockers are **Closed**.
[PR #170](https://github.com/skyRolly/Anamorph/pull/170) is merged as
`b4a95b3a6d86acf222de523c6fb04de23ad8e602`, verified in current main ancestry. The original final-approval date is not
recorded. [ADR_INDEX.md](../docs/architecture/design-decisions/ADR_INDEX.md#owner-approval-reconciliation--record-updated-2026-10-24)
holds the approval provenance, merge ancestry, exact CI evidence and remaining validation.
Approval does not establish an unperformed runtime/manual test result.

## Historical implementation and validation record

The dated entries below preserve the status and evidence at implementation time. Their
Proposed/unmerged/review-pending statements are superseded by the current governance record
above; earlier native pending statements must be read with their exact later CI evidence.
No historical measurements, hashes or release evidence are rewritten.

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


## Review findings A1/A2 — 2026-10-23 follow-up

The pre-fix head was `feebf066f1da86689be9a868a2ffc637a0f503d0`. Reproduced both defects:
(1) preserve platform-mismatch rejection but return success on matching platforms: the old
self-test still passes; (2) append corrupt bytes to the 32px PNG in an isolated checkout
with no build: the old full preflight exits 0. No source input was changed in the shared tree.

The corrected self-test runs 42 matching-platform native fixtures through production
`check_native`: 12 positive build/staged + Standalone on/off cases and 30 required negative
cases. Windows stubs only the loader API boundary; production group parsing/frame comparisons
and target selection run. macOS stubs only plutil/iconutil/sips while production bundle/plist/
ICNS selection and validation run. Linux uses synthetic x86-64 ELF headers and real filesystem
selection. These are portable liveness tests, not native build or visual certification.

Six source mutants are killed: matching-platform inspection bypassed, Windows resource validation
disabled, missing Plugin.ico accepted, altered Plugin.ico accepted, macOS resource comparison
skipped and Linux ELF validation skipped. Missing and altered fixtures fail for their expected
resource-specific reason, not platform mismatch. The PNG/provenance guard also rejects a changed
PNG with a coordinated digest edit, using an independent approved digest and source commit.

The shell preflight now runs source/provenance/CMake/PNG validation and self-tests before other
gates, regardless of build availability. Existing compiled Release outputs trigger native
inspection without a rebuild; otherwise it prints exactly the native skip, candidate paths and
build/check commands. Five committed shell-entry regressions run in source-lint. Manual isolated
full-preflight controls: old corrupt-source 0; new corrupt-source 1; new wrong CMake path 1;
new valid no-build 0 with explicit native and suite skips.

Local Ubuntu managed container / GCC 14.2 / pinned JUCE 9.0.4: checker/source/42 native fixtures,
five shell regressions, six mutants, docs and citation checks pass. Fresh unchanged application
sources pass 944 DSP and 5942 state checks. The native build, full built preflight, pluginval
and new cross-platform CI are recorded separately on the PR head; no unobserved result is
upgraded from Round 1. The local GCC ABI is not the shipping Clang ABI; preflight may report
the known floor difference, and that must not be called a green full preflight.

No CMake, namespace, packaging/installer, DSP/UI/state/parameter/latency/version or artwork
change is included in this review fix. Prior native reports remain historical evidence for
their recorded heads.


**Namespace scope amendment, 2026-10-23:** the old bundle/package identifier statements in
this Round 1 worklog and its raw reports describe that historical icon-only head. The separate
ADR-0063 implementation now uses `tech.rolly.anamorph` under the owner's namespace directive
and explicit implementation/testing approval. Raw old native measurements remain unchanged.

## Native macOS fallback implementation — 2026-10-29

The sections above retain their historical Round 1 scope. This follow-up uses the separate
approval in the supplied handoff prompt, recorded in ADR-0062's 2026-10-29 amendment. It does
not reinterpret Round 1 approval or certify a release.

Baseline: `06105461d79cca68389dd8470c316b84bb56252a`, freshly cloned from live main on this Mac;
the older handoff baseline `f83c1ab` was not restored. Existing checkouts were left intact.
Implementation: `686dbf7d9c3234671918e3979b5b79b834b0c43e`. All B0 PNG hashes, production
identities, JUCE pin, compiler flags, DSP/state/parameters and Windows/Linux resources remain
unchanged. Only the macOS resource path and its regression/evidence gates change.

Environment: macOS 15.8.1 (24H32), native x86_64; selected developer directory
`/Library/Developer/CommandLineTools`; AppleClang 17.0.0, Swift 6.2.4, CMake 4.4.4 and Ninja.
Full Xcode is not installed. The default 26.2 SDK and installed 15.4 SDK trigger the pinned
JUCE's SDK availability guard in this environment. The successful build selects the installed
13.1 SDK and retains `CMAKE_OSX_DEPLOYMENT_TARGET=10.13`. No repository toolchain setting is
changed to work around that local header issue.

The installed `/Applications/Anamorph.app` is 0.9.10, bundle ID `tech.rolly.anamorph`, ad-hoc
signed. Its ICNS is the exact supplied native artifact:
`88b9839f2ed351e9e958963476338b1fe474f87a10d5a0aafbc72a3f89bd582d`, 255107 bytes, containing
only `ic10` (1024px PNG) and `icp5` (32px PNG). Native application-bundle rendering reproduces
noise at 16pt/32pt 1x and other affected sizes. At 32pt 1x, premultiplied RGB MAE is
0.3715916, alpha MAE 0.1832874, silhouette IoU 0.81640625 and transparent fraction zero.
Isolated diagnostic identities reproduce the defect without changing production identifiers
or touching caches. Direct embedded PNG decoding alone still succeeds.

Corrected generated, built and staged ICNS bytes have SHA-256
`9915858b2df52c20eb857c3329ac3e52bf1a1aa1f048be46550b499e9be3cb4d` (733407 bytes).
Native entries are `ic04` 16px and `ic05` 32px ARGB; PNG entries `ic11` 32px,
`ic12` 64px, `ic07` 128px, `ic13`/`ic08` 256px, `ic14`/`ic09` 512px and `ic10` 1024px,
plus bounded `info` metadata. No icp4/icp5/icp6 entry remains. All ten standard iconset names
extract and validate. Both the actual built and signed/staged application render at all
twelve logical-size/backing-scale combinations with maximum RGB MAE 0.01041156, zero alpha
MAE and minimum silhouette IoU 1.0, against the approved source selected by actual pixel size.
The real iconutil positive control passes; the malformed old layout is structurally rejected
and its native render fails the pixel gate. Evidence includes before/after PNGs and metrics.

Build commands, from the task workspace (the existing optional local JUCE override points to
a clean checkout verified at `3fb5cbc0340758596dac39f7c8de47d562b4896e`):

```bash
cmake -S work/Anamorph -B work/Anamorph/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 -DANAMORPH_JUCE_PATH=/Users/rolly/Documents/Codex/2026-10-28/product-only-installer-names/work/JUCE-ci-fix
cmake -S work/Anamorph -B work/Anamorph/build -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk
cmake -S work/Anamorph -B work/Anamorph/build -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX13.1.sdk
cmake --build work/Anamorph/build --config Release --parallel 6
```

The first two SDK choices failed compilation at JUCE's availability guard. The 13.1 build
completed from fresh product objects with VST3, AU, Standalone and both test executables,
each universal where applicable. From `work/Anamorph`, subsequent `cmake -S . -B build` and
`cmake --build build --config Release --parallel 6` completed; another build reported no work.
Ninja's actual graph records both PNG inputs and `GenerateMacIcon.cmake` as dependencies,
and every app/AU/VST3 resource copy consumes that generated output. No manual icon replacement
or handoff/sibling-icon checkout is part of the build.

Completed local checks, from `work/Anamorph` (each exit 0):

```bash
python3 scripts/check-icons.py --self-test
python3 scripts/check-icons.py --report build/icon-sources.json
python3 scripts/test-macos-icons.py
TMPDIR=/Users/rolly/Documents/Codex/2026-10-29/e-x/work/tmp python3 scripts/test-icon-preflight.py
TMPDIR=/Users/rolly/Documents/Codex/2026-10-29/e-x/work/tmp python3 tests/juce_cache_tests.py
TMPDIR=/Users/rolly/Documents/Codex/2026-10-29/e-x/work/tmp python3 scripts/test-workflow-routing.py
python3 scripts/check-icons.py --platform macos --report build/icon-validation-macos.json
python3 scripts/check-product-identity.py --self-test
python3 scripts/check-product-identity.py --build-dir build --platform macos --report build/product-identity-macos.json
TMPDIR=/Users/rolly/Documents/Codex/2026-10-29/e-x/work/tmp bash scripts/preflight.sh
python3 scripts/check-icons.py --platform macos --staged-dir dist/Anamorph-macOS --report build/icon-validation-macos-staged.json
```

The icon self-test exercises 49 matching-platform controls; native regressions have three
tests, shell preflight six and JUCE-cache seven. Full preflight completed source/doc/citation
and dependency/integrity controls, then 941 DSP checks and 5787 state checks with zero failures
on native Intel. An earlier preflight stopped at five source/doc commit-boundary anchors;
the code and anchor updates were kept together and the full gate rerun. `/var` symlink ancestry
also made temporary-directory fixtures fail initially; a real workspace TMPDIR resolves this
without altering the Linux checker or any validation rule.

Local staging follows the existing order: dsymutil capture, `strip -x` on each binary,
`codesign --force --deep --sign -`, then copies to `dist/Anamorph-macOS`. All three built and
all three staged bundles pass `codesign --verify --deep --strict --verbose=2`; signature mode
is ad-hoc, both architecture slices are asserted, and the stripped VST3 exports GetPluginFactory.
Staged plist/icon resources match generation exactly. Exact command arrays, exit codes and
diagnostics are retained in the task's `package-commands.json`; render evidence stays outside
the customer payload. No production signing credential is used or changed.

Warnings/limits: the clean compile emitted 116 warning lines in unchanged C++/JUCE sources.
macOS's allocation guard does not assert raw malloc. Local dSYM extraction returned zero but
reported overwritten/mismatched LTO objects, missing symbols and absent arm64 debug data;
these dSYMs are not certified and are excluded from the review artifact. No pinned Linux
warning baseline, new GitHub CI run, pluginval/DAW acceptance, arm64 runtime or other macOS
version is claimed. Existing CI gates, including debug checks, remain enabled.

Launchpad itself was not observed: Launchpad/Dock UI access timed out, and later Finder
automation lost focus. The installed application remains untouched; no Launchpad database,
layout or broad cache deletion was attempted. After reviewing the supplied build, the single
remaining manual step is to replace `/Applications/Anamorph.app` with the review archive's
`Anamorph.app` and inspect its icon in Launchpad. AppKit evidence is not a claim that this
installed Launchpad result was observed fixed. No merge or release publication occurred.

## Owner Launchpad acceptance and GitHub application — 2026-10-29

After the native run and source review above, the owner reported testing
`Anamorph-macOS-icon-review.zip` in macOS Launchpad and confirmed that it works
correctly. The owner then requested applying the fix/patch to the GitHub repository.
This is direct manual confirmation for the reviewed archive, not an inference from
PNG decoding or AppKit metrics. Its SHA-256 is
`2fa74748c250c6d8256781fdebe12e413820f5ee5f481bbfaa2e8dabda39d88b`, and all three
bundles carry corrected ICNS SHA-256
`9915858b2df52c20eb857c3329ac3e52bf1a1aa1f048be46550b499e9be3cb4d`.
The manual test's OS version and architecture were not specified.

The GitHub application commit is `2ff69a5da5f047777c03834725691f2942333f48`, based
on `06105461d79cca68389dd8470c316b84bb56252a`. Before a trailing blank-line cleanup
in `cmake/MacIcon.cmake`, the reviewed patch produced tree
`a28e2f9255a62a1b1d60b241415cffbf77818ae1`, identical to the macOS delivery's final
tree. Runtime source, source artwork, the native generator's behavior and native
rendering checks were not changed during this review/application.

The earlier native run's Launchpad limitation above remains historical; this owner
confirmation closes that manual check for the supplied build. The documented dSYM,
arm64 runtime, other-OS and DAW limits remain separate from this icon repair.

## macOS 26 CI comparison repair — 2026-10-29

[Build run 40880140543](https://github.com/skyRolly/Anamorph/actions/runs/40880140543)
failed the built and staged icon checks in job `123469156120` on `macos-26-arm64`.
The generated ICNS remained byte-identical to the accepted native review:
`9915858b2df52c20eb857c3329ac3e52bf1a1aa1f048be46550b499e9be3cb4d`.
The saved native renders show macOS 26's glass border and inset, which the direct
PNG reference lacks. At 16pt 1x both checks reported RGB MAE 0.2464358660 and alpha
MAE 0.0366115196; this mismatch is host styling rather than the former noisy ICNS
encoding. The Intel macOS job's original comparison passed.

`macos_reference_icon()` now assembles all ten representations independently from
the immutable approved PNG inputs using native sips/iconutil. It does not call the
production CMake generator or read candidate icon bytes. `render-macos-icon.swift`
compares the candidate app against a disposable reference app through IconServices,
with identical host styling and backing scales. Each evidence directory retains
`approved-reference.icns` and the rendered reference/candidate PNGs. Existing RGB,
alpha, silhouette and transparency limits are unchanged, as are the strict
container rules. A fourth native test requires a complete compatible ICNS carrying
different artwork to fail, protecting reference independence. The original bad
small-slot layout remains structurally rejected and a measured native control.
