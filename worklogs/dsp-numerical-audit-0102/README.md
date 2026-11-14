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
python3 worklogs/dsp-numerical-audit-0102/rebuild_probe.py \
  --checkout /tmp/anamorph-n2-source --build /tmp/anamorph-n2-build \
  --out /tmp/anamorph-n2-probe --probe overlap --variant baseline \
  --also-variant live_toggle --run
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

### Baseline replay provenance follow-up

The historical measurements above are unchanged. `rebuild_probe.py` checks the
exact baseline Git root/HEAD, compares tracked raw bytes, file types and
executable bits with the expected Git tree, and rejects staged changes and all
untracked or ignored inputs, including outside `src/`. The supported trees have
no tracked symlinks or submodules; these are rejected rather than followed.
Checkout filters or line-ending conversion that change Git blob bytes fail
closed. Keep builds, bytecode, logs and other ignored files outside the source
and JUCE checkouts. Index flags such as assume-unchanged and skip-worktree do
not hide changed working-tree bytes. The helper checks the supplied CMake source
and effective JUCE checkout, verifies archived source and patch bytes and their
exact difference from baseline, then configures a new build; the supplied
build's objects and generated rules are not reused. The output must not exist.
Archived C++ is copied into the new isolated output before compiling so an
unverified archive-neighbor header cannot shadow source headers. The identity
records build rules, compiler/configuration, each linked object, executable and,
with `--run`, execution and raw output hashes. Multiple selected variants share
only objects produced by that fresh build. Arbitrary dirty baselines are refused.

These checks assume trusted local tools, compiler, system headers/libraries, configuration environment and filesystem, and no concurrent modifications between checks and use. They do not attest to the compiler or environment, prove reproducibility of historical binaries, prove that an archived patch expresses the intended experiment, or authenticate a maliciously modified helper/Git object store. `HEAD` alone is not a content or object-provenance guarantee. Do not describe an unchecked relink or a recorded `source_sha` as proof that all linked objects came from that source.

`test_replay_provenance.py` supplies synthetic controls for clean inputs, hidden
index flags, staged/tracked/untracked/ignored edits, file modes, symlinks, another
configured source directory, wrong or dirty JUCE, changed archived inputs, and
refusal to reuse a pre-existing build. These tests are not product replays. A
new fresh baseline and `live_toggle` diagnostic replay is recorded separately in
`replay-followup-2026-11-14/`; each produced the same 16 output rows, byte for
byte, as the corresponding preserved historical log. Its identity records the
new source, dependency, objects, commands and executable, not the historical
build. That execution used the immediately preceding guard and helper hashes
recorded in its receipt; later Git-environment and helper-stability checks need
their own real replay before being described as exercised on this input. The
shared `replay_experiment.py` additionally accepts a committed,
reviewed experiment specification and a clean or exactly patched source tree,
then builds fresh targets and records executions. A patch hash identifies bytes;
the specification's stated experimental purpose still requires human review.

R-237-01 remains open across its full scope: the existing H4, N2-02, N2-03,
combined-control, trace and benchmark relinkers have not yet been migrated to
the shared fresh-build mechanism. Their earlier observations and premises remain
historical; their HEAD fields alone do not establish linked-object provenance.
