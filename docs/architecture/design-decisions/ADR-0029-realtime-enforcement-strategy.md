# ADR-0029 — Realtime-safety enforcement: RealtimeSanitizer at the audio entry point, and what deliberately stays out

**Status:** **Accepted** (Build System + validation change — maintainer approval 2026-08-18)

**Follows from:** `ADR-0028` §"RealtimeSanitizer remains reachable and deliberately not taken", which
recorded this as its own decision needing its own ADR. That sentence is now discharged.

## Context

`REALTIME_AUDIO_POLICY.md` is the repository's **Priority-1** policy. Its rule is unconditional —
the audio thread must be *deterministic, lock-free and allocation-free* — and it names a hard red
line of forbidden operations (`new`/`malloc`, any container resize, mutex/lock, blocking wait,
file/network IO, `sleep`, thread creation, `std::async`, exceptions thrown on the path).

Until this ADR, **nothing in CI could detect a violation of it.** The enforcement was:

- **Human review** against the forbidden list, and
- **`docs/architecture/REALTIME_SAFETY_AUDIT.md`**, a hand-written per-module audit.

Neither is a gate. The existing dynamic tools answer different questions and none of them asks
*where* an allocation happened: ASan finds out-of-bounds and lifetime bugs, UBSan finds undefined
behaviour, valgrind memcheck finds uninitialised reads. A `malloc` added to `AnamorphEngine::process`
is, to every one of them, a perfectly correct allocation.

The audit itself recorded the gap. Its §"Items needing a non-static check" asked for exactly this:
*"a sanitizer/RT-audit run … would upgrade the 'no allocation inside JUCE's oversampler call'
assumption from inferred to measured."*

Clang's **RealtimeSanitizer** (RTSan, `-fsanitize=realtime`) is the tool built for this question, and
the pinned toolchain already ships it: `libclang_rt.rtsan-x86_64.a` is present in the clang-22
install that `scripts/setup-llvm-apt.sh` produces (ADR-0028).

## Problem

Adopting a realtime checker is not one decision but five, and getting any of them wrong produces
either a gate that cannot fail or noise that trains people to ignore it:

1. **What is annotated** — the annotation is a source change on a DSP_POLICY-frozen audio path.
2. **Whether the compile-time diagnostic (`-Wfunction-effects`) comes with it.**
3. **How the runtime lane relates to the existing `sanitizers` job.**
4. **What the lane's failure semantics are.**
5. **What covers the platforms RTSan does not** — it is Clang-only, and two of the three shipped
   binaries are built by GCC, MSVC and AppleClang.

## Options

- **A. Do nothing; keep review + the manual audit.** Rejected. The Priority-1 policy stays the only
  binding constraint in the repository with no mechanical check, and the audit is a document that
  rots — its own anchors had drifted by 50+ lines before this change set corrected them.
- **B. Annotate the audio path broadly and enable `-Wfunction-effects` as a compile-time gate.**
  Rejected **on measurement**: 52 warnings from the single annotated engine TU, dominated by calls
  whose definitions the TU cannot see (`juce::dsp::Oversampling::reset` ×9,
  `juce::FloatVectorOperationsBase::copy` ×6, `juce::AudioBuffer::clear` ×4). Clang can only infer a
  callee's effects from a *visible definition*; every JUCE call is opaque to it, and JUCE 9.0.1
  carries **zero** annotations of its own (measured: no occurrence of `clang::nonblocking`,
  `clang::nonallocating` or `__rtsan` anywhere in the pinned checkout). The warnings are about
  correct code. Adopting this would mean either a 52-entry baseline of non-defects or annotating
  a dependency this repository does not own.
- **C. Fold RTSan into the existing `sanitizers` job.** **Impossible, not merely undesirable:** the
  clang-22 driver rejects `-fsanitize=realtime` combined with `address`, `undefined`, the job's
  actual `address,undefined,vptr` set, or `thread` — *"invalid argument '-fsanitize=realtime' not
  allowed with …"*. RTSan requires its own binary.
- **D. Run RTSan as a non-blocking survey step** (`RTSAN_OPTIONS=halt_on_error=false`). Rejected on
  demonstrated semantics: in that mode the process **prints violation reports and still exits 0**.
  A step that reports and passes is the "gate that cannot fail" this repository's testing policy is
  written against (`TESTING_POLICY.md` rule 4).
- **E. Annotate the audio-thread ENTRY POINT only, enforce it at runtime in a dedicated lane, and
  leave `-Wfunction-effects` off until the dependency can answer it.** **Chosen.**

## Decision

### 1. The realtime properties being protected

Exactly those `REALTIME_AUDIO_POLICY.md` already binds — no allocation, no lock, no blocking call,
no IO on the audio path. **This ADR adds no new constraint**; it adds the first mechanical detector
for the constraint that already exists. Where the two could ever disagree, the Policy governs.

