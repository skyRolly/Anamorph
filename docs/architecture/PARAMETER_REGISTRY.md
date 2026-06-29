# PARAMETER_REGISTRY.md

**Compatibility ledger — core asset.** The authoritative record of every host/state
parameter. Changing this surface breaks sessions, automation, and presets in the field.

## INVARIANT (binding)

> **Parameter IDs are immutable.** Removing or renaming an existing Parameter **ID** is
> prohibited. A *display name* may change (the ID is the contract); changing serialization
> semantics, range meaning, or default in a way that alters recall requires an **ADR** +
> migration support + Architecture Review.

Enforced by convention + the version field: all parameters use `ParameterID { id, kVersion }`
with `kVersion = 1`. The string `id` (not the display name) is the persistent key.

Evidence [Verified]: src/PluginParameters.cpp:13 (`kVersion = 1`), :67-68; src/PluginParameters.h:14-88.

## APVTS parameters (host-visible)

Type: F=AudioParameterFloat, C=AudioParameterChoice, B=AudioParameterBool, I=AudioParameterInt.
"Host Visible" = appears in the host's parameter/automation list (every VST3 APVTS parameter
does, regardless of the automatable flag — see note ‡). "Auto Safe" = `withAutomatable(true)`.
"Serialized" = saved in `apvts.copyState()`. Exclusions (A/B, Undo, Preset) in footnotes.

| ID | Display name | Type | Default | Range | Host Visible | Auto Safe | Serialized |
|---|---|---|---|---|---|---|---|
| `channelMode` | Input Channel | C | Stereo (0) | Stereo/Left Only/Right Only | yes | yes | yes |
| `monoSum` | Mono | B | false | — | yes | yes | yes |
| `swap` | Swap L/R (M/S) | B | false | — | yes | yes | yes |
| `inputBalance` | Input Balance | F | 0.0 | −1..+1 | yes | yes | yes |
| `polarityL` | Phase Invert L/M | B | false | — | yes | yes | yes |
| `polarityR` | Phase Invert R/S | B | false | — | yes | yes | yes |
| `msMode` | M/S Mode | B | false | — | yes | yes | yes |
| `drive` | Drive | F | 0.0 | 0..24 dB | yes | yes | yes |
| `algorithm` | Algorithm | C | Velvet Noise (1) | Haas/Velvet Noise/Chorus/Dim-D | yes | yes | yes |
| `amount` | Amount | F | 0.0 | 0..1 | yes | yes | yes |
| `haasDelay` | Haas Delay | F | 12.0 | 1..35 ms | yes | yes | yes |
| `haasSide` | Haas Focus ‖ | C | Left (0) | Left/Right | yes | yes | yes |
| `velvetDensity` | Velvet Density | F | 0.5 | 0..1 | yes | yes | yes |
| `chorusRate` | Chorus Rate | F | 0.5 | 0.05..5 Hz (skew 0.4) | yes | yes | yes |
| `chorusDepth` | Chorus Depth | F | 0.5 | 0..1 | yes | yes | yes |
| `dimMode` | Dimension Mode | C | Classic (1) | Subtle/Classic/Wide/Lush | yes | yes | yes |
| `width` | Width | F | 1.0 | 0..2 | yes | yes | yes |
| `mbEnable` | Multiband Enable | B | **true** ¶ | — | yes | yes | yes |
| `mbBands` | Multiband Bands | I | 4 | 1..4 | yes ‡ | **no** | yes |
| `mbSolo` | Multiband Solo | I | 0 | 0..15 (4-bit mask) | yes ‡ | **no** | yes † |
| `mbFreqLow` | Multiband Split 1 | F | 180.0 | 20..20000 Hz (log) | yes | yes | yes |
| `mbFreqMid` | Multiband Split 2 | F | 800.0 | 20..20000 Hz (log) | yes | yes | yes |
| `mbFreqHigh` | Multiband Split 3 | F | 3000.0 | 20..20000 Hz (log) | yes | yes | yes |
| `mbWidthLow` | Multiband Width 1 | F | 1.0 | 0..2 | yes | yes | yes |
| `mbWidthMid` | Multiband Width 2 | F | 1.0 | 0..2 | yes | yes | yes |
| `mbWidthHiMid` | Multiband Width 3 | F | 1.0 | 0..2 | yes | yes | yes |
| `mbWidthHigh` | Multiband Width 4 | F | 1.0 | 0..2 | yes | yes | yes |
| `monoMakerOn` | Mono Maker | B | false | — | yes | yes | yes |
| `monoMakerFreq` | Mono Maker Freq | F | 120.0 | 20..500 Hz (log, centred 120) | yes | yes | yes |
| `mix` | Mix | F | 1.0 | 0..1 | yes | yes | yes |
| `outputGain` | Output Gain | F | 0.0 | −24..24 dB | yes | yes | yes |
| `outputBalance` | Output Balance | F | 0.0 | −1..+1 | yes | yes | yes |
| `autoGainMatch` | Level Match | B | false | — | yes | yes | yes |
| `solo` | M/S Solo | C | Off (0) | Off/Mid/Side | yes | yes | yes |
| `bypass` | Bypass | B | false | — | yes (host bypass) | yes | yes ◊ |
| `advancedMode` | Advanced Mode | B | false | — | yes | yes | yes ◊◊ |

