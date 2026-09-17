#pragma once

#include <functional>
#include <utility>

#include <juce_core/juce_core.h>

// ============================================================================
//  ARCHITECTURE REVIEW GATE: APPROVED (human architecture review, 2026-09-16).
//  This file is a Thread Model change under `docs/policies/ARCHITECTURE_REVIEW_GATE.md`,
//  which forbids merging one on a green build and which `AI_AGENT_POLICY.md` makes an
//  agent Hard Stop. Its §Procedure was completed and the record is in ADR-0036: step 1
//  (flagged as gated) and step 3 (the ADR section) in §31, step 2 (human review) as the
//  owner's ruling of 2026-09-16 -- *"The `StateCommandGate` architecture is APPROVED and
//  is not to be redesigned"* -- in §32's gate-compliance table, and step 4 (the release
//  compatibility checklist) not reached, because no serialized field, parameter ID or
//  reported latency moves. This banner is the same in-source form `src/PluginProcessor.h`
//  carries for the same gate; round 31 added it here because the ADR held the record and
//  the file did not, which read as an outstanding requirement.
//
//  ADR-0036 ROUND 28 (Devin R802-807, RISK-009). THE ADMISSION EVERY
//  STATE-REPLACING COMMAND PASSES THROUGH.
//
//  WHAT ROUND 27 LEFT OPEN, and why a predicate cannot close it. Round 27 made "is this thread
//  inside a parameter dispatch" ASKABLE (`anamorph::param::insideDispatch`), and wired the answer
//  into the deferred-command flush and the timer retry. It answers for a dispatch THIS PLUG-IN
//  started and for no other. A HOST-started dispatch is indistinguishable from the plug-in side:
//  the host's write enters through the same non-virtual `AudioProcessorParameter::
//  setValueNotifyingHost` the plug-in uses, the listener signatures carry only an index and a
//  value, and the flag that would answer -- JUCE's own `inParameterChangedCallback` -- is a
//  file-static `thread_local` inside the wrapper translation unit, unreachable from here. No
//  plug-in callback BRACKETS the host's either: JUCE calls `finalListener` LAST and returns, so
//  every candidate is a prefix, a non-participant, or something the host's pump DELIVERS.
//
//  SO THE GUARANTEE IS BY CONSTRUCTION, NOT BY DETECTION. The cycle R802 names is
//
//      message thread : holds a parameter's `listenerLock`  ->  WAITS for `soundReplacement`
//      host thread    : holds `soundReplacement`            ->  WAITS for that `listenerLock`
//
//  and the message thread's edge is the only one this plug-in owns. A command that NEVER WAITS for
//  `soundReplacement` cannot supply it, whoever started the dispatch it is nested in and whether or
//  not anything can tell. That is what this gate is: one `tryEnter` at the COMMAND BOUNDARY, held
//  for the whole body, and a deferral when it fails.
//
//  AT THE BOUNDARY, AND HELD -- both halves are load-bearing. Round 26 put a try-lock in
//  `flushDeferredCommands` that was taken and RELEASED before the commands ran, which proved
//  nothing about their own acquisitions; round 27 recorded that. A probe-then-release here would be
//  worse than nothing: between the probe and the first inner acquisition a host thread can take
//  `soundReplacement` and start waiting for the `listenerLock` this thread holds, and the inner
//  acquisition then closes the cycle the probe said was open. Held across the body, every nested
//  acquisition -- `copyStateWithRawValues`, `applyStatePreservingView`, `PresetManager::
//  applySoundTree`, `applyDefaults` -- is a free recursive re-entry (`juce::CriticalSection` is
//  `PTHREAD_MUTEX_RECURSIVE`), and ONE try answers for all of them. It is the shape
//  `pollUndoCoalesceFromTimer` has carried since round 21, moved to where the commands are.
//
//  THE DRAIN IS OUTSIDE THE LOCK, AND THAT IS NOT A DETAIL. `adoptPendingHostState` calls OUT to
//  the host from inside itself -- a restored Oversampling delivers the reported latency
//  synchronously, and `AudioProcessorListener`s run on this thread while it does. Holding a
//  replacement lock across a host callback is the same inversion pointing the other way, and
//  State test 27 (ER-STATE-14) hangs on it: measured, round 21. So the gate drains FIRST, with the
//  non-blocking arm, and REFUSES if the drain did not reach its fixed point -- a command that
//  cannot see the newest session is not a command that should replace it (D-2 section 15). The
//  blocking drain the direct commands used to make is exactly the acquisition R802 cites.
//
//  NOTHING IS DROPPED AND NOTHING IS REORDERED. Every refusal pushes the SAME retry onto the SAME
//  FIFO round 25 built, so a command deferred because a transaction is open and a command deferred
//  because a replacement is in flight queue behind each other in the order the user gave them. The
//  retry doors are the ones that already exist -- the transaction close, the user's next action,
//  and the 20/24 Hz ticks. No new scheduler, no sleep, no timing luck.
// ============================================================================
namespace anamorph
{

// The four answers a gate needs. The processor owns them and `PresetManager` is handed a pointer,
// so both build the same gate from the same source of truth rather than each keeping half of it.
struct StateCommandHooks
{
    // The whole-sound replacement lock (ADR-0036 section 24). Never acquired by waiting from here.
    const juce::CriticalSection* soundReplacement = nullptr;

