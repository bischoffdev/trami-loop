#pragma once

#include <cstdint>

namespace trami
{

/// Musical time constants. trami-loop works in 4/4 with a 1/16 step grid.
inline constexpr int kBeatsPerBar = 4;
inline constexpr int kStepsPerBeat = 4;
inline constexpr int kStepsPerBar = kBeatsPerBar * kStepsPerBeat;

/// Converts between musical positions (bars/steps) and sample positions
/// for a given tempo and sample rate. Immutable and cheap to copy, so it
/// is safe to use on the audio thread.
class Timing
{
public:
    Timing(double bpm, double sampleRate, double swing = 0.0);

    double bpm() const noexcept { return bpm_; }
    double sampleRate() const noexcept { return sampleRate_; }

    /// Swing in [0, 1]: 0 = straight, 1 = odd 1/16 steps delayed by a full
    /// half step (triplet-like feel at ~0.33).
    double swing() const noexcept { return swing_; }

    double samplesPerStep() const noexcept;
    double samplesPerBar() const noexcept;

    /// Sample position of a grid step, including the swing offset.
    std::int64_t stepToSample(std::int64_t step) const noexcept;

    /// Original tempo of a loop, given its duration and length in bars.
    static double loopBpm(double durationSeconds, int lengthInBars) noexcept;

private:
    double bpm_;
    double sampleRate_;
    double swing_;
};

} // namespace trami
