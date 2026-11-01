#include "engine/TimeStretcher.h"

#include <rubberband/RubberBandStretcher.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace trami
{

AudioData TimeStretcher::stretch(const AudioData& input, double timeRatio, double sampleRate)
{
    if (input.numFrames() == 0 || input.numChannels() == 0)
        return input;

    if (timeRatio <= 0.0 || std::abs(timeRatio - 1.0) < 0.0005)
        return input;

    const int numChannels = input.numChannels();
    const std::size_t totalInFrames = static_cast<std::size_t>(input.numFrames());

    using Stretcher = RubberBand::RubberBandStretcher;
    const int options = Stretcher::OptionProcessOffline | Stretcher::OptionThreadingNever |
                        Stretcher::OptionTransientsCrisp;

    Stretcher stretcher(static_cast<size_t>(sampleRate), static_cast<size_t>(numChannels), options, timeRatio, 1.0);

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

} // namespace trami
