# v0.8.11 Crossover-Divider Glide Speed — Investigation (worklog)

> Request: the slowest crossover-divider movement takes ~1.3 s end-to-end; reduce
> it toward ~0.5 s **as a minimal tuning**, without audible FM/pitch modulation,
> preserving the multi-speed anti-FM smoothing design. Numbers are session-local
> (constraint C2). **Outcome: no code change. The 1.3 s is the FM-limited 4 oct/s
> floor that ADR-0015 (Accepted) fixes as its final operating point; reaching
> ~0.5 s requires either exceeding that FM budget (audible, fails Test 29,
> conflicts with an Accepted ADR) or the linear-phase H2 architecture (adds
> reported latency). Both are AI-Agent Hard Stops → Architecture Review Gate.
> Escalated with measurements; the operating point is unchanged.**

- **Date:** 2026-07-20
- **Branch:** `claude/beautiful-sagan-JAUFI`, restarted from `main` @ `09aff87`
  (PR #77 merged; fast-forward, fresh follow-up → new PR).
- **Environment / method:** same container, gcc 13.3.0, Release `-O3 -DNDEBUG`,
  no `-march` (SSE2). Scratchpad harness `scratchpad/xbench.cpp` reproduces the
  exact per-sample smoother from `MultibandWidth.cpp`/`SoloMonitor.cpp` and the
  real `LR4Xover` allpass arithmetic (no JUCE), validated against Test 29's own
  numbers.

## 1. What is smoothed, and where

The crossover-divider handles write the `mbFreqLow/Mid/High` params
(`logFreqRange(20, 20000)`, ≈ **9.97 octaves**). Two consumers glide the *audible*
cutoff toward the param, identically (must stay in sync):

- `src/dsp/MultibandWidth.cpp` — the multiband reconstruction (per-sample glide
  `~L276-339`, constants set in `prepare` `~L18-29`).
- `src/dsp/SoloMonitor.cpp` — the band-solo monitor (`~L213-247`).

The glide is a **slew-limited smoother** (ADR-0015 final): per sample each cutoff
moves by a ~20 ms one-pole demand toward the target, clamped to a
**frequency-proportional rate cap** `R(f) = 4 · max(1, f/300) oct/s`
(`glideStep = exp2(4/sr)`, `kRateRefHz = 300`, `smoothCoeff = 1−exp(−1/(0.02·sr))`).
The "multiple movement speeds" the request refers to **is** this cap: flat 4 oct/s
below 300 Hz, growing ∝ f above (13 oct/s at 1 kHz, 160 at 12 kHz).

The **visual** divider line is not the bottleneck: during an active drag it snaps
1:1 to the pointer (`SpectrumImager` disables `dispEasing` while `busy`); the
`drawnF` ease (~105 ms) only runs for reset/preset/A-B/undo sweeps and is already
~0.5 s. The perceived ~1.3 s sluggishness is the **audio** glide catching up.

## 2. Root cause (measured)

`xbench` extreme-to-extreme settle (20 Hz ↔ 20 kHz), current `(4, 300)`:

| direction | settle |
|---|---|
| 20 Hz → 20 kHz | **1.48 s** |
| 20 kHz → 20 Hz | **1.38 s** |

Travel-time integral confirms the split: **20 → 300 Hz costs ~1.0 s at the flat
4 oct/s floor**; 300 Hz → 20 kHz costs only ~0.36 s (the cap grows with f). So
~70 % of the "1.3 s" is the sub-300-Hz crawl at the flat floor. Matches the
report exactly. On the log-frequency display (~90 px/oct) 4 oct/s ≈ 360 px/s at
any position, so a **low** crossover feels slow while a **high** one zips — the
"slowest movement mode" is precisely the flat floor.

## 3. Why the floor is 4 oct/s — the FM bound (measured)

A swept minimum-phase LR4 crossover is a phase modulator: the peak instantaneous
frequency shift at sweep rate `R` is **0.312·R Hz at any crossing** (ADR-0015
context; `dφ/dt / 2π`). `xbench` reproduces it and matches Test 29:

FM at a **150 Hz** crossing (Test-29 style, drag past a 150 Hz tone), bound = 18 cents:

| base oct/s | 6-oct flick | crawl-cross | verdict |
|---|---|---|---|
| **4 (current)** | **14.3 c** | **14.4 c** | ships; ≈ 1.25 Hz shift |
| 6 | 21.1 c | 20.3 c | **fails 18 c** |
| 8 | 28.1 c | 20.3 c | fails (matches ADR "uncapped ~8 oct/s → 28c") |
| 10 | 30.1 c | — | fails |
| 12 | 39.3 c | — | fails |

The floor's true invariant is a **constant ~1.25 Hz peak shift** (≈ the
bass-register *absolute* frequency JND), not a fixed cents value — confirmed by
sweeping the current design past several bass crossings:

