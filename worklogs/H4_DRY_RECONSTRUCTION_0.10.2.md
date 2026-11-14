# H4 dry reconstruction — N2-01 owner decision and V2-01 verification

**Decision status: Proposed; no H4 production correction is approved or implemented.**

Starting main: `113b9189f0ff4d3b6348c553c559c22ee8ade782`, freshly fetched unchanged.
Product 0.10.1, target 0.10.2. JUCE 9.0.5 remains pinned at
`d60b7a78030661574a615b586d9325ba7a7fa7dd`. Original investigation evidence was
committed first as `6b55dfad` in PR #237. This packet uses that durable
[original report](dsp-numerical-audit-0102/original/REPORT.md), not temporary
conversation history. Candidate patches apply to that original production code
in isolated worktrees only. This PR changes live claims/comments and adds
nonshipping experiments; it leaves H4's executable implementation unchanged.

## Decision requested

Recommend **continued targeted recovery investigation** before choosing a production
candidate. A guarded warm bank remains a direction to assess: keep the existing
dry crossover bank and aligned-delay history warm whenever H4 qualifies, and
recover invalid unused dry state without resetting valid wet/other-channel state.
The current A2 output-only detector misses the engine witness documented below. Keep H4's exact wet
output shortcut and clean-dry Level Match reference. This fits the existing
allpass architecture, signal order, latency and parameter/persistence contracts.
It is not equivalent to globally disabling fullWetIdle. The simpler A/C diagnostics
resolve ordinary reconstruction but are not production-ready: the recovery
amendment below demonstrates retained invalid dry state after extreme finite input.
A2 is an isolated candidate, not an authorized production correction.

The maintainer must explicitly accept the loss of H4's dry-filter CPU saving:
whole-engine instructions rise about **15.3% / 27.4% / 36.9%** at 48 kHz/128 with
2/3/4 bands. Those are measured instruction ratios, not universal CPU or DAW
instance-count promises. No general budget is invented. Wall-clock measurements
have substantial shared-machine noise, detailed below. A subsequent assignment
must implement the approved choice with the new mandatory regression and complete
final-head validation. This round supplies a candidate, not production approval.

## Contract and original H4 decision

ADR-0005 requires A(dry) from the same crossover trajectory for phase-matched
partial Mix, with clean dry at Mix=0. ADR-0004 owns the existing Mix smoothing
and Enable crossfades. The 0.8.9 optimization in commit
`36e69f9b39632e7d5fc039f5c949bf07880b3265` skips the dry bank and m=1 reblend
when Mix is settled at exactly one, Match is off/not engaging and Enable/Bypass
are not fading. Historical attribution reported ~20 us on the shipped default;
that dated number is retained, not treated as this machine's current saving.

The accepted historical Class-B changes are the omitted m=1 float reblend
(measured <=2.4e-10 on its original dump) and a clean-dry Measure reference while
gated (near-crossover Match-engage offset measured up to .53 dB, converging
through its measurement window). Candidate A preserves both choices. Keeping
rings written and coefficients synchronized does **not** maintain recursive
LR4 ladder state. Historical approval did not establish approval of the newly
measured re-entry transient.

## Reproduced N2-01 and independent references

At 48 kHz/128: Haas Amount 0, Width 1, MB enabled, two bands, unit band widths,
20 Hz crossover/tone, mono amplitude .5, Mix 1 for three seconds then target .5;
Match, Mono Maker, Bypass and output trim inactive. The gate executes for 1,116
observed idle blocks: the wet filter state moves, the dry filter state does not,
and the production loudness reference carries clean dry. Re-entry consumes the
previously unused dry branch and its state. This is execution evidence, beyond
selected parameter values or output finiteness.

The oracle uses a separately, continuously executing production MultibandWidth
for unblended A(dry) and a full-wet production engine for wet samples. It explicitly
reproduces downstream Mix float rounding. A unit-width, continuously full-wet
engine independently confirms phase equivalence (healthy 20 Hz difference <=
2.98023224e-8). Neither reference repeats the cold-bank gate. Non-neutral width
cases use the separate warm dry module rather than assuming wet equals dry.

