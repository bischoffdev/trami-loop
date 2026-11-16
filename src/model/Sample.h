#pragma once

#include "model/Ids.h"

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
    int rootNote = -1;           ///< MIDI root note (0-127), or -1 if not set / unpitched.
    double acidBpm = 0.0;        ///< Original tempo from ACID metadata, or 0.0 if not specified.

    /// Original tempo of the loop, derived from its duration and bar length.
    double originalBpm() const noexcept;

    /// Automatic, stable display hue in [0, 1).
    float hue() const noexcept;
};

} // namespace trami
