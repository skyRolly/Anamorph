# Product namespace migration — 2026-10-23

Starting main: `098884e7fe12049010f2eaf8379e1514efdad0ab`.
Branch: `codex/namespace-tech-rolly-anamorph`, stacked on corrected PR #170
(`feature/native-product-icons-round1`, `84d74b4`). PR base must be that branch. Retarget to
main after #170 merges and reconcile ancestry without force-pushing or duplicate implementation.
The separate Windows installer PR stacks next; merges remain owner-controlled.

Owner chose the domain namespace and then approved the concrete implementation/testing
architecture proposal. ADR-0063 remains Proposed; independent human review is required before merge.

## Inventory before editing

A complete local tracked-file content/filename audit covered all 280 main files (and the #170
additions), case-insensitively for `com.rollytech.`, rollytech, BUNDLE_ID, CFBundleIdentifier,
CFBundle, identifier, pkg-ref, AppId, UninstallDisplayIcon, plugin identity, manufacturer code
and plugin code. Main has 62 matching content files and no matching tracked filename. The full
raw before-edit scan is in session evidence. Counts include parameter-identity discussions.

| Tracked file | Matching lines |
|---|---|
| `CHANGELOG.md` | 6 |
| `CMakeLists.txt` | 2 |
| `EULA.md` | 2 |
| `NOTICE` | 1 |
| `PRIVACY.md` | 2 |
| `README.md` | 4 |
| `SUPPORT.md` | 1 |
| `TRADEMARKS.md` | 5 |
| `docs/COMMERCIAL_STATUS.md` | 1 |
| `docs/DOCUMENTATION_COVERAGE.md` | 33 |
| `docs/HANDOVER.md` | 1 |
| `docs/KNOWN_ISSUES.md` | 8 |
| `docs/POSTMORTEMS.md` | 5 |
| `docs/REPOSITORY_MAP.md` | 1 |
| `docs/architecture/API_REFERENCE.md` | 1 |
| `docs/architecture/PARAMETER_REFERENCE.md` | 2 |
| `docs/architecture/PARAMETER_REGISTRY.md` | 2 |
| `docs/architecture/RELEASE_HARDENING_PLAN.md` | 4 |
| `docs/architecture/design-decisions/ADR-0010-host-hidden-internalstate.md` | 1 |
| `docs/architecture/design-decisions/ADR-0011-linux-x11-cpu-render.md` | 1 |
| `docs/architecture/design-decisions/ADR-0023-vendor-manufacturer-code.md` | 12 |
| `docs/architecture/design-decisions/ADR-0038-gesture-state-is-void-once-topology-moves.md` | 2 |
| `docs/architecture/design-decisions/ADR-0039-a-gesture-owns-the-world-it-was-latched-in.md` | 6 |
| `docs/architecture/design-decisions/ADR-0043-a-commit-carries-intent-and-a-plan-is-computed-where-it-is-used.md` | 2 |
| `docs/architecture/design-decisions/ADR-0045-a-positional-latch-is-void-once-its-topology-moves.md` | 3 |
| `docs/architecture/design-decisions/ADR-0050-a-gestures-ownership-ends-with-its-last-on-release-action.md` | 10 |
| `docs/architecture/design-decisions/ADR-0053-a-wheel-belongs-to-the-interaction-it-lands-in.md` | 4 |
| `docs/architecture/design-decisions/ADR-0059-version-references-and-release-tags-are-bare.md` | 2 |
| `docs/architecture/design-decisions/ADR_INDEX.md` | 3 |
| `docs/policies/COMPATIBILITY_POLICY.md` | 3 |
| `docs/procedures/CI_CD.md` | 1 |
| `docs/procedures/PACKAGING.md` | 13 |
| `docs/procedures/TESTING.md` | 2 |
| `docs/user/INSTALLATION.md` | 1 |
| `docs/user/USER_MANUAL.md` | 5 |
| `packaging/linux/INSTALL.txt` | 2 |
| `packaging/macos/INSTALL.txt` | 2 |
| `packaging/macos/build-pkg.sh` | 11 |
| `packaging/windows/Anamorph.iss` | 5 |
| `packaging/windows/INSTALL.txt` | 2 |
| `scripts/check-citations.py` | 1 |
| `scripts/check-dispatch.py` | 1 |
| `scripts/check-docs.py` | 4 |
| `scripts/check-portability.py` | 3 |
| `scripts/check-realtime.py` | 8 |
| `scripts/run-pluginval.ps1` | 1 |
| `src/InternalState.h` | 9 |
| `src/PluginEditor.cpp` | 4 |
| `src/PluginProcessor.h` | 1 |
| `src/PresetManager.cpp` | 2 |
| `src/PresetManager.h` | 3 |
| `src/gui/SpectrumImager.cpp` | 24 |
| `src/gui/SpectrumImager.h` | 8 |
| `tests/state_tests.cpp` | 23 |
| `worklogs/CXX23_MIGRATION_0.9.4.md` | 1 |
| `worklogs/LEGACY_AB_SLOT_BASELINE_0.9.7.md` | 1 |
| `worklogs/POST_0.8.12_AUDIT_AND_ROADMAP.md` | 1 |
| `worklogs/PRESET_MENU_AND_IDENTITY_0.9.2.md` | 2 |
| `worklogs/SPECTRUMIMAGER_GESTURE_TOPOLOGY_AUDIT_0.9.8.md` | 4 |
| `worklogs/SPECTRUMIMAGER_REMAINING_OWNERSHIP_AUDIT_0.9.8.md` | 4 |
| `worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_0.9.8.md` | 19 |
| `worklogs/STATE_HOST_HARDENING_0.9.10.md` | 4 |

