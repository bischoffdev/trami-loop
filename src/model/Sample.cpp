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
    return Timing::loopBpm(durationSeconds, loopBars);
}

float Sample::hue() const noexcept
{
    // Golden-ratio spacing gives well-separated colours for consecutive ids.
    constexpr double kGoldenRatioConjugate = 0.6180339887498949;
    double integral = 0.0;
    return static_cast<float>(std::modf(id.value * kGoldenRatioConjugate, &integral));
}

} // namespace trami
