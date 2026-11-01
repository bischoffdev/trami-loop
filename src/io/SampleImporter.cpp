#include "io/SampleImporter.h"

#include <cmath>

namespace trami
{

SampleImporter::SampleImporter()
{
    formatManager_.registerBasicFormats();
}

bool SampleImporter::isSupportedFormat(const std::filesystem::path& file) const
{
    const auto ext = file.extension().string();
    return ext == ".wav" || ext == ".WAV" || ext == ".mp3" || ext == ".MP3";
}

std::optional<AudioData> SampleImporter::loadAudioData(const std::filesystem::path& file, double targetSampleRate)
{
    const juce::File jFile(juce::String::fromUTF8(file.string().c_str()));
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(jFile));
    if (reader == nullptr || reader->lengthInSamples == 0)
        return std::nullopt;

    const int numChannels = std::min(2, static_cast<int>(reader->numChannels));
    const auto srcLength = static_cast<int>(reader->lengthInSamples);

    juce::AudioBuffer<float> tempBuffer(numChannels, srcLength);
    reader->read(&tempBuffer, 0, srcLength, 0, true, numChannels > 1);

    AudioData audio;
    audio.channels.resize(static_cast<std::size_t>(numChannels));

    if (std::abs(reader->sampleRate - targetSampleRate) < 1.0)
    {
        // Sample rates match; copy directly
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* src = tempBuffer.getReadPointer(ch);
            audio.channels[static_cast<std::size_t>(ch)].assign(src, src + srcLength);
        }
    }
    else
    {
        // Resample to target sample rate using Lagrange interpolation
        const double ratio = reader->sampleRate / targetSampleRate;
        const int destLength = static_cast<int>(std::round(static_cast<double>(srcLength) / ratio));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& dest = audio.channels[static_cast<std::size_t>(ch)];
            dest.resize(static_cast<std::size_t>(destLength));

            juce::LagrangeInterpolator interpolator;
            interpolator.process(ratio, tempBuffer.getReadPointer(ch), dest.data(), destLength);
        }
    }

    return audio;
}

std::optional<ImportResult> SampleImporter::importFile(const std::filesystem::path& sourceFile,
                                                      double targetSampleRate, SampleKind kind, int loopBars,
                                                      const std::optional<std::filesystem::path>& projectDir)
{
    auto audioOpt = loadAudioData(sourceFile, targetSampleRate);
    if (!audioOpt.has_value())
        return std::nullopt;

    std::string finalFileName = sourceFile.filename().string();

    if (projectDir.has_value())
    {
        std::error_code ec;
        const auto samplesDir = *projectDir / "samples";
        std::filesystem::create_directories(samplesDir, ec);

        const auto destination = samplesDir / sourceFile.filename();
        std::filesystem::copy_file(sourceFile, destination, std::filesystem::copy_options::overwrite_existing, ec);
        finalFileName = destination.filename().string();
    }

    Sample sample;
    sample.name = sourceFile.stem().string();
    sample.fileName = finalFileName;
    sample.kind = kind;
    sample.loopBars = std::max(1, loopBars);
    sample.durationSeconds = static_cast<double>(audioOpt->numFrames()) / targetSampleRate;

    return ImportResult{std::move(sample), std::move(*audioOpt)};
}

} // namespace trami
