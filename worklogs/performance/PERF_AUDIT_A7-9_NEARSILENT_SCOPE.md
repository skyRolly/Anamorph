# A7-9 scope correction — the residual is not "silence-only": near-silent NONZERO input, measured

**Date:** 2026-08-30 · **Branch:** `claude/anamorph-ci-workflow-8iu7yk` (PR #131) · **Trigger:** a
review finding against `src/dsp/VelvetNoise.cpp:159` — *"At nonzero float fixpoints, `amountParked`,
`aNext`, and `wNext` skip representable residuals over near-silent input. Such input changes despite
the claimed silence-only scope."* · **Verdict up front: the finding REPRODUCES, and it is a
documentation-scope correction, not a code defect.** No DSP code changed in this round; Test 42 and
the corrected claims did.

## 1. What was claimed, and where the claim's proof was thin

Every A7-9 record said the Class-B residual "appears only on digital silence", resting on the
absorption argument *"`x + a*(d - x)` is bit-exactly `x` for any normal x"*. That argument is wrong
as stated: float addition absorbs the residual only while **|x| ≥ 2⁻²⁴-relative**, i.e.
`|x| >= 2^24 * |residual|`. When the dry sample `x` is *normal but far smaller than the delayed
history* `d` — near-silent input right after loud material — the stalled residual `a*(d - x)`
(`a` just under `FLT_MIN/k`) is **larger than half an ULP of x**, and the sum moves. The
"0 of 102,400 samples different on real signal" measurement was made with ±0.7 noise; nothing
between that and digital silence had ever been driven.

## 2. Method — the repository's own twin-binary A/B

One harness source, compiled twice at the shipped numeric posture
(`-O2 -march=haswell -ffp-contract=off`, FTZ+DAZ via MXCSR exactly as `ScopedNoDenormals` sets it):
once against the **pre-A7-9 sources** (`c04096d^` — the value-test gates), once against the
**current tree**. Identical stimulus proven by input-hash equality per case. Per module × rate ×
tail amplitude: 1 s engage at Amount 0.8 (loud ±0.5 sine, 997 Hz, mono), a ramp to Amount 0 long
enough to outlast every stall (320 blocks @48 kHz, 480 @192 kHz), then a **0.3 s tail at amplitude
A** with the delay histories still loud. Recorded: the last 20 loud stalled blocks + the whole tail,
raw f32, compared **byte-exactly**; the engage and ramp phases compared by FNV-1a hash.

Modules: `VelvetNoise` (48 kHz, density 0.5), `HaasProcessor` (48 kHz, 20 ms, right side),
`ChorusEngine` (48 and 192 kHz — the rate-dependent coefficient). Amplitudes:
0.5, 1e-3, 1e-6, 1e-10, 1e-20, 1e-25, 1e-27, 1e-28, 3e-29, 1e-29, 1e-30, 1e-32, 1e-35, 1e-37, and
0 (the documented silence case, as control). Harness and analysis: session scratch (`a79exp/`);
every number below is from executed runs, none predicted.

## 3. Results — FTZ posture (the shipped x86-64 configuration)

Differing samples in the 0.3 s tail (value-differences; old vs new), max |Δ|, first difference:

Value-differences (old value ≠ new value); the byte-differences beyond these are the signed-zero /
subnormal passthrough class, tabulated separately below:

| tail amplitude | Velvet 48k | Haas 48k | Chorus 48k | Chorus 192k |
|---|---|---|---|---|
| 0.5 … 1e-20 | **0** | **0** | **0** | **0** |
| 1e-25 | 2 | 1 | 2 | 3 |
| 1e-27 | 96 | 31 | 34 | 365 |
| 1e-28 | 824 | 757 | 318 | 4,865 |
| 1e-30 | 2,106 | 958 | 1,098 | 5,384 |
| 1e-35 | 2,126 | 958 | 1,106 | 5,385 |
| 0 (control) | 2,126 | 958 | 1,106 | 5,385 |
| **max \|Δ\| (any amp)** | **6.019e-36** | **6.019e-36** | **3.009e-36** | **1.204e-35** |
| max \|Δ\| on the 0 control alone | 4.912e-36 | 5.872e-36 | 2.818e-36 | 1.128e-35 |

Byte-differences exceed the value counts only at 1e-35 and below (Haas 961→7,645, Chorus 48k
1,113→14,887, Chorus 192k 5,403→60,089 from 1e-35 down to the 0 control; Velvet none): those extras
are the old path canonicalizing `-0.0` input to `+0.0` and (through DAZ) replacing subnormal
passthrough with `+0`/residual on the processed side, where the parked path preserves input bits.

- Every difference starts at the **first tail sample** and is confined to the delay-history window:
  Haas exactly its 960-sample (20 ms) delay, on the delayed channel only; Velvet ~1,069 samples
  (the tap span at density 0.5), on **both** channels (the residual rides side); Chorus ~535 /
  ~3,164 samples, both channels. The loud stalled window before the tail is **bit-identical**, and
  the engage/ramp phases hash-identical, in every case.
- Onset matches the derivation: differences require `|x| <~ 2^24 * (FLT_MIN/k) * |d - x|` —
  ≈ 2e-28 (Haas), ≈ 4e-28 (Chorus 192k) of full scale. At a 1e-25 tail only the handful of samples
  near zero crossings are that small; by 1e-28 most of the window differs.
- All deltas sit inside the documented `FLT_MIN/k` ceilings. **The programme-wide worst case is
  unchanged at Test 41's 1.563e-35** (Chorus 192 kHz, ±0.7-noise history — hotter than this
  harness's 0.5 sine). Within this harness the near-silent maxima slightly EXCEED its own silence
  control (1.204e-35 vs 1.128e-35; Velvet 6.019e-36 vs 4.912e-36): the old-vs-new delta at a
  nonzero dry sample is the residual re-quantized to that sample's coarser ULP grid, which can
  round up to ~1.5× the raw residual — silence has no such grid. Same mechanism, same ceilings.
- *The byte-difference extras are a passthrough-fidelity improvement of the parked path (input bits
  preserved), recorded because a future byte-exact instrument would see them.*

## 4. Results — no-FTZ posture (valgrind; any platform without a flush mode)

With the glide stalling at a ~7e-43 subnormal (platform-coverage F-1) instead of ~`FLT_MIN/k`, the
window shrinks by the same factor: **0 differences at every amplitude down to 1e-30**; at 1e-35,
196–4,358 samples differ with max |Δ| **7.175e-43** (subnormal-scale). Same confinement, same
mechanism, smaller residual.

## 5. Decision, per the review's decision rule

The difference is **expected and intentionally accepted** — it is the *same residual the fix was
approved to remove* ("Removing it IS what the fix means"), landing on the sample class that cannot
absorb it. The parked paths' semantics are exactly as intended (bit-exact identity; Velvet's usual
MS round-trip). **No code change.** What was wrong was the *scope wording*: "digital silence only"
is factually too narrow.

