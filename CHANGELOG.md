# Changelog

All notable user-visible changes to Anamorph. Format follows [Keep a Changelog]; versions are
`MAJOR.MINOR.PATCH` (pre-1.0). The repository has **no git tags**, so each entry cites its **commit
SHA + date** as the Evidence Source (per `docs/policies/CHANGELOG_POLICY.md`). Entries for the
0.6.x line and earlier are reconstructed from commit history (the detailed per-version notes predate this changelog) and are marked accordingly.
Display-name renames are recorded as **Changed**, never as parameter removals (the IDs are immutable).

## [0.8.11] — 2026-07-18
### Changed
- **Multiband and crossover-drag CPU cost reduced (performance Wave 3; no behaviour change
  by design).** Four independent optimisations, all validated by a 12-scenario full-engine
  output twin-dump plus the DSP self-test suite: **(1)** the Band Solo monitor's settled
  fast path is now gated on its crossfade gains alone, so dragging a split with **nothing
  soloed** no longer wakes the whole solo filter bank to compute a provable passthrough
  (bit-identical output; the bank stays cold and re-engaging still snaps to the freshest
  cutoffs under the same ~12 ms crossfade — regression Test 33); **(2)** the four
  crossover filters of one multiband split (wet, dry twin, and the two phase-compensation
  allpasses) now share one coefficient computation per update instead of four identical
  `tan` evaluations — bit-identical, cutting the worst-case per-sample coefficient math of
  a dry-aligned split drag to a quarter; **(3)** the flat-recombination phase-compensation
  allpass is computed directly as the Linkwitz-Riley ladder's first 2nd-order section (the
  `lo+hi` sum it always equalled, the optimisation recorded in `PERFORMANCE_BUDGET.md`
  since 0.8.10) — half the allpass arithmetic; output equal to within one float rounding
  pair (measured max 1.2e-7, a few samples per 200-block dump; 44.1–192 kHz unaffected
  otherwise); **(4)** the settled output-gain stage and settled-Mix dry/wet blend hoist
  their per-sample smoother ticks and constants per block (bit-identical). Session-local
  measurements (48 kHz, Release, Linux x86_64): continuous crossover drags −35…−50 %
  engine cost, settled 3/4-band multiband states −9…−17 %, transparent floor −6.6 %.
  Also: the spectrum analyser's FFT now computes only the non-negative-frequency
  magnitudes it reads (identical visuals, ~half the per-transform magnitude work).
  Full investigation record: `worklogs/performance/WAVE3_INVESTIGATION.md`.
  Evidence: PR #62 (merge `b2481db`). [Verified]

