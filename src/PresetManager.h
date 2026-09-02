#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "PluginParameters.h"

namespace anamorph
{

// ============================================================================
//  PresetManager  (F2)
//
//  FabFilter-style preset switching for the top bar: a flat list of FACTORY
//  presets (built in, defined as overrides on the default sound) followed by
//  USER presets (XML files in the local preset folder). Loading and saving only
//  ever touch the SOUND parameters -- the shared view/Settings params
//  (pid::viewParams) are never part of a preset, exactly like A/B and undo.
//
//  A preset load arrives as plain setValueNotifyingHost calls that open NO
//  gesture, so the processor's gesture-gated undo coalescer (ADR-0008) would
//  otherwise fold it into the baseline WITHOUT an undo step. The onAboutToLoad /
//  onLoaded hooks let the processor bracket each load and record the switch as a
//  single undoable step -- a preset change is a discrete, undoable user action.
// ============================================================================
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);

    struct Entry
    {
        juce::String name;
        bool         isFactory = false;
        juce::File   file;      // user presets only
        juce::String factoryId; // factory presets only -- immutable internal identity (#4)
    };

    // What PRODUCED the current sound, as opposed to what it is CALLED.
    //
    // The list used to be searched by NAME, so a user preset saved under a factory
    // preset's name could never be the "current" entry: the name matched the factory
    // row first and the drop-down tick stayed there no matter which of the two was
    // actually loaded. Identity therefore no longer travels as a name: a factory
    // preset is its immutable `factoryId` (the menu still SHOWS the name, and Save
    // Preset still pre-fills the name), and a user preset is its file on disk. The
    // two namespaces cannot collide, so a duplicate name is now merely a duplicate
    // label.
    //
    // It rides along with a state set through A/B and undo (both in-memory), and since
    // 0.9.2 it is ALSO persisted with the session so the indicator survives reopening a
    // project (ADR-0024 amendment). What is persisted is three additive strings in the
    // PLUG-IN state only -- user preset FILES keep their format byte-for-byte, and the
    // parameter state restores independently of whether the identity resolves.
    struct Selection
    {
        enum class Kind { unknown, factory, userFile };
        Kind         kind = Kind::unknown;
        juce::String factoryId;  // kind == factory
        juce::File   file;       // kind == userFile

        // "Is this the same row?" -- only the fields the kind actually uses take part, so a
        // stale factoryId left on a userFile selection cannot make two equal rows compare
        // different. Used to decide whether a preset load MOVED the identity (#4).
        bool operator== (const Selection& o) const noexcept
        {
            if (kind != o.kind) return false;
            switch (kind)
            {
                case Kind::factory:  return factoryId == o.factoryId;
                case Kind::userFile: return file == o.file;
                case Kind::unknown:
                default:             return true;
            }
        }
        bool operator!= (const Selection& o) const noexcept { return ! operator== (o); }
    };

    // The wire form of a Selection: three plain strings, so the encoding lives with the
    // type rather than being spelled out at each serialization site.
    //
    // A user preset sitting DIRECTLY in the preset folder is stored as its FILE NAME, not
    // its full path: there the name is already a complete identity (`refresh()` scans the
    // folder non-recursively, so only direct children can ever be menu rows), it keeps the
    // user's home directory out of the saved project, and a project moved to another
    // machine still resolves. Anything else stores its absolute path, which keeps
    // `decode(encode(s)) == s` true: a file loaded from outside the folder, one nested in a
    // sub-folder of it (whose bare name would decode to a DIFFERENT same-named file sitting
    // directly in the folder), and one whose name juce::File::isAbsolutePath would accept --
    // a leading `~` on POSIX -- which would decode to a literal relative path instead.
    struct SelectionFields { juce::String kind, factoryId, userFile; };
    static SelectionFields encodeSelection (const Selection&);
    static Selection       decodeSelection (const juce::String& kind,
                                            const juce::String& factoryId,
                                            const juce::String& userFile);

    // Local user-preset folder (created on demand):
    //   macOS  ~/Library/Application Support/RollyTech/Anamorph/Presets
    //   Win    %APPDATA%/RollyTech/Anamorph/Presets
    //   Linux  ~/.config/RollyTech/Anamorph/Presets
    static juce::File presetDirectory();
    static juce::String fileSuffix() { return ".anamorph"; } // shared with the OS chooser filter (#3)

    // The label a manager carries before any session, preset load or save -- and therefore also
    // the right answer for a session that predates the `presetName` field (< 0.6). It is a
    // CONSTANT on purpose: the restore path needs a fallback for that case, and the obvious
    // candidate -- currentName() -- is whatever the PREVIOUS project left on this instance,
    // since hosts reuse one processor across setStateInformation calls.
    static juce::String defaultName() { return "Default"; }

    void refresh();                                  // rescan the user folder
    const juce::Array<Entry>& entries() const noexcept { return list; }

    juce::String currentName() const noexcept { return current; }
    int  currentIndex() const noexcept;              // -1 when name not in list
    bool isDirty() const;                            // sound edited since load/save

    // (Selection also travels with the SESSION since 0.9.2 -- see encodeSelection below.)
    // The preset "metadata" that must travel WITH a state set through undo / A-B /
    // copy (#6): the base preset name and the clean-signature it was loaded at.
    // isDirty() = (current sound != baseline), so restoring both reproduces the
    // exact name + dirty-star the state had.
    juce::String baseline() const noexcept { return sigAtLoad; }
    const Selection& selection() const noexcept { return sel; }
    // `sourceSel` carries the identity (#4) so an A/B switch, an undo or a session restore
    // puts the tick back on the row that actually produced the sound. A pre-0.9.2 session
    // has none to carry, and passes the default (unknown) -> the name fallback.
    //
    // An EMPTY `baselineSig` means the state being adopted never recorded one. The only thing
    // that produces it is a pre-0.6.4 A/B slot, which stored parameters ALONE -- every in-memory
    // producer (the constructor, load, loadFile, saveUser, adoptRestoredState, and therefore
    // currentStateSet and every undo / redo / A-B / copy snapshot built from it) always fills it.
    // "No baseline" is not "modified": soundSig() is never empty, so a literal "" would compare
    // unequal to every possible sound and report the slot dirty forever -- a modified-marker on a
    // preset the slot does not even have (it has no name either). The state being adopted becomes
    // its own clean baseline instead, which is exactly the rule adoptRestoredState() applies to a
    // session root that carries no `presetBaseline`.
    //
    // PRECONDITION for that fallback, not enforced by the signature: the parameters this metadata
    // describes must ALREADY be applied. soundSig() reads the LIVE apvts, so "its own clean
    // baseline" means "the sound in force right now" -- correct only because every caller applies
    // first (applyStateSet does applyStatePreservingView() then this; setStateInformation restores
    // the ANAMORPH child long before the adoption block). A caller that adopted metadata BEFORE
    // applying its parameters would baseline against the outgoing sound and mis-report the dirty
    // star from then on, silently. Adopt after applying, always.
    //
    // There is deliberately NO identity-less overload. One existed until 0.9.2 and it made
    // "forget which row produced this sound" -- the mis-tick ADR-0024 exists to remove -- something
    // a caller could do without writing it down. Pass `Selection()` explicitly to mean it.
    void setMeta (const juce::String& name, const juce::String& baselineSig,
                  const Selection& sourceSel)
    {
        current = name;
        sigAtLoad = baselineSig.isNotEmpty() ? baselineSig : soundSig();
        sel = sourceSel;
    }

    void load (int index);                           // message thread only
    bool loadFile (const juce::File&);               // load an arbitrary .anamorph file (OS chooser, #3)
    void step (int delta);                           // prev/next with wrap-around
    bool saveUser (const juce::String& name);        // write + select; false on IO error

    // Host state restore: adopt the remembered name + identity WITHOUT applying anything.
    // `restoredSel` is whatever the session carried -- `Selection()` (unknown) for a pre-0.9.2
    // session, which is the name fallback -- and it is METADATA ONLY: it never touches a
    // parameter, so the sound
    // restores identically whether or not the identity resolves. Its baseline is derived from the
    // live sound, so the same "apply the parameters first" precondition as setMeta applies. Like
    // setMeta it takes the identity explicitly -- the one-argument overload was removed with
    // setMeta's for the same reason.
    void adoptRestoredState (const juce::String& name, const Selection& restoredSel);

    // Undo bracketing (set by the processor). onAboutToLoad fires BEFORE any parameter changes
    // (flush a settled edit into its own step); onLoaded fires AFTER the new name/baseline are set
    // (record ONE undo step for the switch). Only load()/loadFile() fire them -- never session
    // restore, saveUser, or construction. Empty when no processor is bracketing (safe to skip).
    std::function<void()> onAboutToLoad, onLoaded;

    // Fired by saveUser() after the new name/identity/baseline are in place. A save changes no
    // parameter value, so the processor's gesture-gated coalescer never notices it and its
    // `committed` snapshot would keep the PRE-save name, baseline and identity forever -- the
    // next undo would then restore them and yank the tick back onto the row that was current
    // before the save. Re-baselining here creates no undo step, which is right: saving is not a
    // sound change. Empty when no processor wires it up (safe to skip).
    std::function<void()> onSaved;

    // S10: set by the processor -- generation counter of the sound-parameter
    // values, bumped on every value change. Lets isDirty() reuse its last
    // BUILT signature while provably nothing changed (the comparison against
    // sigAtLoad stays live, so load/save/undo need no invalidation hooks).
    // Empty when no processor wires it up -> isDirty always rebuilds (safe).
    std::function<juce::uint32 ()> soundParamGeneration;

private:
    void applyDefaults();
    // Parse a preset file into its sound tree, or return an INVALID tree if the
    // file is not an Anamorph preset. Both loaders resolve through this, so the
    // root-type rule cannot hold on one path and not the other (ER-STATE-24).
    juce::ValueTree parseSoundFile (const juce::File&) const;
    // PRECONDITION: `state` has already been accepted by parseSoundFile. This
    // function cannot tell a foreign tree from ours -- it looks parameters up by
    // the `id` PROPERTY, which matches under any root -- so the type check has
    // to happen before it, not inside it.
    void applySoundTree (const juce::ValueTree& state);
    void resetSolo();                                // force the per-band solo off (#9)
    juce::String soundSig() const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<Entry> list;
    juce::String current { defaultName() };
    Selection    sel;                   // identity of `current` (#4); unknown after a session restore
    juce::String sigAtLoad;
    mutable juce::String cachedSig;     // last signature built by isDirty() (S10)
    mutable juce::uint32 cachedSigGen = 0; // generation it was built at; 0 = never

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace anamorph
