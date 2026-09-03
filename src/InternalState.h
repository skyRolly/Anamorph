#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "SerializedNumber.h"
#include <array>
#include <atomic>
#include <cmath>
#include <functional>

namespace anamorph
{

// ============================================================================
//  InternalState  (host-hidden session parameters)
//
//  The Settings controls (Oversampling, UI Scale, Scope Persistence, Tooltips, UI
//  Animations) and Show Meters are NOT musical parameters: they are view / engine-config
//  state. Exposing them as VST3 parameters cluttered the host's parameter list (REAPER
//  shows every parameter regardless of JUCE's `withAutomatable(false)` flag), so they are
//  deliberately kept OUT of the APVTS / VST3 parameter tree entirely -- the only reliable
//  way to hide a parameter from a host.
//
//  They still:
//    * persist with the session (serialised in get/setStateInformation),
//    * bind two-way to the GUI via juce::Value (ComboBox::getSelectedIdAsValue,
//      ToggleButton::getToggleStateValue, Slider::getValueObject),
//    * and -- for Oversampling only -- drive the DSP (mirrored to an atomic for the
//      audio thread, with a callback so the wrapper can re-report PDC latency).
//
//  They never participate in A/B, Undo or preset recall: Oversampling is a global engine
//  config, not a musical parameter, and the rest are pure view state. The Multiband
//  parameters are intentionally NOT here -- they remain ordinary APVTS parameters.
// ============================================================================
namespace iid
{
    inline const juce::Identifier oversample   { "int_oversample" };  // ComboBox ID (1..4)
    inline const juce::Identifier uiScale       { "int_uiScale" };     // ComboBox ID (1..5)
    inline const juce::Identifier scopePersist  { "int_scopePersist" };// 0..1
    inline const juce::Identifier metersOn       { "int_metersOn" };
    inline const juce::Identifier tooltipsOn     { "int_tooltipsOn" };
    inline const juce::Identifier uiAnimations   { "int_uiAnimations" };
}

class InternalState : private juce::ValueTree::Listener
{
public:
    // The six host-hidden Settings and their DOCUMENTED defaults, in one place.
    // SERIALIZATION_REGISTRY.md's `ANAMORPH_INTERNAL` table is this list: every
    // field is "Required: No" with a stated Default, which is the contract both
    // the constructor (seed) and restoreState (absent -> default) implement. They
    // were two separate hand-written lists until ER-STATE-18; keeping one is what
    // stops them drifting into two different answers for "absent".
    // The DOMAIN travels with the default, for the same reason the default lives
    // here at all: "what this field may hold" and "what it means when it does not"
    // are one contract, and two lists would drift (ER-STATE-18 merged the previous
    // two). `choiceCount` is meaningful only for `comboId`.
    enum class Kind { comboId, unitRange, boolean };
    struct Setting
    {
        const juce::Identifier& id;
        juce::var defaultValue;
        Kind kind;
        int  choiceCount;   // comboId: ids run 1..choiceCount
    };

    static const std::array<Setting, 6>& settings()
    {
        static const std::array<Setting, 6> table {{
            { iid::oversample,   juce::var (1),     Kind::comboId,   4 },  // 1 == "Off (1x)"
            { iid::uiScale,      juce::var (3),     Kind::comboId,   5 },  // 3 == "M"
            { iid::scopePersist, juce::var (0.5),   Kind::unitRange, 0 },
            { iid::metersOn,     juce::var (false), Kind::boolean,   0 },
            { iid::tooltipsOn,   juce::var (false), Kind::boolean,   0 },
            { iid::uiAnimations, juce::var (true),  Kind::boolean,   0 },
        }};
        return table;
    }