Historical starting base: `com.rollytech.anamorph`; package receipts under `.vst3/.au/.app`,
plus the uninstalled `.probe` canary. ADR-0023 records the manufacturer change and then-unchanged
bundle/packages; ADR-0062 covers the unchanged B0 inputs. Derived generated defaults include
AAX, ARA and internal framework IDs although those formats are disabled.

## Implementation and verification

Canonical base `tech.rolly.anamorph`; package receipts separately use `.vst3/.au/.app`.
All three conventional macOS bundles use the base with the pinned framework. No independent
RTec/Anmr, RollyTech/Anamorph, AppId `{D1E3D8F8-C9CE-415C-AC73-A6AA842987BD}`, version,
C++ source, existing DSP/state tests, format, latency, state or UI change. No old receipt alias,
fallback or migration machinery. B0 source bytes/provenance are untouched.

```sh
python3 scripts/check-product-identity.py --self-test
python3 scripts/check-product-identity.py
python3 scripts/check-product-identity.py --build-dir build --platform linux --report build/product-identity-linux.json
scripts/run-tests.sh
scripts/run-pluginval.sh 10 deterministic
scripts/run-pluginval.sh 10 randomise
```

The new gate checks real generated metadata/native plists, AU codes and VST3 CIDs. Source-lint,
all native build jobs and expanded pkg validation run its self-test before use. Existing
relocation/overwrite canaries and all DSP/state/pluginval/static/sanitizer gates stay enabled.
The shell preflight regression fixture includes the new source gate's inputs, preserving its
actual next-gate check. Local Linux ELF has no CFBundleIdentifier; its JUCE generation metadata
has the base. No Linux desktop resource is installed by the plugin's current packaging.

Prior native Round 1 reports measured the old base in actual app/VST3/AU plists. The corrected
#170 build is the direct before-migration baseline: expected component CID
`ABCDEF019182FAEB52546563416E6D72`, controller `ABCDEF011234ABCD52546563416E6D72`,
AU `aufx / Anmr / RTec`. Compare actual generated output and CI head before claiming native results.

## Historical residuals and limits

Old namespace literals in ADR-0023's historical decision, ADR-0063 context, Round 1 raw native
reports and its dated integration worklog are preserved evidence. A dated note makes clear that
Round 1's frozen-old-bundle statement no longer governs the namespace. None is an active build,
package, test-fixture or canonical documentation identifier. Native host recall/rescan, macOS
receipt installation and shell cache behavior remain human tests. No synthetic test is labeled
native runtime verification. Exact current-head CI and local results are reported in the PR.

VERSIONINFO premise: pinned `juce_ResourceRc.cpp` emits CompanyName, ProductName,
FileDescription, FileVersion and ProductVersion, without InternalName/OriginalFilename.
An API-boundary fixture now runs the real native reader against those exact keys.
