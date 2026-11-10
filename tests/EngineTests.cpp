#include "engine/Sequencer.h"
#include "engine/SnapshotBuilder.h"
#include "engine/SnapshotExchange.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace trami;

namespace
{

constexpr double kRate = 48000.0;

auto impulse()
{
    auto data = std::make_shared<AudioData>();
    data->channels.push_back({1.0f, 0.0f, 0.0f, 0.0f});
    return data;
}

std::vector<std::int64_t> render(Sequencer& seq, int total, int blockSize)
{
    std::vector<float> left(static_cast<std::size_t>(total)), right(static_cast<std::size_t>(total));
    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int n = std::min(blockSize, total - pos);
        seq.process(left.data() + pos, right.data() + pos, n);
    }

    std::vector<std::int64_t> onsets;
    for (int i = 0; i < total; ++i)
        if (left[static_cast<std::size_t>(i)] > 0.5f)
            onsets.push_back(i);
    return onsets;
}

struct Fixture
{
    Song song;
    SampleBank bank;
    SampleId sample;
    PatternId pattern;

    Fixture()
    {
        song.setBpm(120.0); // 6000 samples per step at 48 kHz (96000 samples per bar)
        Sample s;
        s.name = "impulse";
        sample = song.addSample(s);
        bank[sample] = impulse();
        pattern = song.addPattern("P", 1);
    }

    auto snapshot() const { return SnapshotBuilder::buildPattern(song, pattern, bank, kRate); }
};

} // namespace

TEST_CASE("Snapshot exchange hands over and recycles snapshots")
{
    SnapshotExchange exchange;
    REQUIRE(exchange.takePending() == nullptr);

    exchange.publish(std::make_unique<PlaybackSnapshot>());
    PlaybackSnapshot* taken = exchange.takePending();
    REQUIRE(taken != nullptr);
    REQUIRE(exchange.takePending() == nullptr);

    REQUIRE(exchange.tryRetire(taken));
    REQUIRE_FALSE(exchange.tryRetire(taken));
    exchange.collectGarbage();
    REQUIRE(exchange.tryRetire(new PlaybackSnapshot()));
}

TEST_CASE("Constant-power pan has unity gain at centre")
{
    const auto [l, r] = SnapshotBuilder::panGains(0.0f);
    REQUIRE(l == Catch::Approx(1.0f));
    REQUIRE(r == Catch::Approx(1.0f));
    REQUIRE(SnapshotBuilder::panGains(-1.0f).second == Catch::Approx(0.0f).margin(1e-6));
}

TEST_CASE("Events trigger on the exact sample regardless of block size")
{
    for (int blockSize : {64, 441, 512, 4096})
    {
        Fixture f;
        f.song.placeClip(f.pattern, 0, f.sample, 0);
        f.song.placeClip(f.pattern, 1, f.sample, 6);
        f.song.placeClip(f.pattern, 2, f.sample, 30);

        SnapshotExchange exchange;
        exchange.publish(f.snapshot());
        Sequencer seq(exchange, kRate);
        seq.requestPlaying(true);

        const auto onsets = render(seq, 96000, blockSize);
        REQUIRE(onsets == std::vector<std::int64_t>{0, 18000, 90000});
    }
}

TEST_CASE("Pattern loops seamlessly")
{
    Fixture f;
    f.song.placeClip(f.pattern, 0, f.sample, 0);

    SnapshotExchange exchange;
    exchange.publish(f.snapshot());
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    REQUIRE(render(seq, 96000 * 3, 500) == std::vector<std::int64_t>{0, 96000, 192000});
}

TEST_CASE("Swing shifts odd steps by the exact amount")
{
    Fixture f;
    f.song.setSwing(0.5);
    f.song.placeClip(f.pattern, 0, f.sample, 2);
    f.song.placeClip(f.pattern, 0, f.sample, 4);

    SnapshotExchange exchange;
    exchange.publish(f.snapshot());
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    REQUIRE(render(seq, 96000, 256) == std::vector<std::int64_t>{7500, 12000});
}