| crossing | realized FM | shift |
|---|---|---|
| 80 Hz | 27.0 c | 1.26 Hz |
| 100 Hz | 21.6 c | 1.25 Hz |
| 150 Hz | 14.3 c | 1.25 Hz |
| 200 Hz | 10.7 c | 1.24 Hz |
| 300 Hz | 8.3 c | 1.44 Hz (knee) |

The cents grow toward the deep bass (already **27 c at 80 Hz today**, accepted
because it is still ~1.25 Hz). Raising the floor scales that Hz shift up
*everywhere* below 300 Hz → past the JND invariant → audible. The 18-cent Test 29
bound (measured at 150 Hz) is one checkpoint of this Hz invariant, not the limit
itself.

## 4. No safe tuning exists (2-D Pareto)

The rate law has two knobs, `base` and `kRateRefHz`. Every direction violates a
constraint:

- **↑ base** → bass FM up (fails Test 29 at ≥ ~4.8 oct/s; exceeds the 1.25 Hz JND).
- **↓ base** → everything slower (worse).
- **↓ kRateRefHz** → treble FM up — the **fref = 150 variant ADR-0015 already
  measured at −27 dBc past a 1 kHz tone and rejected** (> the −31 dBc bound).
- **↑ kRateRefHz** → the flat floor extends higher, *slowing* the 300–ref region
  (worse), improving only treble FM (not needed).

Settle vs FM near the current point is an **unfavourable trade**: the first base
that still passes Test 29 (~4.8 oct/s) only reaches ~1.31 s (~13 %) while eating
the entire FM margin and raising audible bass FM. Reaching the ~0.5 s target needs
**~12 oct/s ≈ 40 cents / 3.75 Hz** — grossly audible. The shipped `(4, 300)` is
Pareto-optimal for the FM-vs-responsiveness tradeoff. This is exactly what the
task's own priority order forbids ("no audible artifacts" > responsiveness; "a
small remaining transition time is preferable to introducing audible modulation").

## 5. Governance — this is an Accepted-ADR Hard Stop

`R(f) = 4·max(1, f/300)` is the **final decision of ADR-0015 (Accepted)**, chosen
after five measured iterations that rejected every faster continuous-glide option
(§Options / §"v0.8.10 final decision"): 1.25 oct/s (too laggy), ~8 oct/s uncapped
(+31 c, audible), 15 ms one-pole fast-track (~50 c), flat 4 oct/s (pinned normal
drags — the slow-drag regression), fref = 150 (−27 dBc spurs). Its governing trade
is stated verbatim: *"a small amount of controlled FM is preferable to obvious
interaction latency,"* recorded as **KI-012**. Its closing Consequence #3:

> *"Any future attempt to make fast tracking artifact-free must change the
> crossover class (linear-phase, H2) — a reported-latency change requiring a new
> ADR + Architecture Review."*

Therefore reaching ~0.5 s is **not a tuning**:

1. **Raise the FM budget** (base > 4 oct/s) → audible, fails Test 29, and
   **contradicts ADR-0015's Accepted operating point** → *conflict with an Accepted
   ADR* → **AI-Agent Hard Stop** (`CLAUDE.md`, `ARCHITECTURE_REVIEW_GATE.md`).
