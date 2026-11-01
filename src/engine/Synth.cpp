#include "engine/Synth.h"

#include <cmath>
#include <cstdint>
#include <numbers>

namespace trami::synth
{
namespace
{

constexpr double kTwoPi = 2.0 * std::numbers::pi;

AudioData monoBuffer(double sampleRate, double seconds)
{
    AudioData data;
    data.channels.emplace_back(static_cast<std::size_t>(sampleRate * seconds), 0.0f);
    return data;
}

/// Deterministic white noise in [-1, 1].
class Noise
{
public:
    float next() noexcept
    {
        state_ = state_ * 1664525u + 1013904223u;
        return static_cast<float>(state_) / 2147483648.0f - 1.0f;
    }

private:
    std::uint32_t state_ = 22222u;
};

} // namespace

AudioData click(double sampleRate, double frequency)
{
    AudioData data = monoBuffer(sampleRate, 0.03);
    auto& out = data.channels.front();
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const double t = static_cast<double>(i) / sampleRate;
        out[i] = static_cast<float>(std::sin(kTwoPi * frequency * t) * std::exp(-t * 150.0));
    }
    return data;
}

AudioData kick(double sampleRate)
{
    AudioData data = monoBuffer(sampleRate, 0.4);
    auto& out = data.channels.front();
    double phase = 0.0;
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const double t = static_cast<double>(i) / sampleRate;
        const double frequency = 45.0 + 110.0 * std::exp(-t * 30.0);
        phase += kTwoPi * frequency / sampleRate;
        out[i] = static_cast<float>(std::sin(phase) * std::exp(-t * 8.0) * 0.9);
    }
    return data;
}

AudioData snare(double sampleRate)
{
    AudioData data = monoBuffer(sampleRate, 0.25);
    auto& out = data.channels.front();
    Noise noise;
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const double t = static_cast<double>(i) / sampleRate;
        const double tone = std::sin(kTwoPi * 190.0 * t) * std::exp(-t * 25.0);
        const double rattle = noise.next() * std::exp(-t * 18.0);
        out[i] = static_cast<float>((tone * 0.4 + rattle * 0.5) * 0.8);
    }
    return data;
}

AudioData hihat(double sampleRate)
{
    AudioData data = monoBuffer(sampleRate, 0.08);
    auto& out = data.channels.front();
    Noise noise;
    float previous = 0.0f;
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const double t = static_cast<double>(i) / sampleRate;
        const float n = noise.next();
        const float highPassed = n - previous; // Crude high-pass for a brighter sound.
        previous = n;
        out[i] = static_cast<float>(highPassed * std::exp(-t * 60.0) * 0.35);
    }
    return data;
}

AudioData bassLoop(double sampleRate)
{
    // 1 bar at 120 BPM = exactly 2.0 seconds (4 beats of 0.5s each)
    AudioData data = monoBuffer(sampleRate, 2.0);
    auto& out = data.channels.front();

    // 4 notes: A1 (55Hz), C2 (65.4Hz), D2 (73.4Hz), F2 (87.3Hz)
    const double freqs[4] = {55.0, 65.41, 73.42, 87.31};
    const double beatDuration = 0.5;

    for (int beat = 0; beat < 4; ++beat)
    {
        const double freq = freqs[beat];
        const std::size_t startSample = static_cast<std::size_t>(beat * beatDuration * sampleRate);
        const std::size_t endSample = static_cast<std::size_t>((beat + 1) * beatDuration * sampleRate);

        double phase = 0.0;
        for (std::size_t i = startSample; i < endSample && i < out.size(); ++i)
        {
            const double tNote = static_cast<double>(i - startSample) / sampleRate;
            phase += kTwoPi * freq / sampleRate;
            // Warm saw/square-ish wave with lowpass envelope
            const double saw = 2.0 * (phase / kTwoPi - std::floor(phase / kTwoPi + 0.5));
            const double env = std::exp(-tNote * 4.0);
            out[i] = static_cast<float>(saw * env * 0.5);
        }
    }

    return data;
}

} // namespace trami::synth
