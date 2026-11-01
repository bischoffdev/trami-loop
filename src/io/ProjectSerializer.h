#pragma once

#include "engine/StretchCache.h"
#include "io/SampleImporter.h"
#include "model/Song.h"

#include <filesystem>
#include <optional>
#include <string>

namespace trami
{

class ProjectSerializer
{
public:
    /// Saves the song document and copies its audio samples into projectDir/samples/.
    static bool saveProject(const Song& song, const StretchCache& stretchCache,
                            const std::filesystem::path& projectDir);

    /// Loads a project from projectDir/project.trami and decodes audio files in projectDir/samples/.
    static std::optional<Song> loadProject(const std::filesystem::path& projectDir, StretchCache& stretchCache,
                                          SampleImporter& importer, double sampleRate);
};

} // namespace trami
