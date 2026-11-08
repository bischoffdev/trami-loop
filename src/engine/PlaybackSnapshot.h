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
    std::int64_t audioOffset = 0;
    std::int64_t maxFrames = -1;
    int track = -1;
};

struct TrackMixSnapshot
{
    float volume = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool solo = false;
};

/// Immutable, fully resolved description of what the audio thread plays.
/// Built on the message thread from the Song; the audio thread only reads it.
struct PlaybackSnapshot
{
    std::vector<PlaybackEvent> events; ///< Sorted by startSample.
    std::int64_t lengthSamples = 0;
    std::int64_t loopStartSample = 0;
    std::int64_t loopEndSample = 0;
    bool looping = true;
    double samplesPerBeat = 0.0;
    inline static constexpr int kMaxTrackMixes = 64;
    std::array<TrackMixSnapshot, kMaxTrackMixes> trackMixes{};
    bool hasTrackMixes = false;

    /// Owns the audio referenced by events, so it outlives the snapshot's use.
    std::vector<std::shared_ptr<const AudioData>> keepAlive;
};

} // namespace trami
