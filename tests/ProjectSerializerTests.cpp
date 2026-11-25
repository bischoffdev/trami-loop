#include "engine/Synth.h"
#include "io/ProjectSerializer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace trami;

TEST_CASE("ProjectSerializer saves and loads a self-contained project")
{
    constexpr double rate = 44100.0;
    const auto tempDir = std::filesystem::temp_directory_path() / "trami_test_project";
    std::filesystem::remove_all(tempDir);

    Song originalSong;
    originalSong.setBpm(135.0);
    originalSong.setSwing(0.4);
    originalSong.setMasterVolume(1.5f);
    originalSong.setPatternZoom(1.75f, 1.25f);
    originalSong.setArrangerZoom(2.0f, 1.5f);

    Sample s1;
    s1.name = "Kick";
    s1.fileName = "Kick.wav";
    s1.kind = SampleKind::OneShot;
    s1.durationSeconds = 0.4;
    const auto kickId = originalSong.addSample(s1);

    Sample s2;
    s2.name = "Bass";
    s2.fileName = "Bass.wav";
    s2.kind = SampleKind::Loop;
    s2.loopBars = 2;
    s2.durationSeconds = 3.5;
    const auto bassId = originalSong.addSample(s2);

    StretchCache originalCache;
    originalCache.setRawAudio(kickId, std::make_shared<const AudioData>(synth::kick(rate)));
    originalCache.setRawAudio(bassId, std::make_shared<const AudioData>(synth::bassLoop(rate)));

    const auto pat1 = originalSong.addPattern("Verse", 2);
    originalSong.placeClip(pat1, 0, kickId, 0, 4, 1);
    originalSong.placeClip(pat1, 0, kickId, 8);
    const auto clipBassId = *originalSong.placeClip(pat1, 1, bassId, 4);
    originalSong.setClipVolumeEnvelope(pat1, clipBassId, {
        {0.0, 0.25f, EnvelopeCurve::Fast},
        {1.0, 1.5f, EnvelopeCurve::Linear}
    });
    originalSong.setClipReversed(pat1, clipBassId, true);
    originalSong.setClipTranspose(pat1, clipBassId, -5);
    originalSong.setPdcEnabled(true);
    originalSong.setProjectLatencySamples(128);
    originalSong.setPatternTrackLatency(pat1, 1, 256);
    PluginSlot fxSlot;
    fxSlot.name = "TestCompressor";
    fxSlot.format = "VST3";
    fxSlot.identifierString = "test-comp-id";
    fxSlot.latencySamples = 512;
    originalSong.setTrackFxChain(pat1, 1, {fxSlot});

    originalSong.placeBlock(0, pat1, 0);
    originalSong.placeBlock(0, pat1, 2);
    originalSong.arrangerTrack(1)->muted = true;

    // 1. Save
    REQUIRE(ProjectSerializer::saveProject(originalSong, originalCache, tempDir));
    REQUIRE(std::filesystem::exists(tempDir / "project.trami"));
    REQUIRE(std::filesystem::exists(tempDir / "samples" / "Kick.wav"));
    REQUIRE(std::filesystem::exists(tempDir / "samples" / "Bass.wav"));

    // 2. Load
    StretchCache loadedCache;
    SampleImporter importer;
    auto loadedSongOpt = ProjectSerializer::loadProject(tempDir, loadedCache, importer, rate);
    REQUIRE(loadedSongOpt.has_value());

    const auto& loadedSong = *loadedSongOpt;
    REQUIRE(loadedSong.bpm() == Catch::Approx(135.0));
    REQUIRE(loadedSong.swing() == Catch::Approx(0.4));
    REQUIRE(loadedSong.masterVolume() == Catch::Approx(1.5f));
    REQUIRE(loadedSong.patternZoomX() == Catch::Approx(1.75f));
    REQUIRE(loadedSong.patternZoomY() == Catch::Approx(1.25f));
    REQUIRE(loadedSong.arrangerZoomX() == Catch::Approx(2.0f));
    REQUIRE(loadedSong.arrangerZoomY() == Catch::Approx(1.5f));

    REQUIRE(loadedSong.samples().size() == 2);
    REQUIRE(loadedSong.patterns().size() == 1);
    REQUIRE(loadedSong.patterns().front().name == "Verse");
    REQUIRE(loadedSong.patterns().front().lengthBars == 2);
    REQUIRE(loadedSong.patterns().front().tracks[0].clips.size() == 2);
    REQUIRE(loadedSong.patterns().front().tracks[0].clips[0].lengthSteps == 4);
    REQUIRE(loadedSong.patterns().front().tracks[0].clips[0].offsetSteps == 1);
    REQUIRE(loadedSong.patterns().front().tracks[1].clips.size() == 1);
    REQUIRE(loadedSong.patterns().front().tracks[1].clips[0].reversed == true);
    REQUIRE(loadedSong.patterns().front().tracks[1].clips[0].transposeSemitones == -5);
    // Verify the reversed volume envelope and PDC latencies survived save & load without being re-flipped
    REQUIRE(loadedSong.patterns().front().tracks[1].clips[0].volumeEnvelope.size() == 2);
    REQUIRE(loadedSong.patterns().front().tracks[1].clips[0].volumeEnvelope[0].value == Catch::Approx(1.5f));
    REQUIRE(loadedSong.patterns().front().tracks[1].clips[0].volumeEnvelope[1].value == Catch::Approx(0.25f));
    REQUIRE(loadedSong.pdcEnabled() == true);
    REQUIRE(loadedSong.projectLatencySamples() == 128);
    REQUIRE(loadedSong.patternTrackLatency(loadedSong.patterns().front().id, 1) == 256);
    REQUIRE(loadedSong.patterns().front().tracks[1].fxChain.size() == 1);
    REQUIRE(loadedSong.patterns().front().tracks[1].fxChain[0].latencySamples == 512);
    REQUIRE(loadedSong.channelLatencySamples(loadedSong.patterns().front().id, 1) == 256 + 512);

    REQUIRE(loadedSong.arrangement()[0].blocks.size() == 2);
    REQUIRE(loadedSong.arrangement()[1].muted == true);

    // Also verify loading by passing project.trami file directly
    StretchCache fileCache;
    auto loadedByFile = ProjectSerializer::loadProject(tempDir / "project.trami", fileCache, importer, rate);
    REQUIRE(loadedByFile.has_value());
    REQUIRE(loadedByFile->patterns().front().tracks[1].clips[0].transposeSemitones == -5);

    // Verify audio was decoded into loaded cache
    const auto loadedKickId = loadedSong.samples()[0].id;
    const auto loadedAudio = loadedCache.getRawAudio(loadedKickId);
    REQUIRE(loadedAudio != nullptr);
    REQUIRE(loadedAudio->numFrames() > 0);

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("ProjectSerializer preserves exact 48 kHz audio duration and pitch and auto-repairs legacy 44.1 kHz headers")
{
    constexpr double kEngineRate = 48000.0;
    const auto tempDir = std::filesystem::temp_directory_path() / "trami_test_48k_project";
    std::filesystem::remove_all(tempDir);

    Song song;
    song.setBpm(120.0);

    // 2.0-second loop at 48 kHz = exactly 96,000 frames
    AudioData loopAudio = synth::bassLoop(kEngineRate);
    REQUIRE(loopAudio.numFrames() == 96000);

    Sample s;
    s.name = "Loop48k";
    s.fileName = "Loop48k.wav";
    s.kind = SampleKind::Loop;
    s.loopBars = 1;
    s.loopBeats = 4.0;
    s.acidBpm = 120.0;
    s.durationSeconds = static_cast<double>(loopAudio.numFrames()) / kEngineRate; // 2.0s
    const auto sid = song.addSample(s);

    StretchCache cache;
    cache.setRawAudio(sid, std::make_shared<const AudioData>(loopAudio));

    const auto patId = song.addPattern("Main", 4);
    song.placeClip(patId, 0, sid, 0.0, 32.0, 0.0);

    // 1. Save at 48 kHz and reload at 48 kHz: frame count must remain 96,000 (not resampled to 104,490!)
    REQUIRE(ProjectSerializer::saveProject(song, cache, tempDir));

    StretchCache loadedCache;
    SampleImporter importer;
    auto loadedOpt = ProjectSerializer::loadProject(tempDir, loadedCache, importer, kEngineRate);
    REQUIRE(loadedOpt.has_value());
    auto loadedRaw = loadedCache.getRawAudio(loadedOpt->samples().front().id);
    REQUIRE(loadedRaw != nullptr);
    REQUIRE(loadedRaw->numFrames() == 96000);

    // 2. Simulate a legacy .trami save where 96,000 48kHz frames were written with a 44,100 Hz WAV header
    // and no "sampleRate" field in project.trami
    {
        const auto wavPath = tempDir / "samples" / "Loop48k.wav";
        juce::File jWav(juce::String::fromUTF8(wavPath.string().c_str()));
        jWav.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(jWav);
        auto writer = wav.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}
                        .withSampleRate(44100.0)
                        .withNumChannels(loopAudio.numChannels())
                        .withBitsPerSample(16));
        REQUIRE(writer != nullptr);
        juce::AudioBuffer<float> buf(loopAudio.numChannels(), loopAudio.numFrames());
        for (int ch = 0; ch < loopAudio.numChannels(); ++ch)
            buf.copyFrom(ch, 0, loopAudio.channels[static_cast<std::size_t>(ch)].data(), loopAudio.numFrames());
        writer->writeFromAudioSampleBuffer(buf, 0, loopAudio.numFrames());
        writer.reset();

        const auto projFile = tempDir / "project.trami";
        juce::File jProj(juce::String::fromUTF8(projFile.string().c_str()));
        juce::String jsonText = jProj.loadFileAsString();
        jsonText = jsonText.replace("\"sampleRate\": 48000.0,", "");
        jsonText = jsonText.replace("\"sampleRate\": 48000,", "");
        jProj.replaceWithText(jsonText);
    }

    StretchCache repairedCache;
    auto repairedOpt = ProjectSerializer::loadProject(tempDir, repairedCache, importer, kEngineRate);
    REQUIRE(repairedOpt.has_value());
    auto repairedRaw = repairedCache.getRawAudio(repairedOpt->samples().front().id);
    REQUIRE(repairedRaw != nullptr);
    REQUIRE(repairedRaw->numFrames() == 96000);

    std::filesystem::remove_all(tempDir);
}