### Fixed
- **At very high sample rates (192 kHz) a moved crossover now always lands exactly on its
  target; previously it could rest up to 3.75 Hz short forever and keep the solo monitor's
  settled fast path from ever engaging (ADR-0015 "High-Sample-Rate Terminal-Snap
  Robustness").** The cutoff glide's per-sample one-pole add stops changing a float once the
  move drops below `ulp(f)/2`, and the terminal-snap eps (0.05 + 2e-4·f Hz) covers that stall
  only up to 96 kHz (margin ≥ 1.76×; 3.55–4.27× at 44.1/48 kHz): at 192 kHz the margin falls to
  0.88–0.98× just past every binade edge ≥ 2048 Hz (parameter-range hard-stall zones
  [2049–2093], [4097–4437], [8194–9125], [16388–18500] Hz, both approach directions —
  exact-float simulation; higher binades up to the DSP-level 86.4 kHz Nyquist clamp stall too,
  same ≤ 0.4-cent resting error). Audio was
  never wrong (< 0.4 cents off), but cutoffs could never equal targets, so the H1 settled fast
  path stayed unreachable and the crossover filters/smoothers stayed hot indefinitely. The glide
  now also snaps to the exact target the moment the float add can no longer move the cutoff —
  eps, rate law R(f), smoothing, and fade thresholds untouched; behavior at ≤ 96 kHz
  bit-identical (the eps snap always fires first). Guarded by `testHighRateCrossoverSnap`
  (Test 32; DSP tests 30→31, checks 115→130): bitwise-exact landing plus cold-path engagement at
  44.1/48/96/192 kHz — pre-fix it fails at 192 kHz exactly (measured resting gaps
  0.4688/0.9375/1.8750/3.75 Hz, never cold) and passes at the normal rates, doubling as the
  unchanged-behavior guard. Evidence: PR #61 (commit `c72d3c3`, merge `bc5f852`). [Verified]
- **Crossover follower slow-drag regression: normal-speed split drags no longer trail the mouse
  by whole octaves and crawl on for seconds after release (ADR-0015 "Crossover Follower
  Slow-Drag Regression").** The v0.8.10 final follower capped cutoff movement at a flat
  ~4 oct/s, calibrated at a 150 Hz crossing — but the Multiband display maps ~10 octaves onto
  ~900 px, so ordinary 400–2000 px/s gestures are 4–22 oct/s: every normal drag was pinned at
  the cap (a 600 px/s drag released ~2.4 octaves behind and crawled for another 0.6 s, trailing
  audibly throughout), while a violent flick could escape through the discrete-jump bank fade
  and feel instant — the reported "slow drags are limited harder than fast ones" inversion. The
  root cause is physical, not a state bug: a swept LR4's frequency shift is a constant
  `0.312·R` Hz wherever the crossing sits, so a cap flat in oct/s spends its whole artifact
  budget protecting bass crossings and buys nothing but lag at high ones. The glide is now a
  **slew-limited smoother**: per sample each cutoff moves by its ~20 ms one-pole demand toward
  the target, clamped to a **frequency-proportional cap `R(f) = 4·max(1, f/300 Hz)` oct/s** —
  the shift stays ≤ 1.25 Hz below 300 Hz (a 150 Hz crossing still measures ~14 cents, unchanged)
  and ~0.42 % of the crossing (~7 cents) above, the one-pole leg filters the 60 Hz UI staircase
  and tapers arrivals (a bare rate-clamp landing measured −24 dBc of splatter; 300 Hz is the
  measured spur knee — an fref = 150 variant sprayed −27 dBc past a 1 kHz tone, the shipped
  anchor sits at the −41 dBc analysis floor). Normal drags now track 1:1 (the 600 px/s complaint
  gesture converges 0.01 s after release, was 0.63 s) and even a full-panel flick lands in
  ~0.5 s of continuous motion; every prior Test 29 artifact bound holds at the same measured
  values (~14 cents, −41.3 dBc, discrete jumps < 200 ms, click-free). Test 29 gained a
  normal-drag tracking regression on both the Multiband and Solo-monitor paths (band edge at the
  target 0.1–0.35 s after release; the flat-cap follower fails both checks — verified by
  temporarily re-pinning). `MultibandWidth`/`SoloMonitor` only; no signal-order, latency, or
  parameter change. Evidence: PR #60 (commit `3268cc2`, merge `0c50c47`). [Verified]

## [0.8.10] — 2026-07-14
### Changed
- **Alt/Option-click on an unsoloed Band Solo button now solos ONLY that band (exclusive
  solo)** — every other band's solo turns off — instead of soloing all bands at once (the 0.8.9
  behaviour). Alt/Option-clicking an already-soloed band still clears the whole solo mask, and a
  plain click still latches just that band; the press-and-hold momentary audition / hold-drag
  band move are unchanged. Still one write of the `mbSolo` mask under one change gesture, so
  automation, undo/redo and preset recall behave as before. Evidence: this PR. [Verified]
- **The Vectorscope, Level Meter, Stereo Meter and Spectrum Imager now refresh at the display's
  rate (adaptive, capped near 120 Hz) instead of a fixed 60 Hz.** On a 120 Hz (or higher) panel
  the visualizers animate visibly smoother; on a 60 Hz panel they behave exactly as before. A new
  `gui::FrameClock` rides each component's display vertical blank (`juce::VBlankAttachment`) and
  executes every `ceil(refresh/126)`-th frame, so the rate tracks the monitor but is bounded to
  keep paint CPU in check (60→60, 120→120, 144→72, 240→120 Hz); when the refresh rate cannot be
  measured it falls back to 60 Hz by wall clock. Every rate-dependent animation (spectrum
  release, clip-glow rise/fall, the correlation/balance pointer glide, the analyser's hover/press/
  solo eases and split/width glides) was rewritten in elapsed-time (`dt`) form so its speed is
  identical on any display and matches the old 60 Hz curves to within the on-screen colour
  quantum. All Wave-1/Wave-2 GUI optimisations are preserved: the S1/S2/S3 repaint gates, the
  H2/H13/H17 cached static layers, the N2 opaque blits and the H15 idle pre-gate are unchanged, so
  idle CPU stays ~0 and a settled view still stops repainting. The Advanced-only Spectrum Imager
  stops its clock entirely while hidden (Simple mode), and the rate cap re-applies within ~2
  frames when the editor is dragged onto a faster monitor while a single early vblank (scheduler
  jitter) near the 120 Hz cap no longer perturbs the rate. Internal/threading model unchanged
  (still message-thread). Evidence: this PR. [Verified]

### Fixed
- **A forced bulk swap (undo/redo/A-B/preset) landing while an ordinary discrete duck was still
  fading out no longer loses its forced semantics.** The forced request is consumed on entry to
  the engine's parameter-swap state machine; in the narrow (~6 ms) fade-out window of a
  non-forced discrete duck it used to be silently dropped — the swap then finished as a normal
  duck: no wholesale swap at the silent bottom, no smoother snap, and no clean-slate reset, so
  stale delay-line/oversampler audio could replay as the fade lifted (a 0.494-peak Haas-tail
  replay measured against silent input) and a big undo level jump could swell instead of
  snapping while silent. The in-flight duck is now upgraded in place to a forced one (same fade,
  forced bottom); it deliberately keeps duck-to-silence — the dry fill is never engaged mid-fade
  (engaging it would step the fill in at the current dry weight), matching the existing
  no-mid-fade-re-enable latch rule. Fresh forced swaps (Tests 26/27/30) are unchanged. Guarded
  by `testForcedSwapDuringOrdinaryFadeOut` (Test 31; DSP tests 29→30, checks 106→112).
  Evidence: this PR. [Verified]
- **Multiband split movement reworked: no spurious frequencies around a pure tone, no clicks,
  and fast-drag pitch modulation cut to a small controlled bound — while the audible crossover
  stays attached to the mouse.** Four design rounds, each graded against a pure-sine protocol
  (instantaneous frequency of the fundamental, spurs outside ±30 Hz, envelope, at drag speeds
  1–24 oct/s). The physics: a swept IIR crossover is inherently a phase modulator — its allpass
  phase at any fixed frequency rotates up to 2π per crossover crossing, a genuine frequency
  shift of `0.312·R` Hz at sweep rate `R` oct/s, and no smoothing shape removes it, only
  redistributes it. Rejected: the pre-0.8.10 uncapped ~8 oct/s glide (≈2.5 Hz shift — +31 cents
  measured at a 150 Hz crossing); chained ~12 ms fixed-bank crossfades (amplitude/phase
  modulation at the fade cadence — sidebands at −25…−28 dBc around the tone, and a crossfade
  between two phase-different allpasses cannot preserve the magnitude response mid-fade); a
  one-pole tracker τ≈15 ms (FM at the full drag rate — ~50 cents measured at the crossing of a
  fast drag); and a **~1.25 oct/s "inaudibility" cap with 0.25 s release consolidation**
  (measurably clean, but rejected in interactive testing as a UX regression: the audio lagged
  the GUI on ordinary fast drags and jumped after release — interaction latency is the worse
  artifact). Shipped design (ADR-0015 "v0.8.10 final decision") in `MultibandWidth` and
  `SoloMonitor`: **continuous movement tracks each cutoff per sample under a hard ~4 oct/s rate
  cap** — every drag up to 4 oct/s tracks *exactly* (zero GUI/DSP gap), faster movement bounds
  the shift at ~1.25 Hz (measured: worst 100 ms chunk ~15 cents at a 150 Hz crossing, ~2 cents
  at 1 kHz, spurs at the −41 dBc analysis floor, < 0.1 dB envelope ripple — roughly half the
  pre-fix worst case), and even a violent 6-octave flick catches up in ~1.25 s of *continuous*
  motion after release — no timers, no intent prediction, no delayed jump. KI-012 documents the
  accepted trade (a small amount of controlled FM is preferable to obvious interaction latency;
  artifact-free *fast* tracking is impossible with zero-latency IIR crossovers and would
  require linear-phase splits — a reported-latency change gated behind an Architecture Review,
  recorded as the roadmap direction in ADR-0015). **Discrete jumps** (the target stepping
  > 1.5 oct between consecutive blocks — automation steps/snaps, unreachable by dragging) stay
  responsive via a single ~12 ms crossfade to a state-copied second filter bank: one bounded
  transition event (−18 dBc at a 4-octave step) instead of a multi-second ease. Settled
  behaviour is bit-identical; flat recombination, mono compatibility, dry/wet phase alignment,
  Nyquist clamps, latency and serialization unchanged. Regression
  `testMultibandSplitDragNoPitchShift` (Test 29) grades the entire movement at the final
  operating point: worst 100 ms chunk < 18 cents across the drag AND the full catch-up
  including the tone crossing (the shipped cap measures ~14; the uncapped glide ~28 and the
  one-pole ~50 fail), max spur < −31 dBc during a 60 Hz-cadence drag (the chained fades measure
  −28.5 dBc and fail), a released 6-octave flick must land by plain gliding within ~1.5 s (the
  1.25 oct/s follower measures full lag there and fails), discrete 4-octave jumps must land
  < 200 ms, all click-free. Evidence: this PR. [Verified]
- **The intermediate "bounded convergence" follower was evaluated and simplified away
  (ADR-0015 "v0.8.10 final decision").** The 1.25 oct/s cap + release-consolidation follower
  solved the earlier unbounded-catch-up and "stuck follower" defects, but interactive testing
  rejected its interaction latency: a 500 Hz → 2 kHz / 0.5 s drag released with 1.37 oct of
  audible lag and glided on for another second, and the 0.25 s quiet-timeout consolidation — a
  "wait until the user stopped" heuristic — read as a sudden delayed jump after the hand had
  stopped. Final refinement, per the restated product intent (slightly reduce artifact
  severity while preserving direct manipulation): the cap rises to **~4 oct/s** (Cases A and B
  — 500 Hz → 2 kHz in 5 s and in 0.5 s — both track with 0.00 oct lag and 0.00 s settle;
  Case C, a 6-oct/0.25 s flick, settles in ~1.25 s vs 2.75 s), and the **release consolidation
  is removed entirely** (quiet detector, 0.25 s timeout, residue fade — the mechanism, its
  state and its members are gone from `MultibandWidth` and `SoloMonitor`). The discrete-jump
  bank fade remains the only special event (Case D: automation snaps, unreachable by dragging).
  Follower trajectories stay deterministic and closed-form; manual drags and automation share
  the identical path; exactly one smoothing stage exists. The full A–H3 architecture
  investigation history, the earlier follower iterations and their measurements remain a
  permanent record in ADR-0015. Regressions: Test 29 re-thresholded to the final operating
  point (18-cent controlled bound — the uncapped glide fails at ~28; convergence window moved
  to 1.7–2.2 s — the 1.25 oct/s follower fails at 1.00× full level; both directions verified by
  temporarily re-pinning the cap). Evidence: this PR. [Verified]
- **macOS (Apple Silicon native): tooltips no longer show an opaque white rectangle around the
  rounded capsule.** Root cause: `juce::TooltipWindow` declares itself *opaque* (its constructor
  calls `setOpaque(true)`) while the custom tooltip drawing deliberately leaves the pixels
  outside the rounded capsule unpainted — undefined pixels in a window that promised to fill its
  bounds. What renders there depends on the compositing pipeline: Intel and Rosetta happened to
  show the stale (transparent) layer backing, but Apple-Silicon-native AppKit initialises the
  opaque layer-backed window with its background colour first, producing the white corner frame
  (the same undefined-pixels class as KI-006's black corners on uncomposited Linux/X11). The
  editor now declares its `TooltipWindow` **non-opaque on macOS**, so the JUCE peer creates a
  transparent `NSWindow` (clear background) and clears the backing to real alpha on every paint —
  transparent rounded corners by contract on every pipeline. macOS-gated: Windows and Linux keep
  their existing behaviour (uncomposited Linux keeps the KI-006 corner pre-fill). Code-path fix
  verified by inspection of the JUCE 8.0.14 peer (`drawRectWithContext` clears non-opaque
  windows); on-hardware confirmation on Apple Silicon is pending (KI-011). Evidence: this PR.
  [Verified — code path; hardware re-test pending]
- **Undo/Redo with an extreme Output Gain no longer produces a loud transient (forced-duck dry
  fill now follows the output stage).** The forced-swap dry fill (introduced below) crossfades
  the ducked output toward the delay-aligned raw input ring — which carries the *unity-level*
  input, while the processed path around it is scaled by Output Gain (or the Level-Match gain)
  × Output Balance. At extreme settings (e.g. Output Gain −24 dB) an undo/redo Mix toggle
  burst the fill in up to 24 dB louder than the surrounding audio. The fill is now presented at
  the **output-stage gain heard when the duck began**, latched at fade-out entry exactly like
  the fill's delay offset (`dryDuckLat`) — latched, not live, because the gain smoothers snap
  to the new state at the silent bottom where the fill carries full weight, and a live gain
  would step audibly there. At unity gain/balance the arithmetic is bit-identical to the
  previous fill (Tests 26/27 unchanged); true bypass still presents the raw ring at unity by
  design. Regression `testDryFillRespectsOutputGain` (Test 30): at −24 dB the transition peak
  must stay within 2× the steady output — the unscaled fill measures 15.8× and fails — while
  still filling (no dip toward silence). Evidence: this PR. [Verified]
- **Option/Alt-click (and double-click) reset of a knob/slider now creates a normal Undo step.**
  Root cause: the reset wrote the slider value programmatically, which reaches the parameter
  *without* a host change gesture; the processor's undo coalescer deliberately treats
  gesture-less changes as host automation and folds them into the committed baseline — no undo
  entry, and the redo stack survived when it should have been invalidated (so Undo skipped the
  reset and reverted the previous edit, and Redo stayed available after a reset). The `Knob`
  reset is now wrapped in `beginChangeGesture`/`endChangeGesture` around the value write —
  exactly how the Multiband display's split/width resets already did it (those, and every other
  Imager edit, were verified to share the same gesture-based undo path and needed no change) —
  so a reset lands as one undoable step, clears redo, and records one automation move in the
  host. `undo()`/`redo()` additionally flush a settled-but-unpolled gesture first (the editor
  polls the coalescer at 24 Hz), so an edit finished immediately before the click can no longer
  be silently skipped over. Automation, presets and serialization unchanged; the host-hidden
  Settings knob (Vectorscope Persist) intentionally stays outside undo as before. Evidence:
  this PR. [Verified]
- **Multiband: closely-spaced crossovers no longer cut the level around the crossover
  frequencies.** With three splits concentrated together the band around the crossovers behaved
  like an EQ dip (measured −17.75 dB at 800/1000/1250 Hz), even at unit width and without moving a
  band. Root cause: the reconstruction summed the serially-split bands directly, which is only
  flat for a single crossover — an LR4 low+high is an allpass, so with more crossovers the lower
  bands were missing the allpass phase of the splits above them and partially cancelled around the
  (shared, when close) crossover region. The reconstruction now phase-compensates each lower band
  by running the running low-sum through each higher split's allpass before adding the next band,
  so it telescopes to a true allpass (flat). Recombination is now flat to ±0.0 dB at every split
  spacing (regression test `testMultibandFlatRecombination`); mono compatibility, solo, automation,
  presets/serialization and the reported latency are unchanged (the compensation is an equal-on-
  L/R, zero-integer-latency IIR allpass). Only `bands−2` extra allpass sections run (none for 1–2
  bands). Evidence: this PR. [Verified]
- **Rapid consecutive Undo/Redo (or discrete changes) during the crossfade no longer reuse stale
  dry-fill state.** A second forced swap arriving while a previous forced duck was still fading in
  kept the first swap's dry-fill decision and delay offset; if the two swaps differed in reported
  latency the second could read the raw-input ring at the wrong offset or stay silent when it
  should have dry-filled. Every forced swap now re-evaluates dry-fill against the state heard at
  that moment, latching the read offset for the duck's lifetime and only tightening (never
  re-enabling) it mid-fade. Follow-up to the undo/redo dropout fix below; ordinary single swaps are
  byte-identical (twin-dump verified). Regression test `testRapidForcedSwapDryFill`. Evidence: this
  PR. [Verified]
