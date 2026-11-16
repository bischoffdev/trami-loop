#include "engine/TimeStretcher.h"

#include <rubberband/RubberBandStretcher.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace trami
{

AudioData TimeStretcher::stretch(const AudioData& input, double timeRatio, double sampleRate, double pitchScale)
{
    if (input.numFrames() == 0 || input.numChannels() == 0)
        return input;

    const bool noTimeChange = (timeRatio <= 0.0 || std::abs(timeRatio - 1.0) < 0.0005);
    const bool noPitchChange = (pitchScale <= 0.0 || std::abs(pitchScale - 1.0) < 0.0005);

    if (noTimeChange && noPitchChange)
        return input;

    const double effTimeRatio = (timeRatio <= 0.0) ? 1.0 : timeRatio;
    const double effPitchScale = (pitchScale <= 0.0) ? 1.0 : pitchScale;

    const int numChannels = input.numChannels();
    const std::size_t totalInFrames = static_cast<std::size_t>(input.numFrames());

    using Stretcher = RubberBand::RubberBandStretcher;
    const int options = Stretcher::OptionProcessOffline | Stretcher::OptionThreadingNever |
                        Stretcher::OptionTransientsCrisp;

    Stretcher stretcher(static_cast<size_t>(sampleRate), static_cast<size_t>(numChannels), options, effTimeRatio, effPitchScale);

    // Prepare channel pointers
    std::vector<const float*> inPtrs(static_cast<std::size_t>(numChannels));
    for (int ch = 0; ch < numChannels; ++ch)
        inPtrs[static_cast<std::size_t>(ch)] = input.channels[static_cast<std::size_t>(ch)].data();

    // 1. Study
    stretcher.study(inPtrs.data(), totalInFrames, true);

    // 2. Process & Retrieve
    AudioData output;
    output.channels.resize(static_cast<std::size_t>(numChannels));

    std::size_t inPos = 0;
    std::vector<const float*> chunkPtrs(static_cast<std::size_t>(numChannels));

    // Temporary retrieval buffer
    constexpr std::size_t kRetrieveBlock = 1024;
    std::vector<std::vector<float>> retrieveBuf(static_cast<std::size_t>(numChannels),
                                                std::vector<float>(kRetrieveBlock));
    std::vector<float*> outPtrs(static_cast<std::size_t>(numChannels));
    for (int ch = 0; ch < numChannels; ++ch)
        outPtrs[static_cast<std::size_t>(ch)] = retrieveBuf[static_cast<std::size_t>(ch)].data();

    auto drainAvailable = [&]() {
        while (true)
        {
            const int avail = stretcher.available();
            if (avail <= 0)
                break;

            const std::size_t toRead = std::min(static_cast<std::size_t>(avail), kRetrieveBlock);
            const std::size_t got = stretcher.retrieve(outPtrs.data(), toRead);
            if (got == 0)
                break;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& chVec = output.channels[static_cast<std::size_t>(ch)];
                const auto& buf = retrieveBuf[static_cast<std::size_t>(ch)];
                chVec.insert(chVec.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(got));
            }
        }
    };

    while (inPos < totalInFrames)
    {
        std::size_t req = stretcher.getSamplesRequired();
        if (req == 0)
            req = 1024;

        const std::size_t chunk = std::min(req, totalInFrames - inPos);
        for (int ch = 0; ch < numChannels; ++ch)
            chunkPtrs[static_cast<std::size_t>(ch)] = inPtrs[static_cast<std::size_t>(ch)] + inPos;

        const bool isFinal = (inPos + chunk >= totalInFrames);
        stretcher.process(chunkPtrs.data(), chunk, isFinal);
        inPos += chunk;

        drainAvailable();
    }

    // Drain any remaining output frames
    drainAvailable();

    return output;
}

AudioData TimeStretcher::resample(const AudioData& input, double pitchScale)
{
    if (input.numFrames() == 0 || input.numChannels() == 0)
        return input;

    if (pitchScale <= 0.0 || std::abs(pitchScale - 1.0) < 0.0005)
        return input;

    const int numChannels = input.numChannels();
    const std::int64_t totalInFrames = input.numFrames();
    const std::int64_t destLength = std::max<std::int64_t>(
        1, static_cast<std::int64_t>(std::round(static_cast<double>(totalInFrames) / pitchScale)));

    AudioData output;
    output.channels.resize(static_cast<std::size_t>(numChannels));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto& inCh = input.channels[static_cast<std::size_t>(ch)];
        auto& outCh = output.channels[static_cast<std::size_t>(ch)];
        outCh.resize(static_cast<std::size_t>(destLength));

        auto sampleAt = [&](std::int64_t idx) -> float {
            if (idx < 0 || idx >= totalInFrames)
                return 0.0f;
            return inCh[static_cast<std::size_t>(idx)];
        };

        for (std::int64_t outIdx = 0; outIdx < destLength; ++outIdx)
        {
            const double inPos = static_cast<double>(outIdx) * pitchScale;
            const auto baseIdx = static_cast<std::int64_t>(std::floor(inPos));
            const float d = static_cast<float>(inPos - static_cast<double>(baseIdx));

            // 6-point (5th-order) Lagrange polynomial interpolation
            const float ym2 = sampleAt(baseIdx - 2);
            const float ym1 = sampleAt(baseIdx - 1);
            const float y0  = sampleAt(baseIdx);
            const float y1  = sampleAt(baseIdx + 1);
            const float y2  = sampleAt(baseIdx + 2);
            const float y3  = sampleAt(baseIdx + 3);

            const float d1  = d - 1.0f;
            const float d2  = d - 2.0f;
            const float d3  = d - 3.0f;
            const float dp1 = d + 1.0f;
            const float dp2 = d + 2.0f;

            const float c0 = -0.0083333333333333f * d * d1 * d2 * d3 * dp1;
            const float c1 =  0.0416666666666667f * d * d1 * d2 * d3 * dp2;
            const float c2 = -0.0833333333333333f * dp1 * d1 * d2 * d3 * dp2;
            const float c3 =  0.0833333333333333f * dp1 * d * d2 * d3 * dp2;
            const float c4 = -0.0416666666666667f * dp1 * d * d1 * d3 * dp2;
            const float c5 =  0.0083333333333333f * dp1 * d * d1 * d2 * dp2;

            outCh[static_cast<std::size_t>(outIdx)] = c0 * ym2 + c1 * ym1 + c2 * y0 + c3 * y1 + c4 * y2 + c5 * y3;
        }
    }

    return output;
}

} // namespace trami