TEST_CASE("Nothing plays while stopped; rewind returns to start")
{
    Fixture f;
    f.song.placeClip(f.pattern, 0, f.sample, 0);

    SnapshotExchange exchange;
    exchange.publish(f.snapshot());
    Sequencer seq(exchange, kRate);

    REQUIRE(render(seq, 10000, 512).empty());

    seq.requestPlaying(true);
    render(seq, 10000, 512);
    REQUIRE(seq.position() == 10000);

    seq.requestRewind();
    REQUIRE(render(seq, 100, 100) == std::vector<std::int64_t>{0});
}

TEST_CASE("Metronome clicks on every beat")
{
    Fixture f;

    SnapshotExchange exchange;
    exchange.publish(f.snapshot());
    Sequencer seq(exchange, kRate);
    seq.setMetronomeEnabled(true);
    seq.requestPlaying(true);

    std::vector<float> left(96000), right(96000);
    seq.process(left.data(), right.data(), 96000);

    std::vector<int> starts;
    for (int i = 1; i < 96000; ++i)
        if (left[static_cast<std::size_t>(i)] != 0.0f && left[static_cast<std::size_t>(i - 1)] == 0.0f &&
            (starts.empty() || i - starts.back() > 2000))
            starts.push_back(i - 1);
    REQUIRE(starts == std::vector<int>{0, 24000, 48000, 72000});
}

TEST_CASE("New snapshots are adopted while playing and keep the position")
{
    Fixture f;
    SnapshotExchange exchange;
    exchange.publish(f.snapshot());
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);
    render(seq, 30000, 512);

    f.song.placeClip(f.pattern, 0, f.sample, 16);
    exchange.publish(f.snapshot());

    const auto onsets = render(seq, 30000, 512);
    REQUIRE(onsets == std::vector<std::int64_t>{18000});
}

TEST_CASE("Arrangement plays pattern blocks across multiple tracks and stops at end")
{
    Fixture f;
    f.song.placeClip(f.pattern, 0, f.sample, 0); // hit at beat 1

    const auto pat2 = f.song.addPattern("P2", 1);
    f.song.placeClip(pat2, 0, f.sample, 8); // hit at beat 2 (step 8 = 24000 samples)

    // Track 0: Pattern 1 at bar 0 (0..96000) and bar 2 (192000..288000)
    f.song.placeBlock(0, f.pattern, 0);
    f.song.placeBlock(0, f.pattern, 2);

    // Track 1: Pattern 2 at bar 1 (96000..192000)
    f.song.placeBlock(1, pat2, 1);

    auto snap = SnapshotBuilder::buildArrangement(f.song, f.bank, kRate);
    REQUIRE_FALSE(snap->looping);
    REQUIRE(snap->lengthSamples == 96000 * 3); // 3 bars total

    SnapshotExchange exchange;
    exchange.publish(std::move(snap));
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    const auto onsets = render(seq, 96000 * 3, 512);
    // Bar 0: sample 0
    // Bar 1: sample 96000 + 24000 = 120000
    // Bar 2: sample 192000
    REQUIRE(onsets == std::vector<std::int64_t>{0, 120000, 192000});
}

TEST_CASE("Arrangement loops within custom loop region")
{
    Fixture f;
    f.song.placeClip(f.pattern, 0, f.sample, 0); // hit at beat 1

    f.song.placeBlock(0, f.pattern, 0);

    auto snap = SnapshotBuilder::buildArrangement(f.song, f.bank, kRate, 0, 1);
    REQUIRE(snap->looping);
    REQUIRE(snap->loopStartSample == 0);
    REQUIRE(snap->loopEndSample == 96000);

    SnapshotExchange exchange;
    exchange.publish(std::move(snap));
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    const auto onsets = render(seq, 150000, 512);
    REQUIRE(onsets.size() == 2);
    REQUIRE(onsets[0] == 0);
    REQUIRE(onsets[1] == 96000);
}

