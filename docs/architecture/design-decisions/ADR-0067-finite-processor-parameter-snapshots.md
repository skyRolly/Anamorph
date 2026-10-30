# ADR-0067 — Finite processor parameter snapshots

**Status:** Accepted — owner direct approval in this chat on 2026-10-30 (Asia/Shanghai).
**Architecture Review:** APPROVED for the complete proposal in PR #187, including ADR-0067's
0 dB Output Gain fallback/Density consequences and ADR-0068's nested-callback support boundary.
This approval is not evidence of implemented fixes or new-binary manual validation.
**Implementation:** Immutable parameter/default bindings, finite snapshot and display guards, and private-copy
repair of the unusable-value/usable-raw restore window are implemented in PR #187.
State tests 146/147 exercise the production processor; PR #187 merged at `b085c87`.
Final-main Build 1956 and normal Release 5 passed; the owner attested targeted
human validation complete on 2026-10-30. Final unpublished draft/provenance and separately
recorded architecture/manual approvals: `worklogs/RELEASE_READINESS_0.10.0.md`.

## Context

KI-029 remains reproducible at main `0ac384088104dfd4dd128ab6751c828c6cd51be8`.
`ParamPointers::toEngine` reads the APVTS atomics for processing, preparation and latency
prediction. Continuous NaNs reach engine targets; discrete readers call `roundToInt` without
a finite predicate. The final audio self-heal can make the output finite by muting it. That is
not an audible-parameter contract. The numeric-NaN XML formatter repair is already complete.

## Problem

Choose deterministic NaN/positive-infinity/negative-infinity behavior at the processor's DSP
boundary without changing valid automation/readback, session readers or the bulk-apply handshake.
This is a parameter-meaning decision under ADR_POLICY, not approval inferred from a passing test.

## Options

| Candidate | Host/readback and recall | Audio and lifetime | Ownership/complexity |
| --- | --- | --- | --- |
| Preserve | No compatibility delta | Existing sustained mutes and module-specific recovery remain | Smallest but fails the objective |
| Stateless default at the DSP boundary | Exact current host/readback/text/serialized values remain; invalid session/preset resolution stays as today | Every non-finite raw atomic uses that parameter's actual declared default; no history or reset lifetime | Bounded checks on existing loads; immutable defaults; no new cross-thread state |
| Reject/normalize ingress | Host readback and text entry change; must retain finite raw values of custom discrete parameters and investigate every JUCE notification | Can keep the preceding parameter target, including Output Gain | Replacing/interposing stock float setters and parsers is materially broader; does not alone cover transient/direct raw writes |
| Last finite target at the DSP boundary | Host readback remains; invalid saved-state fallback still differs from live history | Avoids a default-gain jump; must specify initial target, inactive modules, reset/prepare, A/B lifetime and partial bulk snapshots | A cache must have one owner. A mutable cache shared by the three `toEngine` readers is rejected. An audio/prepare-only cache with advancement only after trusted ADR-0057 snapshots is a distinct, more complex alternative |

## Decision (approved 2026-10-30)

Select **stateless parameter-default substitution in `ParamPointers::toEngine`**. Cache each
parameter's denormalized declared default at `bind`, beside its existing atomic pointer, and make
every load finite **before** any float-to-integer conversion, enum conversion or Boolean threshold.
The finite branch returns exactly the loaded float: no new clamping, snapping or store/report pass.
Use the layout's defaults, not `EngineParameters`' neutral initializers: `dimMode` and `mbEnable`
already demonstrate why those are different. Preserve all existing `seq_cst` loads.

Also guard non-finite inputs **before** the integer conversions in host text formatting
(`RawChoice::indexFor`, `RawInt::getText`, percentage/Hz formatters). Numeric `getValue()` still
preserves the existing raw value. A NaN display formats the declared default; custom discrete
+Inf/−Inf displays format the upper/lower endpoint already rendered by APVTS range conversion.
Finite text output and `getValueForText` parsing are unchanged. Use the existing formatter and
labels, not new user-interface wording. A DSP-only guard would leave these conversions unsafe.

The table describes the value *actually reaching the cached raw atomic*, after the existing setter
or text conversion. It deliberately does not promise that every non-finite host input survives
those conversions:

