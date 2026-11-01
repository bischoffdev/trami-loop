#pragma once

#include "engine/AudioData.h"

namespace trami
{

/// Offline time-stretching using Rubber Band.
class TimeStretcher
{
public:
    /// Stretches `input` audio by `timeRatio` (targetDuration / sourceDuration).
    /// A timeRatio < 1.0 speeds up (shorter output), > 1.0 slows down (longer output).
    /// If timeRatio is ~1.0 or input is empty, returns a copy of input.
    static AudioData stretch(const AudioData& input, double timeRatio, double sampleRate);
};

} // namespace trami