| Witness | Baseline peak, first 960 samples | RMS error | Short-window level error | A / C error vs independent module | Last baseline error >16 float eps |
|---|---:|---:|---:|---:|---:|
| 20 Hz split/tone | .136108398438 | .101481723183 | -2.56591928782 dB | 0 / 0 | sample 6513 (~136 ms) |
| 50 Hz split/tone | .0545571446419 | .0298264175759 | -.470427233122 dB | 0 / 0 | sample 2596 (~54 ms) |

The baseline recovers: the .5–.6 s tail window matches the warm reference within
the healthy rounding envelope. This is a transient reconstruction error, not
persistent corruption, measured stationary response, a comb-response plot,
listening evidence or native-DAW incidence.

The 34-case risk matrix includes 20/50/100/800/3000 Hz; 2/3/4 bands; 44.1/48/96/192
kHz; 32/64/127/128 blocks and variable lengths down to one; .05/3/10-second idle;
Mix 0/.01/.049/.05/.25/.5/.9/near-one; repeated dips/reversals; broadband noise,
unequal stereo, non-neutral widths, crossover motion, topology edits, Enable and
Bypass toggles, Match activation/deactivation and actual x4 wrapped Drive.
A/C peak error excluding the Match policy comparison is <=5.96046448e-8.
Every recorded A/C sample in the .4 s re-entry windows is identical. Results
include complete .6 s error/recovery metrics. No Cartesian-product coverage is
claimed. Static 20/50 witnesses carry the explicit gate/state premise checks;
zero gate counters in excluded movement/OS/topology rows mean unmeasured, not
that those configurations never qualified for H4.

The Match interaction's full-wet twin is not a valid sample-equality oracle:
one engine remains on H4's clean reference while the Mix-dipped engine measures
A(dry). It shows a .00323051214 peak difference even for healthy A/C. This is
reported separately as an interaction/accepted-reference-policy comparison;
it is not counted as residual reconstruction failure or reclassified as a new
production finding. A and C have identical recorded outputs and metrics there.

### Aligned delay history is a separate prerequisite

The initial filters-only diagnostic fixes the zero-latency witness but leaves
clean data in the aligned ring while gated. At x4 latency, its first-window error
is **1.63614749908e-5**, confined to early re-entry; A/C reduce this to
5.96046448e-8 by maintaining aligned ring contents. The H4 loudness reference
still reads the separate clean ring. Callgrind shows actual Oversampling
processSamplesUp/processSamplesDown execution with positive call counts for the
x4 Drive case; a selected factor alone is not the premise.

## Candidate comparison

