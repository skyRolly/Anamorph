#pragma once

// ============================================================================
//  AbSlotIndex
//
//  Single source of truth for clamping a restored A/B slot index. `abActive` is
//  read out of session state (the AB child's "active" property) and used to
//  index the size-2 `abSlot[]` / `abUndo[]` arrays. A hand-edited, corrupted, or
//  forward-version blob can carry an out-of-range value, which would otherwise be
//  an out-of-bounds read/write. Clamp it to a valid slot on restore.
//
//  Dependency-free on purpose so the headless self-test harness (which does NOT
//  link juce_audio_processors) can guard the exact production invariant.
// ============================================================================
namespace anamorph
{
    // Valid A/B slots are 0 (A) and 1 (B). Any out-of-range index clamps to the
    // nearest valid slot, so indexing abSlot[]/abUndo[] is always in bounds.
    constexpr int kNumAbSlots = 2;

    inline constexpr int clampAbSlotIndex (int raw) noexcept
    {
        return raw < 0 ? 0 : (raw > kNumAbSlots - 1 ? kNumAbSlots - 1 : raw);
    }
}