TEST_CASE("Muted and soloed pattern tracks in playback")
{
    Fixture f;
    f.song.placeClip(f.pattern, 0, f.sample, 0);
    f.song.placeClip(f.pattern, 1, f.sample, 8);

    // Both play normally
    auto snapNormal = SnapshotBuilder::buildPattern(f.song, f.pattern, f.bank, kRate);
    REQUIRE(snapNormal->events.size() == 2);

    // Mute track 0
    f.song.findPattern(f.pattern)->tracks[0].muted = true;
    auto snapMuted = SnapshotBuilder::buildPattern(f.song, f.pattern, f.bank, kRate);
    REQUIRE(snapMuted->events.size() == 1);
    REQUIRE(snapMuted->events[0].startSample == 24000);

    // Solo track 0 while unmuted
    f.song.findPattern(f.pattern)->tracks[0].muted = false;
    f.song.findPattern(f.pattern)->tracks[0].solo = true;
    auto snapSolo = SnapshotBuilder::buildPattern(f.song, f.pattern, f.bank, kRate);
    REQUIRE(snapSolo->events.size() == 1);
    REQUIRE(snapSolo->events[0].startSample == 0);
}

TEST_CASE("Muted arranger tracks are excluded from playback")
{
    Fixture f;
    f.song.placeClip(f.pattern, 0, f.sample, 0);
    f.song.placeBlock(0, f.pattern, 0);
    f.song.arrangerTrack(0)->muted = true;

    auto snap = SnapshotBuilder::buildArrangement(f.song, f.bank, kRate);
    REQUIRE(snap->events.empty());
}

TEST_CASE("Soloed arranger tracks silence other tracks")
{
    Fixture f;
    auto pat2 = f.song.addPattern("Pattern 2", 1);
    f.song.placeClip(f.pattern, 0, f.sample, 0);
    f.song.placeClip(pat2, 0, f.sample, 0);

    f.song.placeBlock(0, f.pattern, 0);
    f.song.placeBlock(1, pat2, 0);

    // Solo track 1
    f.song.arrangerTrack(1)->solo = true;
    auto snap = SnapshotBuilder::buildArrangement(f.song, f.bank, kRate);
    REQUIRE(snap->events.size() == 1);
}

TEST_CASE("Preview plays and can be replaced mid-playback without crash")
{
    SnapshotExchange exchange;
    Sequencer seq(exchange, kRate);

    auto audio1 = std::make_shared<AudioData>();
    audio1->channels.push_back(std::vector<float>(10000, 1.0f));

    auto audio2 = std::make_shared<AudioData>();
    audio2->channels.push_back(std::vector<float>(10000, 0.5f));

    seq.triggerPreview(audio1);

    std::vector<float> left(256, 0.0f), right(256, 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(seq.isPreviewActive());
    REQUIRE(left[0] > 0.0f);

    // Trigger second preview while first is still actively playing
    seq.triggerPreview(audio2);

    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(seq.isPreviewActive());

    // Stop preview
    seq.stopPreview();
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE_FALSE(seq.isPreviewActive());
}

TEST_CASE("Rapid preview triggers do not crash or leak")
{
    SnapshotExchange exchange;
    Sequencer seq(exchange, kRate);

    std::vector<float> left(128, 0.0f), right(128, 0.0f);

    for (int i = 0; i < 50; ++i)
    {
        auto audio = std::make_shared<AudioData>();
        audio->channels.push_back(std::vector<float>(500, 0.7f));
        seq.triggerPreview(std::move(audio));

        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        seq.process(left.data(), right.data(), 128);
    }

    seq.collectPreviewGarbage();
    REQUIRE(true);
}

TEST_CASE("Real-time track mix changes take effect immediately during playback")
{
    Fixture f;
    // Sustained audio buffer with amplitude 1.0f
    auto sustained = std::make_shared<AudioData>();
    sustained->channels.push_back(std::vector<float>(96000, 1.0f));
    sustained->channels.push_back(std::vector<float>(96000, 1.0f));
    f.bank[f.sample] = sustained;

    f.song.placeClip(f.pattern, 0, f.sample, 0); // track 0
    f.song.placeClip(f.pattern, 1, f.sample, 0); // track 1

    SnapshotExchange exchange;
    exchange.publish(f.snapshot());
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    std::vector<float> left(256, 0.0f), right(256, 0.0f);

    // 1. Initial playback: both tracks play at volume 1.0 (left should be ~2.0)
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[10] == Catch::Approx(2.0f));

    // 2. Mute track 0: voices on track 0 should silence immediately
    seq.setTrackMuted(0, true);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[10] == Catch::Approx(1.0f)); // Only track 1 is sounding

    // 3. Mute track 1: both tracks muted -> silence immediately
    seq.setTrackMuted(1, true);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[10] == Catch::Approx(0.0f));

    // 4. Unmute track 0: track 0 sounds immediately
    seq.setTrackMuted(0, false);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[10] == Catch::Approx(1.0f));

    // 5. Volume adjustment on track 0 from 1.0 to 0.5: gain changes immediately
    seq.setTrackVolume(0, 0.5f);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[10] == Catch::Approx(0.5f));

    // 6. Pan adjustment on track 0 to full right (+1.0): left channel becomes 0 immediately
    seq.setTrackPan(0, 1.0f);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[10] == Catch::Approx(0.0f).margin(1e-5));
    REQUIRE(right[10] > 0.0f);

    // 7. Unmute track 1 and solo track 0: track 1 is silenced, only soloed track 0 sounds
    seq.setTrackMuted(1, false);
    seq.setTrackSolo(0, true);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[10] == Catch::Approx(0.0f).margin(1e-5));
    REQUIRE(right[10] > 0.0f);
}