    // Is this stored property a number we can actually use, and if so what is it?
    // The ONE predicate both restore paths ask -- the modern repair below and the
    // legacy migration further down, which used to carry its own copy of exactly
    // this logic. `SerializedNumber.h` states why the test is on the INPUT text
    // rather than on the converted value, and why it is stricter than a general
    // parser (it must refuse "inf" and "nan", which JUCE's own reader accepts).
    // A property that is already a typed var -- an int, double or bool, as the
    // defaults table and any live tree carry -- is usable as-is; only a STRING has
    // to earn it.
    static bool usableNumber (const juce::var& prop, double& out) noexcept
    {
        if (prop.isVoid()) return false;
        if (prop.isString() && ! looksLikePlainNumber (prop.toString().trim().toRawUTF8()))
            return false;                                   // "abc", "", "0x10", "inf", "nan"
        const double v = (double) prop;
        // Judged on the float narrowing, as the other paths judge it, so "1e39"
        // (finite as a double, infinite as the float these values were) resolves
        // the same way everywhere.
        if (! isUsableSerializedValue ((float) v)) return false;
        out = v;
        return true;
    }

    // The maintainer-approved recovery for a value that is PRESENT but not valid
    // (decision of 2026-09-02, "Policy B -- repair during restore and persist the
    // repaired value"). Returns a value that is always inside the field's
    // documented domain AND correctly typed, so what restoreState writes back is
    // what a later save persists and a later reload reads unchanged.
    //
    //   * a valid present value is returned unchanged (an in-domain id, a 0..1
    //     persistence, a real boolean);
    //   * a finite out-of-domain number is CLAMPED to the nearest valid value --
    //     and clamped in DOUBLE, before any integer conversion, so the conversion
    //     is defined for every input that reaches it (the discipline ER-STATE-17
    //     established on the legacy path, for the same [conv.fpint] reason);
    //   * anything not usable as a number -- malformed text, non-finite, absent
    //     type -- becomes the field's documented default.
    //
    // A fractional id resolves by truncation after the clamp (2.7 -> 2), which is
    // what the ComboBox already did with it; the repair makes that durable rather
    // than re-deciding it on every load.
    static juce::var repairedValue (const Setting& s, const juce::var& stored)
    {
        if (s.kind == Kind::boolean && stored.isBool())
            return stored;                                   // already a real boolean

        double v = 0.0;
        if (! usableNumber (stored, v))
            return s.defaultValue;

        switch (s.kind)
        {
            case Kind::comboId:   return juce::var ((int) juce::jlimit (1.0, (double) s.choiceCount, v));
            case Kind::unitRange: return juce::var (juce::jlimit (0.0, 1.0, v));
            // A boolean has exactly TWO valid serialized spellings, and they are the
            // two this plug-in's own writer emits: `juce::var(bool)` reaches XML as
            // "0" or "1". Anything else that happens to parse as a number is a
            // MALFORMED value, not a truthy one, and takes the documented default --
            // which is what the approved policy means by "do not allow arbitrary
            // malformed numeric coercion to define durable state" (ER-STATE-22,
            // round 20). Before this, the rule was `v != 0.0`, so a corrupted "-1"
            // or "-2" silently ENABLED a setting the file never asked for, and the
            // repair then persisted that as a real `true`. Note the asymmetry it
            // produced: "0" is the only value that could not turn a setting on.
            // `exactlyEqual` rather than `==`: the comparison IS the intent here
            // (the domain is two exact values), and it is how this repository
            // states that without widening the -Wfloat-equal gate.
            case Kind::boolean:   if (juce::exactlyEqual (v, 0.0)) return juce::var (false);
                                  if (juce::exactlyEqual (v, 1.0)) return juce::var (true);
                                  return s.defaultValue;
        }
        return s.defaultValue;
    }

    InternalState()
    {
        tree = juce::ValueTree ("ANAMORPH_INTERNAL");
        for (const auto& s : settings())
            tree.setProperty (s.id, s.defaultValue, nullptr);
        tree.addListener (this);
        publishFromTree (0);
    }

    ~InternalState() override { tree.removeListener (this); }

    // --- GUI two-way binding (message thread) ----------------------------
    juce::Value oversampleValue()   { return tree.getPropertyAsValue (iid::oversample,   nullptr); }
    juce::Value uiScaleValue()      { return tree.getPropertyAsValue (iid::uiScale,      nullptr); }
    juce::Value scopePersistValue() { return tree.getPropertyAsValue (iid::scopePersist, nullptr); }
    juce::Value metersValue()       { return tree.getPropertyAsValue (iid::metersOn,     nullptr); }
    juce::Value tooltipsValue()     { return tree.getPropertyAsValue (iid::tooltipsOn,   nullptr); }
    juce::Value animationsValue()   { return tree.getPropertyAsValue (iid::uiAnimations, nullptr); }

