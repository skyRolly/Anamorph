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
        juce::File   file;     // user presets only
    };

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

    // The preset "metadata" that must travel WITH a state set through undo / A-B /
    // copy (#6): the base preset name and the clean-signature it was loaded at.
    // isDirty() = (current sound != baseline), so restoring both reproduces the
    // exact name + dirty-star the state had.
    juce::String baseline() const noexcept { return sigAtLoad; }
    void setMeta (const juce::String& name, const juce::String& baselineSig) noexcept
    {
        current = name;
        sigAtLoad = baselineSig;
    }

    void load (int index);                           // message thread only
    bool loadFile (const juce::File&);               // load an arbitrary .anamorph file (OS chooser, #3)
    void step (int delta);                           // prev/next with wrap-around
    bool saveUser (const juce::String& name);        // write + select; false on IO error

    // Host state restore: adopt the remembered name WITHOUT applying anything.
    void adoptRestoredState (const juce::String& name);

    // Undo bracketing (set by the processor). onAboutToLoad fires BEFORE any parameter changes
    // (flush a settled edit into its own step); onLoaded fires AFTER the new name/baseline are set
    // (record ONE undo step for the switch). Only load()/loadFile() fire them -- never session
    // restore, saveUser, or construction. Empty when no processor is bracketing (safe to skip).
    std::function<void()> onAboutToLoad, onLoaded;

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
    juce::String sigAtLoad;
    mutable juce::String cachedSig;     // last signature built by isDirty() (S10)
    mutable juce::uint32 cachedSigGen = 0; // generation it was built at; 0 = never

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace anamorph
