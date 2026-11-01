#include "model/Sample.h"

#include "model/Timing.h"

#include <cmath>

namespace trami
{

double Sample::originalBpm() const noexcept
{
    return kind == SampleKind::Loop ? Timing::loopBpm(durationSeconds, loopBars) : 0.0;
}

float Sample::hue() const noexcept
{
    // Golden-ratio spacing gives well-separated colours for consecutive ids.
    constexpr double kGoldenRatioConjugate = 0.6180339887498949;
    double integral = 0.0;
    return static_cast<float>(std::modf(id.value * kGoldenRatioConjugate, &integral));
}

} // namespace trami
