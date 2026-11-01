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

/// Runs the sequencer for `total` samples in blocks of `blockSize` and returns
/// the sample indices where the left channel is non-zero (onsets).
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
        song.setBpm(120.0); // 6000 samples per step at 48 kHz
        Sample s;
        s.name = "impulse";
        sample = song.addSample(s);
        bank[sample] = impulse();
        pattern = song.addPattern("P", 1); // 96000 samples
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
    REQUIRE_FALSE(exchange.tryRetire(taken)); // trash slot occupied
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
    Fixture f; // empty pattern, 1 bar = 4 beats of 24000 samples

    SnapshotExchange exchange;
    exchange.publish(f.snapshot());
    Sequencer seq(exchange, kRate);
    seq.setMetronomeEnabled(true);
    seq.requestPlaying(true);

    std::vector<float> left(96000), right(96000);
    seq.process(left.data(), right.data(), 96000);

    // A click starts with a rising sine; detect the first non-zero sample after silence.
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

    f.song.placeClip(f.pattern, 0, f.sample, 8); // step 8 = sample 48000
    exchange.publish(f.snapshot());

    const auto onsets = render(seq, 30000, 512);
    REQUIRE(onsets == std::vector<std::int64_t>{18000}); // 48000 - 30000
}
