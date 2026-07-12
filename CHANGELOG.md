# Changelog

All notable user-visible changes to Anamorph. Format follows [Keep a Changelog]; versions are
`MAJOR.MINOR.PATCH` (pre-1.0). The repository has **no git tags**, so each entry cites its **commit
SHA + date** as the Evidence Source (per `docs/policies/CHANGELOG_POLICY.md`). Entries for the
0.6.x line and earlier are reconstructed from commit history (the detailed per-version notes predate this changelog) and are marked accordingly.
Display-name renames are recorded as **Changed**, never as parameter removals (the IDs are immutable).

## [Unreleased]
### Fixed
- **The Band Solo tooltip reads `Solo this band` again.** The `- Alt-click solos / clears all
  bands` suffix shipped in 0.8.9 alongside the alt-click feature was never requested wording and
  has been removed; the alt-click behaviour itself is unchanged. UI copy is now covered by an
  explicit rule in `AI_AGENT_POLICY.md` (user-visible text requires explicit instruction).
  Evidence: PR #58. [Verified]
### Changed
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
- **VelvetNoise folds the fixed ±1 tap sign into the stored tap weight (Wave 2 / ALG-4)**: the
  sparse-FIR gather does one multiply per tap instead of two and no longer reads the sign array.
  Bit-identical output — `w·(±1)` is an exact sign flip and the gather's evaluation order is
  unchanged; proven byte-identical across the 25-scenario full-engine dump (audio and scope-ring
  publications). Only the already-approved low-risk fold; the larger tap-order restructure (H5)
  is not part of this change. Expected effect (existing Round-2 measurements): −2-3 µs on the
  velvet-1.0 row. Evidence: PR #58. [Verified]
- **VelvetNoise sparse-FIR gather is restructured tap-outer (Wave 2 / H5)**: while the density
  glide is settled and no transport-stop fade is in flight, the 64 random-index history reads per
  sample become one contiguous streaming run per tap over a linear image of the history, with the
  per-sample accumulation kept in the original ascending-tap order — **bit-identical output**,
  proven byte-identical across a 31-scenario full-engine dump including new density-glide,
  transport-stop-flush and engage/park-cycle scenarios; the glide, stop-fade and parked paths keep
  the original per-sample loop verbatim. Expected effect (existing Round-2 measurements): −25-30 %
  on the velvet-1.0 row (the gather owned 41.7 % of it and 45.6 % of its D1 read misses).
  Evidence: PR #58. [Verified]
- **The multiband/solo/mono-maker crossovers run on a local flat-state LR4 (Wave 2 / H6)**: all
  ten `juce::dsp::LinkwitzRileyFilter<float>` instances are replaced by `LR4Xover`, which
  reproduces the JUCE filter's coefficient derivation and TPT ladder expression-for-expression
  (including which products round in float and which sums run in double) while storing its state
  in flat per-channel floats instead of heap `std::vector`s — the vector indexing was 4.5-7 % of
  every multiband/solo row. **Bit-identical**: proven byte-exact on the 33-scenario full-engine
  dump, including new 4-band solo engage/change/clear cycles (cold re-entry) and per-sample
  crossover/mono-freq glide scenarios; reported latency unchanged. No dependency change (JUCE
  itself is untouched). Evidence: PR #58. [Verified]
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

## [0.8.9] — 2026-07-11
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