- **Undo / Redo (and A/B switch / preset load) no longer produce a brief audible dropout.**
  Root cause: those actions route through the engine's *forced* switch duck, whose raised-cosine
  output gain reaches exactly 0 and dwells there until the next block boundary (~6 ms fade-out
  + up to one host block of hard zeros + the slow ~28 ms fade-in) — 15–25 ms of effective
  silence by design. The forced duck is now **dry-filled**: while it is in flight the output is
  crossfaded toward the delay-aligned raw input already maintained for the true-bypass
  crossfade (the ring's writes were always warm; only its dead read-back was gated — H9), so
  the swap is heard as a short dip to the dry signal instead of a gap. The processed weight
  still reaches 0 at the bottom, so every masking property of the silent-bottom swap (smoother
  snap, wholesale node reset, oversampler-latency latch) is unchanged, as is the reported
  latency. A forced swap that crosses a latency boundary deliberately keeps the original
  duck-to-silence (its ring read offset would jump at full dry weight). Ordinary discrete
  switches (algorithm dropdown, routing toggles) are untouched. Validated: the 33-scenario
  full-engine twin dump is **byte-identical** pre/post on every existing path (md5
  `c35ed5e3…`, latencies identical), and the new regression test `testForcedSwapNoDropout`
  (Test 26) holds the minimum 2 ms-window RMS across a forced swap at ≥ 0.93× (continuous
  bulk swap) / ≥ 0.65× (algorithm swap) of steady level — the pre-fix engine measures 0.000
  on both and fails. Evidence: this PR. [Verified]

### Known issues
- **KI-009 (documented, not fixed):** in **REAPER on Linux/macOS**, the Save Preset text field
  loses keyboard focus — pressing Space while it is active can trigger the DAW transport, and after
  the field loses focus a click cannot restore editing until the Save Preset window is closed and
  reopened. Other tested DAWs do not reproduce it; the root cause is not yet confirmed. Recorded as
  a **host-specific issue pending manual investigation** (`docs/KNOWN_ISSUES.md` KI-009). No fix in
  this release.

## [0.8.9] — 2026-07-12
### Added
- **Alt/Option-click on a Band Solo button acts on every band at once**: alt-clicking a soloed
  band's headphone icon clears the whole solo mask; alt-clicking an unsoloed band's icon solos
  all active bands. A plain click still latches just that band, and the press-and-hold momentary
  audition / hold-drag band move are unchanged. Implemented as one write of the existing `mbSolo`
  mask parameter under the usual change gesture, so host automation records one move, undo/redo
  treats it as one step, and preset recall (which clears the live solo) is unaffected. Validated
  headless across 1/2/3/4-band layouts × soloed/unsoloed/mixed masks, host-automation interplay,
  undo/redo and preset load (18/18 assertions). Evidence: PR #56. [Verified]
### Fixed
- **A destroyed plugin instance no longer leaves a dangling parameter listener registered.**
  The Wave-2 micro-animation re-arm listener (`viewGenWatcher`, added for the Bypass view
  parameter) was registered in the constructor but — unlike every other parameter listener in
  the processor — was never unregistered in the destructor; the view parameter (owned by the
  `AudioProcessor` base subobject, torn down after derived members) could then outlive the
  watcher holding a dangling listener pointer. Registration and unregistration are now fully
  symmetric across all three listener mechanisms. Internal-only: no DSP, latency, parameter,
  automation, preset or serialization effect under normal operation. Validated with
  `valgrind --tool=memcheck` across the self-test suite's ~20 processor construct/destruct
  cycles (0 errors from 0 contexts). Evidence: PR #58 (commit f6a5d49). [Verified]
- **The Band Solo tooltip reads `Solo this band` again.** The `- Alt-click solos / clears all
  bands` suffix shipped alongside the alt-click feature was never requested wording and has been
  removed; the alt-click behaviour itself is unchanged. UI copy is now covered by an explicit
  rule in `AI_AGENT_POLICY.md` (user-visible text requires explicit instruction).
  Evidence: PR #58. [Verified]
- **Toggling Advanced mode no longer flashes a torn frame** (most controls appearing to jump or
  shake for one frame). Both toggle paths resized the window before updating the mode's control
  visibility; `setSize` notifies the host synchronously mid-handler, so a host that paints inside
  that callback rendered the new layout with the old mode's visible-control set (entering
  Advanced: grown window with empty Multiband/Input/Output tiers; leaving: Advanced controls
  stacked over the Simple layout). The calls now run visibility-first (the order the constructor
  always used); the tree is mode-consistent at every instant a host can observe it, with no added
  layout work and no change to the resize/DPI/reopen paths. Reproduced and verified fixed under a
  host-wrapper shim that paints at the `childBoundsChanged` instant (all three toggle paths);
  post-toggle layout proven motionless across 30/30 sampled frames. Evidence: PR #56. [Verified]
