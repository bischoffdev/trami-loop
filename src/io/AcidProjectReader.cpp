#include "io/AcidProjectReader.h"
#include "io/AcidChunkReader.h"
#include "model/Timing.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_set>

namespace trami
{

namespace
{

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool hasAudioExtension(const std::string& path)
{
    const auto lower = toLower(path);
    return lower.ends_with(".wav") || lower.ends_with(".mp3") || lower.ends_with(".aif") ||
           lower.ends_with(".aiff") || lower.ends_with(".ogg") || lower.ends_with(".flac");
}

bool isValidPathChar(char c)
{
    return c >= 0x20 && c != '"' && c != '<' && c != '>' && c != '|' && c != '?' && c != '*' && c != '\r' && c != '\n';
}

std::filesystem::path findFileCaseInsensitive(const std::filesystem::path& dir, const std::string& targetName)
{
    const auto lowerTarget = toLower(targetName);
    std::error_code ec;

    if (!std::filesystem::exists(dir, ec))
        return {};

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec))
    {
        if (entry.is_regular_file(ec))
        {
            if (toLower(entry.path().filename().string()) == lowerTarget)
                return entry.path();
        }
    }
    return {};
}

std::filesystem::path findFileStemCaseInsensitive(const std::filesystem::path& dir, const std::string& stem)
{
    const auto lowerStem = toLower(stem);
    std::error_code ec;

    if (!std::filesystem::exists(dir, ec))
        return {};

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec))
    {
        if (entry.is_regular_file(ec) && hasAudioExtension(entry.path().string()))
        {
            if (toLower(entry.path().stem().string()) == lowerStem)
                return entry.path();
        }
    }
    return {};
}

struct RiffChunk
{
    std::string id;        ///< 4 chars (e.g. "RIFF", "LIST", "tmap", "trkh", "acid", "rgnh")
    std::string subType;   ///< 4 chars if id is "RIFF" or "LIST" (e.g. "ACID", "tlst", "trak", "rlst", "rgn ")
    std::size_t offset = 0;///< byte offset of payload in buffer
    std::size_t size = 0;  ///< payload size in bytes
};

std::vector<RiffChunk> findChunks(const std::vector<std::uint8_t>& buf, std::size_t start, std::size_t end)
{
    std::vector<RiffChunk> chunks;
    std::size_t cur = start;
    while (cur + 8 <= end && cur + 8 <= buf.size())
    {
        RiffChunk c;
        c.id = std::string(reinterpret_cast<const char*>(buf.data() + cur), 4);
        std::uint32_t len = 0;
        std::memcpy(&len, buf.data() + cur + 4, 4);
        cur += 8;

        c.offset = cur;
        c.size = std::min(static_cast<std::size_t>(len), end >= cur ? end - cur : 0);

        if ((c.id == "LIST" || c.id == "RIFF") && c.size >= 4)
        {
            c.subType = std::string(reinterpret_cast<const char*>(buf.data() + cur), 4);
        }

        chunks.push_back(c);
        cur += (static_cast<std::size_t>(len) + 1) & ~static_cast<std::size_t>(1);
    }
    return chunks;
}

std::vector<std::string> extractTrkhStrings(const std::uint8_t* payload, std::size_t size)
{
    std::vector<std::string> result;
    if (size < 4)
        return result;

    std::uint32_t hdrSize = 0;
    std::memcpy(&hdrSize, payload, 4);
    if (hdrSize < 36 || hdrSize >= size)
        hdrSize = 52; // standard ACID track header size

    if (hdrSize < size)
    {
        const std::uint8_t* ptr = payload + hdrSize;
        const std::uint8_t* end = payload + size;

        while (ptr + 2 <= end)
        {
            std::u16string u16;
            while (ptr + 2 <= end)
            {
                char16_t ch = 0;
                std::memcpy(&ch, ptr, 2);
                ptr += 2;
                if (ch == 0)
                    break;
                u16.push_back(ch);
            }
            if (!u16.empty())
            {
                juce::String jstr(juce::CharPointer_UTF16(reinterpret_cast<const juce::CharPointer_UTF16::CharType*>(u16.c_str())));
                result.push_back(jstr.toStdString());
            }
        }
    }

    // Fallback: If no UTF-16 strings were found, scan for ASCII audio file names
    if (result.empty())
    {
        for (std::size_t i = 0; i + 4 <= size; ++i)
        {
            const char* cptr = reinterpret_cast<const char*>(payload + i);
            if (std::memcmp(cptr, ".wav", 4) == 0 || std::memcmp(cptr, ".WAV", 4) == 0 ||
                std::memcmp(cptr, ".mp3", 4) == 0 || std::memcmp(cptr, ".MP3", 4) == 0 ||
                std::memcmp(cptr, ".aif", 4) == 0 || std::memcmp(cptr, ".ogg", 4) == 0)
            {
                std::size_t s = i;
                while (s > 0 && isValidPathChar(static_cast<char>(payload[s - 1])))
                    --s;
                result.push_back(std::string(reinterpret_cast<const char*>(payload + s), (i + 4) - s));
                break;
            }
        }
    }

    return result;
}

