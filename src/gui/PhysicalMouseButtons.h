#pragma once

// ============================================================================
//  PhysicalMouseButtons
//
//  "Is a mouse button PHYSICALLY held right now?" -- asked of the OS, not of
//  JUCE's cached event state.
//
//  This exists because JUCE's own realtime query does not answer it on macOS.
//  In the pinned JUCE 9.0.1, `getNativeRealtimeModifiers` on macOS
//  (juce_NSViewComponentPeer_mac.mm:302-307) refreshes only the KEYBOARD flags
//  via `[NSEvent modifierFlags]` and then returns `ModifierKeys::currentModifiers`
//  -- whose mouse-button bits are the cached ones JUCE maintains from events it
//  received. A release delivered to no JUCE peer never updates them, so the
//  cached bits stay "down" forever and any predicate of the form
//  "logically down AND physically up" is dead on that platform. That is KI-013,
//  and it is why KI-028's abandoned-gesture sweep never ran on macOS.
//
//  The correct API is right there in the same file, 1500 lines below:
//  `[NSEvent pressedMouseButtons]` (juce_NSViewComponentPeer_mac.mm:1867), a
//  global real-time query JUCE uses for tracking-area enter/exit but never wires
//  into the modifier path. This header is that one call, and nothing else.
//
//  Everywhere except macOS, JUCE's realtime query already reads the real state
//  (X11 queries the pointer; Windows asks GetAsyncKeyState), so the portable
//  implementation simply forwards to it and this file adds no behaviour at all.
// ============================================================================

namespace anamorph::gui
{
    // Message thread. On macOS this asks AppKit; elsewhere it forwards to
    // juce::ModifierKeys::getCurrentModifiersRealtime(), which is already
    // authoritative there.
    bool anyPhysicalMouseButtonDown();
}