36 APVTS parameters. Evidence [Verified]: src/PluginParameters.cpp:114-198.

Footnotes:
- **‡** `withAutomatable(false)` is set on `mbBands`/`mbSolo`, but it does **not** hide them in
  all hosts (REAPER lists every VST3 parameter). They remain host-visible; only the automatable
  flag is off. Source: src/PluginParameters.cpp:148-156, :183-190.
- **†** `mbSolo` is **excluded from presets** (`isPresetExcluded`): a preset load resets solo to
  off. It still travels with A/B + Undo. Source: src/PluginParameters.h:84-87.
- **◊** `bypass` is a **view param** (`pid::viewParams`): excluded from A/B, Undo, and presets,
  but still serialized in the main session state. Source: src/PluginParameters.h:70-72.
- **◊◊** `advancedMode` is excluded from **presets** but travels with A/B + Undo (0.8.2 "ADV
  travels with A/B"). Source: src/PluginParameters.h:84-87; README:94-95.
- **‖** Display name renamed `Haas Side` → `Haas Focus` in 0.8.6; the **ID `haasSide` is
  unchanged** (the immutability invariant in action). Evidence [Partially Verified]: README:37;
  src/PluginParameters.cpp:135-136.
- **¶** The APVTS default is `true` (the user-facing default when Advanced Mode is on). The
  `EngineParameters` POD default is **`false`** (src/dsp/EngineParameters.h:72): when Advanced
  Mode is off, `toEngine` skips the multiband section, so the engine sees the neutral POD default
  and the multiband stage is inactive in Simple mode. The two differing defaults are intentional
  and serve different roles — host-facing default vs. Simple-mode neutral state.
  Evidence [Verified]: src/PluginParameters.cpp:147,263-294; src/dsp/EngineParameters.h:72.

## Host-hidden parameters (InternalState — NOT APVTS)

Deliberately kept out of the APVTS/VST3 tree so the host cannot list them (the only reliable
way to hide a parameter). Serialized in `ANAMORPH_INTERNAL`; never in A/B/Undo/presets.

| Identifier | Purpose | Default | Host Visible | Serialized |
|---|---|---|---|---|
| `int_oversample` | Oversampling factor (drives DSP + PDC) | 1 ("Off/1x") | **no** | yes |
| `int_uiScale` | Window size | 3 ("M") | no | yes |
| `int_scopePersist` | Vectorscope persistence | 0.5 | no | yes |
| `int_metersOn` | Show meters | false | no | yes |
| `int_tooltipsOn` | Tooltips | false | no | yes |
| `int_uiAnimations` | UI animations | true | no | yes |

Evidence [Verified]: src/InternalState.h:31-55.

## Deprecations / removals (history)

| Former APVTS ID(s) | Change | When | Migration |
|---|---|---|---|
| `oversample`, `uiScale`, `scopePersist`, `metersOn`, `tooltipsOn`, `uiAnimations` | **Removed from APVTS**, moved to `InternalState` | 0.8.4 | `InternalState::migrateFromLegacyApvts` reads the legacy PARAM nodes from old saved state. |

This is the one precedent for a parameter-surface change. It was done **with a migration path**
(the model the compatibility policy requires). Evidence [Partially Verified]: README:49-58;
src/InternalState.h:95-122; src/PluginProcessor.cpp:345-348.

## Introduced / Deprecated columns

`Introduced` per-parameter version is **Unverified** for the bulk of the set: the repository
has **no git tags**, so exact introduction versions cannot be proven from the tree. Documented
changes (the `haasSide` rename, the 0.8.4 removals above) are cited where evidence exists.
`Deprecated`: none of the current 36 APVTS IDs is deprecated.
