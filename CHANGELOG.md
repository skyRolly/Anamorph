# Changelog

All notable user-visible changes to Anamorph. Format follows [Keep a Changelog]; versions are
`MAJOR.MINOR.PATCH` (pre-1.0). Entries up to and including `[0.8.12]` predate git tags and cite
their **commit SHA + date** as the Evidence Source (per `docs/policies/CHANGELOG_POLICY.md`).
The annotated-tag convention and the tag-triggered release pipeline exist
(`docs/procedures/RELEASE_PROCESS.md` §Tagging), but **no tag has been cut yet**: `[0.9.0]` was
written as a release entry and then superseded before it was tagged, so the first annotated
`vX.Y.Z` tag will be **v0.9.7** (0.9.0 through 0.9.6 were each written up and superseded
before tagging),
and from that tag onward the tag is also a citable Evidence
Source. Until then every entry cites a commit SHA or a PR. Entries for the
0.6.x line and earlier are reconstructed from commit history (the detailed per-version notes predate this changelog) and are marked accordingly.
Display-name renames are recorded as **Changed**, never as parameter removals (the IDs are immutable).

## [0.9.7] — 2026-09-03
### Changed
- **Turning up Drive, or changing algorithm, no longer interrupts the sound when Oversampling is
  on.** The latency the plug-in reports to your DAW used to depend on whether the oversampler was
  actually running, and it only runs when it has something to do — so with Oversampling set to 2×,
  4× or 8×, nudging Drive off zero (or switching to Chorus / Dimension-D) switched it on and changed
  the reported latency mid-move. DAWs answer a latency change by restarting their audio graph, which
  you hear as a dropout in the middle of an ordinary knob move. The reported latency now depends on
  the **Oversampling setting and nothing else**, so it changes when you change that setting and at
  no other time. Everything else about oversampling is unchanged, including the part that matters
  for CPU: the oversampler is still switched off whenever there is no nonlinear work for it, and the
  saving was measured to confirm it (2×/4×/8× with Drive at 0 cost the same as Oversampling Off,
  within run-to-run noise). While it is off, a plain delay of the same few samples holds the timing
  steady in its place, so the plug-in still delivers exactly the delay it declares.
  **What changes for you:** selecting 2×/4×/8× now shows a few samples of latency in your DAW even on
  a fully linear chain, where it used to show none — that is the price of the sound no longer being
  interrupted, and the delay is compensated as it always was. One trade in the other direction: an
  A/B, preset or undo switch **that changes the Oversampling setting itself** while Drive is at zero
  now dips briefly, where it used to be seamless — the same short dip the Oversampling menu has
  always had, so both ways of changing that setting now behave alike, and switching Oversampling is
  the one moment a dip is expected. Sessions are unaffected — nothing in the saved file changed.
  Decision: ADR-0034. Regression coverage: DSP test 52.
  Evidence: PR #135. [Verified]
- **Turning Drive through zero with Oversampling on is now seamless, not merely un-interrupted.**
  Crossing that point switches the plug-in between two internal paths, and the short dip that used to
  cover the switch could not actually cover it: the dip is applied at the very end of the chain,
  *after* Haas and Velvet Noise, whose delay lines are 12–35 ms long. The switch's discontinuity went
  into those delay lines at full level and came back out one delay later, with the dip already over —
  which is why the interruption was reported specifically for Haas and Velvet Noise. The two paths
  are now **crossfaded** instead, so there is nothing to cover and no dip at all. Measured on a steady
  tone across every combination of 2×/4×/8×, Haas and Velvet Noise, Drive rising through zero and
  falling through it, as an instant jump and as a 300 ms knob turn: the result is now identical to
  the same move with Oversampling switched off. The oversampler is still switched off when there is
  no nonlinear work for it, so the CPU saving is unchanged. Decision: ADR-0035.
  Regression coverage: DSP test 53. Evidence: PR #135. [Verified]

### Fixed
- **A click when Drive crossed zero while the plug-in was bypassed, with Oversampling on.** The same
  moving latency had a second effect nobody had reported: with Bypass engaged the output is the
  untouched input, held back by exactly the amount the plug-in declares — so when that amount changed
  the held-back signal jumped a few samples at full level, and the short dip that hides such changes
  does not apply while bypassed. Measured on a 220 Hz tone whose largest natural step between two
  samples is 0.0144: the jump stepped 0.0716 at 2× and 0.0999 at 4× and 8×, five to seven times a
  smooth signal. With the reported latency no longer moving, the jump cannot happen; the measured
  worst step is now exactly the smooth signal's own. Regression coverage: DSP test 52.
  Evidence: PR #135. [Verified]
- **Switching Oversampling from 2×, 4× or 8× to Off briefly dropped Drive and the modulation
  algorithms out of the sound.** The crossfade that hands the sound between the oversampled path and
  the normal one was left running across the Oversampling switch itself — and with Oversampling set
  to Off there is no oversampled path for it to fade from, so for about 12 ms after the switch the
  output was the plain, unprocessed input: no Drive, and no Chorus or Dimension-D. It arrived just as
  the switch's short dip was lifting, and it went into Haas's and Velvet Noise's delay lines at full
  level, so it came back out again a moment later. Measured with a 1 kHz tone and Drive at 18 dB, as
  the strength of the distortion the Drive stage produces: it fell to about a third of its settled
  value and took the full 12 ms to come back, against a switch between two oversampling factors —
  the same dip, the same everything, but with the oversampler running on both sides of it — which
  held perfectly steady. The crossfade now finishes at the switch instead of surviving it, and it
  can no longer fade toward a path that is not there. Switching between 2×, 4× and 8×, and switching
  Oversampling on, are all unchanged. Regression coverage: DSP test 54.
  Evidence: PR #135. [Verified]

## [0.9.6] — 2026-09-03
### Fixed
- **A damaged value in a project file is now cleaned out of the file even when it happens to read as
  the setting you already had.** Opening a project with a damaged control value repairs it, and the
  repair is written back so the next save cleans the file. But the write-back only happened when the
  repair actually *changed* something on screen — and damaged text usually reads as zero, which for
  any control whose range starts at its default (Drive, Amount, Channel Mode and others) *is* the
  value already loaded. Nothing looked wrong, so nothing was rewritten, and the damaged text stayed
  in the file through every later save — where an older version of the plug-in, which reads that
  field rather than the newer one beside it, would still find it. The file is now cleaned whenever
  the value it carries was damaged, whether or not the repair changes what you hear. Values that are
  genuinely valid are left exactly as they were written. Regression coverage: State test 36.
  Evidence: PR #134. [Verified]
- **A preset the plug-in refuses no longer dips the audio.** Loading a preset briefly ducks the
  sound so the change is never heard as a click — the right thing when a preset actually loads. But
  the dip was set up the moment you *asked* for a preset, before the plug-in had looked at the file.
  So a file it then refused — another plug-in's preset, or a corrupted one — still dipped the audio
  for about 32 ms while loading nothing at all. Measured on an engaged widener: the stereo width
  fell to 0.45 of its settled level for that moment. The dip now comes from the load itself, so it
  happens when, and only when, a preset is really applied. Loading a real Anamorph preset is
  unchanged and still ducks exactly as before. Regression coverage: State test 35.
  Evidence: PR #134. [Verified]
- **Loading a preset that is not an Anamorph preset no longer wipes your sound.** "Load Preset…"
  and the preset menu both let you point them at any file on your machine. If that file was a valid
  XML preset from *another* plug-in, it was treated as one of ours: any parameter whose name
  happened to match took the other plug-in's value, every parameter it did not mention was reset to
  its default, and the load reported success — so a foreign preset silently replaced the sound you
  were working on, and the only way back was Undo. A preset file now has to actually be an Anamorph preset. Anything
  else is refused before it can touch anything: the sound, the preset name and the menu tick all
  stay exactly as they were, the same way a corrupted file has always been refused. Loading real
  Anamorph presets is unchanged, including older ones that do not carry every parameter — those
  still fill in the missing controls with their defaults, exactly as before. Regression coverage:
  State test 34.
  Evidence: PR #134. [Verified]
- **At extreme levels the L/R balance meter no longer shows a lopsided signal as perfectly
  centred.** The balance meter reads how the energy is split between left and right. It works from
  the two channels' running energies added together, and above roughly 1.3e19 in sample value — far
  beyond anything you would mix, but ordinary numbers to the plug-in, and reachable when Bypass is
  passing a broken upstream signal through untouched — that addition ran out of range internally.
  Each channel's own energy was still fine and the difference between them was still fine; only
  their **sum** was not, and dividing by it collapsed the reading to **0 — dead centre** — for a
  signal that was in fact heavily weighted to one side. Nothing sounded different; only the meter
  lied, and it lied in the most misleading direction. When that addition does run out of range, the
  split is now worked out at higher precision instead, so the meter reports the real split at any
  level, and a genuinely centred signal still reads centred. Every reading at ordinary levels is
  bit-for-bit identical to before, and the phase-correlation display and the existing recovery from
  genuinely invalid audio are both unchanged. Regression coverage:
  DSP test 51.
  Evidence: PR #134. [Verified]