    // --- DSP read (audio thread, lock-free) ------------------------------
    // The low byte of the engine-config word (see publishEngineConfig): one relaxed
    // 64-bit load, lock-free on every target (asserted below), no ordering role.
    int oversampleIndex() const noexcept { return (int) (engineConfig.load (std::memory_order_relaxed) & kOversampleMask); } // 0..3

    // The SpectrumImager polls UI-animation state through a float atomic (legacy shape):
    // 1.0 == on, 0.0 == off. Mirrors the uiAnimations property.
    const std::atomic<float>* animationsFloatPtr() const noexcept { return &animFloat; }

    // --- message-thread reads (editor) -----------------------------------
    bool  metersOn()     const { return (bool)  tree[iid::metersOn]; }
    bool  tooltipsOn()   const { return (bool)  tree[iid::tooltipsOn]; }
    bool  animationsOn() const { return (bool)  tree[iid::uiAnimations]; }
    float scopePersist() const { return (float) (double) tree[iid::scopePersist]; }
    int   uiScaleIndex() const { return juce::jlimit (0, 4, (int) tree[iid::uiScale] - 1); }

    // Fired (message thread) when Oversampling changes, so the wrapper can re-report PDC.
    std::function<void()> onOversampleChanged;

    // Change generation (H15, Wave 2): bumped on every property change, including a
    // session restore. The Settings widgets bind these values two-way (juce::Value),
    // so they can move with the cursor OUTSIDE the editor -- the editor's micro-anim
    // poll re-arms on this counter instead of hashing widget values every frame.
    juce::uint32 generation() const noexcept { return gen.load (std::memory_order_relaxed); }

    // --- state persistence ----------------------------------------------
    juce::ValueTree copyState() const { return tree.createCopy(); }

    // A restore is two things, and since D-2 (RISK-007, ADR-0036) they happen on
    // two different threads when the host calls setStateInformation off the
    // message thread: RESOLVING what the six fields should become (pure, any
    // thread) and WRITING them into the tree the GUI binds to (message thread
    // only -- the juce::Value bindings read this tree there). The two static
    // resolvers below produce the exact values the former in-place loops wrote,
    // typed as they wrote them; `applyResolved` is the former loop's write half;
    // `publishEngineConfig` is the one part of a restore that must reach the
    // engine SYNCHRONOUSLY on the caller's thread (the oversampling index the
    // audio thread and prepareToPlay read, in the generation-tagged engine-config
    // word), so the ordinary setState-then-activate host order still comes up at
    // the restored oversampling from the first sample -- the LATEST restore's.
    // restoreState / migrateFromLegacyApvts keep their message-thread meaning as
    // resolve-then-apply, so every existing call site and test reads unchanged.
    void restoreState (const juce::ValueTree& src)          { applyResolved (resolveRestore (src)); }
    void migrateFromLegacyApvts (const juce::ValueTree& t)  { applyResolved (resolveLegacy (t)); }

    // The six fields a MODERN session restores to, from its ANAMORPH_INTERNAL node.
    // EVERY field is resolved, present or not: an absent one takes its documented
    // default (the registry's `ANAMORPH_INTERNAL` table), exactly as the legacy
    // resolver does for the legacy shape. `tree` is a processor member and a host
    // restores into ONE live instance repeatedly, so skipping an absent field does
    // not mean "leave it alone" -- it means "keep the PREVIOUS project's value",
    // which is not a state this session ever described. Measured before this loop
    // wrote unconditionally (ER-STATE-18, --partial-settings-probe): a modern
    // session omitting a single Setting inherited the previous project's value in
    // 6 cases out of 6, while the legacy path inherited in 0 -- the reverse of
    // where the review looked. A session that CARRIES the field is unaffected.
    // PRESENT values are REPAIRED to their domain rather than adopted verbatim, and
    // the repaired value is what gets written -- so a later save persists it and a
    // reload reads it back unchanged (the maintainer's Policy B, approved
    // 2026-09-02, ER-STATE-21). Before it, a present-but-invalid value was kept
    // exactly as the file spelled it: measured over nineteen malformed inputs, all
    // nineteen survived into the next save, eight left an out-of-domain ComboBox id
    // in the tree and three left a non-finite scope persistence. A VALID present
    // value is returned unchanged by repairedValue(), so an ordinary session
    // restores exactly as before. Returns an INVALID tree for invalid input, which
    // applyResolved ignores -- the former early return.
    static juce::ValueTree resolveRestore (const juce::ValueTree& src)
    {
        if (! src.isValid()) return {};
        juce::ValueTree out ("ANAMORPH_INTERNAL");
        for (const auto& s : settings())
            out.setProperty (s.id,
                             src.hasProperty (s.id) ? repairedValue (s, src.getProperty (s.id))
                                                    : s.defaultValue,
                             nullptr);
        return out;
    }

