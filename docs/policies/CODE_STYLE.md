# CODE_STYLE.md

Repository Governance Policy. C++ conventions, derived from the existing codebase. New code must
read like the surrounding code.

## Language / build

- **C++17**, no compiler extensions (`CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_EXTENSIONS OFF`).
- Builds clean under `juce::juce_recommended_warning_flags` (CMakeLists.txt:143,165) — keep it warning-free.

## Naming (observed)

| Element | Convention | Example |
|---|---|---|
| Class / struct | PascalCase | `AnamorphEngine`, `MultibandWidth`, `LoudnessMatch` |
| Method / function | camelCase | `processBlock`, `setCrossovers`, `predictLatency` |
| Member variable | camelCase (no `m_` prefix) | `switchState`, `bypassBlend`, `mbRunning` |
| Constant | `k`-prefixed | `kVersion`, `kNoInject`, `kAlignMix` |
| Namespace | lowercase | `anamorph`, `pid`, `iid` |
| Parameter IDs | `pid::` string constants | `pid::haasSide` = `"haasSide"` |

## Structure

- DSP core lives in `namespace anamorph` under `src/dsp/`; the wrapper/editor are in the global
  namespace under `src/`.
- Small DSP utilities are **header-only** (`MidSide.h`, `Correlation.h`, `LevelMeters.h`,
  `ScopeBuffer.h`); larger modules are `.h`/`.cpp` pairs.
- One responsibility per file; a banner comment block (`// ===== Name ===== ...`) documents each
  class's purpose at the top.
- Member initialisers in the header (`float switchPhase = 1.0f;`).
- Use `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` on owning classes.

## Real-time discipline (cross-ref `REALTIME_AUDIO_POLICY.md`)

- Mark audio-path methods `noexcept`.
- Allocate only in `prepare()`. Never `new`/resize on the audio path.
- Qualify JUCE types (`juce::SmoothedValue`, `juce::dsp::Oversampling`) rather than blanket `using namespace juce`.

## Comments

- Comments explain **why**, not what — especially the rationale behind a click-free transition, an
  ordering choice, or a compatibility quirk. The codebase frequently cites the originating feedback
  (`(#3)`, `(Issue 2)`, `(Known Issue #1)`); keep that traceability when extending such code, and
  reflect significant ones into an ADR/POSTMORTEM.
- Formatting: 4-space indent, braces on their own line (Allman), as in existing files.

## Tests

- New DSP behaviour gets a deterministic test in `tests/dsp_tests.cpp` using the existing
  `check(cond, "what")` harness (`TESTING_POLICY.md`).

This is a descriptive style policy; non-structural style tweaks do not require an ADR, but they
must not change behaviour (a pure-formatting change is **not** a CHANGELOG entry — see
`CHANGELOG_POLICY.md`).
