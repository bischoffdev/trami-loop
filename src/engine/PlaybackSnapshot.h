#pragma once

#include "engine/AudioData.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace trami
{

struct PlaybackEnvelopeNode
{
    float positionNorm = 0.0f; ///< [0.0 .. 1.0] across the event duration
    float value = 1.0f;
    std::uint8_t curve = 0;    ///< 0 = Linear, 1 = Fast, 2 = Slow
};

inline constexpr int kMaxPlaybackEnvNodes = 32;

inline float evaluatePlaybackEnvelope(const std::array<PlaybackEnvelopeNode, kMaxPlaybackEnvNodes>& nodes,
                                      int count, float normPos, float defaultVal) noexcept
{
    if (count <= 0)
        return defaultVal;
    if (normPos <= nodes[0].positionNorm)
        return nodes[0].value;
    if (normPos >= nodes[static_cast<std::size_t>(count - 1)].positionNorm)
        return nodes[static_cast<std::size_t>(count - 1)].value;

    for (int i = 0; i + 1 < count; ++i)
    {
        const auto& p0 = nodes[static_cast<std::size_t>(i)];
        const auto& p1 = nodes[static_cast<std::size_t>(i + 1)];
        if (normPos >= p0.positionNorm && normPos <= p1.positionNorm)
        {
            const float span = p1.positionNorm - p0.positionNorm;
            if (span <= 1e-6f)
                return p1.value;
            const float t = std::clamp((normPos - p0.positionNorm) / span, 0.0f, 1.0f);
            float shaped = t;
            if (p0.curve == 1)
                shaped = std::sqrt(t);
            else if (p0.curve == 2)
                shaped = t * t;
            return p0.value + shaped * (p1.value - p0.value);
        }
    }
    return nodes[static_cast<std::size_t>(count - 1)].value;
}

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
    std::int64_t fadeInSamples = 0;
    std::int64_t fadeOutSamples = 0;
    std::uint32_t clipId = 0;
    bool reversed = false;
    bool hasClipFx = false;
    float fxSend = 0.0f;
    std::array<PlaybackEnvelopeNode, kMaxPlaybackEnvNodes> volEnv{};
    int volEnvCount = 0;
    std::array<PlaybackEnvelopeNode, kMaxPlaybackEnvNodes> panEnv{};
    int panEnvCount = 0;
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

    std::array<PlaybackEnvelopeNode, kMaxPlaybackEnvNodes> masterVolEnv{};
    int masterVolEnvCount = 0;

    /// Owns the audio referenced by events, so it outlives the snapshot's use.
    std::vector<std::shared_ptr<const AudioData>> keepAlive;
};

} // namespace trami
