# TESTING.md

How to run and interpret the validation suite. Acceptance levels and the hard gate are defined in
`docs/policies/TESTING_POLICY.md`.

## Headless self-tests (DSP + state)

```bash
scripts/build.sh                 # build (produces AnamorphTests + AnamorphStateTests)
scripts/run-tests.sh             # runs BOTH console apps (fail-closed: a missing binary fails)
```

`run-tests.sh` finds `AnamorphTests` and `AnamorphStateTests` under `build/` and runs both; it
exits non-zero on any failed `check` or missing binary. Evidence [Verified]: scripts/run-tests.sh.

### What the tests cover

`tests/dsp_tests.cpp` has **53 DSP tests** using a `check(cond, "what")` harness, covering: MS
round-trip (bit-exact), transparent default, true-bypass null + latency match, Mono Maker
(post-Mix), Multiband mono-compat, Solo band selectivity + transparency, Level Match
(unity/no-ratchet/silence-freeze/mix-coupling/multiband-unity), crossover automation safety,
NaN recovery, four click-free crossfade tests (transitions, bypass, multiband enable,
solo+multiband-enable), the dry-align gate comb regression (`testDryAlignGateRecomb`,
Wave 2 / H4: a Mix dip after a gated full-wet stretch must re-engage the dry bank
phase-matched — the KI-#1 metric), the split-movement regression
(`testMultibandSplitDragNoPitchShift`, Test 29): the worst 100 ms pitch chunk of a 150 Hz tone
must stay < 18 cents (the accepted controlled-FM bound of the R(f) = 4·max(1, f/300) oct/s
slew-limited smoother, ADR-0015 final + slow-drag fix) through drags and the whole catch-up —
including an unbroken crawl-crossing scenario where the crossover passes the tone (~14 cents
measured; the pre-0.8.10 uncapped ~8 oct/s glide measures ~28 and the interim bare one-pole
tracker ~50, both fail) — the max spectral spur around a 1 kHz tone during a 60 Hz-cadence drag
must stay below −31 dBc (measures −41.3; the interim chained bank crossfades measure −28.5 dBc
and the rejected fref=150 cap variant −27, both fail), a discrete 4-octave target step must land
within ~200 ms via the bank crossfade, a RELEASED 6-octave flick must land by plain gliding well
under a second, and a NORMAL-SPEED drag (150 Hz → 12 kHz over 0.95 s at a 60 Hz cadence,
~600 px/s on the real display) must have its audible band edge AT the target 0.1–0.35 s after
release on both paths (the flat 4 oct/s cap of the slow-drag regression measures 0.47 of full
level on the solo path and 0.60 of the width-0 leak on the multiband path — both fail), all
click-free — on both the Multiband and Solo-monitor paths; and the forced-duck dry-fill gain regression (`testDryFillRespectsOutputGain`, Test 30):
with Output Gain at −24 dB an undo/redo-style Mix toggle must not spike beyond 2× the steady
output (the unscaled raw-level fill measures 15.8× and fails) while still filling the dip; and
the forced-swap-during-fade-out regression (`testForcedSwapDuringOrdinaryFadeOut`, Test 31): a
forced bulk swap landing while an ordinary discrete duck is still fading OUT must keep forced
semantics — stale delay-line audio must not replay after the silent bottom (the pre-fix engine,
which dropped the consumed forced request in that window, measures a 0.494-peak Haas-tail replay
against silent input and fails) — while the upgrade stays click-free and the duck still bottoms
at silence; and the high-sample-rate terminal-snap regression (`testHighRateCrossoverSnap`,
Test 32): a moved crossover must land **bitwise-exactly** on its target and let the solo
monitor's settled fast path go cold, at 44.1/48/96/192 kHz, through targets inside the measured
192 kHz float-stall zones (just above the binade edges ≥ 2048 Hz) including the worst one
(16.6 kHz) — the pre-fix glide, whose one-pole add stalls below `ulp(f)/2` while the gap is
still above the terminal-snap eps, rests 0.4688/0.9375/1.8750/3.75 Hz short at 192 kHz, never
goes cold, and fails, while the normal-rate passes double as the unchanged-behavior guard; and
the solo-monitor cold-through-drag regression (`testSoloColdThroughDrag`, Test 33, Wave 3): with
NOTHING soloed, dragging the splits at UI cadence must leave the monitor's settled fast path
engaged — the bank stays cold, the output buffer is **bit-untouched** on every block — and
re-engaging a solo must snap the cutoffs to the freshest drag targets under the engage crossfade
(the pre-Wave-3 gains+cutoffs gate wakes the bank on the first target move and glides instead of
snapping, failing both the stayed-cold and freshest-snap checks); and the parked-Haas
warm-history regression (`testHaasParkedWarmHistory`, Test 34, Wave 4): with Haas selected and
Amount settled at exactly 0 (under FTZ, as on the real audio thread) every block must pass
through **bit-untouched**, re-engaging on silent input must play back audio recorded WHILE
parked (the delay lines must keep recording through the parked fast path — this fails if a
future change stops the parked ring writes), and re-parking must return to bit-transparency
once the wet glide drains; and three feature-coverage tests added after a 2026-08-18
line/branch-coverage audit found these shipped stages had **zero** executions in either suite:
mono-sum input conditioning (`testMonoSumInputConditioning`, Test 35: a pure-side tone is
silenced, a mono tone passes at level with no side content, and mono-sum-off preserves the side
control), M/S input solo (`testMsSoloInputIsolation`, Test 36: Mid solo passes mono / rejects
side, Side solo passes side — and, the documented feedback-#15 property, Side solo on mono
content stays silent even at full Amount because the solo runs BEFORE the widener), and the
Level-Match injection consume paths (`testMatchInjectRestore`, Test 37, feedback #16/#23: both
the un-ducked defensive consume and the forced-duck silent-bottom consume adopt the injected
per-A/B-slot trim as a SEED — measured ≤ −4 dB displayed from a −6 dB injection — after which
MEASURE re-converges as the design intends, with no level slam); and the audio-path allocation
guard (`testProcessIsAllocationFree`, Test 38, ADR-0029): `tests/AllocationGuard.h` replaces
`operator new`/`delete` and interposes the malloc family, arms the counters **only** around
`process()` (allocation in `prepare()` is required by policy), and asserts zero across the same
algorithm × oversampling × M/S matrix — 3,840 armed calls. It is the tier that reaches **MSVC**,
where RealtimeSanitizer does not run, since `operator new` replacement is standard C++. The test
**self-checks its counters first** and discloses any half that is not live: the malloc half is
compiled out under ASan (an executable-defined `malloc` fights ASan's allocator) and the whole
guard is compiled out for the valgrind build (`-DANAMORPH_NO_ALLOC_GUARD` — memcheck reports
`Mismatched free() / delete []` when `operator new` hands back `std::malloc` memory), each with a
`::warning::` rather than a silent pass. Under RealtimeSanitizer the whole guard is compiled out
too, and there it is a correctness requirement rather than a convenience — its interposers would
shadow RTSan's own and blind that lane (ADR-0029 §7). That stand-down is detected by
`__has_feature(realtime_sanitizer)` and **cross-checked from outside the compiler**: the
`realtime` job also passes `-DANAMORPH_RTSAN_LANE=1`, and the header `#error`s if the lane is
declared while the guard is still live, so a renamed or removed feature name fails the build
instead of silently hollowing out the lane.

The newest DSP test is the **A7-9 near-silent parked-identity guard**
(`testA79ParkedNearSilentIdentity`, Test 42). A 2026-08-30 review pass measured that the pre-A7-9
stalled paths moved **near-silent NONZERO** input too — the absorption `x + residual == x` needs
`|x| >= 2^24 * |residual|`, and tails at 1e-25…1e-37 of full scale with warm loud history differ
from the parked paths by up to 1.204e-35 inside the delay-history window
(`worklogs/performance/PERF_AUDIT_A7-9_NEARSILENT_SCOPE.md`). The test drives one instance to the
stall the way a user does, re-warms the history, and asserts the parked output is **bit-exact
identity** (`memcmp`) on 1e-30 and 1e-35 tails — two amplitudes because the discriminating window is
posture-dependent (FTZ: both fire against the pre-fix sources; `ANAMORPH_TESTS_NO_FTZ`: the 1e-35
tail fires) — plus a stimulus self-check so it cannot pass vacuously. Twelve checks, proven to fail
against the pre-A7-9 sources on all four module/rate cases.

The test before it is the **A7-9 parked-path liveness gate**
(`testA79ParkedPathsReachableAfterStall`, Test 41). It answers a question this suite could not ask
for two waves: *is a fast path ever actually reached?* `VelvetNoise`, `HaasProcessor` and
`ChorusEngine` each carry a cheap Amount-0 path, and each was gated on the wet glide reaching
**exactly** 0 — which under FTZ it never does, because with a 0 target the update is `a -= k*a` and
the DECREMENT underflows before `a` does, stalling the glide just under `FLT_MIN/k`. Every one of
those paths was therefore dead after a user turned Amount down, and nothing observed it, because on
ordinary real signal `x + 1e-35*(d - x)` is bit-exactly `x` (Test 42 above covers the near-silent
class where that absorption fails).

The oracle is **a second instance of the same module**. `S` is driven the way a user drives it —
engaged, then turned down and left to stall. `P` sees the identical input with Amount at 0 from
`prepare()`, so it is genuinely parked. All three modules record the **input** in their delay lines
rather than their own output, so the two rings hold identical history and any difference between the
two outputs is the residual and nothing else. Three checks per case, each a different claim: real
signal must be **exactly** equal (the "A7-9 changed no audible bit" guard, true before and after);
digital silence must stay within the derived `FLT_MIN/k` stall ceiling; and digital silence must be
**exactly 0** — which is the gate, and which **fails on all four cases against the pre-A7-9
sources**. `ChorusEngine` is run at 48 kHz *and* 192 kHz because its smoothing coefficient is the
only rate-dependent one of the three, so 192 kHz is where the worst case lives and it is asserted
rather than extrapolated. Under `ANAMORPH_TESTS_NO_FTZ` the stall still
occurs — at a ~7e-43 subnormal rather than ~`FLT_MIN/k` (an earlier version of this paragraph and of
the test's own comment claimed the glide walks to a true zero there; both were corrected against
measurement — platform-coverage audit F-2) — the fixpoint gate parks on it, and the exact-zero check
would still fire against the pre-fix sources. No discrimination is lost without FTZ; only the stall
value moves.

The DSP test before it is the **Velvet gather/per-sample path-equivalence oracle**
(`testVelvetGatherEqualsPerSampleLoop`, Test 40, A7-2T). It exists because the guard described next
is a RELATIVE one: Test 39 compares the build under test against itself at different block lengths,
so its oracle cannot see a defect that is a pure function of the sample stream -- a gather whose
taps all read one sample too deep gives the same wrong answer at every block length. Test 40 supplies
the missing ABSOLUTE reference, and needs no product change to do it: the module already contains two
implementations of the same arithmetic, and the gather's eligibility gate ends with
`numSamples <= (int) accum.size()` while `accum` is sized from `prepare()`'s `maxBlockSize` alone --
so an instance prepared for a SMALLER block runs the per-sample loop over the same audio, with an
identical ring, tap set, weights and coefficients (all derived from the sample rate and seed, never
from the block size). The two must be **bit-identical**. Swept at **44.1 / 48 / 96 / 192 kHz** over
block sizes **32 / 128 / 512 / 4096** plus a **density-1.0** pass: 4096 exceeds `decorrSamps` at 44.1
and 48 kHz, so every tap splits into a ring run plus a same-block tail -- a regime Test 39 cannot
reach, its largest block being 512 -- and density 1.0 activates all 64 taps, where the default 0.5
activates only the shallow 32. It **proves itself live** on a seeded one-sample tap-delay error,
which fails 20 of its 20 equivalence checks at sample 3 of block 0. The same seed is caught by Test 39
too, but through that test's SCHEDULE rather than its oracle -- at block 215, its transport stop;
with the stop removed, at block 247, its moving density; with every path crossing removed, not at all.
**This test is the gate for A7-2**: the ring-gather rewrite is bit-identical when right and silently
wrong-by-a-constant-delay when not, and it must not land before this is green.

The DSP test before *that* is the **Velvet block-length invariance guard**
(`testVelvetBlockLengthInvariance`, Test 39, A7-1 / 0.9.5; renamed under A7-2B). It was written when
`VelvetNoise` carried its H5 linear history image ACROSS blocks -- slid forward rather than
re-gathered -- which made the image cross-block state, correct only while every path that did not
maintain it invalidated it. **A7-2B deleted the image and the slide**, so that state no longer
exists; the test is kept unchanged because its assertion is about the module's contract rather than
that mechanism, and it now guards the ring split's block-anchored arithmetic instead. The test
drives the module through one fixed schedule (engage, park,
re-engage, transport stop, moving density) at **44.1 / 48 / 96 / 192 kHz**, once in 512-sample
blocks, once in 32-sample blocks and once through a **cycle of mixed sizes** (32, 128, 64, 256, 32 --
summing to 512, so the events still land on a block boundary and every neighbouring pair differs),
and requires all three to be **bit-identical**: every piece of state here advances per SAMPLE, so the
output is a function of the sample stream alone, and any per-block bookkeeping that is wrong BY THE
BLOCK LENGTH -- a stale slide offset then, a mis-split ring run now -- perturbs the runs differently
and cannot survive the comparison. The mixed cycle is what
exercises the slide arithmetic's real subject: `linHistSlide` carries the JUST-PROCESSED block's
length, so a run whose blocks never change size could be correct with the offset confused for a
constant. The comparison is made on BITS rather than with `==` -- `-Wfloat-equal` is at zero in the
Clang baseline, and a float `==` is the wrong predicate for a bit-identity claim anyway, calling +0
and -0 equal (which this module's own signed-zero algebra cares about) and NaN unequal to itself. The same test passes unchanged against the pre-0.9.5 engine, so it asserts a
contract the module already had rather than one invented for the change. It **proves itself live**
three ways: the engaged stretch must really decorrelate; the transport stop must really flush the
wet (measured 15.4-25.2 % of the engaged figure with the stop, 90.6-128.9 % with the stop event
removed, so the 50 % bound sits between two measured populations); and both defect classes were
seeded and caught -- a wrong slide fails at sample 32, a missing invalidation at the stop block.
`worklogs/performance/PERF_AUDIT_v0.9.5_IMPLEMENTATION.md` §2.2.

The newest DSP test is the **Oversampling → Off handoff guard**
(`testOversamplingOffHandoffKeepsProcessing`, Test 54, ADR-0035 points 8–9, v0.9.7). It pins that
switching Oversampling from 2×, 4× or 8× **to Off** does not take the processing with it.

**Why it exists, and why Tests 52 and 53 could not have caught it.** The path crossfade `osBlend`
is a mechanism for a LIVE flip of `osActiveFor` — a Drive move or an Algorithm change, which is
exactly what Test 53 covers. A **factor** change is not that: it ducks, and it moves the reported
latency, so the two paths are not sample-aligned and must not be mixed at all. The blend was
nevertheless left in flight across that duck bottom, and in the `→ Off` direction the path it was
still weighting did not exist — `currentOversampler()` is null for Off, so the wrapped buffer was
never computed and the mix ran toward an `osPathScratch` holding the raw input. For the 12 ms of
the ramp the Drive stage and the modulation algorithms were simply absent, at full level into the
wideners' delay lines. Test 52 never leaves a factor selected across a switch and Test 53 never
changes the factor, so neither gesture reaches this bottom.

**The control is a factor→factor switch** (2× → 4×), not an Oversampling-Off run: it opens the
identical duck, moves the reported latency the same way and performs the same oversampler, chorus
and stand-in-ring resets, and differs in exactly one respect — the wrap runs on both sides of it,
so the crossfade has nothing to hand over. Every threshold is therefore calibrated against a
measurement rather than a constant, and the control's own dip (the chorus is reset at every duck
bottom by design) is not mistaken for a defect.

**The observable is the drive's third harmonic**, H3/H1 by Hann-windowed Goertzel on a 1 kHz mono
probe — a ratio of two bins, so the duck's fade, which is a large time-varying gain sitting on top
of everything in this window, cancels out of it. A waveform-difference or a bare level metric is
dominated by that envelope exactly where the defect lives. The drive stage and the mod algorithms
share the single wrapped buffer and stand or fall together, which is why one probe answers for
both; the second scenario is Chorus **with** drive, so the mod path really is engaged. Modulation
cannot be read directly during the handoff — the chorus is reset at the duck bottom in every run,
so its side energy is 0.000 there on the control as well as on the legs.

**32 checks; 12 fail against the pre-change engine and 0 after.** Measured at the duck bottom:
H3/H1 0.103 before against 0.289 after and a 0.288 control, recovering over exactly the 12 ms of
the blend; output level 0.014 before against 0.022 after and 0.022 on the control. A companion
probe, `AnamorphTests --os-off-probe`, prints the H3/H1 and RMS traces for all seven switch
directions — out of the wrap, into it, and between two factors — and asserts nothing.

Before it, the **oversampling path-swap guard**
(`testDriveCrossingIsSeamlessWithOversampling`, Test 53, ADR-0035, v0.9.7). It pins that crossing the
Drive threshold with a factor selected is **indistinguishable from crossing it with Oversampling
Off** — 24 combinations of {2×, 4×, 8×} × {Haas, Velvet} × {0 → 6 dB, 6 → 0 dB} × {instantaneous
step, 300 ms knob sweep}, each against its own Oversampling-Off control.

**Why it exists, and why Test 52 could not have caught it.** Engaging or disengaging the wrap swaps
the nonlinear region between two paths, and the click-free duck that used to cover the swap **cannot
cover it**: the duck's gain is applied at the output stage, downstream of Haas (12–35 ms) and Velvet
(~21 ms). The discontinuity entered their delay lines at full level and re-emerged one widener-delay
later, with the ~28 ms fade-in over. Test 52 leg E measures the same gesture and passes throughout,
because it reads BLOCK RMS and a few-sample discontinuity 19–28 ms downstream does not move a block's
RMS. **Nothing in the suite inspected this transition at sample resolution**, which is why the defect
survived the round that fixed the latency. Measured on the pre-change engine as a multiple of the
settled sample-to-sample step, arriving at duck bottom + the widener's own delay: Haas 2.62× (0 → 6)
and 1.16× (6 → 0), Velvet 5.77× and 2.42×, against Oversampling-Off controls of 2.00 / 0.96 / 1.89 /
1.00 — and the arrival offset does not move with the factor, which is what identifies the widener's
delay line rather than the latency as the carrier.

**The control is Oversampling Off**, for the same reason Test 52 leg E's is: the same knob move with
no factor selected engages no path swap, so whatever it measures is the Drive change itself, and
requiring the oversampled runs to match it asks the only question worth asking — can you tell from
the signal that the wrap was switched? **Both gestures**, because they fail differently: the
instantaneous step is what automation and preset recall deliver, the sweep is what a knob delivers
and is what was reported, and the step additionally caught a second defect the sweep does not — the
drive envelope advanced `factor` times faster inside the wrap, so the two paths diverged mid-crossfade
by 2.0× / 4.0× / 7.3× at 2×/4×/8×, scaling with the factor.

**56 checks; 26 fail against the pre-change engine and 0 after.** A companion probe,
`AnamorphTests --forced-swap-probe`, prints what an A/B swap, preset recall or undo actually does to
the level, the stereo image and the sample continuity for seven swap classes; it asserts nothing and
is the record behind the seamlessness investigation.

Before it, the **oversampling latency-stability guard**
(`testOversamplingLatencyIsFactorOnly`, Test 52, ADR-0034, v0.9.7). It pins that the latency reported
to the host is a function of the **Oversampling factor alone** — the fix for a reported host-graph
restart on an ordinary Drive or Algorithm move — and it is built so that the two wrong fixes fail it.

**Four legs, each rejecting a different wrong answer, plus a fifth that catches a click the change
also removes.** *Leg A* walks the whole
{factor}×{algorithm}×{drive} grid (80 combinations) and requires every cell of a factor to predict
that factor's number; its non-vacuity check requires the factors not all to report the same number
(they do not: 4, 6, 6 at 2×, 4×, 8×). *Leg A2* is the reported gesture — a live Drive sweep from 6 dB
to 0 across the engagement threshold, 400 blocks with the duck running inside it — and requires the
reported number never to move, which is a stronger statement than leg A's endpoints. *Leg B* is the
state the suite had never covered at all: a factor **selected with the wrap skipped**, where the
delay comes from the stand-in ring; it measures the impulse through the bypass path and requires the
peak at exactly the reported sample, so a build that reported a latency the chain did not have would
fail. *Leg C* keeps the CPU saving honest: twin instances on identical noise, one with oversampling
Off and one with the factor selected at Drive 0, and the second must be the first **delayed by
exactly `lat`, bit for bit**. A build that made the number constant by simply running the
oversampler all the time passes A, A2 and B and fails C — measured on exactly that counterfactual at
**1.31–1.57 absolute** on a ±1 stream, because a half-band IIR round trip is not an integer shift.

*Leg D* is the fifth, and it was found by auditing the change rather than by the report: in true
bypass the output is the raw input read from a ring at `-lat`, and the Bypass crossfade is applied
AFTER the switch duck's gain — so while fully bypassed the duck attenuates nothing and a `lat` that
moved on the Drive threshold jumped the read position by 4–6 samples at full level. It sweeps the
threshold crossing while bypassed and requires the worst sample-to-sample step not to exceed a smooth
signal's own bound. **Its start phase is swept over 16 offsets, and that is load-bearing**: the jump
steps the output by |x(t) − x(t+lat)|, which is near zero at a peak of the sine and maximal at a zero
crossing, so a single fixed phase measures whatever the block arithmetic lines up. The first draft
ran at one phase and **passed against the defective engine**; the same code at a different block size
measured 0.068 / 0.094. Swept, the pre-change engine measures **0.07162** (2×) and **0.09988** (4×,
8×) against a 0.01440 bound, and the post-change engine measures exactly the bound.

*Leg E* is the one that keeps the whole test honest about what was actually reported. Holding the
NUMBER still is only half of it: the crossing is still a discrete path change, so it still opens the
click-free duck, and an ordinary duck fades to **silence** — an interruption on an ordinary knob
move, invisible to every other leg here. Measured with the latency fix in place and the dry-fill
branch absent: **−52.6 / −53.3 / −53.3 dB** at 2×/4×/8× with **6.7 ms** more than 20 dB down. The leg
uses **Oversampling Off as its control** rather than a fixed dB threshold, because the same knob move
with no factor selected opens no duck at all and its shallow dip (the drive blend easing out) is the
floor any correct build must match; asserting a number would have to guess how much of the dip
belongs to the blend.

**26 checks; 12 fail against the pre-change engine and 0 after.** Note which: six are in legs A and
A2, three in leg D, three in leg E. Legs B and C pass on the pre-change build too — trivially,
because there the skipped state reports 0 and delays by 0 — so they are there to catch a bad *fix*,
not the original defect, and the test says so rather than implying twenty-six discriminating checks.
Leg C asserts BIT-exactness and its comment records why that is legitimate in its configuration and
must not be copied to one that engages a widener: `HaasProcessor` is not shift-invariant (its
fractional read coefficient depends on the absolute write index), a pre-existing property unrelated
to this change. The companion probe `AnamorphTests --os-latency-probe` prints the full matrix; on the
pre-change build its `predict` column read 0 → 4 (2×) and 0 → 6 (4×, 8×) for a Drive move of
0.005 dB → 6 dB.

Before it, the **extreme-finite balance guard**
(`testCorrelationBalanceExtremeFiniteInput`, Test 51, ER-DSP-11, round 23). It is the sibling of
Test 50 and is deliberately kept independent of it: that one owns the phase meter's `ll * rr`
**product**, this one the balance's `ll + rr` **sum**, and fixing the product did nothing for the
sum. Both accumulators stay finite, the numerator `rr - ll` cannot overflow either (it lies in
`[-ll, rr]` for non-negative operands), but the float sum leaves float past `FLT_MAX` and
`finite / +Inf` is a well-formed **0** — so a badly lopsided pair published **perfectly centred**.

**Built around the overflow edge, not around large values.** `1.8e19 / 0.2e19` is *more* lopsided
than `1.8e19 / 1.0e19` and read correctly in both builds, because its energies sum to 3.277e38 and
stay under `FLT_MAX`; the test asserts that case unchanged, which is what proves level is not the
variable. The defect legs assert the **value**, both directions — `-0.5285` and `+0.5285` against
the double reference — because `-0.0` is finite, is symmetric with `+0.0`, and is the exact wrong
answer, so neither "is it finite" nor "is it symmetric" would have caught it. **12 checks**: three
normal-range controls at three distinct values (balanced reads centred, L-louder and R-louder read
their true figures — so "always 0", "always non-zero" and "always one-sided" each fail one), a
premise leg (a perfectly correlated extreme pair must still read +1, proving `sanitize` never fired
and the accumulators are healthy), the two extreme unequal legs, an exact sign-flip symmetry check,
the balanced-extreme leg that must stay centred, the non-overflowing lopsided discriminator, an
**ER-DSP-10-intact** leg, and the Test 45 poison contract. **2 of the 12 fail against the pre-fix
build, 0 after.** Normal-range behaviour is additionally verified bit-for-bit outside the suite:
pre- and post-fix compared over 19,671,802 randomised finite-sum energy pairs spanning 1e-40 to
1e38, **zero differing bit patterns**; and scale invariance was swept across the edge itself — at a
fixed 3:1 energy ratio the true balance is −0.5 throughout, and the pre-fix build holds −0.5 up to
`s = 8.5e37` then drops to −0.0 the moment the sum stops being finite, while the fixed build holds
−0.5 across the whole sweep.

The DSP test before that is the **extreme-finite phase-meter guard**
(`testCorrelationMeterExtremeFiniteInput`, Test 50, ER-DSP-10, round 21). **It is not Test 45's
class and the two must not be merged.** There a NON-finite sample poisons an accumulator and
`publish()`'s `sanitize()` is the cure; here every value that guard can see is FINITE — the samples,
the three per-sample products and all six accumulators — so it never fires, and the overflow happens
*after* it, inside `correlation()`, where `ll * rr` is a float multiply. The test asserts that
premise rather than assuming it: `getEnergy()` (which IS `llFast + rrFast`) must read finite and
`> 1e19`, so a flushed accumulator would fail the leg that establishes the setup.

**It is built around the threshold, not around "big numbers", and that is what makes it a proof of
the mechanism.** √FLT_MAX ≈ 1.844e19, so the product overflows once `|l| > 4.295e9`. The test drives
correlated mono at **4.0e9** and at **5.0e9** — one binade apart, differing in exactly one respect —
and requires +1 from both. The pre-fix build prints `4.0e9 -> fast 1.0000 | 5.0e9 -> fast 0.0000`.
Beyond that: extreme anti-phase must read −1 rather than the −0.0 the overflow produced (−0.0 is
finite, which is why "assert the result is finite" would have been no test at all); a **scale
invariance** leg requires 0.5 and 1.0e10 to agree to 1e-6, which is the contract the overflow
actually violated; and an unequal-but-correlated extreme pair must still read +1 with a truthful L/R
balance. **Three normal-range controls** refuse the degenerate fix — ordinary correlated reads +1,
ordinary anti-phase reads −1, and a decorrelated input (alternating L-only/R-only frames) reads ~0,
which "always return +1" cannot pass — and a final leg re-asserts the Test 45 poison contract, so
the new branch cannot have rescued a genuinely non-finite sample instead of healing it.
**13 checks; 6 of them fail against the pre-fix build, 0 after.** The normal range is additionally
verified bit-for-bit outside the suite: the pre- and post-fix expressions were compared over
19,995,466 randomised finite-product triples spanning `ll`/`rr` from 1e-40 to 1e19 with **zero
differing bit patterns**.

The DSP test before that is the **restored-session settling guard**
(`testRestoredModulesDoNotGlideIn`, Test 49, ER-DSP-09, round 20). It pins the contract that
`AnamorphEngine::prepare` now states: a restored non-default session must OPEN in its own sound, not
glide into it over the first 10-100 ms. Each subject is compared against a REFERENCE of the same
module settled on the same targets, so the assertion is "these two are the same signal" rather than
a level threshold — the subject is exact (a snap assigns the target), and the whole residual belongs
to the reference, whose settled-by-silence legs approach their targets asymptotically at ~1e-8 after
200 blocks. The 1e-5 bound therefore sits four orders above the reference's own noise and four
orders below the defect, which moves the first block by ~0.2 of full scale. **Eleven checks**: a leg
per module (Haas, Velvet, Mono Maker, Chorus), each with a non-vacuity check that the module really
acts on the input so an "identical" verdict cannot be identical silence; an ENGINE leg; and a
live-smoothing control.

**The engine leg is the one that discriminates, and it exists because the first version of this test
did not.** The four module legs call `snapToTargets()` themselves, so they pass whether or not
`AnamorphEngine::prepare` calls it — verified by deleting the engine's four calls and watching the
test stay green. The engine leg drives the real `prepare` → `setParameters` → `process` path and
fails without them. It uses the **Mono Maker** deliberately: it is the one affected module with no
delay line, so a fresh instance's empty history cannot be mistaken for the glide the test is about
(and it is an ADVANCED-mode control, so the leg enables advanced mode or `toEngine` never maps it).
The **live-smoothing control** is the counterpart requirement from the same round: a parameter moved
AFTER prepare must still glide, so it asserts the first block after the move is *audibly different*
from the settled result — a fix that simply disabled smoothing fails it.

The suite
additionally carries **one state-restoration robustness guard**,
`testAbActiveClampOnCorruptState` — it drives a corrupted `<AB active="…">` blob through the same
read+clamp the processor uses (`anamorph::clampAbSlotIndex`, `src/AbSlotIndex.h`) and asserts an
out-of-range A/B index can never index `abSlot[]`/`abUndo[]` out of bounds, while valid 0/1 are
preserved. Evidence [Verified]: tests/dsp_tests.cpp (`main` registers all tests).

### State-compatibility self-tests (v0.8.13 harness)

`tests/state_tests.cpp` additionally carries a **ThreadSanitizer probe that the suite never
runs**: `AnamorphStateTests --state-thread-probe` drives host `setState`/`getState` calls from a
second thread against the editor tick's reads on the main thread — the interaction RISK-007
describes. It is gated behind that flag precisely because, if the risk is real, running it IS
undefined behaviour; it exists to be run under TSan, where the question has a mechanical answer.
Round 2 ran it and it reported four data races (`docs/FUTURE_RISKS.md` RISK-007).

A second opt-in instrument sits beside it: `AnamorphStateTests --latency-restore-probe`. It
**measures and prints** — it asserts nothing and returns 0 either way — because what it examines is
the reported latency, an `ARCHITECTURE_REVIEW_GATE.md` hard-stop category, so a probe that encoded
an expectation would be legislating one. It answers whether a restore that moves Drive or Algorithm
through the absent-PARAM path leaves the host holding a stale latency. Round 2's answer: no.
Step 0 shows a bare `setValue()` does not re-report (4 samples before and after), and step 0b shows
why that does not matter — `apvts.replaceState()` on its own takes Drive to its default and the
latency 4 → 0, because `updateParameterConnectionsToChildTrees` appends an empty `PARAM` node for
every adapter the new tree lacks, and that append reaches `setValueNotifyingHost` through the
APVTS's own `valueTreeChildAdded`. Keep the probe with the finding it refuted (ER-STATE-07, and
with it round 1's ER-STATE-01): a later JUCE bump can change that internal, and this is what would
catch it.

`AnamorphStateTests --legacy-ab-probe` is the third opt-in instrument, and the one whose printed
numbers are the evidence behind State test 26. It seeds a "previous project" with distinguishable A
and B sounds, restores a session that carries no A/B data into the SAME instance, then switches
slots and prints what comes back — measuring the contamination rather than asserting the contract,
so a later reader can see the sizes. Pre-fix it printed `slot B carries the PREVIOUS project's
sound: YES` (raw width 0.10 against a restored 0.75) and, with the previous project left active on
B, `first switch after the restore reads A: 0.90 -> CONFIRMED stale`. Kept beside the test for the
same reason as the two above: the test asserts the rule, the probe shows the magnitude.

`AnamorphStateTests --legacy-match-probe` is the fourth, and like `--latency-restore-probe` it
**measures and prints without asserting** — because what it examines was REFUTED on impact, so a
probe that encoded an expectation would be pinning a non-defect. It asks whether the per-slot
Level-Match gains (`abMatchGain[]`, never reset on any restore path and never serialized) reach the
output after a restore that carries no A/B data. Round 9's answer: the stale figure IS injected
(engine match −7.10 → −2.18 dB on the block it lands, tracking the previous project's B), but the
output level does not move — matched same-instance counterfactual, fresh-instance control and a
worst-case switch with no settle are all indistinguishable. The injection lands at the silent bottom
of the switch duck and is superseded by the live loudness measurement before the fade-in completes.
Keep the probe with the finding it refuted: it is what a later round re-measures instead of
re-deriving.

State test 27's first leg is **deterministic** since round 12, and it says exactly what it proves.
It uses a barrier the product itself provides: `AudioProcessor::setLatencySamples()` notifies its
`AudioProcessorListener`s synchronously, from inside the call, whenever the reported value changes
(pinned JUCE 9.0.1, `juce_AudioProcessor.cpp:415-436`), and the listener lock is released before
each callback — so a test listener can hold a delivery open while a real off-message thread makes
a second request *inside* it, with no test hook in production code and no timing race. A build that
clears the request flag AFTER delivering fails it (measured in round 12: `next tick -> 4, expected
0`); the pre-round-11 double-clear window — two adjacent atomics on one thread with no call
between them — is **not reachable** by any external mechanism, so that fix rests on inspection and
the leg's comment says so. The two waits are bounded polls for the processor's own 20 Hz timer
(deadline 40 periods), not sleeps standing in for synchronisation: the outcome is fixed the moment
the request is or is not in the flag. Two harness lessons from its earlier versions are kept in the
comment — comparing 0 with 0 (latency only moves with Drive when oversampling is on), and a tight
`callPendingTimersSynchronously()` loop, which fires nothing against a 20 Hz timer.

`AnamorphStateTests --legacy-settings-probe` is the fifth opt-in instrument and the evidence behind
State test 28: it feeds malformed host-hidden Settings ("nan", "inf", "1e39", "abc", "7", …) through
the real v0.2 restore and prints what `migrateFromLegacyApvts` put in the tree, what the clamped
consumers saw, and what a re-save then wrote. Pre-fix on x86-64 every non-finite value became
−2147483647 (an impossible ComboBox id, persisted on save), "2147483647" wrapped to INT_MIN, and
scopePersist passed NaN/±inf/out-of-range straight through. Round 13 extended State test 28 to the
repository's real frozen pre-0.8.4 fixture (`legacy_pre_0_8_4_view_params.xml`), mutated in place — only
the six Settings values replaced, the surrounding session (width, mix, `My Vocal`, both slots) asserted
intact on every restore — so the guard runs on the genuine legacy file and not only a synthetic shape. The DSP suite gained a sibling,
`AnamorphTests --match-inject-probe`, the engine-only half of the ER-STATE-13 question, written so
it cross-builds with nothing but AnamorphDSP and runs under `qemu-aarch64-static`.

`AnamorphStateTests --partial-settings-probe` is the sixth opt-in instrument and the evidence
behind State test 29. It asks whether a MODERN session that omits one host-hidden Setting leaves the
previous project's value in force on a reused instance, and it deliberately reports the modern and
legacy paths side by side — because the review that raised the finding located it in
`migrateFromLegacyApvts`, and the measurement put it the other way round: **6 of 6 fields inherited
on the modern path, 0 of 6 on the legacy one**. Keep the two columns; they are what stops the two
paths being confused again.

`AnamorphStateTests --reprepare-race-probe` is the seventh, and like `--state-thread-probe` it is
built to run under ThreadSanitizer: a thread that is not the message thread moves Drive and then
re-prepares the processor, 200 times over, while the main thread does only what the real message
thread would — serve the processor's own 20 Hz latency timer. On the pre-round-15 code TSan names
the two races the review predicted — `AnamorphEngine::prepare` writing `latency2` against
`predictLatency` reading it from `timerCallback`, and `AudioProcessor::setLatencySamples` reached
from both threads — and the plain build counts 3,980 of 4,000 deliveries running on the host's
thread; after the fix both are zero and TSan is silent over the probe and the whole suite. The
value-level symptom (the host left holding the older number) was **not observed** in 4,000
iterations: it needs the timer's compare-and-store to straddle the host's, and only an instance's
first prepare can change the number at all, so the probe reports the counts and asserts nothing.
State test 30 pins the invariant that closes the class deterministically instead — no delivery from
the preparing thread, the report unchanged after the join, and the tick then serving the PREPARED
value, equal to a message-thread prepare's.

`AnamorphStateTests --modern-settings-probe` is the eighth opt-in instrument, and like
`--latency-restore-probe` it **measures and asserts nothing**: what it examines is the recovery
semantics for a malformed MODERN host-hidden Setting, which no document stated when it was written
(`SERIALIZATION_REGISTRY.md` has stated them since round 18), so a probe
that encoded an expectation would be legislating one. It writes nineteen malformed values — out of
domain, non-numeric, `nan`, `inf`, `1e39`, coerced booleans — one at a time into a genuine modern
save's `ANAMORPH_INTERNAL` node, restores each, and reports the tree value, every consumer, whether
the next save persists it, and whether opening the real editor repairs it. Round 16's answer, in
full: **no crash and no undefined behaviour on any of the nineteen** (the legacy path's undefined
`(int)` conversion has no modern counterpart — `juce::var`→`int` on a string is a safe parse), and
**every DSP-facing read is clamped at source** (`oversampleIndex` through `jlimit(0,3)`,
`uiScaleIndex` through `jlimit(0,4)`), so nothing can index out of range whatever the tree holds.
What the probe does show is that the value itself is kept verbatim: **19 of 19 persist into the next
save**, eight leave an out-of-domain ComboBox id in the tree, and three leave a **non-finite** scope
persistence — `scopePersist()` being the one consumer with no clamp at its read. Opening the editor
repairs four of them (the Slider's range constrains a too-high or overflowing persistence; the
ComboBox coerces a fractional id) and leaves the rest, including `nan`. The ingress is bounded and
worth stating with the result: the modern values are written by the constructor's defaults table,
`restoreState`, the clamped `migrateFromLegacyApvts`, and the Settings widgets — all of which
produce valid values — so a malformed modern value can only arrive from a hand-edited or corrupted
file. **No production code was changed on this evidence** (round 16, ER-STATE-21): defining what a
malformed *present* value should mean is a serialization-contract decision, and the registry's
`ANAMORPH_INTERNAL` table currently states defaults for ABSENT only.

**Round 17 followed the one unclamped consumer to its end, and found a defect there.** The probe
gained a second table that models the real editor chain for `scopePersist` — Value to Slider, then
`applyScopePersist`'s `pow(v, 0.737f)`, then `Vectorscope::setPersistence`'s `jlimit`, then
`windowFrames()`'s `jmap` and its `(int)` conversion. Two of seven inputs arrive at that conversion
non-finite: `"nan"`, which travels intact because `juce::jlimit` returns its argument when neither
comparison is true; and any NEGATIVE value, which is finite in the file and becomes a NaN at the
`pow`. `(int)` of a non-finite float is undefined, the same class round 12 closed on the legacy
path. Fixed at the consumer, where the invariant is declared, and pinned by **State test 32**: a
unit leg over the real `Vectorscope`, and an end-to-end leg that restores four malformed sessions,
constructs the real editor and reads the persistence back off the scope component itself. **7 checks
fail without the guard, 0 with it.**

**Round 18 implemented the maintainer's answer to the contract question** — Policy B, repair during
restore and persist the repaired value. **State test 33** is that policy's contract: thirty-nine
cases across all six settings and every malformed class (out-of-domain, fractional id, non-numeric text,
`nan`, `inf`, `1e39`, boolean-shaped junk), each asserted three ways — the live value is the repaired
one, the malformed text is GONE from the next save, and reloading that save reads the same value
back. **Ten of the thirty-nine are valid-value controls**, which is what stops a fix that merely
resets everything to defaults from passing, and a final leg pins ABSENT as a separate rule that still takes
the documented default (ER-STATE-18). **83 checks fail against the pre-policy build, 0 after.** The
probe above now shows the repaired behaviour rather than the verbatim one.

**Round 20 took it from thirty to thirty-nine cases, for the half of the policy the first
implementation got wrong** (ER-STATE-22). Policy B was implemented with `v != 0.0` for the three
boolean settings — the C coercion rather than the field's domain — so a corrupted `-1`, `-2`, `2`
or `0.5` SWITCHED THE SETTING ON and the repair then persisted that as a genuine `true`; `0` was the
only value on the whole real line that could not. Nine cases were added (`-1`, `-2`, `0.5` and `2`
across the boolean fields) and **one existing case's expectation was corrected**: `int_metersOn` =
`"2"` was written down as resolving to `true`, which is the defect stated as an expectation rather
than caught. The boolean rows now carry **six** of the ten valid-value controls — including
`int_uiAnimations` = `"0"`, so that a fix which simply forced every boolean to its default cannot
pass. **12 of the suite's checks fail against the `v != 0.0` build**, and they are the whole delta:
nothing outside the boolean rows moves.

`AnamorphStateTests --risk008-probe` is the ninth opt-in instrument and is **synthetic by
construction, labelled as such in its own output**. It answers what a pending D-1 latency request
costs when nothing is servicing the JUCE message queue — the state a Linux VST3 host leaves behind
when it supplies `IRunLoop` only through `IPlugFrame` and the editor closes. The CAUSE is established
by reading the pinned wrapper, not by running a host: none was available in the review
environment. (The missing half arrived separately — the maintainer ran the predicted-failure
workflow on Linux in REAPER with the real VST3, and the latency updated with the editor both open
and closed; `docs/FUTURE_RISKS.md` RISK-008 carries that result and its limits. The probe is
unchanged by it: what it measures is the COST of an unserviced queue, not whether a host produces
one.) The CONSEQUENCE is exact,
because `juce::Timer` delivers only by posting a message for the message thread to run, so a console
harness that does not pump IS an unserviced queue — the same reason State tests 27, 30 and 31 have to
pump explicitly. Measured: across a 1000 ms unserviced window the reported latency does not move, and
22 ms after servicing resumes the request is delivered in full. It **asserts nothing and returns 0**,
and no sleep stands in for synchronisation: the negative phase asserts a state that cannot become
true later without servicing, and its deadline only bounds the run.

`AnamorphStateTests --legacy-match-probe` gained a companion in State test 31 rather than a new
probe: the per-slot Level-Match memory (ER-STATE-20) is observed through the product's own
behaviour. Two properties make that exact rather than a tolerance game — after a restore with no
A/B data both slots are re-seeded from the SAME restored state, so the switch is parameter-neutral,
and `LoudnessMatch` holds its published value on silence by documented design — so a switch
performed over silent blocks leaves `getMatchGainDb()` reading the injected value verbatim. The
test restores each blob into a reused instance AND a fresh one and requires them to agree, which is
the state-isolation contract stated directly and cancels the matcher's feed-forward predict term
exactly. It also performs the host's ordinary post-restore activation, without which the reused
instance's leftover audio in its delay lines flushes through and moves the reading 0.052 dB —
engine history rather than A/B state.

**State test 36 pins the durability half of the repair contract** (ER-STATE-25, round 27). State
test 20 already covers "a repaired parameter reaches the saved state", but it poisons with `nan`,
which makes `applyNorm`'s `! (|norm - getValue()| <= 1e-6)` gate true **on the comparison alone** —
so it exercises the repair path and never the gate's other side. This test takes that other side:
malformed text whose repair lands on the value the parameter already holds. **The precondition is
arithmetic, and the test searches for a qualifying parameter rather than hard-coding one** —
`replaceState` reads unusable text as the denormalised 0, `applyNorm` resolves it to the default, so
the gate is false exactly when `convertTo0to1(0) == getDefaultValue()`. It asserts on the
**serialized artefact**, not the runtime value, because "restore → parameter == default" passes
before the fix: the live APVTS tree that `copyState()` reads, and the bytes `getStateInformation`
emits. The whole poison→restore→save→reload cycle runs **twice**, so corruption cannot survive one
cycle and return on the next. Three legs guard the boundaries: a genuinely valid value equal to the
default must be left **exactly as written** (no needless rewrite), an out-of-range `raw` must be
rewritten canonically too, and a malformed `raw` beside a valid `value` must still fall back to the
value. **11 of its 30 checks fail against the pre-fix build, 0 after.**

**State test 35 pins the other half of a refused load: it must have no AUDIO side effect**
(ER-GUI-06, round 26). Round 24 made a foreign preset a no-op for STATE; the editor was still
raising the masking duck *before* asking the manager to load, so a load the manager then refused
still dry-filled the next ~32 ms. **The test drives the real editor**, because the defect was in the
editor's ordering and nothing below it could see the bug: `presetPrev`/`presetNext` carry
`setComponentID ("presetnav")` and differ by button text, so the child walk reaches the production
`onClick`. The harness writes two preset files whose names put the foreign row immediately after a
valid one — asserted, not assumed — loads the valid one so `currentIndex()` points at it, and then
clicks Next so `step(+1)` lands on the foreign row and is refused. **The observable is Test 48's**:
a MONO stimulus into an engaged widener, so every trace of side energy in the output is the
widener's own and a duck's dry fill collapses it; twin processors are driven identically and
compared to each other rather than to a threshold. **Both directions are asserted**, which is what
stops "delete the duck" from passing: a refused step must leave the next block *identical* to the
un-clicked control (to 1e-9), and a successful `loadFile` must still collapse it. A malformed file
takes the same no-side-effect path. **2 of its 16 checks fail against the pre-fix build, 0 after** —
measured side RMS 0.201786 against a 0.443549 control before, 0.443549 against 0.443549 after.

**State test 34 pins the preset loaders' acceptance test** (ER-STATE-24, round 24). A `.anamorph`
file is an `<ANAMORPH>` root, and a document with any other root is refused by **both** loaders
exactly as an unparsable one is — `loadFile` returns `false`, `load(index)` is a clean no-op, and
neither the sound nor the preset identity moves. The test is built so a reset cannot hide: the
sentinel is **five** parameters at values each asserted to differ from that parameter's own default
(the failure mode *is* "everything becomes its default", so a one-parameter probe could pass by
coincidence), and preservation is compared **exactly**, against what the parameters actually hold
after being set rather than against the literals requested — a stepped parameter quantises what it
stores, and the claim is preservation, not equality with a literal. The foreign document carries
`PARAM` children with *our* `id` spellings, so the rejection cannot be attributed to unrecognisable
children, and the log prints the before/after values so the failure mode is legible rather than
inferred from a boolean. Both loaders are exercised — the OS-chooser path and the menu path, the
latter after a `refresh()` that is asserted to have listed the file — and the whole rule is re-run
from a **second** distinct sound, per the repository's repeated-state-mutation discipline. Three
legs guard what must NOT change: malformed XML keeps its existing rejection, a valid `<ANAMORPH>`
root with only one `PARAM` still adopts that one and still defaults the rest, and a full valid
preset still round-trips. **6 of its 29 checks fail against the pre-fix build, 0 after.**

`AnamorphStateTests --restore-fade-probe` is the tenth opt-in instrument and, like
`--modern-settings-probe`, **measures and asserts nothing**. It drives the ordinary host order —
restore a non-default session into a fresh instance, THEN activate — and prints each module's
per-block deviation from the input over twelve blocks, so a module that opens at its restored target
reads the same in block 1 as in block 12. It is the magnitude behind ER-DSP-09 in the product's own
terms. **Its ratio deliberately does not separate two causes**, and the probe says so in its own
header: a smoother gliding from the wrong start (the defect) and empty delay-line / filter history
filling up (not a defect — `prepare` clears that history by contract, and Haas's own 28 ms line is
longer than the block the first point covers). So the ratio RISES when the fix lands without
reaching 1.0 — measured block1/block12 before → after: Haas 0.17 → 0.72, Velvet 0.09 → 0.18,
Chorus 0.29 → 0.68, Dimension-D 0.39 → 0.90, Mono Maker 0.35 → 0.58 — and a ratio below 1.0 here
is not by itself evidence of a defect. **DSP Test 49 is the discriminating instrument**, because its
reference cancels the history term exactly.

`AnamorphStateTests --state-prepare-race-probe` is the eleventh, and the third TSan probe the suite
never runs (`--state-thread-probe` and `--reprepare-race-probe` are the others; if the race is real
the probe's own execution is undefined behaviour, which is why it is opt-in). It answers the one
thread pairing D-2's recorded scope does not mention: a host calling `setStateInformation` on one
thread and `prepareToPlay` on another while the editor tick reads. **The verdict is the REPORT SET**,
compared against the four `--state-thread-probe` already measures — `abActive`, the `abUndo` vector
twice, and a `juce::String` refcount exchange. Measured round 20: the same four reports and no
others, which is what classified ER-STATE-23 as already covered by D-2 rather than a new bug
(`docs/FUTURE_RISKS.md` RISK-007).

**D-2 / ADR-0036 (2026-09-03) closed that class, and the three probes are now expected SILENT** —
and are run to prove it rather than trusted: the **`tsan` CI job** builds this suite with
`-fsanitize=thread`, proves the lane live with a seeded-race canary (`tests/tsan_canary.cpp`), then
runs each of the four probes five times with `halt_on_error=1` and the whole suite once under the
instrument. The fourth, **`AnamorphStateTests --d2-stress-probe`**, is the twelfth opt-in instrument:
every thread the ownership model names at once — a host thread restoring three sessions in turn and
saving after each, a second host thread re-preparing at alternating rates, the audio thread
processing noise, and the main thread doing what the editor and the processor's timer do (the tick's
reads, `pollUndoCoalesce`, a Settings binding read and written, A/B switches, undo/redo, factory
preset loads, gesture edits, the timer). On the pre-D-2 tree it reports the register's classes and
more (the Settings tree, the slot handles); after, silence, in every run.

**State tests 37–41 pin the ownership contract deterministically** (42–43, below, its round-2 closures), one per contract the task set
(`tests/state_tests.cpp` §D-2), each draining through `pollUndoCoalesce()` — the editor's own path —
so the same file measures the pre-fix tree with the same instruments:

- **37 (A/B)** — a host thread restores two sessions with different slot sets in turn while the
  owner cycles A → B → A → B between them. Before the drain the owner still sees the *previous*
  program whole (active index, name, undo history) while the sound has already moved; after it, the
  restored one whole. A host-thread save taken *inside* the pending window is byte-identical to the
  owner's save after the adoption, and one taken after it is too.
- **38 (restore)** — B → C → B → C off the message thread with the audio thread running: the
  oversampling atomic is stored before the restore returns, the Settings tree follows at the drain,
  the reported latency reaches a message-thread restore's truth, the audio path stays finite, and a
  save after the last restore is byte-identical to the session restored.
- **39 (preset)** — every factory preset loaded twice round with the audio thread processing and a
  host thread saving in a loop the whole time: identity by index and name, every parameter equal to a
  control instance's after the same load, and a sequenced host-thread save equal to the owner's.
- **40 (prepare / re-prepare)** — restores on one host thread and re-prepares at alternating rates
  and block sizes on another, the editor-shaped main thread reading and draining: no latency report
  from either host thread, and the timer leaves the host holding the final session's truth with the
  atomic and the tree agreeing.
- **41 (undo)** — history exists; a host restore leaves it whole until the drain and clears it at
  the drain; the owner then walks new history — undo, undo, redo, redo, a Copy undone on the other
  slot — while a host thread saves throughout, and every step lands exactly.

**State tests 42–47 (rounds 2–5, the PR review's findings) reproduce a reviewed interleaving
deterministically** rather than by timing, through the two seams `AnamorphAudioProcessor::seams`
exposes for exactly that (empty in production: one null check each, on non-audio paths). Each was
mutation-tested — its fix reverted in isolation makes it fail, 42 alongside 37's window check
(44 uses no seam for its first three legs, the adoption seam for the fourth):

- **42 (first-save consistency)** — the host thread is inside its save and has TAKEN the mailbox
  (the previous program's snapshot); the seam holds it there while the owner adopts the restore and
  republishes; the host side then decides. Its save must equal the owner's save after the adoption.
  The round-1 tree read two generation atomics after the take, saw "adopted", and wrote the old
  snapshot's name, slots and Settings around the restored sound; the generation now travels inside
  the snapshot (ADR-0036 §5).
- **43 (overlapping restores)** — two layers. On an `InternalState` alone: restore A (generation 1),
  restore B (generation 2), then A's delayed completion, an edit inside the window, B's completion, an
  edit after it, a late republication of A, a newer C — the engine-config word holds the latest
  arrival's oversampling throughout (ADR-0036 §8). On the processor: the owner has taken A from the
  cell, B lands from a host thread before A's tail runs (the adoption seam), and A's tail must not
  overwrite B's oversampling — an activation in the window primes the engine at B's setting and
  reports B's latency; B's adoption then brings the tree to B and a save equals B's session.
- **44 (Settings edits inside the pending window, round 3)** — precedence by arrival (ADR-0036
  §9): each of the six Settings edited alone after a restore arrived stands through the adoption
  while the other five take the restore's values, the engine word and a save agreeing; all six
  edited stand; an edit made *before* the restore arrived is replaced by it; two restores around
  two edits through the adoption seam (an edit after R1 but before R2 survives R1 and is replaced
  by R2, one after R2 survives both); an inline restore replaces every field. Every edit starts
  from a shown program it actually changes (a toggle can only flip what is shown). Mutation-tested:
  the adoption writing every field again fails it.
- **45 (an action in the handoff window, round 4)** — the restoring thread is held at the
  `beforeRestorePut` seam, where its SOUND is applied but its handoff is not in the cell, so the
  message thread cannot see it; an A/B switch (then, in a second leg, a Copy-to-other) runs there
  against the outgoing session; the handoff completes and the restore is adopted. The result must
  be ONE session: the restored metadata, the restored sound, the restored slots, and a save
  byte-identical to the session restored. Mutation-tested — with the adoption's sound re-install
  removed the save carries the restore's identity over the action's sound (ADR-0036 §10).
- **46 (an inline restore over a pending one, round 4)** — a restore handed over from a host
  thread, then one arriving on the message thread. The `afterRestoreTake` seam fires inside the
  drain and the live sound there must be the PENDING session's: the incoming session's decode has
  not run yet, because the inline path drains first. Mutation-tested — decoding before the drain
  adopts the pending restore against the incoming session's sound.
- **47 (a sound edit while a restore is pending, round 5)** — the counterpart to 45, and the case
  its first cut got wrong: after the handoff returns the restore sits in the cell until the next
  drain, and a knob turn is the one message-thread mutation that does not drain. One edit, then
  several across two parameters, then an untouched parameter — the edits must survive the adoption
  while the restored session's program, Oversampling and identity land, and a save must equal a
  message-thread restore of the same session carrying the same edit. No seam is needed. Mutation-
  tested — keying the adoption's re-install on `soundParamGen` (any change) instead of
  `soundSetGen` (wholesale replacements) erases every edit (ADR-0036 §12).

State tests 22 and 27 were re-shaped in the same change: their off-thread requester used to be a
`juce::Value` written from a worker, which after D-2 models nothing the plug-in does; both now drive
the real `setStateInformation` from the worker, and 27 asserts the ORDER of reported values because
the tick that adopts a restore delivers its latency from inside the adoption.

`tests/state_tests.cpp` (**46 tests**, own console target `AnamorphStateTests`) automates the
COMPATIBILITY policy family against the **real `AnamorphAudioProcessor`** (the target compiles
the plugin sources; since 2026-08-21 it also constructs and destroys the real editor, headlessly
and without ever showing it — no peer, no message loop, no interaction):
serialized-schema shape (every `SERIALIZATION_REGISTRY.md` field), a **parameter-registry
snapshot** (IDs/names/order/automation flags/step texts exact + range mappings probed at 5
normalised points, vs `tests/fixtures/parameter_registry.snapshot`), a raw-exact
save→load→save round-trip (byte-identical; APVTS + `raw` + InternalState + A/B slots + preset
meta; undo cleared), the three legacy migration paths via frozen fixtures
(`legacy_v0_2_bare_apvts.xml`, `legacy_pre_0_6_4_ab_slots.xml`,
`legacy_pre_0_8_4_view_params.xml`), corrupt/foreign-state robustness (garbage/truncated blob,
out-of-range `AB@active` clamp end-to-end, unknown future fields, corrupt slot XML), the user
preset save→reload round-trip incl. the exclusion rules (`mbSolo` reset, Bypass/`advancedMode`
untouched), A/B + view-param preservation across restore, **factory/user preset identity when a
user preset carries a factory preset's name** (0.9.2: saving under the shared name selects the USER
row, both rows stay individually selectable, an A/B round-trip keeps the identity, an undo after a
save keeps it too, a preset switch invalidates redo when the identity moves even if the two presets
sound identical **but re-picking the already-selected row does not**, and a
`.anamorph` loaded from OUTSIDE the preset folder or a user preset deleted from disk both tick
**nothing** rather than falling back to the same-named factory row),
**factory-id integrity** (ids present, unique, and every one resolving in the table — an
unresolvable id would apply the plain defaults, so exactly one factory preset may sit on the
all-defaults signature), and the **indicator identity across a session reload** (factory and user
identities restore, per A/B slot; an unresolvable factory id, a deleted user preset, a preset nested
in a SUB-folder of the preset folder, a preset whose file NAME `juce::File::isAbsolutePath` accepts
(a leading `~` on POSIX) and a pre-0.9.2 session with no identity each take their documented
fallback; and in EVERY one of those
eight paths — the seven that go through the reload helper plus the A/B slot check — the restored
parameters are asserted bit-identical, because the identity is metadata and must never influence the
sound), and the **wrapper audio path** (`testWrapperProcessBlockAudioPath`, 2026-08-18: the real
`processBlock` over a denormal-provoking noise→silence matrix with **no test-side FTZ arming**, so
it regresses `processBlock`'s own `ScopedNoDenormals` — and it is the only test in either suite
that drives the wrapper's audio path, which is what points the `sanitizers` job's ASan/UBSan and
valgrind runs of this suite at the wrapper's parameter snapshotting and buffer handling; a
liveness RMS check first proves the invariant is not vacuously green, and the
`ANAMORPH_TESTS_NO_FTZ` escape relaxes only the denormal half, exactly as in the DSP suite),
and the **editor lifetime** (`testEditorConstructDestroy`, 2026-08-21: `createEditor()` five times
over, each laid out and destroyed through `editorBeingDeleted`, asserting the premise first — a
non-null editor of the concrete type — then that layout ran and that **no peer was created**, so
"headless" is a property the test proves rather than one the environment happens to supply).
It needed no new target and no CMake change: this binary already compiled `PluginEditor.cpp` and
already linked `juce_audio_utils`/`juce_dsp`/`juce_opengl`, because `createEditor()` references
them — it had simply never been called. What that buys is the reason to do it here at all: this
suite already runs under **ASan+UBSan+vptr, LeakSanitizer, valgrind memcheck and LTO codegen**, so
the editor's constructor and destructor — 68 direct children, three LookAndFeels, an
`OpenGLContext` member, a `VBlankAttachment` and a `FrameClock` — now run under all of them.
Measured clean on the first exposure: 940×720, 68 children, five construct/destroy cycles, no
sanitizer report. **Linux-only by construction** (`#if JUCE_LINUX || JUCE_BSD`): headless editor
construction is unverified on Windows and macOS where this suite is also a blocking gate, and
KI-007 records that the GPU-less Windows runner cannot host editor GUI tests at all — every
instrument this test feeds is a Linux job, so the scoping costs no coverage. Widening it needs one
green run on the other two, not an argument.
Evidence [Verified]: tests/state_tests.cpp; CMakeLists.txt (`AnamorphStateTests`).

**Changing the parameter surface intentionally** (ADR + `PARAMETER_REGISTRY.md` update
required, per `PARAMETER_COMPATIBILITY_POLICY.md`): re-freeze the snapshot with
`AnamorphStateTests --write-snapshot` and let the snapshot diff be reviewed in the PR. An
**unintentional** change fails the suite on all three CI platforms — that is the point.
The registry comparison is numerically tolerant (1e-4 relative) only for the numeric fields —
the five range-mapping probes, the default, and the interval (libm ULP differences across
platforms); IDs, names, ordering, flags, counts and step texts compare exactly.

### Adding a test

Bug fixes ship a regression test that **fails on the old code and passes on the fix** (the
project's established practice; `docs/policies/TESTING_POLICY.md`). Use the existing
`check(cond, "description")` harness and add the call in `main` (DSP behaviour →
`tests/dsp_tests.cpp`; state/serialization/preset behaviour → `tests/state_tests.cpp`).

## Opt-in targets (not built by default, not shipped)

Three targets exist behind OFF-by-default options or outside CMake entirely. None of them enters a
release build; each answers a question the two self-test suites structurally cannot.

| Target | How to build | What it answers |
|---|---|---|
| `AnamorphBench` | `-DANAMORPH_BUILD_BENCH=ON`, Release | The `PERFORMANCE_BUDGET` §"required benchmark procedure" matrix — ns/sample and worst single block across sample rate, block size, algorithm, oversampling and multiband. |
| `AnamorphFuzzState` | `-DANAMORPH_BUILD_FUZZ=ON` with Clang + `-fsanitize=address,undefined` | `setStateInformation` against inputs nobody wrote by hand. |
| `AnamorphDspDump` | `-DANAMORPH_BUILD_DSPDUMP=ON`, Release | Whether a dependency bump changed engine output at all — §Proving a dependency bump is bit-identical. |
| `tests/realtime_effects.cpp` | no target — `clang++ -fsyntax-only -Werror=unknown-warning-option -Werror=function-effects`, then the same command again with `-DANAMORPH_EFFECTS_CANARY`, which must FAIL | Whether the JUCE-free leaf DSP is provably effect-clean at **compile** time, on branches no test executes — and, through the second compile, whether the diagnostic proving it is still active at all. |

```bash
# Benchmark. It REFUSES to run (exit 2) if it cannot identify the CPU and
# ANAMORPH_BENCH_CPU is unset -- PERFORMANCE_BUDGET constraint C2: a number
# without its machine and method is not a measurement.
cmake -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DANAMORPH_BUILD_BENCH=ON
cmake --build build-bench --target AnamorphBench
ANAMORPH_BENCH_SECONDS=10 ANAMORPH_BENCH_REPS=5 \
  ./build-bench/AnamorphBench_artefacts/Release/AnamorphBench

# State fuzzing. A REJECTED blob is a pass; the oracle is the sanitizer.
cmake -B build-fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=clang-22 -DCMAKE_CXX_COMPILER=clang++-22 \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DANAMORPH_BUILD_FUZZ=ON -DANAMORPH_BUILD_TESTS=OFF -DANAMORPH_BUILD_STANDALONE=OFF
cmake --build build-fuzz --target AnamorphFuzzState
# Note: libFuzzer SAVES new coverage-increasing inputs into the corpus
# directory it is given. They are .gitignore'd (only the `*.bin` seeds
# are tracked), so a local run cannot add them to a commit by accident.
# The note sits ABOVE the command, not between its continuation and its
# arguments: a `\` followed by a comment line splices the two into
# `ASAN_OPTIONS=... # ...`, which sets nothing for anything, and the fuzzer
# then runs on the next line with leak detection ON -- see below.
ASAN_OPTIONS=detect_leaks=0 \
  ./build-fuzz/AnamorphFuzzState tests/fuzz-corpus -max_total_time=90
```

`detect_leaks=0` on the fuzz run is required, not optional: the harness leaks JUCE's
`ScopedJuceInitialiser_GUI` on purpose, because letting `shutdownJuce_GUI()` run under libFuzzer's
`exit()` double-frees in `DeletedAtShutdown::deleteAll()` during `__run_exit_handlers`. Leak coverage
for the same code is the `sanitizers` job's, which runs with `detect_leaks=1`.

**CI now builds all four and gates on three of them.** The fuzz run and the compile-only effects
check are hard gates on their output. `AnamorphDspDump` joined them on 2026-08-21 and is the third:
`linux-lto-tests` runs it with `--self-check`, which asserts every scenario is repeatable and that
they are distinct from each other, and exits 3 if not — so the gate is on the instrument's ability to
discriminate, not on its hashes. Nothing in CI diffs those hashes and nothing stores them; that is
still a human's step at bump time (§Proving a dependency bump is bit-identical). Until that date no
job built this target at all, which made it the one committed harness with no protection against the
rot its neighbour's CI step exists to prevent. The benchmark is built and smoke-run but its
*numbers* are not gated — measured run-to-run spread on an idle machine is 7.2% (median ns/sample)
and 65.4% (worst block), so a threshold would be noise rather than signal; what the build catches is
a harness that has silently stopped compiling against the engine it measures. The fuzz run and the
compile-only effects check are hard gates.

## Proving a dependency bump is bit-identical

`DEPENDENCY_POLICY.md` rule 2 makes bit-identical engine output the gate a JUCE bump must pass.
`tests/dsp_dump.cpp` is the instrument, and it is **committed** — the two bumps that passed this rule
before it existed each used a scratchpad tool that was then discarded, so the gate was permanent and
the instrument was rebuilt from scratch every time.

The tool prints one deterministic line per scenario: an FNV-1a hash over **every output byte** plus
the engine's reported latency, across 32 scenarios (4 algorithms × 4 oversampling factors × M/S
off/on) at 48 kHz / 512 samples, 120 blocks of fixed-seed noise then 120 of digital silence — the
silence phase is what catches denormal and tail differences the noise phase hides.

```bash
# Build the SAME source against two JUCE checkouts, otherwise identical flags.
for JUCE in /path/to/JUCE-old /path/to/JUCE-new; do
  out="dump-$(basename "$JUCE")"
  cmake -B "build-$out" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DANAMORPH_BUILD_DSPDUMP=ON -DANAMORPH_BUILD_TESTS=OFF \
        -DANAMORPH_BUILD_STANDALONE=OFF -DANAMORPH_JUCE_PATH="$JUCE"
  cmake --build "build-$out" --target AnamorphDspDump
  "./build-$out/AnamorphDspDump_artefacts/Release/AnamorphDspDump" > "$out.txt"
done
diff dump-JUCE-old.txt dump-JUCE-new.txt && echo "bit-identical"
```

An empty diff is the proof. Any differing line names the exact scenario to investigate, and the
latency column moving is its own finding — a reported-latency change is an AI-agent hard stop.

**The tool checks itself before it reports, every run, not on request.** Two properties, because
they fail independently: every scenario must be **repeatable** (the same scenario run twice hashes
the same — otherwise every diff is noise) and all 32 must be **distinct from each other**
(otherwise a diff is empty for the wrong reason). It exits **3** rather than printing a table it has
not shown to be discriminating.

That second check is not hypothetical. The first run of the original scratchpad tool left
`algoAmount` at its `0` default, which is identity for the wet path, so the algorithms hashed the
same as one another and the tool reported 32 matching hashes while never reaching the code under
test. It was caught by a human noticing two rows that should differ did not. Setting `algoAmount`
back to `0` in the committed harness today reproduces it exactly — 16 colliding scenario pairs,
named, exit 3. **Fix the scenario set; never relax the check.**

Two build choices are deliberate. It does **not** link `juce_recommended_lto_flags`, unlike
`AnamorphBench` beside it: the bench must measure the shipped binary so it carries the shipped
flags, while this tool must isolate one variable and LTO is a second one — link-time inlining can
differ between two runs for reasons unrelated to the dependency under test. And nothing is stored:
no committed golden hashes, because that would be the golden-master DSP baseline this repository
deliberately rejects. The question is never "does this match a stored value" but "does build A match
build B", and only a diff between two runs answers it.

## pluginval (VST3 + AU conformance)

```bash
scripts/run-pluginval.sh 10 deterministic        # fixed seed (mode A)
scripts/run-pluginval.sh 10 randomise            # --randomise x3 (mode B)
scripts/run-pluginval.sh 10                      # deterministic (default mode)
scripts/run-pluginval.sh                         # default strictness 8 (the working bar)
scripts/run-pluginval.sh 10 deterministic au     # the AU, macOS only (see below)
```

Strictness targets (spec 11.3): `5` development, `8` standard gate, `10` pre-release gold standard.
The value CI enforces is `ANAMORPH_PLUGINVAL_STRICTNESS` in `.github/workflows/build.yml`, which is
the single place it is written down.

Each `mode` — `deterministic` and `randomise` (`--randomise`, randomised test **order**) — runs
**3 consecutive** passes, and **both must pass on all three platforms** (Windows uses
`run-pluginval.ps1`): the randomise mode exercises state restoration under an order a fixed run
cannot reach.

**The deterministic seed is nonzero, and it did not used to be.** Both scripts passed
`--random-seed 0`, which pluginval reads as *"generate a random seed"* (`Source/PluginTests.h`;
`Source/CommandLine.cpp` forwards the flag only when it differs from that default) — so the
"deterministic" mode drew a fresh seed on every run and a failure in it could not be reproduced from
the log. The seed is now pinned to a nonzero value, identically in both scripts. It is meaningful
*without* `--randomise`: it seeds the RNG the tests themselves draw from, while `--randomise` only
shuffles test order, so the two flags are independent.

**The third argument is the format**: `vst3` (default) or `au`. `au` is macOS-only and **errors**
(exit 2) on any other host rather than skipping — a gate that quietly does nothing is worse than no
gate. The AU is the only format Logic and GarageBand load, and it is validated in CI on macOS in both
modes ×3, exactly like the VST3. macOS resolves Audio Units through the **AudioComponent registry**,
which only knows about bundles under a `Components` directory, so a freshly built, never-installed
`.component` can report *zero plugin types* however correct it is. Install it first — CI copies it to
`~/Library/Audio/Plug-Ins/Components` and kills `AudioComponentRegistrar` to force a re-scan — and
point the script at it with `ANAMORPH_PLUGINVAL_BUNDLE`. That variable is fail-closed: set but
missing is an error, never a silent fall back to discovery.

Discovery of the bundle is fail-closed on **ambiguity** too: exactly one match under `build/` is
required. The previous `find … | head -n1` validated whichever bundle enumerated first, so a stale or
multi-config tree could pass the release gate on a bundle that was not the one just built — a
local-only hazard, which is exactly where it would go unnoticed.

The script downloads pluginval if absent (a failed `chmod +x` on it is now an error rather than
`|| true`, so a setup fault reports where it happens instead of resurfacing as an opaque "cannot
execute" from the validation loop), and runs under `xvfb-run` when available (Linux editor tests need
a display). Evidence [Verified]: scripts/run-pluginval.sh / scripts/run-pluginval.ps1.

### Signal-only retry (known X11 host flake)

`run-pluginval.sh` treats a real
validation failure (exit < 128) as a failure immediately. On Linux it retries up to 3 times **only on
a signal-crash** (exit ≥ 128) to absorb a use-after-free in **pluginval's own JUCE** X11
`XEmbedComponent` (a `ConfigureNotify`→`callAsync` on rapid editor open/close), not a plugin defect —
the plugin already drops its OpenGL child window on Linux (ADR-0011). On Windows,
`run-pluginval.ps1` fails immediately on a real validation failure **and, since 2026-08-31, on a
real abnormal termination too** (a negative / ≥256 exit code, i.e. a Win32 exception of a launched
validator): until then its loop still gave every crash up to 3 tries per pass — a retry originally
justified by the null-`$LASTEXITCODE` detection problem that the KI-007 `WaitForExit` fix had
already retired, leaving it excusing exclusively genuine crashes on a platform with no documented
flake (ER-CI-01). Its 3-attempt loop now covers only a `$null` exit code, which after that fix can
mean nothing but "the process never launched" — a setup fault, not a verdict.

**The retry is scoped to the platform its justification names.** Until 2026-08-18 the script applied
the same three attempts on **macOS**, which shares none of that X11 machinery: there, a crash had two
extra chances to pass and no documented flake to absorb, and this section already described the
behaviour as Linux-only. `CRASH_RETRY_ATTEMPTS` is now set from `uname -s` — 3 on Linux, **1**
everywhere else — and a single-attempt failure prints a distinct message so it cannot be misread as
an exhausted retry. The 2026-08-31 Windows change above completes the same scoping for the third
platform. Evidence [Verified]: scripts/run-pluginval.sh (`run_one_pass`, and the `case
"$(uname -s)"` above it); scripts/run-pluginval.ps1 (verdict block).

## CI integration

All three build jobs run the self-tests + pluginval in **both** modes (deterministic ×3 + randomise
×3), and **all three are blocking** — Windows/macOS do not use `continue-on-error`, so a non-zero
pluginval exit fails the job on every platform. Linux/macOS use `run-pluginval.sh`; Windows uses
`run-pluginval.ps1`. macOS additionally runs the whole gate a second time for the **AU**, and runs
the self-tests a second time for the **x86_64 slice under Rosetta 2**.

A fourth build job, **`macos-intel`** (`macos-15-intel`), runs the same self-tests and the same
full gate — VST3 and AU, both modes ×3 — against a thin `x86_64` build on **native Intel
hardware**, and it is blocking too. It exists for the difference between *an x86_64 binary running
under Rosetta* and *an x86_64 binary running on an Intel CPU*: Rosetta translates and then executes
on arm64, so the DSP invariants that depend on the hardware denormal-flush bits are being checked
against the wrong register file. It packages and uploads nothing — the shipped macOS artifact is
still the universal bundle from the `macos` job. Its first step **fails** the job if `uname -m` is
not `x86_64` or `sysctl.proc_translated` is not `0`, so it can never report a green Intel result
from somewhere that is not Intel.

Seven further jobs run beside the build jobs, none in a `needs:` chain in either direction, so a
finding in one never skips a binary that is otherwise fine. (`merge-check` is not among them: its
`if:` is the exact complement of every other job's, so it runs only on the same-repo pull-request
event — where it is the only job that runs at all.)

| Job | Run it locally as |
|---|---|
| `docs` | `python3 scripts/check-docs.py --self-test && python3 scripts/check-docs.py` |
| `source-lint` | `python3 scripts/check-portability.py --self-test` then the lint, `python3 scripts/check-realtime.py --self-test` then that lint, then `python3 scripts/check-citations.py --self-test` then `--check --base <rev>` |
| `sanitizers` | ASan+UBSan over both suites, then valgrind memcheck over both suites (the valgrind step sets `ANAMORPH_TESTS_NO_FTZ=1` — see below) |
| `realtime` | `cmake -B build-rtsan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C(XX)_COMPILER=clang(++)-<major> -DCMAKE_C(XX)_FLAGS="-fsanitize=realtime -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=realtime`, build `AnamorphTests`, run it with **no `RTSAN_OPTIONS`** (ADR-0029 — `halt_on_error=false` would make it report and pass) |
| `tsan` | `cmake -B build-tsan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C(XX)_COMPILER=clang(++)-<major> -DCMAKE_C(XX)_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread -DANAMORPH_BUILD_STANDALONE=OFF`, build `AnamorphStateTests`, then with `TSAN_OPTIONS=halt_on_error=1:exitcode=66` run `--state-thread-probe`, `--state-prepare-race-probe`, `--reprepare-race-probe` and `--d2-stress-probe` (five times each in CI) and the suite once; the canary first (`clang++ -fsanitize=thread tests/tsan_canary.cpp` must FAIL with a data-race report). Needs `libclang-rt-<major>-dev`; on a kernel with 32-bit ASLR entropy, `sysctl vm.mmap_rnd_bits=28` |
| `linux-lto-tests` | `cmake -B build-lto -G Ninja -DCMAKE_BUILD_TYPE=Release -DANAMORPH_BUILD_STANDALONE=OFF -DCMAKE_C_FLAGS=-flto -DCMAKE_CXX_FLAGS=-flto -DCMAKE_EXE_LINKER_FLAGS=-flto`, build both test targets, run both — the suites against the shipped optimization class (see `CI_CD.md`) |
| `fuzz` | the `AnamorphFuzzState` recipe under §"Opt-in targets" above, verbatim — the CI step adds only `-seed=20260818 -rss_limit_mb=4096 -print_final_stats=1` and an `-artifact_prefix` for the reproducer it uploads on a finding |

**`ANAMORPH_TESTS_NO_FTZ=1` is for valgrind and nothing else.** The DSP suite treats a denormal in
the engine output as a failure, which holds because the audio path runs under
`juce::ScopedNoDenormals` and the CPU flushes denormals to zero *in hardware*. valgrind emulates
floating point and does not honour the FTZ/DAZ bits, so under memcheck denormals survive and the
check fails on a build that is correct on every real CPU — while memcheck itself reports **zero
errors** on the same run. The variable relaxes that half of the check (NaN and Inf stay failures)
and only a literal `1` enables it. Every native job runs without it, so the invariant is gated on
every push on every platform; never set it for a normal run. A run that *is* relaxed says so — the
suite prints a `::warning::` naming the un-asserted half at start-up and repeats it beside the
verdict, so an inherited or stale setting cannot produce a `ALL TESTS PASSED` line that looks like a
full gate.

**`check-citations.py` needs a base revision, and which one you pick changes the answer.** CI compares
against the *previous push*; a local run against `origin/main` can differ, because a document whose
citation *count* differs from the base falls back to ordinal pairing, which only judges base
spellings still present verbatim — and a re-anchor changes the spelling. Run **both** before
concluding the gate is green. If you re-anchor a citation deliberately, declare the pair in
`DELIBERATE_REAIMS` in the **same change set**: the tool cannot tell a repair from a drift, so a fix
landed on its own turns the gate red on the commit that fixed it.

See `CI_CD.md`. Evidence [Verified]: `.github/workflows/build.yml`.

## Failure analysis

| Symptom | Likely cause | Where to look |
|---|---|---|
| A `check` assertion fails | DSP regression | the named test in `tests/dsp_tests.cpp`; compare against the invariant it guards (`docs/policies/DSP_POLICY.md`) |
| A state-test `check` fails | serialization / parameter-surface regression | the named test in `tests/state_tests.cpp`; if the change is INTENTIONAL it needs the compatibility-policy process (ADR + registry update + `--write-snapshot`) |
| pluginval exits < 128 | real validation failure | the pluginval log line; do **not** retry — it's a genuine defect |
| pluginval exits ≥ 128 (crash) | the known X11 host flake | retried automatically; if it still fails after 3 tries, treat as a failure (`scripts/run-pluginval.sh:172-198`, `run_one_pass`) |
| `AnamorphTests`/`AnamorphStateTests` `not found` | not built yet | run `scripts/build.sh` first (`scripts/run-tests.sh:51-73`) |

## Gaps in the automated coverage (known, deliberate)

Things the gates above do **not** do. All are recorded so nobody assumes coverage that
doesn't exist. One entry — automated AU validation — is now **closed** and kept struck through
rather than deleted, because a gap that was real and is now covered is worth being able to find.

- **GUI-lifetime defects have no headless test.** This is a **`TESTING_POLICY` rule-1 exception
  under ADR-0025**, and this entry is the register that ADR names. Its four required disclosures:

  1. *Why no reliable test exists.* The Level-2/3 surface is two console targets
     (`scripts/run-tests.sh`), and `tests/state_tests.cpp:6-11` records that it constructs and
     destroys the editor but never SHOWS it — "no peer, no message loop, no interaction". A defect
     that exists only while a
     **modal child is open and its owner is destroyed** — the 0.9.2 preset drop-down crash,
     **INC-010** — has no object to act on there. Level 4 does open and close the editor, but
     pluginval drives a host we do not control and never opens a menu first.
  2. *What replaced it.* The fix removes the lifetime rather than the symptom: a menu given
     `withParentComponent` is a child component, so `ModalComponentManager`'s
     `ComponentMovementWatcher` cancels it with result 0 on the owner's destruction **or hide**, and
     it has no independent lifetime left to get wrong. The remaining asynchronous window is closed by
     a `SafePointer`. Every other async/modal callback in the editor was audited for the same shape;
     the "Load Preset…" file chooser was the only other one, and it got the same guard. The
     mechanism was re-derived from the pinned JUCE source rather than assumed — see INC-010.
  3. *Where the gap is tracked.* Here, and cross-referenced from `POSTMORTEMS.md` INC-010.
  4. *Whether infrastructure could close it.* **Partly, and concretely.** The *structural* half —
     "is the menu a child of the editor" — becomes assertable the moment the harness instantiates an
     editor, which the sibling plug-in Anabasis already does in its own suite on all three CI
     runners. That is a harness change to prove on the Windows and macOS runners on its own merits,
     not to fold into a crash fix. The *behavioural* half — destroy the owner while the menu is
     modal and then click an item — needs a driven message loop and remains out of reach. Per
     ADR-0025 §5 this entry is revisited when that harness lands, not left standing.

- **Editor interaction defects have no headless test either.** A second
  **`TESTING_POLICY` rule-1 exception under ADR-0025**, covering **all six** v0.9.3 GUI fixes.
  Enumerated in full rather than leaving any to be inferred, because ADR-0025 §3 makes the four
  disclosures mandatory *per invocation* and every one of the six ships without a regression test:

  1. the Multiband add-split preview line stalling under a moving pointer;
  2. the unified pop-up dismissal shield — a dismissing click must close the pop-up and touch nothing
     underneath (Settings drop-downs, the Save Preset text menu, the preset menu);
  3. **pop-up lifetime** — a drop-down must not outlive the editor being hidden, destroyed or sent to
     the background, and cancelling one must neither pull the host window back to the front nor apply
     a half-typed inline edit;
  4. menu width measured from the item text (it clipped *Select All*);
  5. disabled menu items drawn dimmed;
  6. **Tooltips off meaning off** — gated at the source through the virtual `getTipFor`.

  The same four disclosures apply to all six:

  1. *Why no reliable test exists.* They need things the two console targets do not have — a real
     vblank tick plus pointer motion over a settled spectrum for the first, JUCE's modal machinery
     delivering a real mouse-down for the shield, and a rasteriser plus a font for the menu-width and
     disabled-item rendering. The editor is constructed but never shown
     (`tests/state_tests.cpp:6-11`), and neither suite has a pointer or a display.
  2. *What replaced it.* Every root cause was traced to specific lines — our own S2 repaint gate for
     the first, `juce_Component.cpp:2507-2544` and `juce_ModalComponentManager.cpp:81-89` in the
     pinned tree for the shield, and the mismatch between `getIdealPopupMenuItemSize` and
     `drawPopupMenuItem`'s own layout for the width. Where a GUI test would normally be the evidence,
     the shield's riskiest property is instead **proved from the source**: it cannot be raised in
     front of a menu, because `MenuWindow` sets `alwaysOnTop` (`juce_PopupMenu.cpp:365`) and
     `Component::toFront` on a non-always-on-top component inserts behind every always-on-top sibling
     (`juce_Component.cpp:914-922`). Conditions and reasoning in
     `worklogs/GUI_INTERACTION_FIXES_v0.9.3.md`, plus a manual check per platform. That check was
     **performed and signed off by the maintainer on 2026-08-09 for the first two fixes** (the
     add-split preview line and the pop-up dismissal behaviour), discharging this disclosure for
     those. The later three — the shield's interception-only redesign, the two menu-rendering fixes
     and the Tooltips transition — carried a sign-off on the **problem reports and the required
     contract** rather than on a manual test of the implementation. The **visual** half of what was
     then still owed is now discharged: the maintainer **reviewed and approved it on 2026-08-11** —
     the equal-width Widen / Style-Focus row is confirmed **intentional**, the narrower Simple-mode
     Widen control is **accepted**, the current pop-up/menu width behaviour is **accepted**, and the
     remaining visual verification items are **approved** (recorded in
     `worklogs/GUI_INTERACTION_FIXES_v0.9.3.md` §7 and §10). That sign-off covers the **visual/UI**
     items only: the behavioural per-platform checks in the same lists (a dismissing click reaching
     no control, pop-up lifetime across a hidden/closed/backgrounded window, the out-of-process host
     confirmation) and the **installer** checks in the fifth bullet below are **not** covered by it
     and remain owed. None of this touches the Level-5 *audio* audition or
     the compatibility checklist, which are separate and remain open (`HANDOVER.md` §Release Status).
  3. *Where the gap is tracked.* Here, alongside the INC-010 entry above, and referenced from
     **INC-011**'s Prevention field.
  4. *Whether infrastructure could close it.* **Yes, and it is the same infrastructure** the INC-010
     entry names: a harness that instantiates the editor and drives synthetic mouse events. All of
     these become assertable at that point — a hover move must dirty the frame, a click while a
     pop-up is open must reach the shield and no control, and a measured menu must fit its longest
     item. Revisited when that harness lands.

- **Hover occlusion under an open pop-up has no committed test.** A third
  **`TESTING_POLICY` rule-1 exception under ADR-0025**, for the 0.9.4 fix that stops a control
  covered by a drop-down reporting itself hovered (`cursorIsOverOpenPopup()`). The four disclosures,
  written fresh per ADR-0025 §3 rather than deferred to the entry above:

  1. *Why no reliable regression test exists.* It would have to live on the Level-2/3 surface, and
     that surface has no pointer and nothing on screen: `tests/state_tests.cpp:6-11` records that it
     "constructs and destroys the editor but never shows it", and the defect is
     a property of `Component::getMouseXYRelative()` — `getLocalPoint (nullptr,
     Desktop::getMousePositionFloat())` (`juce_Component.cpp:3233-3236`) — so reproducing it needs a
     **real OS cursor** over a **real menu window**, i.e. a display. Level 4 opens the editor but
     pluginval drives a host we do not control and never opens a menu, let alone positions a pointer
     inside one.
  2. *What verification was performed instead.* Not a structural argument — a **measurement**, on
     the running editor. A throw-away harness (not committed; see disclosure 4) linked the real
     `AnamorphAudioProcessor`, instantiated the editor into a window on an `xvfb` display, warped the
     real pointer with `Desktop::setMousePosition`, and read the eased `"hovA"` property the
     LookAndFeel actually paints from. With the pointer at the centre of an open combo list
     (menu `702,279 125×114`), the `Knob` underneath (`705,307 122×131`) read **0.990 before the fix
     and 0.000 after**, three runs each, same geometry and same probe point; the combo that owns the
     list read ~0.02 both ways, so the fix removes the false highlight without touching the true one.
     Dismissing the list with the pointer unmoved returned the knob to **0.990**, so nothing is left
     stuck dark. The preset-menu branch was **mutation-tested**: with the modal-child scan disabled
     and a preset library large enough to make the menu 690 px tall, the A/B control underneath read
     **0.990**; restored, **0.000** — so that branch is load-bearing, not defensive. The un-settle in
     `refreshPopupShield` was mutation-tested the same way (**1.000 → 0.022** with it, **0.990 →
     0.990** without).
  3. *Where the gap is tracked.* Here, and from `CHANGELOG.md` `[0.9.4]` and `HANDOVER.md`. The two
     defects the same measurement found were filed as **KI-024** (the Settings / About / Save-Preset
     overlays occlude identically — measured at hovA **0.990** behind an open Settings panel) and
     **KI-025** (the idle gate could seal on a still-lit control when the pointer left the editor
     inside one frame — measured **0.990**). **Both were fixed on 2026-08-19** and removed from
     `KNOWN_ISSUES.md` per its fixed-item rule. Those fixes ship under this same exception and for
     the same reason, and were verified the same way — the harness above, extended to every overlay,
     plus three mutation runs and a before/after idle-pass measurement.
  4. *Whether infrastructure could close it.* **Yes — and this fix narrows the standing claim above,
     which is worth recording rather than repeating.** The INC-010 and v0.9.3 entries both state that
     the *behavioural* half — a driven message loop with synthetic pointer input — "remains out of
     reach". Measured 2026-08-19, on Linux it is not: `xvfb` is already installed on the CI runner
     for pluginval, and the harness above drove the editor, opened menus and positioned the pointer
     with no repository change at all. What is still owed is making it a **committed** target — a
     CMake target, an `xvfb` wrapper in `run-tests.sh`, and the same thing proven on the Windows and
     macOS runners, where no equivalent virtual display is configured. That is a harness change to
     land on its own merits with its own CI evidence, exactly as the INC-010 entry says, and not to
     fold into a hover fix. Revisited when it lands; at that point this entry and the two above are
     closed together, because the same harness reaches all three.

- ~~**The AU is never validated automatically.**~~ **CLOSED.** The macOS job now runs the full
  pluginval gate against `Anamorph.component` as well as `Anamorph.vst3` — same strictness, both
  modes, ×3 each — so the build Logic Pro and GarageBand load passes the same format-conformance
  gate as the VST3. The registry problem this entry described is solved the way it predicted: an
  install step copies the bundle into `~/Library/Audio/Plug-Ins/Components/` and forces a
  refresh (`killall -9 AudioComponentRegistrar`) before validation, and
  `ANAMORPH_PLUGINVAL_BUNDLE` points the script at *that* copy rather than at the build tree.
  **Ordering:** the AU (and VST3) gates run **after** the packaging step, against
  `dist/Anamorph-macOS/` — the stripped, ad-hoc-signed tree the artifact is uploaded from — so the
  validated bytes are the shipped bytes. The stripped-but-unsigned state this entry warned about
  never arises, because `package` signs *after* it strips and the gate runs after both. The
  installed copy is removed again once the AU gates have reported, so reproducing these steps by
  hand does not leave a plug-in behind in your real `~/Library`. One thing this deliberately did
  **not** do, so the remaining scope is not overstated:
  - It uses **pluginval**, not Apple's `auval` (`auval -v aufx Anmr RTec`, matching the
    `PLUGIN_CODE` / `PLUGIN_MANUFACTURER_CODE` in `CMakeLists.txt:424-425`). pluginval hosts the AU
    through JUCE's `AudioUnitPluginFormat`, which is the same resolution path a JUCE-hosted DAW
    takes and the same test set the other two platforms are held to; `auval` is Apple's own
    conformance tool and tests things pluginval does not. Adding it is a further step, not a
    substitute for this one.
- **No frozen golden-audio reference exists.** `tests/fixtures/` holds a parameter-registry
  snapshot and three legacy session XMLs — metadata, not audio. The DSP suite pins *behavioural
  invariants* (exact nulls, click-freeness, spectral-spur and pitch bounds, cold-path bit-identity)
  rather than a stored waveform, which is deliberate: a golden audio file would freeze bit-exact
  output and collide with the Class-B numerical changes `DSP_POLICY.md` explicitly permits. The
  right tool for "did this change alter the sound" is the **twin dump** — build the engine before
  and after, run the same scenario matrix through both, compare hashes and reported latencies —
  which is what the JUCE 9 migration used across 32 scenarios
  (`worklogs/JUCE9_MIGRATION_v0.8.13.md`). That harness has been **committed** since the
  2026-08-18 round as `tests/dsp_dump.cpp` (§"Proving a dependency bump is bit-identical" above)
  — this bullet said "session-local and not committed" for five rounds after that stopped being
  true (ER-TST-05); the method no longer needs re-creating per investigation.

- **The twin dump hashes a fixed engaged steady-state matrix, and everything outside it is
  outside the bit-identity claim.** Each of the 32 scenarios applies one parameter set, calls
  `reset()` (which flushes any in-flight switch duck), and hashes 240 settled blocks — so the
  compared surface includes the continuous-smoother glides, the M/S conditioning loop, and all
  four algorithm modules engaged, but **excludes** the switch-duck/adopt machinery, the Bypass
  and Multiband-Enable crossfades and their off paths, M/S solo, `SoloMonitor`'s band-pass, the
  L/R-domain conditioning branch, the crossover glide, the full-wet idle path, and the NaN-heal
  pass — all first-party float code compiled under the same ISA flags. On GCC/Clang the
  contraction risk in those blind paths is closed binary-wide by ADR-0031's 0-FMA objdump
  census; on MSVC it rests on documented `/fp:precise` semantics plus the toolset ≥ 14.30
  assertion, not on measurement. ADR-0032 hedges with "for this instrument's coverage"; this
  entry is where that boundary is actually written down (ER-TST-02, 2026-08-31). Extending the
  matrix with transition scenarios is a recorded round-2 candidate, not a commitment.

- **No gate ever installs anything.** CI builds the packages and inspects them — the Inno Setup
  exe, the expanded `.pkg` (component identifiers, `customize="allow"`, non-relocatable
  components, payload completeness), the staged Linux tree — but never runs an installation,
  because installing needs elevation and would mutate the runner. Everything that only exists at
  *install* time is therefore manual. **INC-012** is what that gap costs: bundle relocation is a
  property of Installer.app's behaviour, invisible in the archive, and every manual check until
  then had been a first install onto a machine with no prior copy — the one case it cannot affect.
  The checks that remain owed to a human:
  - **macOS `.pkg`, per format (VST3 / AU / app), four cases each:** fresh machine · over an
    existing install · after moving the installed item elsewhere · after deleting it. Each must end
    with the item present at the destination in
    `docs/procedures/PACKAGING.md` §"macOS reinstall behaviour", and a moved copy must be left
    where the user put it.
  - **Linux `install.sh`/`uninstall.sh`:** both modes, plus the failure paths (no `sudo` on
    `PATH`; a `sudo` the user cannot authenticate). *Verified 2026-08-11 on Linux against a stubbed
    payload* — default/`1`/unrecognised answers all install per-user with no elevation, `2` installs
    system-wide via `sudo`, missing `sudo` and denied elevation each exit 1 having installed
    nothing, root skips the prompt, and install→uninstall round-trips in both modes. The
    **replacement transaction** was verified the same way and to the same date, by injecting each
    failure rather than reading the code: failed staging, failed commit, `INT`/`TERM`/`HUP` delivered
    inside the swap window, `SIGKILL` in the window followed by recovery on the next run (including
    a next run that itself fails), staging location on a normal layout and on a `~/.vst3` symlinked
    to a second filesystem, uninstall after an interrupted install, and the coexistence warning —
    each against a control run of the previous script that ends with nothing installed. *Re-verified
    2026-08-20 against a stubbed payload* for the options and the staging guards brought over from
    the sibling product: `--help`, `--user`/`--system` non-interactively, both flags together and an
    unrecognised option each exiting 1, a repeated flag accepted, `--user` under root refused by
    both scripts; a `TERM` inside the copy and inside each of the two rename windows leaving either
    the previous plug-in or the new one in place with no scratch left behind; a symlinked and a
    foreign-owned candidate together stopping the run with both paths named and nothing installed;
    a group-writable candidate refused, left untouched and fallen through; a parked copy restored by
    the next install, reported rather than skipped when its directory is no longer usable, kept by a
    plain uninstall and removed only by `--discard-parked`; and a system-wide install as root
    naming the per-user copy it coexists with. `INT` was delivered as `TERM` because a job
    backgrounded by a non-interactive shell inherits `SIGINT` ignored — a property of the harness,
    not of the script, whose three signal traps are identical. What those
    runs do **not** cover, and a real machine must: that a DAW actually finds
    `~/.vst3/Anamorph.vst3` after a per-user install.
  - **Windows installer:** unchanged in 0.9.3 beyond the two 0.9.2 casing corrections.
  **Could infrastructure close it:** yes, and cheaply for macOS — `installer -pkg … -target /`
  on the runner, then assert the three destinations, re-run after `mv`-ing one away. That is the
  obvious follow-up if this class recurs.

## What cannot be verified headlessly

Audio **sound quality** and GUI/vectorscope **visual appearance** cannot be judged in a headless
sandbox. Load the built `.vst3` in a DAW (e.g. Reaper) on a machine with audio + display. A green
build + pluginval pass is "ready to audition," not final sign-off
(`docs/policies/TESTING_POLICY.md` Level 5).