TEST_CASE("Clips are cut off at the loop point of the pattern")
{
    Fixture f;
    // Pattern f.pattern is 1 bar (96000 samples, 32 1/32-steps).
    // Sample is 96000 samples long.
    // Place clip at step 16 (half-way through bar):
    // startSample is 48000.
    // The pattern ends at 96000.
    // The event's maxFrames must be capped to 48000 so it does not spill past the loop boundary!
    f.song.placeClip(f.pattern, 0, f.sample, 16);
    auto snap = SnapshotBuilder::buildPattern(f.song, f.pattern, f.bank, kRate);
    REQUIRE(snap->events.size() == 1);
    REQUIRE(snap->events[0].startSample == 48000);
    REQUIRE(snap->events[0].maxFrames == 48000);
}

TEST_CASE("Sequencer loops within custom loop region")
{
    Fixture f;
    // Clip at step 4 (sample 12000) and step 8 (sample 24000)
    f.song.placeClip(f.pattern, 0, f.sample, 4);
    f.song.placeClip(f.pattern, 0, f.sample, 8);

    // Build pattern with loop region from step 4 (12000) to step 8 (24000)
    auto snap = SnapshotBuilder::buildPattern(f.song, f.pattern, f.bank, kRate, 4, 8);
    REQUIRE(snap->loopStartSample == 12000);
    REQUIRE(snap->loopEndSample == 24000);

    SnapshotExchange exchange;
    exchange.publish(std::move(snap));
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    // Play 36000 samples. It should start at 0, hit 12000, then wrap at 24000 back to 12000!
    // So triggers will be at 12000 and 24000 (after loop wrap at 12000 again)
    auto triggers = render(seq, 36000, 256);
    REQUIRE(triggers.size() >= 2);
    REQUIRE(triggers[0] == 12000);
    REQUIRE(triggers[1] == 24000);
}

TEST_CASE("Sequencer seek updates position and stops active voices")
{
    Fixture f;
    f.song.placeClip(f.pattern, 0, f.sample, 0);

    auto snap = SnapshotBuilder::buildPattern(f.song, f.pattern, f.bank, kRate);
    SnapshotExchange exchange;
    exchange.publish(std::move(snap));
    Sequencer seq(exchange, kRate);

    // Initial position is 0
    REQUIRE(seq.position() == 0);

    // Seek to sample 48000 while stopped
    seq.requestSeek(48000);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(seq.position() == 48000);

    // Start playing, let it advance
    seq.requestPlaying(true);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(seq.position() == 48000 + 256);

    // Seek back to sample 10000
    seq.requestSeek(10000);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(seq.position() == 10000 + 256);
}