    // One-time migration from a pre-0.8.4 session, where these were ordinary APVTS
    // parameters. Old sessions have no ANAMORPH_INTERNAL child, so without this their
    // saved Oversampling / UI Scale / Persistence / Tooltips / Animations / Show Meters
    // would silently revert to defaults. Reads the legacy PARAM nodes (id/value) out of the
    // saved APVTS state and maps them onto the host-hidden InternalState.
    static juce::ValueTree resolveLegacy (const juce::ValueTree& apvtsState)
    {
        if (! apvtsState.isValid()) return {};

        // A legacy PARAM value is used only when it is a USABLE serialized number --
        // the same predicate the session and preset restore paths apply
        // (SerializedNumber.h, ER-STATE-05): plain decimal text, finite after
        // parsing. Anything else means the field's default, exactly as an absent
        // node does. This is not cosmetic: JUCE's text parser accepts "nan" and
        // "inf" as numbers, and the `(int)` conversion below of a NaN, an infinity
        // or an out-of-range double is UNDEFINED BEHAVIOUR (C++ [conv.fpint]).
        // Measured through the real v0.2 restore before this guard existed, on
        // x86-64: every such value became -2147483647 in the tree -- an impossible
        // ComboBox id, saved back out with the session on the next save -- and
        // "2147483647" wrapped to INT_MIN through a second UB, signed overflow in
        // the `+ 1`. On AArch64 the same inputs saturate differently (NaN -> 0,
        // +overflow -> INT_MAX, which the `+ 1` then overflows), so the corruption
        // was also platform-dependent. State test 28 pins every case.
        // The predicate is `usableNumber` above -- ONE copy, shared with the modern
        // repair, which is where this logic used to be duplicated verbatim. The
        // behaviour is unchanged in every case State test 28 pins.
        auto legacy = [&apvtsState] (juce::StringRef id, double fallback) -> double
        {
            for (int i = 0; i < apvtsState.getNumChildren(); ++i)
            {
                auto c = apvtsState.getChild (i);
                if (c.hasType ("PARAM") && c.getProperty ("id").toString() == id)
                {
                    double v = 0.0;
                    return usableNumber (c.getProperty ("value"), v) ? v : fallback;
                }
            }
            return fallback;
        };

        // Choice params stored a 0-based index; the ComboBox IDs here are 1-based.
        // Clamp into the combo's domain IN DOUBLE, before the integer conversion:
        // the conversion is then of a value in [0, count-1], defined for every
        // input this lambda can return, and the `+ 1` cannot overflow. A finite
        // out-of-domain value ("7") lands on the nearest valid choice, which is
        // what NormalisableRange does for an out-of-range parameter.
        auto comboId = [] (double index0, int count) -> int
        {
            return (int) juce::jlimit (0.0, (double) (count - 1), index0) + 1;
        };
        juce::ValueTree out ("ANAMORPH_INTERNAL");
        out.setProperty (iid::oversample,   comboId (legacy ("oversample", 0.0), 4), nullptr); // ids 1..4
        out.setProperty (iid::uiScale,      comboId (legacy ("uiScale",    2.0), 5), nullptr); // ids 1..5
        out.setProperty (iid::scopePersist, juce::jlimit (0.0, 1.0, legacy ("scopePersist", 0.5)), nullptr);
        out.setProperty (iid::metersOn,     legacy ("metersOn",   0.0) > 0.5,     nullptr);
        out.setProperty (iid::tooltipsOn,   legacy ("tooltipsOn", 0.0) > 0.5,     nullptr);
        out.setProperty (iid::uiAnimations, legacy ("uiAnimations", 1.0) > 0.5,   nullptr);
        return out;
    }

