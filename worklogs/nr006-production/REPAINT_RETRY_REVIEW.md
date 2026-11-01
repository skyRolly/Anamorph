# Vectorscope final-silence repaint review

**2026-11-01 (Asia/Shanghai), PR #202.** Accepted the reported scheduling bug.
The old scheduler cleared `frameDirty` when requesting repaint; activity polling
could already commit silence before paint rejected a reused slot. Subsequent
silent/frozen ticks could then leave the last active picture indefinitely.

The existing GUI-local flag now clears only when paint accepts a valid window.
There is no ScopeBuffer protocol, atomic ordering, allocation, audio-thread work,
thread or ownership change. JUCE message-manager serialization remains required.
This repairs an unreleased branch defect; no additional customer changelog entry
is warranted under CHANGELOG_POLICY.

## Deterministic negative control

State 150 exercises the same poll/repaint scheduler used by visible FrameClock
ticks through a private friend accessor. A `CachedComponentImage` observer counts
actual `repaint()` invalidations without a native peer. The test accepts active
samples, polls 8,000 zero frames, invalidates the last tag before paint, verifies
retention, restores validity and requires a new repaint on later silent audio.
Only a scheduled repaint is then painted; it must replace the waveform with
zeros. Continued silence must go idle. A separate rejected paint at a frozen
publication must also retry, then return to idle after acceptance.

Seven assertions supplement, rather than replace, the original 27 focused checks.
With the old clear-on-schedule semantics from `89d2d8f`/parent merge `e1e2e18`,
plus only the behavior-preserving private scheduler extraction needed by the
headless observer, **34 checks / 3 failures** reproduce missing silent retry,
missing silent replacement and missing frozen retry. Raw output:
[original.log](repaint-review/original.log). All producer/reader concurrency
checks pass in that control. With the corrected scheduler the same checks must
pass; no sleep, scheduler retry or relaxed expectation is used in this regression.

## Reproduction and verification

Use the repository's ordinary Release configuration and JUCE pin:

```sh
cmake --build build --target AnamorphStateTests AnamorphTests -j3
build/AnamorphStateTests_artefacts/Release/AnamorphStateTests --scope-snapshot-probe
MALLOC_PERTURB_=1 build/AnamorphStateTests_artefacts/Release/AnamorphStateTests
MALLOC_PERTURB_=1 build/AnamorphTests_artefacts/Release/AnamorphTests
```

To reproduce the negative control in a disposable copy, retain the scheduler
extraction and test, restore `frameDirty = false` immediately before `repaint()`
in `updateDisplay()`, and replace paint's assignment with the old ignored
`snapshot.accept(...)` result. Both changes are needed to restore the old behavior.

Corrected local execution: **34/34 focused, 10,987/10,987 full state and
945/945 DSP checks passed** (`MALLOC_PERTURB_=1` for full suites).
[fixed.log](repaint-review/fixed.log) and
[digests.json](repaint-review/digests.json) retain the focused output and source/raw-log
SHA-256 identities. The scope protocol self-test still detects the original three
failures and both validation/invalidation mutants. Structural docs (181 files),
citation anchors (606 retained / four previously re-spelled or removed relative
to #199), tables/links and whitespace checks pass.
Exact-head native/sanitizer/CodeQL/MSVC CI is tracked in the PR discussion; older
head successes do not qualify this correction. Headless coverage does not replace
targeted native visual/editor-lifecycle or representative cost acceptance.
