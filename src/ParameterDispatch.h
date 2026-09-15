#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ============================================================================
//  ADR-0036 ROUND 27 (Devin R1390). THE DYNAMIC EXTENT OF A PARAMETER DISPATCH,
//  MADE ASKABLE.
//
//  THE QUESTION THIS TYPE ANSWERS, and why nothing else in the tree could answer it.
//  Rounds 21-26 built one rule and never had a way to TEST it: *nothing that can block on
//  `soundReplacement` may execute from the dynamic extent of a parameter listener callback*
//  (THREADING_POLICY; `copyStateWithRawValues` has carried the sentence since round 18). JUCE
//  holds a parameter's `listenerLock` across the WHOLE dispatch -- every plug-in listener and
//  then the `finalListener`, which is JUCE's own `AudioProcessor::ParameterChangeForwarder`
//  relaying to every `AudioProcessorListener`, i.e. to the HOST
//  (juce_AudioProcessorParameter.cpp:78-85, :101-108, :111-120). A host that pumps its message
//  loop from one of those callbacks therefore runs timers, UI events and queued messages with
//  that lock held. Everything the pump delivers is inside the extent; nothing in it knew.
//
//  WHY THE LISTENER SIDE CANNOT ANSWER IT, established in round 21 and re-confirmed here. A
//  depth counter kept around THIS plug-in's own listener body reads zero at exactly the moment
//  it would need to read one: JUCE calls the `finalListener` LAST, after our listener has
//  returned, so the host's pump -- which happens inside the forwarder -- sees a counter that has
//  already been decremented. Registration order cannot be relied on to fix that either; the
//  forwarder is always last by construction.
//
//  SO THE BRACKET IS ON THE CALL, NOT ON THE CALLBACK. `setValueNotifyingHost`,
//  `beginChangeGesture` and `endChangeGesture` are NON-VIRTUAL on `juce::AudioProcessorParameter`
//  (juce_AudioProcessorParameter.h:141, :149, :156), so a parameter subclass cannot intercept
//  them and the only place left is the caller. Every one of those calls that this plug-in makes
//  goes through the four functions below, and `scripts/check-dispatch.py` fails the build on a
//  raw member call anywhere in `src/` outside this header -- which is what makes the set COMPLETE
//  rather than merely large. `AudioProcessorValueTreeState::replaceState` is in the set for the
//  same reason: JUCE pushes every parameter through `setValueNotifyingHost` from inside it.
//
//  ROUND 26 REJECTED THIS AS NON-MINIMAL ON A COUNT THAT WAS WRONG, and the correction is
//  recorded rather than quietly dropped. ADR-0036 section 29 said there were "43 raw parameter-
//  write calls across 15 functions in the imager alone"; that count came from a grep that matched
//  the API names inside COMMENTS, of which this tree has many. The real figure is 30 call sites
//  across four files, and 15 of them are the imager's. A wrong count is why the cheaper fix was
//  refused, so the count is now a lint rather than a recollection.
//
//  THREAD-LOCAL, NOT A MEMBER, AND NOT AN ATOMIC. The question is "does THIS thread's stack
//  contain a dispatch", which is a property of a stack and of nothing else. A shared counter
//  would be wrong twice: a host thread inside `applySoundTree` would make the message thread
//  refuse for no reason, and the two threads racing on the same int is a data race
//  ThreadSanitizer would report (correctly). One `thread_local int` costs a TLS load on paths
//  that are already doing a full parameter dispatch.
//
//  WHAT A NON-ZERO DEPTH MEANS TO A READER: *you may be inside a parameter listener's dynamic
//  extent, so you may not block on `soundReplacement`.* It is deliberately CONSERVATIVE -- the
//  bracket covers the whole call including the part before JUCE takes the lock -- because
//  over-refusing costs one retry and under-refusing is the deadlock.
// ============================================================================
namespace anamorph::param
{

// The depth itself. Public because `PluginEditor.h`'s attachment straddle (below) raises and
// lowers it across two separate listener callbacks and cannot use a scope object; everything
// else in the tree goes through `ScopedDispatch` or the four wrappers.
inline thread_local int dispatchDepth = 0;

// "Is this thread inside a parameter dispatch this plug-in started?"
[[nodiscard]] inline bool insideDispatch() noexcept { return dispatchDepth > 0; }

// The raise/lower pair for a caller that cannot hold a scope across the extent. Every OTHER
// caller uses `ScopedDispatch`, which cannot leak.
inline void enterDispatch() noexcept { ++dispatchDepth; }
inline void exitDispatch()  noexcept { --dispatchDepth; }

// RAII, and the reason the wrappers below are functions rather than macros: a dispatch that
// throws (JUCE does not, but a listener might) still lowers the depth on the way out. A leaked
// raise would make `flushDeferredCommands` refuse FOREVER, which is the "strand indefinitely"
// failure the whole mechanism exists to avoid -- so the only unscoped raise in the tree carries
// its own balance check and its own destructor backstop (see `AttachmentWitness`).
struct ScopedDispatch
{
    ScopedDispatch()  noexcept { enterDispatch(); }
    ~ScopedDispatch() noexcept { exitDispatch(); }
    ScopedDispatch (const ScopedDispatch&)            = delete;
    ScopedDispatch& operator= (const ScopedDispatch&) = delete;
};

// ---- THE FOUR WRAPPED ENTRY POINTS. The raw member calls below are the ONLY ones ------------
// ---- `scripts/check-dispatch.py` permits anywhere in `src/`. -------------------------------

inline void setValueNotifyingHost (juce::AudioProcessorParameter* p, float normalised)
{
    if (p == nullptr) return;
    const ScopedDispatch dispatching;
    p->setValueNotifyingHost (normalised);
}

inline void beginChangeGesture (juce::AudioProcessorParameter* p)
{
    if (p == nullptr) return;
    const ScopedDispatch dispatching;
    p->beginChangeGesture();
}

inline void endChangeGesture (juce::AudioProcessorParameter* p)
{
    if (p == nullptr) return;
    const ScopedDispatch dispatching;
    p->endChangeGesture();
}

// NOT a parameter call, and in the set anyway. `replaceState` hands JUCE a tree and JUCE pushes
// every parameter out of it through `setValueNotifyingHost` -- so the whole of it is a parameter
// dispatch, made from inside a held `soundReplacement` by both of this plug-in's whole-sound
// replacements. A host pumping from one of those pushes used to reach the deferred flush with
// nothing to tell it that a replacement was half-applied underneath.
inline void replaceState (juce::AudioProcessorValueTreeState& apvts, const juce::ValueTree& tree)
{
    const ScopedDispatch dispatching;
    apvts.replaceState (tree);
}

} // namespace anamorph::param
