# NR-014 — Rotary accessibility titles and bounded component regression

## Decision and source — 2026-11-02 (Asia/Shanghai)

**Confirmed metadata defect; correction submitted in [PR #204](https://github.com/skyRolly/Anamorph/pull/204).**
Source/test/evidence commit: `d49736e9a496cfb83db67804a8439daa55200f26`;
subsequent documentation commits add provenance and exact-head review evidence.
Integrated main `425539385ebfcd1ca2bab1befe3de37618ce02d2` supplies all 12 rotary
captions through `PluginEditor::setupRotary` but assigns no accessibility title.
The omission is also present in tagged 0.9.9/0.10.0 source. Pinned JUCE9.0.4's
`SliderAccessibilityHandler` inherits the component title; `Component::getTitle()`
returns the default empty title rather than the adjacent label. This establishes
missing programmatic metadata, not a measured native screen-reader failure.

Reuse the **existing visible caption** via `s.setTitle(name)` in that factory.
The 12 call sites cover Drive, Amount, Width, Delay, Density, Rate, Depth, Mix,
Output, both Balance controls and Freq. The captions do not change in M/S mode;
`updateMsLabels` changes polarity-button text/tooltips and refreshes value text.
No new/reworded UI copy, host parameter display name, focus policy, DSP/parameter/
state contract, latency, threading or rendering architecture is introduced.
The separately configured scope-persistence bar and custom-drawn imager are
outside this bounded correction; this is not complete application accessibility.

**Alternatives:** preserve empty titles (leaves a demonstrated metadata gap);
add new descriptions/qualified names or change Tab focus (needs UX wording and
host-routing decisions plus native evidence); replace controls/painting (no
mechanism or need established). Reusing owned captions is the smallest supported
correction. Promote the bounded part of NR-014 from P3 to P2, independent of the
Deferred NR-003 matrix and unpinned NR-013 focus report. Keep the wider native
visual/keyboard/assistive-device audit pending.

## Regression and original-code control

`tests/gui_accessibility_tests.h`, State152, runs in the ordinary state suite and
`--gui-accessibility-probe`. It creates the real editor through the processor,
opens Simple→Advanced→Simple and identifies every rotary through **two distinct
host parameter writes and attachment readbacks**, rather than private test hooks
or matching defaults. It checks captions/titles, attached values, directly
received unmodified Right/Left, rejected modified arrows, bit-exact unrelated
normalized parameters, non-vacuous visible/enabled coverage, and actual control
destruction before reopen. No native window or host/global focus is simulated.

Only widget identification/readback allows eight float epsilons relative to the
raw value: JUCE Slider's double skew mapping and APVTS's float mapping differ by
a few ULPs for MonoMaker. An initial bit-exact lookup falsely missed three widgets;
that instrumentation was corrected before the retained control. The title and
unrelated-state expectations are unchanged; no DSP numerical tolerance is relaxed.
Key-return checks allow float conversion precision, not a broad audible threshold.

The nonshipping Linux/Ninja reproduction utility compiles the pinned original
editor in temporary storage, then substitutes **only that object** into the real
state target's actual link command. Both binaries execute the same current tests,
flags, processor and JUCE objects; no checkout, CMake target or assertion is edited.

```sh
# Activate the repository's pinned toolchain and configure/build the ordinary tests first.
python3 tests/experiments/nr014_rotary_metadata_control.py \
  --build-dir build --output-dir /tmp/anamorph-nr014-controls
```

Retained [original log](nr014-rotary-accessibility/original.log),
[corrected log](nr014-rotary-accessibility/corrected.log) and
[source/exit metadata](nr014-rotary-accessibility/metadata.json):

| Object under test | Checks | Failures | Disposition |
|---|---|---|---|
| Original main editor `4255393` | 924 | 36 | Only missing titles:12 controls ×3 editor lifetimes |
| Corrected editor, otherwise identical regression | 924 | 0 | Metadata, attachments, delivered keys and lifetime controls pass |

Existing keyboard policy is preserved. JUCE Slider disables automatic keyboard
focus in its constructor; calling its `keyPressed` here proves component handling
**after delivery**, not Tab traversal, host shortcut dispatch or a screen reader's
native value interface. Enabling focus without host evidence would be a separate
behavioral change. Duplicate existing Balance captions are preserved, not a claim
of globally unique accessible names.

## Available validation and limits

Local shipping-Clang Release build, pinned JUCE9.0.4, Debian13 x86-64 shared
container; 48kHz/128-frame setup. Full suites under `MALLOC_PERTURB_=1`:
**945 DSP / 11,911 state**, zero failures; focused924/0. The new 924 checks account
for the increase from integrated main's10,987. Source/log hashes, platform and
image digests are in [validation.json](nr014-rotary-accessibility/validation.json).
The [incremental build log](nr014-rotary-accessibility/incremental-build.log.gz)
retains the original bytes of existing editor-shadow/state-helper prototype warnings
(compressed because compiler suggestion lines include trailing spaces); no baseline or
suppression is changed. Current-head native CI is recorded in the PR discussion;
local results never qualify a different head implicitly.

A nonshipping [software-pixel probe](nr014-rotary-accessibility/software-pixel-probe.cpp)
compares the same component with titles present, cleared and restored, at identical
geometry and without pumping animation between captures. Three real editor
lifetimes produce nonempty 940×720 /940×900 /940×720 images, with **zero changed
pixels** for either comparison. [Raw observations](nr014-rotary-accessibility/software-pixels.log).
This validates the changed metadata's software-drawing effect; it is not a native
GPU, DAW, high-DPI, screen-reader or visual-lifecycle audition. Images remain local
scratch outputs rather than new golden rendering expectations.

A separate attempt to create a native editor peer under isolated Xvfb failed with
X11 `BadAtom`/`X_ChangeProperty` before interactions, under both temporary and
ordinary window flags. Cause is unestablished: this is an unavailable probe surface,
not proof of a product regression, native acceptance or an excuse to suppress X11
errors. No native assistive-device or host keyboard outcome is marked passed.

## Local pluginval outcome and unresolved crash attribution

The freshly built shipping-Clang VST3 completed strictness10 deterministic ×3 and
randomise ×3 using the unchanged repository runner. Deterministic pass3's first
attempt exited139 during “Automatable Parameters”, after Editor Automation; its
second attempt passed. The other five passes succeeded on first attempts. Retained
[deterministic log](nr014-rotary-accessibility/pluginval-deterministic.log.gz) and
[randomised log](nr014-rotary-accessibility/pluginval-randomise.log.gz) preserve
all output, including the crash and retry; raw digests are in validation.json.

The runner classifies the signal under its existing Linux-only crash policy,
historically established for pluginval/JUCE XEmbed. This occurrence has no stack,
so its root cause is **unattributed**, not proved to be that historical flake or
an introduced product defect. The changed JUCE `Component::setTitle` path is only
a message-thread String assignment, without an accessibility callback, DSP work
or new lifetime owner. Preserve the diagnostic and require current-head native
CI; a signal with a product stack or recurrence outside this class warrants a
separate investigation. No retry count, timeout, detector or test is changed.

Follow-up at source head`f42412b`: Linux CI also completed both modes×3 but its
randomise pass2 exited139 **after SUCCESS**, recovering on attempt2. Its normal
suites passed945/11911 and executed State152. The baseline main Linux run completed
six first attempts without a signal; that limited absence does not identify the
new instance's cause. A scratch-extracted GDB16.3, locked validator and unchanged
fresh VST3 then ran three strictness10/seed1 diagnostics under Xvfb, following all
inferiors and trapping fatal signals: all normal exits, no fault caught. Debugger
pacing can hide an intermittent issue, so this does not exonerate it or establish
XEmbed/product attribution. No system or product dependency was installed/changed.
[Hashes, exact outcomes and raw logs](nr014-rotary-accessibility/validator-diagnostic.json)
and [capture commands](nr014-rotary-accessibility/validator-capture.gdb) preserve
that bounded diagnostic. Current-head native gates and owner acceptance remain
separate. A reproducible product stack requires a new targeted investigation.


## Integration, review and continuation

The approved five PRs are integrated on actual main`4255393`; this rotary correction
is separate branch `fix/nr014-rotary-accessibility-titles` and remains **unmerged**.
Its [review PR #204](https://github.com/skyRolly/Anamorph/pull/204), source commit
and final-head gate ledger are recorded in the execution register and PR discussion. New merge authorization and native UX/assistive-device acceptance
remain outstanding. No 0.10.0 tag/draft/asset/version/release operation is performed.

Keep NR-006/010 representative costs blocked on identified controlled hardware,
NR-013 on a pinned REAPER/OS reproduction, and NR-015 on useful scope/authorized
native systems. Do not replace those inputs with container timing or speculative
focus changes. [Roadmap](NEXT_RELEASE_DEVELOPMENT_ROADMAP.md) and
[release disposition](RELEASE_READINESS_0.10.0.md) track independent next work.
