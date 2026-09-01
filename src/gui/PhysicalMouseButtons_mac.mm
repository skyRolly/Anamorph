#include "PhysicalMouseButtons.h"

#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_MAC

#import <AppKit/AppKit.h>

namespace anamorph::gui
{
    // The whole macOS half of KI-013/KI-028, in one call.
    //
    // `[NSEvent pressedMouseButtons]` is a global, real-time bitmask of the
    // physically held buttons -- it does not depend on this process having
    // received the corresponding events, which is exactly the case JUCE's cached
    // modifier state cannot represent. A release that happened over another
    // application, or over the desktop, is visible here and nowhere else.
    //
    // Cheap enough for the editor's 24 Hz reconcile: it reads state AppKit
    // already maintains for the window server, with no round trip.
    bool anyPhysicalMouseButtonDown()
    {
        return [NSEvent pressedMouseButtons] != 0;
    }
}

#endif