| Candidate | Correctness / normal behavior | Cost / worst work | State, memory and complexity | Approval / disposition |
|---|---|---|---|---|
| A: existing bank + aligned history always warm during H4 | Resolves measured re-entry errors; retains exact-wet shortcut and clean Match reference | +15/27/37% Ir in 2/3/4-band reference rows; ordinary existing dry processing every block, no replay spike | Existing filters/buffers only; two local consumer choices; no new allocation, locks or thread path | Ordinary cost/correctness reference only; unguarded recovery is rejected below |
| A2: A plus exceptional-only dry-channel recovery | Ordinary matrix matches A; earlier stress clears observed invalid state, but a new engine witness leaves hidden invalid state | +15.3/27.4/36.9% Ir, only .13–.27% more than A in measured OS-off rows; bounded per-block scan, exceptional fixed state resets | Existing buffers; new dry/channel-only reset helpers, no wet/raw/other-channel reset | Not recovery-complete; continue investigation, Proposed owner decision; production unchanged |
| C: same state updates, discard unused dry output at latency zero; A at nonzero latency | Same recorded sample outputs/state behavior as A; OS needs history output | Saves <.6% whole-engine instructions against A; +.1% versus A on measured OS row | Adds discard mode/default parameter/branches to MultibandWidth; no new buffers | Same unused-state recovery risk until guarded; small saving does not justify the extra interface |
| B: finite input/coeff replay on re-entry | No general exact state equivalence established; 4096 samples fail at low cutoffs/high rates | Potentially hundreds of normal-block equivalents on one unannounced Mix block | History must include coefficient trajectories and topology/fades, not input alone | Reject fixed-short-history correction; larger bounded approximation requires separate proof and RT assessment |
| C shortcut: substitute dedicated allpasses / copy wet state | Does not preserve existing dx s3/s4 state or non-neutral wet/dry history | A different representation could save work, but no general equivalence follows | LR4Xover explicitly forbids mixing full and first-section-only processing on one instance; wet can have Drive/Width/delay information loss | Not a viable local correction; core-maths replacement needs ADR/architecture review |
| D: restrict skip to one band | One band has no recursive crossover, so skip is safe there; affected 2–4-band path becomes A | No independent multi-band CPU saving beyond A | Same graph and timing; no hidden warm-up window | Safe restriction reduces to A on affected configurations |
| D: mask cold output / delay Mix response / lengthen smoothing | Does not establish phase/state equivalence for an unannounced Mix change | Moves cost/error or changes response timing | Changes accepted transition/automation behavior | Rejected for this maintenance correction without an explicit contract/architecture decision |
| E: retain current H4 | Retains confirmed supported low-frequency transient and CPU saving | Current baseline | No implementation change | Requires explicit limitation/phase-guarantee decision; historical H4 approval is insufficient |
| F: defer | Production stays unchanged and finding stays open | No new production cost | Keep reproducer and Proposed ADR | Current disposition until owner chooses; not evidence that defect is fixed |

A's source changes no DSP node/order, reported latency, thread model, parameter
semantics, formats, dependency or persistence. The explicit owner decision is
required by this assignment's H4 tradeoff boundary. A new allpass representation,
changed Mix timing, latency or state/thread model would also trigger the applicable
ARCHITECTURE_REVIEW_GATE / ADR_POLICY requirements. Proposed ADR-0074 is not Accepted.
The new dry-only recovery proposal itself remains subject to owner review. No
new reset strategy or the separately deferred N2-04/05/06/07 correction is
implicitly authorized. Extreme/non-finite recovery remains an implementation
validation requirement; the ordinary-signal matrix does not establish every
inactive-state recovery guarantee.

## Performance assessment

INTEL XEON PLATINUM 8573C, managed Linux 6.18.57, Debian trixie GCC14.2 / Release
C++23 / Haswell / -ffp-contract=off; exact JUCE pin above. Comparison sources
share the original baseline and differ only by preserved diagnostic patches.
Buffers/input are prepared outside timing. Each repetition warms one second,
then measures at least 400 blocks (or ~one third second) and 20 re-entry blocks.
72 configurations per run: 48/192 kHz, 32/256 blocks, 2/3/4 bands, settled/moving
crossovers, full/partial Mix and x4 Drive. Five repetitions per run; order
baseline-A-C-C-A-baseline gives ten measurements per cell per variant.

Whole-engine wall-clock median A/baseline ratios range .986–1.636 over full-wet
OS-off rows (median ~1.206); C .914–1.583 (median ~1.208). Values below one and
large variation are scheduler/measurement noise, not proof that extra bank work
is free. Median within-cell coefficient of variation is ~16.3% baseline, 12.6% A,
13.8% C; maxima ~81.8%, 38.7%, 90.9%. These data are not a native CPU budget or
universal instance-count estimate.

Worst observed wall blocks: baseline 29.878 ms, A 4.473 ms, C 35.881 ms. Worst
observed re-entry blocks: 4.861 / 2.704 / 1.427 ms. These include descheduling on
a shared machine; neither their ordering nor their absolute values establishes
DSP deadline safety or violation. No algorithmic replay work exists in A/C:
re-entry uses the existing partial-Mix processing and smoothing loops.

