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

TEST_CASE("TimeStretcher applies pitch scaling without changing duration")
{
    constexpr double rate = 48000.0;
    const auto orig = makeSine(rate, 0.5, 440.0, false);
    REQUIRE(orig.numFrames() == 24000);

    // Shift up 12 semitones (pitchScale = 2.0, time ratio = 1.0)
    const double pitchScale = std::pow(2.0, 12.0 / 12.0);
    const auto out = TimeStretcher::stretch(orig, 1.0, rate, pitchScale);
    REQUIRE(out.numFrames() > 0);
    // Length in frames should be close to original length
    const auto diff = std::abs(static_cast<long long>(out.numFrames()) - static_cast<long long>(orig.numFrames()));
    REQUIRE(diff < 2000);
}

TEST_CASE("StretchCache caches and returns transposed audio")
{
    constexpr double rate = 48000.0;
    Song song;
    song.setBpm(120.0);

    Sample s;
    s.name = "loop";
    s.kind = SampleKind::Loop;
    s.loopBars = 1;
    s.durationSeconds = 2.0;
    const auto id = song.addSample(s);

    auto raw = std::make_shared<const AudioData>(makeSine(rate, 2.0, 220.0, false));
    StretchCache cache;
    cache.setRawAudio(id, raw);

    // Semitones 0 should match base bank audio
    const auto* samplePtr = song.findSample(id);
    REQUIRE(samplePtr != nullptr);
    auto baseBank = cache.getOrUpdateSync(song, rate);
    auto audio0 = cache.getAudio(*samplePtr, song.bpm(), rate, 0);
    REQUIRE(audio0 == baseBank[id]);

    // Transposed +2 semitones creates new audio
    auto audioPlus2 = cache.getAudio(*samplePtr, song.bpm(), rate, 2);
    REQUIRE(audioPlus2 != nullptr);
    REQUIRE(audioPlus2 != baseBank[id]);

    // Querying again returns the cached shared_ptr
    auto audioPlus2Cached = cache.getAudio(*samplePtr, song.bpm(), rate, 2);
    REQUIRE(audioPlus2Cached == audioPlus2);
}

TEST_CASE("TimeStretcher resample pitches one-shots cleanly via resampling")
{
    constexpr double rate = 48000.0;
    const auto orig = makeSine(rate, 0.5, 440.0, true);
    REQUIRE(orig.numFrames() == 24000);

    // Shift up 12 semitones (pitchScale = 2.0 -> half length)
    const auto pitchedUp = TimeStretcher::resample(orig, 2.0);
    REQUIRE(pitchedUp.numChannels() == 2);
    REQUIRE(pitchedUp.numFrames() == 12000);

    // Shift down 12 semitones (pitchScale = 0.5 -> double length)
    const auto pitchedDown = TimeStretcher::resample(orig, 0.5);
    REQUIRE(pitchedDown.numChannels() == 2);
    REQUIRE(pitchedDown.numFrames() == 48000);

    // Transpose 0 semitones returns original frames
    const auto unpitched = TimeStretcher::resample(orig, 1.0);
    REQUIRE(unpitched.numFrames() == orig.numFrames());
}

TEST_CASE("StretchCache transposes one-shots using clean resampling without phase vocoder")
{
    constexpr double rate = 48000.0;
    Song song;
    song.setBpm(120.0);

    Sample s;
    s.name = "snare";
    s.kind = SampleKind::OneShot;
    s.durationSeconds = 0.25;
    const auto id = song.addSample(s);

    auto raw = std::make_shared<const AudioData>(makeSine(rate, 0.25, 440.0, false));
    StretchCache cache;
    cache.setRawAudio(id, raw);

    const auto* samplePtr = song.findSample(id);
    REQUIRE(samplePtr != nullptr);

    // +7 semitones (pitchScale = 2^(7/12) ~= 1.4983)
    auto transposed = cache.getAudio(*samplePtr, song.bpm(), rate, 7, SampleKind::OneShot);
    REQUIRE(transposed != nullptr);
    REQUIRE(transposed != raw);
    // Length should be shorter due to faster resampled playback
    REQUIRE(transposed->numFrames() < raw->numFrames());
    const double expectedFrames = std::round(static_cast<double>(raw->numFrames()) / std::pow(2.0, 7.0 / 12.0));
    REQUIRE(transposed->numFrames() == static_cast<std::int64_t>(expectedFrames));
}