    // Write a resolved set into the GUI-bound tree. MESSAGE THREAD ONLY: the
    // juce::Value bindings and the editor's getters read `tree` there, and
    // ValueTree is not a synchronised type. Every field is written, in the table's
    // order, so the listener fires (the word, onOversampleChanged, onChanged)
    // exactly as the in-place loops did. An invalid tree is a no-op (the former
    // early returns for invalid input). This is the INLINE restore -- one that runs
    // on the message thread itself -- which is the newest arrival by definition and
    // therefore replaces every field.
    void applyResolved (const juce::ValueTree& resolved)
    {
        writeResolved (resolved, false, messageGeneration);
    }

    // Write an ADOPTED host-thread restore into the tree (D-2 round 3): the same
    // write, except that a field the user edited AFTER this restore arrived is newer
    // than the restore and is kept. "Arrived" is the restore's synchronous
    // publication on its own thread, which is what gives it its generation; an edit
    // records, against its field, the generation of the latest restore that had
    // arrived when the edit was made (the tag the engine-config word carried -- the
    // one place a restore's arrival is visible to this thread before its adoption).
    // So an edit made before this restore arrived carries a lower generation and is
    // replaced -- the restore is the newer arrival -- and one made after it carries
    // this generation or a later one and stands. That is the ordering every other
    // message-thread mutation already has through adopt-before-use: a user action
    // inside the pending window lands ON TOP of the restore, never under it. The
    // word is republished from the whole tree afterwards, so the two agree whatever
    // the loop kept.
    void adoptResolved (const juce::ValueTree& resolved, juce::uint32 restoreGeneration)
    {
        writeResolved (resolved, true, restoreGeneration);
    }

    // The engine-facing half of a restore (D-2): the oversampling index the audio
    // thread reads every block and prepareToPlay reads to prime the engine, published
    // as ONE tagged word -- the index in the low byte, the generation of the ARRIVAL
    // publishing it in the high 32 bits. The rule is "latest arrival wins": the
    // store lands only if no arrival with a HIGHER generation has published, and the
    // compare-exchange decides that and stores in the same operation, so there is no
    // check-then-store window in which an OLDER restore's completion (its adoption
    // on the message thread, generation g) could overwrite a NEWER restore's value
    // (generation h > g, already published from the host thread). An older arrival
    // publishing the SAME generation again is idempotent (the message thread
    // adopting exactly the restore the host thread published). Returns whether it
    // landed. The tree is untouched; no callback fires; the caller raises the
    // latency request itself. Any thread but the audio thread.
    bool publishEngineConfig (const juce::ValueTree& resolved, juce::uint32 generation) noexcept
    {
        if (! resolved.isValid()) return false;
        return publishOversample (juce::jlimit (0, 3, (int) resolved[iid::oversample] - 1), generation);
    }

    // Message thread: the generation of the last host restore it adopted, which tags
    // every publication the TREE makes from here on -- a Settings edit, an inline
    // restore, an adoption. Set BEFORE an adoption writes the tree, so the tail's
    // own writes carry that restore's generation and yield to a newer one.
    void noteAdoptedGeneration (juce::uint32 g) noexcept { messageGeneration = g; }

    // The generation that published the current engine config (any thread).
    juce::uint32 engineConfigGeneration() const noexcept
    {
        return (juce::uint32) (engineConfig.load (std::memory_order_acquire) >> 32);
    }

