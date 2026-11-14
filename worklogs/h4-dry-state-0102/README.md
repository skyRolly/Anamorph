# Reproduce the H4 decision experiments

Read [the decision report](../H4_DRY_RECONSTRUCTION_0.10.2.md) and
[Proposed ADR-0074](../../docs/architecture/design-decisions/ADR-0074-h4-dry-state-reentry.md).
The production math in this branch is unchanged. Patch files are **nonshipping
diagnostic candidates**, not approved fixes. `manifest.json` identifies the
original bytes behind deterministic gzip files. JSON identities include exact
source-file and ELF hashes plus compile/link commands; an unchanged Git SHA does
not identify a dirty diagnostic variant without its patch/file digests.

## Build from the recorded source and pin

Use CMake >=3.22, Ninja, JUCE platform development libraries and a C++23 compiler.
The recorded toolchain is GCC14.2 / Release / Haswell / contraction off, JUCE
9.0.5 at `d60b7a78030661574a615b586d9325ba7a7fa7dd`. The original toolchain
recipe is retained in `../dsp-numerical-audit-0102/Toolchain.Dockerfile`.
No product/dependency update is required. Set packet/source/build paths for your
machine; the original `/workspace` directories are not required.

```sh
git worktree add --detach /tmp/h4-base 113b9189f0ff4d3b6348c553c559c22ee8ade782
git clone https://github.com/juce-framework/JUCE.git /tmp/h4-juce
git -C /tmp/h4-juce checkout d60b7a78030661574a615b586d9325ba7a7fa7dd
cmake -S /tmp/h4-base -B /tmp/h4-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 -DANAMORPH_BUILD_STANDALONE=OFF -DANAMORPH_JUCE_PATH=/tmp/h4-juce
cmake --build /tmp/h4-build --target AnamorphTests -j 2
```

Create another detached worktree at the **same SHA** for each candidate and
`git -C /tmp/h4-A apply /absolute/packet/candidate-A.patch`, similarly for C/A2.
The filters-only and no-gate controls use their separate patches. Never apply
them to the branch under production review. No candidate changes the JUCE pin.

## Correctness and the optional failing oracle

For each checkout (baseline or patched) run:

```sh
python3 /absolute/packet/build_probe.py --checkout /tmp/h4-A \
 --base-source /tmp/h4-base --build /tmp/h4-build \
 --source /absolute/packet/h4_probe.cpp --out /tmp/h4-A-probe
/tmp/h4-A-probe/probe /tmp/h4-A-probe/samples.csv > /tmp/h4-A-probe/matrix.csv
python3 /absolute/packet/verify_oracle.py /tmp/h4-A-probe/matrix.csv \
 --out /tmp/h4-A-probe/oracle.json
```

The baseline must fail the two first-window error assertions. The never-H4
mutation instead fails the two execution premises although numerical error is
zero. A/C/A2 ordinary healthy controls should pass. `h4_probe.cpp` records wet
state progression, frozen/changed dry state in the static witnesses, H4's clean
reference, actual re-entry consumption and subsequent recovery. The independent
dry production module runs continuously; the wet reference stays full wet.
The Match interaction is a reference-policy comparison, not an equality oracle.
No mandatory failing test was added to CI.

`recovery_probe.cpp` exercises the finite-extreme unused-state case. Build it
with the same helper and run the resulting executable with no arguments. Trace
engine variants add nonshipping counters at the actual final-wet guard and the
candidate-only dry guard; their source/patch and command identities are retained.
They are not used for timing or instruction measurements. `recovery_probe_v1.cpp`
preserves the first uninstrumented experiment.

## Wall-clock and instruction measurements

The original wall-clock binary used the preserved `h4_probe_bench_v1.cpp` source;
v2 adds correctness observations without changing benchmark operations. Build
that source with the same helper, then run `probe --bench`. It writes 72 cells x
five repetitions. Repeat in baseline-A-C-C-A-baseline order for the original ten
measurements per cell. Keep CPU quota, flags, warm-up, inputs, sample rate, block
size and parameters identical. Read `h4-bench-machine.json` and the raw CSVs;
large wall-clock outliers include shared-host descheduling.

For instruction attribution, install Valgrind 3.24 (headers included), compile
`h4_count.cpp` with the helper and invoke:

```sh
valgrind --tool=callgrind --collect-atstart=no \
 --callgrind-out-file=/tmp/h4-Ir /tmp/h4-count/probe 48000 128 4 0 0 1
```

Arguments are rate / block / bands / moving / actual x4 Drive / initial Mix.
Client requests collect exactly 256 setParameters+process calls, excluding
warm-up/input generation. `run-counts.py` retains the original absolute commands;
`instructions.md` and raw callgraphs supply counts and actual OS execution.
`replay_probe.cpp` is a single-filter finite-history experiment, not a production
replay implementation. Its initial timing is illustrative; it does not establish
a host deadline or general exact reconstruction.

Gzip-decompress individual results to recover their original bytes and verify
the manifest SHA256. Rebuildable objects/ELFs are omitted; their identities and
all required source/commands remain. Original source variants used the three
changed probe/engine/Multiband TUs; the current helper also recompiles MonoMaker
and SoloMonitor so the added diagnostic LR4 recovery method has consistent
headers in every dependent first-party DSP TU. JUCE objects come from the fresh
same-pin baseline build. No prebuilt or stale JUCE configuration is evidence.

No automated output here establishes native DAW behavior, audition, hardware
acceptance, universal CPU budgets, architecture approval or release readiness.


`thread_bench.cpp` is a separate Linux CPU-time batch check. Compile it with the
same helper, then run baseline-A2-A2-baseline with no arguments. It includes
identical pre-generated input copies in every timed batch, rather than comparing
its absolute ns/sample directly with the original per-block clock harness.
`results/h4-counts-guard-final/` is the dependency-consistent final instruction
repeat; every value matches the preserved first guarded count.
The unrelated-channel guard control and reset-both mutation are generated by
`make_recovery_trace.py --unrelated [--reset-both-mutation]`; only the right-state
preservation comparison changes (4/4 versus 0/4). Trace v2 before the support-only
include guard is retained as `recovery_probe_trace_v2.cpp` and matches all three
initial trace source digests byte-for-byte.


## Additional trajectory and final production controls

Build `trajectory_probe.cpp` with `build_probe.py`, then run it with a sample CSV
path exactly as `h4_probe.cpp`. Four rows distinguish sustained per-sample cutoff
gliding from a discrete bank fade overlapping re-entry, including actual x4
Drive. `glide_count.cpp` uses the same build and Callgrind command interface as
`h4_count.cpp`; `run-glide-counts.py` preserves all eight original invocations.
Its glide observer is outside the collection region. `instructions.md` separates
these counts from the original mostly-epsilon-snap movement rows.

Final production validation uses A `61e82e51`, B `003c9a1a` and disposable combined
`7764df5c`. Check out the full recorded SHAs in the delivery ledger and configure
a fresh CMake build from each with the same JUCE pin. Run both full suites; use
`--n2-controls` / `--multiband-endpoints` for focused production selectors.
`combined-final-controls.py` restores only one defect at a time by generating
untracked engine variants and deriving compile/link commands from the combined
build; its original root/build/out paths must be adjusted on another machine.
The generated sources, exact commands, binary hashes and logs are retained.
The portable per-PR `run_controls.py` / `make_controls.py` supply the equivalent
fresh-build recipe on their respective review branches.

`relink-final-endpoint-controls.py` reuses a fresh `build_probe.py` identity with
actual `tests/dsp_tests.cpp` as its entry, then recompiles only the three isolated
engine controls. Pass `--identity`, `--checkout`, `--build`, `--out`; unchanged
objects and the new generated sources/commands/binaries are all hashed. It
preserves the final endpoint-bit classifier and out-of-range preservation grid.
The clamp-only negative control is a detached B worktree with just the helper's
endpoint comparisons restored to <=0 / >=1; it fails only the coefficient
preservation check while the engine boundary rows still pass.

Combined H4 matrices use that exact combined production SHA, first unchanged,
then with `candidate-A2.patch` applied in a separate detached worktree. Rebuild
with the helper from the fresh combined build. These are diagnostic variants,
not changes to either production PR. No H4 candidate is committed under `src/`.
Full source commands and ELF identities are in the corresponding `results/`
subdirectories; logs retain their original bytes behind deterministic gzip.