Valgrind 3.24 Callgrind counts only setParameters+process, 256 measured blocks,
excluding warm-up/input generation. TLS trust was supplied from the managed
combined CA bundle after initial missing-trust errors; no TLS check was disabled.
Only valgrind and libc debug symbols were installed; zero toolchain/runtime
packages were upgraded. Instruction counts remove scheduler noise and support
the extra-work conclusion, without predicting cycles or native CPU percentage.
[Full instruction table](h4-dry-state-0102/instructions.md), raw callgraphs and
commands are preserved. At four bands/48k/128: baseline 106,363.80 Ir/block;
A 145,342.81; C 144,952.80; settled partial Mix 147,391.81. A retains the
m=1 recombination saving, but loses the recursive dry-bank saving. C saves only
~390 Ir/block against A in that row. No formal memory/CPU budget is introduced.

### Bounded replay experiment

The isolated LR4 experiment replays the exact retained input **and each cutoff**,
then compares the first 960 outputs with a continuously active production filter.
At 20 Hz / 192 kHz, 4096 retained samples still give .095760434866 peak error;
16384 give .000534832478. At 48 kHz, 4096 give .000532865524. Some 16384/65536
rows converge to identical float output on these particular stimuli; that is not
proof of arbitrary state equivalence. The real recurrence retains an A^N*s0 term;
rounding can erase it for a particular finite sequence, not by assumption for
all supported histories/coefficient trajectories.

65536 samples of stereo input plus three cutoff trajectories already require
~1.25 MiB before bank/topology/fade metadata. The measured single-channel,
single-filter replay costs roughly .8–3 ms in the retained run, versus roughly
2 us for 128 ordinary filter samples; those illustrative kernel timings are not
a measured production replay deadline. This experiment rejects a fixed short
replay shortcut, not every possible future state-reconstruction algorithm.

## V2-01: repair the claim-to-evidence boundary

The old live claim was **“Both dry delay rings stay warm; re-engage is comb-free
(testDryAlignGateRecomb)”**. Test 25 uses default crossovers/broadband noise,
10 transition blocks of 256 frames and trRatio>.90; its settled ratio>.95 also
remains valid for its aggregate metric. Both assertion conditions and the original
configuration are retained. The test's claim text and live architecture/source
comments now describe what that metric establishes. Historical commit/report
measurements are not rewritten.

Applying the same 2560-sample RMS criterion to the independent 128-frame 50 Hz
witness gives **.981325059961**, passing despite .05455714 first-window peak
error. The unchanged Test 25 did not execute that scenario. An aggregate pass
therefore cannot establish sample-level or worst-frequency reconstruction.

The new optional oracle has eight assertions: H4 clean-reference/active-wet-bank
premise; actual re-entry/state consumption; first-960-sample error; subsequent
recovery, for both 20 and 50 Hz witnesses. The threshold is 16 float eps
(1.90734863e-6) for <=.5 normalized stimuli, versus <=1.1921e-7 healthy independent
controls and the known MS/interpolation rounding chain. It is a conservative
phase-reference envelope, not a replacement for N2-03's exact bit contract.

| Variant | Assertions / failures | Exact changed assertion |
|---|---:|---|
| original production H4 | 8 / 2 | first-960 reconstruction error, both witnesses |
| A | 8 / 0 | warm independent reconstruction |
| C | 8 / 0 | same |
| guarded A2 | 8 / 0 | same, with exceptional unused-dry recovery |
| “never enter H4” mutation | 8 / 2 | H4 execution premise, both witnesses; numeric errors are zero |

Thus the regression cannot pass merely because H4 never executed. The baseline
also proves frozen dry state alongside moving wet state. **This failing-on-main
oracle is nonshipping research, not a new failing mandatory CI test.** Add it to
the mandatory suite with an approved N2-01 production correction. Documentation
scope correction can be reviewed independently now.

## Compatibility, remaining risks and next action

Review the documentation/Proposed ADR separately from N2-02 PR #238 and N2-03
PR #239. Those two production corrections are independently reviewable and have
no H4 optimization change. Keep all PRs open/unmerged and auto-merge off. The
combined validation branch contains only those two production corrections;
H4 candidates remain separate diagnostic sources.

