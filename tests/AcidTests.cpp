#include "io/AcidChunkReader.h"
#include "io/AcidProjectReader.h"
#include "io/SampleImporter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace trami;

namespace
{

/// Creates a minimal valid WAV file with an embedded 'acid' chunk
void createSyntheticAcidWav(const std::filesystem::path& destFile,
                            bool oneShot,
                            int numBeats,
                            int meterNumer,
                            int meterDenom,
                            float tempoBpm,
                            uint16_t rootNote)
{
    std::ofstream out(destFile, std::ios::binary);

    // 16-bit mono PCM, 44100 Hz, 0.5 seconds (22050 samples)
    constexpr uint32_t sampleRate = 44100;
    constexpr uint16_t numChannels = 1;
    constexpr uint16_t bitsPerSample = 16;
    constexpr uint32_t numSamples = 22050;
    constexpr uint32_t dataBytes = numSamples * numChannels * (bitsPerSample / 8);

    // 24 bytes acid chunk payload
    constexpr uint32_t acidPayloadSize = 24;
    constexpr uint32_t acidChunkTotal = 8 + acidPayloadSize;

    // fmt chunk: 16 bytes payload + 8 header = 24 bytes
    constexpr uint32_t fmtChunkTotal = 8 + 16;

    // data chunk total = 8 + dataBytes
    const uint32_t dataChunkTotal = 8 + dataBytes;

    // RIFF size = 4 ('WAVE') + fmtChunkTotal + acidChunkTotal + dataChunkTotal
    const uint32_t riffSize = 4 + fmtChunkTotal + acidChunkTotal + dataChunkTotal;

    // Write RIFF header
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&riffSize), 4);
    out.write("WAVE", 4);

    // Write fmt chunk
    out.write("fmt ", 4);
    constexpr uint32_t fmtSize = 16;
    constexpr uint16_t audioFormat = 1; // PCM
    constexpr uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    constexpr uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    out.write(reinterpret_cast<const char*>(&fmtSize), 4);
    out.write(reinterpret_cast<const char*>(&audioFormat), 2);
    out.write(reinterpret_cast<const char*>(&numChannels), 2);
    out.write(reinterpret_cast<const char*>(&sampleRate), 4);
    out.write(reinterpret_cast<const char*>(&byteRate), 4);
    out.write(reinterpret_cast<const char*>(&blockAlign), 2);
    out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // Write acid chunk
    out.write("acid", 4);
    out.write(reinterpret_cast<const char*>(&acidPayloadSize), 4);

    uint32_t flags = 0;
    if (oneShot)
        flags |= 0x01;
    if (rootNote > 0)
        flags |= 0x02; // root note set

    uint16_t rNote = rootNote;
    uint16_t reserved1 = 0;
    float reserved2 = 0.0f;
    uint32_t beats = static_cast<uint32_t>(numBeats);
    uint16_t mDenom = static_cast<uint16_t>(meterDenom);
    uint16_t mNumer = static_cast<uint16_t>(meterNumer);
    float tempo = tempoBpm;

    out.write(reinterpret_cast<const char*>(&flags), 4);
    out.write(reinterpret_cast<const char*>(&rNote), 2);
    out.write(reinterpret_cast<const char*>(&reserved1), 2);
    out.write(reinterpret_cast<const char*>(&reserved2), 4);
    out.write(reinterpret_cast<const char*>(&beats), 4);
    out.write(reinterpret_cast<const char*>(&mDenom), 2);
    out.write(reinterpret_cast<const char*>(&mNumer), 2);
    out.write(reinterpret_cast<const char*>(&tempo), 4);

    // Write data chunk (synthetic sine wave)
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&dataBytes), 4);
    for (uint32_t i = 0; i < numSamples; ++i)
    {
        const double t = static_cast<double>(i) / sampleRate;
        const int16_t val = static_cast<int16_t>(std::sin(2.0 * 3.1415926535 * 440.0 * t) * 16000.0);
        out.write(reinterpret_cast<const char*>(&val), 2);
    }
}

} // namespace