- **Extremely loud but still-valid audio no longer makes the phase meter read the wrong thing.**
  The correlation display shows how alike the two channels are: +1 for a mono signal, 0 for
  unrelated channels, −1 for anti-phase. It works from the running energy of each channel, and
  above roughly 4.3 billion in sample value — far above anything you would mix, but ordinary
  numbers as far as the plug-in is concerned, and reachable when Bypass is passing a broken upstream
  signal through untouched — the two energies multiplied together ran out of range internally,
  though each of them on its own was still fine. The reading then collapsed to **0, "unrelated"** —
  for a signal that was in fact perfectly mono — and an anti-phase signal read 0 instead of −1.
  Nothing sounded different; only the meter lied, and it lied in the
  most misleading direction. That multiplication is now worked out at higher precision when it runs
  out of range, so the meter reads the same value at any level, which is what a correlation display
  is supposed to do. Every reading at ordinary levels is bit-for-bit identical to before, and the
  existing recovery from genuinely invalid audio is unchanged. Regression coverage: DSP test 50.
  Evidence: PR #134. [Verified]
- **A project no longer fades into its own effects when it opens.** Haas, Velvet Noise, Chorus /
  Dimension-D and the Mono Maker crossover each glide their settings rather than jumping, so that
  moving a control while the music plays never clicks. On a project *opening*, that glide was
  starting from the wrong place: the effects were told the project's settings only after they had
  already been set up, so the first fraction of a second played the previous settings sliding into
  the saved ones instead of the saved ones outright. Measured over the first block against the
  settled sound, the effects opened at 0.17 (Haas), 0.09 (Velvet Noise), 0.29 (Chorus), 0.39
  (Dimension-D) and 0.35 (Mono Maker crossover) of where the project said they should be, arriving
  over roughly the next 10–100 ms. Each of them now opens already at the saved setting. Moving a
  control while playing still glides exactly as before — only the moment a project opens changed.
  Regression coverage: DSP test 49, which includes a control proving live moves still glide.
  Evidence: PR #134. [Verified]
- **A damaged on/off Setting in a project file can no longer switch a feature on that the project
  never asked for.** Three Settings are simple on/off switches (Show Meters, Tooltips, UI
  Animations) and are written to the project as `0` or `1`. When the Settings repair described in
  the next entry met a damaged value that still read as a number — `-1`, `-2`, `7` — it treated
  anything other than zero as "on", so a corrupted file could turn a feature on, and the repair
  then wrote that back as a genuine `1`. Only an exact `0` or `1` is now accepted; anything else
  is damage and takes that switch's documented default (Show Meters off, Tooltips off, UI
  Animations on), which is the same value the project would get if it did not carry the setting at
  all. Valid `0`/`1` values are
  preserved exactly as before. Regression coverage: State test 33.
  Evidence: PR #134. [Verified]
- **A damaged Settings value in a project file is now repaired when the project opens, and the
  repaired value is what gets saved.** The six Settings (Oversampling, UI Scale, Scope Persistence,
  Show Meters, Tooltips, UI Animations) are stored with the project. If one was damaged — hand-edited,
  or corrupted in transit — to something outside its range, or to text that is not a number at all,
  it used to be taken at face value and written straight back out on the next save, so the damage
  stayed in the file and was re-interpreted every time the project opened. Each value is now brought
  back into its valid range as the project loads: a number merely out of range moves to the nearest
  valid setting, and anything unreadable falls back to that setting's documented default. What the
  project saves next is the repaired value, so opening and re-saving cleans the file instead of
  carrying the damage forward. Projects with valid settings are unaffected, and a project that simply
  does not carry a setting still gets that setting's default exactly as before. Regression coverage:
  State test 33.
  Evidence: PR #134. [Verified]
- **A damaged Scope Persistence value in a project file can no longer put the vectorscope's afterglow
  into an undefined state.** The Settings panel's Scope Persistence is stored with the project as a
  number from 0 to 1. A project whose stored value was damaged (hand-edited, or corrupted in
  transit) to `nan`, or to any *negative* number, produced a not-a-number afterglow length inside
  the vectorscope: negative values became one on the way in, because the display curve raises the
  stored value to a fractional power. Neither was caught, because the clamp that should have caught
  them is written in a way that a not-a-number slips straight through, and opening the Settings
  panel did not repair either. The vectorscope now falls back to its default afterglow for any such
  value, exactly as the meters already do for a damaged audio sample. Ordinary projects are
  unaffected; the stored value is repaired separately, by the Settings repair described above.
  Regression coverage: State test 32.
  Evidence: PR #134. [Verified]
- **Opening a project that has no A/B data no longer carries the previous project's Level Match
  amount into the first A/B switch.** The amount Level Match had settled on is remembered per A/B
  slot, so that switching between A and B does not make the level lurch while the matcher
  re-converges. That memory is not stored in the project file — it describes the session you are
  in — but it was also never cleared when a project was opened, and a host reuses one plug-in
  instance across projects. Opening a project saved before A/B existed, or one whose A/B data is
  missing, therefore left the previous project's figures in place, and the first A/B switch handed
  them to the new project's matcher: the Level Match readout showed the old project's number and
  the matcher re-converged from it. Measured on the real restore paths as −1.040 dB and −2.438 dB
  where a freshly opened plug-in shows 0. The memory is now cleared with the slots themselves, so
  opening a project leaves the plug-in in the state it would be in if you had just added it. A
  project that does carry A/B data still restores both slots exactly as before. Regression
  coverage: State test 31.
  Evidence: PR #134. [Verified]
- **A host that activates the plug-in on a background thread can no longer be told a stale
  latency, and the plug-in no longer reads its own latency figures while they are being
  rewritten.** Most hosts activate a plug-in on their main thread, and there nothing changes: the
  reported latency still updates instantly. But JUCE's Linux VST3 wrapper services a plug-in's
  messages from a background thread until the host registers its run loop — for the plug-in's
  whole life if the host never does — and a few hosts activate plug-ins off their main thread
  outright. In those, activation wrote the reported latency on the host's thread at the same time
  as the plug-in's own 20 Hz latency timer could be writing it on the message thread, and that
  timer could read the freshly prepared oversampling latencies mid-rewrite — two data races
  ThreadSanitizer reports on the previous build, with a reachable ending in which the host is left
  holding the older number and nothing pending corrects it. Activation now routes its report
  through the same request mechanism host automation already uses (delivered on the message
  thread, within one 50 ms tick when the activation was off-thread), and the engine publishes its
  latency figures atomically. Regression coverage: State test 30; `AnamorphStateTests
  --reprepare-race-probe` under ThreadSanitizer.
  Evidence: PR #134. [Verified]
- **A project that does not carry one of the Settings no longer inherits it from the project you
  had open before.** The Settings panel (Oversampling, UI Scale, Scope Persistence, Show Meters,
  Tooltips, UI Animations) is stored with the project. If a project file was missing one of those
  — hand-edited, truncated, or written by a build that did not have that setting yet — opening it
  left that one setting at whatever the *previous* project had set, because a host reuses one
  plug-in instance across projects. The rest of the project loaded correctly, so the wrong value
  was easy to miss, and it was written into the file on the next save. Each missing setting now
  comes back at its documented default; a project that does carry the setting still restores it
  exactly as before. Regression coverage: State test 29.
  Evidence: PR #134. [Verified]
- **A damaged setting in a very old project can no longer leave a Settings menu blank or spread
  garbage into the project on the next save.** Projects saved by versions before 0.8.4 carry
  Oversampling, UI Scale and Scope Persistence as ordinary parameters that are converted on load.
  A damaged value there — "nan", "inf", a number too large to store, or text that is not a number
  at all — went through a conversion with no defined result: on Intel it became an impossible
  menu id (−2147483647) that the Oversampling or UI Scale menu could not display and that was
  then written back into the project, while on Apple Silicon the same file produced different
  values, and Scope Persistence accepted NaN or infinity outright. Such values now resolve to the
  setting's default (a number outside the menu's range lands on the nearest valid choice), on
  every platform, and a valid old project converts exactly as before.
  Regression coverage: State test 28.
  Evidence: PR #134. [Verified]
- **A damaged project file can no longer relabel the sound you already had.** A project file that
  claimed to be an Anamorph session but carried no sound data at all — truncated, hand-edited, or
  written by a future version — restored nothing, yet the plug-in still took the file's preset name,
  its highlighted preset row, its modified-marker and its Settings. The result described a session
  that had never loaded, over a sound that had not changed. Such a file is now ignored completely,
  which is what the plug-in already did for a file it did not recognise at all.
  Regression coverage: State test 27.
  Evidence: PR #134. [Verified]
- **Loading an old session no longer leaves the wrong sound in the A and B slots.** When a session
  that carries no A/B data was loaded, the A and B compare slots kept whatever was already in them
  instead of the session being opened. On an instance the host had reused across projects that was
  the *previous project's* A and B sounds; on a freshly inserted instance it was the plug-in's
  opening Default. Either way the session's own sound loaded correctly and then pressing A or B
  recalled something else. Affected sessions saved by v0.2 (which predates the A/B feature) and any
  session whose A/B block is absent. Both slots and the active-slot marker now come back from the
  session that was actually loaded; a session that does carry A/B data is unaffected and still
  restores both slots as saved. Regression coverage: State test 26.
  Evidence: PR #134. [Verified]
- **Automating Drive or Widen Algorithm no longer does housekeeping on the audio thread.** When a
  host automated either control, the plug-in re-reported its latency from whichever thread moved the
  parameter — the audio thread, during playback. That report takes locks and, when the latency
  actually changes, allocates memory and writes to a pipe: all things that can cause a dropout in a
  realtime context. The report now happens on the plug-in's own message thread instead. Editing in
  the UI, loading a preset and undoing are unchanged and still update instantly; only a change
  arriving from elsewhere is handed over, and the host may learn of it up to 50 ms later.
  Regression coverage: State test 22.
  Evidence: PR #134. [Verified]
