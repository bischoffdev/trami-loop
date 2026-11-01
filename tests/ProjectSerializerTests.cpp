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
    originalSong.placeClip(pat1, 0, kickId, 0);
    originalSong.placeClip(pat1, 0, kickId, 8);
    originalSong.placeClip(pat1, 1, bassId, 4);

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

    REQUIRE(loadedSong.samples().size() == 2);
    REQUIRE(loadedSong.patterns().size() == 1);
    REQUIRE(loadedSong.patterns().front().name == "Verse");
    REQUIRE(loadedSong.patterns().front().lengthBars == 2);
    REQUIRE(loadedSong.patterns().front().tracks[0].clips.size() == 2);
    REQUIRE(loadedSong.patterns().front().tracks[1].clips.size() == 1);

    REQUIRE(loadedSong.arrangement()[0].blocks.size() == 2);
    REQUIRE(loadedSong.arrangement()[1].muted == true);

    // Verify audio was decoded into loaded cache
    const auto loadedKickId = loadedSong.samples()[0].id;
    const auto loadedAudio = loadedCache.getRawAudio(loadedKickId);
    REQUIRE(loadedAudio != nullptr);
    REQUIRE(loadedAudio->numFrames() > 0);

    std::filesystem::remove_all(tempDir);
}