TEST_CASE("AcidChunkReader correctly parses raw 24-byte acid chunk data")
{
    uint8_t payload[24] = {0};

    // Flags: loop, root note set
    uint32_t flags = 0x02;
    std::memcpy(payload + 0, &flags, 4);

    // Root note: 60 (Middle C)
    uint16_t rootNote = 60;
    std::memcpy(payload + 4, &rootNote, 2);

    // Beats: 8 (2 bars in 4/4)
    uint32_t numBeats = 8;
    std::memcpy(payload + 12, &numBeats, 4);

    // Meter: 4/4
    uint16_t denom = 4;
    uint16_t numer = 4;
    std::memcpy(payload + 16, &denom, 2);
    std::memcpy(payload + 18, &numer, 2);

    // Tempo: 126.5 BPM
    float tempo = 126.5f;
    std::memcpy(payload + 20, &tempo, 4);

    auto meta = AcidChunkReader::parseChunkData(payload, sizeof(payload));
    REQUIRE(meta.has_value());
    REQUIRE(meta->isAcidized == true);
    REQUIRE(meta->isOneShot == false);
    REQUIRE(meta->hasRootNote == true);
    REQUIRE(meta->rootNote == 60);
    REQUIRE(meta->numBeats == 8);
    REQUIRE(meta->meterNumerator == 4);
    REQUIRE(meta->meterDenominator == 4);
    REQUIRE(meta->tempo == Catch::Approx(126.5));
    REQUIRE(meta->suggestedLoopBars == 2);
}

TEST_CASE("AcidChunkReader detects one-shot flag")
{
    uint8_t payload[24] = {0};
    uint32_t flags = 0x01; // One-Shot
    std::memcpy(payload + 0, &flags, 4);

    auto meta = AcidChunkReader::parseChunkData(payload, sizeof(payload));
    REQUIRE(meta.has_value());
    REQUIRE(meta->isOneShot == true);
}

TEST_CASE("AcidChunkReader extracts metadata directly from a WAV file")
{
    const auto tempWav = std::filesystem::temp_directory_path() / "test_acid_loop.wav";
    std::filesystem::remove(tempWav);

    createSyntheticAcidWav(tempWav, false, 16, 4, 4, 132.0f, 62); // 16 beats = 4 bars, D4
    REQUIRE(std::filesystem::exists(tempWav));

    auto meta = AcidChunkReader::readFromFile(tempWav);
    REQUIRE(meta.has_value());
    REQUIRE(meta->isAcidized == true);
    REQUIRE(meta->isOneShot == false);
    REQUIRE(meta->numBeats == 16);
    REQUIRE(meta->suggestedLoopBars == 4);
    REQUIRE(meta->tempo == Catch::Approx(132.0));
    REQUIRE(meta->rootNote == 62);

    std::filesystem::remove(tempWav);
}

TEST_CASE("SampleImporter automatically reads Acidized loop metadata")
{
    const auto tempWav = std::filesystem::temp_directory_path() / "bass_acid_loop.wav";
    std::filesystem::remove(tempWav);

    // 8 beats = 2 bars at 125 BPM
    createSyntheticAcidWav(tempWav, false, 8, 4, 4, 125.0f, 55);

    SampleImporter importer;
    auto res = importer.importFile(tempWav, 44100.0, SampleKind::OneShot, 1);
    REQUIRE(res.has_value());

    // Auto-detection should have promoted it to Loop with 2 bars and 125 BPM
    REQUIRE(res->sample.kind == SampleKind::Loop);
    REQUIRE(res->sample.loopBars == 2);
    REQUIRE(res->sample.acidBpm == Catch::Approx(125.0));
    REQUIRE(res->sample.rootNote == 55);
    REQUIRE(res->sample.originalBpm() == Catch::Approx(125.0));

    std::filesystem::remove(tempWav);
}

