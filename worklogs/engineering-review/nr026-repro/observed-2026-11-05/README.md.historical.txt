# NR-026 JUCE 9.0.5 component diagnostics

This is nonshipping evidence for Architecture Review. The product inputs are the
`f1d3ff81e0f239d0bec7c8873f54ebfce78e15c7` tree and JUCE 9.0.5 at
`d60b7a78030661574a615b586d9325ba7a7fa7dd`. The runner refuses a different
JUCE HEAD, a dirty JUCE tree, changed product build inputs, or an incomplete list
of the five `juce_gui_basics` translation units. The source SHA in the ledger is
for the product's build inputs; later documentation-only branch commits do not
change those inputs.

`original-probe.cpp` is byte-for-byte the `gui-inventory-probe-v2.cpp` content
embedded in `gui-restore-lifetime.json` (SHA-256 `0d78b980…ae336a5`). Its
`wheel live` and `wheel close` selectors exercise the actual attached Width
knob's standalone wheel handler. `held-wheel-probe.cpp` is a **new** diagnostic
based on the event and control fixture in State test 88; the historical held
probe with recorded hash `e20828e5…10c5a` was not retained. It calls the
actual knob `mouseDown`, then delivers a held-button wheel event to the
production `wheelTakenByAnyPress` route. In both probes, a listener on the real
Width parameter either keeps the editor alive or destroys it synchronously during
the wheel's parameter notification. Both cases start from Width 0.50 and use a
positive `deltaY=0.2` notch. They require a working X11 display; the commands
below supply Xvfb. They do not simulate a DAW or native hardware acceptance.

On a fresh Linux checkout with the repository's normal build dependencies:

```bash
./scripts/setup-linux.sh full
git clone --branch 9.0.5 --depth 1 https://github.com/juce-framework/JUCE.git /tmp/anamorph-nr026-juce
git -C /tmp/anamorph-nr026-juce rev-parse HEAD
# Require d60b7a78030661574a615b586d9325ba7a7fa7dd above.
cmake -S . -B build-nr026 -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JUCE=/tmp/anamorph-nr026-juce -DANAMORPH_BUILD_STANDALONE=OFF
cmake --build build-nr026 --target AnamorphStateTests --parallel 2
xvfb-run -a build-nr026/AnamorphStateTests_artefacts/Release/AnamorphStateTests > /tmp/nr026-state.stdout 2> /tmp/nr026-state.stderr
python3 worklogs/engineering-review/nr026-repro/run.py --build build-nr026 --juce /tmp/anamorph-nr026-juce --out /tmp/nr026-new-observation
python3 worklogs/engineering-review/nr026-repro/verify.py
```

The runner obtains the target's real compile/link commands from Ninja, rebuilds
`PluginEditor.cpp`, all five `juce_gui_basics` units and each probe with
`-O1 -g -fsanitize=address -fno-omit-frame-pointer`, and links those objects
against the unchanged Release target objects with ASan. The JSON command manifest
contains the **exact argv and working directory** for every compilation, link and
invocation in the retained 2026-11-05 observation. The remaining product and JUCE
objects, including parameter dispatch, are not instrumented. `ASAN_OPTIONS` is
`detect_leaks=0:halt_on_error=1:symbolize=1`; leak detection is outside this
lifetime diagnostic. Xvfb and a distinct XDG profile are used for each case.

A live case must exit 0, move Width 0.50→0.53 and balance one gesture begin/end.
A close case is expected to exit 1 with ASan heap-use-after-free: the standalone
read is in `Slider::ScopedDragNotification::~ScopedDragNotification`, and the held
read is in `Slider::getValue` from `Knob::takeWheelNotch` after `setValue` returns.
The close runs terminate at the ASan report, so neither their gesture completion
nor their Undo history is observable. State 86/88 cover ordinary gestures and Undo
only; the full state-suite output here records 13,959 checks with zero failures.
The runner classifies expected crashes as diagnostic observations, separate from
normal passing CI tests.

The `observed-2026-11-05/` directory retains the actual stdout and stderr of
all four cases, the full state-suite output, and the command manifest. The two nonempty
ASan teardown stderr files are gzip-compressed without changing their decompressed bytes;
the manifest hashes the raw output and the verifier checks it after decompression. The JSON
ledger lists their SHA-256 values and labels this as a **new** observation. Its verifier
cross-checks the ledger result/hash fields against the command manifest and retained outputs. The
original 9.0.5 command manifest, held probe, raw output and binaries were not
recoverable; their historical hashes are recorded only as unretained identifiers.
Binaries from this new run are intentionally not committed (each is about 162 MB);
rebuild them using the commands above. The observed binaries' local hashes remain
in the manifest for provenance, not as a claim that their bytes are archived.

The retained run used a rootless Debian 13 tool directory with GCC 14.2.0 and
CMake 3.31.6. Its exact configuration command was:

```bash
cmake -S /workspace/Anamorph -B /workspace/.anamorph-tools/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JUCE=/workspace/.anamorph-tools/JUCE -DANAMORPH_BUILD_STANDALONE=OFF -DCMAKE_PREFIX_PATH=/workspace/.anamorph-tools/root/usr
```

That command inherited package include/library paths recorded in the manifest's
`environment` object. The first local build lacked `libxi-dev`; after installing
it in the rootless tool directory, the same configured target built and ran.
This bootstrap interruption is separate from the four measured diagnostic cases.
