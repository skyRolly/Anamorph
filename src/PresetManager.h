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
    };

    // The wire form of a Selection: three plain strings, so the encoding lives with the
    // type rather than being spelled out at each serialization site.
    //
    // A user preset sitting DIRECTLY in the preset folder is stored as its FILE NAME, not
    // its full path: there the name is already a complete identity (`refresh()` scans the
    // folder non-recursively, so only direct children can ever be menu rows), it keeps the
    // user's home directory out of the saved project, and a project moved to another
    // machine still resolves. Anything else -- a file loaded from outside the folder, or
    // one nested in a sub-folder of it -- stores its absolute path, which keeps
    // `decode(encode(s)) == s` true rather than silently re-pointing it at a same-named
    // file sitting directly in the folder.
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
    void setMeta (const juce::String& name, const juce::String& baselineSig,
                  const Selection& sourceSel) noexcept
    {
        current = name;
        sigAtLoad = baselineSig;
        sel = sourceSel;
    }
    // Convenience for callers with no identity to carry (currently tests only).
    void setMeta (const juce::String& name, const juce::String& baselineSig) noexcept
    {
        setMeta (name, baselineSig, Selection());
    }

    void load (int index);                           // message thread only
    bool loadFile (const juce::File&);               // load an arbitrary .anamorph file (OS chooser, #3)
    void step (int delta);                           // prev/next with wrap-around
    bool saveUser (const juce::String& name);        // write + select; false on IO error

    // Host state restore: adopt the remembered name + identity WITHOUT applying anything.
    // `restoredSel` is whatever the session carried (default = unknown, i.e. a pre-0.9.2
    // session), and it is METADATA ONLY -- it never touches a parameter, so the sound
    // restores identically whether or not the identity resolves.
    void adoptRestoredState (const juce::String& name, const Selection& restoredSel);
    void adoptRestoredState (const juce::String& name) { adoptRestoredState (name, Selection()); }

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
    void applySoundTree (const juce::ValueTree& state);
    void resetSolo();                                // force the per-band solo off (#9)
    juce::String soundSig() const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<Entry> list;
    juce::String current { "Default" };
    Selection    sel;                   // identity of `current` (#4); unknown after a session restore
    juce::String sigAtLoad;
    mutable juce::String cachedSig;     // last signature built by isDirty() (S10)
    mutable juce::uint32 cachedSigGen = 0; // generation it was built at; 0 = never

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace anamorph