- **A knob's number readout no longer breaks Undo when you release the mouse outside the plug-in
  window — on all three platforms.** Pressing a value readout opens a host edit gesture that is
  closed on release. When the release happened over the host or the desktop, the operating system
  delivered it to nothing and the gesture stayed open — after which nothing you did became its own
  Undo step until the gesture eventually closed. The editor's stuck-press reconcile now closes the
  gesture as well as the visual press state; on macOS it also asks the operating system for the
  *real* mouse-button state instead of a cached copy that such a release never updates, so the
  reconcile is effective there too. That second half additionally closes a long-standing macOS
  annoyance present since 0.8.12: a knob could stay visually "pressed" after a release outside the
  window. Regression coverage: State tests 21 and 23 — 21 proves an unreleased press blocks Undo,
  that the reconcile restores it, that a normal press/release is unaffected, and that the sweep can
  run repeatedly without closing a gesture twice; 23 pins the macOS half.
  Evidence: PR #134. [Verified]
- **Reopening a damaged project no longer leaves the host compensating for the wrong delay.** When a
  corrupted value was rejected on load, the control was repaired but the delay already reported to
  the host was not — so the host kept aligning tracks for a setting the plug-in had discarded, until
  the next reactivation. Measured with oversampling on: the host was told 4 samples for a state that
  needs 0. Regression coverage: State test 24.
  Evidence: PR #134. [Verified]
- **A damaged value in a project or preset can no longer jam a control to the end of its range.**
  Round 0.9.6's earlier fix rejected "not a number" but not the rest of the family: text that is not
  a number at all ("abc", an empty value, "0x10") set the control to the BOTTOM of its range, and an
  infinity — written as "inf", or as an ordinary-looking number too large to store, like 1e39 — set
  it to the TOP. For Width that is a mono collapse and a hard-wide image respectively, from a file
  that looks fine. The cause was that the check ran after the value had been fitted to the control's
  range, and fitting clamps, so an infinity arrived already looking like a legitimate end-of-range
  value. Damaged values are now identified before that, on both the project and preset paths, and
  the control falls back to its default. Regression coverage: State test 19.
  Evidence: PR #134. [Verified]
- **Repairing a damaged project no longer leaves the damage in the file.** When a corrupted value
  was rejected on load, the control was repaired but the project data was not, so the next save
  wrote the bad value straight back out. This build reloaded such a file correctly anyway, so the
  symptom was invisible here — but the file stayed damaged for anything else that reads it,
  including older versions of the plug-in. The repair now reaches the saved data.
  Regression coverage: State test 20.
  Evidence: PR #134. [Verified]
- **Switching A/B or loading a preset while playback is stopped no longer dims the first moment of
  sound.** The short masking fade those switches use was left pending and then fired when playback
  resumed — long after the change it was meant to mask had already been applied silently. Measured
  on an engaged widener: the stereo image collapsed to 0.3 % of its settled width for the first
  32 ms. Regression coverage: Test 48.
  Evidence: PR #134. [Verified]
- **A damaged project or preset file can no longer leave the plug-in permanently silent.** If a
  saved value had been corrupted into "not a number" — by a hand edit, a bad transfer or a failing
  disk — the plug-in adopted it, and from that point produced **no sound at all** for the rest of
  the session, with the controls still showing plausible numbers. Worse, saving wrote the bad value
  straight back out, so reopening the project reproduced the silence. Such a value is now rejected
  on load and the affected control falls back to its default, exactly as it already did for a
  setting the file does not contain at all — on both the project-state and preset-file paths.
  Regression coverage: State test 17, which restores a poisoned session, then plays audio through
  the real plug-in and requires it to be audible.
  Evidence: PR #134. [Verified]
- **A preset file with a truncated entry no longer silently zeroes that control.** If an entry in a
  preset lost its saved number — a truncated write, a hand edit — the control was set to the
  *bottom* of its range rather than left at its default. For Width, whose range runs 0–200 % around
  a 100 % default, that collapsed the image to mono with nothing on screen to explain it. An entry
  with no value now means "not in this file", which is what a missing entry already meant, and the
  control keeps its default. Nothing the plug-in itself saves is affected — it always writes the
  value. Regression coverage: State test 18.
  Evidence: PR #134. [Verified]
- **Inserting the plug-in, or opening a project, no longer dips the sound for the first moment.**
  Every time the plug-in was activated — a fresh insert on a playing track, a project reload, a
  sample-rate or buffer-size change — the audio faded down to near-silence and back over roughly
  the first 35 ms before settling. Measured on a steady tone: the level fell to **0.4 % of normal**
  and took about six blocks to recover. The cause was internal: the engine was set up *before* it
  was told the current settings, so its first look at them registered as a settings change and
  triggered the click-free mute that a real settings change is supposed to get. It is now told the
  settings first, so it starts up already in the right state.
  Evidence: PR #134. [Verified]
- **A project reopened with non-default settings now plays at the right level from the first
  sample.** Because of the same ordering problem, controls such as Output Gain started from their
  default and slid to the saved value over the first ~20 ms — a session saved at −12 dB opened
  about 2.4× too loud for that instant. Regression coverage for both halves is in the state suite
  (State test 16), which measures the level of the first blocks against the settled level.
  Evidence: PR #134. [Verified]
- **Reopening a project no longer plays the first split-second with the wrong settings.** After
  every host re-activation (a sample-rate or buffer-size change, or rendering with a fresh
  instance), the first ~5–20 ms glided from built-in neutral values to the session's actual ones —
  a Mix 0 session opened briefly wet, a −24 dB Output Gain opened briefly hot, an inverted
  polarity ramped through positive. The engine now lands on the session's values from the very
  first sample. Regression test: a Mix 0 session must be a bit-exact null from sample 0 after
  re-activation (Test 44).
  Evidence: PR #134. [Verified]
- **A host that delivers a larger audio block than it promised can no longer crash the plug-in.**
  The engine trusted the host's declared maximum block size absolutely; a block beyond it
  overran internal buffers (a memory-corruption crash in the DAW). Oversized blocks are now
  split internally into contract-sized slices — bit-identical audio for every host that keeps
  its promise, correct audio instead of a crash for one that does not (Test 43).
  Evidence: PR #134. [Verified]
- **The correlation meter can no longer freeze for the rest of the session.** One non-finite
  sample reaching the meter through the Bypass crossfade could latch the phase/balance pointers
  permanently (the same defect class fixed for the level meters in 0.8.x, INC-004). The
  correlation meter now self-heals the way the level meters do (Test 45).
  Evidence: PR #134. [Verified]
- **Loading a very old (0.2-era) session into an already-used plug-in instance now resets the
  Settings panel.** Oversampling, UI Scale, Scope Persistence, Show Meters, Tooltips and UI
  Animations are not stored in a session that old, and the load path never touched them — so they
  silently kept the *previous* project's values. All six now reset to their defaults, matching what
  a session from any later version already did. Regression coverage: State test 4, which sets those
  values first so the check cannot pass by their never having moved.
  Evidence: PR #134. [Verified]
- **An A/B or preset switch no longer loses its remembered Level Match when the host changes sample
  rate at that instant.** The short mute that masks an A/B, preset or Undo switch left an internal
  flag set if the host re-initialised the plug-in while it was still fading — a sample-rate or
  buffer-size change landing inside those ~30 ms. From then on the remembered Level Match trim for
  that slot was silently dropped rather than applied, so the switch played at the wrong level for
  the rest of the session. Measured: an injected −6 dB trim was adopted as 0.0 dB before the fix.
  Regression coverage: Test 47.
  Evidence: PR #134. [Verified]
- **A corrupted A/B slot in a session file can no longer silently lose every setting on a later
  save.** A slot payload that parsed but was not Anamorph data corrupted the internal state
  container when applied; every save from then on wrote settings a fresh instance silently
  skipped on load. Such a payload is now discarded and the slot re-seeded, exactly like an
  unparsable one.
  Evidence: PR #134. [Verified]
