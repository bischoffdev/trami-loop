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
    REQUIRE(song.placeClip(pattern, kTracksPerPattern - 1, sample, 63));
    REQUIRE_FALSE(song.placeClip(pattern, kTracksPerPattern, sample, 0));
    REQUIRE_FALSE(song.placeClip(pattern, 0, sample, 64));
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

TEST_CASE("Clips can be resized with custom length and offset")
{
    Song song;
    const auto sample = song.addSample(makeLoop(2.0, 1));
    const auto pattern = song.addPattern("A", 1);

    const auto clipId = *song.placeClip(pattern, 0, sample, 0);
    REQUIRE(song.resizeClip(pattern, clipId, 2, 6, 2));

    const auto* p = song.findPattern(pattern);
    REQUIRE(p != nullptr);
    const auto& clip = p->tracks[0].clips.front();
    REQUIRE(clip.startStep == 2);
    REQUIRE(clip.lengthSteps == 6);
    REQUIRE(clip.offsetSteps == 2);

    // Invalid parameters rejected
    REQUIRE_FALSE(song.resizeClip(pattern, clipId, -1, 4, 0));
    REQUIRE_FALSE(song.resizeClip(pattern, clipId, 0, 0, 0));
    REQUIRE_FALSE(song.resizeClip(pattern, clipId, 0, 4, -1));
    REQUIRE_FALSE(song.resizeClip(PatternId{99}, clipId, 0, 4, 0));
}

TEST_CASE("Clips support fractional steps for unconstrained resizing and positioning")
{
    Song song;
    const auto sample = song.addSample(makeLoop(2.0, 1));
    const auto pattern = song.addPattern("A", 1);

    const auto clipId = *song.placeClip(pattern, 0, sample, 0.5, 3.25, 1.1);
    const auto* p = song.findPattern(pattern);
    REQUIRE(p != nullptr);
    const auto& clip = p->tracks[0].clips.front();
    REQUIRE(clip.startStep == Catch::Approx(0.5));
    REQUIRE(clip.lengthSteps == Catch::Approx(3.25));
    REQUIRE(clip.offsetSteps == Catch::Approx(1.1));

    // Resize with fractional start, length, and offset
    REQUIRE(song.resizeClip(pattern, clipId, 1.75, 0.5, 0.25));
    const auto& resizedClip = p->tracks[0].clips.front();
    REQUIRE(resizedClip.startStep == Catch::Approx(1.75));
    REQUIRE(resizedClip.lengthSteps == Catch::Approx(0.5));
    REQUIRE(resizedClip.offsetSteps == Catch::Approx(0.25));

    // Move clip with fractional position
    REQUIRE(song.moveClip(pattern, clipId, 1, 4.33));
    const auto& movedClip = p->tracks[1].clips.front();
    REQUIRE(movedClip.startStep == Catch::Approx(4.33));
    REQUIRE(movedClip.lengthSteps == Catch::Approx(0.5));
}

TEST_CASE("Replacing track sample updates track and existing clips")
{
    Song song;
    const auto s1 = song.addSample(makeLoop(2.0, 1));
    const auto s2 = song.addSample(makeLoop(4.0, 2));
    const auto pattern = song.addPattern("A", 1);

    song.placeClip(pattern, 2, s1, 0);
    song.placeClip(pattern, 2, s1, 8);

    REQUIRE(song.replaceTrackSample(pattern, 2, s2));

    const auto* p = song.findPattern(pattern);
    REQUIRE(p != nullptr);
    const auto& trk = p->tracks[2];
    REQUIRE(trk.sampleId == s2);
    REQUIRE(trk.clips.size() == 2);
    REQUIRE(trk.clips[0].sampleId == s2);
    REQUIRE(trk.clips[1].sampleId == s2);
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
    song.placeClip(pattern, 0, sample, 40);

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

    // Track 0 is dedicated to pattern A; placing pattern B on track 0 must fail
    REQUIRE_FALSE(song.placeBlock(0, b, 1));

    // Place another block of pattern A on track 0 at bar 1
    REQUIRE(song.placeBlock(0, a, 1));

    // Placing pattern B on track 1 succeeds
    REQUIRE(song.placeBlock(1, b, 1));

    // Moving blockA to bar 1 fails because track 0 already has a block at bar 1
    REQUIRE_FALSE(song.moveBlock(blockA, 0, 1));
    // Moving blockA to track 1 is rejected because track 1 is dedicated to pattern B
    REQUIRE_FALSE(song.moveBlock(blockA, 1, 3));
    // Moving to unassigned track 2 succeeds
    REQUIRE(song.moveBlock(blockA, 2, 5));
    REQUIRE(song.lengthBars() == 6);

    REQUIRE(song.removePattern(a));
    REQUIRE(song.arrangement()[2].blocks.empty());
    REQUIRE(song.lengthBars() == 2);
}

