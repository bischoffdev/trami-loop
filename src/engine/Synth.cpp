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

} // namespace trami::synth
