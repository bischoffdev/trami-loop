#pragma once

#include "model/Ids.h"
#include "model/Pattern.h"

#include <array>
#include <vector>

namespace trami
{

inline constexpr int kArrangerTracks = 8;

/// A pattern placed on an arranger track, starting at a bar.
struct PatternBlock
{
    BlockId id;
    PatternId patternId;
    int startBar = 0;
};

struct ArrangerTrack
{
    std::string name;
    PatternId patternId;
    TrackMix mix;
    bool muted = false;
    bool solo = false;
    std::vector<PatternBlock> blocks;
};

using Arrangement = std::array<ArrangerTrack, kArrangerTracks>;

} // namespace trami
