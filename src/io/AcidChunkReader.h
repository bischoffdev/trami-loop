#pragma once

#include "model/Sample.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <filesystem>
#include <optional>

namespace trami
{

/// Decoded metadata from a Sonic Foundry / Sony / MAGIX 'acid' RIFF chunk.
struct AcidMetadata
{
    bool isAcidized = false;
    bool isOneShot = false;
    bool isDiskBased = false;
    bool hasRootNote = false;
    int rootNote = -1;            ///< MIDI root note (0-127), e.g. 60 = C4; -1 if not set.
    int numBeats = 4;             ///< Number of musical beats in the loop.
    int meterNumerator = 4;       ///< Time signature numerator (e.g. 4 in 4/4).
    int meterDenominator = 4;     ///< Time signature denominator (e.g. 4 in 4/4).
    double tempo = 120.0;         ///< Original tempo in BPM.
    int suggestedLoopBars = 1;    ///< Derived bar count (e.g. 4 beats = 1 bar, 8 = 2 bars, 16 = 4 bars).

    /// Helper to compute loop bars from beat count and meter numerator.
    static int calculateLoopBars(int beats, int numerator = 4) noexcept
    {
        const int num = (numerator > 0) ? numerator : 4;
        if (beats <= 0)
            return 1;
        const int bars = static_cast<int>(std::round(static_cast<double>(beats) / static_cast<double>(num)));
        return std::max(1, bars);
    }
};

class AcidChunkReader
{
public:
    /// Inspects an open JUCE AudioFormatReader's metadata for an 'acid' chunk.
    static std::optional<AcidMetadata> readFromReader(const juce::AudioFormatReader& reader);

    /// Inspects a WAV file on disk for an 'acid' RIFF chunk.
    static std::optional<AcidMetadata> readFromFile(const std::filesystem::path& wavFile);

    /// Parses the raw 24+ byte payload of an 'acid' RIFF chunk.
    static std::optional<AcidMetadata> parseChunkData(const void* data, std::size_t size);
};

} // namespace trami
