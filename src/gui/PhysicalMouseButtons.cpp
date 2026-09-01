#include "PhysicalMouseButtons.h"

#include <juce_gui_basics/juce_gui_basics.h>

#if ! JUCE_MAC

namespace anamorph::gui
{
    // X11 queries the pointer and Windows calls GetAsyncKeyState inside
    // getNativeRealtimeModifiers, so JUCE's own answer is the physical one here
    // and this is a pure forward. The macOS half lives in the .mm beside this
    // file; see the header for why it has to.
    bool anyPhysicalMouseButtonDown()
    {
        return juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown();
    }
}

#endif
