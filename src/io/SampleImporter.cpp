#include "io/SampleImporter.h"
#include "io/AcidChunkReader.h"
#include "model/Timing.h"

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
                                                      const std::optional<std::filesystem::path>& projectDir,
                                                      double refBpm)
{
    const juce::File jFile(juce::String::fromUTF8(sourceFile.string().c_str()));
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
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* src = tempBuffer.getReadPointer(ch);
            audio.channels[static_cast<std::size_t>(ch)].assign(src, src + srcLength);
        }
    }
    else
    {
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
    sample.durationSeconds = static_cast<double>(audio.numFrames()) / targetSampleRate;

    // 1. Auto-detect ACID chunk metadata directly from the JUCE audio format reader
    bool detectedAcid = false;
    if (auto acid = AcidChunkReader::readFromReader(*reader))
    {
        detectedAcid = true;
        if (acid->isOneShot)
            sample.kind = SampleKind::OneShot;
        else
            sample.kind = SampleKind::Loop;

        sample.loopBars = acid->suggestedLoopBars;
        sample.loopBeats = static_cast<double>(acid->numBeats);
        sample.acidBpm = acid->tempo;
        sample.rootNote = acid->rootNote;
    }

    // 2. Fallback: parse raw RIFF acid chunk directly from file
    if (!detectedAcid)
    {
        if (auto acid = AcidChunkReader::readFromFile(sourceFile))
        {
            detectedAcid = true;
            if (acid->isOneShot)
                sample.kind = SampleKind::OneShot;
            else
                sample.kind = SampleKind::Loop;

            sample.loopBars = acid->suggestedLoopBars;
            sample.loopBeats = static_cast<double>(acid->numBeats);
            sample.acidBpm = acid->tempo;
            sample.rootNote = acid->rootNote;
        }
    }

    // 3. For loops, accurately estimate bars and calculate tempo to match duration
    if (sample.kind == SampleKind::Loop && sample.durationSeconds > 0.0)
    {
        if (sample.loopBeats > 0.0)
        {
            const double calcBpm = (sample.loopBeats * 60.0) / sample.durationSeconds;
            // If stored acidBpm was calculated assuming 4 beats while loopBeats is sub-bar (e.g. 2 beats),
            // or if acidBpm was not specified, use authentic BPM from beats and duration
            if (sample.acidBpm <= 0.0 || (sample.loopBeats < 4.0 && std::abs(sample.acidBpm - calcBpm * (4.0 / sample.loopBeats)) < 1.0))
            {
                sample.acidBpm = calcBpm;
            }
        }
        else
        {
            const double effectiveRefBpm = (sample.acidBpm > 20.0) ? sample.acidBpm : ((refBpm > 20.0) ? refBpm : 120.0);
            const double secPerBar = (60.0 / effectiveRefBpm) * 4.0;
            const int estBars = std::max(1, static_cast<int>(std::round(sample.durationSeconds / secPerBar)));

            if (sample.loopBars <= 1 && estBars > 1)
                sample.loopBars = estBars;

            if (sample.acidBpm <= 0.0)
                sample.acidBpm = Timing::loopBpm(sample.durationSeconds, sample.loopBars);
        }
    }

    return ImportResult{std::move(sample), std::move(audio)};
}

} // namespace trami