TEST_CASE("Clip volume envelope shapes playback audio with gain and fades")
{
    Fixture f;
    auto data = std::make_shared<AudioData>();
    data->channels.push_back(std::vector<float>(1000, 1.0f));
    f.bank[f.sample] = data;

    const auto clipId = *f.song.placeClip(f.pattern, 0, f.sample, 0);
    // Set 50% gain, 4 steps fade in, 4 steps fade out
    f.song.setClipEnvelope(f.pattern, clipId, 0.5f, 4.0f, 4.0f);

    auto snap = SnapshotBuilder::buildPattern(f.song, f.pattern, f.bank, kRate);
    REQUIRE(snap->events.size() == 1);
    REQUIRE(snap->events[0].gainLeft == Catch::Approx(0.5f));
    REQUIRE(snap->events[0].fadeInSamples > 0);
    REQUIRE(snap->events[0].fadeOutSamples > 0);

    SnapshotExchange exchange;
    exchange.publish(std::move(snap));
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    seq.process(left.data(), right.data(), 256);
    // Audio starts at 0 due to fade-in and smoothly ramps up
    REQUIRE(left[0] == Catch::Approx(0.0f).margin(0.001f));
    REQUIRE(left[255] > 0.0f);
}

TEST_CASE("Sound clip longer than loop region stops at end of loop without overlapping itself")
{
    Fixture f;
    // Sample is 4 bars long (96000 * 4 = 384000 samples) filled with constant 0.5f
    auto data = std::make_shared<AudioData>();
    data->channels.push_back(std::vector<float>(96000 * 4, 0.5f));
    f.bank[f.sample] = data;

    // Pattern is 4 bars long, but custom loop region is set to 1 bar (steps 0 to 32)
    f.song.setPatternLength(f.pattern, 4);
    f.song.placeClip(f.pattern, 0, f.sample, 0);

    // Build pattern snapshot with 1-bar loop (steps 0 to 32, i.e. 0 to 96000 samples)
    auto snap = SnapshotBuilder::buildPattern(f.song, f.pattern, f.bank, kRate, 0, 32);
    REQUIRE(snap->loopStartSample == 0);
    REQUIRE(snap->loopEndSample == 96000);
    REQUIRE(snap->events.size() == 1);
    // maxFrames must be capped to 96000 so the clip stops at the end of the loop!
    REQUIRE(snap->events[0].maxFrames == 96000);

    SnapshotExchange exchange;
    exchange.publish(std::move(snap));
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    // Render 2 full loops (192000 samples).
    // In every block, output should be exactly 0.5f (from single voice) and NEVER 1.0f (which would indicate overlapping voices)!
    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    float maxSample = 0.0f;
    for (int rendered = 0; rendered < 192000; rendered += 512)
    {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        seq.process(left.data(), right.data(), 512);
        for (float s : left)
            maxSample = std::max(maxSample, std::abs(s));
    }

    // Since voices never overlap, maxSample should be 0.5f (or less with master), and never >= 0.9f
    REQUIRE(maxSample == Catch::Approx(0.5f).margin(0.01f));
}

TEST_CASE("Sequencer master volume scales overall output audio")
{
    Fixture f;
    auto sustained = std::make_shared<AudioData>();
    sustained->channels.push_back(std::vector<float>(96000, 1.0f));
    sustained->channels.push_back(std::vector<float>(96000, 1.0f));
    f.bank[f.sample] = sustained;
    f.song.placeClip(f.pattern, 0, f.sample, 0);

    auto snap = SnapshotBuilder::buildPattern(f.song, f.pattern, f.bank, kRate);
    SnapshotExchange exchange;
    exchange.publish(std::move(snap));
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    // Default unity master volume (1.0f)
    REQUIRE(seq.masterVolume() == Catch::Approx(1.0f));
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[10] == Catch::Approx(1.0f));

    // Master volume to 0.5f
    seq.setMasterVolume(0.5f);
    REQUIRE(seq.masterVolume() == Catch::Approx(0.5f));
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[10] == Catch::Approx(0.5f));

    // Master volume to 0.0f (mute)
    seq.setMasterVolume(0.0f);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[10] == Catch::Approx(0.0f));
}