TEST_CASE("AcidProjectReader identifies ACID files and loads .acd and .acd-zip")
{
    REQUIRE(AcidProjectReader::isAcidProject("project.acd"));
    REQUIRE(AcidProjectReader::isAcidProject("song.acd-zip"));
    REQUIRE(AcidProjectReader::isAcidProject("track.ACD"));
    REQUIRE_FALSE(AcidProjectReader::isAcidProject("song.wav"));
    REQUIRE_FALSE(AcidProjectReader::isAcidProject("song.trami"));

    // Set up a mock ACID project directory
    const auto testDir = std::filesystem::temp_directory_path() / "acid_test_session";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);

    const auto wav1 = testDir / "DrumBeat.wav";
    const auto wav2 = testDir / "SynthLead.wav";
    createSyntheticAcidWav(wav1, false, 4, 4, 4, 128.0f, 0);   // 1 bar at 128 BPM
    createSyntheticAcidWav(wav2, false, 8, 4, 4, 128.0f, 60);  // 2 bars at 128 BPM

    // Create a mock .acd file referencing DrumBeat.wav and SynthLead.wav
    const auto acdFile = testDir / "MyAcidSong.acd";
    {
        std::ofstream out(acdFile, std::ios::binary);
        // RIFF header with 'ACID' form
        out.write("RIFF", 4);
        uint32_t riffSz = 1024;
        out.write(reinterpret_cast<const char*>(&riffSz), 4);
        out.write("ACID", 4);

        // Tempo tag
        out.write("tmpo", 4);
        float bpm = 128.0f;
        out.write(reinterpret_cast<const char*>(&bpm), 4);

        // Referenced filenames embedded as strings
        out << "Track 1: DrumBeat.wav" << '\0';
        out << "Track 2: SynthLead.wav" << '\0';

        // Pad remainder
        std::vector<char> pad(512, 0);
        out.write(pad.data(), pad.size());
    }

    // 1. Test parsing standalone .acd
    StretchCache cache;
    SampleImporter importer;
    auto songOpt = AcidProjectReader::loadProject(acdFile, cache, importer, 44100.0);
    REQUIRE(songOpt.has_value());

    const auto& song = *songOpt;
    REQUIRE(song.bpm() == Catch::Approx(128.0));
    REQUIRE(song.samples().size() == 2);
    REQUIRE(song.patterns().size() == 1);
    REQUIRE(song.patterns().front().lengthBars >= 16);
    REQUIRE(song.patterns().front().tracks.size() >= 2);

    // Verify raw audio was decoded into cache
    for (const auto& sample : song.samples())
    {
        auto audio = cache.getRawAudio(sample.id);
        REQUIRE(audio != nullptr);
        REQUIRE(audio->numFrames() > 0);
    }

    // 2. Test packaging into an .acd-zip and loading it
    const auto zipFile = testDir / "MyAcidSong.acd-zip";
    {
        juce::File jZip(juce::String::fromUTF8(zipFile.string().c_str()));
        jZip.deleteFile();

        juce::ZipFile::Builder builder;
        builder.addFile(juce::File(juce::String::fromUTF8(acdFile.string().c_str())), 9, "MyAcidSong.acd");
        builder.addFile(juce::File(juce::String::fromUTF8(wav1.string().c_str())), 9, "DrumBeat.wav");
        builder.addFile(juce::File(juce::String::fromUTF8(wav2.string().c_str())), 9, "SynthLead.wav");

        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(jZip);
        REQUIRE(builder.writeToStream(*stream, nullptr));
    }
    REQUIRE(std::filesystem::exists(zipFile));

    StretchCache zipCache;
    std::filesystem::path extractedDir;
    auto zipSongOpt = AcidProjectReader::loadProject(zipFile, zipCache, importer, 44100.0, &extractedDir);
    REQUIRE(zipSongOpt.has_value());
    REQUIRE(zipSongOpt->bpm() == Catch::Approx(128.0));
    REQUIRE(zipSongOpt->samples().size() == 2);
    REQUIRE(zipSongOpt->patterns().size() == 1);
    REQUIRE(zipSongOpt->patterns().front().lengthBars >= 16);
    REQUIRE(zipSongOpt->patterns().front().tracks.size() >= 2);
    REQUIRE(std::filesystem::exists(extractedDir));

    std::filesystem::remove_all(testDir);
    std::filesystem::remove_all(extractedDir);
}