- **The Save Preset name field reliably receives typing — Space included — instead of the host**
  (Space previously triggered host transport). The focus grab ran synchronously from inside the
  preset-menu callback, while the menu's desktop window still owned the OS focus; JUCE abandons a
  focus move when the peer is unfocused, so the grab was a silent no-op on hosts whose window
  keeps focus, and every keystroke fell through to the host. The grab is now verified and
  re-tried across the next message-loop passes (SafePointer-guarded, stops when the overlay
  closes), by which time the menu window is gone and the peer can genuinely take OS focus. While
  the field edits, it consumes its keys (Space inserts a space); with the overlay closed,
  key routing to the host is exactly as before. Validated headless end-to-end through the real
  preset menu with keys dispatched through the peer. Evidence: PR #56. [Verified]
### Changed
- **The editor's micro-animation poll re-arms on change-generation counters (Wave 2 / H15)**:
  with the cursor outside the editor, no button held and the previous pass settled, the 60 Hz
  poll no longer hashes every animated widget's value each frame (68-87 % of the remaining idle
  editor instructions in the Round-2 attribution) — it now compares three relaxed generation
  counters that together cover every path able to move a widget while the mouse is away: the
  existing sound-param generation (host automation, undo/redo, preset and A/B applies, session
  restore), a new view-param generation (host-automated Bypass, via a dedicated listener that
  stays out of the undo/gesture machinery), and a new InternalState generation (the two-way-bound
  Settings values, including their session restore). Same repaints, same animation behaviour —
  only provably-static polling is skipped. Verified live in a headless host: 13/13 eased slider
  positions correct after mouse-outside host automation in every phase, and a host-automated
  Bypass still animates its toggle (the new watcher path). Expected effect (existing Round-2
  measurements): idle editor CPU −~40 %. Evidence: PR #58. [Verified]
- **The Drive waveshaper computes its tanh with a minimax rational kernel (Wave 2 / H3)**: the
  two per-sample libm `tanh` calls become an odd degree-9/8 rational (input clamped at ±9.2,
  result clamped to ±1), call-free and branch-predictable — measured 15.2 → 3.9 ns/sample (3.9×)
  at the kernel level; the same kernel computes the peak-preserving makeup, so full-scale
  mapping stays exact by construction. Class B numerics: max relative error 3.5e-7 (~3 ulp)
  against double `std::tanh` on a 4M-point sweep; exact 0 at 0; hard ±1 saturation. On the
  33-scenario dump, drive-engaged rows differ by ≤4.8e-7 per sample, every non-drive scenario is
  byte-identical, and the Mix=0 null stays sample-exact once the Mix glide lands (DSP_POLICY
  invariant 7 re-verified); Match-toggle stress rows show bounded −63 dBFS-level transients where
  the loudness gate's thresholds amplify ulp-level input differences (readout deltas ~1e-6 dB).
  Expected effect (existing Round-2 measurements): drive rows −25-30 %; everything-on-os4 loses
  most of its ~55 % tanh share. Evidence: PR #58. [Verified]
