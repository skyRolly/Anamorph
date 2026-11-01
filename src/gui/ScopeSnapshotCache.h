#pragma once

#include "../dsp/ScopeBuffer.h"
#include <vector>

namespace anamorph::gui
{
// GUI-owned, serialized by message thread / JUCE paint lock. Allocate at
// construction only. Scratch tail scans do not replace the last accepted
// paint/FFT window. No reader state or ownership reaches the producer.
class ScopeSnapshotCache
{
public:
    explicit ScopeSnapshotCache (int frames)
        : limit (std::clamp (frames, 0, ScopeBuffer::capacity)),
          pendingL (static_cast<std::size_t> (limit)), pendingR (pendingL.size()),
          acceptedL (pendingL.size()), acceptedR (pendingL.size()) {}

    ScopeBuffer::ReadResult readLatest (const ScopeBuffer& scope, int count) noexcept
    {
        return scope.readLatestSnapshot (pendingL.data(), pendingR.data(), std::min (count, limit));
    }

    ScopeBuffer::ReadResult readSince (const ScopeBuffer& scope, std::uint64_t since) noexcept
    {
        return scope.readLatestSince (pendingL.data(), pendingR.data(), since, limit);
    }

    // Accept this cache's latest scratch read once, only on complete success.
    bool accept (ScopeBuffer::ReadResult result) noexcept
    {
        if (! result.valid || result.copied < 0 || result.copied > limit) return false;
        pendingL.swap (acceptedL);
        pendingR.swap (acceptedR);
        accepted = result;
        return true;
    }

    const std::vector<float>& scratchLeft() const noexcept { return pendingL; }
    const std::vector<float>& scratchRight() const noexcept { return pendingR; }
    const std::vector<float>& left() const noexcept { return acceptedL; }
    const std::vector<float>& right() const noexcept { return acceptedR; }
    ScopeBuffer::ReadResult result() const noexcept { return accepted; }

private:
    const int limit;
    std::vector<float> pendingL, pendingR, acceptedL, acceptedR;
    ScopeBuffer::ReadResult accepted { true, 0, 0 };
};
} // namespace anamorph::gui
