#include "engine/StretchCache.h"
#include "engine/TimeStretcher.h"
#include "model/Song.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <future>
#include <numbers>

using namespace trami;

namespace
{

AudioData makeSine(double sampleRate, double durationSec, double freq, bool stereo)
{
    AudioData data;
    const std::size_t n = static_cast<std::size_t>(sampleRate * durationSec);
    data.channels.resize(stereo ? 2 : 1, std::vector<float>(n));

    for (std::size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double>(i) / sampleRate;
        const float val = static_cast<float>(std::sin(2.0 * std::numbers::pi * freq * t));
        data.channels[0][i] = val;
        if (stereo)
            data.channels[1][i] = -val;
    }
    return data;
}

} // namespace

TEST_CASE("TimeStretcher returns same data on ratio 1.0")
{
    const auto orig = makeSine(48000.0, 0.5, 440.0, false);
    const auto out = TimeStretcher::stretch(orig, 1.0, 48000.0);
    REQUIRE(out.numFrames() == orig.numFrames());
    REQUIRE(out.channels[0] == orig.channels[0]);
}

TEST_CASE("TimeStretcher speeds up audio with ratio < 1.0")
{
    constexpr double rate = 48000.0;
    const auto orig = makeSine(rate, 1.0, 440.0, true);
    REQUIRE(orig.numFrames() == 48000);

    const auto out = TimeStretcher::stretch(orig, 0.5, rate);
    REQUIRE(out.numChannels() == 2);
    REQUIRE(out.numFrames() > 23000);
    REQUIRE(out.numFrames() < 25000);
}

TEST_CASE("TimeStretcher slows down audio with ratio > 1.0")
{
    constexpr double rate = 48000.0;
    const auto orig = makeSine(rate, 0.5, 440.0, false);
    REQUIRE(orig.numFrames() == 24000);

    const auto out = TimeStretcher::stretch(orig, 2.0, rate);
    REQUIRE(out.numFrames() > 47000);
    REQUIRE(out.numFrames() < 49000);
}

TEST_CASE("StretchCache passes through one-shots without stretching")
{
    constexpr double rate = 48000.0;
    Song song;
    song.setBpm(130.0);

    Sample s;
    s.name = "hit";
    s.kind = SampleKind::OneShot;
    s.durationSeconds = 0.5;
    const auto id = song.addSample(s);

    auto raw = std::make_shared<const AudioData>(makeSine(rate, 0.5, 440.0, false));
    StretchCache cache;
    cache.setRawAudio(id, raw);

    auto bank = cache.getOrUpdateSync(song, rate);
    REQUIRE(bank[id] == raw);
}

TEST_CASE("StretchCache stretches loops and caches the result")
{
    constexpr double rate = 48000.0;
    Song song;
    song.setBpm(140.0); // faster than original 120

    Sample s;
    s.name = "loop";
    s.kind = SampleKind::Loop;
    s.loopBars = 1;
    s.durationSeconds = 2.0; // 1 bar at 120 BPM
    const auto id = song.addSample(s);

    auto raw = std::make_shared<const AudioData>(makeSine(rate, 2.0, 220.0, false));
    StretchCache cache;
    cache.setRawAudio(id, raw);

    auto bank1 = cache.getOrUpdateSync(song, rate);
    REQUIRE(bank1[id] != nullptr);
    REQUIRE(bank1[id] != raw); // stretched, not the raw pointer
    // Original 2.0s stretched to 140 BPM (~1.714s = ~82285 samples)
    REQUIRE(bank1[id]->numFrames() < raw->numFrames());

    // Subsequent retrieval returns the cached shared_ptr
    auto bank2 = cache.getOrUpdateSync(song, rate);
    REQUIRE(bank2[id] == bank1[id]);
}

TEST_CASE("StretchCache async update completes and delivers bank")
{
    constexpr double rate = 48000.0;
    Song song;
    song.setBpm(130.0);

    Sample s;
    s.name = "loop";
    s.kind = SampleKind::Loop;
    s.loopBars = 1;
    s.durationSeconds = 2.0;
    const auto id = song.addSample(s);

    auto raw = std::make_shared<const AudioData>(makeSine(rate, 2.0, 220.0, false));
    StretchCache cache;
    cache.setRawAudio(id, raw);

    std::promise<SampleBank> promise;
    auto future = promise.get_future();

    cache.requestUpdateAsync(song, rate, [&promise](SampleBank bank) {
        promise.set_value(std::move(bank));
    });

    REQUIRE(future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    auto bank = future.get();
    REQUIRE(bank.contains(id));
    REQUIRE(bank[id] != nullptr);
}
