#pragma once

#include <vector>

namespace trami
{

/// Decoded audio at the engine's sample rate. Mono (1 channel) or stereo (2).
/// Immutable once handed to the engine.
struct AudioData
{
    std::vector<std::vector<float>> channels;

    int numChannels() const noexcept { return static_cast<int>(channels.size()); }
    int numFrames() const noexcept { return channels.empty() ? 0 : static_cast<int>(channels.front().size()); }

    const float* left() const noexcept { return channels.front().data(); }
    const float* right() const noexcept { return channels.back().data(); } ///< Same as left() for mono.
};

} // namespace trami
