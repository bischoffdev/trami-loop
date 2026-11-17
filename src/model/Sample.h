#pragma once

#include "model/Ids.h"
#include "model/Timing.h"

#include <algorithm>
#include <string>

namespace trami
{

enum class SampleKind
{
    OneShot, ///< Plays at its natural speed.
    Loop     ///< Time-stretched to the song tempo.
};

/// An audio file stored in the project's samples folder.
struct Sample
{
    SampleId id;
    std::string name;
    std::string fileName;        ///< Relative to the project's samples folder.
    double durationSeconds = 0.0;
    SampleKind kind = SampleKind::OneShot;
    int loopBars = 1;            ///< Only meaningful for loops.
    double loopBeats = 0.0;      ///< Musical beats in loop cycle (e.g. 2.0 for 2 beats; 0.0 = use loopBars * 4).
    int rootNote = -1;           ///< MIDI root note (0-127), or -1 if not set / unpitched.
    double acidBpm = 0.0;        ///< Original tempo from ACID metadata, or 0.0 if not specified.

    /// Effective loop cycle length in musical beats (e.g. 2.0 for 2 beats, 4.0 for 1 bar, 8.0 for 2 bars).
    double loopCycleBeats() const noexcept
    {
        if (loopBeats > 0.0)
            return loopBeats;
        return static_cast<double>(std::max(1, loopBars) * kBeatsPerBar);
    }

    /// Effective loop cycle length in grid steps (8 steps per beat).
    double loopCycleSteps() const noexcept
    {
        return loopCycleBeats() * static_cast<double>(kStepsPerBeat);
    }

    /// Original tempo of the loop, derived from its duration and beat length.
    double originalBpm() const noexcept;

    /// Automatic, stable display hue in [0, 1).
    float hue() const noexcept;
};

} // namespace trami