- **Classification: Class B stands.** The B-scope statement is corrected everywhere from "digital
  silence only" to the **silence-region sample class**: digital silence, plus near-silent samples
  (below ≈ 2–4e-28 of full scale under FTZ, ≈ 6e-36 without) co-occurring with a louder delay
  history. Bounded by the same per-module `FLT_MIN/k` ceilings; worst case unchanged at 1.563e-35
  ≈ −696 dBFS. Audibility is unaffected — the corrected scope is still hundreds of dB below the
  24-bit noise floor, and the direction of the change is *toward* exact passthrough.
- **Test 42** (`testA79ParkedNearSilentIdentity`, +12 checks → suite at 41 tests / 226 checks) pins
  the accepted side: after a ramp-to-stall, 1e-30 and 1e-35 tails with re-warmed loud history are
  **bit-exact identity** through the parked paths, with a stimulus self-check so the test cannot
  pass vacuously. Proven to FAIL against the pre-A7-9 sources: under FTZ both tails fire on all
  four module-rate cells; under no-FTZ the 1e-35 tail fires on all four (the 1e-30 residual is
  absorbed there — the two-amplitude design is what keeps the test discriminating in both suite
  postures). Exact comparisons throughout (`memcmp`); no epsilon was introduced anywhere.
- Corrected in place: the three DSP comment blocks, Test 41's absorption sentence, `CHANGELOG.md`
  `[0.9.5]` (unreleased), `docs/HANDOVER.md`, `docs/architecture/PERFORMANCE_BUDGET.md`,
  `docs/procedures/TESTING.md`, `docs/policies/TESTING_POLICY.md`; dated correction markers on the
  historical statements in `PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md`,
  `PERF_AUDIT_A7-2B_A7-5E_IMPLEMENTATION.md`, `A7_DECISION_PACKET.md`, and
  `PERF_AUDIT_A7-9_AVX2_IMPLEMENTATION.md`, which stay as written per the repository's
  history-preservation rule.

## 6. Why this does not reopen the numerical policy

`COMPATIBILITY_POLICY`'s within-architecture bit-identity contract compares *builds*, not the
pre/post-A7-9 *sources*; A7-9 was Accepted as Class B with a maintainer-approved budget, and this
round moves no bit of shipped output. The committed twin dump never sees the parked state
(`algoAmount` held at 0.7 — recorded at A7-9 time as the reason the defect survived Wave 4) and is
unaffected. No parameter, preset, saved-state, latency, threading or signal-order change.
