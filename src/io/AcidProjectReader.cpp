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
    const unsigned char uc = static_cast<unsigned char>(c);
    return uc >= 0x20 && c != '"' && c != '<' && c != '>' && c != '|' && c != '?' && c != '*' && c != '\r' && c != '\n';
}

std::string cleanFilename(std::string pathOrName)
{
    if (pathOrName.empty())
        return {};
    std::replace(pathOrName.begin(), pathOrName.end(), '\\', '/');
    while (pathOrName.size() > 1 && pathOrName.back() == '/')
        pathOrName.pop_back();
    const auto pos = pathOrName.find_last_of('/');
    if (pos != std::string::npos)
        pathOrName = pathOrName.substr(pos + 1);
    while (!pathOrName.empty() && static_cast<unsigned char>(pathOrName.front()) <= 0x20)
        pathOrName.erase(pathOrName.begin());
    while (!pathOrName.empty() && static_cast<unsigned char>(pathOrName.back()) <= 0x20)
        pathOrName.pop_back();
    return pathOrName;
}

std::string cleanStem(const std::string& pathOrName)
{
    const std::string fn = cleanFilename(pathOrName);
    const auto dot = fn.find_last_of('.');
    if (dot != std::string::npos && dot > 0)
        return fn.substr(0, dot);
    return fn;
}

bool filenamesMatch(const std::string& a, const std::string& b)
{
    if (toLower(a) == toLower(b))
        return true;
    return juce::String::fromUTF8(a.c_str()).equalsIgnoreCase(juce::String::fromUTF8(b.c_str()));
}

std::string findAudioReferenceInBytes(const std::uint8_t* data, std::size_t size)
{
    if (size < 4)
        return {};

    // 1. Search for ASCII audio extensions
    static const char* kExts[] = {".wav", ".WAV", ".mp3", ".MP3", ".aif", ".aiff", ".AIFF", ".ogg", ".flac"};
    for (std::size_t i = 0; i < size; ++i)
    {
        for (const char* ext : kExts)
        {
            const std::size_t extLen = std::strlen(ext);
            if (i + extLen <= size && std::memcmp(data + i, ext, extLen) == 0)
            {
                std::size_t start = i;
                while (start > 0 && isValidPathChar(static_cast<char>(data[start - 1])))
                {
                    --start;
                    if (i - start > 512)
                        break;
                }
                std::size_t end = i + extLen;
                while (end < size && isValidPathChar(static_cast<char>(data[end])))
                {
                    ++end;
                    if (end - i > 64)
                        break;
                }
                std::string s(reinterpret_cast<const char*>(data + start), end - start);
                if (hasAudioExtension(s))
                    return s;
            }
        }
    }

    // 2. Search for UTF-16LE audio extensions
    for (std::size_t i = 0; i + 8 <= size; i += 2)
    {
        const auto* b = data + i;
        if (b[0] == '.' && b[1] == 0 &&
            (b[2] == 'w' || b[2] == 'W') && b[3] == 0 &&
            (b[4] == 'a' || b[4] == 'A') && b[5] == 0 &&
            (b[6] == 'v' || b[6] == 'V') && b[7] == 0)
        {
            std::size_t start = i;
            while (start >= 2 && data[start - 1] == 0 && isValidPathChar(static_cast<char>(data[start - 2])))
            {
                start -= 2;
                if (i - start > 1024)
                    break;
            }
            std::string utf8;
            for (std::size_t u = start; u < i + 8; u += 2)
            {
                if (data[u + 1] != 0) break;
                utf8.push_back(static_cast<char>(data[u]));
            }
            if (hasAudioExtension(utf8))
                return utf8;
        }
    }

    return {};
}

std::filesystem::path findFileCaseInsensitive(const std::filesystem::path& dir, const std::string& targetName)
{
    const auto target = cleanFilename(targetName);
    if (target.empty())
        return {};
    std::error_code ec;

    if (!std::filesystem::exists(dir, ec))
        return {};

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (entry.is_regular_file(ec))
        {
            if (filenamesMatch(entry.path().filename().string(), target))
                return entry.path();
        }
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec))
    {
        if (entry.is_regular_file(ec))
        {
            if (filenamesMatch(entry.path().filename().string(), target))
                return entry.path();
        }
    }
    return {};
}

