#pragma once

#include "engine/AudioData.h"
#include "engine/PlaybackSnapshot.h"
#include "model/Song.h"

#include <memory>
#include <unordered_map>

namespace trami
{

class StretchCache;

/// Decoded audio for each sample, at the engine sample rate.
using SampleBank = std::unordered_map<SampleId, std::shared_ptr<const AudioData>>;

/// Turns the editable Song into immutable snapshots for the audio thread.
class SnapshotBuilder
{
public:
    /// Pattern mode: loops a single pattern or within [loopStartStep, loopEndStep].
    static std::unique_ptr<PlaybackSnapshot> buildPattern(const Song& song, PatternId pattern,
                                                          const SampleBank& bank, double sampleRate,
                                                          int loopStartStep = 0, int loopEndStep = 0,
                                                          const SampleBank* rawBank = nullptr,
                                                          const std::vector<DragClipOverride>* overrides = nullptr,
                                                          StretchCache* stretchCache = nullptr);

    /// Song / Arrangement mode: plays all pattern blocks across the 8 arranger tracks.
    static std::unique_ptr<PlaybackSnapshot> buildArrangement(const Song& song, const SampleBank& bank,
                                                              double sampleRate,
                                                              int loopStartBar = 0, int loopEndBar = 0,
                                                              const SampleBank* rawBank = nullptr,
                                                              StretchCache* stretchCache = nullptr);

    /// Constant-power pan with unity gain at centre. Returns {left, right}.
    static std::pair<float, float> panGains(float pan) noexcept;
};

} // namespace trami