- **Dragging a knob's number readout now registers with Undo and with host automation
  recording.** The vertical drag on the value box changed the parameter without opening a host
  change gesture, so it produced no Undo step and recorded outside touch/latch automation —
  a third instance of the KI-010 class, now closed for the drag path (typed entry and the
  imager's mouse wheel remain as recorded in KI-010).
  Evidence: PR #134. [Verified]

## [0.9.5] — 2026-08-30
### Changed
- **The Windows build is now compiled for AVX2 too — same sound, and the same new minimum CPU
  requirement the Linux and Intel-macOS builds already carry.** The Windows plug-in now requires an
  Intel Haswell (2013) or AMD Excavator (2015) processor or newer; on an older PC it will not load,
  and the failure looks like a crash in your DAW rather than a message, whatever the Windows
  version. The audio is **bit-identical**: on the Windows compiler, turning AVX2 on moved **none of
  the 32 scenarios** in the engine comparison harness — measured twice before the change was
  adopted, and now re-checked automatically on every build, which will refuse to ship if that ever
  stops being true. The one part of AVX2 that would change the arithmetic (fused multiply-add) is
  left off, exactly as on the other platforms. The speed benefit is the same mechanism measured on
  Linux (about a fifth off the engine's instruction count there); no Windows-specific figure exists
  because no comparable instrument runs on Windows, and none is claimed. Only Apple Silicon Macs
  now carry no processor requirement. See ADR-0032 and
  `docs/policies/COMPATIBILITY_POLICY.md` ("Runtime compatibility: the x86-64 ISA floor").
  Evidence: PR #131. [Verified]
- **Faster on every Intel and AMD machine, with the sound unchanged — and a new minimum CPU
  requirement on Linux and on the Intel half of the macOS build.** The plug-in is now compiled for
  the AVX2 instruction set instead of the 2003-era baseline it had been using, which lets the
  compiler work on twice as many numbers at a time as before. Measured reduction in
  the engine's total instruction count: **−17 %**. The audio is **bit-identical** — the same
  32-scenario engine twin dump, and the same 180-configuration sweep, agree with the previous build
  in every scenario — because fused multiply-add, the one part of AVX2 that would have changed the
  arithmetic, is explicitly disabled. **The requirement:** an Intel Haswell (2013) or AMD Excavator
  (2015) processor or newer. On an older processor the plug-in will not run, and the failure looks
  like a crash in your DAW rather than a message, because the plug-in cannot report a problem with
  instructions it never gets to execute. This affects **Linux** and the **Intel half of the macOS
  build**. Apple Silicon Macs are unchanged and carry no new requirement. *(This entry left the
  Windows build alone; Windows acquired the same requirement separately under ADR-0032 — see the
  AVX2 entry above, which is the current statement for Windows.)*
  Every Mac that can run macOS 15 already exceeds the requirement; a Mac old enough to be affected
  would be running macOS 10.13–10.15. No parameter, preset, saved-state or latency change.
  See ADR-0031 and `docs/policies/COMPATIBILITY_POLICY.md` ("Runtime compatibility: the x86-64 ISA
  floor").
  Evidence: PR #130. [Verified]
- **Lower CPU with Velvet Noise selected at high sample rates, and the sound is unchanged.** The
  change below stopped Velvet Noise rebuilding its ~45 ms decorrelation window every block by
  carrying it forward instead; this one removes the private copy of the window altogether. The
  plug-in already keeps that audio in a circular buffer, and it now reads each decorrelation tap
  straight out of it. What is left costs the same whatever the sample rate, where before it grew
  with it: at a 32-sample buffer the penalty for running at 192 kHz instead of 48 kHz drops from
  about 40 % to nothing measurable. Biggest effect at high rates and small buffers — about a third
  less work at 192 kHz with a 32-sample buffer, about an eighth at 48 kHz — and at 44.1 kHz, where
  there was least to save, it is a wash: slightly better at the default Density, and about 1 % worse
  in one corner (a 32-sample buffer with Density at maximum), which is accepted. Bit-identical
  output: the same 32-scenario dump and the
  same 180 configurations as above, plus a new self-test that checks the fast path against the
  plain one directly. No parameter, preset or latency change.
  Evidence: PR #130. [Verified]
- **Lower CPU with Velvet Noise selected — most at small buffer sizes and high sample rates, and the
  sound is unchanged.** Velvet Noise builds its decorrelation from a ~45 ms window of recent audio.
  Every processing block it was rebuilding a private copy of that whole window from scratch, and the
  cost of doing so depends on the window — not on how much audio the block actually contains. So the
  smaller your buffer, the more often you paid it; and the higher your sample rate, the bigger the
  window and the more each one cost. At 192 kHz with a 32-sample buffer that rebuild had grown to
  most of the plug-in's work. It now carries the window forward from the previous block instead of
  rebuilding it. Measured reduction in the engine's total instruction count: **−14 % at 48 kHz with a
  32-sample buffer, −32 % at 192 kHz with a 32-sample buffer, −9 % at 96 kHz / 128, −5 % at
  48 kHz / 128, −2.5 % at 48 kHz / 256**. Nothing else changes: the audio is **bit-identical**,
  proven by the 32-scenario engine twin dump and by a 180-configuration sweep across every algorithm,
  four sample rates and five buffer sizes, and the reported latency, parameters and saved state are
  untouched.
  Evidence: PR #127. [Verified]

### Fixed
- **Turning Widen down to zero now actually gives the CPU back.** Velvet Noise, Haas and the
  Chorus / Dimension-D engine each have a cheap path for when their Amount is zero, and each was
  entering it only on a freshly loaded plug-in — never after you turned the control down yourself.
  The reason is that the smooth fade to zero never quite arrives: it approaches zero by a fixed
  fraction each sample, and the step eventually becomes too small for the processor to represent, so
  the level freezes a hair above zero and stays there. The test was "is it zero"; it is now "can it
  still move", which is the question the shortcut actually depends on. In a session where you have
  ever turned an algorithm's Amount down, that gives back the work the module was stuck doing —
  most with the Chorus / Dimension-D engine, least with Velvet Noise, whose share the Velvet Noise
  change above had already absorbed nearly all of. **What changes in the audio:**
  only in the **silence region** — digital silence, and signals more than roughly 550 dB below full
  scale — the frozen remainder used to leak an inaudible trace of the delay
  line's contents: at most about 1.6e-35 of full scale, roughly −696 dB, some twenty-five orders of
  magnitude below the smallest difference this project has ever accepted as inaudible, and
  twenty-eight below the smallest step a 24-bit file can hold. That trace is now gone — silence is
  exactly silence, and near-silence passes through untouched. On any real signal the
  output is **bit-for-bit identical**, before and after — verified over 102,400 samples per module at
  every supported sample rate, and re-verified bit-for-bit at every probe level down to 400 dB below
  full scale. No parameter, preset, saved-state or latency change.
  Evidence: PR #130. [Verified]

## [0.9.4] — 2026-08-21
### Fixed
- **A tooltip no longer shows the wrong control's text after it moves.** Hover a Settings control,
  then move the pointer onto the hint box that appeared: the box steps aside as before, but its text
  could change to a *neighbouring* control's — the *UI Scale* hint becoming the *Oversampling* one —
  and stay wrong until you moved the mouse again, when it would snap back. The two halves of the
  hint disagreed with each other: the text was taken from a remembered answer to "what is the
  pointer on?", refreshed only when the mouse sends an event, while the box was placed at the
  pointer's live position, read fresh every time. Move the pointer somewhere that produces no event
  — onto the hint box, which is then taken off screen — and the remembered answer is left behind
  while the box follows the pointer, so the box lands where you are and is labelled with where you
  were. The remembered answer is now checked against the live pointer before it is used, and what
  the pointer is really on is used when it disagrees. Nothing else about how hints appear,
  disappear or move has changed.
- **Controls under the Settings, About and Save Preset panels no longer light up either, and a
  control can no longer stay lit after the pointer leaves.** Two follow-ons to the drop-down fix
  below, with the same cause and two different second halves. The panels cover the editor but are
  not menus, so the earlier fix did not reach them: a knob behind the dim still contained the
  pointer and still glowed through it, on a control that a click would only dismiss the panel. What
  counts as a covering panel is now **derived rather than listed** — a visible child of the editor
  that takes the pointer away from what is behind it — so a panel added later is covered without
  anyone remembering to add it, while the panel's *own* controls keep hovering normally and the
  Bypass dim, which covers the editor but never takes the pointer, correctly leaves the controls
  under it live. Separately, a control could stay glowing after the pointer left the plug-in window
  in one movement: the idle check that keeps a still editor quiet asked "did anything move last
  frame?", which reads the same whether a control is dark or fully lit, so it could go quiet on a
  glowing one. It now also asks whether anything is *still* lit, and the fade is made to land on
  zero instead of approaching it forever — without that second half the first would have kept the
  editor busy permanently. **The idle cost is unchanged: measured 0 passes per second with the
  pointer outside the window, before and after, including straight after a hover.** [Verified]
- **Controls under an open drop-down no longer light up as if you were pointing at them.** With a
  menu open — any of the seven drop-downs, or the preset list — moving the pointer **onto the menu**
  lit whatever sat underneath it: a knob's arc glow and pointer halo, a switch's pill, the A/B
  racetrack. The pointer was on the menu, so the highlight was claiming something untrue, and the
  control it pointed at could not be clicked anyway (a click there just dismisses the menu). The
  cause is that hover here is worked out from the pointer's **position** rather than from mouse
  enter/exit events — deliberately, since v0.6.1, because those events were unreliable and left
  highlights stuck on. A position, though, cannot tell that something has been drawn on top of it,
  so a covered control kept containing the pointer exactly as before. It is now also tested against
  the open menu, and a control the menu covers stays dark. **Nothing else changes**: the box you
  opened keeps its own highlight (the list opens flush *below* it, so the pointer really is still on
  the box), a control merely *beside* the menu is unaffected, and hover returns the instant the menu
  closes. Measured on the running editor rather than argued: with the pointer on an open list, the
  knob beneath it went from fully lit to fully dark, and with a preset library large enough to make
  the preset menu tall, so did the A/B control under that. [Verified]
- **The macOS Audio Unit is now covered by the release gate.** `Anamorph.component` — the build
  Logic Pro and GarageBand load, and the only format they load — previously shipped having passed
  **no automated format-conformance validation at all**: the gate located and validated
  `Anamorph.vst3` on all three platforms and nothing else. The macOS job now runs pluginval against
  the AU as well, at the same strictness, in both modes, three consecutive passes each. It is
  installed into `~/Library/Audio/Plug-Ins/Components/` and the AudioComponent registry refreshed
  first, because macOS resolves Audio Units through that registry and a never-installed `.component`
  can report zero plugin types however correct it is. This closes the coverage gap
  `docs/procedures/TESTING.md` recorded under "Gaps in the automated coverage". [Verified]
- **The "deterministic" half of the pluginval release gate was not deterministic.** Both validation
  scripts passed `--random-seed 0`, and pluginval reads 0 as *"generate a random seed"*
  (`Source/PluginTests.h`; `Source/CommandLine.cpp` forwards the flag only when it differs from that
  default), so the flag was equivalent to passing nothing and a failure in that mode could not be
  reproduced from its log. Measured against pluginval 1.0.4 and this plug-in: seed 0 printed a
  different `Random seed:` on every run, seed 1 printed `0x1` every time. The seed is now pinned to
  the same nonzero value in `run-pluginval.sh` and `run-pluginval.ps1`, so all three platforms
  validate against one seed. The gate still passes at strictness 10 in both modes ×3. [Verified]
- **A non-universal macOS build could have shipped labelled universal.** The packaging step printed
  `lipo -archs` output rather than checking it, and `lipo -archs` exits 0 for any valid Mach-O
  including a thin one — so an arm64-only build would have produced a green run with the evidence
  sitting unread in the log, and Intel users a plug-in that cannot load. Both slices are now
  asserted per bundle. [Verified]

### Changed
- **The Linux version is now built with Clang, and the compiler is chosen rather than inherited.**
  Until now the compiler that produced the Linux download was whichever `g++` the CI image happened
  to provide — nobody picked it, and it could change without a line in any diff. The Linux VST3 and
  Standalone are now built with the pinned Clang 22 toolchain and linked with its matching LLD,
  which is what the link-time optimisation the release build uses actually requires. The same
  toolchain builds the check that runs on each proposed change, so what gets validated is what gets
  shipped, and the stricter warning set Clang applies now covers the code on its way out the door
  rather than in a side job. **GCC has not gone away**: it remains as a second compiler that every
  change is still checked against, on the GCC 16 line, so code that only one of the two accepts
  still fails the build. **The system requirement is unchanged — Ubuntu 23.10 / Debian 13 or newer**
  (see the entry above; an interim step through GCC 16 during this release would have raised it to
  Ubuntu 24.04, and building with Clang does not). Nothing about the audio changes: same source,
  different compiler. [Verified]
- **The Linux installer and uninstaller can now be told what to do instead of asking.**
  `./install.sh --user` and `./install.sh --system` answer the question the script otherwise puts
  on screen, which is the only way to choose when there is no terminal to ask on — a piped or
  scripted run previously took the per-user default with no way to say otherwise, and the only
  non-interactive route to a system-wide install was running the whole script under `sudo`.
  `./uninstall.sh` takes the same two options. Passing both together is refused rather than
  silently resolved (they differ in destination *and* in privilege), an unrecognised option stops
  the run instead of being ignored, and `--user` is refused when the script is running as root,
  because which home directory `$HOME` names under `sudo` depends on the sudoers configuration and
  the install would land somewhere unpredictable. `-h` prints the usage. **With no options nothing
  changes**: an interactive run still asks and defaults to your own account, a run with no terminal
  still installs for the current user, and `sudo ./install.sh` still installs system-wide without
  asking. Linux only. [Verified]
- **A copy of your previous plug-in left behind by an interrupted install is now kept rather than
  deleted, and the installer will not stage into a folder it cannot vouch for.** If an install is
  stopped in the moment between setting the old version aside and putting the new one in place, the
  old one is parked in the installer's own scratch folder and `./install.sh` puts it back — that is
  the only thing that does. `./uninstall.sh` used to sweep it away as scratch; it now keeps it,
  says where it is and prints the command that restores it, and deletes it only if you ask with
  `./uninstall.sh --discard-parked`. Separately, the folder used to assemble the new version is now
  accepted only when it is a real directory owned by the account doing the install and not writable
  by anyone else — a symlink, someone else's directory or a world-writable one is refused by name
  with the paths to inspect, rather than used. An install that cannot find a folder it trusts stops
  without having changed anything. Linux only. [Verified]
- **The Linux build now states which systems it runs on, and can no longer raise that bar
  unnoticed.** The shipped Linux VST3 and Standalone record the glibc/libstdc++ versions of the
  machine they were linked on, and a distribution older than those cannot load them at all — the
  plug-in does not appear in your host, rather than appearing and misbehaving. That floor had never
  been chosen: it was whatever the CI image happened to be, and it rose retroactively when that
  image moved. Measured on the binaries this version ships: **GLIBC_2.38, GLIBCXX_3.4.31 and
  CXXABI_1.3.9, against a declared floor of Ubuntu 23.10 / Debian 13 or newer. Ubuntu 22.04 LTS
  ships glibc 2.35, so Anamorph does not load there.** Nothing about the plug-in itself changed; what changed is that the requirement
  is now written down (`docs/architecture/COMPATIBILITY_MATRIX.md` §"Linux runtime ABI floor") and
  asserted on every build against the exact stripped bytes you receive, so a future toolchain move
  that drops more systems fails the build rather than a user's DAW. Lowering the floor needs an
  older toolchain or a sysroot and is a separate decision that has not been taken; tracked as
  **KI-023**. Windows and macOS are unaffected. [Verified]
- **Building Anamorph from source now needs a C++23 compiler** (was C++17). The project compiles
  at `CMAKE_CXX_STANDARD 23` with compiler extensions still off; CMake ≥ 3.22 and the JUCE 9.0.1
  pin are unchanged, and the released binaries are unaffected — this changes the build
  requirement, nothing a user of the plug-in sees. **Anamorph's sound, reported latency,
  parameters and saved state are unchanged**, proven rather than assumed: the 32-scenario engine
  twin dump is **bit-identical** between C++17 and C++23 — every output hash and every reported
  latency equal — the 140-check DSP suite and the 894-check state suite are green under both
  libstdc++ and libc++, the parameter-registry snapshot frozen under JUCE 8.0.14 still passes
  byte-for-byte, the 27 project translation-unit **compilations** produce an identical 29-instance
  compiler warning set at both standards, and pluginval passes at strictness 10 in both modes ×3.
  (That 27/29 is the wider scope — **both** self-test targets' compile-command sets, so the eight
  shared DSP sources are measured once per target. The JUCE entry below cites 18/19, which is the
  `AnamorphStateTests` set alone; measured at C++23 that narrower set still yields exactly 18 and
  19, so the two figures are the same measurement at different scopes, not drift.)
  One source line changed in the whole migration: `src/dsp/HaasProcessor.cpp` now includes
  `<algorithm>` explicitly, because libc++ stops supplying it transitively from C++20 onward —
  caught by the macOS build, fixed with the include its two sibling DSP files already carry.
  A C++20 fallback was evaluated and not taken; the one Windows caveat (MSVC has no stable
  `/std:c++23`, so CMake requests `/std:c++latest`) is recorded in the ADR.
  Cross-link: `docs/architecture/design-decisions/ADR-0027-cxx23-language-standard.md`,
  `worklogs/CXX23_MIGRATION_v0.9.4.md`. [Verified]
- **JUCE framework 9.0.0 → 9.0.1**, pinned by the release tag's immutable commit SHA
  `e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8` — the same SHA-pin mechanism the 9.0.0 bump
  introduced, so the dependency still cannot move under a re-pointed tag (ADR-0026).
  **No project C++ source change was required and no new build dependency appears.** Neither
  breaking change upstream records for 9.0.1 reaches Anamorph: the vendored
  zlib/libjpeg/libpng/libflac C-language switch was already in force at 9.0.0 (it is documented
  in 9.0.1 retroactively) and Anamorph links no separate copy of those libraries, and the
  relocated WebBrowserComponent package is unreachable with `JUCE_WEB_BROWSER=0`.
  **Anamorph's sound, reported latency, parameters and saved state are unchanged.** Proven
  rather than assumed: the 32-scenario engine twin dump is **bit-identical** — every output hash
  and every reported latency equal under 9.0.0 and 9.0.1 — the 140-check DSP suite and the
  894-check state suite are green, the parameter-registry snapshot frozen under 8.0.14 still
  passes byte-for-byte, and pluginval passes at strictness 10 in both modes ×3. Four of the JUCE
  modules Anamorph depends on — `juce_dsp`, `juce_audio_basics`, `juce_data_structures` and
  `juce_audio_plugin_client` — contain **no code change whatsoever** between the two tags, only
  their version strings, so the DSP, the parameter/state layer and the VST3/AU/Standalone
  wrappers are untouched by the upgrade.
  What the upgrade does bring is upstream maintenance in the framework code the **editor** sits
  on. On **Linux**: a long run of queued messages can no longer starve the window-system
  callbacks (upstream's unresponsive-GUI fix), screens are enumerated even when the window
  manager publishes no `_NET_WORKAREA`, an unavailable XInput2 device list is handled instead of
  dereferenced, and the display-refresh timer is set from the measured refresh period. On
  **Windows**: Direct2D no longer leaves an unpainted seam at opaque component edges under
  fractional display scaling. On **macOS**: guards in the Metal layer renderer and in the message
  manager during shutdown, plus a new CoreAudio path in the Standalone's device layer.
  Cross-link: `docs/architecture/design-decisions/ADR-0026-juce-9.0.1-upgrade.md`,
  `worklogs/JUCE901_UPGRADE_v0.9.4.md`. [Verified]

## [0.9.3] — 2026-08-11
### Changed
- **The Widen row's two drop-downs are now equal width.** *Widen* and the *Style* / *Focus* box beside
  it were noticeably different sizes, with the join between them sitting right of the panel's centre.
  They are now the same width, the join lands on the centre line, and both boxes start a little
  further left; the *Style* and *Focus* captions move with their box. Same controls, same behaviour —
  only the proportions of that row changed. Simple and Advanced modes both.
  Evidence: PR #101. [Verified]
- **Windows installer: two folder prompts now match the labels above them.** The messages shown when
  a destination is left blank read *VST3 Plug-in* and *Standalone Application*, the same capitalisation
  as the fields themselves — the two the 0.9.2 installer title-casing pass missed.
  Evidence: PR #101. [Verified]
- **The Linux installer no longer requires root.** `./install.sh` now asks where to put Anamorph
  and **defaults to your own account** — VST3 into `~/.vst3`, the Standalone into `~/.local/bin` —
  which needs no `sudo` and no password, and touches no system directory. `~/.vst3` is the folder
  REAPER, Bitwig, Ardour and other Linux DAWs already scan, so the plug-in appears exactly as it
  did before. Press Enter to take it; choose *2* for the previous system-wide install into
  `/usr/lib/vst3` and `/usr/local/bin`, and only then are you asked for a password — for the copy
  itself, not for the whole script. Running `sudo ./install.sh` still installs system-wide without
  asking, so existing instructions keep working. If a system-wide install cannot proceed (no
  `sudo`, or a password you cannot supply) it says so and stops without having changed anything.
  `./uninstall.sh` offers the same two choices, so a per-user install is also removed without root.
  **Re-installing can no longer cost you a working plug-in**: the replacement is built first and the
  installed one is only displaced once that copy is complete, so a failure — no space, an unreadable
  download, or closing the terminal part-way — leaves what you already had, and a run stopped at the
  very last moment restores it on the next attempt. If an older **system-wide** install is still
  present when you install for your user, the installer now says so and prints the command that
  removes it, because DAWs scan both places and an update can otherwise look as if it did not apply.
  Linux only — the Windows and macOS installers are unchanged.
  Evidence: PR #102. [Verified]

### Fixed
- **The Multiband "add split" line no longer sticks while you move the mouse.** Hovering the
  Multiband spectrum (Advanced mode) shows a preview line marking where a click would add a new
  split. On a quiet track the line could stop following the pointer and hang at one spot, only
  catching up once the pointer reached something else that redraws — a Solo headphone, a split
  handle. The display was skipping frames it had judged identical to the last one, and the preview
  line's position was not among the things it looked at; it is now. Reported on macOS; the cause was
  platform-independent, so the fix applies everywhere. Nothing else about the display changed — an
  idle, settled view still stops repainting exactly as before.
  Evidence: PR #101. [Verified]
- **A click that closes a menu now only closes the menu.** Whenever a drop-down or right-click menu
  is open, clicking anywhere outside it closes it and does nothing else — it cannot also close the
  Settings panel, toggle A/B, add a Multiband split, move a control, or press a button. Previously
  that one click did two things at once, and in the Save Preset dialog it could **discard the name
  you had just typed**: right-clicking the name field opens the system text menu, and dismissing it
  also closed the dialog. This now holds for the Settings drop-downs, the Save Preset text menu and
  the preset menu alike, within the plug-in window the menu belongs to. Two residuals: **KI-018**, the
  dismissing click reaches no control but the system still counts it, so a *very* quick click straight
  afterwards on the same spot can land as a double-click (pause briefly, or move the pointer a little,
  before the next one); and **KI-020**, with two Anamorph windows open at once a click aimed at the
  *other* one can still act, because menus are managed system-wide rather than per window.
  Evidence: PR #101. [Verified]
- **An open menu no longer outlives the plug-in window.** A drop-down or right-click menu could be
  left behind as a stray always-on-top strip over the rest of the screen in three situations: the
  host **hiding** the plug-in window rather than closing it, the host **closing** it outright, and
  **switching to another application** with the pointer resting on a menu item — in that last case
  clicking the leftover even pulled the plug-in's window back in front of whatever you had switched
  to. On return the plug-in also spent its first click dismissing the leftover instead of pressing
  the control you aimed at. Any menu still open is now closed as soon as the window is hidden, closed
  or sent to the background, matching what the preset menu already did in 0.9.2. Nothing changes
  while the plug-in is in front of you. One residual, tracked as **KI-019**: on **Linux** the
  switching-application case is not covered — hiding and closing the window both are — so a menu left
  open there stays until you click it away.
  Evidence: PR #101. [Verified]
- **The right-click text menu no longer truncates its longest item**, which showed as
  *"Select ..."* instead of *"Select All"*. Menu width is now measured from the text in the menu's
  own font, so it stays correct with a different font, a different UI scale or a different platform
  renderer rather than fitting one machine.
  Evidence: PR #101. [Verified]
- **Menu items that cannot be chosen now look it.** Entries such as *Cut* and *Copy* with nothing
  selected, or *Paste* with an empty clipboard, are drawn dimmed instead of identically to the items
  you can actually pick.
  Evidence: PR #101. [Verified]
- **Turning Tooltips off now takes effect immediately.** A tooltip already on screen used to stay
  there, and moving quickly to another control could still bring up a new one — the setting only
  slowed tooltips down rather than switching them off, and the delay was bypassed entirely while a
  tooltip was showing. Off now means off: the visible one disappears at once and no new one can
  appear. Turning them back on behaves as before.
  Evidence: PR #101. [Verified]
- **macOS: re-running the installer now really re-installs.** If you had moved `Anamorph.app` out of
  `/Applications` (or dragged it to the Trash) and ran the installer again, it reported success while
  `/Applications` stayed empty — it had quietly updated the copy wherever you had put it. The same
  applied to the VST3 and the AU: a plug-in moved out of `/Library/Audio/Plug-Ins/…` was what got
  refreshed, so the DAW still found nothing at the standard path, and re-installing the same version
  over an intact copy could do nothing at all. Every selected component is now written to its
  standard location on every run, regardless of what an earlier install left behind, and an install
  that cannot put an item there reports failure instead of success. A copy you moved is left where
  you put it — delete it yourself if you don't want two.
  Evidence: PR #102; INC-012 in `docs/POSTMORTEMS.md`. [Verified (packaging configuration) /
  manual re-install matrix pending on a Mac — `docs/procedures/TESTING.md`]

## [0.9.2] — 2026-08-07
### Fixed
- **The preset drop-down no longer outlives the plug-in window — and clicking it afterwards no
  longer crashes.** With the preset menu open, closing the plug-in window (or switching to
  another plug-in) left the menu on screen; hovering it lost the custom item styling, and
  clicking any item took down the plug-in and/or the host. The menu was a free-standing
  always-on-top window owned by nothing the editor could reach, and its callback captured a raw
  pointer to the editor — so the click ran on freed memory. (The lost styling was a second,
  harmless symptom: the menu's reference to the editor's look-and-feel merely went null, and a
  null one falls back to JUCE's default.) It is now a **child of the editor**, so it cannot
  outlive it or float outside the window, and it inherits the styling through the component tree
  instead of holding a reference at all; the callback holds a `SafePointer`. The "Load Preset…"
  file-chooser callback — reachable from the same menu — got the same guard.
  Evidence: PR #100. [Verified]
- **A user preset that shares a factory preset's name is now selectable in its own right.** The
  preset list was searched by NAME and the factory block is first, so the tick in the drop-down
  always landed on the factory row — even immediately after saving a user preset over that name,
  and the ‹ › arrows stepped from the wrong row. A factory preset is now identified by an
  immutable internal id and a user preset by its file on disk, two namespaces that cannot
  collide; the menu, the top bar and the Save Preset field still show the **name**, so nothing
  looks different until the names clash. **The choice survives reopening the project**: the
  session now remembers which row was selected, per A/B slot as well. Your `.anamorph` files are
  untouched — nothing was added to them — and the sound always restores exactly as saved, whether
  or not the remembered preset is still there. If it is not (a factory preset removed by a later
  version, or a user preset you have since deleted, renamed or moved), the drop-down simply shows
  no checkmark rather than picking something else with the same name. Projects saved by an earlier
  version keep the old behaviour, which was to fall back to the name.
  Evidence: PR #100. [Verified]
- **Reopening a very old project no longer leaves the previous project's preset name on the A/B
  slots.** Sessions saved before 0.6.4 store the A and B slots as parameter values only, with no
  preset name attached. When the host reloaded such a project into a plug-in that already had a
  project open, each slot kept the *previous* project's preset name and modified-marker while
  showing the newly loaded sound. Each slot now reads as **No Preset** — no borrowed name, and no
  modified-marker either, since a slot that never recorded a preset has nothing to be modified
  from. Sound and parameter values were never affected.
  Evidence: PR #100. [Verified]

### Changed
- **The Settings control "Window Size" is now labelled "UI Scale"** (and its tooltip with it).
  Display name only: the XS…XL steps, what they scale, and the stored session value are
  unchanged — the identifier `int_uiScale` is immutable and untouched, so saved sessions recall
  exactly as before. Evidence: PR #100. [Verified]
- **The installers name their components in title case.** macOS: *VST3 Plug-in*, *AU Plug-in*,
  *Standalone Application* (was "VST3 plug-in" / "AU plug-in" / "Standalone application").
  Windows: the two destination-page labels now read *VST3 Plug-in folder* and *Standalone
  Application folder*. Wording only — what gets installed, and where, is unchanged.
  Evidence: PR #100. [Verified]

## [0.9.1] — 2026-07-30
### Changed
- **The manufacturer code is now `RTec` (was `Anmf`)** — the 4-character vendor identifier every
  RollyTech plug-in shares. It abbreviated the *product* rather than the company, which does not
  survive a product line; the second RollyTech plug-in forced the decision, and it was taken now
  because a manufacturer code only gets more expensive to change. **This is a host-facing identity
  change:** it is the AU component's manufacturer field and it feeds the VST3 class UID, so a
  session saved with any earlier build **reports Anamorph as missing** instead of loading it.
  Re-insert the plug-in and re-load your preset; saved parameter state, preset files and the
  install locations are unaffected, and the DSP is bit-identical to 0.9.0. On macOS, Logic
  re-scans the AU under the new identity and `auval` becomes `auval -v aufx Anmr RTec`.
  Recorded as **ADR-0023** (which also adds the plugin-identity carve-out to
  `COMPATIBILITY_POLICY.md` condition 2); the disruption is tracked as **KI-016**.
  Evidence: PR #97. [Verified]

## [0.9.0] — 2026-07-26
### Added
- **User-installable packages for every platform**, published alongside the flat ZIP
  archive downloads (extracting any zip shows the packaged files directly — no wrapper
  folder, no nested archive). Both install routes on every platform target the standard
  **system-wide** locations. Linux: `install.sh` / `uninstall.sh` ship inside
  `Anamorph-<version>-Linux.zip` (system-wide install to `/usr/lib/vst3` and
  `/usr/local/bin` with `sudo`; no separate tar.gz package). Windows:
  `Anamorph-<version>-Windows-Installer.exe` (Inno Setup — component page with
  *Install VST3* / *Install Standalone*, both pre-selected and at least one required;
  one destination page configures both paths, VST3 above Standalone; VST3 to
  `Common Files\VST3`, Standalone + Start-menu entry to Program Files; real uninstall
  entry in Settings › Apps; no post-install launch checkbox; not yet
  Authenticode-signed, so SmartScreen warns once — RH-PR-5). macOS:
  `Anamorph-<version>-macOS.pkg` with **component selection** (VST3 / AU / Standalone
  app choices, full install by default via Installer's Customize button) to the standard
  `/Library/Audio/Plug-Ins` and `/Applications` locations; package payloads carry no
  quarantine attribute, so the manual `xattr` step the zip needs disappears; not yet
  notarized — right-click → Open once — RH-PR-3. All installers are built in CI from the
  same validated staging directories as the zips; the installers are then moved into the
  release unmodified, and the release zips are archived from those same validated trees.
  Evidence: PR #87 (v0.9.0 release prep); PR #89 (installer/packaging rework:
  component selection, dual-path destination, system-wide installs, ZIP-only
  artifacts). [Verified]
- **An internal testing guide**: `SUPPORT.md` states what a tester may do with a build
  (evaluation only — no source-code rights, no redistribution), where reports go (the project's
  testing channel), the three checks worth doing first, and the six fields a test report must
  carry — Anamorph version *and* build number from the About screen, operating system, DAW/host,
  plug-in format, reproduction steps, and logs/screenshots where applicable. The GitHub
  **Test report — bug** form asks for exactly those plus install route, Oversampling setting and
  whether the Standalone reproduces it. `SUPPORT.md` is attached to every GitHub release as
  `Anamorph-<version>-SUPPORT.md`. It states
  plainly that Anamorph writes **no log file**, so nobody goes looking for one.
  Evidence: PR #91 (v0.9.0 release-hardening audit); PR #92 (lean packages — support/attribution
  as release-page assets); the internal-testing documentation pass. [Verified]
- **Product documents for a closed-source commercial plug-in**: `EULA.md` (an **unapproved
  draft** — not in force, presented by no installer, with every open owner/legal decision
  marked), `PRIVACY.md` (Anamorph collects nothing, transmits nothing and opens no network
  connection of its own — every disk write and the one About-screen link cited to source),
  `TRADEMARKS.md` (product/company name status, the third-party marks used descriptively, and
  the naming obligations the IJG, Xiph.Org and zlib licences impose) and the internal record
  `docs/COMMERCIAL_STATUS.md`. The product model is stated once for a general audience, in the
  README's licensing section, and otherwise only where it is operative — those documents, the
  internal testing guide and the developer records. The user-facing set stays on using the
  product: the manual and the installation guide end with a plain copyright line, and each
  package's `INSTALL.txt` carries one in its own bilingual section. Evidence: the internal-testing documentation pass. [Verified]
- **Third-party attribution accompanies every download.** `NOTICE` and
  `THIRD_PARTY_LICENSES.md` are published as version-named assets on every GitHub release,
  next to the zips and installers (the packages themselves stay lean — payload +
  `INSTALL.txt` only, and `INSTALL.txt` carries installation instructions alone, so the
  release assets are the sole carrier of the mandatory IJG acknowledgement).
  `THIRD_PARTY_LICENSES.md` is a verified
  inventory — every component is classified compiled-in vs vendored-but-not-built from the
  build graph and object symbols, which is how it caught two components (FreeType and stb,
  both inside PlutoVG) that JUCE's own `LICENSE.md` does not list. It also corrects the
  Steinberg VST 3 SDK licence on record: the SDK bundled with JUCE 9.0.0 is **MIT**, not the
  GPLv3/proprietary dual licence earlier documentation described — the VST trademark and
  plug-in distribution terms remain governed separately by Steinberg.
  Evidence: PR #91 (v0.9.0 release-hardening audit);
  `worklogs/RELEASE_HARDENING_AUDIT_v0.9.0.md`. [Verified]
- **User documentation**: a full user manual (`docs/user/USER_MANUAL.md`, also attached to
  GitHub releases) covering every panel, control and parameter, signal flow, the four
  widening algorithms, presets/A-B, workflow examples and troubleshooting; plus a
  per-platform installation guide (`docs/user/INSTALLATION.md`). `INSTALL.txt` is now
  included in the Linux and Windows zips (the macOS zip already shipped one).
  Evidence: PR #87 (v0.9.0 release prep). [Verified]

### Changed
- **JUCE framework 8.0.14 → 9.0.0**, pinned by the release tag's immutable commit SHA
  `f8f8864172464b9adf9eba6101e1f784838d1597` instead of a mutable tag name (supply-chain
  hardening; ADR-0022). Zero project C++ source changes were required. Behaviour proven
  unchanged: 32-scenario engine twin-dump bit-identical (all hashes and reported latencies
  equal under 8.0.14 and 9.0.0), 140-check DSP suite + 774-check state suite green, and the
  parameter-registry snapshot frozen under 8.0.14 passes byte-for-byte under 9.0.0.
  Source builds on Linux need one new package: `libegl-dev` (JUCE 9 creates GL contexts
  via EGL). Cross-link: `docs/architecture/design-decisions/ADR-0022-juce-9.0.0-upgrade-sha-pin.md`,
  `worklogs/JUCE9_MIGRATION_v0.8.13.md`. Evidence: PR #83 / commit `edcba14`. [Verified]

### Fixed
- **Per-push CI artifacts extract straight to the payload** — downloading `Anamorph-<OS>`
  and extracting the artifact zip shows `Anamorph.vst3`, the Standalone and `INSTALL.txt`
  directly: no nested archive, no wrapper folder. **Release downloads keep correct Unix
  permissions**: the artifact transport strips file modes from directory trees, so the
  release job restores the executable bits on the payload paths before archiving each
  validated staging tree into `Anamorph-<version>-<OS>.zip`, then fails closed unless every
  expected executable carries its mode inside the published zip. On the per-push loose-file
  route the executable bits are dropped by the transport; `INSTALL.txt` documents the
  `sh install.sh` / `chmod +x` fallbacks.
  Evidence: PR #84 / commit `42dd8ae` (permission handling; verified against CI-built
  bytes in `worklogs/release-hardening/RH_PR8_RELEASE_PIPELINE.md` §6c); PR #92 (flat
  per-push artifacts); the artifact-cleanup pass (single artifact per platform). [Verified]

### Compatibility
- **No parameter, preset, session or DSP behaviour changes in this release.** Sessions and
  presets saved with any 0.8.x build load unchanged (and this is now regression-tested —
  see Build / Release below). The engine's output and reported latency are bit-identical
  to v0.8.12 across the JUCE 9 bump. Evidence: PR #82/#83 validation records. [Verified]

### Documentation
- Post-v0.8.12 repository audit: drift corrections across ~20 developer documents, KI-013
  recorded (macOS release-outside stuck-press reconcile is inert), and the product-readiness
  roadmap that scheduled this release's packaging/user-docs work (including the newly
  identified RH-R10 third-party licence-compliance item).
  Evidence: PR #81 / commit `15c4159`; PR #86 / commits `96f2ae5`, `2a55b14`. [Verified]