    // "Is this an extent a state-replacing command must not run in at all?" -- a user transaction
    // is open (round 25), or this thread is inside a dispatch THIS plug-in started (round 27).
    // Deliberately incomplete: it cannot see a host-started dispatch, which is why the try below
    // exists. What it buys is that the OBSERVABLE cases defer rather than merely not-deadlock.
    std::function<bool()> refuseNow;

    // Adopt any pending host restore with the NON-BLOCKING arm and answer whether the cell is now
    // empty. False means a restore is still pending, and the command waits for a door that can
    // take it.
    std::function<bool()> drainToFixedPoint;

    // Round 25's deferred-command FIFO, and the only place a refused command goes.
    std::function<void (std::function<void()>)> enqueue;

    // HOW DEEP IN ADMITTED COMMANDS THIS THREAD ALREADY IS, and it is not a convenience. A command
    // calls other commands -- `undo()` runs `pollUndoCoalesce`, `load` runs `loadAdopted`,
    // `PresetManager::saveUser` runs the processor's re-baseline -- and an INNER gate must not
    // repeat the outer one's two side effects. It must not TRY again (the lock is already this
    // thread's, so the try would succeed and prove nothing new), and above all it must not DRAIN
    // again: the adoption calls out to the host from inside itself, and running that callout
    // underneath a held replacement lock is the inversion State test 27 measured in round 21. At
    // depth > 0 the gate therefore admits immediately and does nothing else.
    //
    // A plain `int`, not an atomic and not a `thread_local`: state commands are message-thread only
    // (the same thread `userTransactionDepth` is counted on), and it must be PER PROCESSOR -- a
    // `thread_local` would let an outer command on one instance silently admit an inner command on
    // another, which the test harness constructs routinely.
    int* nesting = nullptr;
};

class StateCommandGate
{
public:
    // `drainFirst == false` is for an OUTERMOST command whose caller has already drained through
    // some other route and whose contract forbids draining again -- `PresetManager::loadAdopted`
    // called directly, whose row was derived from the session that drain established (section 23).
    // A NESTED command needs no such flag: the depth below answers for it.
    // `afterDrain`, when set, runs between the drain and the try -- the one point in the
    // admission that is after the fixed point and still OUTSIDE the replacement lock. It exists
    // for the two "relative decision" seams (`atRelativeDecision`, `beforeRelativeTarget`), whose
    // whole subject is a restore that arrives after a command's drain and is therefore NOT the
    // session the command acts on (section 23). Empty in production, as every seam is; a nested
    // admission does not run it, because the outer one owns the drain it belongs to.
    StateCommandGate (const StateCommandHooks& hooks,
                      std::function<void()> retry,
                      bool drainFirst = true,
                      const std::function<void()>& afterDrain = {})
    {
        // THE REFUSAL QUESTION IS ASKED FIRST, AHEAD OF THE NESTING SHORTCUT, and a test found out
        // why: `pollUndoCoalesce` is itself an admitted command, so the deferred commands its flush
        // runs are all NESTED -- and a command that opens a transaction of its own and queues work
        // from inside it must still have that work QUEUED, not admitted because an outer gate
        // happens to hold the lock. Nesting answers "the lock is held and the drain has run"; it
        // does not answer "a transaction is open", and round 25's rule is the one that owns that.
        // (State test 101 leg H: with the order the other way round the nested enqueue ran inline
        // and the order came out 1 3 2 instead of 1 2 3.)
        if (hooks.refuseNow && hooks.refuseNow())
        {
            defer (hooks, std::move (retry));
            return;
        }

        // ALREADY INSIDE AN ADMITTED COMMAND ON THIS THREAD. The lock is held, the drain has run,
        // and there is nothing left for this gate to decide: it admits and counts itself so the
        // outermost one still owns the release. See `StateCommandHooks::nesting`.
        if (hooks.nesting != nullptr && *hooks.nesting > 0)
        {
            nesting = hooks.nesting;
            ++*nesting;
            granted = true;
            return;
        }

        // OUTSIDE any lock of ours, and before the try: see the header note.
        if (drainFirst && hooks.drainToFixedPoint && ! hooks.drainToFixedPoint())
        {
            defer (hooks, std::move (retry));
            return;
        }

        if (afterDrain) afterDrain();

        // NO REPLACEMENT LOCK MEANS NOTHING TO GUARD -- AND NOTHING TO DEFER TO (round 32, Devin
        // `src/StateCommandGate.h:R163-172`). A `PresetManager` with no processor wired up is a
        // real, documented configuration: `stateCommandAdmission` hands this gate a default-built
        // `StateCommandHooks` whose every member is empty, and that manager's contract -- stated at
        // its `stateCommand` declaration -- is that every command runs SYNCHRONOUSLY, exactly as it
        // did before round 25. Until round 32 this constructor read the null lock as a failed try
        // and fell through to `defer`, whose `enqueue` is also empty: `saveUser` returned
        // `OpResult::deferred`, nothing was written, and no completion was ever called. The command
        // was not deferred; it was dropped.
        //
        // There is nothing to decide here. The gate exists to keep a state-replacing command from
        // waiting on a lock a host thread may be holding while it waits on us; with no such lock in
        // the configuration there is no cycle to break, no queue to join, and the honest answer is
        // an UNGUARDED admission. Every other refusal above is untouched, and so is the whole
        // behaviour of a configured gate: `refuseNow` and `drainToFixedPoint` are consulted first
        // and are simply empty here, exactly as they were.
        if (hooks.soundReplacement == nullptr)
        {
            nesting = hooks.nesting;
            if (nesting != nullptr) ++*nesting;
            granted = true;
            return;
        }

        if (hooks.soundReplacement->tryEnter())
        {
            replacement = hooks.soundReplacement;
            nesting     = hooks.nesting;
            if (nesting != nullptr) ++*nesting;
            granted     = true;
            return;
        }

        defer (hooks, std::move (retry));
    }

