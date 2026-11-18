#pragma once

#include "engine/StretchCache.h"
#include "io/SampleImporter.h"
#include "model/Song.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace trami
{

struct AcidTimelineClip
{
    int startTicks = 0;
    int endTicks = 0;
    int sampleOffsetFrames = 0;
    int transposeSemitones = 0;
    float gain = 1.0f;
    std::vector<EnvelopePoint> volumeEnvelope;
};

struct AcidTrack
{
    int trackIndex = 0;
    std::string trackName;
    std::string samplePath;
    std::string sampleFileName;
    int trackType = 0;
    std::uint32_t totalSampleFrames = 0;
    bool hasAcidChunk = false;
    bool isOneShot = false;
    bool isDiskBased = false;
    int rootNote = -1;
    int beatCount = 0;
    int meterNumerator = 4;
    int meterDenominator = 4;
    float originalBpm = 0.0f;
    float volume = 1.0f;
    float pan = 0.0f;
    std::vector<double> stretchMarkers;
    std::vector<AcidTimelineClip> clips;
};

struct AcidTimelineEvent
{
    int trackIndex = 0;
    std::string sampleFileName;
    int startBar = 0;
    int lengthBars = 1;
    int transposeSemitones = 0;
};

class AcidProjectReader
{
public:
    /// Returns true if the file is an ACID project (.acd or .acd-zip) by extension or magic header.
    static bool isAcidProject(const std::filesystem::path& file);

    /// Loads an ACID project (.acd or .acd-zip), importing all referenced samples
    /// and reconstructing the song (BPM, tracks, clips, and arrangement).
    /// If outExtractedDir is non-null and the project is an .acd-zip, it receives the
    /// path where the project assets were extracted.
    static std::optional<Song> loadProject(const std::filesystem::path& projectFile,
                                           StretchCache& stretchCache,
                                           SampleImporter& importer,
                                           double sampleRate,
                                           std::filesystem::path* outExtractedDir = nullptr);

    /// Parses an uncompressed .acd / .acp project file from disk.
    /// mediaSearchDir is the directory where referenced audio files should be located.
    static std::optional<Song> parseAcdFile(const std::filesystem::path& acdFile,
                                            const std::filesystem::path& mediaSearchDir,
                                            StretchCache& stretchCache,
                                            SampleImporter& importer,
                                            double sampleRate,
                                            const std::filesystem::path& secondarySearchDir = {});
};

} // namespace trami
