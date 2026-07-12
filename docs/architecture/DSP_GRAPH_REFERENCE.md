# DSP_GRAPH_REFERENCE.md

Node dependency topology of the serial DSP chain. Purpose: prevent unsafe reordering. A
node may only be moved if **Can Reorder? = Yes** and the move preserves every invariant in
`SIGNAL_FLOW.md`. Any "No" reorder requires an ADR + Architecture Review.

Evidence [Verified]: src/dsp/AnamorphEngine.cpp:493-949 (`process`).

## Topology table

| # | Node | Input depends on | Output feeds | Can Reorder? | Rationale / ADR |
|---|---|---|---|---|---|
| 0 | Input level tap | raw input | meters | No | Must tap the *raw* plugin input (pre-conditioning). :583 |
| 0b | Bypass dry capture | raw input | Bypass crossfade (7) | No | Must capture RAW input before conditioning so Bypass nulls exactly. :592-606 |
| 1 | Input conditioning | raw input | effect engine, dry, Level-Match ref | No | Defines the conditioned signal everything downstream uses. :609 |
| 1b | M/S Solo | conditioned input | effect engine | No | Must isolate Mid/Side *before* the widener (else widening fights solo). :614-617 |
| 2a | Drive + Chorus/Dim-D (in OS) | conditioned input | linear algorithm | Partial | Drive must precede the linear algorithm; OS wrap must enclose only nonlinear/mod. :631-645 |
| 2b | Haas / Velvet (linear) | post-Drive | global Width | Partial | One algorithm active at a time; runs at base rate, outside OS. :648-649 |
| 2c | Global Width (MS) | post-algorithm | Multiband | No | Width is MS side-gain on the full band before band-splitting. :652-653 |
| 2d | Multiband Width | post-Width + dry (for A(dry)) | Mix | No | Produces wet + phase-matched A(dry); solo-agnostic. A(dry) is gated off while Mix sits at exactly 1 with Match off and no crossfade in flight (Wave 2 / H4); the dry delay rings stay warm so a Mix dip re-engages phase-matched. :667-707 |
| 3 | Dry/Wet Mix | dry (delay+phase aligned) + wet | Mono Maker | No | Must follow the full effect engine; consumes A(dry). :728-759 |
| 4 | Mono Maker | mixed signal | Output stage | No | Must be POST-Mix so lows are mono at any Mix amount (0.8.0). :765-766 |
| 5 | Output stage (Gain/Match/Balance + duck) | post-Mono-Maker | Band Solo | No | Level Match measures here; gain/balance are final trims. :771-829 |
| 6 | Band Solo monitor | produced output | NaN heal / Bypass | No | POST-EVERYTHING audition; mask 0 = identity. :894 |
| 6b | NaN/Inf self-heal | produced output | Bypass | No | Last-line finite guard; only touches non-finite samples. :854-870 |
| 7 | Bypass crossfade | produced output + raw dry (0b) | meters | No | Final processed↔raw crossfade. :878-888 |
| 8 | Metering tap | final output | GUI | No | Taps the monitored (post-everything) output. :891-898 |

"Partial" = limited internal freedom (e.g. Drive before the algorithm; which oversampler
factor) but the stage's position in the chain is fixed.

## Hard reorder prohibitions (regression-defining)

- **Mono Maker must stay post-Mix.** Pre-Mix Mono Maker was the 0.7.3–0.7.5 solo/low-cut bug
  class; 0.8.0 rebuilt the chain to fix it. (Partially Verified: CHANGELOG.md [0.8.0]; commit
  `018dcdd` "rebuild the signal flow as a strict serial chain".)
- **Band Solo must stay post-everything and monitoring-only.** Weaving solo into the
  Multiband DSP caused the same bug class. (Partially Verified: CHANGELOG.md [0.8.0]; commit `018dcdd`.)
- **Oversampling must wrap only Drive + Chorus/Dim-D.** Wrapping linear stages adds needless
  latency/CPU and changes PDC. (Verified: src/dsp/AnamorphEngine.cpp:19-23.)
- **Global Width before Multiband.** Width is a full-band MS side-gain; the Multiband then
  splits and applies per-band width. (Verified: :652-667.)

## Shared crossover sub-bank

`MonoMaker`, `MultibandWidth`, and `SoloMonitor` all use the local flat-state `LR4Xover`
(src/dsp/LR4Xover.h, Wave 2 / H6 — bit-identical arithmetic to the
`juce::dsp::LinkwitzRileyFilter` it replaced, LP/HP dual output) with an identical per-sample multiplicative glide
(`glideCoeff = exp2(8/sr)`, ~8 oct/s). `MultibandWidth` and `SoloMonitor` share the identical
Nyquist-safe clamp `[20, max(1000, 0.45·sr)]` + 1.1× top-down ordering.

Evidence [Verified]: src/dsp/MonoMaker.cpp:17,33-37; MultibandWidth.cpp:55-71,113-123;
SoloMonitor.cpp:44-58.
