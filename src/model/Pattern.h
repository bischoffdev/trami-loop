#pragma once

#include "model/Ids.h"
#include "model/Sample.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace trami
{

inline constexpr int kTracksPerPattern = 16;

/// A sample placed on a pattern track at a 1/16 grid step.
struct Clip
{
    ClipId id;
    SampleId sampleId;
    int startStep = 0;
    int lengthSteps = 0; ///< Custom length in steps. 0 = default/natural duration.
    int offsetSteps = 0; ///< Start offset in steps into the audio.
};

/// Mixer settings shared by pattern and arranger tracks.
struct TrackMix
{
    float volume = 1.0f; ///< Linear gain, 0..2.
    float pan = 0.0f;    ///< -1 (left) .. +1 (right).
};

struct PatternTrack
{
    std::string name;
    TrackMix mix;
    bool muted = false;
    bool solo = false;
    SampleId sampleId;
    std::optional<SampleKind> kindOverride;
    std::vector<Clip> clips;
};

/// A pattern representing a track with its mixer settings and clips.
struct Pattern
{
    PatternId id;
    std::string name;
    int lengthBars = 1;
    std::vector<PatternTrack> tracks{1};

    int lengthSteps() const noexcept;
    PatternTrack& track() noexcept { return tracks.front(); }
    const PatternTrack& track() const noexcept { return tracks.front(); }
};

} // namespace trami
