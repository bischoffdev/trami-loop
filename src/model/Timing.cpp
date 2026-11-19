#include "model/Timing.h"

#include <algorithm>
#include <cmath>

namespace trami
{

const char* groovePresetName(GroovePreset preset) noexcept
{
    switch (preset)
    {
    case GroovePreset::Straight:
        return "Straight (Off)";
    case GroovePreset::Swing16_55:
        return "16th Note Swing (55%)";
    case GroovePreset::Swing16_62:
        return "16th Note Swing (62%)";
    case GroovePreset::Swing16_68:
        return "16th Note Swing (68%)";
    case GroovePreset::Shuffle8_60:
        return "8th Note Shuffle (60%)";
    case GroovePreset::Shuffle8_67:
        return "8th Note Shuffle (67%)";
    case GroovePreset::Mpc58:
        return "MPC 16th (58%)";
    case GroovePreset::Mpc64:
        return "MPC 16th (64%)";
    case GroovePreset::TripletFeel:
        return "Triplet Feel (66%)";
    case GroovePreset::Count:
        break;
    }
    return "Straight (Off)";
}

double groovePresetSwing(GroovePreset preset) noexcept
{
    switch (preset)
    {
    case GroovePreset::Straight:
        return 0.0;
    case GroovePreset::Swing16_55:
        return 0.20;
    case GroovePreset::Swing16_62:
        return 0.48;
    case GroovePreset::Swing16_68:
        return 0.72;
    case GroovePreset::Shuffle8_60:
        return 0.40;
    case GroovePreset::Shuffle8_67:
        return 0.67;
    case GroovePreset::Mpc58:
        return 0.32;
    case GroovePreset::Mpc64:
        return 0.56;
    case GroovePreset::TripletFeel:
        return 0.66;
    case GroovePreset::Count:
        break;
    }
    return 0.0;
}

bool groovePresetIsEighthNote(GroovePreset preset) noexcept
{
    return preset == GroovePreset::Shuffle8_60 || preset == GroovePreset::Shuffle8_67;
}

Timing::Timing(double bpm, double sampleRate, double swing, bool eighthNoteGroove)
    : bpm_(bpm), sampleRate_(sampleRate), swing_(std::clamp(swing, 0.0, 1.0)), eighthNoteGroove_(eighthNoteGroove)
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

    // 16th-note pair = 4 steps (half = 2), 8th-note pair = 8 steps (half = 4).
    const double pairLen = eighthNoteGroove_ ? 8.0 : 4.0;
    const double halfLen = pairLen * 0.5;
    const double maxShift = halfLen * 0.5; // up to 1.0 step (16th) or 2.0 steps (8th)
    const double shift = swing_ * maxShift;

    const double pairBase = std::floor(step / pairLen) * pairLen;
    const double sub = step - pairBase;

    if (sub < halfLen)
    {
        return pairBase + sub * (halfLen + shift) / halfLen;
    }
    else
    {
        return pairBase + (halfLen + shift) + (sub - halfLen) * (halfLen - shift) / halfLen;
    }
}

double Timing::swungStepToStep(double swungStep) const noexcept
{
    if (swing_ <= 0.0)
        return swungStep;

    const double pairLen = eighthNoteGroove_ ? 8.0 : 4.0;
    const double halfLen = pairLen * 0.5;
    const double maxShift = halfLen * 0.5;
    const double shift = swing_ * maxShift;

    const double pairBase = std::floor(swungStep / pairLen) * pairLen;
    const double sub = swungStep - pairBase;
    const double splitPoint = halfLen + shift;

    if (sub < splitPoint)
    {
        return pairBase + sub * halfLen / splitPoint;
    }
    else
    {
        const double rem = pairLen - splitPoint;
        if (rem <= 0.0001)
            return pairBase + halfLen;
        return pairBase + halfLen + (sub - splitPoint) * halfLen / rem;
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