TEST_CASE("Sequencer output loudness metering detects peaks, RMS and clipping")
{
    Fixture f;
    auto sustained = std::make_shared<AudioData>();
    sustained->channels.push_back(std::vector<float>(96000, 0.8f));
    sustained->channels.push_back(std::vector<float>(96000, 0.8f));
    f.bank[f.sample] = sustained;
    f.song.placeClip(f.pattern, 0, f.sample, 0);

    auto snap = SnapshotBuilder::buildPattern(f.song, f.pattern, f.bank, kRate);
    SnapshotExchange exchange;
    exchange.publish(std::move(snap));
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    seq.process(left.data(), right.data(), 256);
    auto levels = seq.outputLevels();
    REQUIRE(levels.peakL == Catch::Approx(0.8f));
    REQUIRE(levels.peakR == Catch::Approx(0.8f));
    REQUIRE(levels.rmsL > 0.0f);
    REQUIRE(levels.clipped == false);

    // Boost master volume to 1.5 -> output will be 0.8 * 1.5 = 1.2 >= 1.0 (clipping!)
    seq.setMasterVolume(1.5f);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);

    levels = seq.outputLevels();
    REQUIRE(levels.peakL == Catch::Approx(1.2f));
    REQUIRE(levels.clipped == true);

    // Lower master volume and let it ramp down
    seq.setMasterVolume(0.5f);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);

    // Reset clip indicator and process clean block
    seq.resetClipped();
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);

    levels = seq.outputLevels();
    REQUIRE(levels.clipped == false);
}

TEST_CASE("Removing an active clip silences its voice immediately during playback")
{
    Fixture fix;
    auto sound = std::make_shared<AudioData>();
    sound->channels.push_back(std::vector<float>(48000, 0.9f));
    fix.bank[fix.sample] = sound;

    auto clipId = fix.song.placeClip(fix.pattern, 0, fix.sample, 0.0);
    REQUIRE(clipId.has_value());

    SnapshotExchange exchange;
    exchange.publish(SnapshotBuilder::buildPattern(fix.song, fix.pattern, fix.bank, kRate));
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    // Initial block triggers clip at step 0
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[0] == Catch::Approx(0.9f));
    REQUIRE(left[100] == Catch::Approx(0.9f));

    // Remove clip from pattern while playback is in progress
    fix.song.removeClip(fix.pattern, *clipId);
    exchange.publish(SnapshotBuilder::buildPattern(fix.song, fix.pattern, fix.bank, kRate));

    // Next block adopts new snapshot; voice for deleted clip should be silenced immediately
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);

    for (int i = 0; i < 256; ++i)
    {
        REQUIRE(left[i] == 0.0f);
        REQUIRE(right[i] == 0.0f);
    }
}

TEST_CASE("Dragging a clip is reflected in real time via snapshot overrides and immediate voice sync")
{
    Fixture fix;
    auto sound = std::make_shared<AudioData>();
    sound->channels.push_back(std::vector<float>(48000, 0.8f));
    fix.bank[fix.sample] = sound;

    auto clipId = fix.song.placeClip(fix.pattern, 0, fix.sample, 0.0);
    REQUIRE(clipId.has_value());

    SnapshotExchange exchange;
    exchange.publish(SnapshotBuilder::buildPattern(fix.song, fix.pattern, fix.bank, kRate));
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    // Initial block plays clip at step 0
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[0] == Catch::Approx(0.8f));

    // User drags clip to step 4 (sample 12000 at 120 BPM) - far in the future
    DragClipOverride dragOv;
    dragOv.clipId = *clipId;
    dragOv.originalTrack = 0;
    dragOv.originalStartStep = 0.0;
    dragOv.currentTrack = 0;
    dragOv.currentStartStep = 4.0;
    dragOv.currentLengthSteps = 4.0;
    dragOv.currentGain = 0.8f;
    std::vector<DragClipOverride> overrides = {dragOv};

    auto draggedSnap = SnapshotBuilder::buildPattern(fix.song, fix.pattern, fix.bank, kRate, 0, 0, nullptr, &overrides);
    REQUIRE(draggedSnap->events.size() == 1);
    REQUIRE(draggedSnap->events[0].startSample == 12000);
    REQUIRE(draggedSnap->events[0].clipId == clipId->value);

    // Publish drag state while sequencer is around sample 256
    exchange.publish(std::move(draggedSnap));

    // Next block: voice at step 0 is stopped because clip moved away
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);
    REQUIRE(left[0] == 0.0f);

    // Seek/jump right before step 4 (sample 11900)
    seq.requestSeek(11900);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256); // crosses 12000 (offset 100)

    // Triggered at offset 100 in this block
    REQUIRE(left[99] == 0.0f);
    REQUIRE(left[100] == Catch::Approx(0.64f)); // 0.8 sample * 0.8 gain
}

