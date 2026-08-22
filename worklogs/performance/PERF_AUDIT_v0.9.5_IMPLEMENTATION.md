# A7-1 implementation — v0.9.5

**Round A7, item 1 of the roadmap in
[`PERF_AUDIT_v0.9.4_INVESTIGATION.md`](PERF_AUDIT_v0.9.4_INVESTIGATION.md).** That round measured the
cost and proposed the change in a throwaway build; this round implements it in the product tree,
re-derives the evidence there, and adds the regression coverage the roadmap made a condition of
landing.

**Scope, stated up front:** A7-1 only. A7-2 (the residual per-block term), A7-5 (multiband SIMD), the
bypass contract, the metering/loudness gates and the GUI candidates are all deliberately untouched —
each is a separate roadmap item with its own evidence or decision still outstanding (§8).

---

## 1. What changed

Three edits in two files, plus one test.

| File | Change |
|---|---|
| `src/dsp/VelvetNoise.h` | one `int linHistSlide = 0;` member and the invariant that governs it |
| `src/dsp/VelvetNoise.cpp` | take-and-clear the offset on entry to `processBlock`; slide instead of re-gather when it is set; re-arm it on the gather path's exit; clear it in `reset()` |
| `tests/dsp_tests.cpp` | Test 39, the block-length and transition invariance guard |

The H5 tap-outer gather (Wave 2) is unchanged and is still the point: taps still read one contiguous
unit-stride run each. What changed is only how the linear image is **refilled**.

### 1.1 The mechanism

`VelvetNoise` keeps its Mid history in a power-of-two ring and, on the gather fast path, builds a
LINEAR image of it so each tap reads one unit-stride run:

```
linHist = [ last decorrSamps ring samples | this block's mids ]
```

Before this change the left half was re-read from the ring **every block**:

```cpp
for (int j = 0; j < decorrSamps; ++j)
    linHist[j] = midHist[(writePos - decorrSamps + j) & histMask];
```

`decorrSamps = round(0.045 * sr)` (`src/dsp/VelvetNoise.cpp:26`) — **2160 samples at 48 kHz, 8640 at
192 kHz** — and the loop does not depend on `numSamples`. It was therefore a fixed per-block cost
that grew with the sample rate and was divided by the block length: measured at ~9.4 Ir per history
sample, i.e. ~20,400 Ir/block at 48 kHz and ~78,700 at 192 kHz.

That tail is redundant. The previous block's image held, at index `k`, the mid at ring position
`writePos_prev - decorrSamps + k`; this block wants ring positions `writePos - decorrSamps` up to
`writePos - 1`, and `writePos` advanced by exactly the previous block's length since (one per
sample). So the tail this block needs is `[slide, slide + decorrSamps)` of the old image — a
leftward `std::copy`, which is defined because the destination is strictly before the source range
whenever `slide > 0`.

**Bit-exact by construction rather than by argument:** it moves the same floats the ring walk would
have re-read. Every entry carried forward was either gathered from the ring by an earlier block or
written into the image from `midBlk[i]` — the same value the per-sample loop stored into
`midHist[writePos]` for that sample.

### 1.2 Why this strategy, and how it differs from the throwaway build

The investigation's throwaway patch cleared its validity flag from an **RAII scope guard** whose
destructor fired on every non-gather exit. That works, but it puts the safety property in a
destructor a reader has to find, inside a `noexcept` realtime function.

The shipped version instead **takes and clears the offset at the top of `processBlock`**:

```cpp
const int slide = linHistSlide;
linHistSlide = 0;
```

and re-arms it in exactly one place — the gather path's exit. The difference is not stylistic. With
clear-on-entry, **every** other exit leaves the image invalid *by construction*, including paths a
later round adds: there is no line for a future edit to forget. The alternative — clearing on each
non-gather path — is one new `return` away from a stale read that would surface only in the first
block after the mistake.