| Input/path | Approved behavior |
| --- | --- |
| Raw NaN | DSP substitutes that parameter's declared default; host `getValue()` remains unchanged |
| Raw +Inf | Same default substitution; a +Inf already converted to a finite upper endpoint remains that endpoint |
| Raw −Inf | Same default substitution; a −Inf already converted to a finite lower endpoint remains that endpoint |
| Text | Existing parser/readback preserved. For example Width `nan` reaches the fallback; Mono Maker frequency `nan` already becomes finite 500 Hz and remains 500 Hz; Boolean text still resolves Off |
| Session/undo/A/B | Existing usable-`raw` > usable-`value` > default resolver retained; invalid values do not acquire a new serialized meaning |
| Preset file | Existing usable denormalized `value` > default rule retained |
| `value="nan" raw="0.71"` restore window | Resolve the unusable `value` from the authoritative usable `raw` on the private restore copy before JUCE installs it. Final exact-raw reassertion is unchanged; no transient NaN/default is installed for this field |
| Fresh/reset/re-prepare | Exactly the same stateless fallback; no remembered runtime target to discard or share |
| Finite automation | Bit-identical mapping and host readback; subsequent finite input immediately resumes the normal smoothed path |

**Tradeoff requiring an explicit ruling:** Output Gain falling back from −18 dB to its 0 dB default
is an 18 dB target increase. The existing smoother still applies, but this is an audible change,
not a promise of constant loudness. Density falling back from 0.3 to 0.5 is also a real sound
change and may invalidate Level Match. The owner explicitly accepted these consequences in approving this decision on 2026-10-30. No output limiter is proposed.

## Consequences and compatibility

**Implementation correction (2026-10-30):** the approved proposal incorrectly attributed host
session restore atomicity to ADR-0057. Host session installs do not enter that handshake, and
no new host-thread bulk counter/ordering is introduced. The existing resolver already selects
usable `raw`; the existing private-copy repair now also repairs an unusable `value` when `raw`
is usable. This realizes the same resolved sound before JUCE's first notification. It changes
the re-saved malformed `value` spelling to the resolved finite value, preserving field meaning,
exact final raw readback, valid finite bytes and all accepted parser boundaries. Dedicated
window regressions must prove the first notification and final restore, not infer a handshake.

Preserve IDs, ranges, defaults, ordering, automation flags, serialization fields and all valid
finite session/preset bytes. No schema migration is needed: older finite sessions take the same
branch and older invalid sessions retain their existing resolver. Numeric NaN serialization,
the direct engine/module self-heal protections, and Accepted ADRs remain in place.

The processor interpretation of invalid Density changes. State tests 128/138 contain explicit
premises about the prior held-density behavior; after approval, amend only those related premises
and pin the newly chosen audio/Level-Match result. Keep direct-engine Tests 64/74 and their held
density guarantees. Do not weaken unrelated finite, A/B provenance or match-memory assertions.
ADR-0007/0009 remain Accepted; this proposal does not silently edit their historical measurements
or claim approval of their previously open parameter-policy question.

## Implementation and acceptance plan

1. Introduce an immutable pointer/default pair or equivalent binding helper and route **all**
   snapshot loads through it; add the corresponding pre-conversion text-format guards. No lookup,
   allocation, CAS, new lock, cache mutation or parameter
   writes on the audio path; unchanged signal order and latency formula.
2. Extend `--nonfinite-audio-probe` into default regressions after approval. Keep its original
   production-path mute failure as the negative control. Add whole-snapshot finite assertions,
   every host parameter (including discrete controls), all three non-finite values, host text
   parsing **and formatting**,
   Simple/Advanced, initial/reset/re-prepare, live automation and unchanged readback.
3. Exercise sessions, presets, both A/B slots and undo, including the unusable-value/usable-raw
   window paused through the existing production seams. Assert the final usable raw sound.
4. Compare finite controls bit-for-bit through both paths; verify recovered audio and Level Match,
   not only `isfinite(output)` (the existing self-heal already returns finite zeroes).
5. Run allocation/static realtime guards, full suites, required sanitizers and both pluginval modes;
   then the mandatory three-platform release matrix. Obtain targeted human audition/automation
   approval for the newly chosen invalid-input sound, including Output Gain and Density.

## Related code and evidence + confidence

Evidence [Verified]:
- Source: `src/PluginParameters.cpp` (`RawChoice`, parsers, `ParamPointers::bind/toEngine`),
  `src/PluginParameters.h` (`sessionNormalisedValue`), `src/SerializedXml.h`.
- Tests: DSP 59/64/74; State 123/128/138/142; `--nonfinite-ingress-probe`,
  `--nonfinite-save-probe`, new opt-in `--nonfinite-audio-probe`.
- Measurements and exact release baseline: `worklogs/RELEASE_READINESS_0.10.0.md`.

The original defect, existing resolver and implemented production-path regressions are verified.
The original audio diagnostic failed 24 of 288 checks before the fix; State test 146 now adds
exact default-twin assertions. State test 147 covers every registered parameter and the restore
window. Fresh platform/sanitizer validation and targeted human approval remain separate gates.
