#pragma once

#include "engine/StretchCache.h"
#include "model/Song.h"

#include <filesystem>
#include <functional>

namespace trami
{

struct ExportOptions
{
    double sampleRate = 44100.0;
    int bitsPerSample = 16;
};

class WavExporter
{
public:
    using ProgressCallback = std::function<void(float progress0to1)>;

    /// Renders the complete song arrangement offline to a WAV file.
    static bool exportMixdown(const Song& song, StretchCache& stretchCache, const ExportOptions& options,
                              const std::filesystem::path& outputFile,
                              ProgressCallback progress = nullptr);
};

} // namespace trami
