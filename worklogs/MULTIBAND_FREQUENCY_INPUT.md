# Multiband frequency input and animation correction

Date: 2026-11-08 (Asia/Shanghai). Investigation base: `da0429b9` (`origin/main`).
This is a focused text-entry correction authorized by the owner, with PR preparation
and no merge authorization. Historical shorthand observations in earlier audit records
remain historical. No parameter IDs, numerical ranges/defaults/mapping, saved state,
thread model or DSP code need to change.

## Interpretation recorded before production changes

Normalize whitespace and punctuation, preserving decimal points and a leading minus.
Validate the entire resulting numeric expression and its optional Hz/k/kHz suffix.
Explicit units win; only positive *bare* numbers strictly below 20 imply kHz.
Clamp valid numeric results to 20–20,000 Hz; reject malformed/empty expressions.
Use the same parser for all three Multiband parameters and the inline editor.
Unchanged effective layouts, invalid text and cancellation must be side-effect free.
Apply sound/host/Undo changes synchronously and animate only the displayed positions.

## Source investigation, before implementation

| Path / requirement | Current behavior and cause | Disposition |
|---|---|---|
| Open / initial text | `openFreqEditor` checks the live handle, seeds `freqText(crossover(i))`, tracks typed intent, focuses and selects text. Rounded display strings are not authoritative without an edit. | Preserve. |
| Inline shorthand | `parseFreq` strips `khz `, checks for any `k`, then uses `v <= 20`; exactly 20 becomes 20 kHz. | Use the strict boundary. |
| Explicit Hz | Removing `hz` loses unit information, so `19Hz`/`1Hz` become 19/1 kHz. | Preserve explicit-unit meaning. |
| Host split converters | Split 1/2 use `hzFrom` (bare Hz); Split 3 uses `khzFrom` (inclusive shorthand). | Share Multiband-only interpretation. Keep `hzFrom` on Mono Maker/Chorus. |
| Whitespace / punctuation | Spaces in units work; other whitespace inside numbers and commas/slashes terminate numeric-prefix conversion. `1,5kHz` becomes 1 kHz. Decimal points work. | Normalize before full-expression validation. |
| Zero / negative / limits | Inline commit clamps valid zero/negative and above-range results. | Preserve clamping; prevent float overflow escaping conversion. |
| Invalid input | `getFloatValue` accepts prefixes and returns zero on failure. Invalid/empty inline input is committed at the low geometric limit, with a gesture/Undo; malformed trailing expressions can be accepted. | Reject before a transaction/gesture/store/animation. Host conversion returns current readback when invalid. |
| Enter / focus loss | Both dispatch `commitFreqEditor` through `SafePointer`; mouse-down also commits. | Preserve the callbacks and focus behavior. |
| Escape / untouched dismissal | Escape closes without a store; untouched text is ignored even if automation moved the parameter since opening (ADR-0043). | Preserve. |
| Retyped equivalent input | Bypasses the untouched gate and unconditionally writes the primary inside a gesture. | Add an effective-layout no-op check. |
| Frequency mapping | `freqToX` uses the monotone axis; inverse uses 30 bisections. An unnecessary round trip can perturb the requested float. | Keep mapping; store the parsed frequency directly when projection did not move it. |
| Typed placement | `projectActionGaps` uses 46 px outer margins as well as 46 px inter-split gaps, sliding the whole cluster. Even one split cannot reach either endpoint. | Use full plot bounds for typed placement, preserving inter-split spacing and all other projection paths. |
| Active indices / band counts | One/two/three splits all have the outer margin; an interior split additionally needs room for each neighbor toward the requested edge. | Verify every live index at 2/3/4 bands. An interior parsed endpoint remains constrained by necessary neighbor spacing. |
| Neighbor ownership | Snapshot once after Begin; store ownership is read back; End can replace the pin; every neighbor proves pin, topology and its own original value. | Preserve ADR-0042/0043/0047 and the transaction bracket. A preflight is only a no-op prediction; recompute after Begin. |
| Undo / host | Stable parameter pointers and copied processor callbacks complete the multi-store action even if Begin/value/End destroys the editor. One outer user transaction includes post-End neighbors. | Preserve existing lifetime tests and exact Undo/Redo. |
| Typed animation ON | No sweep request/easing arm; `paint`/`tick` copy authoritative positions and jump. | Arm existing display easing for changed typed plans, before value notifications can repaint. |
| Typed animation OFF | Positions snap on tick/paint when no prior easing exists. | Preserve; also snap a preexisting tail immediately on paint when switched off. |
| Reset | `resetCrossover` requests `onSweep`; editor sets a 0.45 s shared window when enabled. Same 0.105 s exponential display tail. | Preserve. |
| Preset / A/B / Undo / Redo | Editor actions request the shared window; disabled animations bypass it. | Preserve. |
| Drag / wheel / automation | Direct changes track parameters; busy drags clear easing. During an existing sweep the current parameter remains the target. | Preserve; do not delay host/APVTS/DSP writes. |
| Progression / completion | `tick(dt)` uses exponential/log-frequency easing and a 0.3 px terminal snap; tail continues after the shared window expires. | Reuse unchanged rate/curve and coordinated split handling. |
| Interruption / hide / topology | Busy interaction and topology changes snap; hidden components stop ticking; reopening initializes from live parameters. | Verify; preserve authoritative targets and lifetime checks. |

