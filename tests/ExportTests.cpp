#include "engine/Synth.h"
#include "io/WavExporter.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_audio_formats/juce_audio_formats.h>

using namespace trami;

TEST_CASE("WavExporter exports valid WAV files in all supported formats")
{
    Song song;
    song.setBpm(120.0);

    Sample s;
    s.name = "Kick";
    s.durationSeconds = 0.4;
    const auto kickId = song.addSample(s);

    StretchCache cache;
    cache.setRawAudio(kickId, std::make_shared<const AudioData>(synth::kick(44100.0)));

    const auto pat = song.addPattern("P", 1);
    song.placeClip(pat, 0, kickId, 0);
    song.placeBlock(0, pat, 0); // 1 bar arrangement

    const std::vector<ExportOptions> formats = {
        {44100.0, 16},
        {44100.0, 24},
        {48000.0, 16},
        {48000.0, 24},
    };

    juce::AudioFormatManager manager;
    manager.registerBasicFormats();

    for (const auto& opt : formats)
    {
        const auto tempFile =
            std::filesystem::temp_directory_path() /
            ("trami_export_" + std::to_string(static_cast<int>(opt.sampleRate)) + "_" +
             std::to_string(opt.bitsPerSample) + ".wav");

        float lastProgress = 0.0f;
        REQUIRE(WavExporter::exportMixdown(song, cache, opt, tempFile,
                                           [&lastProgress](float p) { lastProgress = p; }));
        REQUIRE(lastProgress == 1.0f);
        REQUIRE(std::filesystem::exists(tempFile));

        // Read back and verify WAV properties
        const juce::File jFile(juce::String::fromUTF8(tempFile.string().c_str()));
        std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(jFile));
        REQUIRE(reader != nullptr);
        REQUIRE(reader->sampleRate == opt.sampleRate);
        REQUIRE(reader->bitsPerSample == opt.bitsPerSample);
        REQUIRE(reader->numChannels == 2);
        REQUIRE(reader->lengthInSamples > 0);

        std::filesystem::remove(tempFile);
    }
}