struct ParsedAcidProject
{
    double bpm = 120.0;
    bool hasExplicitTempo = false;
    int rootNote = -1;
    std::vector<AcidTrack> tracks;
};

ParsedAcidProject parseRiffAcid(const std::vector<std::uint8_t>& buffer)
{
    ParsedAcidProject proj;
    if (buffer.size() < 12)
        return proj;

    if (std::memcmp(buffer.data(), "RIFF", 4) != 0)
        return proj;

    const auto rootChunks = findChunks(buffer, 0, buffer.size());
    if (rootChunks.empty())
        return proj;

    const auto& root = rootChunks[0];
    if (root.subType != "ACID" && root.subType != "ACD " && root.subType != "sfba")
        return proj;

    const auto subChunks = findChunks(buffer, root.offset + 4, root.offset + root.size);

    // 1. Parse tmap chunk (tempo map)
    for (const auto& c : subChunks)
    {
        if (c.id == "tmap" && c.size >= 12)
        {
            const std::uint8_t* p = buffer.data() + c.offset;
            std::uint32_t usPerBeat = 0;
            std::memcpy(&usPerBeat, p + 8, 4);
            if (usPerBeat >= 100000 && usPerBeat <= 10000000) // 6 to 600 BPM
            {
                proj.bpm = 60000000.0 / static_cast<double>(usPerBeat);
                proj.hasExplicitTempo = true;
            }
            if (c.size >= 16)
            {
                std::uint32_t rNote = 0;
                std::memcpy(&rNote, p + 12, 4);
                if (rNote <= 127)
                    proj.rootNote = static_cast<int>(rNote);
            }
            break;
        }
    }

    // 2. Parse tlst chunk (track list)
    for (const auto& c : subChunks)
    {
        if (c.id == "LIST" && c.subType == "tlst")
        {
            const auto traks = findChunks(buffer, c.offset + 4, c.offset + c.size);
            int trackIdx = 0;
            for (const auto& trak : traks)
            {
                if (trak.id != "LIST" || trak.subType != "trak")
                    continue;

                const auto trakSub = findChunks(buffer, trak.offset + 4, trak.offset + trak.size);
                AcidTrack acidTrack;
                acidTrack.trackIndex = trackIdx++;

                for (const auto& tc : trakSub)
                {
                    if (tc.id == "trkh")
                    {
                        const auto strings = extractTrkhStrings(buffer.data() + tc.offset, tc.size);
                        if (!strings.empty())
                        {
                            acidTrack.samplePath = strings[0];
                            acidTrack.sampleFileName = std::filesystem::path(strings[0]).filename().string();
                            if (strings.size() >= 2 && !strings[1].empty())
                                acidTrack.trackName = strings[1];
                            else
                                acidTrack.trackName = std::filesystem::path(acidTrack.sampleFileName).stem().string();
                        }
                    }
                    else if (tc.id == "acid" && tc.size >= 24)
                    {
                        const std::uint8_t* p = buffer.data() + tc.offset;
                        std::uint32_t flags = 0;
                        std::memcpy(&flags, p + 0, 4);
                        std::uint16_t rootNote = 0;
                        std::memcpy(&rootNote, p + 4, 2);
                        std::uint32_t beats = 4;
                        std::memcpy(&beats, p + 12, 4);
                        float tempo = 120.0f;
                        std::memcpy(&tempo, p + 20, 4);

                        acidTrack.isOneShot = (flags & 0x01) != 0;
                        acidTrack.rootNote = (flags & 0x02) ? static_cast<int>(rootNote) : -1;
                        acidTrack.beatCount = (beats > 0) ? beats : 4;
                        acidTrack.originalBpm = tempo;
                    }
                    else if (tc.id == "LIST" && tc.subType == "rlst")
                    {
                        const auto rgns = findChunks(buffer, tc.offset + 4, tc.offset + tc.size);
                        for (const auto& rgn : rgns)
                        {
                            if (rgn.id != "LIST" || rgn.subType != "rgn ")
                                continue;

                            const auto rgnSub = findChunks(buffer, rgn.offset + 4, rgn.offset + rgn.size);
                            for (const auto& rc : rgnSub)
                            {
                                if (rc.id == "rgnh" && rc.size >= 12)
                                {
                                    const std::uint8_t* p = buffer.data() + rc.offset;
                                    std::int32_t startTicks = 0;
                                    std::memcpy(&startTicks, p + 4, 4);
                                    std::int32_t endTicks = 0;
                                    std::memcpy(&endTicks, p + 8, 4);
                                    std::int32_t sampleOffset = 0;
                                    if (rc.size >= 20)
                                        std::memcpy(&sampleOffset, p + 16, 4);
                                    std::int32_t transpose = 0;
                                    if (rc.size >= 32)
                                        std::memcpy(&transpose, p + 28, 4);

                                    if (endTicks > startTicks && startTicks >= 0)
                                    {
                                        AcidTimelineClip clip;
                                        clip.startTicks = startTicks;
                                        clip.endTicks = endTicks;
                                        clip.sampleOffsetTicks = std::max(0, sampleOffset);
                                        clip.transposeSemitones = std::clamp(transpose, -48, 48);
                                        acidTrack.clips.push_back(clip);
                                    }
                                }
                            }
                        }
                    }
                }

                proj.tracks.push_back(std::move(acidTrack));
            }
            break;
        }
    }

    return proj;
}

} // namespace

