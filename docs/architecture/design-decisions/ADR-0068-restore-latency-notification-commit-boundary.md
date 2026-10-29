# ADR-0068 — Restore completion precedes synchronous latency notification

**Status:** Proposed, 2026-10-30 (Asia/Shanghai). Human Architecture Review required.
**Implementation:** NOT IMPLEMENTED. ADR-0036 remains Accepted and the production protocol is unchanged.

## Context

At main `0ac384088104dfd4dd128ab6751c828c6cd51be8`, the real latency callback in
`--restore-reentry-probe` starts and joins a newer background restore, pumps the real timer door
and saves inside the callback. Host state calls are serialized. Callback-save assertions pass,
but the resumed older tail overwrites the newer preset name: **30 checks / 4 failures**.
`--restore-lock-probe` also witnesses an outer command owning `soundReplacement` after the
adoption's own scoped acquisition ends. That is a lock edge, not a supported-host deadlock.

## Problem

A latency notification currently exposes a half-adopted program to reentry. A completed newer
restore must survive both sound and metadata, and supported callback saves must describe one
coherent program. Do not solve final metadata by hiding pending restores from callback saves.

## Options

| Option | Assessment |
| --- | --- |
| `if (adoptingRestore) return` | Rejected: the callback save can contain `Default` metadata with newer Width 0.6; violates ADR-0036's fixed-point drain |
| Hold `soundReplacement` across the tail/callback | Rejected: a callback joining a background restore can wait on the lock its caller holds; existing State test 27 covers this class |
| Cancel superseded tails with a message-thread application epoch | Viable alternative, but must check inside Settings' per-field loop, cover inline generation-zero restores, suppress obsolete publication/Undo resets, and define saves while a tail is still incomplete. Checking the preset-name store alone is insufficient |
| Publish an active coherent restore snapshot for nested saves | Viable alternative, broader snapshot/overlay ownership contract; must reconcile per-field Settings edits and nested inline restores |
| Complete the program before issuing its latency notification | Proposed: remove the production callout from the unfinished tail; preserve generation, exchange-cell, lock and fixed-point save rules |

## Decision proposed for human review

Support a finite nesting of **synchronous latency callbacks** that perform serialized host state
operations, including the existing newer-background-restore/join/timer-pump/save schedule and
inline message-thread restores. A save issued from those callbacks must synchronously drain
pending restores to the same fixed point as it does now. This does not authorize simultaneous
off-thread state calls or new state operations inside parameter-listener dispatch.

Bracket both restore application paths on the message thread with a restore-application depth:
the scope begins before sound reinstallation/inline sound installation and ends after Settings,
both A/B slots, Undo, preset name/identity/baseline, committed state and the final program snapshot
have committed. A synchronous latency request raised inside this scope records a pending request
on the **existing** D-1 request path rather than calling the host immediately. It does not skip
an adoption, consume a restore without applying it, or change a reported latency value.

Drain the pending latency report only after the restore scope has ended and no enclosing state
command owns `soundReplacement`. Read current committed parameters when delivering; do not retain
the old decode's latency or metadata. There must be **no restore write after this callout**.
Consequently a newer restore invoked from it completes after all older writes, and an older tail
cannot resume to overwrite it. Inline generation zero needs no special ordering comparison:
the same completion/callout boundary applies to both paths.

If an enclosing preset/state command still owns the replacement lock, leave the request set for
the existing 20 Hz timer. That timer must also defer delivery while a restore scope or state
command is active, including when recursively pumped. Outside those scopes, retain synchronous
message-thread reporting. Keep the existing release-store/acquire-exchange, clear-before-read,
single-clear rule so a request raised inside the callback survives for the next delivery.

## Ownership, ordering and boundaries

- The depth counters are message-thread-owned, stack-balanced and not synchronization payloads.
  Check message-thread identity before reading them from `requestLatencyUpdate`; off-thread
  callers continue to publish only the existing atomic request.
- No new thread, mutex, cell, audio-path allocation or wait. Do not read engine-owned mutable
  state from the message thread; use the existing `predictLatency`/snapshot path.
- `adoptingRestore` continues to suppress intermediate program publications, not restore drains.
  Saves from the supported latency callbacks occur after that suppression scope has closed.
- Preserve announcement-before-install, one acquisition spanning take + reinstallation,
  non-consuming try failure, per-field Settings edit precedence, and fixed-point adoption.
- Callbacks from APVTS per-parameter writes and arbitrary host/opposing-lock cycles retain their
  existing support boundary. No native host has been shown to perform the synthetic F9 schedule.
- The outer-held lock witness may remain even though no latency callout is permitted there.
  Keep that residual separately characterized; do not label all of RISK-009 resolved.

## Consequences and compatibility

Sound, fields, preset resolution, A/B/Undo semantics and the latency number stay unchanged for
ordinary finite restores. A restore's latency notification now observes a fully committed program.
Under an enclosing state command, reporting may wait for the existing timer instead of occurring
inside that command. This timing/support-contract change is explicitly gated; the owner's request
to investigate F9 is not approval of this particular design.

## Implementation and acceptance plan

1. Add the bounded message-thread restore scope and centralize safe latency delivery admission.
   Cover every entry to `deliverLatency`, including the timer, prepare and synchronous request.
   Do not add a new blocking flush or change the existing request's atomic ordering.
2. Convert the unchanged failing `--restore-reentry-probe` into a passing default regression.
   Its current four metadata failures are the original-code negative control; retain its callback
   save checks that reject the early-return guard.
3. Add older/newer, inline/background, pump/no-pump and repeated/return schedules. Assert all
   Settings fields, both A/B payloads/active slot, preset identity/baseline/name, Undo baseline,
   and save/reload coherence. Include a callback save with no newer restore: it must describe the
   complete older restore, rather than a partially written Settings tree.
4. Keep the lock probe and add a non-blocking witness that no actual latency callout executes
   under an outer command's lock. Test nested requests and delayed timer reporting for liveness;
   no new indefinite wait or recursive self-flush is allowed.
5. Run full suites, existing TSan probes/canary and sanitizer/static checks, both pluginval modes,
   mandatory native platform gates and targeted human session/automation/latency validation.

## Related code and evidence + confidence

Evidence [Verified]:
- Source: `src/PluginProcessor.cpp` (`adoptPendingHostState`, `adoptRestoreTail`,
  `setStateInformation`, `getStateInformation`, D-1 delivery methods), `src/InternalState.h`
  (`writeResolved` and synchronous oversampling callback), `src/StateCommandGate.h`.
- Tests: `tests/state_hardening_tests.h` (`probeRestoreTailReentry`, `runRestoreAdoptionLockProbe`);
  State test 27 and ADR-0036's fixed-point/adoption regression coverage.
- Current measurements: `worklogs/RELEASE_READINESS_0.10.0.md`; historical rejected guard and
  lock-edge evidence: `worklogs/STATE_HOST_HARDENING_0.9.10.md`.

The failure and lock edge are verified. The proposed completion boundary is **not implemented,
approved or validated**; no P1 closure or release sign-off is claimed.