2. **Linear-phase H2 crossovers** (the only instant *and* artifact-free path,
   "Deferred, not rejected" in ADR-0015) → adds ~30–80 ms **reported latency** →
   **Hard Stop** → new ADR + Architecture Review + PDC/dry-align rework.

Both routes require human/Architecture-Review sign-off. Neither is a minimal DSP
tuning, so per the FIRST INSTRUCTION in `CLAUDE.md` the correct action is **stop
and escalate** with the evidence, not to move the operating point.

## 6. Decision

**No change to the crossover glide rate law or any smoother constant.** The
`(4 oct/s, 300 Hz)` point is at the FM-transparency edge (the 1.25 Hz bass JND)
and is ADR-0015's Accepted decision. The measured investigation is the
deliverable; the two responsiveness paths above are escalated for a product /
architecture decision (they are Hard Stops).

If the maintainer wishes to trade transparency for speed *within* the existing
architecture, the only lever is the FM budget: e.g. `base = 4.8 oct/s`
(`kRateRefHz = 360` to hold the treble asymptote) reaches ~1.31 s and still passes
Test 29 at ~17.7 c — but this raises the bass shift to ~1.5 Hz (more audible than
the shipped 1.25 Hz) for a ~13 % gain, and would need ADR-0015 amended. Not
recommended, and not applied.

## 7. Independent adversarial verification

A Workflow (`xover-glide-verify`, 4 independent lenses tasked to **refute** "no
acoustically-safe speedup exists") completed: **4/4 CONFIRM, 0 refute, 0 safe
levers.** (Unlike Waves 4/5 and the final pass, the fan-out was not lost to the
org token limit this time.) Independent findings:

- **DSP/FM math** — reproduced the shipped `LR4Xover` as a swept allpass:
  `shift = 0.312·R Hz` is **frequency-independent** (constant 1.25 Hz at R = 4
  oct/s at 30/60/150/300 Hz crossings). So the "shift ≤ 1.25 Hz at every crossing"
  bound fixes `R(f) ≤ 4 oct/s` **pointwise**, and the time-minimizing transparent
  traverse is `R(f) = 4` everywhere — **exactly the shipped flat floor**
  (T_min = log2(300/20)/4 = 0.977 s). No faster-yet-transparent shape exists.
- **Test-constraint** — two forbid-fast asserts pin both knobs: Test 29
  `cents < 18` at 150 Hz pins base (18 c near base ~4.8), and the `spur < −31 dBc`
  at 1 kHz pins ref (fref = 150 sprays −27 dBc and fails). No test requires faster
  sub-300 tracking; they only forbid excess FM.
- **Psychoacoustic** — bass pure-tone frequency DL ≈ 1 Hz (≈ constant-Hz) across
  the sub-300 span; the shipped 1.25 Hz already sits **at** ~1 steady JND
  (transparent only via ~1.5–2× transient-threshold elevation). 1.87 Hz (base = 6)
  lands inside the transient-detection band — not defensibly transparent.
- **Alternative-mechanism** — asymmetric post-release drain re-adds the identical
  `0.312·R` FM (worse, while auditioning); lowering `kFadeThresholdOct` substitutes
  the rejected chained-crossfade (fails the −31 dBc spur test); visual decoupling is
  already shipped (the divider snaps 1:1 to the pointer, only audio glides);
  shortening the one-pole tail saves ~5 % (~50 ms) and risks the −24 dBc arrival
  corner. No safe lever.

Conclusion: triply-backed (session harness + Accepted ADR-0015 + 4 adversarial
lenses) — the `(4 oct/s, 300 Hz)` operating point is a genuine Pareto edge.

## 8. Validation

- `AnamorphTests`: **140 checks, 0 failures** (unchanged — no product code
  touched). Test 29 (< 18-cent FM bound), the normal-drag tracking checks, and
  Test 32 (192 kHz snap) remain the governing guards.
- `xbench` self-validation: current `(4, 300)` reproduces Test 29's ~14 cents and
  the ADR's ~1.25 Hz / 28-cent-at-8-oct/s figures, and the ~1.3 s settle — so the
  measurements above are grounded in the shipped arithmetic.
