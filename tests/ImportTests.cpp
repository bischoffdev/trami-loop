#include "io/SampleImporter.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_audio_formats/juce_audio_formats.h>

#include <filesystem>

using namespace trami;

TEST_CASE("SampleImporter recognizes audio extensions")
{
    SampleImporter importer;
    REQUIRE(importer.isSupportedFormat("kick.wav"));
    REQUIRE(importer.isSupportedFormat("beat.WAV"));
    REQUIRE(importer.isSupportedFormat("song.mp3"));
    REQUIRE(importer.isSupportedFormat("track.MP3"));
    REQUIRE_FALSE(importer.isSupportedFormat("notes.txt"));
    REQUIRE_FALSE(importer.isSupportedFormat("image.png"));
}

TEST_CASE("SampleImporter imports synthetic WAV file with resampling")
{
    // Create a temporary WAV file at 44.1 kHz
    juce::File tempWav = juce::File::createTempFile(".wav");
    juce::WavAudioFormat wavFormat;

    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(tempWav);
    auto writer = wavFormat.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(44100.0).withNumChannels(1).withBitsPerSample(16));
    REQUIRE(writer != nullptr);

    juce::AudioBuffer<float> buffer(1, 44100);
    buffer.clear();
    // Fill half a second of noise
    for (int i = 0; i < 22050; ++i)
        buffer.setSample(0, i, 0.5f);
    writer->writeFromAudioSampleBuffer(buffer, 0, 44100);
    writer.reset();

    // Now import at 48.0 kHz
    SampleImporter importer;
    const std::filesystem::path p(tempWav.getFullPathName().toStdString());

    auto result = importer.importFile(p, 48000.0, SampleKind::Loop, 1);
    REQUIRE(result.has_value());
    REQUIRE(result->sample.kind == SampleKind::Loop);
    REQUIRE(result->sample.loopBars == 1);
    REQUIRE(result->audio.numChannels() == 1);
    // 1 second of 44.1kHz resampled to 48kHz is ~48000 frames
    REQUIRE(result->audio.numFrames() > 47000);
    REQUIRE(result->audio.numFrames() < 49000);

    tempWav.deleteFile();
}
