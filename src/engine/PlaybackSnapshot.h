#pragma once

#include "engine/AudioData.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace trami
{

/// One sample trigger at an exact position on the playback timeline.
struct PlaybackEvent
{
    std::int64_t startSample = 0;
    const AudioData* audio = nullptr;
    float gainLeft = 1.0f;
    float gainRight = 1.0f;
};

/// Immutable, fully resolved description of what the audio thread plays.
/// Built on the message thread from the Song; the audio thread only reads it.
struct PlaybackSnapshot
{
    std::vector<PlaybackEvent> events; ///< Sorted by startSample.
    std::int64_t lengthSamples = 0;
    bool looping = true;
    double samplesPerBeat = 0.0;

    /// Owns the audio referenced by events, so it outlives the snapshot's use.
    std::vector<std::shared_ptr<const AudioData>> keepAlive;
};

} // namespace trami