bool AcidProjectReader::isAcidProject(const std::filesystem::path& file)
{
    const auto ext = toLower(file.extension().string());
    if (ext == ".acd" || ext == ".acd-zip" || ext == ".acd-bak")
        return true;

    std::ifstream in(file, std::ios::binary);
    if (!in.is_open())
        return false;

    char header[12] = {0};
    if (!in.read(header, 12))
        return false;

    // ZIP archive
    if (std::memcmp(header, "PK\x03\x04", 4) == 0)
        return true;

    // RIFF container with ACID form type
    if (std::memcmp(header, "RIFF", 4) == 0)
    {
        if (std::memcmp(header + 8, "ACID", 4) == 0 || std::memcmp(header + 8, "ACD ", 4) == 0 ||
            std::memcmp(header + 8, "sfba", 4) == 0)
            return true;
    }

    return false;
}

std::optional<Song> AcidProjectReader::loadProject(const std::filesystem::path& projectFile,
                                                   StretchCache& stretchCache,
                                                   SampleImporter& importer,
                                                   double sampleRate,
                                                   std::filesystem::path* outExtractedDir)
{
    std::error_code ec;
    if (!std::filesystem::exists(projectFile, ec))
        return std::nullopt;

    bool isZip = (toLower(projectFile.extension().string()) == ".acd-zip");
    if (!isZip)
    {
        std::ifstream testStream(projectFile, std::ios::binary);
        char magic[4] = {0};
        if (testStream.read(magic, 4) && std::memcmp(magic, "PK\x03\x04", 4) == 0)
            isZip = true;
    }

    if (isZip)
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "trami_acid_projects" / projectFile.stem();
        std::filesystem::create_directories(tempRoot, ec);

        const juce::File jArchive(juce::String::fromUTF8(projectFile.string().c_str()));
        juce::ZipFile zip(jArchive);

        const juce::File jDest(juce::String::fromUTF8(tempRoot.string().c_str()));
        const auto uncompressResult = zip.uncompressTo(jDest);
        if (uncompressResult.failed())
            return std::nullopt;

        if (outExtractedDir != nullptr)
            *outExtractedDir = tempRoot;

        std::filesystem::path acdPath;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(tempRoot, ec))
        {
            if (entry.is_regular_file(ec) && toLower(entry.path().extension().string()) == ".acd")
            {
                acdPath = entry.path();
                break;
            }
        }

        if (!acdPath.empty())
            return parseAcdFile(acdPath, tempRoot, stretchCache, importer, sampleRate);

        return parseAcdFile(projectFile, tempRoot, stretchCache, importer, sampleRate);
    }

    return parseAcdFile(projectFile, projectFile.parent_path(), stretchCache, importer, sampleRate);
}