TEST_CASE("Each arranger track is dedicated to one pattern")
{
    Song song;
    const auto p1 = song.addPattern("Verse", 2);
    const auto p2 = song.addPattern("Chorus", 4);

    song.setArrangerTrackPattern(0, p1);
    song.setArrangerTrackPattern(1, p2);

    REQUIRE(song.arrangerTrackPattern(0) == p1);
    REQUIRE(song.arrangerTrackPattern(1) == p2);
    REQUIRE(song.findTrackForPattern(p1) == 0);
    REQUIRE(song.findTrackForPattern(p2) == 1);

    // Track 0 only allows p1
    REQUIRE(song.placeBlock(0, p1, 0).has_value());
    REQUIRE_FALSE(song.placeBlock(0, p2, 2).has_value());

    // Track 1 only allows p2
    REQUIRE(song.placeBlock(1, p2, 0).has_value());
    REQUIRE_FALSE(song.placeBlock(1, p1, 4).has_value());
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

TEST_CASE("Patterns can have multiple tracks added, duplicated, and removed")
{
    Song song;
    const auto sample1 = song.addSample(makeLoop(2.0, 1));
    const auto sample2 = song.addSample(makeLoop(4.0, 2));
    const auto patId = song.addPattern("Pattern A", 2);

    auto* pat = song.findPattern(patId);
    REQUIRE(pat != nullptr);
    REQUIRE(pat->tracks.size() == 1);

    const int t2 = song.addTrackToPattern(patId, sample2);
    REQUIRE(t2 == 1);
    REQUIRE(pat->tracks.size() == 2);
    REQUIRE(pat->tracks[1].sampleId == sample2);

    song.placeClip(patId, 1, sample2, 0);
    const int t3 = song.duplicateTrackInPattern(patId, 1);
    REQUIRE(t3 == 2);
    REQUIRE(pat->tracks.size() == 3);
    REQUIRE(pat->tracks[2].sampleId == sample2);
    REQUIRE(pat->tracks[2].clips.size() == 1);
    REQUIRE(pat->tracks[2].clips.front().id != pat->tracks[1].clips.front().id);

    REQUIRE(song.removeTrackFromPattern(patId, 1));
    REQUIRE(pat->tracks.size() == 2);
}

TEST_CASE("Undo reverts track sample substitution and clip operations")
{
    Song song;
    SongHistory history;
    const auto s1 = song.addSample(makeLoop(2.0, 1));
    const auto s2 = song.addSample(makeLoop(4.0, 2));
    const auto patId = song.addPattern("Pattern A", 1);
    song.setPatternTrackSample(patId, 0, s1);
    song.placeClip(patId, 0, s1, 0);

    // Snapshot before substituting sample on track
    history.pushState(song);
    song.replaceTrackSample(patId, 0, s2);
    REQUIRE(song.findPattern(patId)->tracks[0].sampleId == s2);
    REQUIRE(song.findPattern(patId)->tracks[0].clips.front().sampleId == s2);

    // Undo substitution
    REQUIRE(history.undo(song));
    REQUIRE(song.findPattern(patId)->tracks[0].sampleId == s1);
    REQUIRE(song.findPattern(patId)->tracks[0].clips.front().sampleId == s1);

    // Redo substitution
    REQUIRE(history.redo(song));
    REQUIRE(song.findPattern(patId)->tracks[0].sampleId == s2);
    REQUIRE(song.findPattern(patId)->tracks[0].clips.front().sampleId == s2);
}

TEST_CASE("Tracks can be renamed for patterns and arrangement, and preserved through undo")
{
    Song song;
    SongHistory history;
    const auto patId = song.addPattern("Beat", 1);
    const int t1 = song.addTrackToPattern(patId);
    REQUIRE(t1 == 1);

    song.renamePatternTrack(patId, 0, "Lead Synth");
    song.renamePatternTrack(patId, 1, "Sub Bass");
    REQUIRE(song.patternTrackName(patId, 0) == "Lead Synth");
    REQUIRE(song.patternTrackName(patId, 1) == "Sub Bass");

    song.renameArrangerTrack(0, "Drums Group");
    song.renameArrangerTrack(1, "Vocals");
    REQUIRE(song.arrangerTrackName(0) == "Drums Group");
    REQUIRE(song.arrangerTrackName(1) == "Vocals");

    history.pushState(song);
    song.renamePatternTrack(patId, 0, "Altered Name");
    REQUIRE(song.patternTrackName(patId, 0) == "Altered Name");

    REQUIRE(history.undo(song));
    REQUIRE(song.patternTrackName(patId, 0) == "Lead Synth");
}

TEST_CASE("Track kind toggle is independent per track")
{
    Song song;
    const auto sample = song.addSample(makeLoop(2.0, 1));
    const auto patId = song.addPattern("Beat", 1);
    const int t1 = song.addTrackToPattern(patId, sample);
    song.setPatternTrackSample(patId, 0, sample);

    // Default kind derives from sample
    REQUIRE(song.patternTrackKind(patId, 0) == SampleKind::Loop);
    REQUIRE(song.patternTrackKind(patId, 1) == SampleKind::Loop);

    // Overriding track 0 to OneShot must not affect track 1 or sample
    song.placeClip(patId, 0, sample, 0, 16);
    song.setPatternTrackKind(patId, 0, SampleKind::OneShot);
    REQUIRE(song.patternTrackKind(patId, 0) == SampleKind::OneShot);
    REQUIRE(song.patternTrackKind(patId, 1) == SampleKind::Loop);
    REQUIRE(song.findSample(sample)->kind == SampleKind::Loop);

    const auto* p = song.findPattern(patId);
    REQUIRE(p != nullptr);
    REQUIRE(p->tracks[0].clips.front().lengthSteps == 0);
}

TEST_CASE("Tracks can be rearranged in patterns and arrangement")
{
    Song song;
    const auto s = song.addSample(makeLoop(2.0, 1));
    const auto patId = song.addPattern("P", 1);
    song.addTrackToPattern(patId, s);
    song.addTrackToPattern(patId, s);

    song.renamePatternTrack(patId, 0, "Track A");
    song.renamePatternTrack(patId, 1, "Track B");
    song.renamePatternTrack(patId, 2, "Track C");

    REQUIRE(song.movePatternTrack(patId, 0, 2));
    REQUIRE(song.patternTrackName(patId, 0) == "Track B");
    REQUIRE(song.patternTrackName(patId, 1) == "Track C");
    REQUIRE(song.patternTrackName(patId, 2) == "Track A");

    // Arranger track reordering
    song.renameArrangerTrack(0, "Arr A");
    song.renameArrangerTrack(1, "Arr B");
    song.placeBlock(0, patId, 0);

    REQUIRE(song.moveArrangerTrack(0, 1));
    REQUIRE(song.arrangerTrackName(0) == "Arr B");
    REQUIRE(song.arrangerTrackName(1) == "Arr A");
    REQUIRE(song.arrangement()[1].blocks.size() == 1);
}

TEST_CASE("Arranger tracks can be deleted")
{
    Song song;
    const auto patId = song.addPattern("P", 1);
    song.renameArrangerTrack(0, "Track 0");
    song.renameArrangerTrack(1, "Track 1");
    song.placeBlock(0, patId, 0);
    song.placeBlock(1, patId, 2);

    REQUIRE(song.deleteArrangerTrack(0));
    REQUIRE(song.arrangerTrackName(0) == "Track 1");
    REQUIRE(song.arrangement()[0].blocks.size() == 1);
    REQUIRE(song.arrangement()[0].blocks[0].startBar == 2);
    REQUIRE(song.arrangement()[kArrangerTracks - 1].name.empty());
    REQUIRE(song.arrangement()[kArrangerTracks - 1].blocks.empty());
}

TEST_CASE("Clip volume envelope gain and fades can be modified")
{
    Song song;
    const auto s = song.addSample(makeLoop(2.0, 1));
    const auto patId = song.addPattern("P", 1);
    const auto clipId = *song.placeClip(patId, 0, s, 0);

    const auto* clip = song.findClip(patId, clipId);
    REQUIRE(clip != nullptr);
    REQUIRE(clip->gain == Catch::Approx(1.0f));
    REQUIRE(clip->fadeInSteps == Catch::Approx(0.0f));
    REQUIRE(clip->fadeOutSteps == Catch::Approx(0.0f));

    REQUIRE(song.setClipEnvelope(patId, clipId, 1.5f, 2.0f, 4.0f));
    REQUIRE(clip->gain == Catch::Approx(1.5f));
    REQUIRE(clip->fadeInSteps == Catch::Approx(2.0f));
    REQUIRE(clip->fadeOutSteps == Catch::Approx(4.0f));
}

TEST_CASE("Multiple clips can be resized and deleted together")
{
    Song song;
    const auto s = song.addSample(makeLoop(2.0, 1));
    const auto patId = song.addPattern("P", 2); // 64 steps
    const auto c1 = *song.placeClip(patId, 0, s, 0);
    const auto c2 = *song.placeClip(patId, 0, s, 16);
    const auto c3 = *song.placeClip(patId, 1, s, 0);

    REQUIRE(song.findPattern(patId)->tracks[0].clips.size() == 2);
    REQUIRE(song.findPattern(patId)->tracks[1].clips.size() == 1);

    // Resize a group together
    REQUIRE(song.resizeClip(patId, c1, 0, 12, 0));
    REQUIRE(song.resizeClip(patId, c2, 16, 12, 0));
    REQUIRE(song.resizeClip(patId, c3, 0, 12, 0));

    REQUIRE(song.findClip(patId, c1)->lengthSteps == 12);
    REQUIRE(song.findClip(patId, c2)->lengthSteps == 12);
    REQUIRE(song.findClip(patId, c3)->lengthSteps == 12);

    // Delete a group together
    std::vector<ClipId> toDelete = {c1, c3};
    for (auto cid : toDelete)
        REQUIRE(song.removeClip(patId, cid));

    REQUIRE(song.findClip(patId, c1) == nullptr);
    REQUIRE(song.findClip(patId, c2) != nullptr);
    REQUIRE(song.findClip(patId, c3) == nullptr);
    REQUIRE(song.findPattern(patId)->tracks[0].clips.size() == 1);
    REQUIRE(song.findPattern(patId)->tracks[1].clips.empty());
}

TEST_CASE("Clip reverse flag can be set and toggled")
{
    Song song;
    const auto s = song.addSample(makeLoop(2.0, 1));
    const auto patId = song.addPattern("P", 1);
    const auto clipId = *song.placeClip(patId, 0, s, 0);

    REQUIRE_FALSE(song.isClipReversed(patId, clipId));
    REQUIRE(song.setClipReversed(patId, clipId, true));
    REQUIRE(song.isClipReversed(patId, clipId));
    REQUIRE(song.findClip(patId, clipId)->reversed == true);

    REQUIRE(song.setClipReversed(patId, clipId, false));
    REQUIRE_FALSE(song.isClipReversed(patId, clipId));
}

TEST_CASE("Clip transposition semitones can be set, clamped, and queried")
{
    Song song;
    const auto s = song.addSample(makeLoop(2.0, 1));
    const auto patId = song.addPattern("P", 1);
    const auto clipId = *song.placeClip(patId, 0, s, 0);

    REQUIRE(song.clipTranspose(patId, clipId) == 0);

    REQUIRE(song.setClipTranspose(patId, clipId, 7));
    REQUIRE(song.clipTranspose(patId, clipId) == 7);
    REQUIRE(song.findClip(patId, clipId)->transposeSemitones == 7);

    // Negative semitones
    REQUIRE(song.setClipTranspose(patId, clipId, -12));
    REQUIRE(song.clipTranspose(patId, clipId) == -12);

    // Clamping beyond [-48, 48]
    REQUIRE(song.setClipTranspose(patId, clipId, 100));
    REQUIRE(song.clipTranspose(patId, clipId) == 48);

    REQUIRE(song.setClipTranspose(patId, clipId, -100));
    REQUIRE(song.clipTranspose(patId, clipId) == -48);

    // Invalid pattern or clip returns 0 and false
    REQUIRE_FALSE(song.setClipTranspose(PatternId{999}, clipId, 5));
    REQUIRE_FALSE(song.setClipTranspose(patId, ClipId{999}, 5));
    REQUIRE(song.clipTranspose(patId, ClipId{999}) == 0);
}

TEST_CASE("SongHistory tracks clip transposition across undo and redo")
{
    Song song;
    SongHistory history;
    const auto s = song.addSample(makeLoop(2.0, 1));
    const auto patId = song.addPattern("P", 1);
    const auto clipId = *song.placeClip(patId, 0, s, 0);

    history.pushState(song);
    song.setClipTranspose(patId, clipId, 5);
    REQUIRE(song.clipTranspose(patId, clipId) == 5);

    REQUIRE(history.undo(song));
    REQUIRE(song.clipTranspose(patId, clipId) == 0);

    REQUIRE(history.redo(song));
    REQUIRE(song.clipTranspose(patId, clipId) == 5);
}

TEST_CASE("Song time signature supports custom meters and computes steps per bar/beat")
{
    Song song;
    REQUIRE(song.timeSigNumerator() == 4);
    REQUIRE(song.timeSigDenominator() == 4);
    REQUIRE(song.stepsPerBeat() == 8);
    REQUIRE(song.stepsPerBar() == 32);

    song.setTimeSignature(3, 4);
    REQUIRE(song.timeSigNumerator() == 3);
    REQUIRE(song.timeSigDenominator() == 4);
    REQUIRE(song.stepsPerBeat() == 8);
    REQUIRE(song.stepsPerBar() == 24);

    const auto patId = song.addPattern("Empty", 1);
    REQUIRE(song.findPattern(patId)->lengthSteps() == 24);

    song.setTimeSignature(6, 8);
    REQUIRE(song.timeSigNumerator() == 6);
    REQUIRE(song.timeSigDenominator() == 8);
    REQUIRE(song.stepsPerBeat() == 4);
    REQUIRE(song.stepsPerBar() == 24);
    REQUIRE(song.findPattern(patId)->lengthSteps() == 24);

    song.setTimeSignature(5, 4);
    REQUIRE(song.stepsPerBar() == 40);
    REQUIRE(song.findPattern(patId)->lengthSteps() == 40);

    // Clamping & invalid denominator fallback
    song.setTimeSignature(99, 3);
    REQUIRE(song.timeSigNumerator() == 16);
    REQUIRE(song.timeSigDenominator() == 4);
}

TEST_CASE("SongHistory undoes and redoes multi-point automation curves point by point")
{
    Song song;
    SongHistory history;
    const auto patId = song.addPattern("P", 2);

    // Step 1: Enable master curve with 2 endpoints
    history.pushState(song);
    song.setMasterAutomation(patId, {{0.0, 1.0f, EnvelopeCurve::Linear}, {64.0, 1.0f, EnvelopeCurve::Linear}});
    REQUIRE(song.findPattern(patId)->masterVolumeAutomation.size() == 2);

    // Step 2: Add point A
    history.pushState(song);
    song.setMasterAutomation(patId, {{0.0, 1.0f, EnvelopeCurve::Linear}, {16.0, 0.5f, EnvelopeCurve::Linear}, {64.0, 1.0f, EnvelopeCurve::Linear}});
    REQUIRE(song.findPattern(patId)->masterVolumeAutomation.size() == 3);

    // Step 3: Add point B
    history.pushState(song);
    song.setMasterAutomation(patId, {{0.0, 1.0f, EnvelopeCurve::Linear}, {16.0, 0.5f, EnvelopeCurve::Linear}, {32.0, 1.5f, EnvelopeCurve::Linear}, {64.0, 1.0f, EnvelopeCurve::Linear}});
    REQUIRE(song.findPattern(patId)->masterVolumeAutomation.size() == 4);

    // Undo point by point
    REQUIRE(history.undo(song));
    REQUIRE(song.findPattern(patId)->masterVolumeAutomation.size() == 3);
    REQUIRE(history.undo(song));
    REQUIRE(song.findPattern(patId)->masterVolumeAutomation.size() == 2);
    REQUIRE(history.undo(song));
    REQUIRE(song.findPattern(patId)->masterVolumeAutomation.empty());

    // Redo point by point
    REQUIRE(history.redo(song));
    REQUIRE(song.findPattern(patId)->masterVolumeAutomation.size() == 2);
    REQUIRE(history.redo(song));
    REQUIRE(song.findPattern(patId)->masterVolumeAutomation.size() == 3);
    REQUIRE(history.redo(song));
    REQUIRE(song.findPattern(patId)->masterVolumeAutomation.size() == 4);
}
