#pragma once

#include "engine/AudioData.h"

namespace trami
{

/// Offline time-stretching using Rubber Band.
class TimeStretcher
{
public:
    /// Stretches `input` audio by `timeRatio` (targetDuration / sourceDuration) and scales pitch by `pitchScale`.
    /// A timeRatio < 1.0 speeds up (shorter output), > 1.0 slows down (longer output).
    /// pitchScale > 1.0 shifts pitch up (e.g. 2^(semitones/12)), < 1.0 shifts pitch down.
    /// If timeRatio is ~1.0, pitchScale is ~1.0 or input is empty, returns a copy of input.
    static AudioData stretch(const AudioData& input, double timeRatio, double sampleRate, double pitchScale = 1.0);

    /// High-fidelity 6-point polynomial resampling for one-shots (sampler-style pitch shifting).
    /// Preserves 100% of punch, transient attack, and phase coherence without phase-vocoder/metallic artifacts.
    /// pitchScale > 1.0 pitches up (shorter duration); pitchScale < 1.0 pitches down (longer duration).
    static AudioData resample(const AudioData& input, double pitchScale);
};

} // namespace trami
