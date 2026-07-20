# Wave 5 — Runtime Performance Investigation (worklog)

> Working notes + handover record for the Wave-5 optimisation pass. Numbers are
> session-local (constraint C2). Read `WAVE4_INVESTIGATION.md` first: this wave
> answers its two open items (the fresh-eyes sweep and the small-buffer
> attribution) and implements what they justified.

- **Date:** 2026-07-20
- **Branch:** `claude/beautiful-sagan-JAUFI` (rebased onto `main` @ `912a755` —
  security-tooling PRs #65–#67/#69–#75; their code changes are test-side
  precision casts only, no `src/` change — Wave-4 commit now `432f2ba`).
- **Environment / method:** as Wave 4 (same container, gcc 13.3.0, Release
  flags; frozen-binary callgrind A/B as the citable instrument — the
  container's wall-clock drift again measured ±10–20 % between adjacent runs).
  Harness extended with two `hostlike` rows that re-send the (unchanged)
  parameter snapshot every block, exactly as the real wrapper does — the plain
  rows call `setParameters` once, so they cannot see per-block parameter-path
  costs.
- **Fresh-eyes sweep:** the Wave-4-planned Workflow fan-out ran this wave
  (per-block lens + per-sample lens complete; the GUI lens was again lost to
  the org token limit — GUI fresh-eyes remains open). The per-sample lens ran
  without its automated reviewer, so every finding below was re-verified
  against source in-line before any edit.

## Corrected Wave-4 datum

Wave 4 recorded "per-block overheads ≈ per-sample cost at 64-sample blocks"
(88.6 vs 44.9 ns/sample). Interleaved re-measurement shows **+13–20 %**, and
callgrind shows **+10 % instructions** (~4.5k Ir of fixed work per block); the
2× figure was container CPU drift between the two original runs. The fixed
per-block work attributes across `setParameters`+`updateDerived` (~250 Ir),
the meter/loudness publish tails, and per-block module entries — no single
dominant item.

## Findings → decisions (verification carried in-line on every item)

| id | finding (file:line at HEAD) | decision |
|----|------------------------------|----------|
| W5-1/E | `setParameters` re-adopts a bit-identical snapshot + re-runs `updateDerived` (2 `decibelsToGain` pow + ~25 setters + ~12 smoother targets) every block of steady playback. Verified safe: `prepare()` derives everything up-front, and the one non-pure `updateDerived` input (`loudness.getMatchGainDb()`) is re-applied fresh by `process()`'s matchTarget refresh each block. | **implement** — bitwise field-equality gate (`sameParameters`) on the Normal/continuous branch only |
| W5-B | Velvet parked loop (11 % of the transparent floor) runs provably-dead per-sample work: density trial+glide+weights compare at an absorbing fixpoint, amount glide at 0→0, stop-machine checks with `!stopping`. Env/gate + history writes must keep running (W3-9). | **implement** — parked fast loop, gated like the H5 gather (shared `densityAtFixpoint`), multiplier chain kept verbatim (no ±0 caveat) |
| W5-C | Global-Width loop calls `getNextValue()` per sample when settled; JUCE's settled `getNextValue()` returns `target` without mutation (verified in juce_SmoothedValue.h:309-312). | **implement** — settled hoist, gliding branch verbatim |
| W5-2/F | `publishAll` computes `db(peakHold)` twice per channel (publish + clip compare) — 4 redundant log10/block, always. | **implement** — locals reuse (the keyed-memo extension rejected: state for noise-level gain) |
| W5-5 | `barFall()` recomputes `expf(-blockDur/0.1)` per falling tick; pure function of blockDur. | **implement** — cache keyed on blockDur |
| W5-3 | MEASURE branch recomputes `1-exp(-blockDur/tau)`, tau ∈ {0.06, 0.9}, every audible block. | **implement** — both coeffs cached per block size; sampleRate re-keys via `prepare()` |
| W5-6 | `estBoostDb(drive, mix)` (2 pow + tanh + 2 log10) re-evaluated every block while Drive engaged; stateless pure function. | **implement** — memo keyed on the bitwise (drive, mix) pair |
| W5-7 | `dLufs`/`wLufs` (2 double log10) computed every block but consumed only in the `!silent` branch — dead work on every silent block (the DAW-idle state). | **implement** — pure code motion into the branch |
| W5-4 | 4 locked atomic RMWs per block (`duckRequest`/`matchInject`/2× `resetReq` exchanges) whose result is almost always idle; a load-first gate would defer a racing request by one block. | **REJECTED** — the exchange-consumed-on-audio-thread pattern is documented in THREADING_POLICY; changing consumption timing is (at least the appearance of) a thread-model change → Hard-Stop territory, for ~40-80 ns/block |
| W5-8 | Cache the whole `toEngine` snapshot keyed on the S10 generation counters. | **REJECTED** — the counters were not designed as a complete change-tracking contract for every `EngineParameters` input (the finder itself flagged the `reassertParameters` notifyHost=false path); a missed bump would silently freeze parameters. W5-1 takes the safe majority of the win |
| W5-A | With OS off (lat==0) the mix-path dry/align rings store-then-reload the same sample; segmented copies + direct scratch refs would drop the round-trip. | **DEFERRED** — the equivalence argument is sound, but it restructures the H9/H4/KI-1 invariant region (two branches, ~40 lines) for low-single-digit % — the highest blast radius proposed this wave. Recorded with the fix sketch for Wave 6 |
| W5-D | The four K-weighting chains are identical independent 2-biquad cascades — a 4-lane bank would SLP-vectorize (~6-12 % of the floor, the largest remaining item; per-lane IEEE arithmetic is order-identical, so bit-exactness is achievable and twin-dump-verifiable). | **DEFERRED** — a medium-size restructure of the contractual LoudnessMatch inner loop deserves its own focused pass with kernel prototyping; top Wave-6 candidate |
| — | W3-10 Width==1 gate | unchanged Class-B deferral (W5-C takes the safe part of that stage's win) |
| — | Category-C items (W3-7/8, H7) | out of scope — need the maintainer's Architecture Review |

## Implementation notes (single commit, all Class A)

- `src/dsp/AnamorphEngine.{h,cpp}`: `sameParameters` (bitwise field compare —
  NaN==NaN by bits, +0≠-0; the header comment binds future fields to the list,
  the same maintenance contract `discreteDiffers`/`copyContinuous` already
  carry) gating the Normal-branch adopt; the settled-Width hoist.
- `src/dsp/VelvetNoise.cpp`: `densityAtFixpoint` hoisted (the H5 gate's own two
  compares — warning count unchanged); parked fast loop keeping env/gate,
  history writes and the verbatim zero-multiplier write-back.
- `src/dsp/LevelMeters.h`: `publishAll` db locals; `barFall` cache.
- `src/dsp/LoudnessMatch.{h,cpp}`: estBoostDb bitwise memo; MEASURE coeff
  cache; LUFS conversion moved into its only consumer branch.

## Results

- **Twin dump**: all 19 scenarios **bit-exact** vs the frozen pre-Wave-5 binary
  (glide rows exercise the sameParameters miss path per block; the settled rows
  its hit path; velvet-1.0 guards the untouched active gather).
- **DSP suite**: 140 checks, 0 failures (no new tests — no behavioural surface
  changed; Test 34 already guards the parked-family invariants).
- **Warnings**: identical set (VelvetNoise's two pre-existing float-equal
  warnings moved with the hoisted bool; nothing added anywhere).
- **Callgrind A/B** (whole-program Ir, same inputs):

| scenario | Δ | note |
|---|---:|---|
| default-transparent | **−4.45 %** | parked-Velvet loop + width hoist + publish/loudness trims |
| default-b64 | **−5.18 %** | same + the per-block trims weigh more at 64 samples |
| default-hostlike | **−4.50 %** | adds the real wrapper's per-block setParameters |
| hostlike-b64 | **−5.50 %** | " |
| haas-parked | −0.20 % | velvet not selected there; width/publish trims only |
| velvet-1.0 | −0.16 % | untouched active path (regression guard) |

- **W5-1 isolated** (hostlike − plain, per `setParameters` call on an unchanged
  snapshot): **~250 → ~91 instructions (−63 %)**, consistent at both block
  sizes.
- W5-7's saving (silent blocks) is invisible in these noise-fed rows by
  construction; it is two double log10s per silent block, reasoned not
  measured.

## Handover notes for Wave 6

- **Top candidates, in order:** W5-D (K-weighting 4-lane bank — prototype the
  kernel first, keep only on a bit-exact twin dump + measured ≥3 % floor win),
  W5-A (lat==0 mix-ring round-trip elimination — the full fix sketch and the
  overlap caveat are in the Workflow findings journal), the GUI fresh-eyes
  sweep (never ran — two attempts lost to the org token limit).
- The transparent floor after Wave 5: LevelMeters ~22 % and LoudnessMatch ~24 %
  (both contractual; W5-D attacks the latter's inner loop without touching the
  contract), VelvetNoise parked now ~7-8 %, engine inline ~11 %.
- `bench_engine.cpp` gained `default-hostlike`/`hostlike-b64`; `bench_pre_hl`/
  `bench_post_hl` are the frozen pair for this wave's A/B.