### 2. RTSan's role: the runtime enforcer, at one annotated entry point

`AnamorphEngine::process` carries `ANAMORPH_NONBLOCKING` (`src/dsp/RealtimeAnnotations.h`). It is the
engine's audio-thread entry point, and the entire serial DSP chain runs inside it, so **one
annotation places the whole chain under enforcement** — the sanitizer follows real calls at runtime
and needs no annotation on the callees.

The annotation is a **type attribute**, written after the parameter list and after `noexcept`. The
prefix spelling is a hard compile error, not a warning: *"'clang::nonblocking' attribute cannot be
applied to a declaration"*.

It is guarded by `__has_cpp_attribute(clang::nonblocking)` and expands to nothing elsewhere, because
three of the four shipped toolchains are not the pinned Clang. GCC 13 compiles the guarded macro
cleanly and emits `-Wattributes: scoped attribute directive ignored` on the *raw* spelling — the
guard is what keeps that out of the Clang warning gate.

**It changes no code.** Verified by object comparison rather than argued: the real
`src/dsp/AnamorphEngine.cpp`, compiled by clang-22 at `-O3` with the project's flags, produces a
**byte-identical** object with the attribute live and with the macro emptied. That is what permits
the annotation on a DSP_POLICY-frozen path at all.

### 3. `-Wfunction-effects` is deliberately NOT enabled

Option B's measurement is the reason. The rule this ADR sets: **the annotation marks entry points for
the runtime tool; the compile-time diagnostic waits for the dependency.** The re-evaluation trigger
is stated in §8.

`-Wperf-constraint-implies-noexcept`, which ADR-0028 paired with this decision, is also **not**
enabled — but for the opposite reason: it is a no-op here. It fires on definitions whose function
effects imply `noexcept`, and `AnamorphEngine::process` is *already* declared `noexcept`. Enabling it
would gate on nothing.

### 4. The CI lane, and its failure semantics

A new **`realtime`** job builds `AnamorphTests` only, with `-fsanitize=realtime` on the clang-22
toolchain the repository already pins, and runs it. Three properties are load-bearing:

- **Its own build directory and its own ccache lineage** — forced by option C's driver restriction.
- **Default `RTSAN_OPTIONS`.** The job sets none. RTSan halts on the first violation by default
  (exit **43**); the one setting that would break the gate is `halt_on_error=false`, so the lane's
  correctness depends on *not* configuring it. This is stated in the workflow comment.
- **A liveness canary runs first**, per `TESTING_POLICY.md` rule 4: a tiny translation unit whose
  annotated function allocates is compiled and run, and the step **fails if it does not abort**.
  Without it a lane whose instrumentation silently stopped working is indistinguishable from a clean
  audio path.

  The canary asserts **two** things — a non-zero exit *and* the sanitizer's own
  `ERROR: RealtimeSanitizer` report — because the first alone is not enough: a canary that died for
  an unrelated reason would pass. The two-part assertion was not theoretical caution. The first
  draft grepped for the bare word `RealtimeSanitizer`, which the canary's **own failure message**
  contained, so an uninstrumented build satisfied it and a dead lane reported itself live. It was
  caught by running the step's logic against a deliberately uninstrumented build before the job
  ever ran in CI — which is the same "prove it can fail" discipline the canary itself exists for,
  applied one level up. The canary's message no longer carries the token, and the workflow greps
  the report signature.

Only `AnamorphTests` runs under RTSan. `AnamorphStateTests` drives the wrapper, but its audio-path
coverage is one test; the DSP suite is where the audio path is exercised across the whole feature
matrix, and RTSan's value is proportional to path coverage.

### 5. Known limitations, stated so they are not rediscovered as surprises

- **Coverage is what the suite executes.** RTSan is a runtime tool: an unexercised branch containing
  a `malloc` is invisible. The DSP suite's matrix is the coverage.
- **Optimizer elision.** At `-O2`/`-O3` Clang can delete a non-escaping `malloc`/`free` pair before
  the RTSan pass sees it, so a *synthetic* canary of that shape can pass. Real allocations through
  JUCE (`AudioBuffer`, `HeapBlock`) are **not** elided and are caught — this is why the canary in §4
  is written against a real allocation rather than a throwaway pair.
- **Weakening overrides are not diagnosed.** Clang accepts an override that *drops* `nonblocking`
  from an annotated virtual without any diagnostic, even under `-Wfunction-effects`. The enforcement
  is the runtime context, not the type system.
- **Platform reach.** Linux and macOS only, Clang only. The shipped Windows (MSVC) and macOS
  (AppleClang) binaries are never built by this lane.
