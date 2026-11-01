#include "model/Pattern.h"

#include "model/Timing.h"

namespace trami
{

int Pattern::lengthSteps() const noexcept
{
    return lengthBars * kStepsPerBar;
}

} // namespace trami