TEST_CASE("Pattern length is no longer restricted to 8 bars")
{
    Song song;
    const auto pat1 = song.addPattern("Short", 1);
    REQUIRE(song.findPattern(pat1)->lengthBars == 1);

    const auto pat16 = song.addPattern("Medium", 16);
    REQUIRE(song.findPattern(pat16)->lengthBars == 16);

    const auto pat64 = song.addPattern("Long", 64);
    REQUIRE(song.findPattern(pat64)->lengthBars == 64);

    const auto pat256 = song.addPattern("Arrangement", 256);
    REQUIRE(song.findPattern(pat256)->lengthBars == 256);

    REQUIRE(song.setPatternLength(pat1, 128));
    REQUIRE(song.findPattern(pat1)->lengthBars == 128);

    REQUIRE(song.setPatternLength(pat1, 1024));
    REQUIRE(song.findPattern(pat1)->lengthBars == 1024);
}

TEST_CASE("AcidProjectReader reconstructs authentic ACID arrangements from RIFF tlst and rlst")
{
    const auto testDir = std::filesystem::temp_directory_path() / "acid_riff_arrangement_test";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);

    const auto loopWav = testDir / "loop.wav";
    const auto shotWav = testDir / "shot.wav";
    createSyntheticAcidWav(loopWav, false, 4, 4, 4, 100.0f, 60); // 1 bar loop at 100 BPM
    createSyntheticAcidWav(shotWav, true, 4, 4, 4, 100.0f, 60);  // one shot

    const auto acdFile = testDir / "song.acd";

    // Helper lambda to write a chunk
    auto writeChunk = [](std::vector<uint8_t>& buf, const char* id, const std::vector<uint8_t>& payload) {
        buf.insert(buf.end(), id, id + 4);
        uint32_t sz = static_cast<uint32_t>(payload.size());
        const uint8_t* szP = reinterpret_cast<const uint8_t*>(&sz);
        buf.insert(buf.end(), szP, szP + 4);
        buf.insert(buf.end(), payload.begin(), payload.end());
        if (sz % 2 != 0)
            buf.push_back(0); // pad
    };

    // Helper lambda to write a LIST chunk
    auto writeList = [&writeChunk](std::vector<uint8_t>& buf, const char* listType, const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> fullPayload;
        fullPayload.insert(fullPayload.end(), listType, listType + 4);
        fullPayload.insert(fullPayload.end(), payload.begin(), payload.end());
        writeChunk(buf, "LIST", fullPayload);
    };

    // 1. Build tmap payload (20 bytes, 600,000 us/beat = 100 BPM)
    std::vector<uint8_t> tmapPayload(20, 0);
    uint32_t tmapSize = 20;
    uint32_t usPerBeat = 600000;
    std::memcpy(tmapPayload.data() + 0, &tmapSize, 4);
    std::memcpy(tmapPayload.data() + 8, &usPerBeat, 4);

    // 2. Build Track 0 (Loop track)
    std::vector<uint8_t> trak0Payload;
    {
        // trkh
        std::vector<uint8_t> trkhPayload(52, 0);
        uint32_t hdrSz = 52;
        std::memcpy(trkhPayload.data(), &hdrSz, 4);
        // append UTF-16LE string "loop.wav\0LoopTrack\0"
        std::u16string s1 = u"loop.wav";
        std::u16string s2 = u"LoopTrack";
        const uint8_t* p1 = reinterpret_cast<const uint8_t*>(s1.data());
        trkhPayload.insert(trkhPayload.end(), p1, p1 + (s1.length() + 1) * 2);
        const uint8_t* p2 = reinterpret_cast<const uint8_t*>(s2.data());
        trkhPayload.insert(trkhPayload.end(), p2, p2 + (s2.length() + 1) * 2);
        writeChunk(trak0Payload, "trkh", trkhPayload);

        // acid chunk (flags = 0 -> Loop)
        std::vector<uint8_t> acidPayload(24, 0);
        float bpm = 100.0f;
        uint32_t beats = 4;
        std::memcpy(acidPayload.data() + 12, &beats, 4);
        std::memcpy(acidPayload.data() + 20, &bpm, 4);
        writeChunk(trak0Payload, "acid", acidPayload);

        // rlst with 1 region: start=0, end=3072 (2 bars = 64 steps)
        std::vector<uint8_t> rlstPayload;
        std::vector<uint8_t> rgn0Payload;
        std::vector<uint8_t> rgnhPayload(36, 0);
        int32_t rgnSz = 36;
        int32_t startTicks = 0;
        int32_t endTicks = 3072;
        std::memcpy(rgnhPayload.data() + 0, &rgnSz, 4);
        std::memcpy(rgnhPayload.data() + 4, &startTicks, 4);
        std::memcpy(rgnhPayload.data() + 8, &endTicks, 4);
        writeChunk(rgn0Payload, "rgnh", rgnhPayload);
        writeList(rlstPayload, "rgn ", rgn0Payload);
        writeList(trak0Payload, "rlst", rlstPayload);
    }

    // 3. Build Track 1 (One-Shot track with 2 regions and transposition)
    std::vector<uint8_t> trak1Payload;
    {
        // trkh
        std::vector<uint8_t> trkhPayload(52, 0);
        uint32_t hdrSz = 52;
        std::memcpy(trkhPayload.data(), &hdrSz, 4);
        std::u16string s1 = u"shot.wav";
        std::u16string s2 = u"ShotTrack";
        const uint8_t* p1 = reinterpret_cast<const uint8_t*>(s1.data());
        trkhPayload.insert(trkhPayload.end(), p1, p1 + (s1.length() + 1) * 2);
        const uint8_t* p2 = reinterpret_cast<const uint8_t*>(s2.data());
        trkhPayload.insert(trkhPayload.end(), p2, p2 + (s2.length() + 1) * 2);
        writeChunk(trak1Payload, "trkh", trkhPayload);

        // acid chunk (flags = 1 -> One-Shot)
        std::vector<uint8_t> acidPayload(24, 0);
        uint32_t flags = 1;
        float bpm = 100.0f;
        uint32_t beats = 4;
        std::memcpy(acidPayload.data() + 0, &flags, 4);
        std::memcpy(acidPayload.data() + 12, &beats, 4);
        std::memcpy(acidPayload.data() + 20, &bpm, 4);
        writeChunk(trak1Payload, "acid", acidPayload);

        // rlst with 2 regions:
        // Region 1: start=1536 (bar 1 = step 32), end=2304 (step 48, len=16), transpose=+4
        // Region 2: start=4608 (bar 3 = step 96), end=6144 (step 128, len=32), transpose=-2
        std::vector<uint8_t> rlstPayload;

        std::vector<uint8_t> rgn1Payload;
        std::vector<uint8_t> rgnh1(36, 0);
        int32_t rgnSz = 36;
        int32_t tickStart1 = 1536, tickEnd1 = 2304, tr1 = 4;
        std::memcpy(rgnh1.data() + 0, &rgnSz, 4);
        std::memcpy(rgnh1.data() + 4, &tickStart1, 4);
        std::memcpy(rgnh1.data() + 8, &tickEnd1, 4);
        std::memcpy(rgnh1.data() + 28, &tr1, 4);
        writeChunk(rgn1Payload, "rgnh", rgnh1);
        writeList(rlstPayload, "rgn ", rgn1Payload);

        std::vector<uint8_t> rgn2Payload;
        std::vector<uint8_t> rgnh2(36, 0);
        int32_t tickStart2 = 4608, tickEnd2 = 6144, tr2 = -2;
        std::memcpy(rgnh2.data() + 0, &rgnSz, 4);
        std::memcpy(rgnh2.data() + 4, &tickStart2, 4);
        std::memcpy(rgnh2.data() + 8, &tickEnd2, 4);
        std::memcpy(rgnh2.data() + 28, &tr2, 4);
        writeChunk(rgn2Payload, "rgnh", rgnh2);
        writeList(rlstPayload, "rgn ", rgn2Payload);

        writeList(trak1Payload, "rlst", rlstPayload);
    }

    // Combine tracks into tlst
    std::vector<uint8_t> tlstPayload;
    writeList(tlstPayload, "trak", trak0Payload);
    writeList(tlstPayload, "trak", trak1Payload);

    // Combine into root RIFF ('ACID')
    std::vector<uint8_t> riffPayload;
    riffPayload.insert(riffPayload.end(), {'A', 'C', 'I', 'D'});
    writeChunk(riffPayload, "tmap", tmapPayload);
    writeList(riffPayload, "tlst", tlstPayload);

    // Write file
    {
        std::ofstream out(acdFile, std::ios::binary);
        out.write("RIFF", 4);
        uint32_t rSz = static_cast<uint32_t>(riffPayload.size());
        out.write(reinterpret_cast<const char*>(&rSz), 4);
        out.write(reinterpret_cast<const char*>(riffPayload.data()), riffPayload.size());
    }

    // Now parse with AcidProjectReader
    StretchCache cache;
    SampleImporter importer;
    auto songOpt = AcidProjectReader::loadProject(acdFile, cache, importer, 44100.0);
    REQUIRE(songOpt.has_value());

    const auto& song = *songOpt;
    REQUIRE(song.bpm() == Catch::Approx(100.0));
    REQUIRE(song.patterns().size() == 1);

    const auto& pat = song.patterns().front();
    // Max tick is 6144 = 4 bars
    REQUIRE(pat.lengthBars == 4);
    REQUIRE(pat.tracks.size() == 2);

    // Track 0
    REQUIRE(pat.tracks[0].name == "LoopTrack");
    REQUIRE(song.patternTrackKind(pat.id, 0) == SampleKind::Loop);
    REQUIRE(pat.tracks[0].clips.size() == 1);
    REQUIRE(pat.tracks[0].clips[0].startStep == Catch::Approx(0.0));
    REQUIRE(pat.tracks[0].clips[0].lengthSteps == Catch::Approx(64.0));
    REQUIRE(pat.tracks[0].clips[0].transposeSemitones == 0);

    // Track 1
    REQUIRE(pat.tracks[1].name == "ShotTrack");
    REQUIRE(song.patternTrackKind(pat.id, 1) == SampleKind::OneShot);
    REQUIRE(pat.tracks[1].clips.size() == 2);
    // Region 1: startTicks=1536 -> step 32, len=768 ticks -> 16 steps, transpose +4
    REQUIRE(pat.tracks[1].clips[0].startStep == Catch::Approx(32.0));
    REQUIRE(pat.tracks[1].clips[0].lengthSteps == Catch::Approx(16.0));
    REQUIRE(pat.tracks[1].clips[0].transposeSemitones == 4);
    // Region 2: startTicks=4608 -> step 96, len=1536 ticks -> 32 steps, transpose -2
    REQUIRE(pat.tracks[1].clips[1].startStep == Catch::Approx(96.0));
    REQUIRE(pat.tracks[1].clips[1].lengthSteps == Catch::Approx(32.0));
    REQUIRE(pat.tracks[1].clips[1].transposeSemitones == -2);

    std::filesystem::remove_all(testDir);
}