    ~StateCommandGate()
    {
        if (granted && nesting != nullptr)
            --*nesting;

        if (replacement != nullptr)
            replacement->exit();
    }

    // True: run the body, with the replacement lock held for its whole duration. False: the
    // command has been queued -- return immediately and touch nothing.
    [[nodiscard]] bool admitted() const noexcept { return granted; }

    StateCommandGate (const StateCommandGate&)            = delete;
    StateCommandGate& operator= (const StateCommandGate&) = delete;
    StateCommandGate (StateCommandGate&&)                 = delete;
    StateCommandGate& operator= (StateCommandGate&&)      = delete;

private:
    static void defer (const StateCommandHooks& hooks, std::function<void()> retry)
    {
        // A REFUSAL WITH NOWHERE TO PUT THE COMMAND IS A DROPPED COMMAND, and round 32 made that
        // loud rather than silent. The one configuration that used to reach here with an empty
        // queue -- no `soundReplacement` at all -- is admitted above and never arrives; what is
        // left is a half-wired set (a lock but no FIFO), which is incoherent and would lose the
        // user's action. The assertion says so in a debug build; release behaviour is unchanged.
        jassert (hooks.enqueue != nullptr);
        if (hooks.enqueue) hooks.enqueue (std::move (retry));
    }

    const juce::CriticalSection* replacement = nullptr;   // non-null only while HELD
    int* nesting = nullptr;                               // non-null only while this gate counts
    bool granted = false;
};

} // namespace anamorph