The N2-01 decision remains P2. No audibility, native DAW incidence, hardware
performance, universal deadline, full extreme-input state guarantee or accepted
owner approval is inferred. N2-04–07 retain confirmed P3 status and their original
witnesses/follow-ups. Candidate review must preserve raw-history/self-heal
ownership and existing D-01/T-01/D-02/R-01 protections. Final production validation
must include recovery and actual execution premises, not output finiteness alone.

Next: owner chooses the guarded A2 direction's measured CPU/recovery tradeoff,
an explicit limitation, or deferral. If approved, implement bank + aligned-history
maintenance and scoped recovery together, retain H4's wet/measurement policies,
integrate the independent regression, then run all prescribed final-source checks.
Do not ship the unguarded A/C or filters-only diagnostics. No release work follows
from this packet.

## Reproduction and durable evidence

Start with [the portable README](h4-dry-state-0102/README.md). It includes clean
baseline builds, candidate patches, the correctness/performance/count/replay
commands, exact source/binary/dependency identities and raw gzip evidence.
The original absolute-path command ledgers remain historical; the supplied
builders derive new commands from a fresh local CMake/Ninja build.
The versioned benchmark v1 source matches the original wall-clock run; v2 adds
wet-state/recovery observations without changing benchmark operations.

## Recovery amendment — the simple warm-bank diagnostic is insufficient

A later bounded experiment found a candidate-only state-recovery deficiency,
not a new confirmed defect in main. It deliberately constructs finite extreme
stereo samples from the signs of a production LR4 impulse response, amplitude
1.68e38, with opposite channels. Even their MS subtraction remains finite
(3.36e38). Global Width zero makes the wet path finite. Tests use 48 kHz/512,
two bands and 20/800/8000 Hz crossovers.

The simple A diagnostic leaves **eight non-finite dry integrator values** after
the burst and after **120 subsequent valid blocks (61,440 samples, 1.28 s)**.
Wet audio remains valid (measured peaks .2111–.21995 during the DC return).
The next Mix dip zeros **512/512 left-channel samples** and resets that dry state.
Original gated production has zero invalid dry states and no wholly-zero dip
block for the same sequence. This distinguishes output finiteness from internal
recovery, and is a reason not to ship a successful ordinary-audio diagnostic as-is.
The actual final-wet guard and candidate-only dry guard are traced separately;
trace variants perform observation only and are not performance binaries.

The A2 diagnostic scans the otherwise-unused dry output with the existing
exponent-mask detector pattern, scrubs exceptional samples before aligned-ring
writes and clears only that channel's dx/dax states in both banks. It preserves
wet filter/width/fade state and the valid other-channel, clean/raw/OS histories.
Earlier aligned-ring contents are finite by the guarded-write / existing
consuming-path self-heal invariant. That invariant, latency re-entry and unrelated
state preservation require explicit validation before a production PR.

A2 stress rows have zero retained invalid states and zero wholly-zero dip samples
at all three crossovers. Completed ordinary-matrix, execution-premise,
unaffected-channel, negative-control and guarded-cost measurements follow below.
This dry-only guard is a candidate design, not N2-04's conditioned-history
correction or N2-05/06/07 implementation. The A cost above is the unguarded
warm-bank reference; A2's separate final measurements include its guard.

### A2 executed recovery and cost observations

The traced A diagnostic executes no final-wet heal during the extreme stimulus
or 120 valid blocks, then **exactly one final-wet heal at the Mix dip**. The
traced A2 diagnostic executes **four local dry heals per channel** (one for each
extreme block), no final-wet heal at any stage, and retains zero invalid states.
This proves the relevant paths and consumption, rather than inferring them from
zero samples or selected parameters. All three 20/800/8000 Hz rows discriminate.

