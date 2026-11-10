#include "model/Pattern.h"

#include "model/Timing.h"

#include <algorithm>

namespace trami
{

int Pattern::lengthSteps() const noexcept
{
    return lengthBars * kStepsPerBar;
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
