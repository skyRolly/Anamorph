# 0.10.2 second numerical and interaction audit — durable evidence

This archive preserves the completed investigation at main
`113b9189f0ff4d3b6348c553c559c22ee8ade782`, product 0.10.1, JUCE 9.0.5
`d60b7a78030661574a615b586d9325ba7a7fa7dd`. Begin with
[the original report](original/REPORT.md) and [original procedure](original/README.md).
Those files are preserved byte-for-byte as dated historical evidence. Their absolute
workspace paths describe the original execution; they are not required by the portable
procedure below. The report's implementation prohibition belongs to that completed round;
the subsequent assignment separately authorizes N2-02/N2-03 only.

`manifest.json` lists every preserved file's original bytes and SHA256. Raw logs and CSVs
are gzip-compressed with deterministic metadata; decompress to recover the original bytes.
All original probe sources, diagnostic engine copies/patches, command streams, comparison
scripts, rejected hypotheses, confidence/priority assessments, limits and P3 follow-ups are
retained. ELF binaries, object files and build trees are omitted as rebuildable products;
the original binary/object hashes are retained. No product fix or architecture acceptance
is inferred from a diagnostic variant.

The independent production findings remain N2-01 through N2-07; verification weakness is
V2-01. Three P2 production findings, four P3 findings and one P2 verification claim were
confirmed. No P0/P1 was established. The fresh historical baseline had 4,502 DSP and 16,535
State assertions passing; this does not validate any later source. N2-04–07 remain confirmed
and outside the new assignment's production implementation scope. N2-01 requires an H4
warm-state/CPU decision. Existing D-01/T-01/D-02/R-01 fixes are not reopened by the archive.

## Replay on another machine

Use a C++23 compiler, CMake >=3.22, Ninja and JUCE's platform development libraries.
`Toolchain.Dockerfile` records the original Debian/GCC toolchain package recipe. It is a
toolchain record, not a product binary. The original machine used GCC 14.2, contraction off,
Release, x86-64 Haswell flags. Other platforms may have different float details.

```sh
git worktree add --detach /tmp/anamorph-n2-source 113b9189f0ff4d3b6348c553c559c22ee8ade782
git clone https://github.com/juce-framework/JUCE.git /tmp/anamorph-n2-juce
git -C /tmp/anamorph-n2-juce checkout d60b7a78030661574a615b586d9325ba7a7fa7dd
cmake -S /tmp/anamorph-n2-source -B /tmp/anamorph-n2-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DANAMORPH_BUILD_STANDALONE=OFF \
  -DANAMORPH_JUCE_PATH=/tmp/anamorph-n2-juce
cmake --build /tmp/anamorph-n2-build --target AnamorphTests AnamorphStateTests -j 2
python3 worklogs/dsp-numerical-audit-0102/rebuild_probe.py \
  --checkout /tmp/anamorph-n2-source --build /tmp/anamorph-n2-build \
  --out /tmp/anamorph-n2-probe --probe overlap --variant baseline
/tmp/anamorph-n2-probe/overlap-baseline
```

The helper derives compile/link commands from the freshly built baseline objects, validates
the exact source SHA, and replaces only the nonshipping entry point and optional diagnostic
engine object. It accepts `probe`, `gate`, `overlap`, `processor`, `os`, `heal`, `drive`.
Use a different `--out` for each variant. The `gate` and `probe` executables take a CSV output
filename. `processor` uses the State target's isolated preset-directory seam; run under Xvfb
on Linux if no display exists. `os` needs `--variant os_trace` for the terminal trace.

Reproduce the original comparisons with these pairs:

| Probe | Baseline versus diagnostic | Evidence |
|---|---|---|
| overlap | baseline / live_toggle | N2-02, both directions and fade phases, 44.1/48 kHz |
| gate | baseline / always_dry | N2-01/V2-01, output and continuously full-wet production twin |
| probe | baseline / endpoint | N2-03 terminal wet/dry bits, original CSV comparator |
| heal | baseline / trace / retain_dry | N2-04 guard execution and subsequent dry-history consumption |
| probe | baseline / mix_zero / mix_one | N2-05/N2-06 independent endpoints and recovery |
| drive | baseline / drive_endpoint | N2-07 isolated from the downstream Mix reblend |
| processor | baseline | N2-01/N2-02/N2-05 at production processor boundary |

The original comparison scripts contain the old path only in their data-directory constant;
replace that constant with the replay output directory. The inputs, formulas and raw results
are preserved. `test25-oracle.py` applies Test 25's criterion to a 128-frame independent probe;
it does not claim the unchanged Test 25 ran that tone. Trace variants perform IO and are not
real-time or performance evidence. No headless result establishes audition, DAW incidence,
hardware behavior or architecture approval.

This archive is supporting developer evidence under SOURCE_OF_TRUTH. Accepted ADRs and
policies retain authority. Subsequent implementation and decision results belong in
`worklogs/ORDINARY_DSP_CORRECTIONS_0.10.2.md`; do not overwrite historical reports or logs.
