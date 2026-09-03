# REALTIME_AUDIO_POLICY.md

**Priority: 1 (highest).** System Policy — derived from the actual audio-path implementation.
Binding constraint on `processBlock` / `AnamorphEngine::process` and every DSP module's
`process`/`reset`.

## Rule

The audio thread must be **deterministic, lock-free, and allocation-free**. Every operation on
the audio path must be O(1) or bounded/deterministic in time.

## Forbidden on the audio thread (hard red line)

`new` / `delete` · `malloc` / `free` · any heap allocation or container resize (`std::vector`
resize/`push_back`, `juce::AudioBuffer::setSize`) · `mutex` / `lock` / `condition_variable` ·
blocking waits · filesystem IO · network IO · `sleep` · C++ exceptions thrown on the path ·
`future` / `promise` / `std::async` · thread creation · `system()` / `fork()` / subprocess.

## Permitted

- Reads/writes of pre-allocated buffers and scalar state.
- Atomic loads/stores (relaxed for published meters; release/acquire for the scope ring).
- In-place IIR coefficient recompute (`LR4Xover::setCutoffFrequency`) — bounded.
- `std::fill` over a pre-sized buffer (no resize) — e.g. `reset()` and Velvet's transport-stop flush.
- Transcendental functions (`tanh`, `sin`, `log10`, `pow`) — bounded, no allocation.
- `juce::ScopedNoDenormals` (required; active for the whole block).

## Current compliance

**Verified** across all 12 DSP modules + the processor: no `new`/`malloc`/resize/mutex/IO on any
audio path; all allocation confined to `prepare()`. Full audit: `docs/architecture/REALTIME_SAFETY_AUDIT.md`.

Evidence [Verified]:
- Source: src/PluginProcessor.cpp:296-364 (`processBlock`; `ScopedNoDenormals` at :119), src/dsp/AnamorphEngine.cpp:43-181 (prepare allocations) vs :660-1339 (alloc-free process)
- Audit: docs/architecture/REALTIME_SAFETY_AUDIT.md

## Enforcement

- **RealtimeSanitizer, in CI, on every push** (ADR-0029). `AnamorphEngine::process` carries
  `ANAMORPH_NONBLOCKING` (`src/dsp/RealtimeAnnotations.h`), and the `realtime` job builds the DSP
  suite with `-fsanitize=realtime` and runs it: an allocation, lock or blocking call anywhere in the
  chain **below the annotated `process` entry** aborts the job at the offending frame. The
  per-block wrapper path *above* it (`processBlock` → `PluginParameters::toEngine` →
  `AnamorphEngine::setParameters`) is outside RTSan's enforcement; its non-allocation classes are
  gated by `check-realtime.py`, which seeds those names directly (ER-RT-02, 2026-08-31), and its
  allocations by Test 38's armed guard. This is the first mechanical detector for the rule
  above — ASan, UBSan and valgrind all treat an audio-path allocation as perfectly correct code.
  Its bounds are stated in the ADR and are real: it is Clang/Linux+macOS only, it sees only what the
  suite executes, and the shipped Windows and macOS binaries are built by compilers it never runs on.
  A **liveness canary** in the same job proves the lane can fail before its silence is trusted.
- **An allocation guard compiled into the DSP suite** (`tests/AllocationGuard.h`, Test 38) counts
  `operator new` and malloc-family allocations while `process()` runs and asserts zero across the
  whole algorithm × oversampling matrix. It exists because RTSan cannot reach every shipped
  toolchain: `operator new` replacement is standard C++ and therefore works under **MSVC**, where
  RTSan does not run. It proves its counters are live before reporting a zero, and any configuration
  in which a half is inactive says so with a `::warning::` rather than passing silently. It stands
  down entirely under RealtimeSanitizer, where its interposers would otherwise shadow the
  sanitizer's own allocation interceptors — RTSan is the stronger detector in that build.
- **A static lint over audio-path bodies** (`scripts/check-realtime.py`, in `source-lint` with its
  own `--self-test`). Both runtime tiers see only the code the suite executes; this one reads the
  branches it never takes, on every platform, with no build. Its scope is this rule's scope: the
  wrapper `processBlock`, `AnamorphEngine::process`, every module's `reset`/`softReset`, and —
  since ER-RT-02 — `setParameters`/`toEngine`, which the audio thread also enters every block.
  It is function-scoped — `prepare()` is required to allocate, so a file-wide scan would flag the
  legitimate sizing there. Scoped does NOT mean "seeds only": the lint computes the transitive set
  of bodies those seeds reach, so a helper is scanned because it is CALLED from an audio path, not
  because of what it is named.
  **That closure is same-file.** A callee whose definition lives in another translation unit is not
  text this lint has, and it is covered only if its own name is a seed. The boundary is deliberate
  and is not a redesign candidate: `scripts/check-realtime.py` carries the round-3 census showing
  the cross-file gap currently holds no violation (every forbidden-class match in a DSP unit the
  audio thread reaches cross-file sits in that module's own `prepare()`), and the two RUNTIME tiers
  cover the same ground from the other side. Re-measure that census before assuming it still holds.
- Any change touching an audio path is reviewed against this list.
- Buffer sizing must happen in `prepare()`. If a feature needs more scratch, grow it in
  `prepare()`, never in `process()`.
- A change that could introduce an unbounded or per-block allocation triggers the
  **Architecture Review Gate** and an **AI Agent Hard Stop** (`AI_AGENT_POLICY.md`).
- Changing this policy requires an ADR.
