# N2-03 — exact Multiband Enable stage endpoints

Starting main `113b9189f0ff4d3b6348c553c559c22ee8ade782`, freshly fetched and
unchanged. Product 0.10.1, target 0.10.2, JUCE 9.0.5 at
`d60b7a78030661574a615b586d9325ba7a7fa7dd`. Mandatory original evidence was
committed first as `6b55dfad`; [the original report](dsp-numerical-audit-0102/original/REPORT.md)
retains the six-ULP witness, 151 changed channel-samples and original diagnostic.
This independent correction does not contain N2-02 or an H4 candidate.

## Mechanism and correction

ADR-0004 requires exact endpoints of the Multiband Enable crossfade. Its
block-level `blending` fast path is insufficient when the 12 ms smoother lands
inside the block. `from + 1*(to-from)` may round a finite destination differently;
zero also loses the sign of a negative-zero source for suitable destinations.
The same loop applied that expression to the clean-dry/A(dry) reference.

A private production sample kernel selects the source for blend 0 and destination
for blend 1. Targets are booleans. Only exact zero/one values select endpoints; every other
coefficient keeps the old float expression. All four existing
wet/dry contribution sites use the kernel and still share one getNextValue per
sample. The helper is actual shipping arithmetic, not a diagnostic hook; its
private access in tests permits stage-boundary bit assertions before later Mix
arithmetic can hide differences. There is no data-layout, graph, latency,
parameter, persistence, crossfade duration, reset or allocation change. The
existing settled-block shortcuts and H4 behavior remain intact.

## Production-path oracle

The Test 23 extension has 292 assertions: four direct kernel assertions and six
assertions in each of 48 engine legs. Rates 44.1/48/96 kHz; blocks 64/127/256;
2/3/4 bands; Mix 1/.4; Level Match off/on. Haas Amount .35, global Width 1.4,
non-neutral band widths and unequal stereo make both wet/dry destinations differ
from their sources. Held, repeated, mid-ramp reversed and return legs exercise
both endpoints, interior ticks and the settled suffix of terminal blocks.
At 48 kHz/127 the first 576-tick landing is sample 67 in transition block five.
Dividing 48 kHz/64 is a separate block-boundary control.

Each engine block copies only the unblended production filter state. A separate
MultibandWidth executes on the actual pre-Multiband and conditioned dry inputs,
with the same cold-bank reset prerequisite, to establish independent destination
samples. Assertions compare the private production kernel's wet output and the
actual aligned scratch / Level-Match reference by float bits. An explicit model
of existing downstream Mix rounding then checks audible output with Match off.
Match-on cases check the reference boundary, not downstream gain bits.

This reference intentionally shares the incoming filter history: the tested
contract is selection of that stage's computed endpoint, not whether H4 kept
that filter history warm. It cannot establish N2-01 phase reconstruction and does
not pretend to. D-01/T-01/D-02/R-01 tests remain unchanged.

| Engine source | Assertions / failures | Specific sensitivity |
|---|---:|---|
| corrected | 292 / 0 | exact wet/dry boundary; original interior expression |
| original main arithmetic | 292 / 66 | dry/reference bits and downstream predicted output |
| wet-only old arithmetic, dry corrected | 292 / 22 | output assertions only; dry assertions all pass |
| dry-only old arithmetic, wet corrected | 292 / 44 | aligned-dry / Level-Match reference assertions only |

The original and wet-only controls use the same corrected kernel declaration in
the tests, but execute the original expressions at the actual production call
sites. Thus a passing direct kernel test cannot conceal omitted production use.
Their different assertion failures isolate the two contributions. Raw logs retain
an early Test 77 label, later changed to Test 23 extension so independent N2-02's
new Test 77 can coexist. The assertions/configurations are unchanged.

## Replay and identities

Build this branch's AnamorphTests from source with the pinned JUCE, then run
`AnamorphTests --multiband-endpoints`. [Logs and source digests](n2-endpoints-0102/identity.json)
are gzip-compressed without changing their original bytes. A portable probe
entry and relinker are retained in that directory. Example:

```sh
python3 worklogs/n2-endpoints-0102/build_probe.py --checkout /tmp/n2-endpoints \
 --base-source /tmp/n2-endpoints --build /tmp/n2-build \
 --source /tmp/n2-endpoints/worklogs/n2-endpoints-0102/probe.cpp --out /tmp/n2-probe
/tmp/n2-probe/probe
```

The build must be fresh from the specified source/pin. The local incremental
measurement used GCC14.2, Release C++23, Haswell, contraction off and unchanged
objects from a freshly built original baseline. The test/engine/Multiband objects
were rebuilt. This is distinct from final-head hosted CI. Recreate negative
call-site controls with the included script; do not alter the branch under review.
The maximum output error is a numerical endpoint-contract violation, not an
assertion of audibility. N2-04/05/06/07 remain confirmed and uncorrected.

The original-engine 48-row matrix observes **2,828 aligned-scratch and 2,828 reference bit mismatches** and **750 downstream channel-sample bit
differences**, peak **5.96046448e-8**. The corrected result is zero for each.
Source declarations added by the private kernel shift three existing header
citations; those anchors are re-pointed to the same text, without changing ADRs.

The final local full DSP binary passes **4,794 checks** (4,502 existing + 292
endpoint-extension assertions). Documentation/citation, portability and realtime
source lints pass. Full platform/sanitizer/State/ABI/pluginval and exact-tip
hosted evidence remain separate and are recorded when available.

## Endpoint-only review refinement

The final kernel tests exact zero/one by a zero absolute difference, rather than
clamping coefficients with <=0 / >=1. Thus every other finite coefficient keeps
the original expression even outside the smoother's expected range. The direct
preservation grid now includes -.25 and 1.25; the assertion count remains 292.
This avoids relying on a universal floating-point range claim about recursive
smoother addition at every possible host sample rate. No new main defect is
inferred from an unobserved out-of-range coefficient. The initial `391d3f0a`
measurements/CI remain historical; final-source validation is recorded separately.

The engine oracle classifies endpoint coefficients by their float bits, independently of the production comparison, while retaining all 292 assertions.