## Alternatives considered

- Extending the generic `hzFrom` would alter unrelated controls. Keep a dedicated shared
  Multiband parser with one suffix check and one numeric validation pass; no regex grammar,
  general parser framework or parameter-architecture refactor is needed.
- A stock string-to-value callback has no invalid-result channel or current parameter
  reference. A small `AudioParameterFloat` subclass can return current normalized readback
  on rejection while retaining stock range/storage/host behavior.
- Keeping the outer margins prevents exact endpoints; changing shared drag projection
  would expand scope. Relax only typed placement to full axis bounds. Endpoint handles
  already exist through host automation; inter-split spacing and hit testing remain.
- Recomputing a preflight plan only before Begin would violate ADR-0043. Repeat the same
  pure plan inside the existing gesture, keeping the no-op check outside it.
- A second animation clock or delaying parameter writes is unnecessary. `dispEasing`
  already retains origins through paint and converges all display positions to live values.
  Arming it directly avoids extending the global knob sweep for an inline edit.

## Reproducible probes

The implementation is limited to `PluginParameters.{h,cpp}` and
`SpectrumImager.{h,cpp}`. `parseMultibandFrequency` validates a normalized decimal
and its suffix, then limits the double-precision result before float conversion.
Only the three Multiband float parameters use the small host-conversion override;
their stock numerical storage, ranges, defaults, IDs and formatting remain intact.
Invalid host text returns current normalized readback. Host-owned gesture handling
is outside this getter; inline rejection opens no gesture or transaction.

Typed projection reserves `index * 46 px` on the left and
`(splitCount - 1 - index) * 46 px` on the right before pushing neighbors. This
reaches an outer endpoint when possible and constrains interior targets only as
ordering/spacing require. Whole-cluster translation would move an unrelated far
neighbor by another 46 px on repeated constrained middle edits even while the pin
stays unchanged. The final projection is idempotent and preserves that far neighbor.
Unprojected targets keep the parsed float rather than an inverse-axis approximation.
The inline text box stays within component bounds when reopened at an edge.

Invalid, untouched, numerically equivalent and effectively unchanged projected
edits return before a transaction/host gesture. The real plan is still recomputed
after Begin. Store readback, pin/topology/neighbor ownership, post-End spreading,
copied processor callbacks and the outer Undo transaction retain their exact checks.
The tiny float tolerance is used only to predict no-ops, never to prove ownership.

Changed typed plans arm `dispEasing` before parameter notifications can repaint.
The existing frame body supplies the same exponential/log-frequency motion and
terminal snap for all moved splits; targets are always live authoritative values.
No shared knob window, timer, timing curve or DSP update is added/delayed. `paint`
also clears a running tail immediately when animations are switched off. Reset,
preset, A/B, Undo/Redo, drag and wheel handlers keep their existing write/sweep paths.
All other source/document changes are regression coverage or evidence-anchor updates.

