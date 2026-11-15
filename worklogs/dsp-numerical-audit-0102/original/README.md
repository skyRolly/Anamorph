# Anamorph 0.10.2 investigation artifacts

Investigated source: `113b9189f0ff4d3b6348c553c559c22ee8ade782`. The `source/` directory is a detached, clean Git worktree at that SHA. JUCE was freshly cloned as tag 9.0.5, commit `d60b7a78030661574a615b586d9325ba7a7fa7dd`, in `/workspace/anamorph-audit-juce-9.0.5`. The existing `/workspace/.cloud-setup/deps/JUCE` is 9.0.4 and is **not** used. `build/` was newly configured for this source/dependency; none of the old `Anamorph/build` products were used. The toolchain is the local `codex-rolly-native:trixie-gcc14-v1` image (GCC 14.2, CMake 3.31.6), Linux x86-64, Release, `-ffp-contract=off`; this is not a DAW or hardware test.

All paths below use `/workspace/anamorph-audit-113b9189` as `AUDIT`. The shell commands in `compile-probe.sh`, `build-*.sh` and `commands.txt` use absolute paths from this workspace. To reproduce elsewhere, place the worktree and JUCE at those paths or regenerate the scripts from the new CMake/Ninja command output and substitute paths. The `engine-*.cpp` and `*.patch` files are diagnostic copies derived from the source SHA; none are production changes. The trace prints from the audio call and thus is not a real-time test. The variants do not represent reviewed fixes.

Baseline build, inside the native toolchain container with `/workspace` mounted:

```sh
cmake -S /workspace/anamorph-audit-113b9189/source -B /workspace/anamorph-audit-113b9189/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DANAMORPH_JUCE_PATH=/workspace/anamorph-audit-juce-9.0.5 \
  -DANAMORPH_BUILD_STANDALONE=OFF
cmake --build /workspace/anamorph-audit-113b9189/build --target AnamorphTests -j 3
/workspace/anamorph-audit-113b9189/build/AnamorphTests_artefacts/Release/AnamorphTests
```

`build.log`, `dsp-tests.log`, `state-build.log` and `state-tests.log` retain the fresh build and runs; State used Xvfb and its disposable test profile. Obtain the exact object/link commands by running `ninja -t commands AnamorphTests > ../commands.txt` from `build/`. From `build/`, `bash ../compile-probe.sh` compiles `probe.cpp` and links it with the fresh baseline objects. Run `../probe-baseline ../baseline.csv > ../baseline.log` to generate the transition and overlap results. `python3 ../make-variants.py` generates the named variants and compile scripts. From `build/`, `bash ../build-trace.sh`, `bash ../build-endpoint.sh`, `bash ../build-live_toggle.sh`, `bash ../build-mix_zero.sh`, `bash ../build-mix_one.sh`, `bash ../build-drive_endpoint.sh` and `bash ../build-drive_mix.sh` build the diagnostic variants. Run these from the artifact directory, passing a CSV filename to `probe-trace` and `probe-endpoint` (e.g. `./probe-endpoint endpoint.csv > endpoint.log`) and without an argument to the other variants. `python3 compare.py` compares baseline, trace and endpoint CSVs by case/sample/channel.

`baseline-expanded.log` / `mix-one.log` contain the later Mix=1 discriminating case. The earlier `baseline.log`, `trace.log`, `endpoint.log`, `live_toggle.log`, `mix-zero.log`, `drive-endpoint.log` and `drive-mix.log` are preserved raw runs. `trace-stderr.log` records actual Multiband terminal ticks and self-heal; `compare.log` records the differential output. Diagnostic toggle-copy, endpoint, Mix and Drive variants each change only the relevant suspect operation, as their patches show.

Additional focused scripts and probes: `os-probe.cpp` / `build-os-probe.sh` trace actual wrapped-filter crossfade terminal ticks; `gate-probe.cpp` / `build-gate-probe.sh` compare the H4 gated dry bank to a diagnostic continuously running bank; `heal-probe.cpp` / `build-heal-probe.sh` compare valid conditioned dry history after a wet self-heal. Read their logs and the report for outcome and limitations. For the processor-level probe, build the fresh `AnamorphStateTests` target first, save `ninja -t commands AnamorphStateTests` as `state-commands.txt`, then run `bash ../compile-processor-probe.sh` from `build/`; execute `processor-probe` under Xvfb. The processor probe calls the production `AnamorphAudioProcessor::processBlock`, not a VST3 host wrapper.

Baseline SHA, pin, remote status, findings, exact commands/results and bounds are documented in `REPORT.md`. The original tracked checkout remains unchanged. No fixes, PRs, releases or tags are produced here.

For the expanded ordinary-duck matrix, after building `engine-live_toggle.o`, run `bash ../build-overlap-matrix.sh` in `build/`, then `./overlap-baseline > overlap-matrix.log` and `./overlap-live > overlap-live-matrix.log` in the artifact directory. The matrix covers both initial states, both fade phases and 44.1/48 kHz. `gate-twin-baseline.csv` additionally contains the unmodified full-wet twin; `compare-gate-twins.py` prints the transient comparison. `test25-oracle.py` applies Test 25's aggregate threshold to the independent 128-frame probe data; it does not claim the unchanged test used that input.
