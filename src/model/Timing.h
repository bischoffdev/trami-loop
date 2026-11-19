#pragma once

#include <cstdint>

namespace trami
{

/// Musical time constants. trami-loop works in 4/4 with a 1/32 base step grid.
inline constexpr int kBeatsPerBar = 4;
inline constexpr int kStepsPerBeat = 8;
inline constexpr int kStepsPerBar = kBeatsPerBar * kStepsPerBeat;

/// Named Groove Pool presets (16th-note swing, 8th-note shuffle, MPC, triplet, human feel).
enum class GroovePreset
{
    Straight = 0,
    Swing16_55 = 1,
    Swing16_62 = 2,
    Swing16_68 = 3,
    Shuffle8_60 = 4,
    Shuffle8_67 = 5,
    Mpc58 = 6,
    Mpc64 = 7,
    TripletFeel = 8,
    Count = 9
};

const char* groovePresetName(GroovePreset preset) noexcept;
double groovePresetSwing(GroovePreset preset) noexcept;
bool groovePresetIsEighthNote(GroovePreset preset) noexcept;

/// Converts between musical positions (bars/steps) and sample positions
/// for a given tempo and sample rate. Immutable and cheap to copy, so it
/// is safe to use on the audio thread.
class Timing
{
public:
    Timing(double bpm, double sampleRate, double swing = 0.0, bool eighthNoteGroove = false);

    double bpm() const noexcept { return bpm_; }
    double sampleRate() const noexcept { return sampleRate_; }

    /// Swing in [0, 1]: 0 = straight, 1 = odd 1/16 (or 1/8) steps delayed by a full
    /// half subdivision (triplet-like feel at ~0.33).
    double swing() const noexcept { return swing_; }
    bool eighthNoteGroove() const noexcept { return eighthNoteGroove_; }

    double samplesPerStep() const noexcept;
    double samplesPerBar() const noexcept;

    /// Returns the swung musical step position for a given straight step position,
    /// matching the exact swing timing used in stepToSample.
    double stepToSwungStep(double step) const noexcept;

    /// Converts a swung step position back to a straight musical step.
    double swungStepToStep(double swungStep) const noexcept;

    /// Sample position of a grid step, including the swing offset.
    std::int64_t stepToSample(double step) const noexcept;

    /// Original tempo of a loop, given its duration and length in bars.
    static double loopBpm(double durationSeconds, int lengthInBars) noexcept;

private:
    double bpm_;
    double sampleRate_;
    double swing_;
    bool eighthNoteGroove_ = false;
};

} // namespace trami