State test 158 (`tests/multiband_frequency_tests.h`) is added before production edits.
Run `AnamorphStateTests --multiband-frequency-probe` for the required table plus extra
normalization/boundary cases through real host converters and inline commit callbacks,
invalid/no-op effects, all endpoint layouts, Enter/focus/Escape, animation progression,
neighbor convergence, exact Undo/Redo and interruptions. Existing State 75 and the
SpectrumImager callback/frequency-editor lifetime suites retain their reentrant oracles.

Final local verification (Linux x86-64, GCC 14.2, official JUCE 9.0.5
`d60b7a78030661574a615b586d9325ba7a7fa7dd`):

- State 158: **461 checks / 0 failures**, including 57 valid inputs through all
  four production paths, 19 invalid inputs, every active endpoint layout,
  equivalent/crowded-layout no-ops, 60/120 Hz progression, converged neighbors,
  Enter/focus/Escape, rapid commits, real drag/wheel takeovers, host retargeting,
  animations-off paint, topology, hide/show and existing reset/preset/A/B/Undo/Redo.
- Canonical `scripts/run-tests.sh`: **945 DSP checks / 0 failures** and
  **16,535 State checks / 0 failures**, with filesystem-isolation controls.
  Existing State 75 and callback/frequency-editor destruction/Undo tests run in
  that complete suite; registry, state compatibility and unrelated parsers remain guarded.
- Release VST3 and Standalone builds complete with the existing flags. The
  prepared helper caches were regenerated for 9.0.5; no build/dependency pin changed.
- Documentation/release-note, portability, realtime, dispatch, state-coverage and
  citation checks run with their liveness self-tests. Citation changes only
  repair evidence locations, including old rationale/version references that a
  mechanical offset left on unrelated statements.

The aggregate local preflight stops at its ABI check: this prepared Debian/GCC 14
toolchain produces `CXXABI_1.3.15`, above the shipping floor `1.3.14`. The floor is
unchanged; shipping-toolchain ABI, sanitizer and cross-platform results belong to
the PR CI record. All remaining local source/citation/test checks were run separately.
Native DAW automation, OS focus delivery/vblank cadence, visual audition and
simultaneous processor destruction are not established by these component tests.
The existing surviving-processor lifetime contract is unchanged.
Final pluginval and CI status are reported in the PR to avoid duplicating mutable results.
Local VST3 pluginval subsequently completed **strictness 10, deterministic ×3 and
randomise ×3**, all on their first attempt. Its initial download returned HTTP 503;
the retained official 1.0.4 archive was verified against the locked archive hash,
byte count and executable hash using the production acquisition verifier before
seeding the content cache. Both unmodified gate commands then verified that cache.

The first probe setup could not advance frames because `isShowing()` requires a
native peer. That result is not used as frame-progression evidence. A five-line,
behavior-preserving extraction leaves the visibility guard in `tick` and gives the
existing friend adapter access to the identical `advanceFrame(dt)` body. With the
original parser/commit/paint logic still intact, the corrected baseline reproduced
**403 checks / 187 failures**. This surface also permits retiring TESTING's historical
held-solo guard exception with a deterministic timestamp/control test; no timer,
native-window dependency, easing curve or thread is added.

## Input matrix: original observed results

Measured through the production converters and two-band inline editor at the default editor size. Frequencies are Hz. Splits 1/2 used identical converters. The fixed probe applies the required oracle to all four paths.

