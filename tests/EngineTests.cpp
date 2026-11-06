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
        f.song.placeClip(f.pattern, 1, f.sample, 3);
        f.song.placeClip(f.pattern, 2, f.sample, 15);

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
    f.song.placeClip(f.pattern, 0, f.sample, 1);
    f.song.placeClip(f.pattern, 0, f.sample, 2);

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

    f.song.placeClip(f.pattern, 0, f.sample, 8);
    exchange.publish(f.snapshot());

    const auto onsets = render(seq, 30000, 512);
    REQUIRE(onsets == std::vector<std::int64_t>{18000});
}

TEST_CASE("Arrangement plays pattern blocks across multiple tracks and stops at end")
{
    Fixture f;
    f.song.placeClip(f.pattern, 0, f.sample, 0); // hit at beat 1

    const auto pat2 = f.song.addPattern("P2", 1);
    f.song.placeClip(pat2, 0, f.sample, 4); // hit at beat 2 (step 4 = 24000 samples)

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

TEST_CASE("Muted and soloed pattern tracks in playback")
{
    Fixture f;
    f.song.placeClip(f.pattern, 0, f.sample, 0);
    f.song.placeClip(f.pattern, 1, f.sample, 4);

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