The additional valid-channel oracle uses a continuously valid right input and
finite Drive output while only left dry processing overflows. A separate cloned
production module advances the right dry state from the exact pre-block state.
A2 must preserve all right dx/dax integrator bits. Its targeted mutation clears
both dry channels after only the left guard fires; this is a recovery ownership
negative control, not a proposed implementation. Results are in the retained
trace ledgers. The known extreme Drive cancellation finding N2-07 remains
unfixed; this oracle needs only finite wet execution, and does not assert the
Drive transfer function.

A2's measured instruction ratios to original H4 at 48 kHz/128 are **1.15324,
1.27405, 1.36858** for 2/3/4 bands. Across the selected OS-off count rows the guard
adds **.13–.27%** over A; actual x4 Drive is 1.15038 times original versus A's
1.14952. These counts exclude observer instrumentation. Wall-clock guarded and
combined/final-source results are separate; no unmeasured deadline is accepted.

The valid-channel control passes **4/4 exact right-state comparisons** with A2;
the reset-both mutation passes **0/4**, while finite-input/wet premises, four
left dry heals, zero right dry heals and zero final-wet heals remain identical.
Only the state-preservation condition changes, so the mutation is sensitive to
the claimed recovery ownership. Trace variants are excluded from performance.

The two completed ordinary DSP corrections also pass a fresh disposable combined
build, finally at `7764df5c2d02eee7a11c475fcca93097f9559dee`: **5,514 DSP / 16,555 State**
checks. Combined-source targeted mutations independently preserve sensitivity:
restoring N2-02 preservation fails 316/720 controls while endpoints pass 292/292;
restoring N2-03 arithmetic fails 66/292 endpoints while controls pass 720/720.
This combined source contains no H4 production correction. Separate guarded
combined-source diagnostics are identified explicitly in the final delivery ledger.

## Final candidate measurement boundaries

At four bands the skipped stage executes six full LR4 splits plus four
first-section compensation allpass calls per sample; two/three bands execute
2/4 full splits plus 0/2 compensation calls. The historical “6 LR4” shorthand
omitted those compensation calls. No allpass or signal graph is replaced.

Guarded A2 adds no state/buffer allocation. It preserves the existing dry-filter
math, updates all bank/crossover/width clocks through normal processing, and only
clears affected dry-channel integrators on an observed exceptional output.
Finite derived samples are preserved; clean/raw/OS histories and the other dry
channel are not cleared. Its normal 34-case matrix passes the same eight-point
low-frequency oracle. The combined A+B+A2 diagnostic passes **5,514 full DSP
checks** using current-source dependent DSP objects; this is explicitly not an
approved production integration or native DAW validation.

A dependency-consistent five-TU rebuild repeats all **12 A2 Callgrind counts
identically**, avoiding reliance on a header-stale object. The first three-TU
measurements remain historical records. Current replay recompiles Engine,
MultibandWidth, MonoMaker, SoloMonitor and the probe to cover the added LR4
channel reset declaration consistently. JUCE objects retain their exact pin.

A second balanced wall-clock comparison (baseline-A2-A2-baseline; ten samples per
cell) reports A2/baseline full-wet OS-off ratios **.859–1.700**, median **1.123**.
A2 within-cell CV median is **14.6%**, maximum **56.4%**; worst observed wall block
**11.139 ms**, re-entry **2.616 ms**, including shared-host scheduling. This
ordering is not a deadline or native CPU guarantee.

The Linux thread-CPU batch check removes off-CPU waiting and includes identical
pre-generated input copies plus setParameters/process for both variants. It
also remains variable: full-wet OS-off median ratios **.992–2.088**, median
**1.407**, with within-cell CV medians **15.0% baseline / 20.0% A2**, maxima
**47.5% / 101.3%**. These measurements do not support a precise universal CPU
percentage. Instruction ratios establish extra algorithmic work more reliably;
cycle cost, SMT/host interference and native instance counts remain unestablished.
The owner may accept that bounded extra work or request a focused benchmark on
representative hardware before approving production. A formal budget project is
not proposed.