std::optional<Song> AcidProjectReader::parseAcdFile(const std::filesystem::path& acdFile,
                                                    const std::filesystem::path& mediaSearchDir,
                                                    StretchCache& stretchCache,
                                                    SampleImporter& importer,
                                                    double sampleRate)
{
    std::vector<std::uint8_t> buffer;
    {
        std::ifstream in(acdFile, std::ios::binary | std::ios::ate);
        if (in.is_open())
        {
            const auto sz = in.tellg();
            if (sz > 0 && sz < 100 * 1024 * 1024) // limit to 100MB
            {
                buffer.resize(static_cast<std::size_t>(sz));
                in.seekg(0, std::ios::beg);
                in.read(reinterpret_cast<char*>(buffer.data()), sz);
            }
        }
    }

    if (buffer.empty())
        return std::nullopt;

    // 1. Try parsing full RIFF ACID project structure
    auto riffProj = parseRiffAcid(buffer);

    // If RIFF structure had tracks with clips or samples, use the authentic arrangement
    if (!riffProj.tracks.empty())
    {
        std::unordered_set<std::string> loadedFileNames;
        std::error_code ec;

        // Calculate song length in bars from max clip end tick (384 ticks/beat, 1536 ticks/bar)
        int maxEndTicks = 0;
        for (const auto& t : riffProj.tracks)
        {
            for (const auto& c : t.clips)
                maxEndTicks = std::max(maxEndTicks, c.endTicks);
        }

        int arrangementBars = (maxEndTicks > 0) ? ((maxEndTicks + 1535) / 1536) : 64;
        arrangementBars = std::clamp(arrangementBars, 1, 9999);

        // Adopt loop tempo if project tempo was not explicitly given in tmap
        if (!riffProj.hasExplicitTempo)
        {
            for (const auto& t : riffProj.tracks)
            {
                if (t.originalBpm > 20.0f)
                {
                    riffProj.bpm = t.originalBpm;
                    break;
                }
            }
        }

        Song song;
        song.setBpm(riffProj.bpm);
        song.setMasterVolume(1.0f);

        const std::string patternName = acdFile.stem().string().empty() ? "ACID Arrangement" : acdFile.stem().string();
        const auto patternId = song.addPattern(patternName, arrangementBars);

        int numPatternTracks = 0;

        for (const auto& track : riffProj.tracks)
        {
            // Skip tracks that have neither clips nor an identifiable sample name
            if (track.clips.empty() && track.sampleFileName.empty())
                continue;

            if (numPatternTracks >= kTracksPerPattern)
                break;

            // Resolve sample audio file
            std::filesystem::path resolved;

            if (!track.samplePath.empty() && std::filesystem::exists(track.samplePath, ec))
                resolved = track.samplePath;

            if (resolved.empty() && !track.sampleFileName.empty())
            {
                const auto cand = mediaSearchDir / track.sampleFileName;
                if (std::filesystem::exists(cand, ec))
                    resolved = cand;
            }

            if (resolved.empty() && !track.sampleFileName.empty())
            {
                const auto cand = mediaSearchDir / "samples" / track.sampleFileName;
                if (std::filesystem::exists(cand, ec))
                    resolved = cand;
            }

            if (resolved.empty() && !track.sampleFileName.empty())
                resolved = findFileCaseInsensitive(mediaSearchDir, track.sampleFileName);

            if (resolved.empty() && !track.sampleFileName.empty())
                resolved = findFileStemCaseInsensitive(mediaSearchDir, std::filesystem::path(track.sampleFileName).stem().string());

            SampleId sampleId;

            if (!resolved.empty())
            {
                const int loopBarsHint = std::max(1, static_cast<int>(track.beatCount / 4));
                auto importRes = importer.importFile(resolved, sampleRate,
                                                     track.isOneShot ? SampleKind::OneShot : SampleKind::Loop,
                                                     loopBarsHint);
                if (importRes.has_value())
                {
                    Sample s = importRes->sample;
                    s.kind = track.isOneShot ? SampleKind::OneShot : SampleKind::Loop;
                    if (track.beatCount > 0)
                        s.loopBars = std::max(1, static_cast<int>(track.beatCount / 4));
                    if (track.originalBpm > 20.0f)
                        s.acidBpm = track.originalBpm;
                    if (track.rootNote >= 0)
                        s.rootNote = track.rootNote;

                    sampleId = song.addSample(s);
                    stretchCache.setRawAudio(sampleId, std::make_shared<AudioData>(std::move(importRes->audio)));
                    loadedFileNames.insert(toLower(resolved.filename().string()));
                }
            }

            // If audio file could not be found (e.g. external CD), create placeholder so track & clips are preserved
            if (!sampleId.isValid())
            {
                Sample s;
                s.name = !track.trackName.empty() ? track.trackName : track.sampleFileName;
                s.fileName = !track.sampleFileName.empty() ? track.sampleFileName : (s.name + ".wav");
                s.kind = track.isOneShot ? SampleKind::OneShot : SampleKind::Loop;
                s.loopBars = std::max(1, static_cast<int>(track.beatCount / 4));
                s.acidBpm = (track.originalBpm > 20.0f) ? track.originalBpm : riffProj.bpm;
                s.rootNote = track.rootNote;

                const int numFrames = static_cast<int>(std::round(sampleRate * (60.0 / riffProj.bpm) * 4.0 * s.loopBars));
                auto silent = std::make_shared<AudioData>();
                silent->channels.resize(2, std::vector<float>(static_cast<std::size_t>(std::max(1024, numFrames)), 0.0f));

                sampleId = song.addSample(s);
                stretchCache.setRawAudio(sampleId, silent);
            }

            const int patternTrackIdx = (numPatternTracks == 0) ? 0 : song.addTrackToPattern(patternId, sampleId);
            if (patternTrackIdx >= 0)
            {
                numPatternTracks++;
                if (patternTrackIdx == 0)
                    song.setPatternTrackSample(patternId, 0, sampleId);

                const std::string displayName = !track.trackName.empty() ? track.trackName : track.sampleFileName;
                song.renamePatternTrack(patternId, patternTrackIdx, displayName);
                song.setPatternTrackKind(patternId, patternTrackIdx, track.isOneShot ? SampleKind::OneShot : SampleKind::Loop);

                const double totalSteps = static_cast<double>(arrangementBars * kStepsPerBar);

                // Place exact clips from ACID region list
                for (const auto& clip : track.clips)
                {
                    const double startStep = static_cast<double>(clip.startTicks) / 48.0;
                    double lengthSteps = static_cast<double>(clip.endTicks - clip.startTicks) / 48.0;
                    const double offsetSteps = static_cast<double>(clip.sampleOffsetTicks) / 48.0;

                    const double maxAllowed = totalSteps - startStep;
                    lengthSteps = std::min(lengthSteps, maxAllowed);

                    if (startStep >= 0.0 && startStep < totalSteps && lengthSteps > 0.0)
                    {
                        auto placedClipId = song.placeClip(patternId, patternTrackIdx, sampleId, startStep, lengthSteps, offsetSteps);
                        if (placedClipId.has_value() && clip.transposeSemitones != 0)
                        {
                            song.setClipTranspose(patternId, *placedClipId, clip.transposeSemitones);
                        }
                    }
                }
            }
        }

        // Also import any additional audio files present in mediaSearchDir into the project's sample pool
        if (std::filesystem::exists(mediaSearchDir, ec))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(mediaSearchDir, std::filesystem::directory_options::skip_permission_denied, ec))
            {
                if (entry.is_regular_file(ec) && hasAudioExtension(entry.path().string()))
                {
                    const auto fname = entry.path().filename().string();
                    if (!loadedFileNames.contains(toLower(fname)))
                    {
                        auto importRes = importer.importFile(entry.path(), sampleRate, SampleKind::Loop, 1);
                        if (importRes.has_value())
                        {
                            const auto extraId = song.addSample(importRes->sample);
                            stretchCache.setRawAudio(extraId, std::make_shared<AudioData>(std::move(importRes->audio)));
                            loadedFileNames.insert(toLower(fname));
                        }
                    }
                }
            }
        }

        // Assign arrangement
        song.setArrangerTrackPattern(0, patternId);
        song.arrangerTrack(0)->name = patternName;
        song.placeBlock(0, patternId, 0);

        return song;
    }

    // -------------------------------------------------------------------------
    // Fallback Heuristic Scanner (for files without standard RIFF 'tlst' chunks)
    // -------------------------------------------------------------------------
    double projectBpm = 120.0;
    bool foundExplicitTempo = false;

    if (buffer.size() >= 8)
    {
        for (std::size_t i = 0; i + 4 <= buffer.size(); i += 2)
        {
            float f = 0.0f;
            std::memcpy(&f, buffer.data() + i, 4);
            if (f >= 50.0f && f <= 260.0f)
            {
                bool looksLikeTempo = false;
                if (i >= 4)
                {
                    if (std::memcmp(buffer.data() + i - 4, "tmpo", 4) == 0 ||
                        std::memcmp(buffer.data() + i - 4, "TMPO", 4) == 0 ||
                        std::memcmp(buffer.data() + i - 4, "BPM ", 4) == 0)
                    {
                        looksLikeTempo = true;
                    }
                }

                if (looksLikeTempo)
                {
                    projectBpm = static_cast<double>(f);
                    foundExplicitTempo = true;
                    break;
                }
            }
        }
    }

    std::vector<std::string> referencedFiles;
    std::unordered_set<std::string> seenBasenames;

    for (std::size_t i = 0; i < buffer.size(); ++i)
    {
        if (i + 4 <= buffer.size())
        {
            const char* ptr = reinterpret_cast<const char*>(buffer.data() + i);
            if (std::memcmp(ptr, ".wav", 4) == 0 || std::memcmp(ptr, ".WAV", 4) == 0 ||
                std::memcmp(ptr, ".mp3", 4) == 0 || std::memcmp(ptr, ".MP3", 4) == 0 ||
                std::memcmp(ptr, ".aif", 4) == 0 || std::memcmp(ptr, ".ogg", 4) == 0)
            {
                std::size_t start = i;
                while (start > 0 && isValidPathChar(static_cast<char>(buffer[start - 1])))
                {
                    --start;
                    if (i - start > 260)
                        break;
                }

                std::size_t end = i + 4;
                if (end < buffer.size() && (std::memcmp(ptr, ".aif", 4) == 0) &&
                    (buffer[end] == 'f' || buffer[end] == 'F'))
                {
                    ++end;
                }

                const std::string foundStr(reinterpret_cast<const char*>(buffer.data() + start), end - start);
                if (foundStr.length() >= 4 && hasAudioExtension(foundStr))
                {
                    const auto base = std::filesystem::path(foundStr).filename().string();
                    if (!base.empty() && !seenBasenames.contains(toLower(base)))
                    {
                        seenBasenames.insert(toLower(base));
                        referencedFiles.push_back(base);
                    }
                }
            }
        }

        if (i + 8 <= buffer.size())
        {
            const auto* b = buffer.data() + i;
            if (b[0] == '.' && b[1] == 0 && (b[2] == 'w' || b[2] == 'W') && b[3] == 0 &&
                (b[4] == 'a' || b[4] == 'A') && b[5] == 0 && (b[6] == 'v' || b[6] == 'V') && b[7] == 0)
            {
                std::size_t start = i;
                while (start >= 2 && buffer[start - 1] == 0 && isValidPathChar(static_cast<char>(buffer[start - 2])))
                {
                    start -= 2;
                    if (i - start > 520)
                        break;
                }

                std::string utf8Str;
                for (std::size_t u = start; u < i + 8; u += 2)
                    utf8Str.push_back(static_cast<char>(buffer[u]));

                if (hasAudioExtension(utf8Str))
                {
                    const auto base = std::filesystem::path(utf8Str).filename().string();
                    if (!base.empty() && !seenBasenames.contains(toLower(base)))
                    {
                        seenBasenames.insert(toLower(base));
                        referencedFiles.push_back(base);
                    }
                }
            }
        }
    }

    std::error_code ec;
    if (std::filesystem::exists(mediaSearchDir, ec))
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(mediaSearchDir, std::filesystem::directory_options::skip_permission_denied, ec))
        {
            if (entry.is_regular_file(ec) && hasAudioExtension(entry.path().string()))
            {
                const auto base = entry.path().filename().string();
                if (!seenBasenames.contains(toLower(base)))
                {
                    seenBasenames.insert(toLower(base));
                    referencedFiles.push_back(base);
                }
            }
        }
    }

    if (referencedFiles.empty())
        return std::nullopt;

    struct LoadedSample
    {
        std::string name;
        int loopBars = 1;
        double tempo = 0.0;
        int rootNote = -1;
        std::shared_ptr<const AudioData> audio;
    };
    std::vector<LoadedSample> loadedSamples;

    for (const auto& fileName : referencedFiles)
    {
        std::filesystem::path resolved;

        const auto candidateDirect = mediaSearchDir / fileName;
        if (std::filesystem::exists(candidateDirect, ec))
            resolved = candidateDirect;

        if (resolved.empty())
        {
            const auto candidateSamples = mediaSearchDir / "samples" / fileName;
            if (std::filesystem::exists(candidateSamples, ec))
                resolved = candidateSamples;
        }

        if (resolved.empty())
            resolved = findFileCaseInsensitive(mediaSearchDir, fileName);

        if (resolved.empty() && std::filesystem::exists(fileName, ec))
            resolved = fileName;

        if (resolved.empty())
            continue;

        auto importRes = importer.importFile(resolved, sampleRate, SampleKind::Loop, 1);
        if (!importRes.has_value())
            continue;

        LoadedSample ls;
        ls.name = importRes->sample.name;
        ls.loopBars = importRes->sample.loopBars;
        ls.tempo = importRes->sample.acidBpm;
        ls.rootNote = importRes->sample.rootNote;
        ls.audio = std::make_shared<const AudioData>(std::move(importRes->audio));

        if (!foundExplicitTempo && ls.tempo > 20.0)
            projectBpm = ls.tempo;

        loadedSamples.push_back(std::move(ls));
    }

    if (loadedSamples.empty())
        return std::nullopt;

    Song song;
    song.setBpm(projectBpm);
    song.setMasterVolume(1.0f);

    std::vector<SampleId> songSampleIds;
    int maxLoopBars = 1;
    for (std::size_t i = 0; i < loadedSamples.size(); ++i)
    {
        Sample s;
        s.name = loadedSamples[i].name;
        s.fileName = loadedSamples[i].name + ".wav";
        s.kind = SampleKind::Loop;
        s.loopBars = loadedSamples[i].loopBars;
        s.acidBpm = loadedSamples[i].tempo;
        s.rootNote = loadedSamples[i].rootNote;

        maxLoopBars = std::max(maxLoopBars, s.loopBars);

        const auto realId = song.addSample(s);
        songSampleIds.push_back(realId);
        stretchCache.setRawAudio(realId, loadedSamples[i].audio);
    }

    const int arrangementBars = std::max(64, maxLoopBars * 4);
    const std::string patternName = acdFile.stem().string().empty() ? "ACID Arrangement" : acdFile.stem().string();
    const auto patternId = song.addPattern(patternName, arrangementBars);

    const int totalSteps = arrangementBars * kStepsPerBar;
    const int numTracks = std::min(static_cast<int>(songSampleIds.size()), kTracksPerPattern);

    for (int t = 0; t < numTracks; ++t)
    {
        const auto sampleId = songSampleIds[static_cast<std::size_t>(t)];
        const auto* sample = song.findSample(sampleId);
        if (sample == nullptr)
            continue;

        const int trackIdx = (t == 0) ? 0 : song.addTrackToPattern(patternId, sampleId);
        if (trackIdx < 0)
            continue;

        if (t == 0)
            song.setPatternTrackSample(patternId, 0, sampleId);

        song.renamePatternTrack(patternId, trackIdx, sample->name);

        const int loopBars = std::max(1, sample->loopBars);
        const int loopSteps = loopBars * kStepsPerBar;

        for (int step = 0; step + loopSteps <= totalSteps; step += loopSteps)
        {
            song.placeClip(patternId, trackIdx, sampleId, static_cast<double>(step), static_cast<double>(loopSteps));
        }
    }

    song.setArrangerTrackPattern(0, patternId);
    song.arrangerTrack(0)->name = patternName;
    song.placeBlock(0, patternId, 0);

    return song;
}

} // namespace trami