- The documentation set is now grouped into **four explicitly separated classes** — user,
  internal/testing, legal/licensing and developer — with the authority rules for each in
  `docs/SOURCE_OF_TRUTH.md` and the index in `README.md`. `docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`
  gained documentation-only triggers so adding or reclassifying a document, or changing the
  product/licensing status, forces the corresponding index updates.
  Evidence: the internal-testing documentation pass. [Verified]

### Build / Release
- **Tag-triggered release pipeline** (`.github/workflows/release.yml`): pushing an annotated
  `vX.Y.Z` tag validates fail-closed (annotated tag ⇄ CMake `project VERSION` ⇄ CHANGELOG
  section), runs the full existing 3-OS build/validation matrix exactly once via
  `workflow_call`, and creates a **draft** GitHub Release with versioned assets,
  `SHA256SUMS.txt` and `RELEASE_MANIFEST.txt` (version / tag / commit / CI build number /
  run URL / hashes). Publishing the draft remains a manual maintainer action after the
  Level-5 audition. Rehearsed end-to-end before this release (Actions run 30011792515).
  Evidence: PR #84 / commit `d991e46`; rehearsal record PR #85 / commit `6ea31ba`. [Verified]
- **State-serialization & parameter-compatibility regression harness** as a blocking CI gate
  on all three platforms: the new `AnamorphStateTests` target (774 checks) freezes the
  parameter registry against a committed snapshot and regression-tests state round-trips,
  the three legacy session-format migration paths, corrupt/foreign state handling, preset
  round-trips and A/B preservation. Validation infrastructure only.
  Evidence: PR #82 / commit `d6bdb13`; `worklogs/STATE_HARNESS_v0.8.13.md`. [Verified]