    // Fired (message thread) after ANY property of the tree changed -- a Settings
    // edit through a juce::Value binding, or a restore's adoption. The processor
    // republishes its program snapshot from it (D-2). Empty when nothing is wired.
    std::function<void()> onChanged;

private:
    void writeResolved (const juce::ValueTree& resolved, bool adoption, juce::uint32 generation)
    {
        if (! resolved.isValid()) return;
        const juce::ScopedValueSetter<bool> writing (applyingResolved, true);
        const auto& table = settings();
        for (size_t i = 0; i < table.size(); ++i)
        {
            // Edited after this restore arrived: the edit is the newer arrival, it stands.
            if (adoption && (juce::int32) (editGeneration[i] - generation) >= 0) continue;
            tree.setProperty (table[i].id, resolved.getProperty (table[i].id), nullptr);
        }
        // The word follows the tree, whatever the loop kept (idempotent when the
        // callbacks above already published it); a change the loop produced without
        // an oversample property write cannot happen, but the latency re-report is
        // raised on the word's actual move rather than on that reasoning.
        if (publishFromTree (generation) && onOversampleChanged) onOversampleChanged();
    }

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id) override
    {
        gen.fetch_add (1, std::memory_order_relaxed); // H15 re-arm signal
        if (applyingResolved)
        {
            // A restore's write (inline, or an adoption): the word carries the
            // generation the write is for -- noteAdoptedGeneration() for an
            // adoption -- and lands only if no newer host restore has published.
            publishFromTree (messageGeneration);
        }
        else
        {
            // A user edit -- the editor's juce::Value bindings are the only other
            // writer. It is newer than every restore that has arrived so far: the
            // latest of those is the generation the word carries, so the edit is
            // recorded against its field at that generation (adoptResolved keeps
            // it against that restore and every older one) and the word is
            // published with it, so the edit lands over the restore it follows.
            // A restore that arrives LATER carries a higher generation, wins the
            // word, and replaces the field at its adoption -- the newer arrival.
            const auto arrival = engineConfigGeneration();
            const auto& table = settings();
            for (size_t i = 0; i < table.size(); ++i)
                if (table[i].id == id) editGeneration[i] = arrival;
            publishFromTree (arrival);
        }
        if (id == iid::oversample && onOversampleChanged) onOversampleChanged();
        if (onChanged) onChanged();
    }

    // Message thread, from the tree: the animation flag the imager (message thread)
    // polls -- message-thread state on both ends, so a plain mirror the host thread
    // never writes -- and the oversampling index through the tagged word with the
    // given generation. Returns whether the word's INDEX moved.
    bool publishFromTree (juce::uint32 generation) noexcept
    {
        animFloat.store ((bool) tree[iid::uiAnimations] ? 1.0f : 0.0f, std::memory_order_relaxed);
        const int before = oversampleIndex();
        publishOversample (juce::jlimit (0, 3, (int) tree[iid::oversample] - 1), generation);
        return oversampleIndex() != before;
    }

    // The one writer of the engine-config word. Lands iff no arrival with a higher
    // generation has published; the comparison and the store are one CAS.
    bool publishOversample (int index, juce::uint32 generation) noexcept
    {
        const auto fresh = ((juce::uint64) generation << 32) | ((juce::uint64) index & kOversampleMask);
        auto current = engineConfig.load (std::memory_order_acquire);
        do
        {
            if ((juce::int32) ((juce::uint32) (current >> 32) - generation) > 0)
                return false;   // a newer arrival's config stands
        }
        while (! engineConfig.compare_exchange_weak (current, fresh,
                                                     std::memory_order_acq_rel, std::memory_order_acquire));
        return true;
    }

    static constexpr juce::uint64 kOversampleMask = 0xff;

    juce::ValueTree tree;
    // The engine-config word: oversampling index (low byte) tagged with the
    // generation of the arrival that published it (high 32 bits). Audio reads the
    // low byte; the host thread (a restore) and the message thread (the tree) write
    // it through publishOversample. Generation 0 is the constructor's publication.
    std::atomic<juce::uint64> engineConfig { 0 };
    static_assert (std::atomic<juce::uint64>::is_always_lock_free,
                   "the audio thread reads the engine-config word every block");
    juce::uint32 messageGeneration = 0;      // message thread only
    bool applyingResolved = false;           // message thread only: inside writeResolved
    // Per field, the generation of the latest restore that had arrived when the user
    // last edited it (message thread only; 0 = never edited). adoptResolved keeps a
    // field whose value is >= the restore being adopted.
    std::array<juce::uint32, 6> editGeneration {};
    std::atomic<float> animFloat { 1.0f };   // written on the message thread only; polled there
    std::atomic<juce::uint32> gen { 1 };
};

} // namespace anamorph