| Input | Required | Original host 1/2 | Original host 3 | Original inline |
|---|---:|---:|---:|---:|
| `20` | 20 | 20 | 20000 | 15121.99 |
| `20Hz` | 20 | 20 | 20000 | 15121.99 |
| `20 Hz` | 20 | 20 | 20000 | 15121.99 |
| `19` | 19000 | 20 | 19000 | 15121.99 |
| `19k` | 19000 | 19000 | 19000 | 15121.99 |
| `19 k` | 19000 | 19000 | 19000 | 15121.99 |
| `19 kHz` | 19000 | 19000 | 19000 | 15121.99 |
| `19 K` | 19000 | 19000 | 19000 | 15121.99 |
| `1` | 1000 | 20 | 1000 | 999.9996 |
| `1k` | 1000 | 1000 | 1000 | 999.9996 |
| `1 khz` | 1000 | 1000 | 1000 | 999.9996 |
| `1.25` | 1250 | 20 | 1250 | 1250 |
| `1.25k` | 1250 | 1250 | 1250 | 1250 |
| `21` | 21 | 21 | 21 | 35.80882 |
| `21k` | 20000 | 20000 | 20000 | 15121.99 |
| `0` | 20 | 20 | 20 | 35.80882 |
| `3` | 3000 | 20 | 3000 | 2999.997 |
| `0.1` | 100 | 20 | 100 | 100 |
| `0.1k` | 100 | 100 | 100 | 100 |
| `0.578` | 578 | 20 | 577.9999 | 577.9997 |
| `1.578` | 1578 | 20 | 1578 | 1577.999 |
| `1.578k` | 1578 | 1578 | 1578 | 1577.999 |
| `-10` | 20 | 20 | 20 | 35.80882 |
| `-10k` | 20 | 20 | 20 | 35.80882 |
| `190k` | 20000 | 20000 | 20000 | 15121.99 |
| `190 kHz` | 20000 | 20000 | 20000 | 15121.99 |
| `1,000` | 1000 | 20 | 1000 | 999.9996 |
| `1,5kHz` | 15000 | 1000 | 1000 | 999.9996 |
| `1.5kHz` | 1500 | 1500 | 1500 | 1500 |
| `1,0,0,00` | 10000 | 20 | 1000 | 999.9996 |
| `10,000` | 10000 | 20 | 9999.999 | 9999.999 |
| `1,8k` | 18000 | 1000 | 1000 | 999.9996 |
| `1.8k` | 1800 | 1800 | 1800 | 1799.999 |
| `1.8 K` | 1800 | 1800 | 1800 | 1799.999 |
| `19Hz` | 20 | 20 | 19000 | 15121.99 |
| `20kHz` | 20000 | 20000 | 20000 | 15121.99 |
| `20.0Hz` | 20 | 20 | 20000 | 15121.99 |
| `20.5Hz` | 20.5 | 20.5 | 20.5 | 35.80882 |
| `20.5` | 20.5 | 20.5 | 20.5 | 35.80882 |
| `20/20Hz` | 2020 | 20 | 20000 | 15121.99 |
| `1 k Hz` | 1000 | 1000 | 1000 | 999.9996 |
| `1 K H Z` | 1000 | 1000 | 1000 | 999.9996 |
| ` 1 \t. 8\n k H z ` | 1800 | 1000 | 1000 | 999.9996 |
| `19.999` | 19999 | 20 | 19999 | 15121.99 |
| `20.0001` | 20.0001 | 20.0001 | 20.0001 | 35.80882 |
| `0Hz` | 20 | 20 | 20 | 35.80882 |
| `-0.1 K H Z` | 20 | 20 | 20 | 35.80882 |
| `+1.5k` | 1500 | 1500 | 1500 | 1500 |
| `(1'500)Hz!` | 1500 | 20 | 20 | 35.80882 |
| `.5k` | 500 | 500 | 500 | 499.9998 |
| `20.` | 20 | 20 | 20000 | 15121.99 |
| `20000` | 20000 | 20000 | 20000 | 15121.99 |
| `20001 Hz` | 20000 | 20000 | 20000 | 15121.99 |
| `0.000001` | 20 | 20 | 20 | 35.80882 |
| `1Hz` | 20 | 20 | 1000 | 999.9996 |
| `9999999999999999999999999999999999999999999999999999k` | 20000 | 20000 | 20000 | 15121.99 |