- **New CI packaging artifacts** `Anamorph-Windows-installer` and `Anamorph-macOS-installer`
  carry the installers above; the Linux payload now additionally carries
  `install.sh`/`uninstall.sh`. Each platform uploads exactly one customer artifact
  (`Anamorph-<OS>`) plus its `-debug` symbols; the release job archives the release zip
  from that same tree and moves the installers in unmodified, both fail-closed.
  Evidence: PR #87 (v0.9.0 release prep); PR #89 (packaging rework); PR #92 (flat
  artifacts); the artifact-cleanup pass. [Verified]

## [0.8.12] — 2026-07-22
### Changed
- **Advanced-mode GPU/rendering cost reduced (performance Wave 6; pixel-identical, no behaviour
  change).** The per-band solo-headphone glyph in the spectral band editor no longer wraps its
  transparency layer around the whole plot: on macOS/Windows (GPU-composited) each visible headphone
  was allocating a **plot-sized offscreen framebuffer + full-plot alpha composite every frame** the
  spectrum repaints while Advanced is open and audio plays — up to ~4×/frame, all behind an ~18×15 px
  icon. The layer is now clipped to the glyph (offscreen ≈ 26×23 px, a ~200× smaller allocation) and
  skipped entirely at full opacity (the soloed band, where no seam can form). Output is byte-identical
  (the clip margin covers the earcups + anti-aliasing); idle, Simple-mode and hidden GPU cost were
  already ~0 and are unchanged. Linux (CPU render, no GL per ADR-0011) sees the same reduction as lower
  paint allocation. Everything else in the render pipeline was investigated and left unchanged (the
  spectrum cannot be made opaque pixel-identically — it nests in a translucent rounded panel). Full
  record: `worklogs/performance/WAVE6_GPU_RENDER_INVESTIGATION.md`.
  Evidence: PR #79 / commit `2c649ac` (performance Wave 6). [Verified]