TEST_CASE("Swing changes are reflected in real time during active playback")
{
    Fixture fix;
    auto sound = std::make_shared<AudioData>();
    sound->channels.push_back(std::vector<float>(48000, 1.0f));
    fix.bank[fix.sample] = sound;

    // Place clip on 16th-note offbeat step 2 (each 16th note is 2 steps in 1/32 grid)
    auto clipId = fix.song.placeClip(fix.pattern, 0, fix.sample, 2.0);
    REQUIRE(clipId.has_value());

    // Initially swing is 0 (straight): at 120 BPM, step 2 is at sample 6000
    fix.song.setSwing(0.0);
    SnapshotExchange exchange;
    exchange.publish(SnapshotBuilder::buildPattern(fix.song, fix.pattern, fix.bank, kRate));
    Sequencer seq(exchange, kRate);
    seq.requestPlaying(true);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    // Verify initial snapshot event startSample
    {
        auto snap = SnapshotBuilder::buildPattern(fix.song, fix.pattern, fix.bank, kRate);
        REQUIRE(snap->events.size() == 1);
        REQUIRE(snap->events[0].startSample == 6000);
    }

    // Now change swing to 50% (0.5) in real time
    fix.song.setSwing(0.5);
    auto swungSnap = SnapshotBuilder::buildPattern(fix.song, fix.pattern, fix.bank, kRate);
    REQUIRE(swungSnap->events.size() == 1);
    // Offbeat swing offset = 0.5 * 3000 = 1500 samples -> startSample = 6000 + 1500 = 7500
    REQUIRE(swungSnap->events[0].startSample == 7500);

    // Publish swung snapshot during playback while playhead is at sample 5000
    seq.requestSeek(5000);
    exchange.publish(std::move(swungSnap));

    // Process a block around the old trigger sample (sample 5900..6156)
    seq.requestSeek(5900);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);

    // Old trigger at sample 6000 (offset 100) must NOT fire because swing delayed it to 7500
    REQUIRE(left[100] == 0.0f);
    for (float sampleVal : left)
        REQUIRE(sampleVal == 0.0f);

    // Now process a block around sample 7400 (7400..7656)
    seq.requestSeek(7400);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    seq.process(left.data(), right.data(), 256);

    // Should fire at sample 7500 (offset 100)
    REQUIRE(left[99] == 0.0f);
    REQUIRE(left[100] == Catch::Approx(1.0f));

    // Test fractional step offbeat detection (e.g. unconstrained clip placed around step 1.98)
    fix.song.removeClip(fix.pattern, *clipId);
    auto fracClip = fix.song.placeClip(fix.pattern, 0, fix.sample, 1.98);
    REQUIRE(fracClip.has_value());

    auto fracSnap = SnapshotBuilder::buildPattern(fix.song, fix.pattern, fix.bank, kRate);
    REQUIRE(fracSnap->events.size() == 1);
    // Continuous swing scaling for fractional steps (1.98 * (2.0 + 0.5) / 2.0 * 3000 = 7425 samples)
    REQUIRE(fracSnap->events[0].startSample == 7425);
}
