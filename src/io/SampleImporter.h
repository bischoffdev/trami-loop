#pragma once

#include "engine/AudioData.h"
#include "model/Sample.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <filesystem>
#include <optional>
#include <string>

namespace trami
{

struct ImportResult
{
    Sample sample;
    AudioData audio;
};

class SampleImporter
{
public:
    SampleImporter();

    /// Reads a WAV or MP3 file, resamples to targetSampleRate if necessary,
    /// and optionally copies it into targetFolder/samples/.
    std::optional<ImportResult> importFile(const std::filesystem::path& sourceFile, double targetSampleRate,
                                          SampleKind kind, int loopBars = 1,
                                          const std::optional<std::filesystem::path>& projectDir = std::nullopt);

    /// Decodes an audio file into an AudioData buffer at targetSampleRate without copying.
    std::optional<AudioData> loadAudioData(const std::filesystem::path& file, double targetSampleRate);

    bool isSupportedFormat(const std::filesystem::path& file) const;

private:
    juce::AudioFormatManager formatManager_;
};

} // namespace trami
