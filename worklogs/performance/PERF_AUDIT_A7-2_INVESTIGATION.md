# A7-2 investigation — the residual VelvetNoise per-block term

**Status: investigation complete. NO product code changed.** Two designs were prototyped outside the
tree and measured; both are bit-exact. **The direction the roadmap proposed is the one that should
NOT be taken**, and the reason is measured rather than argued. The alternative is ready to execute
and is described here in enough detail to be implemented directly, but landing it is a maintainer
decision for the two reasons in §7.

Follows [`PERF_AUDIT_v0.9.4_INVESTIGATION.md`](PERF_AUDIT_v0.9.4_INVESTIGATION.md) §7 item A7-2 and
[`PERF_AUDIT_v0.9.5_IMPLEMENTATION.md`](PERF_AUDIT_v0.9.5_IMPLEMENTATION.md), which shipped A7-1.

---

## 1. What the residual term actually is

Solved from the 32- vs 256-sample pair on the shipped v0.9.5 engine, per function:

| | 48 kHz | 192 kHz |
|---|---|---|
| **total fixed per block** | **13,502 Ir** | **39,429 Ir** |
| `__memcpy_avx_unaligned_erms` (the A7-1 slide) | **8,704 (64 %)** | **34,624 (88 %)** |
| `VelvetNoise::processBlock` itself | 924 | 929 |
| `AnamorphEngine::process` | 359 | 359 |
| `__log10f` + `__logf` (per-block LUFS / meter dB) | 1,380 | 1,380 |
| `StereoLevel::publish` | 242 | 247 |

Two things follow, and the second is the one that bounds this item.

**The slide IS the residual term**, and it is the only part that scales with the sample rate:
8,704 × 4 = 34,816 against 34,624 measured, which is `decorrSamps = round(0.045 * sr)` doing exactly
what it did before A7-1 — at 4.03 Ir per float moved.

**The rest is not VelvetNoise's and is not A7-2's subject.** ~4,800 Ir/block is identical at both
sample rates: the per-block libm conversions in Level-Match and the meters, the engine's own
per-block bookkeeping, the meter publish. **So A7-2's ceiling is 8,704 of 13,502 at 48 kHz (64 %) and
34,624 of 39,429 at 192 kHz (88 %)** — removing the slide cannot take the fixed term below ~4,800.

---

## 2. The two designs

### Variant A — the roadmap's proposal: a cursor into an over-sized image

`linHist` is grown by `max(maxBlockSize, decorrSamps)` and a cursor `linBase` marks where the live
tail starts. A gather block writes only its own mids at `linBase + decorrSamps` and advances the
cursor; the tail is already in place because the previous block put it there. When the cursor would
run past the buffer, the tail is re-seated to the front with one copy — the same copy A7-1 does, now
amortised.

### Variant B — gather straight from the ring, with no image at all

