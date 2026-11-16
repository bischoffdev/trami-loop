#include "io/AcidChunkReader.h"

#include <cstring>
#include <fstream>

namespace trami
{

std::optional<AcidMetadata> AcidChunkReader::readFromReader(const juce::AudioFormatReader& reader)
{
    const auto& meta = reader.metadataValues;
    const bool hasTempo = meta.containsKey(juce::WavAudioFormat::acidTempo);
    const bool hasBeats = meta.containsKey(juce::WavAudioFormat::acidBeats);
    const bool hasOneShot = meta.containsKey(juce::WavAudioFormat::acidOneShot);

    if (!hasTempo && !hasBeats && !hasOneShot)
        return std::nullopt;

    AcidMetadata result;
    result.isAcidized = true;

    if (hasOneShot)
        result.isOneShot = (meta.getValue(juce::WavAudioFormat::acidOneShot, "0").getIntValue() != 0);

    if (hasBeats)
        result.numBeats = std::max(1, meta.getValue(juce::WavAudioFormat::acidBeats, "4").getIntValue());

    if (meta.containsKey(juce::WavAudioFormat::acidNumerator))
        result.meterNumerator = std::max(1, meta.getValue(juce::WavAudioFormat::acidNumerator, "4").getIntValue());

    if (meta.containsKey(juce::WavAudioFormat::acidDenominator))
        result.meterDenominator = std::max(1, meta.getValue(juce::WavAudioFormat::acidDenominator, "4").getIntValue());

    if (hasTempo)
    {
        const double bpm = meta.getValue(juce::WavAudioFormat::acidTempo, "120.0").getDoubleValue();
        result.tempo = (bpm > 10.0 && bpm < 999.0) ? bpm : 120.0;
    }

    if (meta.containsKey(juce::WavAudioFormat::acidRootSet))
    {
        result.hasRootNote = (meta.getValue(juce::WavAudioFormat::acidRootSet, "0").getIntValue() != 0);
        if (result.hasRootNote && meta.containsKey(juce::WavAudioFormat::acidRootNote))
        {
            result.rootNote = meta.getValue(juce::WavAudioFormat::acidRootNote, "-1").getIntValue();
        }
    }

    result.suggestedLoopBars = AcidMetadata::calculateLoopBars(result.numBeats, result.meterNumerator);
    return result;
}

std::optional<AcidMetadata> AcidChunkReader::parseChunkData(const void* data, std::size_t size)
{
    if (data == nullptr || size < 24)
        return std::nullopt;

    const auto* bytes = static_cast<const std::uint8_t*>(data);

    // 0..3: uint32 dwFlags
    std::uint32_t flags = 0;
    std::memcpy(&flags, bytes + 0, 4);

    // 4..5: uint16 wRootNote
    std::uint16_t rootNote = 0;
    std::memcpy(&rootNote, bytes + 4, 2);

    // 12..15: uint32 dwNumBeats
    std::uint32_t numBeats = 0;
    std::memcpy(&numBeats, bytes + 12, 4);

    // 16..17: uint16 wMeterDenom
    std::uint16_t meterDenom = 0;
    std::memcpy(&meterDenom, bytes + 16, 2);

    // 18..19: uint16 wMeterNumer
    std::uint16_t meterNumer = 0;
    std::memcpy(&meterNumer, bytes + 18, 2);

    // 20..23: float32 fTempo
    float tempo = 120.0f;
    std::memcpy(&tempo, bytes + 20, 4);

    AcidMetadata result;
    result.isAcidized = true;
    result.isOneShot = (flags & 0x01) != 0;
    result.hasRootNote = (flags & 0x02) != 0;
    result.rootNote = result.hasRootNote ? static_cast<int>(rootNote) : -1;
    result.numBeats = (numBeats > 0) ? static_cast<int>(numBeats) : 4;
    result.meterNumerator = (meterNumer > 0) ? static_cast<int>(meterNumer) : 4;
    result.meterDenominator = (meterDenom > 0) ? static_cast<int>(meterDenom) : 4;

    if (tempo > 10.0f && tempo < 999.0f)
        result.tempo = static_cast<double>(tempo);
    else
        result.tempo = 120.0;

    result.suggestedLoopBars = AcidMetadata::calculateLoopBars(result.numBeats, result.meterNumerator);
    return result;
}

std::optional<AcidMetadata> AcidChunkReader::readFromFile(const std::filesystem::path& wavFile)
{
    std::ifstream file(wavFile, std::ios::binary);
    if (!file.is_open())
        return std::nullopt;

    char header[12];
    if (!file.read(header, 12))
        return std::nullopt;

    if (std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0)
        return std::nullopt;

    while (file)
    {
        char chunkHdr[8];
        if (!file.read(chunkHdr, 8))
            break;

        std::uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, chunkHdr + 4, 4);

        if (std::memcmp(chunkHdr, "acid", 4) == 0)
        {
            if (chunkSize >= 24)
            {
                std::vector<char> buffer(chunkSize);
                if (file.read(buffer.data(), chunkSize))
                    return parseChunkData(buffer.data(), chunkSize);
            }
            break;
        }

        // Chunks are padded to word boundaries
        const std::streamoff skip = (chunkSize + 1) & ~1ULL;
        file.seekg(skip, std::ios::cur);
    }

    return std::nullopt;
}

} // namespace trami
