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

On a Linux checkout with the repository's normal build dependencies, Git, Ninja,
CMake, a C++ compiler, Python 3 and Xvfb available without elevation:

```bash
git clone --branch 9.0.5 --depth 1 https://github.com/juce-framework/JUCE.git /tmp/anamorph-nr026-juce
git -C /tmp/anamorph-nr026-juce rev-parse HEAD
# Require d60b7a78030661574a615b586d9325ba7a7fa7dd above.
cmake -S . -B build-nr026 -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JUCE=/tmp/anamorph-nr026-juce -DANAMORPH_BUILD_STANDALONE=OFF
cmake --build build-nr026 --target AnamorphStateTests --parallel 2
python3 worklogs/engineering-review/nr026-repro/run.py --build build-nr026 --juce /tmp/anamorph-nr026-juce --out /tmp/nr026-new-observation
python3 worklogs/engineering-review/nr026-repro/verify.py --self-test
python3 worklogs/engineering-review/nr026-repro/verify.py
```

Installing missing system packages is a separate environment-provisioning step.
The optional `scripts/setup-linux.sh full` uses `sudo apt-get` for a non-root
user; preinstalled or rootless dependencies also work, as in the retained run.
The isolation runner itself does not require elevated privileges.

`--out` must be a new path. The runner first builds a read-only path witness
linked to the same JUCE core and production `PresetManager` as the State suite.
It runs the isolation controls, the **complete** State suite, and then all four
ASan cases. It refuses to start the suite or a case if the witness or isolation
controls fail. Each receives a fresh private 0700 `/tmp/anamorph-nr026-*` tree:
`HOME` is its `home`, `XDG_CONFIG_HOME` is `home/.config`, and `TMPDIR` and the
other XDG user directories are separate children. JUCE's resolved home, application
data, preset and temp paths must exactly match these directories before each run.
The preset directory is `home/.config/RollyTech/Anamorph/Presets`; State fixtures
that use `juce::File::createTempFile` also stay inside this tree via `TMPDIR`.
Each Xvfb child and test inherits that environment. The runner rejects a changed
or missing environment variable, a non-private directory, any symlink along those
paths, or a `home/.config/user-dirs.dirs` entry.

This check is necessary because the pinned JUCE Linux `juce_Files_linux.cpp`
uses `HOME`, reads `~/.config/user-dirs.dirs` for an `XDG_CONFIG_HOME` mapping,
and otherwise falls back to `~/.config`. It does **not** use the environment
variable `XDG_CONFIG_HOME` to resolve `userApplicationDataDirectory`.
`PresetManager::presetDirectory()` appends `RollyTech/Anamorph/Presets`.
The actual witness checks those linked implementations, rather than assuming a
conventional XDG implementation. A manually invoked State binary or the general
`scripts/run-tests.sh` is outside this wrapper and does not acquire its isolation.

The nine isolation controls include a fake caller-home preset sentinel, an actual
JUCE configuration redirect, a symlink escape, missing/altered variables, an
unusable temporary parent, two overlapping processes, SIGTERM during a child run
with a grandchild, a read-only control showing the old XDG-only environment still
resolves to the fake caller HOME, and cleanup against a replaced profile root. The
sentinel remains unchanged, and redirected and symlink paths are rejected before
mutating tests run. The root-replacement control confirms cleanup refuses the
replacement and preserves its sentinel. Failed-case logs and the incrementally
written manifest remain in `--out`, outside the profile. On ordinary completion,
failure, SIGINT or SIGTERM the runner stops the child process group and removes only
its own verified temporary tree; a changed root identity is never removed. SIGKILL
or machine loss can leave a private temporary tree behind. This is a Linux test
isolation mechanism, not a security sandbox against a hostile process running as
the same user.

The runner obtains the target's real compile/link commands from Ninja, rebuilds
`PluginEditor.cpp`, all five `juce_gui_basics` units and each probe with
`-O1 -g -fsanitize=address -fno-omit-frame-pointer`, and links those objects
against the unchanged Release target objects with ASan. The JSON command manifest
contains the **exact argv and working directory** for every compilation, link and
invocation in the retained 2026-11-05 observation. The remaining product and JUCE
objects, including parameter dispatch, are not instrumented. `ASAN_OPTIONS` is
`detect_leaks=0:halt_on_error=1:symbolize=1`; leak detection is outside this
lifetime diagnostic. Xvfb and a distinct verified disposable user profile are used
for the State suite and for each case.

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
That historical procedure did not set an isolated HOME: its suite invocation was
direct and its diagnostic runner set only XDG_CONFIG_HOME. Its historical results
are not evidence of preset isolation; no conclusion about actual user-file changes
can be drawn from them. Copies of the then-current README, runner and verifier are
retained as historical text; their original hashes are unchanged.

The separate `observed-isolated-2026-11-05/` directory retains the actual new
command manifest and output streams, including the initial seven isolation controls and
the complete State suite. Its two ASan stderr streams are gzip-compressed with
their original decompressed bytes and raw hashes preserved. The ledger identifies the exact executed runner inputs
by hash (the tooling was uncommitted during that execution), product/JUCE sources,
resolved paths and result identities. Its verifier checks both observations and
has corruption controls. The isolated run again completed 13,959 State checks;
both live cases passed and the two close cases reported their respective ASan UAFs.
The results do not establish teardown gesture completion, Undo or DAW incidence.
The subsequent eighth legacy-XDG control and the independent rerun of all eight
controls are recorded as `control_refresh` in the ledger. The final nine-control
run adds root-identity cleanup and descendant termination checks; their exact
source and raw output are retained separately as `final_controls`. No State or
ASan product scenario changed or was repeated for these test-only additions.

Two deliberate weakening mutations are also retained in the ledger: removing the
profile-root identity check and terminating only the direct child each makes its
specific regression fail. Reproduce them after `run.py` creates the path witness,
using the same runtime-library environment as the witness:

```bash
python3 worklogs/engineering-review/nr026-repro/test_isolation_mutations.py --probe /tmp/nr026-new-observation/path-probe --out /tmp/nr026-mutation-controls
```

The output path must be new. The harness retains each expected failing unittest
stream and exits successfully only when both mutations are rejected.

The retained run used a rootless Debian 13 tool directory with GCC 14.2.0 and
CMake 3.31.6. Its exact configuration command was:

```bash
cmake -S /workspace/Anamorph -B /workspace/.anamorph-tools/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JUCE=/workspace/.anamorph-tools/JUCE -DANAMORPH_BUILD_STANDALONE=OFF -DCMAKE_PREFIX_PATH=/workspace/.anamorph-tools/root/usr
```

That command inherited package include/library paths recorded in the manifest's
`environment` object. The first local build lacked `libxi-dev`; after installing
it in the rootless tool directory, the same configured target built and ran.
This bootstrap interruption is separate from the four measured diagnostic cases.