std::filesystem::path findFileStemCaseInsensitive(const std::filesystem::path& dir, const std::string& stem)
{
    const auto targetStem = cleanStem(stem);
    if (targetStem.empty())
        return {};
    std::error_code ec;

    if (!std::filesystem::exists(dir, ec))
        return {};

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (entry.is_regular_file(ec) && hasAudioExtension(entry.path().string()))
        {
            if (filenamesMatch(entry.path().stem().string(), targetStem))
                return entry.path();
        }
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec))
    {
        if (entry.is_regular_file(ec) && hasAudioExtension(entry.path().string()))
        {
            if (filenamesMatch(entry.path().stem().string(), targetStem))
                return entry.path();
        }
    }
    return {};
}

std::filesystem::path findFileFuzzyCaseInsensitive(const std::filesystem::path& dir, const std::string& stem)
{
    const auto clean = toLower(cleanStem(stem));
    if (clean.length() < 3)
        return {};

    std::error_code ec;
    if (!std::filesystem::exists(dir, ec))
        return {};

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (entry.is_regular_file(ec) && hasAudioExtension(entry.path().string()))
        {
            const auto entryStem = toLower(entry.path().stem().string());
            if (entryStem.find(clean) != std::string::npos || clean.find(entryStem) != std::string::npos)
                return entry.path();
        }
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec))
    {
        if (entry.is_regular_file(ec) && hasAudioExtension(entry.path().string()))
        {
            const auto entryStem = toLower(entry.path().stem().string());
            if (entryStem.find(clean) != std::string::npos || clean.find(entryStem) != std::string::npos)
                return entry.path();
        }
    }
    return {};
}

