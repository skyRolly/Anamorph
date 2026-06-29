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
- In-place IIR coefficient recompute (`LinkwitzRileyFilter::setCutoffFrequency`) — bounded.
- `std::fill` over a pre-sized buffer (no resize) — e.g. `reset()` and Velvet's transport-stop flush.
- Transcendental functions (`tanh`, `sin`, `log10`, `pow`) — bounded, no allocation.
- `juce::ScopedNoDenormals` (required; active for the whole block).

## Current compliance

**Verified** across all 12 DSP modules + the processor: no `new`/`malloc`/resize/mutex/IO on any
audio path; all allocation confined to `prepare()`. Full audit: `docs/architecture/REALTIME_SAFETY_AUDIT.md`.

Evidence [Verified]:
- Source: src/PluginProcessor.cpp:64-131 (`ScopedNoDenormals`), src/dsp/AnamorphEngine.cpp:26-113 (prepare allocations) vs :472-899 (alloc-free process)
- Audit: docs/architecture/REALTIME_SAFETY_AUDIT.md

## Enforcement

- Any change touching an audio path is reviewed against this list.
- Buffer sizing must happen in `prepare()`. If a feature needs more scratch, grow it in
  `prepare()`, never in `process()`.
- A change that could introduce an unbounded or per-block allocation triggers the
  **Architecture Review Gate** and an **AI Agent Hard Stop** (`AI_AGENT_POLICY.md`).
- Changing this policy requires an ADR.