### Fixed
- **A Multiband band's Width no longer changes on a bare mouse click.** Pressing on a band's
  horizontal width line used to snap the Width to the click position immediately — a click a few
  pixels off the line (grab tolerance 8 px) moved the divider and wrote the parameter with no drag.
  A press now only **begins** the interaction; the Width updates on the first mouse **drag** (the
  same press-then-drag contract the vertical crossover handles already used, and the identical
  `yToWidth` drag mapping). A click that never drags begins and ends an empty gesture — no value
  change, no automation/undo step. Drag feel, parameter mapping, snapping and every other Multiband
  interaction are unchanged. Single-line correction in `SpectrumImager::mouseDown`; full record:
  `worklogs/BANDWIDTH_DRAG_FIX_v0.8.12.md`.
  Evidence: PR #80 / commit `c0cbd05` (v0.8.12 GUI interaction fix). [Verified]
- **The Multiband Width drag is now RELATIVE, with a click-vs-drag threshold.** Grabbing a band's
  Width line and dragging now moves the value by the mouse **delta** from the grab (the line stays
  attached to the grabbed point) instead of snapping to the absolute cursor, and the value only starts
  moving once the cursor has crossed a 3 px threshold — so a click, or tiny hand jitter, never nudges
  Width. This matches the vertical crossover-handle drag contract (grab-offset + 3 px gate). Parameter
  mapping, smoothing and every other Multiband interaction are unchanged.
  Evidence: PR #80 (v0.8.12 GUI interaction refinement). [Verified]
