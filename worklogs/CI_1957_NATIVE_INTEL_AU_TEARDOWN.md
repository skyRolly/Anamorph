# Build 1957 — native Intel AU teardown investigation

Recorded 2026-10-30, Asia/Shanghai. Owner request: investigate and fix the
macos-intel failure, without weakening the existing validation gate.

## Observed failure

Build 1957 / run 41005362987, source `14237044cb51a80db5656461543c92a066c97ac0`,
job 123898933769, fails in AU randomise pass 2/3 at strictness 10. Seed `0x3e1c70`.
All tests complete and `SUCCESS` is printed, followed by `std::__1::bad_function_call`,
`pluginval received Abort trap: 6, exiting immediately`, and exit 9. The shell gate
correctly classifies a crash and fails immediately; macOS has no crash retry.
Native DSP/state tests and the other three pluginval format/mode groups completed.

The later main Build 1959 / run 41005877545, source `a428f6a`, completes the
same native Intel job (123900549180). Production source is unchanged between
these documentation commits. This establishes intermittency, not crash ownership.
The earlier PR #141 signature is similar, but neither observed tail supplies a
throw stack. Historical wording that assigned it to pluginval itself was stronger
than the retained evidence. No product fix or host-side exemption is established yet.

## Bounded observation

The investigation-only workflow `au-teardown-diagnostic.yml` runs on this single
investigation branch, on macos-15-intel. It reconstructs the original source with
build number 1957 and the normal thin AU configuration. These are reconstructed
bytes, not recovered original bytes; the workflow retains their hash and host/toolchain.
The validator remains the official content-locked v1.0.4 executable. No release,
framework/toolchain pin, conformance strictness, normal test invocation or retry changes.

`diagnose-au-teardown.py` runs its inert outcome controls, then proves the native
debugger can catch a deliberately empty std::function invocation before trying
the real AU. It records SIGABRT before pluginval's handler exits, all thread
stacks and loaded images. A bounded campaign uses the observed data seed with
randomised test ordering. A missing debugger result, launch error, unexpected stop,
timeout or no reproduction is not reported as a successful fix. Capturing the
expected crash is a diagnostic outcome, not a passing pluginval validation.

The original full Build matrix remains active and unchanged. The existing 0.10.0
tag and unpublished draft retain their verified identities; this investigation
does not replace or publish the release. Root-cause and regression records follow
the actual native evidence; no speculative production change is made.

### Observer liveness correction

First diagnostic run 41010691687 reconstructs the thin AU and the native canary
does stop on bad_function_call/SIGABRT. LLDB batch mode then skips ordinary `-o`
commands after a crash, so no result marker/full stack was emitted; the liveness
assertion correctly rejected the observer before any product campaign.
The driver now uses LLDB's `-k` crash hooks as well as normal-exit commands.
The next run retrieves retained artifact 12957996229 (API/archive SHA-256
`94b8543ef4af3a66fe77f24edb8e96fa623fb3481eb56ca8bae76bca19d816d6`),
and additionally verifies the AU binary's independently measured SHA-256
`4f574ba6542c07b79c9d772ceb951ffbab54b81bfdcfe4ad3c83992ee7cc258a`.
It reuses those exact reconstructed bytes instead of silently rebuilding them.

### Reproduced abort; first-throw capture required

Run 41011609262, artifact 12958500629 (SHA-256
`eaf801f71f81d38090243a3f3c7c9a8505772a69a08ea44a76bd3b2b515db66c`),
passes the native canary. Five real invocations exit successfully, then the sixth
reproduces the post-SUCCESS abort. The message-thread stack is already inside
AppKit's exception rethrow; the original C++ throw frame has unwound. The validator
worker is still stopping its timeout thread. This still does not establish blame.
The next observation filters __cxa_throw's x86_64 type_info argument for
bad_function_call and stops before unwinding. Its actual canary must prove that
specific first-throw path, rather than accepting a later abort as equivalent.