- **Third-party frames.** A violation inside JUCE is reported at the JUCE frame. That is correct
  behaviour — it is still a violation on this project's audio path — but the fix may be a call-site
  change here rather than a defect there.

### 6. False positives and noise

None observed: the DSP suite runs violation-free under RTSan at both `-O2`-class (RelWithDebInfo)
and `-O0` (§Evidence). The realistic future sources are lazy one-time initialisation on a first call
and third-party internals. The response to a future report is to **investigate the source**, not to
add a suppression: `RTSAN_OPTIONS=suppressions=` exists and this repository deliberately does not
use it, because a suppression file is where a real regression eventually hides. If one is ever
genuinely required it is a change to this ADR.

### 7. What covers the platforms RTSan cannot, and its status

Two complementary mechanisms were **demonstrated** during the investigation that produced this ADR:

- An **allocation guard** — replaceable `operator new`/`delete` plus malloc-family interposition
  compiled into the test binary, armed only around `process()` calls. Measured 7,680 armed calls
  across 32 configurations with **zero** allocations, and caught both seeded violation classes
  (`std::vector` growth via `operator new`; `juce::AudioBuffer::setSize` via raw `malloc`).
- A **static realtime lint** over `src/dsp` for the forbidden-token classes, in the shape of the
  repository's existing grep-lints with their `--self-test` convention.

Their value is precisely where RTSan does not reach: the `operator new` half is standard-guaranteed
replaceable and therefore works under **MSVC**, and the static lint runs on every platform with no
build at all.

**Both were implemented in the change set immediately following this ADR** (2026-08-18, maintainer
approval). The plan above survived contact with two corrections worth recording, because both
change what a future reader should believe:

- **No CMake change was needed after all.** The guard is a header included by `tests/dsp_tests.cpp`
  (`tests/AllocationGuard.h`); the one build that must exclude it does so through a compile flag on
  that job's existing configure line. So the review-gated Build System change this section
  anticipated never arose.
- **The valgrind hazard is real but was mischaracterised here.** It is not `vgpreload` replacing the
  interposers: memcheck tracks which allocator produced each block and intercepts the `new`/`delete`
  and `malloc`/`free` families **separately**, so an `operator new` that returns `std::malloc` memory
  is reported as *"Mismatched free() / delete / delete []"* on every later delete. Measured against
  the real JUCE-linked suite under the pipeline's exact invocation, where it fails the step. A small
  standalone probe does **not** reproduce it — which is how the earlier characterisation arose, and
  is why the note now names the binary that does. The resolution is the same either way: that build
  compiles the guard out (`-DANAMORPH_NO_ALLOC_GUARD`) and the test discloses and skips.

The ASan hazard was confirmed as written: an executable-defined `malloc` fights ASan's allocator, so
the malloc half is preprocessed out there while the `operator new` half keeps asserting.

**Measured coverage of the shipped guard**, across four configurations of the same suite:

| configuration | `operator new` | malloc family | outcome |
|---|---|---|---|
| GCC Release (`linux`, `merge-check`) | live | live | 3,840 armed calls, 0 allocations |
| Clang + RTSan (`realtime`) | live | live | 3,840 armed calls, 0 allocations |
| Clang + ASan/UBSan (`sanitizers`) | live | compiled out, disclosed | 3,840 armed calls, 0 allocations |
| GCC + valgrind (`sanitizers`) | compiled out, disclosed | compiled out | memcheck 0 errors |

Both violation classes were seeded into the real `AnamorphEngine::process` and caught: a
`juce::AudioBuffer` (the raw-malloc route JUCE actually uses — `worst per call: malloc=1`) and a
`std::vector` growth (the `operator new` route, the half that works on MSVC — `new=1`). Each failed
the suite with exit 1.

### 8. How the strategy evolves

Each of these is a stated trigger, not an open-ended intention:

- **JUCE gains function-effect annotations** → re-evaluate `-Wfunction-effects` (option B). The
  measurement to repeat is the 52-warning census.
- **Clang diagnoses effect-weakening overrides** → the type system starts carrying part of the
  contract, and `processBlock` becomes worth annotating.
- **RTSan gains a Windows implementation** → re-evaluate the platform gap that §7's mechanisms exist
  to cover.
- **A violation is reported that is not a project defect** → §6 governs: investigate, and change this
  ADR rather than adding a suppression.
- **The Clang pin moves** (ADR-0028) → the lane follows `ANAMORPH_CLANG_VERSION` automatically; no
  action unless RTSan's own behaviour changed.

### 9. Maintenance implications

One annotation, one guarded header, one CI job, one canary. The annotation cannot silently rot: if
the entry point is renamed the build fails. The lane's cost is one extra build of the DSP suite
(~20 ninja edges, ccache-able) and ~1.3 s of runtime. The canary is the maintenance the repository
already performs for its four lints.