TEST_CASE("AcidProjectReader loads real-world aldi.acd if present on disk")
{
    const std::filesystem::path aldiAcd = "C:/Users/Kessler Heinz/Desktop/aldi/aldi.acd";
    std::error_code ec;
    if (!std::filesystem::exists(aldiAcd, ec))
        return;

    StretchCache cache;
    SampleImporter importer;
    auto songOpt = AcidProjectReader::loadProject(aldiAcd, cache, importer, 44100.0);
    REQUIRE(songOpt.has_value());

    const auto& song = *songOpt;
    REQUIRE(song.bpm() == Catch::Approx(100.0));
    REQUIRE(song.patterns().size() == 1);

    const auto& pat = song.patterns().front();
    // 294912 ticks / 1536 = 192 bars
    REQUIRE(pat.lengthBars == 192);

    // Verify track names and clips exist
    bool foundGongOneShot = false;
    bool foundPulseRingerTranspose = false;

    for (std::size_t t = 0; t < pat.tracks.size(); ++t)
    {
        const auto& trk = pat.tracks[t];
        if (trk.name == "gong")
        {
            REQUIRE(song.patternTrackKind(pat.id, static_cast<int>(t)) == SampleKind::OneShot);
            REQUIRE(trk.clips.size() == 2);
            foundGongOneShot = true;
        }
        else if (trk.name == "PulseRinger")
        {
            REQUIRE(trk.clips.size() == 2);
            for (const auto& c : trk.clips)
            {
                REQUIRE(c.transposeSemitones == -3);
            }
            foundPulseRingerTranspose = true;
        }
    }

    REQUIRE(foundGongOneShot);
    REQUIRE(foundPulseRingerTranspose);
}
