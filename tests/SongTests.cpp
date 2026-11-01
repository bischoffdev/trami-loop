#include "model/Song.h"
#include "model/UndoManager.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace trami;

namespace
{

Sample makeLoop(double seconds, int bars)
{
    Sample s;
    s.name = "loop";
    s.fileName = "loop.wav";
    s.durationSeconds = seconds;
    s.kind = SampleKind::Loop;
    s.loopBars = bars;
    return s;
}

} // namespace

TEST_CASE("Global settings are clamped")
{
    Song song;
    song.setBpm(1000.0);
    song.setSwing(-1.0);
    song.setMasterVolume(5.0f);
    REQUIRE(song.bpm() == 300.0);
    REQUIRE(song.swing() == 0.0);
    REQUIRE(song.masterVolume() == 2.0f);
}

TEST_CASE("Samples get unique ids, a loop BPM and distinct colours")
{
    Song song;
    const auto a = song.addSample(makeLoop(2.0, 1));
    const auto b = song.addSample(makeLoop(4.0, 1));
    REQUIRE(a != b);
    REQUIRE(song.findSample(a)->originalBpm() == Catch::Approx(120.0));
    REQUIRE(song.findSample(b)->originalBpm() == Catch::Approx(60.0));
    REQUIRE(song.findSample(a)->hue() != song.findSample(b)->hue());
}

TEST_CASE("One-shots have no original BPM")
{
    Sample s;
    s.durationSeconds = 0.3;
    REQUIRE(s.originalBpm() == 0.0);
}

TEST_CASE("Clips are validated against pattern, track, sample and length")
{
    Song song;
    const auto sample = song.addSample(makeLoop(2.0, 1));
    const auto pattern = song.addPattern("Intro", 2);

    REQUIRE(song.placeClip(pattern, 0, sample, 0));
    REQUIRE(song.placeClip(pattern, 15, sample, 31));
    REQUIRE_FALSE(song.placeClip(pattern, 16, sample, 0));
    REQUIRE_FALSE(song.placeClip(pattern, 0, sample, 32));
    REQUIRE_FALSE(song.placeClip(pattern, 0, SampleId{99}, 0));
    REQUIRE_FALSE(song.placeClip(PatternId{99}, 0, sample, 0));
}

TEST_CASE("Tracks can hold multiple clips; clips move between tracks")
{
    Song song;
    const auto sample = song.addSample(makeLoop(2.0, 1));
    const auto pattern = song.addPattern("A", 1);

    const auto c1 = *song.placeClip(pattern, 0, sample, 0);
    song.placeClip(pattern, 0, sample, 8);
    REQUIRE(song.findPattern(pattern)->tracks[0].clips.size() == 2);

    REQUIRE(song.moveClip(pattern, c1, 3, 4));
    REQUIRE(song.findPattern(pattern)->tracks[0].clips.size() == 1);
    REQUIRE(song.findPattern(pattern)->tracks[3].clips.front().startStep == 4);

    REQUIRE(song.removeClip(pattern, c1));
    REQUIRE_FALSE(song.removeClip(pattern, c1));
}

TEST_CASE("Removing a sample removes its clips")
{
    Song song;
    const auto sample = song.addSample(makeLoop(2.0, 1));
    const auto pattern = song.addPattern("A", 1);
    song.placeClip(pattern, 0, sample, 0);

    REQUIRE(song.removeSample(sample));
    REQUIRE(song.findPattern(pattern)->tracks[0].clips.empty());
}

TEST_CASE("Duplicating a pattern copies clips with fresh ids")
{
    Song song;
    const auto sample = song.addSample(makeLoop(2.0, 1));
    const auto original = song.addPattern("Verse", 1);
    const auto clip = *song.placeClip(original, 2, sample, 4);

    const auto copy = *song.duplicatePattern(original);
    const auto& copied = song.findPattern(copy)->tracks[2].clips;
    REQUIRE(song.findPattern(copy)->name == "Verse copy");
    REQUIRE(copied.size() == 1);
    REQUIRE(copied.front().id != clip);
    REQUIRE(copied.front().startStep == 4);
}

TEST_CASE("Shrinking a pattern drops clips beyond the end")
{
    Song song;
    const auto sample = song.addSample(makeLoop(2.0, 1));
    const auto pattern = song.addPattern("A", 2);
    song.placeClip(pattern, 0, sample, 0);
    song.placeClip(pattern, 0, sample, 20);

    REQUIRE(song.setPatternLength(pattern, 1));
    REQUIRE(song.findPattern(pattern)->tracks[0].clips.size() == 1);
}

TEST_CASE("Blocks on one arranger track must not overlap")
{
    Song song;
    const auto twoBars = song.addPattern("A", 2);

    REQUIRE(song.placeBlock(0, twoBars, 0));
    REQUIRE_FALSE(song.placeBlock(0, twoBars, 1));
    REQUIRE(song.placeBlock(0, twoBars, 2));
    REQUIRE(song.placeBlock(1, twoBars, 1));
    REQUIRE(song.lengthBars() == 4);
}

TEST_CASE("Growing a pattern is rejected if its blocks would overlap")
{
    Song song;
    const auto pattern = song.addPattern("A", 1);
    song.placeBlock(0, pattern, 0);
    song.placeBlock(0, pattern, 1);

    REQUIRE_FALSE(song.setPatternLength(pattern, 2));
    REQUIRE(song.findPattern(pattern)->lengthBars == 1);
}

TEST_CASE("Blocks can be moved, and removing a pattern removes its blocks")
{
    Song song;
    const auto a = song.addPattern("A", 1);
    const auto b = song.addPattern("B", 1);
    const auto blockA = *song.placeBlock(0, a, 0);
    song.placeBlock(0, b, 1);

    REQUIRE_FALSE(song.moveBlock(blockA, 0, 1));
    REQUIRE(song.moveBlock(blockA, 2, 5));
    REQUIRE(song.lengthBars() == 6);

    REQUIRE(song.removePattern(a));
    REQUIRE(song.arrangement()[2].blocks.empty());
    REQUIRE(song.lengthBars() == 2);
}

TEST_CASE("SongHistory performs undo and redo on song snapshots")
{
    Song song;
    SongHistory history;

    song.setBpm(120.0);
    history.pushState(song);

    song.setBpm(140.0);
    history.pushState(song);

    song.setBpm(160.0);

    REQUIRE(history.canUndo());
    REQUIRE(history.undo(song));
    REQUIRE(song.bpm() == 140.0);

    REQUIRE(history.undo(song));
    REQUIRE(song.bpm() == 120.0);

    REQUIRE_FALSE(history.undo(song));

    REQUIRE(history.canRedo());
    REQUIRE(history.redo(song));
    REQUIRE(song.bpm() == 140.0);
}
