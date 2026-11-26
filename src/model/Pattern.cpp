#include "model/Pattern.h"

#include "model/Timing.h"

#include <algorithm>
#include <cmath>

namespace trami
{

float evaluateEnvelope(const std::vector<EnvelopePoint>& points, double pos, float defaultVal) noexcept
{
    if (points.empty())
        return defaultVal;
    if (pos <= points.front().position)
        return points.front().value;
    if (pos >= points.back().position)
        return points.back().value;

    for (std::size_t i = 0; i + 1 < points.size(); ++i)
    {
        const auto& p0 = points[i];
        const auto& p1 = points[i + 1];
        if (pos >= p0.position && pos <= p1.position)
        {
            const double span = p1.position - p0.position;
            if (span <= 1e-9)
                return p1.value;
            const double t = std::clamp((pos - p0.position) / span, 0.0, 1.0);
            double shaped = t;
            if (p0.curve == EnvelopeCurve::Fast)
                shaped = std::sqrt(t);
            else if (p0.curve == EnvelopeCurve::Slow)
                shaped = t * t;
            return static_cast<float>(p0.value + shaped * (p1.value - p0.value));
        }
    }
    return points.back().value;
}

namespace
{
EnvelopeCurve invertEnvelopeCurve(EnvelopeCurve c) noexcept
{
    if (c == EnvelopeCurve::Fast)
        return EnvelopeCurve::Slow;
    if (c == EnvelopeCurve::Slow)
        return EnvelopeCurve::Fast;
    return EnvelopeCurve::Linear;
}
} // namespace

std::vector<EnvelopePoint> reverseEnvelope(const std::vector<EnvelopePoint>& points, double minPos, double maxPos)
{
    if (points.empty())
        return {};

    const std::size_t n = points.size();
    std::vector<EnvelopePoint> out(n);
    for (std::size_t k = 0; k < n; ++k)
    {
        const auto& src = points[n - 1 - k];
        out[k].position = std::clamp((minPos + maxPos) - src.position, minPos, maxPos);
        out[k].value = src.value;
        out[k].curve = (k + 1 < n) ? invertEnvelopeCurve(points[n - 2 - k].curve) : EnvelopeCurve::Linear;
    }
    return out;
}

int activeChainLatencySamples(const std::vector<PluginSlot>& chain) noexcept
{
    int total = 0;
    for (const auto& s : chain)
    {
        if (!s.bypassed && s.latencySamples > 0)
            total += s.latencySamples;
    }
    return total;
}

int Pattern::lengthSteps() const noexcept
{
    return lengthBars * stepsPerBar;
}

std::unordered_map<std::uint32_t, ClipCrossfade> computeTrackCrossfades(std::vector<TrackClipSpan> clips)
{
    std::unordered_map<std::uint32_t, ClipCrossfade> result;
    if (clips.empty())
        return result;

    for (const auto& c : clips)
    {
        result[c.id.value] = {std::max(0.0f, c.explicitFadeIn), std::max(0.0f, c.explicitFadeOut)};
    }

    // Sort by startStep, then spanSteps
    std::sort(clips.begin(), clips.end(), [](const TrackClipSpan& a, const TrackClipSpan& b) {
        if (a.startStep != b.startStep)
            return a.startStep < b.startStep;
        return a.spanSteps < b.spanSteps;
    });

    const std::size_t n = clips.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto& a = clips[i];
        if (a.spanSteps <= 0.0)
            continue;
        const double endA = a.startStep + a.spanSteps;

        for (std::size_t j = i + 1; j < n; ++j)
        {
            const auto& b = clips[j];
            if (b.spanSteps <= 0.0)
                continue;
            if (b.startStep >= endA)
                break; // Clips are sorted by startStep, so no later clip can overlap with a

            const double endB = b.startStep + b.spanSteps;
            const double oStart = b.startStep;
            const double oEnd = std::min(endA, endB);
            const double overlap = oEnd - oStart;

            if (overlap > 0.0)
            {
                // Clip a (earlier) fades out over the overlap
                const float fadeOut = static_cast<float>(overlap);
                result[a.id.value].fadeOutSteps = std::max(result[a.id.value].fadeOutSteps, fadeOut);

                // Clip b (later) fades in over the overlap
                const float fadeIn = static_cast<float>(overlap);
                result[b.id.value].fadeInSteps = std::max(result[b.id.value].fadeInSteps, fadeIn);
            }
        }
    }

    return result;
}

} // namespace trami