The wall/CPU-time stimuli use 20/800/4000 Hz stationary splits (moving low split
starts toward 100 Hz). Instruction attribution uses a 100 Hz low split and the
original remaining defaults. Each variant within a comparison uses identical
parameters/input/flags; absolute times are not cross-compared between harnesses.
Replay timing recomputes every recorded cutoff, including fixed rows; an
optimized fixed-coefficient replay could hoist that work. The prototype times
are illustrative, not a lower bound on every possible replay algorithm.

### Proven sustained gliding and overlapping bank fade

The original low-frequency "moving" rows mostly hit the crossover epsilon snap
at each block; those rows alone do not prove sustained per-sample coefficient
work. A separate trajectory probe moves a 1000 Hz split by a smooth half-octave
trajectory, below the 1.5-octave bank-fade threshold. It checks actual live cutoff,
bank fade state and a conservative residual-distance/float-rounding bound to
prove that the per-sample glide continues throughout the qualified blocks.
At 48/192 kHz it proves 172,032 / 598,816 gliding samples, zero bank-fade blocks.
Baseline first-window peak errors are .000812493265 / .000635564327; A2 errors
are zero against the independent continuously warm reference.

A distinct 20-to-800 Hz jump four milliseconds before the Mix dip overlaps an
actual bank fade for five observed blocks. Baseline peak errors are .000535443425
without wrapping and .001080095768 with actual x4 Drive. A2 gives zero and
5.96046448e-8 respectively. This checks both banks' dry progression while fading,
rather than inferring it from a selected topology. All four rows recover within
the measured tail window; `trajectory_probe.cpp` and raw streams retain premises.

Instruction attribution of the sustained glide excludes its observer from the
collection region. It proves 32,768 / 32,768 / 8,192 / 65,280 gliding samples in
the following 256-block measurements (the final row has one unqualified block).

| Rate / block / bands | Baseline Ir/block | A2 Ir/block | A2 / baseline |
|---|---:|---:|---:|
| 48k / 128 / 2 | 87,240.32 | 99,793.33 | 1.14389 |
| 48k / 128 / 4 | 120,039.41 | 160,523.42 | 1.33726 |
| 192k / 32 / 4 | 32,665.68 | 42,885.70 | 1.31287 |
| 192k / 256 / 4 | 233,173.78 | 313,999.82 | 1.34663 |

These are additional bounded-work counts, not native timing or deadline results.
No replay burst is introduced at re-entry. The A2 exception path scans the block,
scrubs only invalid derived samples and performs at most 48 integrator writes per
failed channel; it allocates no new buffer or history. Future production review
must still validate all recovery/latency interactions and prescribed sanitizer
and real-time gates on the approved implementation.

## Independent follow-up — hidden invalid state (2026-11-14)

An isolated call to the actual LR4 filter establishes that a finite output sum can coincide with an overflowed recursive state at the end of a sample; a zero next input then yields non-finite output. See [the reproducible witness and its precise limitations](h4-review-followup-0102/README.md). This invalidates the general inference that A2's finite unused-output scan proves all dry-filter state finite. A later engine-level extreme-input witness confirms a hidden invalid dry state at a gated block boundary while monitored dry and wet outputs are finite. The next block is zero even if Mix remains full wet or the left dry state is reset, so the observed zero block is not isolated to dry re-entry. Bank/fade/latency and repeated-recovery coverage remains incomplete. The earlier observations and costs are unchanged for their recorded cases. Until that work is done A2's recovery completeness is unproven, and its previous recommendation is conditional on resolving this newly identified gap. Continue targeted investigation before recommending a production candidate; retaining H4 with its documented limitation or deferring remains available to the owner. A2 acceptance would explicitly accept both its measured costs and the unresolved recovery risk, not a proven recovery guarantee.


A separate diagnostic adds per-channel state scans across both dry banks. It catches the hidden-state witness, but the next zero block still occurs and the candidate has neither a full oracle nor measured cost. See the [engine witness, controls and experimental patch](h4-review-followup-0102/README.md). It is not an approved implementation or a demonstrated complete recovery design.
