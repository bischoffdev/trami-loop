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
    const bool isOffbeat = (step % 2) != 0;
    const double swingOffset = isOffbeat ? swing_ * samplesPerStep() * 0.5 : 0.0;
    return static_cast<std::int64_t>(std::llround(static_cast<double>(step) * samplesPerStep() + swingOffset));
}

double Timing::loopBpm(double durationSeconds, int lengthInBars) noexcept
{
    if (durationSeconds <= 0.0 || lengthInBars <= 0)
        return 0.0;
    return static_cast<double>(lengthInBars * kBeatsPerBar) * 60.0 / durationSeconds;
}

} // namespace trami