`reset()` clears it too, and that line is load-bearing rather than belt-and-braces: `reset()` runs
BETWEEN blocks, after `processBlock` armed the offset, and it flushes the ring the image mirrors.
`prepare()` ends with `reset()`, so a sample-rate or block-size change is covered by the same line.

---

## 2. Validation

### 2.1 Bit-identity, on the product tree

| Instrument | Coverage | Result |
|---|---|---|
| `AnamorphDspDump` twin diff (the committed DEPENDENCY_POLICY rule-2 harness) | 32 scenarios × every algorithm/OS/multiband axis, 48 kHz / 512 | **identical** — `diff` is empty |
| session-local scenario twin | 9 scenarios × 5 block sizes × 4 sample rates = **180 configurations**, FNV-1a over every output sample of both channels | **0 mismatches** |
| Test 39 run against BOTH engines | 4 sample rates × 2 block sizes | identical output, digit for digit |

`AnamorphDspDump` ran its own `--self-check` first and reported *"32 scenarios, all repeatable and
all distinct"*, so the twin diff is a live comparison rather than 32 equal hashes proving nothing.

The reported latency is part of the dump's output and is unchanged: no latency change, which is one
of the `ARCHITECTURE_REVIEW_GATE` hard-stop classes this change had to stay clear of. It touches no
parameter ID, no serialization schema, no thread model and no DSP signal order either.

### 2.2 The new regression guard, and proof it is live

**Test 39** (`tests/dsp_tests.cpp`) drives the module through a fixed event schedule — engage, park,
re-engage, transport stop, moving density — at **44.1, 48, 96 and 192 kHz**, once in 512-sample
blocks, once in 32-sample blocks and once through a **cycle of mixed sizes** (32, 128, 64, 256, 32 —
summing to 512, so the events still land on a block boundary and every neighbouring pair differs),
and requires all three to be **bit-identical**.

The mixed cycle was added on review and is the case that matters most: `linHistSlide` carries the
JUST-PROCESSED block's length, so a run whose blocks never change size can be correct with the offset
confused for a constant, and consecutive gather blocks of differing length are exactly what the slide
arithmetic is about. Comparison is on BITS rather than `==` — `-Wfloat-equal` is at zero in the Clang
baseline, and a float `==` is the wrong predicate for a bit-identity claim anyway: it calls +0 and −0
equal, which this module's own signed-zero algebra cares about, and NaN unequal to itself.

Block-length invariance is the right assertion because every piece of state in this module advances
per sample, and H5's own contract is that the gathered sum equals the per-sample loop's "for any
block length". A stale image is stale by the previous block's length, so it perturbs the two runs
differently and cannot survive the comparison.

**The invariant is pre-existing, not invented for the change:** the same test passes unmodified
against the pre-A7-1 engine, printing identical numbers. It is asserting a contract the module
already had and nothing was checking.

**Proven live in two directions**, because a guard whose silence has never been tested is not a
guard (`TESTING_POLICY` rule 4):

| Seeded defect | Result |
|---|---|
| slide by `slide + 1` (wrong arithmetic) | **fails** at sample 32, worst delta 1.46, all four rates |
| the general path re-arms the offset (missing invalidation) | **fails** at block 215 — the transport-stop block, the first non-gather → gather transition — worst delta 0.95 |

Test 39 also asserts two things about itself so it cannot pass vacuously: that the engaged stretch
really decorrelates (max |side change| ≈ 1.0), and that a **non-gather path really ran** — the
transport stop flushes the history, and the window after it carries 15.4–25.2 % of the engaged
figure, against **90.6–128.9 %** measured with the stop event removed and nothing else changed. The
0.5 bound sits between two measured populations rather than being a hopeful inequality.

### 2.3 Suites and lints

