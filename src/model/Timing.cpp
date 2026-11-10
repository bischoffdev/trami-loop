#include "model/Timing.h"

#include <algorithm>
#include <cmath>

namespace trami
{

Timing::Timing(double bpm, double sampleRate, double swing)
    : bpm_(bpm), sampleRate_(sampleRate), swing_(std::clamp(swing, 0.0, 1.0))
{
}

double Timing::samplesPerStep() const noexcept
{
    const double secondsPerBeat = 60.0 / bpm_;
    return secondsPerBeat * sampleRate_ / kStepsPerBeat;
}

double Timing::samplesPerBar() const noexcept
{
    return samplesPerStep() * kStepsPerBar;
}

double Timing::stepToSwungStep(double step) const noexcept
{
    if (swing_ <= 0.0)
        return step;

    // In 1/32 grid, 1 beat = 8 steps, 1 16th note = 2 steps.
    // Each 16th-note pair has length 4 steps (e.g. 0..4, 4..8, 8..12).
    // The first 16th note [0..2] is onbeat and expands by (swing_ * 1.0) steps.
    // The second 16th note [2..4] is offbeat and contracts by (swing_ * 1.0) steps.
    const double pairBase = std::floor(step / 4.0) * 4.0;
    const double sub = step - pairBase;

    if (sub < 2.0)
    {
        return pairBase + sub * (2.0 + swing_) / 2.0;
    }
    else
    {
        return pairBase + (2.0 + swing_) + (sub - 2.0) * (2.0 - swing_) / 2.0;
    }
}

double Timing::swungStepToStep(double swungStep) const noexcept
{
    if (swing_ <= 0.0)
        return swungStep;

    const double pairBase = std::floor(swungStep / 4.0) * 4.0;
    const double sub = swungStep - pairBase;
    const double splitPoint = 2.0 + swing_;

    if (sub < splitPoint)
    {
        return pairBase + sub * 2.0 / splitPoint;
    }
    else
    {
        const double rem = 4.0 - splitPoint;
        if (rem <= 0.0001)
            return pairBase + 2.0;
        return pairBase + 2.0 + (sub - splitPoint) * 2.0 / rem;
    }
}

std::int64_t Timing::stepToSample(double step) const noexcept
{
    return static_cast<std::int64_t>(std::llround(stepToSwungStep(step) * samplesPerStep()));
}

double Timing::loopBpm(double durationSeconds, int lengthInBars) noexcept
{
    if (durationSeconds <= 0.0 || lengthInBars <= 0)
        return 0.0;
    return static_cast<double>(lengthInBars * kBeatsPerBar) * 60.0 / durationSeconds;
}

} // namespace trami
