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

std::int64_t Timing::stepToSample(std::int64_t step) const noexcept
{
    // In 1/32 grid, 1 beat = 8 steps, 1 16th note = 2 steps.
    // 16th-note offbeats occur when (step / 2) is odd.
    const bool is16thOffbeat = ((step / 2) % 2) != 0;
    const double swingOffset = is16thOffbeat ? swing_ * samplesPerStep() * 1.0 : 0.0;
    return static_cast<std::int64_t>(std::llround(static_cast<double>(step) * samplesPerStep() + swingOffset));
}

double Timing::loopBpm(double durationSeconds, int lengthInBars) noexcept
{
    if (durationSeconds <= 0.0 || lengthInBars <= 0)
        return 0.0;
    return static_cast<double>(lengthInBars * kBeatsPerBar) * 60.0 / durationSeconds;
}

} // namespace trami