- **Controls no longer stay stuck "pressed" when the mouse button is released outside the plugin
  window.** If a host delivers the mouse-up over the host/desktop (so JUCE never routes it to the
  editor and its cached button state stays stale-down), knobs, sliders and the Multiband drag could
  remain visually/logically held. The editor now reconciles against the **real OS button state**
  (`getCurrentModifiersRealtime()`, gated so it is queried only while a button appears held): a genuine
  release clears the stuck press glow, the value-box drag flag, the Persist-bar drag and any stuck
  Multiband gesture. Effective on **Windows and Linux** (JUCE's macOS realtime query returns the cached
  button state, so macOS behaviour is unchanged — where AppKit's mouse capture makes lost releases rare
  to begin with). Normal drag, press-feedback onset and automation are unchanged. Full record:
  `worklogs/MOUSE_RELEASE_STATE_FIX_v0.8.12.md`.
  Evidence: PR #80 (v0.8.12 GUI interaction fix). [Verified]

## [0.8.11] — 2026-07-20
### Changed
- **Per-block and settled-state CPU cost reduced further (performance Wave 5; no behaviour
  change by design).** Eight Class-A trims, all bit-exact on the 19-scenario engine
  twin-dump and green on the 140-check suite: the engine now skips re-adopting a
  bit-identical parameter snapshot (the steady-playback case — the per-block parameter
  path drops from ~250 to ~91 instructions); the Velvet Noise widener, parked at
  Amount 0 (the default state), skips its provably-dead per-sample glide/weights/stop
  bookkeeping while its presence envelope, gate and history keep tracking (re-engage
  unchanged); the settled global-Width stage hoists its per-sample smoother call; the
  meter publish path and Level-Match drop redundant per-block log10/exp/pow
  recomputations of unchanged pure functions, and silent blocks skip the LUFS
  conversion they never consumed. Session-local callgrind: default transparent state
  −4.5 %, 64-sample-block host-like state −5.5 % whole-run instructions; active
  algorithm paths unchanged (±0.2 %). Also corrects a Wave-4 measurement note: the
  recorded "2× per-sample cost at 64-sample blocks" was container CPU drift; the real
  small-buffer overhead is +10–20 %. Full record:
  `worklogs/performance/WAVE5_INVESTIGATION.md`.
  Evidence: PR #76 (performance Waves 4+5). [Verified]
- **Idle and background CPU cost reduced (performance Wave 4; no behaviour change by
  design).** Eight independent Class-A optimisations, all validated bit-exact on a
  19-scenario full-engine output twin-dump (including NaN-injection self-heal rows),
  pixel-identical on raw-pixel dumps of the affected views, and green on the DSP suite
  (now 140 checks): **(1)** the Input/Output level meter caches its static layer (panel,
  headers, bar slots, dB ruler) like the other three visualizers and became opaque —
  measured −29…−31 % per meter frame, pixel-identical; **(2)** the spectrum analyser
  converts bins to dB once per new FFT instead of on every decay tick (−92 % of the
  release-tail loop after audio stops) and **(3)** reuses its paint path storage (no
  per-paint heap growth); **(4)** the editor's 24 Hz tick re-shapes the preset name,
  polls combo hover and re-formats the Level-Match readout only when their inputs
  actually changed; **(5)** the vectorscope stops scanning the scope ring while the
  plugin window is hidden by the host (parity with the other visualizers' hidden
  gates); **(6)** the Haas widener, when selected with Amount at 0, skips its dead
  per-sample delay read + blend while keeping the delay lines recording, so re-engaging
  is seamless (regression Test 34 guards the warm history); **(7)** the defensive
  NaN/Inf scan runs a vectorized detector first and only heals when something is
  actually non-finite (bit-identical healing); **(8)** the scope and bypass ring fills
  copy in contiguous segments instead of per-sample. Session-local callgrind: default
  transparent state −4.9 % instructions, Haas-parked −12.4 %, bypass engaged −3.0 %.
  Evidence: PR #76 (performance Wave 4); worklogs/performance/WAVE4_INVESTIGATION.md. [Verified]
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

### Security
- **Build hardening (RH-PR-2, ADR-0021): shipped binaries are now stripped, with debug symbols
  retained as separate CI artifacts.** Release binaries previously shipped their full static
  symbol table (~15,000 entries — every internal DSP/processor method name readable), while no
  debug info existed on any platform, so field crashes were unsymbolizable. Now every platform
  generates full debug info (`-g`; `/Zi`+`/DEBUG` on Windows — Release PDBs exist for the first
  time), captures it as `Anamorph-<OS>-debug` artifacts (split `.debug` / PDB / dSYM), and strips
  the public binaries (Linux VST3 20% smaller; dynamic exports untouched — hosts still resolve
  `GetPluginFactory`). The customer-facing artifacts are upload-gated on the strip/packaging
  steps succeeding and self-validate (no symbol table, no `.debug`/PDB files) with all debug
  material purged from the public staging copy before any check that can abort — a partial
  packaging failure can no longer upload a symbol-bearing artifact. Additional pinned hardening:
  full RELRO + `BIND_NOW` + non-exec stack and explicit `-fstack-protector-strong` on Linux,
  dead-code section GC on all POSIX platforms, `/guard:cf` (Control Flow Guard) + explicit
  `/DYNAMICBASE /NXCOMPAT` on Windows. CI artifact and macOS ad-hoc signing failures are no
  longer swallowed (`|| true` removed; `if-no-files-found: error`), and Linux pluginval now
  validates the exact stripped bytes users receive. Behaviour-neutral by construction and by
  measurement: no optimization/numerics flag touched, full self-test suite green and a
  deterministic ~10.7 s engine twin dump is byte-identical before/after (evidence: ADR-0021;
  `worklogs/release-hardening/RH_PR2_INVESTIGATION.md`). Evidence: PR #63. [Verified]

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
  recorded as the roadmap direction in ADR-0015). **Discrete jumps** (the target stepping >
  1.5 oct between consecutive blocks — automation steps/snaps, unreachable by dragging) stay
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
  perceptual regressions (ADR-0012). Evidence: `CMakeLists.txt` `:33`; commit `41acaa7`. [Verified]
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
  editor now renders CPU-side on Linux/BSD (visually identical). Evidence: commit `c924ff8`. [Partially Verified] / code [Verified] (`src/PluginEditor.cpp:247-257`).

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
