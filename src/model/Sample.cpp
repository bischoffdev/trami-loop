#include "model/Sample.h"

#include "model/Timing.h"

#include <cmath>

namespace trami
{

double Sample::originalBpm() const noexcept
{
    if (kind != SampleKind::Loop)
        return 0.0;
    if (acidBpm > 0.0)
        return acidBpm;
    if (durationSeconds > 0.0)
    {
        const double beats = loopCycleBeats();
        if (beats > 0.0)
            return (beats * 60.0) / durationSeconds;
    }
    return Timing::loopBpm(durationSeconds, loopBars);
}

float Sample::hue() const noexcept
{
    // Golden-ratio spacing gives well-separated colours for consecutive ids.
    constexpr double kGoldenRatioConjugate = 0.6180339887498949;
    double integral = 0.0;
    return static_cast<float>(std::modf(id.value * kGoldenRatioConjugate, &integral));
}

const char* Sample::pitchClassName(int midiNote) noexcept
{
    static constexpr const char* kNames[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    if (midiNote < 0)
        return "None";
    return kNames[midiNote % 12];
}

int Sample::keyConformSemitones(int sampleRootNote, int projectRootNote) noexcept
{
    if (sampleRootNote < 0 || projectRootNote < 0)
        return 0;
    int diff = (projectRootNote % 12) - (sampleRootNote % 12);
    while (diff > 6)
        diff -= 12;
    while (diff < -6)
        diff += 12;
    return diff;
}

std::vector<double> Sample::detectTransients(const float* left, const float* right,
                                             std::int64_t numFrames, double sampleRate,
                                             int maxMarkers)
{
    std::vector<double> markers;
    if (left == nullptr || numFrames <= 256 || sampleRate <= 0.0 || maxMarkers <= 0)
        return markers;

    const std::int64_t hop = std::max<std::int64_t>(128, static_cast<std::int64_t>(sampleRate * 0.01)); // ~10ms hop
    const std::int64_t numWindows = numFrames / hop;
    if (numWindows < 3)
        return markers;

    std::vector<float> energy(static_cast<std::size_t>(numWindows), 0.0f);
    for (std::int64_t w = 0; w < numWindows; ++w)
    {
        const std::int64_t start = w * hop;
        const std::int64_t end = std::min(numFrames, start + hop);
        float sum = 0.0f;
        for (std::int64_t f = start; f < end; ++f)
        {
            const float s = (right != nullptr) ? (0.5f * (left[f] + right[f])) : left[f];
            sum += s * s;
        }
        energy[static_cast<std::size_t>(w)] = std::sqrt(sum / static_cast<float>(end - start));
    }

    std::vector<float> flux(static_cast<std::size_t>(numWindows), 0.0f);
    float maxFlux = 0.0f;
    for (std::int64_t w = 1; w < numWindows; ++w)
    {
        const float diff = std::max(0.0f, energy[static_cast<std::size_t>(w)] - energy[static_cast<std::size_t>(w - 1)]);
        flux[static_cast<std::size_t>(w)] = diff;
        maxFlux = std::max(maxFlux, diff);
    }

    if (maxFlux <= 1e-4f)
        return markers;

    const float threshold = maxFlux * 0.22f;
    const std::int64_t minGapWindows = std::max<std::int64_t>(4, static_cast<std::int64_t>((sampleRate * 0.06) / static_cast<double>(hop))); // >=60ms apart

    struct Candidate
    {
        double posNorm = 0.0;
        float strength = 0.0f;
    };
    std::vector<Candidate> candidates;

    std::int64_t lastPeakW = -minGapWindows;
    for (std::int64_t w = 1; w + 1 < numWindows; ++w)
    {
        const float val = flux[static_cast<std::size_t>(w)];
        if (val >= threshold &&
            val >= flux[static_cast<std::size_t>(w - 1)] &&
            val >= flux[static_cast<std::size_t>(w + 1)])
        {
            if (w - lastPeakW >= minGapWindows)
            {
                const double norm = std::clamp(static_cast<double>(w * hop) / static_cast<double>(numFrames), 0.001, 0.999);
                candidates.push_back({norm, val});
                lastPeakW = w;
            }
            else if (!candidates.empty() && val > candidates.back().strength)
            {
                const double norm = std::clamp(static_cast<double>(w * hop) / static_cast<double>(numFrames), 0.001, 0.999);
                candidates.back() = {norm, val};
                lastPeakW = w;
            }
        }
    }

    if (static_cast<int>(candidates.size()) > maxMarkers)
    {
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate& a, const Candidate& b) { return a.strength > b.strength; });
        candidates.resize(static_cast<std::size_t>(maxMarkers));
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate& a, const Candidate& b) { return a.posNorm < b.posNorm; });
    }

    markers.reserve(candidates.size());
    for (const auto& c : candidates)
        markers.push_back(c.posNorm);

    return markers;
}

} // namespace trami
