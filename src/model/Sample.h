#pragma once

#include "model/Ids.h"
#include "model/Timing.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace trami
{

enum class SampleKind
{
    OneShot,    ///< Plays at its natural speed (RAM-cached).
    Loop,       ///< Time-stretched to the song tempo and conformed to project key.
    DiskStream  ///< ACID Disk-Based track mode for long continuous audio files (streams without time-stretching).
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
    int rootNote = -1;           ///< MIDI root note (0-127), or -1 if not set / unpitched ("Don't Transpose").
    double acidBpm = 0.0;        ///< Original tempo from ACID metadata, or 0.0 if not specified.
    bool missing = false;        ///< True if the audio file could not be found on disk.
    std::vector<double> stretchMarkers; ///< Normalized positions [0.0 .. 1.0] of transient / stretch markers.
    std::string author;          ///< Optional RIFF metadata: Author / Artist.
    std::string copyright;       ///< Optional RIFF metadata: Copyright.
    std::string comments;        ///< Optional RIFF metadata: Comments / Summary.

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

    /// Detects transient onset positions (normalized in [0.0 .. 1.0]) from raw stereo/mono audio frames.
    static std::vector<double> detectTransients(const float* left, const float* right,
                                                std::int64_t numFrames, double sampleRate,
                                                int maxMarkers = 32);

    /// Returns a pitch-class note name ("C", "C#", ..., "B") for a MIDI note, or "None" if < 0.
    static const char* pitchClassName(int midiNote) noexcept;

    /// Computes the shortest semitone shift in [-6, +6] to conform `sampleRootNote` to `projectRootNote`.
    static int keyConformSemitones(int sampleRootNote, int projectRootNote) noexcept;
};

} // namespace trami