### 10. Scope of this change, and the next step

**In this change:** `RealtimeAnnotations.h`, the annotation on `AnamorphEngine::process`, the
`realtime` CI job with its canary, and the documentation sync.

**Delivered in the immediately following change (2026-08-18):** the allocation guard
(`tests/AllocationGuard.h` + `testProcessIsAllocationFree`) and the static realtime lint
(`scripts/check-realtime.py`, in `source-lint` with its own `--self-test`), completing the three
tiers. The static lint's role is narrow and deliberately so: it is the only tier that reads code the
suite never executes (measured coverage of `src/dsp` is 93.4 % of lines / 79.9 % of branches), and it
is function-scoped because `prepare()` is required to allocate — a file-wide token scan would flag
the eight legitimate `setSize` calls in `AnamorphEngine.cpp` and be switched off.

## Consequences

- `REALTIME_AUDIO_POLICY.md`'s rule acquires its first mechanical detector; a future `malloc` on the
  audio path fails a job instead of surviving to a user's DAW.
- `REALTIME_SAFETY_AUDIT.md` stops being the only evidence for its own claims. Its
  oversampler TODO is now **partially** answered by measurement (§Evidence) — the ADR does not close
  it, because the interposition probe that produced that number is not a committed gate.
- The repository accepts a Clang-only, Linux/macOS-only runtime gate as the first tier, with the
  cross-platform tier scheduled rather than shipped. That asymmetry is deliberate and is §7.
- One more CI job on every push. It is not in any `needs:` chain inside `build.yml`, matching
  `sanitizers` and `linux-lto-tests` — a realtime finding does not withhold the per-push binary the
  behavioural gates passed. **On the release path it does block**, and that is correct rather than
  incidental: `release.yml` calls `build.yml` as one job and `draft-release` depends on its
  aggregate result, which is what `RELEASE_POLICY.md` §Artifacts means by reusing the `build.yml`
  gates unchanged. (Corrected 2026-08-18 from the original wording, which stated the non-blocking
  half without its release-path scope; the decision is unchanged.)
- No change to any shipped byte, any DSP behaviour, or any policy text. This ADR **amends no Policy**;
  it implements enforcement for one that already exists.

## Related code

- `src/dsp/RealtimeAnnotations.h` — the guarded macro and its reasoning
- `src/dsp/AnamorphEngine.h` — the annotated audio-thread entry point
- `.github/workflows/build.yml` — the `realtime` job (canary, then the suite)
- `tests/realtime_canary.cpp` — the liveness canary
- `docs/policies/REALTIME_AUDIO_POLICY.md` — the rule being enforced
- `docs/architecture/REALTIME_SAFETY_AUDIT.md` — the manual audit this supplements
- `docs/architecture/design-decisions/ADR-0028-clang-toolchain-pin.md` — the pin that makes the
  runtime available, and the sentence this ADR discharges

Evidence [Verified]:
- **Toolchain**: `libclang_rt.rtsan-x86_64.a` present in the pinned clang-22 install;
  `sanitizer/rtsan_interface.h` ships beside it.
- **Detection**: an annotated function calling `pthread_mutex_lock` under `-fsanitize=realtime` at
  `-O2` aborts with exit **43** and a symbolized `RealtimeSanitizer: unsafe-library-call` report.
- **Gate semantics**: the same binary under `RTSAN_OPTIONS=halt_on_error=false` prints the reports
  and exits **0**.
- **Composability**: clang-22 rejects `realtime` with each of `address`, `undefined`,
  `address,undefined,vptr` and `thread`.
- **Behaviour-neutrality**: `src/dsp/AnamorphEngine.cpp` compiled by clang-22 at `-O3` with project
  flags is **byte-identical** with the attribute live and with `-DANAMORPH_NONBLOCKING=`.
- **Portability guard**: `g++-13 -Wall -Wextra -Wattributes` compiles the guarded macro with no
  diagnostic and emits `-Wattributes` on the raw spelling; the macro expands to
  `[[clang::nonblocking]]` under clang-22 and to nothing under g++-13.
- **`-Wfunction-effects` census**: **52** warnings from the annotated engine TU; the flag is in
  neither `-Wall` nor `-Wextra`.
- **JUCE 9.0.1**: zero occurrences of `clang::nonblocking` / `clang::nonallocating` / `__rtsan` in
  the pinned checkout.
- **The audio path under RTSan**, and **the lane failing on a seeded violation**: see the run
  recorded in `docs/DOCUMENTATION_COVERAGE.md` for this change set.
- Policy: `docs/policies/REALTIME_AUDIO_POLICY.md`; `docs/policies/TESTING_POLICY.md` rule 4 (the
  canary requirement); `docs/policies/ARCHITECTURE_REVIEW_GATE.md` (why §7's guard waits).