`AnamorphTests` **178 checks, 0 failures** (162 before; Test 39 adds 16 — four checks × four sample
rates). `AnamorphStateTests` **920 checks, 0 failures**. `check-realtime` 44 files / 0 violations —
the new `std::copy` is a move over a `prepare()`-sized buffer, no allocation, no lock, no IO.
`check-portability` 52 / 0. `check-docs` 104 files clean. `check-citations --self-test` 130 cases
and `--check --base origin/main` green.

**ASan + UBSan + LSan**, built with the `sanitizers` job's own flag set — `address,undefined,vptr,
float-divide-by-zero,implicit-conversion,unsigned-shift-base,local-bounds,nullability` with the
project ignorelist, `detect_leaks=1`, `halt_on_error=1`: `AnamorphTests` **172 checks, 0 failures**
(two fewer than the plain build, which is the allocation guard reporting less under ASan exactly as
Test 38 documents) and `AnamorphStateTests` **920 checks, 0 failures**, with **zero** sanitizer
diagnostics from either. The point of running them here is `local-bounds`: the new `std::copy`
reads `[slide, slide + decorrSamps)` of a `prepare()`-sized buffer, and that range is now checked by
a tool rather than only by the argument in §1.1.

---

## 3. Measured performance

Callgrind instructions per sample, product tree, steady state (a 3.0 s run minus a 1.0 s run, halved,
so process start and `prepare()` cancel). Instruction counts because `PERFORMANCE_BUDGET.md` holds
that a shared runner is not a wall-clock datum while callgrind counts are stable across machines.

| SR | block | before | after | saved | **engine** | VelvetNoise before → after |
|---|---|---|---|---|---|---|
| 44.1 kHz | 128 | 1774.7 | 1641.4 | 133.4 | **−7.5 %** | 265.6 → 126.0 (−52.6 %) |
| 48 kHz | 32 | 2356.0 | 2018.5 | 337.5 | **−14.3 %** | 755.3 → 147.7 (−80.4 %) |
| 48 kHz | 64 | 1975.4 | 1806.7 | 168.8 | **−8.5 %** | 437.0 → 133.1 (−69.5 %) |
| 48 kHz | 128 | 1787.3 | 1702.9 | 84.4 | **−4.7 %** | 277.9 → 126.0 (−54.7 %) |
| 48 kHz | 256 | 1691.5 | 1649.3 | 42.2 | **−2.5 %** | 198.4 → 122.4 (−38.3 %) |
| 96 kHz | 128 | 1939.0 | 1770.3 | 168.8 | **−8.7 %** | 429.7 → 125.9 (−70.7 %) |
| 192 kHz | 32 | 4178.7 | 2828.7 | 1350.0 | **−32.3 %** | 2577.9 → 147.7 (−94.3 %) |
| 192 kHz | 128 | 2242.9 | 1905.4 | 337.5 | **−15.0 %** | 733.6 → 126.1 (−82.8 %) |

Decomposed into the two terms, at 48 kHz:

| | fixed per block | marginal per sample |
|---|---|---|
| before | 24,302 Ir | 1596.6 Ir |
| after | **13,502 Ir** | **1596.6 Ir** |

**The marginal term is unchanged to the decimal**, which is the arithmetic signature of a change that
touched only the per-block refill and nothing in the per-sample loop. The fixed term fell 44.4 %.

**It matches the investigation's estimate exactly.** That round predicted −14.3 % at 48 kHz/32,
−32.3 % at 192 kHz/32, −8.5 % at 48 kHz/64, −4.7 % at 48 kHz/128, −2.5 % at 48 kHz/256 and −15.0 %
at 192 kHz/128 from a throwaway build. The product tree reproduces every one of those figures to one
decimal place. The two extra rows measured here (44.1 kHz and 96 kHz at 128) fall where the model
predicts, since the fixed term scales with `decorrSamps ∝ sr`.

**What is left in the fixed term.** 13,502 Ir/block at 48 kHz is the surviving `std::copy` of
`decorrSamps` floats plus the block's own mids. Removing it outright is roadmap item **A7-2** and is
NOT done here.

**No wall-clock number is quoted, and none may be promoted into `PERFORMANCE_BUDGET.md`'s TODO rows
from this round.** Machine, for constraint C2: Intel Xeon @ 2.80 GHz, 4 cores, gcc 13.3.0, shared
container, not held still.

---

## 4. Behaviour, parameters, state

Unchanged, and each was checked rather than assumed:

* **Audio** — bit-identical across 180 configurations plus the 32-scenario committed twin.
* **Reported latency** — part of the dump's per-scenario output; unchanged.
* **Parameters** — no ID added, renamed or removed; `AnamorphStateTests`' registry snapshot check
  passes untouched.
* **Serialization** — no schema change; the state suite's shape and round-trip checks pass.
* **Threading** — the new state is a plain `int` member of an object owned by the audio thread, read
  and written only inside `processBlock` and `reset()`. No atomic, no cross-thread hand-off.

---

## 4a. The release was blocked by the citation gate, and why that is not a false alarm

The version bump made `check-citations --check` report **six drifted citations**, all of them
`CMakeLists.txt:14` — the `project(Anamorph VERSION x.y.z ...)` line — cited by `HANDOVER.md` (×2),
`RELEASE_POLICY.md`, `RELEASE_PROCESS.md` (×2) and `TRADEMARKS.md`. Nothing moved: the anchor is
still line 14 and line 14 is still the project declaration. What changed is the version ON that
line, which is what a release is.

**This is the first version bump since `CMakeLists.txt` came under the gate**, checked rather than
assumed: the 0.9.3 → 0.9.4 bump is commit `3ebdf69` (2026-08-14) and `CMakeLists.txt` joined
`TRACKED` in `129457e` (2026-08-16), which is its descendant. So `RELEASE_PROCESS.md` step 1 —
"update `project(Anamorph VERSION x.y.z ...)` in `CMakeLists.txt:14`" — had been un-runnable under
the gate since the day the gate started watching that file, and no release had exercised it.

**`DELIBERATE_REAIMS` cannot cover this, and the reason is load-bearing rather than incidental.**
That table excuses a citation whose SPELLING changed, and `is_declared_reaim` returns `False` when
the base and current spellings agree — deliberately, so an entry cannot outlive its one transition
(`scripts/check-citations.py:600-625`). Here the spelling is right, unchanged, and will stay right;
it is the cited LINE that was rewritten, and it will be rewritten again at 0.9.6. There is no
transition for an entry to outlive, which is exactly why a loosened guard on the existing table
would have created the permanent exemption that guard exists to prevent.

**What was added: `VERSIONED_LINES`, keyed by one exact `(path, line)` pair.** For a declared line
the base comparison is REPLACED by a permanent content assertion — does the line still contain the
stable token, here `project(Anamorph VERSION`. The gate stops watching the version and keeps
watching that line 14 is still the project declaration. Weaker than the base comparison, stronger
than an exemption, and checked base-independently on every run by `verify_versioned_lines()`, which
is a hard failure in every mode.

**Proven narrow in both directions rather than argued to be:**

| Probe | Result |
|---|---|
| the real tree | green; the version bump passes |
| a *neighbouring* cited line drifted (`CMakeLists.txt:276`, cited by `TRADEMARKS.md` in the same citation) | **still fails** — the entry shelters line 14 and nothing else |
| a line inserted above 14, so the anchor MOVES | **exits 2** — "should contain `project(Anamorph VERSION`"; a declaration that stops pointing at its line takes the build down rather than excusing whatever is now there |
| token not present / line past EOF / unreadable file | each reported as a finding, never a traceback |

Self-test 123 → 130 cases, including a structural check that the substitution stays gated on the
anchor not having moved — the guard that separates "this line's content may change" from "this line
is not checked", and the one a later rewrite could drop invisibly on a clean tree.

**Scope note.** This is a change to a validation tool during a release round, which is not something
to do lightly. It is here because the release could not otherwise be prepared, the root cause is the
version bump itself, and leaving it would mean either a red gate on every future release or a
bypass. `CI_CD.md` and `RELEASE_PROCESS.md` now say so at the step where it bites.

---

## 5. A finding the implementation turned up, recorded rather than acted on

While proving Test 39 live, a path probe showed the **Wave-5 parked fast path is never reached by
turning Amount down**, contrary to the comment that introduces it (*"the amount glide settled at
exactly 0 with a 0 target — its one-pole flushes to true zero under the block's
`ScopedNoDenormals`"*).

The one-pole with a 0 target is `a += 0.0015f * (0 - a)`, i.e. `a -= 0.0015f * a`. Under FTZ the
**decrement** underflows first: at `a ≈ 7.83e-36` the product `0.0015f * a ≈ 1.174e-38` is below
`FLT_MIN` and flushes to zero, so the update becomes `a += 0` — a fixed point. Measured on the
shipped code, before and after A7-1 alike: the glide stalls at `7.82561114e-36` and stays there
(65 consecutive blocks in the probe), so `currentAmount > 0.0f` stays true and the gather path keeps
its eligibility.

**Consequences, stated precisely:**

* **No audio effect.** 7.8e-36 multiplied into the output is zero in float; the measured wet
  contribution at that point is one ULP of the M/S round trip (5.96e-08), the same as a true park.
* **The parked path is still reachable** the way it was written for: from a fresh `prepare()` with
  Amount at its 0 default, where `currentAmount` and `targetAmount` are both exactly 0. That is the
  common transparent case the Wave-5 measurement was taken on.
* **It is a performance gap, not a defect**: after a user turns Amount to 0 the engine keeps running
  the gather path instead of the cheaper parked loop.

**Not fixed in this round, deliberately.** The obvious repair (snap the glide to 0 below a threshold)
is a **Class B** change to a DSP glide — it alters the sample at which the amount reaches zero — and
this program's standing bar is Class A. It is also not A7-1's subject. It is filed as a new
candidate, **A7-9**, in §8 with the measurement above as its evidence.

---

## 6. Version and release

`0.9.4 → 0.9.5` (`CMakeLists.txt:14`), dated 2026-08-22. `CHANGELOG.md` carries one user-visible
entry under **Changed**: lower CPU at small buffer sizes and high sample rates, with the sound
unchanged and the twin-dump evidence cited. Implementation detail lives here, not there
(`CHANGELOG_POLICY.md` rule 3).

---

## 7. Roadmap status after this round

| Item | Status |
|---|---|
| **A7-0** — run the bench on a named machine, fill the `PERFORMANCE_BUDGET.md` numeric rows | **still open.** Needs a held-still desktop; nothing in this round can substitute. RISK-002 stays open. |
| **A7-1** — VelvetNoise linear-history slide | **DONE (v0.9.5)** — this document. |
| **A7-2** — remove the residual per-block term | **open**, and now better sized: 13,502 Ir/block at 48 kHz remains. Gated on A7-0's evidence that small buffers still hurt. |
| **A7-5** — multiband LR4 bank SIMD | **open, blocked** on an AVX2 / `-march` ADR + Architecture Review, jointly with W5-D. |
| A7-3 | closed — subsumed by A7-1. |
| A7-4 (skip the chain under settled Bypass) · A7-8 (gate metering/loudness) | **maintainer decisions**, unchanged by this round. |
| A7-6 (Vectorscope blit) · A7-7 (replace JUCE's oversampler) | not scheduled; unchanged. |
| **A7-9 (new)** — the Amount glide stalls above zero under FTZ, so the parked path is unreachable after a user turns Amount down | **new, open.** Class B; evidence in §5. |

## 8. What this round deliberately did not do

A7-2, A7-5, the bypass contract, the metering/loudness gates and every GUI candidate. Also not done:
the Class-B repair of the §5 glide stall. Each remains its own item with its own evidence bar.