- **The multiband dry-align reconstruction pauses while nothing can consume it (Wave 2 / H4)**:
  with the Mix glide parked at exactly 1, Level Match off (and not mid-engage), and no
  enable/bypass crossfade in flight, the phase-matched A(dry) bank (six crossover filters per
  sample — half the multiband cost) and the Mix blend pass are skipped; both dry delay lines keep
  running, so lowering Mix re-engages the reconstruction phase-matched (new regression test
  `testDryAlignGateRecomb` asserts the KI-#1 mono-sum metric through the gate/re-engage cycle).
  Class B by design: in the gated state the output is the exact processed signal instead of its
  Mix=1 float re-blend (measured ≤2.4e-10 difference), and the Measure readout follows the
  delay-aligned clean input while gated — so engaging Match immediately after a long gated
  stretch starts from a measurement without the multiband reconstruction ripple (worst measured
  0.53 dB initial level offset on a near-crossover synthetic, converging as the loudness window
  refills; the engage is always duck- and glide-smoothed, never a click). Expected effect
  (existing Round-2 measurements): multiband rows −~20 %. Evidence: PR #58. [Verified]
- **The multiband/solo/mono-maker crossovers run on a local flat-state LR4 (Wave 2 / H6)**: all
  ten `juce::dsp::LinkwitzRileyFilter<float>` instances are replaced by `LR4Xover`, which
  reproduces the JUCE filter's coefficient derivation and TPT ladder expression-for-expression
  (including which products round in float and which sums run in double) while storing its state
  in flat per-channel floats instead of heap `std::vector`s — the vector indexing was 4.5-7 % of
  every multiband/solo row. **Bit-identical**: proven byte-exact on the 33-scenario full-engine
  dump, including new 4-band solo engage/change/clear cycles (cold re-entry) and per-sample
  crossover/mono-freq glide scenarios; reported latency unchanged. No dependency change (JUCE
  itself is untouched). Evidence: PR #58. [Verified]
- **VelvetNoise sparse-FIR gather is restructured tap-outer (Wave 2 / H5)**: while the density
  glide is settled and no transport-stop fade is in flight, the 64 random-index history reads per
  sample become one contiguous streaming run per tap over a linear image of the history, with the
  per-sample accumulation kept in the original ascending-tap order — **bit-identical output**,
  proven byte-identical across a 31-scenario full-engine dump including new density-glide,
  transport-stop-flush and engage/park-cycle scenarios; the glide, stop-fade and parked paths keep
  the original per-sample loop verbatim. Expected effect (existing Round-2 measurements): −25-30 %
  on the velvet-1.0 row (the gather owned 41.7 % of it and 45.6 % of its D1 read misses).
  Evidence: PR #58. [Verified]
- **VelvetNoise folds the fixed ±1 tap sign into the stored tap weight (Wave 2 / ALG-4)**: the
  sparse-FIR gather does one multiply per tap instead of two and no longer reads the sign array.
  Bit-identical output — `w·(±1)` is an exact sign flip and the gather's evaluation order is
  unchanged; proven byte-identical across the 25-scenario full-engine dump (audio and scope-ring
  publications). Only the already-approved low-risk fold; the larger tap-order restructure (H5)
  is not part of this change. Expected effect (existing Round-2 measurements): −2-3 µs on the
  velvet-1.0 row. Evidence: PR #58. [Verified]
- **Chorus/Dimension-D LFO generation is a quadrature recurrence (Wave 2 / H11)**: the two
  per-sample `std::sin` calls are one double-precision `(sin, cos)` pair advanced by a fixed
  per-sample rotation and re-seeded from the LFO phase at every block start (the right channel's
  90° offset is exactly the `cos` component). Modulation rate, depth, stereo phase offset and
  the reported latency are unchanged; the LFO phase state itself still accumulates exactly as
  before, so block-to-block continuity and re-engage from the parked amount-0 fast path are
  bit-identical. Audible output is numerically class B: differences are confined to
  chorus-active blocks and bounded by a sub-0.1-sample delay wobble (measured ≤8.2e-4 peak
  sample delta across the 25-scenario full-engine dump; all other scenarios byte-identical).
  Expected effect (from the existing Round-2 measurements, no new profiling): chorus/Dim-D rows
  −~5 µs; everything-on-os4 −15-20 µs. Evidence: PR #58. [Verified]
- **Final Wave-1 DSP micro-optimisations (H9 + H10 + H12, one bundle)**: (H9) two per-block
  buffer copies that were byte-identical dead weight are gone — the silence-edge scan now reads
  the dry/Mix buffer it always duplicated (`inputScratch` removed), the loudness matcher is fed
  the live output pointers it always copied (`wetScratch` removed) — and the bypass ring's
  delay-aligned read-back is skipped while the Bypass crossfade is settled off (the ring writes
  always continue, so a later engage still reads valid history). (H10) input conditioning
  returns before its per-sample loop when the routing is at the default identity and the
  balance/polarity smoothers are fully settled at exactly 0 / +1 — every sample would compute
  `x·1·1`, and a settled smoother tick is mutation-free, so the skip is state-identical. (H12)
  Chorus and Dimension-D skip their LFO sines and 2/4 interpolated delay reads per sample while
  the wet glide sits at exactly 0 (it flushes to true zero under the block's FTZ) — the delay
  writes, write indices, iterated phase accumulation and depth glide all still advance, so a
  re-engage is bit-identical (the VelvetNoise S5 pattern). Output proven byte-identical on a
  114 MB, 25-scenario full-engine dump including chorus/Dim-D idle→engage→idle cycles at base
  and 4× oversampled rates, all eight conditioning routings, and bypass toggles under OS
  latency; reported latency unchanged. Expected effect (from the Wave-1.4 measurements):
  ~−7-10 % of the transparent floor (conditioning ~5 %, dead copies ~3-5 %) and the parked
  chorus/Dim-D rows drop ~8-14 µs/block to just above the floor. Evidence: PR #55. [Verified]
- **Branchless level-meter envelopes (H8)**: the per-sample rise-or-fall coefficient picks and
  the peak attack-or-decay picks in `StereoLevel::process` (and the NaN/Inf input clamp) now go
  through a branchless bit-select instead of data-dependent ternaries. Those branches flipped
  with the audio itself, so the predictor could not learn them — measured: they owned 87 % of
  ALL branch mispredicts in the transparent engine profile (the two RMS-body picks alone 76 %).
  After the change the meters' mispredicts drop from 239k to 911 per 4 s window (−99.6 %) and
  total engine mispredicts fall 87 %. Values are bit-identical for every input including
  NaN/Inf/−0.0 (the chosen value's bits pass through untouched): a 3,000-block meter-value dump
  across music/clip/silence/denormal/NaN-injection/alternating-polarity regimes and the
  22.5 M-sample full-engine dump are both byte-equal to the pre-change build. Measured wall
  (interleaved): active default −10.3 % (42.2 → 37.9 µs/block), other active rows −2…−7 %;
  the all-silence row pays ~+1.5 µs (perfectly-predicted branches were free; the mask ops are
  not) — a disclosed trade in favour of the active case. Evidence: PR #55. [Verified]
- **Spectrum analyser bottom-layer cache (H17)**: the analyser's glass panel, band tints and
  frequency-grid verticals — everything painted below the live spectrum — are now rendered once
  into a cached physical-resolution image and composited per frame instead of being re-rasterized
  at 60 Hz. Unlike the scope/meter caches the key includes eased inputs (panel hover wash, drawn
  split/width positions, width-hover washes, solo mask); every one of those eases converges
  exactly onto a snap, so the key settles and steady-state paints never rebuild (measured: zero
  rebuilds, ~430-instruction guard), while an animating value rebuilds per frame at the old
  drawing cost. The layer stays translucent (ARGB): the analyser sits on the editor's
  semi-transparent Multiband panel, so the N2 opacity pattern is deliberately not applied.
  Validated byte-identical against the uncached renderer across 26 scenarios (quiet and
  clip-red runs × widths incl. odd, 1.25× scale and back, LookAndFeel refresh, split/width
  parameter changes, solo mask, resize storm, destroy/recreate, silence decay). Measured:
  analyser paint −20 % at component level; Advanced-view active editor −3.7 % of a core
  (interleaved) — the remaining analyser cost is the live spectrum path itself plus the
  layer composite. Default view, idle, and editor-closed cost unchanged. Evidence: this PR.
  [Verified]
- **Opaque cached scope/meter rendering (N2)**: the Vectorscope and both Correlation/Balance
  meters are now `setOpaque(true)`; their cached static layers are RGB images whose rounded-panel
  corners pre-fill the editor's flat backdrop colour (`colours::bg`) — exactly what the parent
  used to show through. The per-frame layer blit therefore becomes an opaque copy instead of a
  per-pixel alpha composite (previously the single largest item of the active default-view GUI
  profile), and the editor no longer re-renders its background beneath these components on every
  repaint. Measured (interleaved, same session): active default-view editor CPU −11.6 %; the blit
  cost share more than halved; parent background overdraw halved (the remainder belongs to other,
  still-translucent children); idle and editor-closed cost unchanged; zero steady-state cache
  rebuilds. Composited pixel validation across 42 scenarios (signal, clip ring, silence, resize,
  resize storm, 1.25× scale, LookAndFeel refresh, reopen, persistence, all four meter combos,
  pointer at extremes): every difference bounded at ±1 channel LSB (±2 at fractional DPI scales),
  confined to the rounded-corner anti-aliasing arcs (one compositing-quantization step moved from
  blit time to cache-build time). The corner pre-fill couples these components to the editor's
  flat `colours::bg` backdrop — documented at both call sites. Evidence: PR #55. [Verified]
- **Correlation/Balance meter static-layer cache (H13)**: each `StereoMeter` now renders its
  glass panel and centre tick once into a cached physical-resolution image (rebuilt only on
  resize, DPI/UI-scale change or LookAndFeel change) and blits it per frame; the live pointer
  (glow, gradient core, highlight) and the end labels keep their exact draw order on top.
  Measured (Wave 1.2 profiling): the panel fill was 14.6 % of the active default-view GUI
  profile. Validated byte-identical against the uncached renderer across all four
  orientation/type combos, pointer at centre/extremes (including over the end labels), resize,
  continuous resize, 1.25× scale, LookAndFeel refresh and reopen — at every integral physical
  size; at fractional physical sizes (e.g. 125 % DPI on an odd height) the blit takes JUCE's
  interpolating path (the `setBufferedToImage` behaviour) with sub-perceptual AA-border wobble.
  Evidence: PR #55. [Verified]
- **Vectorscope paint cost (H2)**: the scope's static layer — background gradient, rounded panel,
  glass edges, grid and axis labels, all a pure function of (size, physical scale, look) — is now
  rendered once into a cached ARGB image at physical resolution and blitted per frame; only the
  signal-dependent point cloud and clip ring are rasterized live. The cache rebuilds only on
  resize, DPI/scale change or a LookAndFeel change; a normal repaint never re-rasterizes it, and
  the repaint *scheduling* (60 Hz timer + 0.8.8 idle gate) is untouched. Rendering is verified
  pixel-identical: a 10-scenario before/after snapshot harness (signal, clip ring, silence,
  resize, continuous-resize storm, 1.25× scale, LookAndFeel refresh, component reopen,
  persistence change) produced byte-identical images in every case. Measured before the change
  (0.8.8+H1 profile): `Vectorscope::paint` was 66 % of the active default-view GUI profile, ~70 %
  of it this static layer. Evidence: PR #55. [Verified]
- **Band Solo monitor settled fast path (H1)**: with nothing soloed, every crossfade gain fully
  settled (`passGain` at exactly 1, all band gains at exactly 0) and no crossover glide pending,
  `SoloMonitor::process` now skips its per-sample work (6 Linkwitz-Riley `processSample` calls +
  5 smoother ticks per sample) — the settled output is provably the input — and the filter bank
  goes cold. Re-entry (solo engage) resets the filters and snaps the cutoff glide while every
  band gain is still ~0, so the charge-up is masked by the existing ~12 ms crossfade; engaging,
  changing and clearing solo stay click-free (measured: identical max sample-to-sample step at
  every boundary, steady-state solo output converges to 0 difference). Parked output is
  bit-identical except the sign of exact zeros (a `-0.0` input is now passed through instead of
  being rewritten to `+0.0` by the settled `1·x + 0·band` arithmetic; 6,723 signed-zero flips and
  0 numeric differences across a 22.5 M-sample 15-scenario full-engine dump). Measured on the
  profiling reference (Xeon 2.1 GHz, 48 kHz/512): transparent engine floor 42.0 → 25.4 µs/block
  (−39 %), active default 45.4 → 30.4 µs/block (−33 %); every no-solo scenario drops ~15-20 µs.
  Evidence: PR #55. [Verified]

## [0.8.8] — 2026-07-08
### Added
- Documentation library under `docs/`: architecture reference + 12 ADRs, binding policies,
  procedures, and tracking docs (HANDOVER, POSTMORTEMS, KNOWN_ISSUES, FUTURE_RISKS, REPOSITORY_MAP,
  DOCUMENTATION_COVERAGE), plus this `CHANGELOG.md`. No plugin/behaviour change.
  Evidence: commits `c9b7fdf`, `a9e915e`, `97060b2`. [Verified]
### Changed
- **Engine CPU micro-optimisations**: the drive waveshaper's peak-preserving makeup (1/tanh) and
  its gain/blend reads are hoisted out of the per-sample loop once both smoothers have settled
  (any glide keeps the exact per-sample path), the two always-on delay rings wrap their write
  index by branch instead of an integer division per sample, and the Level-Match silence-energy
  scan runs only while Match is on or its engage duck is in flight (which keeps the silence-edge
  snap decision exactly as before). Output is bit-identical (full-engine dump across 20 scenarios
  incl. drive automation, all oversampling modes, impulse/delay-alignment, bypass, Match toggling
  with an engage-vs-audio-edge alignment sweep: byte-equal; reported latency unchanged). Measured:
  drive engaged −19/−38/−73 µs per 512-sample block at OS ×2/×4/×8, ~−6 µs/block across the board
  from the ring wrap, ~−1.6 µs/block with Match off. Evidence: PR #54. [Verified]
- **Spectrum analyser paint cost**: the analyser's per-paint work is cheaper without changing a
  single pixel -- the inverse frequency-axis mapping (a 30-iteration bisection previously run
  ~3× per pixel column) and the clip-red column→FFT-bin mapping are now served from lookup tables
  rebuilt only on resize / sample-rate change, and the clip feather buffer is reused across paints
  instead of reallocated. Every value comes from the exact same math as before, so output is
  bit-identical (verified byte-for-byte across five widths, clip off and clip on). Measured:
  ~9.5 → ~7.5 ms per full analyser paint at 900 px (worst case, clip lit) on the software
  renderer. Evidence: PR #54. [Verified]
- **Micro-animation idle cost**: the per-frame widget poll (hover/press/toggle/knob easing)
  resolves each widget's type once at registration instead of two dynamic_casts per widget per
  frame, replaces ~70 per-widget mouse queries with one editor-level test while the cursor is
  outside, and skips the walk entirely only when everything is provably static (cursor outside,
  no button held, no sweep, previous pass moved nothing, and a fingerprint of every tracked
  slider value / toggle state unchanged -- so host automation and session restores re-arm it the
  same frame). Hover/stuck-hover behaviour and all animation timing unchanged (measured: ~4,300
  widget evaluations/s idle → 0, with instant full-rate resume on cursor entry). Evidence:
  PR #54. [Verified]
- **Editor idle polling**: the 24 Hz undo-coalescing and preset-dirty polls now rebuild their
  parameter-signature strings only when a sound parameter actually changed (a relaxed-atomic
  generation counter, `soundParamGen`, bumped by the existing per-parameter listener and on host
  state restore so the preset dirty-star stays correct); polling cadence, undo coalescing and the
  dirty-star semantics are unchanged. Measured: 48 signature builds/s (~1 700 String formats/s)
  while idle → 0. Evidence: PR #54. [Verified]
- **Scope ring publish batched**: the audio thread now publishes the vectorscope/analyser ring's
  write index once per block (one release-store) instead of once per sample, on the same atomic
  with the same release/acquire contract -- readers see whole blocks atomically and can never
  observe partially committed frames. Audio output, ring contents and read counters are
  byte-identical (deterministic dump), and a two-thread stress (10⁹-frame scale) shows no
  publication tears in either the old or new design. Measured: ~−2 µs per 512-sample block
  median across the matrix. Evidence: PR #54. [Verified]
- **Velvet decorrelator CPU (2)**: the sparse-FIR tap accumulation is now skipped when its
  contribution is exactly zero -- Amount exactly 0 (the default state) or the presence gate
  exactly closed (silence from start, or after the transport-stop flush) -- outside any stop
  fade, which keeps running the full path. No thresholds: only provably-exact zeros are skipped,
  history writes and every envelope/glide keep running, and output is bit-identical (validated
  sample-exact across 12 scenarios / ~5.6 M samples incl. a signed-zero adversarial case).
  Engine cost with Velvet at Amount 0 drops a further ~15-19 µs per 512-sample block at 48 kHz.
  Evidence: PR #54. [Verified]
- **Velvet decorrelator CPU**: the per-sample tap re-weighting (64-tap rebuild + square-root
  normalisation) now runs only while the Density glide is actually moving; once the glide reaches
  its float fixpoint the rebuild is skipped on an exact bit-compare (never a threshold -- the
  pre-0.4.1 drift gate was the #18 zipper and stays dead). Output is bit-identical in every
  scenario, moving or settled (validated sample-exact across 9 scenarios / ~3.9 M samples incl.
  fast Density drags, transport stop and the default preset). Engine cost with Velvet selected
  drops ~36-38 µs per 512-sample block at 48 kHz (default idle state −40 %); zipper-free Density
  behaviour is unchanged. Evidence: PR #54. [Verified]
- **Meters idle CPU/GPU** (Balance / Correlation pointers, Levels panel): each meter now repaints
  only when what it draws actually changed, and the default-hidden Levels panel stops its 60 Hz
  timer entirely while hidden (it restarts on Show Meters). The correlation/balance pointers'
  return-to-centre relax completes in full, then lands exactly on target (final step under 0.2 px
  and a quarter of a colour quantum -- invisible); the Levels panel compares every published value
  bitwise, so no decay, hold, clip colour or number update can ever be skipped. Ballistics, attack/
  release and all animations are unchanged while values move (measured: full-rate repaints while
  anything moves incl. the whole silence decay; 0 repaints once settled; hidden-meter timer
  wakeups 60/s → 0). Evidence: PR #54. [Verified]
- **Spectrum (Multiband) idle CPU/GPU**: the analyser now runs its 8192-point FFT only when the
  analysis window actually changed, and repaints only while something on screen still moves
  (spectrum decay, clip-red fade, animations, drags). Digital silence stops the FFT as soon as
  the window has drained (~170 ms) while the displayed decays complete in full; a frozen
  transport or a hidden imager (Simple mode / hidden editor) costs nothing. Re-showing resumes
  live analysis on the first frame. Analysis maths, FFT size/window, decay rates and rendering
  are unchanged (measured: 60/60 FFTs+paints per second while active, before and after; silence:
  FFT stops after ~11 ticks, paints end once decays land; hidden: 60 FFTs/s → 0).
  Evidence: PR #54. [Verified]
- **Vectorscope idle CPU/GPU**: the 60 Hz timer now repaints only while the displayed picture can
  actually change; after the trail fully scrolls out on digital silence (or when the host stops
  processing), the view paints one final frame and goes idle. Rendering while audio flows is
  unchanged (measured: full frame rate active; 0 repaints/s once quiescent; idle-editor CPU
  −40 % on the Linux Standalone under Xvfb). Trail look, decay timing, and persistence behaviour
  are pixel-identical. Evidence: PR #54. [Verified]
- Upgraded the pinned **JUCE** dependency **8.0.8 → 8.0.14** (`CMakeLists.txt` `ANAMORPH_JUCE_TAG`;
  see ADR-0012). Build/dependency change only — no DSP, signal-chain, parameter, or serialization
  changes; CI re-validates the build + 23 DSP self-tests + pluginval (strictness 10), green on the
  Linux gate. The post-upgrade manual audition (Level 5) against the 8.0.8 baseline found no
  perceptual regressions (ADR-0012). Evidence: `CMakeLists.txt:33`; commit `41acaa7`. [Verified]
- Refactored the root `README.md` (slimmed; version history moved into this file) and `CLAUDE.md`
  (policy entry-point); corrected documentation citations and aligned/clarified the signal-chain
  section comments in `EngineParameters.h` / `AnamorphEngine.cpp` (comment-only, no behaviour
  change). Evidence: commits `e83370d`, `2fe5e05`, `1914c52`, `655b6e4`. [Verified]
- CI pluginval gate **unified and hardened across all three platforms**: each of Linux, Windows and
  macOS now runs pluginval at strictness 10 in **two explicit, blocking steps** — deterministic
  (`--random-seed 0`) **and** `--randomise` — **each repeated 3 consecutive times**. The previous
  Windows/macOS `continue-on-error` (which swallowed `exit 1` and reported a false green) is removed;
  a non-zero pluginval exit now fails the job on every platform. Linux/macOS use
  `scripts/run-pluginval.sh`, Windows uses the new `scripts/run-pluginval.ps1` (same structure).
  `actions/checkout` and `actions/upload-artifact` bumped `v4 → v5` (clears the Node 20 deprecation
  warning). Evidence: `.github/workflows/build.yml`, `scripts/run-pluginval.sh`,
  `scripts/run-pluginval.ps1`.
- **Parameter display-name renames** (parameter **IDs unchanged**, so automation/state survive):
  "Algorithm" → **"Widen Algorithm"** and "Dimension Mode" → **"Dim-D Style"**, matching the GUI.
  `Multiband Bands` and `Multiband Solo` are now **exposed and automatable** in the host automation
  list (the previous `withAutomatable(false)` was removed). Conversely, **`Advanced Mode` is now
  non-automatable** (`isAutomatable()` = false): it is a UI-layout toggle, not a sound parameter.
  Host-automating it drives editor resizes (`applyUiScale`), and on **Linux/X11** the resize
  `ConfigureNotify` storm hits a use-after-free in the **host's** JUCE `XEmbedComponent` during rapid
  open/close (reproduced locally; the core dump lands in `XEmbedComponent` — KI-003/KI-007). A layout
  toggle has no place in an automation lane anyway. IDs, ranges and defaults are unchanged (a recorded
  automation-flag change, `PARAMETER_COMPATIBILITY_POLICY` rule 5). Evidence: `src/PluginParameters.cpp`;
  `docs/architecture/PARAMETER_REGISTRY.md`.
- **CI: the randomise pluginval gate is never skipped.** The randomise step (all three platforms) is
  guarded with `if: ${{ !cancelled() }}`, so a deterministic-mode failure no longer skips the randomise
  run — both modes report independently every CI run. The job still fails if either mode fails.
  Evidence: `.github/workflows/build.yml`; `docs/procedures/CI_CD.md`.
### Fixed
- **A/B compare slots are independent from plugin open again.** The two A/B slots were snapshotted
  **lazily** on the *first* A/B switch (`abEnsureInit`), so editing A *before* ever visiting B made B
  born as a copy of A's **already-edited** state — switching to B showed A's parameters, not the open
  (Default) state. Whether B ever looked "clean" depended on when the host happened to call
  `getStateInformation` (which also runs `abEnsureInit`) — a host-timing accident. Both slots are now
  initialized to the open state in the constructor, so an edit to A never leaks into B. The A/B
  switch/apply/serialization logic is unchanged (ADR-0008); only *when* the initial snapshot is taken
  changed. Evidence: `src/PluginProcessor.cpp` (constructor `abEnsureInit()`).
- **A corrupt user preset no longer leaves the undo bracket half-open.** In `PresetManager::load`,
  `onAboutToLoad` (which flushes undo coalescing) fired *before* the preset XML was parsed, so a file
  that failed to parse returned early and never fired the matching `onLoaded`, silently flushing a
  settled edit without recording its undo step. The XML is now parsed **before** the bracket is opened
  (matching `loadFile`), so a parse failure is a clean no-op. Evidence: `src/PresetManager.cpp` (`load`).
- **Windows pluginval: the script now WAITS for pluginval — fixes garbled output and false pass/fail
  (KI-007).** `pluginval.exe` is a **GUI-subsystem** app, so PowerShell's call operator (`& $pv`)
  returned immediately with a `$null` exit code *without waiting*. The original `exit $LASTEXITCODE`
  false-greened (null → `exit 0`); after the crash-retry loop was added, that null was misread as a
  crash and **each retry launched another pluginval that kept validating in the background** — three
  concurrent validators writing one console (the "garbled" interleaving) and a false failure, while the
  plugin actually validated fine. `scripts/run-pluginval.ps1` now launches pluginval via
  `System.Diagnostics.Process` (`UseShellExecute=$false`) + `WaitForExit()` and reads the **real**
  `.ExitCode`; exactly one runs at a time (no interleaving). OpenGL GPU rendering stays **ON** for
  Windows/macOS (`#if ! (JUCE_LINUX || JUCE_BSD)`); Windows CI keeps `--skip-gui-tests` conservatively
  (the GPU-less runner's GDI-generic OpenGL 1.1 very likely can't render the JUCE GL editor — never
  observed because the wait bug masked all Windows editor results; the editor is validated on Linux +
  macOS). Evidence: `scripts/run-pluginval.ps1`; KI-007.
- **Host state restore no longer notifies the host of parameter changes (Devin review).** During
  `setStateInformation`, `reassertParameters` called `setValueNotifyingHost` for each restored
  parameter, notifying the host mid-restore (some DAWs treat that as an automation write). It now takes
  a `notifyHost` flag: the host-restore path updates `getValue()` (`setValue`) and writes the DSP raw
  atomic directly — **no host notification** — while undo/redo/A-B (editor-initiated) keep the full
  notifying path. Evidence: `src/PluginProcessor.cpp` (`reassertParameters`).
- **Preset switching is undoable again (regression from the gesture-gated undo).** A preset load
  arrives as gesture-less `setValueNotifyingHost` calls, so the new gesture-gated coalescer folded it
  into the baseline **without** an undo step — after switching presets you could not Undo back to the
  previous preset. Each load is now explicitly bracketed (`PresetManager::onAboutToLoad` / `onLoaded`):
  a settled edit is flushed first, then the switch is recorded as exactly **one** undo step in the
  **active A/B slot's** history. A/B slots keep their independent histories (by design, ADR-0008);
  only preset switches *within* a slot are chained, and the switch itself is now an undo/redo step.
  Evidence: `src/PluginProcessor.cpp` (`commitPresetSwitchUndoStep`, constructor hooks),
  `src/PresetManager.cpp` (`load` / `loadFile`).
- **Windows pluginval no longer reports a false green when it crashes.** `scripts/run-pluginval.ps1`
  ran `exit $LASTEXITCODE`, but an abnormal pluginval termination (e.g. a crash in the editor tests)
  leaves `$LASTEXITCODE` `$null`, and `exit $null` exits **0** — so a crashed run *passed* the gate
  (observed: the Windows step ran in ~6–7 s vs Linux ~40 s / macOS ~185 s, ending at
  `pluginval: FAILED … (exit )` with an empty code yet still green). The script now treats a
  null/negative/large exit code as a crash (never success) and, like `run-pluginval.sh`, retries a
  crash and still fails after the retries — only a clean `exit 0` passes. This surfaces a pre-existing
  Windows "Editor Automation" crash (now tracked as **KI-007**). Evidence: `scripts/run-pluginval.ps1`.
- **Undo/Redo: one step per gesture, and host automation is never recorded.** Undo coalescing was
  time/signature-settle based, so a slow drag that dwelt mid-gesture (esp. Multiband Split / Band
  Width) recorded multiple intermediate steps, and any host-automation move could create undo steps.
  It is now **gesture-gated**: the processor listens to parameter begin/end gestures and commits
  exactly **one** undo step after the last gesture closes; automation (which never opens a gesture)
  folds into the baseline without an undo entry. A/B switch/copy **and undo/redo** reset the gesture
  state (a state jump is never a user gesture, so nothing re-commits after it). Evidence:
  `src/PluginProcessor.cpp` (`parameterGestureChanged` / `pollUndoCoalesce` / `undo` / `redo`).
- **Combo-box pop-ups drop BELOW the box again** instead of covering it with the selected item under
  the cursor. Added `AnamorphLookAndFeel::getOptionsForComboBoxPopupMenu` targeting the box's screen
  bounds (omitting the JUCE default `withItemThatMustBeVisible`/`withInitiallySelectedItem`). Evidence:
  `src/gui/LookAndFeel.cpp` (`getOptionsForComboBoxPopupMenu`).
- **Discrete parameters now round-trip their exact value under pluginval `--randomise`.** Stock
  `AudioParameterBool`/`Choice`/`Int` snap `getValue()` to the nearest legal step, which for few-step
  params can be `>0.1` from the raw value pluginval sets (seed-dependent "not restored" failures) — and
  they cannot be subclassed to fix it (JUCE declares their `getValue()`/`setValue()` **private**). The
  discrete params are reimplemented as minimal from-scratch `juce::RangedAudioParameter` subclasses
  (`RawChoice`/`RawBool`/`RawInt`) whose `getValue()` keeps the exact raw normalised value (restored via
  the `raw` attribute + `reassertParameters`); the DSP still reads the snapped value via
  `getRawParameterValue()` and host text via `getAllValueStrings()`. Because these are no longer the
  stock concrete types, `getBypassParameter()` now holds an `AudioProcessorParameter*` (no
  `dynamic_cast`) and the ComboBox item list is read from `getAllValueStrings()` — no behaviour change.
  See ADR-0013. Evidence: `src/PluginParameters.cpp` (`RawChoice`/`RawBool`/`RawInt`).
- **State restoration now round-trips every parameter exactly.** Two issues, both surfaced by the
  `--randomise` *Plugin state restoration* gate: (1) a wholesale `apvts.replaceState` did not
  reliably propagate to every parameter's cached value (an occasional param kept its pre-restore
  value); (2) APVTS serialises the **denormalised/snapped** value, so a **discrete** param
  (Bool/Choice/Int) given a raw normalised value mid-step (e.g. `Input Channel` at `0.177521` on a
  3-choice) round-tripped to the nearest legal value — `>0.1` away — and pluginval flagged it "not
  restored". Fix: `getStateInformation` additively records each parameter's **exact raw
  `getValue()`** as a `raw` attribute on its `PARAM` node, and `setStateInformation` →
  `reassertParameters` restores from `raw` (falling back to the denormalised `value` for legacy
  sessions). Additive + backward-compatible (old sessions ignore `raw`; the APVTS `value` is
  unchanged — no field removed/renamed). Evidence: `src/PluginProcessor.cpp`
  (`getStateInformation` / `reassertParameters`); CI runs `28356632727`, `28388176607` (the
  `--randomise` failures: discrete params "not restored"). See `SERIALIZATION_REGISTRY.md`.
- **The exact-value restore is extended to user actions** — undo / redo / A-B apply now re-assert
  every parameter from the snapshot (`reassertParameters` after `replaceState` in
  `applyStatePreservingView`), and A/B-slot snapshots carry the `raw` attribute
  (`copyStateWithRawValues`, used by `currentStateSet`), so discrete params no longer snap-drift on
  slot switching or undo. Evidence: `src/PluginProcessor.cpp`.
- **Windows CI no longer skips the randomise pluginval pass.** `run-pluginval.ps1` now makes the
  pluginval **exit code the sole** pass/fail signal (`$ErrorActionPreference = Continue` +
  `$PSNativeCommandUseErrorActionPreference = $false`), so pluginval's stderr progress can no longer
  throw a terminating error that fails the *deterministic* step and makes GitHub **skip** the
  randomise step. Evidence: `scripts/run-pluginval.ps1`.
- **Defensive A/B bounds.** `abSwitchTo` clamps its slot index (`juce::jlimit(0, kNumAbSlots-1, …)`),
  and `abUndo` / `abSlot` / `abMatchGain` are sized from `anamorph::kNumAbSlots` (single source of
  truth) instead of a hardcoded `2`. Evidence: `src/PluginProcessor.{h,cpp}`; `src/AbSlotIndex.h`.
- **Linux:** tooltips no longer render opaque **black corners** outside the rounded capsule on X11
  without a compositor — `drawTooltip` now fills the corner area with the capsule colour when
  per-pixel window alpha is unavailable; macOS/Windows transparent corners are unchanged (KI-006).
  Evidence: `src/gui/LookAndFeel.cpp` (`drawTooltip`). [Partially Verified] (Linux visual re-test pending)
- Session restore now **clamps a corrupted / out-of-range A/B "active" index** so it can never index
  the A/B slot arrays out of bounds; valid sessions are unaffected. Evidence:
  `src/PluginProcessor.cpp` (`setStateInformation`), `src/AbSlotIndex.h`; regression test
  `testAbActiveClampOnCorruptState`. [Verified]

## [0.8.7] — 2026-06-28
### Fixed
- Audible click when toggling Multiband Enable while a Band Solo was active: the post-everything
  Band-Solo monitor now runs every block (mask driven from `mbEnable`) instead of being hard-gated,
  so it morphs solo↔passthrough over its own ramp. Evidence: commit `6a24b82`; test
  `testSoloMultibandEnableClickFree`. [Verified]

## [0.8.6] — 2026-06-28
### Fixed
- Alt/Option-click knob reset now animates like double-click (a `resetSweep` flag opts the eased
  travel out of the button-held snap). Evidence: commit `10fbfa0`. [Partially Verified]
### Changed
- Multiband Enable now transitions via a ~12 ms click-free output crossfade (warm crossover bank),
  not a duck-to-silence — no mute/dropout. Evidence: commit `10fbfa0`; test
  `testMultibandEnableCrossfadeClickFree`. [Verified]
- Renamed the automation parameter display name **"Haas Side" → "Haas Focus"** (ID `haasSide`
  unchanged). Evidence: commit `10fbfa0`; `src/PluginParameters.cpp:135-136`. [Verified]

## [0.8.5] — 2026-06-28
### Fixed
- Linux editor crash under rapid open/close (OpenGL/X11 `XEmbedComponent` use-after-free): the
  editor now renders CPU-side on Linux/BSD (visually identical). Evidence: commit `c924ff8`. [Partially Verified] / code [Verified] (`src/PluginEditor.cpp:246-256`).

## [0.8.4] — 2026-06-27
### Changed
- Oversampling, Window Size, Scope Persistence, Tooltips, UI Animations and Show Meters are hidden
  from the host parameter list (moved out of the APVTS into a host-hidden `InternalState`); pre-0.8.4
  sessions are migrated. Evidence: commit `6bd158b`. [Partially Verified] / code [Verified].

## [0.8.3] — 2026-06-27
### Changed
- Bypass is a true click-free crossfade and the chain + Level-Match analysis always run (Bypass only
  changes the audio path). Confirmed there is no 0 dBFS output clipper. Evidence: commit `3686d12`;
  tests `testBypassCrossfadeClickFree`, `testLevelMatchRunsInBypass`. [Verified]

## [0.8.2] — 2026-06-27
### Fixed
- Multiband crossover automation no longer explodes near Nyquist (Nyquist-safe clamp + top-down
  ordering); meters recover from a NaN burst; Level Match reads ~0 at unity with Multiband on; clean
  Bypass transitions; meter holds reset on a transport seek. Evidence: commit `f259a80`; tests
  `testCrossoverAutomationSafe`, `testMeterRecoversFromNaN`, `testMultibandUnityMatch`. [Verified]
### Changed
- Advanced state travels with A/B; Settings/Multiband-Bands/Solo de-cluttered from automation;
  M/S-clarified automation names. Evidence: commit `f259a80`. [Partially Verified]

## [0.8.1] — 2026-06-23
### Fixed
- Band Solo is click-free and ghost-free (warm monitor, no duck); Level Match no longer ratchets
  toward −24 dB or slams at Mix=100% (Measure + absolute Predict). Evidence: commit `6d2023b`; tests
  `testSoloNoGhostInSilence`, `testLevelMatchNoRatchet`, `testLevelMatchMixCouplingNoSlam`. [Verified]
### Changed
- Folded the two outlier transitions into the one anti-click layer; band-pass preview is
  press-and-hold only. Evidence: commit `6d2023b`. [Partially Verified]

## [0.8.0] — 2026-06-22
### Changed
- Signal flow rebuilt as a strict serial chain: **Processing → Mix → Mono Maker (post-Mix) → Output
  → Band Solo monitor (post-everything)**, eliminating the solo/low-cut bug class. Evidence: commit
  `018dcdd`; tests `testMonoMakerPostMix`, `testSoloMonitor`. [Verified]

## [0.7.5] – [0.7.0] — 2026-06-21…22
### Changed
- 0.7.5 (`6846c60`): Mono Maker lows follow band 0's solo. 0.7.4 (`818b22f`): keep Mono Maker lows
  present while a band is soloed. 0.7.3 (`37526da`): Multiband Solo obeys Mix; Windows window-size
  DPI fix. 0.7.2 (`7d0ccdf`): phase-align the dry path with the Multiband crossovers for the Mix
  (mono-compatible at any Mix). 0.7.1 (`911701d`): per-band width smoothing (fast-drag clicks) +
  3-OS full-format CI. 0.7.0 (`dac5beb`): ground-up Multiband spectral editor (1–4 bands, drag-to-
  split, per-band Solo); pluginval gate at strictness 10. Evidence: the cited commits.
  [Partially Verified]

## [0.6.x] and earlier — 2026-06 (reconstructed)
`[Unverified Historical Reconstruction]` — the 0.2 → 0.6.19 line (variable-band Multiband DSP+GUI,
4-bit per-band Solo, the asymmetric click-free switch duck, Undo/Redo per A/B slot, M/S
encode→decode, transparent-on-load, level meters, oversampling) is described in commit history (the pre-refactor README's "What's new" sections, now superseded by this changelog) and exists as version commits in history (e.g. 0.6.10
`98e2886` … 0.6.19 `9da01ad`), but the repository has **no tags** to attribute exact per-version
feature sets to a released artifact. See `README.md` history for the narrative.

[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/