void logAcid(const std::string& msg)
{
    std::ofstream out("/private/tmp/trami_acid_load.log", std::ios::app);
    if (out.is_open())
        out << msg << "\n";
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

    const std::string directAudioRef = findAudioReferenceInBytes(payload, size);

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
            bool looksLikePackedAscii = false;
            while (ptr + 2 <= end)
            {
                char16_t ch = 0;
                std::memcpy(&ch, ptr, 2);
                ptr += 2;
                if (ch == 0)
                    break;
                uint8_t low = ch & 0xFF;
                uint8_t high = (ch >> 8) & 0xFF;
                if (high >= 0x20 && high <= 0x7E && low >= 0x20 && low <= 0x7E)
                    looksLikePackedAscii = true;
                u16.push_back(ch);
            }
            if (!u16.empty() && !looksLikePackedAscii)
            {
                juce::String jstr(juce::CharPointer_UTF16(reinterpret_cast<const juce::CharPointer_UTF16::CharType*>(u16.c_str())));
                const std::string s = jstr.toStdString();
                if (!s.empty())
                    result.push_back(s);
            }
        }
    }

    // Fallback: If no valid UTF-16 strings were found, extract null-terminated ASCII strings
    if (result.empty())
    {
        if (hdrSize < size)
        {
            const std::uint8_t* ptr = payload + hdrSize;
            const std::uint8_t* end = payload + size;
            while (ptr < end)
            {
                std::string asciiStr;
                while (ptr < end && *ptr != 0 && isValidPathChar(static_cast<char>(*ptr)))
                {
                    asciiStr.push_back(static_cast<char>(*ptr));
                    ptr++;
                }
                if (ptr < end && *ptr == 0)
                    ptr++;
                if (!asciiStr.empty())
                    result.push_back(asciiStr);
            }
        }
    }

    if (!directAudioRef.empty())
    {
        if (result.empty() || (!hasAudioExtension(result[0]) && hasAudioExtension(directAudioRef)))
        {
            result.insert(result.begin(), directAudioRef);
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
                            acidTrack.sampleFileName = cleanFilename(strings[0]);
                            if (strings.size() >= 2 && !strings[1].empty())
                                acidTrack.trackName = cleanFilename(strings[1]);
                            else
                                acidTrack.trackName = cleanStem(acidTrack.sampleFileName);
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

                        acidTrack.hasAcidChunk = true;
                        acidTrack.isOneShot = (flags & 0x01) != 0;
                        acidTrack.rootNote = (flags & 0x02) ? static_cast<int>(rootNote) : -1;
                        acidTrack.beatCount = (beats > 0) ? beats : 4;
                        acidTrack.originalBpm = (tempo > 20.0f && tempo < 999.0f) ? tempo : 0.0f;
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
                                        clip.sampleOffsetTicks = (sampleOffset > 0 && sampleOffset < (endTicks - startTicks)) ? sampleOffset : 0;
                                        clip.transposeSemitones = std::clamp(transpose, -48, 48);
                                        acidTrack.clips.push_back(clip);
                                    }
                                }
                            }
                        }
                    }
                }

                if (acidTrack.sampleFileName.empty() || !hasAudioExtension(acidTrack.sampleFileName))
                {
                    const auto foundInTrak = findAudioReferenceInBytes(buffer.data() + trak.offset, trak.size);
                    if (!foundInTrak.empty())
                    {
                        acidTrack.samplePath = foundInTrak;
                        acidTrack.sampleFileName = cleanFilename(foundInTrak);
                        if (acidTrack.trackName.empty())
                            acidTrack.trackName = cleanStem(acidTrack.sampleFileName);
                    }
                }
                acidTrack.sampleFileName = cleanFilename(acidTrack.sampleFileName);
                if (acidTrack.trackName.empty())
                    acidTrack.trackName = cleanStem(acidTrack.sampleFileName);
                else
                    acidTrack.trackName = cleanFilename(acidTrack.trackName);

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
    {
        std::ofstream out("/private/tmp/trami_acid_load.log", std::ios::trunc);
        out << "=== Loading ACID Project ===\n";
        out << "Project file: " << projectFile.string() << "\n";
    }

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

    logAcid("ACID file: " + acdFile.string());
    logAcid("mediaSearchDir: " + mediaSearchDir.string());
    logAcid("Files in mediaSearchDir:");
    int audioFileCount = 0;
    std::error_code dirEc;
    if (std::filesystem::exists(mediaSearchDir, dirEc))
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(mediaSearchDir, std::filesystem::directory_options::skip_permission_denied, dirEc))
        {
            if (entry.is_regular_file(dirEc) && hasAudioExtension(entry.path().string()))
            {
                logAcid("  [audio on disk] " + entry.path().filename().string() + "  ->  " + entry.path().string());
                audioFileCount++;
            }
        }
    }
    logAcid("Total audio files found on disk: " + std::to_string(audioFileCount));

    // 1. Try parsing full RIFF ACID project structure
    auto riffProj = parseRiffAcid(buffer);

    // If RIFF structure had tracks with clips or samples, use the authentic arrangement
    if (!riffProj.tracks.empty())
    {
        logAcid("Tracks found in RIFF ACID project: " + std::to_string(riffProj.tracks.size()));
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

            std::vector<std::string> candidates;
            auto addCandidate = [&](const std::string& raw) {
                if (raw.empty()) return;
                std::string fn = cleanFilename(raw);
                if (!fn.empty()) {
                    candidates.push_back(fn);
                    std::string st = cleanStem(fn);
                    if (!st.empty() && st != fn) {
                        candidates.push_back(st + ".wav");
                        candidates.push_back(st + ".WAV");
                    }
                }
            };

            addCandidate(track.sampleFileName);
            addCandidate(track.samplePath);
            addCandidate(track.trackName);

            // 1. Direct match in mediaSearchDir
            for (const auto& cand : candidates)
            {
                const auto candidateDirect = mediaSearchDir / cand;
                if (std::filesystem::exists(candidateDirect, ec) && std::filesystem::is_regular_file(candidateDirect, ec))
                {
                    resolved = candidateDirect;
                    break;
                }
            }

            // 2. Direct match in mediaSearchDir / "samples"
            if (resolved.empty())
            {
                for (const auto& cand : candidates)
                {
                    const auto candidateSamples = mediaSearchDir / "samples" / cand;
                    if (std::filesystem::exists(candidateSamples, ec) && std::filesystem::is_regular_file(candidateSamples, ec))
                    {
                        resolved = candidateSamples;
                        break;
                    }
                }
            }

            // 3. Case-insensitive search directly in mediaSearchDir (non-recursive)
            if (resolved.empty() && std::filesystem::exists(mediaSearchDir, ec))
            {
                for (const auto& cand : candidates)
                {
                    const auto lowerCand = toLower(cand);
                    for (const auto& entry : std::filesystem::directory_iterator(mediaSearchDir, ec))
                    {
                        if (entry.is_regular_file(ec) && toLower(entry.path().filename().string()) == lowerCand)
                        {
                            resolved = entry.path();
                            break;
                        }
                    }
                    if (!resolved.empty())
                        break;
                }
            }

            // 4. Stem match directly in mediaSearchDir (non-recursive)
            if (resolved.empty() && std::filesystem::exists(mediaSearchDir, ec))
            {
                for (const auto& cand : candidates)
                {
                    const auto lowerStem = toLower(cleanStem(cand));
                    if (lowerStem.empty()) continue;
                    for (const auto& entry : std::filesystem::directory_iterator(mediaSearchDir, ec))
                    {
                        if (entry.is_regular_file(ec) && hasAudioExtension(entry.path().string()) &&
                            toLower(entry.path().stem().string()) == lowerStem)
                        {
                            resolved = entry.path();
                            break;
                        }
                    }
                    if (!resolved.empty())
                        break;
                }
            }

            // 5. Recursive search in mediaSearchDir (by filename and stem)
            if (resolved.empty())
            {
                for (const auto& cand : candidates)
                {
                    resolved = findFileCaseInsensitive(mediaSearchDir, cand);
                    if (!resolved.empty())
                        break;
                    resolved = findFileStemCaseInsensitive(mediaSearchDir, cleanStem(cand));
                    if (!resolved.empty())
                        break;
                }
            }

            // 6. Direct samplePath if it exists on disk
            if (resolved.empty() && !track.samplePath.empty())
            {
                if (std::filesystem::exists(track.samplePath, ec) && std::filesystem::is_regular_file(track.samplePath, ec))
                    resolved = track.samplePath;
            }

            // 7. Search in parent directory of mediaSearchDir (e.g. if project is in a subfolder)
            if (resolved.empty())
            {
                const auto parentDir = mediaSearchDir.parent_path();
                if (std::filesystem::exists(parentDir, ec) && parentDir != mediaSearchDir)
                {
                    for (const auto& cand : candidates)
                    {
                        resolved = findFileCaseInsensitive(parentDir, cand);
                        if (!resolved.empty())
                            break;
                        resolved = findFileStemCaseInsensitive(parentDir, cleanStem(cand));
                        if (!resolved.empty())
                            break;
                    }
                }
            }

            // 8. Fuzzy / substring match on stem
            if (resolved.empty())
            {
                for (const auto& cand : candidates)
                {
                    resolved = findFileFuzzyCaseInsensitive(mediaSearchDir, cleanStem(cand));
                    if (!resolved.empty())
                        break;
                }
            }

            logAcid("  Track " + std::to_string(track.trackIndex) + " ['" + track.trackName + "'] (sampleFileName: '" + track.sampleFileName + "', samplePath: '" + track.samplePath + "')");
            if (!resolved.empty())
                logAcid("    -> MATCHED: " + resolved.string());
            else
                logAcid("    -> MISSING (placeholder created)");

            SampleId sampleId;

            if (!resolved.empty())
            {
                const int loopBarsHint = (track.beatCount > 0) ? std::max(1, static_cast<int>(track.beatCount / 4)) : 1;
                const SampleKind initialKind = track.hasAcidChunk
                    ? (track.isOneShot ? SampleKind::OneShot : SampleKind::Loop)
                    : SampleKind::Loop;

                auto importRes = importer.importFile(resolved, sampleRate, initialKind, loopBarsHint, std::nullopt, riffProj.bpm);
                if (importRes.has_value())
                {
                    Sample s = importRes->sample;
                    if (track.hasAcidChunk)
                    {
                        s.kind = track.isOneShot ? SampleKind::OneShot : SampleKind::Loop;
                        if (track.beatCount > 0)
                        {
                            s.loopBeats = static_cast<double>(track.beatCount);
                            s.loopBars = std::max(1, static_cast<int>(track.beatCount / 4));
                        }
                        if (track.originalBpm > 20.0f)
                            s.acidBpm = track.originalBpm;
                        if (track.rootNote >= 0)
                            s.rootNote = track.rootNote;
                    }

                    // For loops: always ensure loopBars, loopBeats and acidBpm are consistent with duration
                    if (s.kind == SampleKind::Loop && s.durationSeconds > 0.0)
                    {
                        if (s.loopBeats > 0.0)
                        {
                            const double calcBpm = (s.loopBeats * 60.0) / s.durationSeconds;
                            // If stored acidBpm was calculated assuming 4 beats while loopBeats is sub-bar (e.g. 2 beats),
                            // or if acidBpm was not specified, use authentic BPM from beats and duration
                            if (s.acidBpm <= 0.0 || (s.loopBeats < 4.0 && std::abs(s.acidBpm - calcBpm * (4.0 / s.loopBeats)) < 1.0))
                            {
                                s.acidBpm = calcBpm;
                            }
                        }
                        else
                        {
                            const double effectiveTempo = (s.acidBpm > 20.0) ? s.acidBpm : riffProj.bpm;
                            if (effectiveTempo > 20.0)
                            {
                                const double secPerBar = (60.0 / effectiveTempo) * 4.0;
                                const int estBars = std::max(1, static_cast<int>(std::round(s.durationSeconds / secPerBar)));
                                if (s.loopBars <= 1 && estBars > 1)
                                    s.loopBars = estBars;
                            }

                            if (s.acidBpm <= 0.0 && s.loopBars > 0)
                            {
                                s.acidBpm = Timing::loopBpm(s.durationSeconds, s.loopBars);
                            }
                        }
                    }

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
                s.kind = track.hasAcidChunk ? (track.isOneShot ? SampleKind::OneShot : SampleKind::Loop) : SampleKind::Loop;
                if (track.beatCount > 0)
                {
                    s.loopBeats = static_cast<double>(track.beatCount);
                    s.loopBars = std::max(1, static_cast<int>(track.beatCount / 4));
                }
                else
                {
                    s.loopBars = 1;
                }
                s.acidBpm = (track.originalBpm > 20.0f) ? track.originalBpm : riffProj.bpm;
                if (s.loopBeats > 0.0 && s.loopBeats < 4.0 && s.acidBpm > 20.0f)
                {
                    s.acidBpm = s.acidBpm * (s.loopBeats / 4.0);
                }
                s.rootNote = track.rootNote;

                const int numFrames = static_cast<int>(std::round(sampleRate * (60.0 / riffProj.bpm) * s.loopCycleBeats()));
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

                SampleKind effectiveKind = SampleKind::Loop;
                if (track.hasAcidChunk)
                {
                    effectiveKind = track.isOneShot ? SampleKind::OneShot : SampleKind::Loop;
                }
                else if (const auto* s = song.findSample(sampleId))
                {
                    effectiveKind = s->kind;
                }
                song.setPatternTrackKind(patternId, patternTrackIdx, effectiveKind);

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
                    const auto base = cleanFilename(foundStr);
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
                    const auto base = cleanFilename(utf8Str);
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