The linear image exists only to make each tap's read unit-stride. But the ring is already unit-stride
in `i`; it merely wraps. For tap `t`, sample `i`'s source is `midHist[(writePos + i - pos[t]) &
histMask]` while `i < pos[t]`, and this block's own `midBlk[i - pos[t]]` after that (the ring does not
hold those yet — the per-sample loop writes them later). Both are contiguous, and the ring part splits
in two only where it wraps: **1–3 unit-stride runs per tap, and no image to build.**

H5's property is kept — every read is still a contiguous streaming run — while the thing H5 built to
get it goes away.

---

## 3. Both are bit-exact

Each prototype was compared against the shipped v0.9.5 engine over **180 configurations** — 9
scenarios (working, multiband off, Haas, Chorus, Dimension-D, oversampling ×8, defaults, bypass,
split drag) × 5 block sizes × 4 sample rates — hashed (FNV-1a) over every output sample of both
channels.

| | mismatches |
|---|---|
| Variant A | **0 / 180** |
| Variant B | **0 / 180** |

Neither changes the accumulation order, the starting `+0`, or any value: A moves the same floats to a
different offset, B reads the same floats from where they already are.

---

## 4. Measured cost

Callgrind Ir/sample, steady state (3.0 s run minus 1.0 s, halved), against the shipped engine:

| SR | block | v0.9.5 now | A (cursor) | B (no image) | A | B |
|---|---|---|---|---|---|---|
| 48 kHz | 32 | 2018.5 | 1755.1 | 1787.4 | **−13.1 %** | −11.4 % |
| 48 kHz | 128 | 1702.9 | 1639.2 | 1646.1 | −3.7 % | −3.3 % |
| 48 kHz | 256 | 1649.3 | 1618.8 | 1621.5 | −1.9 % | −1.7 % |
| 192 kHz | 32 | 2828.7 | 1755.2 | 1788.2 | **−37.9 %** | −36.8 % |
| 192 kHz | 128 | 1905.4 | 1639.3 | 1644.9 | −14.0 % | −13.7 % |

Decomposed at 48 kHz:

| | fixed per block | marginal per sample |
|---|---|---|
| v0.9.5 now | 13,502 Ir | 1596.6 Ir |
| Variant A | **4,984 Ir** | 1599.3 Ir |
| Variant B | **6,066 Ir** | 1597.9 Ir |

Both land near the ~4,800 floor §1 predicts, so both do remove the slide. A is ~1,100 Ir/block cheaper
than B (B pays extra loop set-ups: up to three runs per tap instead of one, ×64 taps). **A is 1–2
percentage points ahead of B on average at every point measured.**

**The sample-rate dependence of the fixed term is gone in both**: at 32-sample blocks, 48 kHz and
192 kHz land on the same figure (1755.1 vs 1755.2 for A, 1787.4 vs 1788.2 for B) where the shipped
engine differs by 810 Ir/sample.

---

## 5. The finding that decides it: variant A does not improve the worst block

A amortises the copy; it does not remove it. On the block where the cursor runs out, A pays the
**full** copy — the same 8,704 Ir at 48 kHz, 34,624 at 192 kHz, that the shipped engine pays every
block. Measured with a counter in the prototype, over 4,000 gather blocks:

| SR | block | compactions | one in |
|---|---|---|---|
| 48 kHz | 32 | 49 | **81.6 blocks** |
| 48 kHz | 128 | 200 | 20.0 blocks |
| 48 kHz | 512 | 800 | **5.0 blocks** |
| 192 kHz | 32 | 14 | 285.7 blocks |
| 192 kHz | 128 | 57 | 70.2 blocks |

**Variant B compacts zero times, at every setting, because it has nothing to compact.**

Why this decides the item rather than being a footnote: a plug-in drops a buffer on its WORST block,
not its average one. The whole reason this fixed term mattered enough to become a roadmap item is
that it lands inside a single `process()` call. Variant A leaves that call exactly as expensive as it
is today and merely makes it rarer — turning a uniform cost into a **periodic spike**, which is a
worse shape for an audio thread even though the average falls. At 512-sample blocks it is barely
amortised at all (one block in five).

`AnamorphBench` reports a worst-single-block figure, but on this machine that column measured up to
65 % run-to-run spread (`PERFORMANCE_BUDGET.md`), so it cannot settle this; the counter above is the
evidence, and the rest is the arithmetic of the algorithm.

### Memory, which points the same way

| | `linHist` per instance, 48 kHz / 192 kHz |
|---|---|
| v0.9.5 now | 10.7 KB / 36.6 KB |
| Variant A | **+8.6 KB / +34.5 KB** (roughly double) |
| Variant B | **−10.7 KB / −36.6 KB** (the buffer is not needed) |

### State, which points the same way again

A **adds** a second piece of cross-block state (`linBase`) to a module that gained its first
(`linHistSlide`) in v0.9.5, and both must be invalidated by the same rule. B **removes** both: with no
image there is nothing to keep valid across blocks, and the A7-1 invariant disappears rather than
being extended. Test 39 keeps its value either way — block-length invariance is exactly the property
that guards B's split-run arithmetic, and the mixed-block-size run added on review covers the case
where the runs differ from block to block.

---

## 6. Conclusion

| | verdict |
|---|---|
| **A7-2A — the roadmap's proposed ring-free double buffer** | **Do not implement.** It buys average CPU by converting a uniform per-block cost into a periodic full-size spike, leaves the worst block exactly where v0.9.5 left it, roughly doubles the history buffer, and adds a second cross-block invariant. |
| **A7-2B — gather straight from the ring** | **Recommended.** 1–2 points behind A on average, ahead of it on the worst block (which is the number that matters), frees the buffer instead of growing it, and deletes the cross-block state instead of adding to it. Class A, 0/180 mismatches. |

The roadmap's framing — *"the proposed direction is a ring-free double-buffer history structure, but
this must be investigated before implementation"* — is answered: investigated, prototyped, measured,
and **the proposal is the weaker of the two options**.

---

## 7. Why this round stops here

Two reasons, both of them the maintainer's call rather than an engineering judgement:

1. **A7-2's own gate is unmet.** The roadmap files A7-2 as *"open… Gated on A7-0's evidence that
   small buffers still hurt."* **A7-0 has not been done**: there is still no wall-clock datum from a
   named, held-still machine, and RISK-002 is still open. Everything above is instruction counts,
   which are the right unit for comparing two implementations and the wrong unit for deciding
   whether a user is dropping buffers. Landing a second optimisation into this function on
   instruction counts alone would be stepping over a gate this programme set for exactly this
   decision.

2. **The recommended change is not the change that was scoped.** A7-2 was scoped as the
   double-buffer; the investigation concludes the double-buffer is the wrong half of the fork and
   recommends a rewrite of the H5 gather kernel instead — Wave-2 contractual code, in the same
   function v0.9.5 changed hours ago and which has not yet had its release audition. Substituting a
   different design for the scoped one, unprompted, is what the architecture review gate exists to
   catch.

**Neither reason is that variant B is risky.** It is Class A over 180 configurations, ~30 lines in one
function, it removes state rather than adding it, and Test 39 already guards the arithmetic it
changes. If the maintainer un-gates it, §8 is the plan.

---

## 8. Ready-to-execute plan for A7-2B

1. **Delete** `linHist`, `linHistSlide` and the `slide` take-and-clear from `processBlock`, and the
   `linHist.assign` in `prepare()`. `midBlk` and `accum` stay.
2. **Replace** the tap loop with the 1–3-run split of §2, keeping the ascending-`t` order and the
   zero-fill of `accum` untouched:
   * `k = pos[t]`; ring portion length `min(k, numSamples)` starting at `(writePos - k) & histMask`,
     emitted as runs bounded by the ring end; then `midBlk[i - k]` for `i >= k`.
   * **Two spellings are forbidden, and the follow-up round verified why** (see
     `PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md` §6). Taking the ring run length as `k` rather than
     `min(k, numSamples)` is a heap overflow of `accum` on the normal path — ASan names the line, so
     it would not ship, but it is the obvious way to write it. Basing the tail on
     `midBlk.data() - k` forms a pointer before the start of the object: **no** diagnostic under
     ASan + UBSan + `local-bounds` + `pointer-overflow`, bit-correct output, silent UB. Index the
     tail as `midBlk[i - k]` with `i >= k`; nothing downstream will catch the alternative.
3. **Keep** the eligibility gate exactly as it is — this changes how the gather reads, not when it
   runs.
4. **Re-point** Test 39's comment at the split-run arithmetic. ~~The test body needs no change,
   which is itself the argument that it was the right test.~~ **Superseded by step 6** — the
   follow-up round proved Test 39 blind to the likeliest A7-2B defect, so the body does need
   company.
5. **Evidence to regenerate before it lands**: `AnamorphDspDump` twin diff; the 180-configuration
   sweep; Test 39 at four rates × three block schedules; both suites under ASan+UBSan (`local-bounds`
   matters here — the run arithmetic is new); full preflight.
6. **A new committed test must land FIRST, not alongside (A7-2T).** Step 4's claim that "the test
   body needs no change" was checked in the follow-up round and does not hold. Test 39's oracle is
   the build under test compared against itself, so it is blind to any defect that is not a function
   of block length — including the likeliest A7-2B defect, a uniform one-sample tap-delay error.
   Seeded, that error is caught by Test 39 only through its SCHEDULE (at its transport stop, or with
   that removed at its moving density); with every path crossing removed Test 39 passes on the
   seeded build at all four rates, so its competence here is incidental and a schedule edit could
   remove it. The oracle that catches it directly needs no
   product change: `prepare()` sizes `accum` from `maxBlockSize` and the gather gate requires
   `numSamples <= accum.size()`, so an instance prepared for a SMALLER block runs the per-sample
   loop over the same audio with an identical ring, tap set and coefficients — assert the two
   bit-identical. Verified live in both directions in
   `PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md` §3–§4.
7. **Extend Test 39's reach** with one block longer than `decorrSamps` (e.g. 44.1 kHz at 4096 — the
   regime where every tap splits and none wraps, and an ordinary offline-bounce buffer) and one
   density-1.0 pass (at the default 0.5 exactly 32 of 64 taps are active, and they are the shallow
   half).
8. **Expected outcome**, measured on the prototype: −11.4 % at 48 kHz/32, −3.3 % at 48 kHz/128,
   −36.8 % at 192 kHz/32, −13.7 % at 192 kHz/128; fixed per-block term 13,502 → 6,066 Ir at 48 kHz
   with **no** compaction spike at any setting; `linHist` freed. **Re-measured** on a faithful
   prototype (the one measured here still carried `linHist`): −11.3 %, −3.3 %, −36.5 %, −13.5 %, and
   the fixed term 12,957 → 5,546 Ir at 48 kHz — and, the point that table missed, 39,373 → 6,104 Ir
   at 192 kHz. B does not shrink the residual term so much as remove its sample-rate dependence.

## 9. Roadmap status after this round

| Item | Status |
|---|---|
| **A7-0** — bench on a named machine, fill the `PERFORMANCE_BUDGET.md` rows | **open**, and gating A7-2. RISK-002 open. |
| **A7-1** | DONE (v0.9.5). |
| **A7-2** | **IMPLEMENTED 2026-08-22 as variant B** — see `PERF_AUDIT_A7-2B_A7-5E_IMPLEMENTATION.md`. Previously: Proposal (A) rejected on measurement; alternative (B) recommended and planned. **A7-2T is now in the tree**; awaiting A7-0 and a maintainer decision on the design substitution. |
| **A7-2T** — commit the path-equivalence oracle (§8 step 6) | **DONE (PR #129), and spent: A7-2B landed against it.** `testVelvetGatherEqualsPerSampleLoop` (Test 40), 24 checks, proven live on a seeded one-sample tap-delay error (20 of 20 fail). A7-2B's remaining gate is A7-0. |
| **A7-5** — multiband LR4 SIMD | open, blocked on an AVX2 ADR (with W5-D). The ADR should not be drafted until the universal binary's two slices are diffed — `PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md` Part II. |
| A7-4 · A7-8 | maintainer decisions, unchanged. |
| A7-3 · A7-6 · A7-7 | not scheduled, unchanged. |
| **A7-9** — the Amount glide stalls above zero under FTZ | open, Class B. **Widened**: the same false premise is load-bearing in `HaasProcessor` and `ChorusEngine` too, and the missed park is the largest single item on this roadmap. Evidence in the v0.9.5 worklog §5 and `PERF_AUDIT_A7-2_A7-5_A7-9_INVESTIGATION.md` Part III. |
