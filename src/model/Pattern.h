#pragma once

#include "model/Ids.h"

#include <array>
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
};

/// Mixer settings shared by pattern and arranger tracks.
struct TrackMix
{
    float volume = 1.0f; ///< Linear gain, 0..2.
    float pan = 0.0f;    ///< -1 (left) .. +1 (right).
};

struct PatternTrack
{
    TrackMix mix;
    std::vector<Clip> clips;
};

/// A fixed-length block of up to 16 tracks of clips.
struct Pattern
{
    PatternId id;
    std::string name;
    int lengthBars = 1;
    std::array<PatternTrack, kTracksPerPattern> tracks;

    int lengthSteps() const noexcept;
};

} // namespace trami
