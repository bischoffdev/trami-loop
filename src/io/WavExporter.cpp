#include "io/WavExporter.h"

#include "engine/Sequencer.h"
#include "engine/SnapshotBuilder.h"
#include "engine/SnapshotExchange.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <vector>

namespace trami
{

bool WavExporter::exportMixdown(const Song& song, StretchCache& stretchCache, const ExportOptions& options,
                                const std::filesystem::path& outputFile, ProgressCallback progress)
{
    if (song.patterns().empty() && song.lengthBars() == 0)
        return false;

    auto bank = stretchCache.getOrUpdateSync(song, options.sampleRate);
    std::unique_ptr<PlaybackSnapshot> snapshot;

    bool hasArrangementBlocks = false;
    for (const auto& track : song.arrangement())
    {
        if (!track.blocks.empty())
        {
            hasArrangementBlocks = true;
            break;
        }
    }

    if (hasArrangementBlocks)
        snapshot = SnapshotBuilder::buildArrangement(song, bank, options.sampleRate, 0, 0, nullptr, &stretchCache);
    else if (!song.patterns().empty())
        snapshot = SnapshotBuilder::buildPattern(song, song.patterns().front().id, bank, options.sampleRate, 0, 0, nullptr, nullptr, &stretchCache);

    if (snapshot == nullptr || snapshot->lengthSamples <= 0)
        return false;

    snapshot->looping = false;

    const std::int64_t totalFrames = snapshot->lengthSamples;

    juce::File jFile(juce::String::fromUTF8(outputFile.string().c_str()));
    jFile.deleteFile();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(jFile);
    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions{}
                                                  .withSampleRate(options.sampleRate)
                                                  .withNumChannels(2)
                                                  .withBitsPerSample(options.bitsPerSample));
    if (writer == nullptr)
        return false;

    SnapshotExchange exchange;
    exchange.publish(std::move(snapshot));
    Sequencer sequencer(exchange, options.sampleRate);
    sequencer.setMasterVolume(song.masterVolume());
    sequencer.setPdcEnabled(song.pdcEnabled());
    sequencer.setProjectLatencySamples(song.projectLatencySamples());
    sequencer.requestPlaying(true);

    const std::int64_t pdcPrerollFrames = (!song.patterns().empty())
        ? static_cast<std::int64_t>(std::max(0, song.totalProjectLatencySamples(song.patterns().front().id)))
        : 0;

    constexpr int kBlockSize = 512;
    std::vector<float> left(kBlockSize);
    std::vector<float> right(kBlockSize);

    juce::AudioBuffer<float> tempBuffer(2, kBlockSize);
    float* const channelData[2] = {tempBuffer.getWritePointer(0), tempBuffer.getWritePointer(1)};

    std::int64_t skippedPreroll = 0;
    std::int64_t rendered = 0;
    while (rendered < totalFrames)
    {
        const std::int64_t remainingPreroll = pdcPrerollFrames - skippedPreroll;
        const int numFrames = static_cast<int>(std::min<std::int64_t>(
            kBlockSize, (totalFrames - rendered) + remainingPreroll));

        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);

        sequencer.process(left.data(), right.data(), numFrames);

        const int dropInBlock = static_cast<int>(std::min<std::int64_t>(numFrames, remainingPreroll));
        skippedPreroll += dropInBlock;
        const int writeFrames = numFrames - dropInBlock;

        if (writeFrames > 0)
        {
            std::copy_n(left.data() + dropInBlock, writeFrames, channelData[0]);
            std::copy_n(right.data() + dropInBlock, writeFrames, channelData[1]);

            writer->writeFromAudioSampleBuffer(tempBuffer, 0, writeFrames);
            rendered += writeFrames;

            if (progress)
                progress(static_cast<float>(rendered) / static_cast<float>(totalFrames));
        }
    }

    return true;
}

} // namespace trami
