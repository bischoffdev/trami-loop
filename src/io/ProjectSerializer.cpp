#include "io/ProjectSerializer.h"

#include <juce_core/juce_core.h>

namespace trami
{
namespace
{
double inferAudioSampleRate(const AudioData& raw, double durationSeconds) noexcept
{
    double rawRate = (durationSeconds > 0.0 && raw.numFrames() > 0)
        ? std::round(static_cast<double>(raw.numFrames()) / durationSeconds)
        : 44100.0;
    for (double stdRate : {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, 32000.0, 22050.0})
    {
        if (std::abs(rawRate - stdRate) < stdRate * 0.006)
        {
            rawRate = stdRate;
            break;
        }
    }
    if (rawRate < 8000.0 || rawRate > 384000.0)
        rawRate = 44100.0;
    return rawRate;
}

bool writeWavFile(const std::filesystem::path& dest, const AudioData& raw, double sampleRate)
{
    if (raw.numChannels() <= 0 || raw.numFrames() <= 0)
        return false;
    juce::File jDest(juce::String::fromUTF8(dest.string().c_str()));
    jDest.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(jDest);
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}
                    .withSampleRate(sampleRate)
                    .withNumChannels(raw.numChannels())
                    .withBitsPerSample(24));
    if (writer == nullptr)
        return false;

    juce::AudioBuffer<float> temp(raw.numChannels(), static_cast<int>(raw.numFrames()));
    for (int ch = 0; ch < raw.numChannels(); ++ch)
        temp.copyFrom(ch, 0, raw.channels[static_cast<std::size_t>(ch)].data(),
                      static_cast<int>(raw.numFrames()));
    return writer->writeFromAudioSampleBuffer(temp, 0, static_cast<int>(raw.numFrames()));
}

const char* kindToString(SampleKind kind) noexcept
{
    if (kind == SampleKind::Loop)
        return "loop";
    if (kind == SampleKind::DiskStream)
        return "disk";
    return "oneshot";
}

SampleKind kindFromString(const juce::String& str) noexcept
{
    if (str == "loop")
        return SampleKind::Loop;
    if (str == "disk")
        return SampleKind::DiskStream;
    return SampleKind::OneShot;
}

juce::Array<juce::var> serializeEnvelope(const std::vector<EnvelopePoint>& pts)
{
    juce::Array<juce::var> arr;
    for (const auto& pt : pts)
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("pos", pt.position);
        obj->setProperty("val", pt.value);
        if (pt.curve != EnvelopeCurve::Linear)
            obj->setProperty("curve", static_cast<int>(pt.curve));
        arr.add(obj.get());
    }
    return arr;
}

std::vector<EnvelopePoint> deserializeEnvelope(const juce::var& v)
{
    std::vector<EnvelopePoint> pts;
    if (const auto* arr = v.getArray())
    {
        for (const auto& item : *arr)
        {
            EnvelopePoint pt;
            pt.position = static_cast<double>(item.getProperty("pos", 0.0));
            pt.value = static_cast<float>(item.getProperty("val", 1.0f));
            const int c = static_cast<int>(item.getProperty("curve", 0));
            if (c == 1)
                pt.curve = EnvelopeCurve::Fast;
            else if (c == 2)
                pt.curve = EnvelopeCurve::Slow;
            else
                pt.curve = EnvelopeCurve::Linear;
            pts.push_back(pt);
        }
    }
    return pts;
}

juce::Array<juce::var> serializeFxChain(const std::vector<PluginSlot>& chain)
{
    juce::Array<juce::var> arr;
    for (const auto& s : chain)
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("name", juce::String(s.name));
        obj->setProperty("format", juce::String(s.format));
        obj->setProperty("fileOrIdentifier", juce::String(s.fileOrIdentifier));
        obj->setProperty("identifierString", juce::String(s.identifierString));
        if (s.bypassed)
            obj->setProperty("bypassed", true);
        if (s.latencySamples > 0)
            obj->setProperty("latencySamples", s.latencySamples);
        if (!s.stateBase64.empty())
            obj->setProperty("stateBase64", juce::String(s.stateBase64));
        arr.add(obj.get());
    }
    return arr;
}

std::vector<PluginSlot> deserializeFxChain(const juce::var& v)
{
    std::vector<PluginSlot> chain;
    if (const auto* arr = v.getArray())
    {
        for (const auto& item : *arr)
        {
            PluginSlot s;
            s.name = item.getProperty("name", "").toString().toStdString();
            s.format = item.getProperty("format", "").toString().toStdString();
            s.fileOrIdentifier = item.getProperty("fileOrIdentifier", "").toString().toStdString();
            s.identifierString = item.getProperty("identifierString", "").toString().toStdString();
            s.bypassed = static_cast<bool>(item.getProperty("bypassed", false));
            s.latencySamples = std::max(0, static_cast<int>(item.getProperty("latencySamples", 0)));
            s.stateBase64 = item.getProperty("stateBase64", "").toString().toStdString();
            if (!s.identifierString.empty() || !s.fileOrIdentifier.empty())
                chain.push_back(std::move(s));
        }
    }
    return chain;
}
} // namespace

bool ProjectSerializer::saveProject(const Song& song, const StretchCache& stretchCache,
                                    const std::filesystem::path& projectDir)
{
    std::error_code ec;
    std::filesystem::create_directories(projectDir / "samples", ec);

    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("version", 2);
    root->setProperty("bpm", song.bpm());
    root->setProperty("swing", song.swing());
    root->setProperty("groovePreset", static_cast<int>(song.groovePreset()));
    root->setProperty("autoCrossfade", song.autoCrossfadeEnabled());
    root->setProperty("envelopeLock", song.envelopeLockEnabled());
    root->setProperty("rippleEdit", song.rippleEditEnabled());
    root->setProperty("pdcEnabled", song.pdcEnabled());
    if (song.projectLatencySamples() > 0)
        root->setProperty("projectLatencySamples", song.projectLatencySamples());
    root->setProperty("masterVolume", song.masterVolume());
    if (song.projectRootNote() >= 0)
        root->setProperty("projectRootNote", song.projectRootNote());
    root->setProperty("timeSigNumerator", song.timeSigNumerator());
    root->setProperty("timeSigDenominator", song.timeSigDenominator());
    root->setProperty("patternZoomX", song.patternZoomX());
    root->setProperty("patternZoomY", song.patternZoomY());
    root->setProperty("arrangerZoomX", song.arrangerZoomX());
    root->setProperty("arrangerZoomY", song.arrangerZoomY());

    // 1. Samples
    juce::Array<juce::var> sampleList;
    for (const auto& s : song.samples())
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("id", static_cast<juce::int64>(s.id.value));
        obj->setProperty("name", juce::String(s.name));
        obj->setProperty("fileName", juce::String(s.fileName));
        obj->setProperty("durationSeconds", s.durationSeconds);
        obj->setProperty("kind", kindToString(s.kind));
        obj->setProperty("loopBars", s.loopBars);
        if (s.loopBeats > 0.0)
            obj->setProperty("loopBeats", s.loopBeats);
        if (s.rootNote >= 0)
            obj->setProperty("rootNote", s.rootNote);
        if (s.acidBpm > 0.0)
            obj->setProperty("acidBpm", s.acidBpm);
        if (s.missing)
            obj->setProperty("missing", true);
        if (!s.stretchMarkers.empty())
        {
            juce::Array<juce::var> markersArr;
            for (double m : s.stretchMarkers)
                markersArr.add(m);
            obj->setProperty("stretchMarkers", markersArr);
        }
        if (!s.author.empty())
            obj->setProperty("author", juce::String(s.author));
        if (!s.copyright.empty())
            obj->setProperty("copyright", juce::String(s.copyright));
        if (!s.comments.empty())
            obj->setProperty("comments", juce::String(s.comments));

        auto raw = stretchCache.getRawAudio(s.id);
        const double rawRate = (raw != nullptr) ? inferAudioSampleRate(*raw, s.durationSeconds) : 44100.0;
        obj->setProperty("sampleRate", rawRate);
        sampleList.add(obj.get());

        // Save raw audio to samples/ folder at its actual in-memory sample rate (and overwrite if an existing file has a mismatched header/duration)
        const auto dest = projectDir / "samples" / s.fileName;
        if (raw != nullptr && raw->numFrames() > 0)
        {
            bool needsWrite = !std::filesystem::exists(dest);
            if (!needsWrite)
            {
                juce::File jExisting(juce::String::fromUTF8(dest.string().c_str()));
                juce::WavAudioFormat wavFmt;
                std::unique_ptr<juce::AudioFormatReader> reader(wavFmt.createReaderFor(jExisting.createInputStream().release(), true));
                if (reader == nullptr ||
                    std::abs(reader->sampleRate - rawRate) > 1.0 ||
                    std::abs(static_cast<std::int64_t>(reader->lengthInSamples) - static_cast<std::int64_t>(raw->numFrames())) > 8)
                {
                    needsWrite = true;
                }
            }
            if (needsWrite)
                writeWavFile(dest, *raw, rawRate);
        }
    }
    root->setProperty("samples", sampleList);

    // 2. Patterns
    juce::Array<juce::var> patternList;
    for (const auto& p : song.patterns())
    {
        juce::DynamicObject::Ptr patObj = new juce::DynamicObject();
        patObj->setProperty("id", static_cast<juce::int64>(p.id.value));
        patObj->setProperty("name", juce::String(p.name));
        patObj->setProperty("lengthBars", p.lengthBars);

        juce::Array<juce::var> trackList;
        for (const auto& t : p.tracks)
        {
            juce::DynamicObject::Ptr trackObj = new juce::DynamicObject();
            if (!t.name.empty())
                trackObj->setProperty("name", juce::String(t.name));
            trackObj->setProperty("volume", t.mix.volume);
            trackObj->setProperty("pan", t.mix.pan);
            trackObj->setProperty("muted", t.muted);
            trackObj->setProperty("solo", t.solo);
            if (t.armed)
                trackObj->setProperty("armed", true);
            if (t.inputChannel != 0)
                trackObj->setProperty("inputChannel", t.inputChannel);
            if (!t.inputStereo)
                trackObj->setProperty("inputStereo", false);
            if (t.fxSend > 0.0f)
                trackObj->setProperty("fxSend", t.fxSend);
            if (t.visibleEnvelope != TrackEnvelopeType::None)
                trackObj->setProperty("visibleEnvelope", static_cast<int>(t.visibleEnvelope));
            if (t.sampleId.isValid())
                trackObj->setProperty("sampleId", static_cast<juce::int64>(t.sampleId.value));
            if (t.kindOverride.has_value())
                trackObj->setProperty("kindOverride", kindToString(*t.kindOverride));
            if (t.colorArgb != 0)
                trackObj->setProperty("colorArgb", static_cast<juce::int64>(t.colorArgb));
            if (t.latencySamples > 0)
                trackObj->setProperty("latencySamples", t.latencySamples);
            if (!t.volumeAutomation.empty())
                trackObj->setProperty("volumeAutomation", serializeEnvelope(t.volumeAutomation));
            if (!t.panAutomation.empty())
                trackObj->setProperty("panAutomation", serializeEnvelope(t.panAutomation));
            if (!t.fxAutomation.empty())
                trackObj->setProperty("fxAutomation", serializeEnvelope(t.fxAutomation));
            if (!t.fxChain.empty())
                trackObj->setProperty("fxChain", serializeFxChain(t.fxChain));

            juce::Array<juce::var> clipList;
            for (const auto& c : t.clips)
            {
                juce::DynamicObject::Ptr clipObj = new juce::DynamicObject();
                clipObj->setProperty("id", static_cast<juce::int64>(c.id.value));
                clipObj->setProperty("sampleId", static_cast<juce::int64>(c.sampleId.value));
                clipObj->setProperty("startStep", c.startStep);
                if (c.lengthSteps > 0.0)
                    clipObj->setProperty("lengthSteps", c.lengthSteps);
                if (c.offsetSteps > 0.0)
                    clipObj->setProperty("offsetSteps", c.offsetSteps);
                if (c.gain != 1.0f)
                    clipObj->setProperty("gain", c.gain);
                if (c.fadeInSteps > 0.0f)
                    clipObj->setProperty("fadeInSteps", c.fadeInSteps);
                if (c.fadeOutSteps > 0.0f)
                    clipObj->setProperty("fadeOutSteps", c.fadeOutSteps);
                if (c.reversed)
                    clipObj->setProperty("reversed", true);
                if (c.transposeSemitones != 0)
                    clipObj->setProperty("transposeSemitones", c.transposeSemitones);
                if (!c.volumeEnvelope.empty())
                    clipObj->setProperty("volumeEnvelope", serializeEnvelope(c.volumeEnvelope));
                if (!c.panEnvelope.empty())
                    clipObj->setProperty("panEnvelope", serializeEnvelope(c.panEnvelope));
                if (!c.fxChain.empty())
                    clipObj->setProperty("fxChain", serializeFxChain(c.fxChain));
                clipList.add(clipObj.get());
            }
            trackObj->setProperty("clips", clipList);
            trackList.add(trackObj.get());
        }
        patObj->setProperty("tracks", trackList);
        if (!p.masterVolumeAutomation.empty())
            patObj->setProperty("masterVolumeAutomation", serializeEnvelope(p.masterVolumeAutomation));
        if (!p.masterFxChain.empty())
            patObj->setProperty("masterFxChain", serializeFxChain(p.masterFxChain));
        if (!p.markers.empty())
        {
            juce::Array<juce::var> markerList;
            for (const auto& m : p.markers)
            {
                juce::DynamicObject::Ptr mObj = new juce::DynamicObject();
                mObj->setProperty("id", m.id);
                mObj->setProperty("step", m.step);
                mObj->setProperty("name", juce::String(m.name));
                markerList.add(mObj.get());
            }
            patObj->setProperty("markers", markerList);
        }
        patternList.add(patObj.get());
    }
    root->setProperty("patterns", patternList);

    // 3. Arrangement
    juce::Array<juce::var> arrangerList;
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        const auto& at = song.arrangement()[static_cast<std::size_t>(t)];
        juce::DynamicObject::Ptr arrTrackObj = new juce::DynamicObject();
        arrTrackObj->setProperty("name", juce::String(at.name));
        if (at.patternId.isValid())
            arrTrackObj->setProperty("patternId", static_cast<juce::int64>(at.patternId.value));
        arrTrackObj->setProperty("volume", at.mix.volume);
        arrTrackObj->setProperty("pan", at.mix.pan);
        arrTrackObj->setProperty("muted", at.muted);
        arrTrackObj->setProperty("solo", at.solo);

        juce::Array<juce::var> blockList;
        for (const auto& b : at.blocks)
        {
            juce::DynamicObject::Ptr blockObj = new juce::DynamicObject();
            blockObj->setProperty("id", static_cast<juce::int64>(b.id.value));
            blockObj->setProperty("patternId", static_cast<juce::int64>(b.patternId.value));
            blockObj->setProperty("startBar", b.startBar);
            blockList.add(blockObj.get());
        }
        arrTrackObj->setProperty("blocks", blockList);
        arrangerList.add(arrTrackObj.get());
    }
    root->setProperty("arrangement", arrangerList);

    const auto jsonString = juce::JSON::toString(juce::var(root.get()));
    const auto projectFile = projectDir / "project.trami";
    juce::File jFile(juce::String::fromUTF8(projectFile.string().c_str()));
    return jFile.replaceWithText(jsonString);
}

std::optional<Song> ProjectSerializer::loadProject(const std::filesystem::path& projectDir,
                                                   StretchCache& stretchCache, SampleImporter& importer,
                                                   double sampleRate)
{
    auto baseDir = projectDir;
    auto projectFile = projectDir / "project.trami";
    if (std::filesystem::is_regular_file(projectDir))
    {
        projectFile = projectDir;
        baseDir = projectDir.parent_path();
    }

    const juce::File jFile(juce::String::fromUTF8(projectFile.string().c_str()));
    if (!jFile.existsAsFile())
        return std::nullopt;

    const auto json = juce::JSON::parse(jFile);
    if (!json.isObject())
        return std::nullopt;

    Song song;
    song.setBpm(json.getProperty("bpm", 120.0));
    if (json.hasProperty("groovePreset"))
    {
        const int gp = static_cast<int>(json.getProperty("groovePreset", 0));
        if (gp >= 0 && gp < static_cast<int>(GroovePreset::Count))
            song.setGroovePreset(static_cast<GroovePreset>(gp));
    }
    song.setSwing(json.getProperty("swing", song.swing()));
    if (json.hasProperty("autoCrossfade"))
        song.setAutoCrossfadeEnabled(static_cast<bool>(json.getProperty("autoCrossfade", true)));
    if (json.hasProperty("envelopeLock"))
        song.setEnvelopeLockEnabled(static_cast<bool>(json.getProperty("envelopeLock", true)));
    if (json.hasProperty("rippleEdit"))
        song.setRippleEditEnabled(static_cast<bool>(json.getProperty("rippleEdit", false)));
    if (json.hasProperty("pdcEnabled"))
        song.setPdcEnabled(static_cast<bool>(json.getProperty("pdcEnabled", true)));
    if (json.hasProperty("projectLatencySamples"))
        song.setProjectLatencySamples(static_cast<int>(json.getProperty("projectLatencySamples", 0)));
    song.setMasterVolume(static_cast<float>(json.getProperty("masterVolume", 1.0f)));
    if (json.hasProperty("projectRootNote"))
        song.setProjectRootNote(static_cast<int>(json.getProperty("projectRootNote", -1)));
    song.setTimeSignature(static_cast<int>(json.getProperty("timeSigNumerator", 4)),
                          static_cast<int>(json.getProperty("timeSigDenominator", 4)));
    stretchCache.clear();

    const float pzx = static_cast<float>(json.getProperty("patternZoomX", 1.0));
    const float pzy = static_cast<float>(json.getProperty("patternZoomY", 1.0));
    song.setPatternZoom(pzx > 0.05f ? pzx : 1.0f, pzy > 0.05f ? pzy : 1.0f);

    const float azx = static_cast<float>(json.getProperty("arrangerZoomX", 1.0));
    const float azy = static_cast<float>(json.getProperty("arrangerZoomY", 1.0));
    song.setArrangerZoom(azx > 0.05f ? azx : 1.0f, azy > 0.05f ? azy : 1.0f);

    std::unordered_map<std::uint32_t, SampleId> sampleIdMap;
    std::unordered_map<std::uint32_t, PatternId> patternIdMap;

    // 1. Load Samples
    if (const auto* samplesVar = json["samples"].getArray())
    {
        for (const auto& sVar : *samplesVar)
        {
            const auto oldId = static_cast<std::uint32_t>(static_cast<juce::int64>(sVar["id"]));
            Sample s;
            s.name = sVar["name"].toString().toStdString();
            s.fileName = sVar["fileName"].toString().toStdString();
            s.durationSeconds = sVar["durationSeconds"];
            s.kind = kindFromString(sVar["kind"].toString());
            s.loopBars = std::max(1, static_cast<int>(sVar["loopBars"]));
            if (sVar.hasProperty("loopBeats"))
                s.loopBeats = static_cast<double>(sVar["loopBeats"]);
            if (sVar.hasProperty("rootNote"))
                s.rootNote = static_cast<int>(sVar["rootNote"]);
            if (sVar.hasProperty("acidBpm"))
                s.acidBpm = static_cast<double>(sVar["acidBpm"]);
            if (sVar.hasProperty("missing"))
                s.missing = static_cast<bool>(sVar["missing"]);
            if (const auto* smArr = sVar["stretchMarkers"].getArray())
            {
                for (const auto& mv : *smArr)
                    s.stretchMarkers.push_back(static_cast<double>(mv));
            }
            if (sVar.hasProperty("author"))
                s.author = sVar["author"].toString().toStdString();
            if (sVar.hasProperty("copyright"))
                s.copyright = sVar["copyright"].toString().toStdString();
            if (sVar.hasProperty("comments"))
                s.comments = sVar["comments"].toString().toStdString();

            // Load decoded audio into stretchCache (check samples/ subfolder first, then project directory)
            std::filesystem::path sampleFilePath = baseDir / "samples" / s.fileName;
            std::error_code ec;
            if (!std::filesystem::exists(sampleFilePath, ec))
            {
                const auto altPath = baseDir / s.fileName;
                if (std::filesystem::exists(altPath, ec))
                    sampleFilePath = altPath;
            }

            std::optional<AudioData> audioOpt;

            // Detect and auto-repair legacy .trami projects where 48kHz (or device-rate) samples were saved with a 44.1kHz WAV header
            if (s.durationSeconds > 0.05 && std::filesystem::exists(sampleFilePath, ec))
            {
                juce::File jSampleFile(juce::String::fromUTF8(sampleFilePath.string().c_str()));
                juce::WavAudioFormat wavFmt;
                std::unique_ptr<juce::AudioFormatReader> reader(
                    wavFmt.createReaderFor(jSampleFile.createInputStream().release(), true));
                if (reader != nullptr && reader->lengthInSamples > 0 && reader->sampleRate > 0.0)
                {
                    const double headerDur = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
                    const double impliedRate = static_cast<double>(reader->lengthInSamples) / s.durationSeconds;
                    double snappedImpliedRate = impliedRate;
                    for (double stdRate : {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0})
                    {
                        if (std::abs(impliedRate - stdRate) < stdRate * 0.01)
                        {
                            snappedImpliedRate = stdRate;
                            break;
                        }
                    }

                    if (std::abs(headerDur - s.durationSeconds) > s.durationSeconds * 0.03 &&
                        std::abs(snappedImpliedRate - reader->sampleRate) > 1000.0 &&
                        std::abs(static_cast<double>(reader->lengthInSamples) / snappedImpliedRate - s.durationSeconds) <
                            s.durationSeconds * 0.015)
                    {
                        // Check if an untouched original WAV exists in baseDir / s.fileName
                        const auto origInParent = baseDir / s.fileName;
                        if (origInParent != sampleFilePath && std::filesystem::exists(origInParent, ec))
                        {
                            audioOpt = importer.loadAudioData(origInParent, sampleRate);
                        }

                        if (!audioOpt.has_value())
                        {
                            // Read the raw frames at their true implied sample rate (snappedImpliedRate) without double-resampling
                            const int numCh = std::min(2, static_cast<int>(reader->numChannels));
                            const int srcLen = static_cast<int>(reader->lengthInSamples);
                            juce::AudioBuffer<float> tempBuf(numCh, srcLen);
                            reader->read(&tempBuf, 0, srcLen, 0, true, numCh > 1);
                            reader.reset();

                            AudioData recovered;
                            recovered.channels.resize(static_cast<std::size_t>(numCh));
                            if (std::abs(snappedImpliedRate - sampleRate) < 1.0)
                            {
                                for (int ch = 0; ch < numCh; ++ch)
                                {
                                    const float* src = tempBuf.getReadPointer(ch);
                                    recovered.channels[static_cast<std::size_t>(ch)].assign(src, src + srcLen);
                                }
                            }
                            else
                            {
                                const double ratio = snappedImpliedRate / sampleRate;
                                const int destLen = static_cast<int>(std::round(static_cast<double>(srcLen) / ratio));
                                for (int ch = 0; ch < numCh; ++ch)
                                {
                                    auto& dest = recovered.channels[static_cast<std::size_t>(ch)];
                                    dest.resize(static_cast<std::size_t>(destLen));
                                    juce::LagrangeInterpolator interp;
                                    interp.process(ratio, tempBuf.getReadPointer(ch), dest.data(), destLen);
                                }
                            }
                            audioOpt = std::move(recovered);
                        }
                        else
                        {
                            reader.reset();
                        }

                        // Permanently repair the WAV header on disk in samples/
                        if (audioOpt.has_value())
                            writeWavFile(sampleFilePath, *audioOpt, sampleRate);
                    }
                }
            }

            if (!audioOpt.has_value())
                audioOpt = importer.loadAudioData(sampleFilePath, sampleRate);
            s.missing = !audioOpt.has_value();

            const auto newId = song.addSample(s);
            sampleIdMap[oldId] = newId;

            if (audioOpt.has_value())
                stretchCache.setRawAudio(newId, std::make_shared<const AudioData>(std::move(*audioOpt)));
        }
    }

    // 2. Load Patterns
    if (const auto* patternsVar = json["patterns"].getArray())
    {
        for (const auto& pVar : *patternsVar)
        {
            const auto oldPatId = static_cast<std::uint32_t>(static_cast<juce::int64>(pVar["id"]));
            const auto name = pVar["name"].toString().toStdString();
            const int lengthBars = std::max(1, static_cast<int>(pVar["lengthBars"]));

            const auto newPatId = song.addPattern(name, lengthBars);
            patternIdMap[oldPatId] = newPatId;

            if (const auto* tracksVar = pVar["tracks"].getArray())
            {
                int trackIdx = 0;
                for (const auto& tVar : *tracksVar)
                {
                    if (trackIdx >= kTracksPerPattern)
                        break;

                    if (Pattern* pat = song.findPattern(newPatId))
                    {
                        if (trackIdx >= static_cast<int>(pat->tracks.size()))
                            pat->tracks.resize(static_cast<std::size_t>(trackIdx + 1));
                        auto& trk = pat->tracks[static_cast<std::size_t>(trackIdx)];
                        if (tVar.hasProperty("name"))
                            trk.name = tVar["name"].toString().toStdString();
                        trk.mix.volume = static_cast<float>(tVar.getProperty("volume", 1.0f));
                        trk.mix.pan = static_cast<float>(tVar.getProperty("pan", 0.0f));
                        trk.muted = tVar.getProperty("muted", false);
                        trk.solo = tVar.getProperty("solo", false);
                        trk.armed = tVar.getProperty("armed", false);
                        if (tVar.hasProperty("inputChannel"))
                            trk.inputChannel = std::clamp(static_cast<int>(tVar.getProperty("inputChannel", 0)), 0, 31);
                        if (tVar.hasProperty("inputStereo"))
                            trk.inputStereo = static_cast<bool>(tVar.getProperty("inputStereo", true));
                        trk.fxSend = static_cast<float>(tVar.getProperty("fxSend", 0.0f));
                        if (tVar.hasProperty("visibleEnvelope"))
                        {
                            const int ve = static_cast<int>(tVar.getProperty("visibleEnvelope", 0));
                            if (ve == 1) trk.visibleEnvelope = TrackEnvelopeType::Volume;
                            else if (ve == 2) trk.visibleEnvelope = TrackEnvelopeType::Pan;
                            else if (ve == 3) trk.visibleEnvelope = TrackEnvelopeType::FxSend;
                        }
                        if (tVar.hasProperty("sampleId"))
                        {
                            const auto oldSid = static_cast<std::uint32_t>(static_cast<juce::int64>(tVar["sampleId"]));
                            if (sampleIdMap.contains(oldSid))
                                trk.sampleId = sampleIdMap[oldSid];
                        }
                        if (tVar.hasProperty("kindOverride"))
                            trk.kindOverride = kindFromString(tVar["kindOverride"].toString());
                        if (tVar.hasProperty("colorArgb"))
                            trk.colorArgb = static_cast<std::uint32_t>(static_cast<juce::int64>(tVar["colorArgb"]));
                        if (tVar.hasProperty("latencySamples"))
                            trk.latencySamples = std::max(0, static_cast<int>(tVar["latencySamples"]));
                        if (tVar.hasProperty("volumeAutomation"))
                            trk.volumeAutomation = deserializeEnvelope(tVar["volumeAutomation"]);
                        if (tVar.hasProperty("panAutomation"))
                            trk.panAutomation = deserializeEnvelope(tVar["panAutomation"]);
                        if (tVar.hasProperty("fxAutomation"))
                            trk.fxAutomation = deserializeEnvelope(tVar["fxAutomation"]);
                        if (tVar.hasProperty("fxChain"))
                            trk.fxChain = deserializeFxChain(tVar["fxChain"]);
                    }

                    if (const auto* clipsVar = tVar["clips"].getArray())
                    {
                        for (const auto& cVar : *clipsVar)
                        {
                            const auto oldSampleId =
                                static_cast<std::uint32_t>(static_cast<juce::int64>(cVar["sampleId"]));
                            const double startStep = static_cast<double>(cVar.getProperty("startStep", 0.0));
                            const double lengthSteps = static_cast<double>(cVar.getProperty("lengthSteps", 0.0));
                            const double offsetSteps = static_cast<double>(cVar.getProperty("offsetSteps", 0.0));

                            if (sampleIdMap.contains(oldSampleId))
                            {
                                if (auto placedClipId = song.placeClip(newPatId, trackIdx, sampleIdMap[oldSampleId], startStep, lengthSteps, offsetSteps))
                                {
                                    // Set reversed flag BEFORE envelopes/fades so loading does not re-flip serialized curves
                                    if (static_cast<bool>(cVar.getProperty("reversed", false)))
                                        song.setClipReversed(newPatId, *placedClipId, true);
                                    const float gain = static_cast<float>(cVar.getProperty("gain", 1.0f));
                                    const float fadeIn = static_cast<float>(cVar.getProperty("fadeInSteps", 0.0f));
                                    const float fadeOut = static_cast<float>(cVar.getProperty("fadeOutSteps", 0.0f));
                                    if (gain != 1.0f || fadeIn > 0.0f || fadeOut > 0.0f)
                                        song.setClipEnvelope(newPatId, *placedClipId, gain, fadeIn, fadeOut);
                                    const int transpose = static_cast<int>(cVar.getProperty("transposeSemitones", 0));
                                    if (transpose != 0)
                                        song.setClipTranspose(newPatId, *placedClipId, transpose);
                                    if (cVar.hasProperty("volumeEnvelope"))
                                        song.setClipVolumeEnvelope(newPatId, *placedClipId, deserializeEnvelope(cVar["volumeEnvelope"]));
                                    if (cVar.hasProperty("panEnvelope"))
                                        song.setClipPanEnvelope(newPatId, *placedClipId, deserializeEnvelope(cVar["panEnvelope"]));
                                    if (cVar.hasProperty("fxChain"))
                                        song.setClipFxChain(newPatId, *placedClipId, deserializeFxChain(cVar["fxChain"]));
                                }
                            }
                        }
                    }
                    ++trackIdx;
                }
            }

            if (Pattern* pat = song.findPattern(newPatId))
            {
                if (pVar.hasProperty("masterVolumeAutomation"))
                    pat->masterVolumeAutomation = deserializeEnvelope(pVar["masterVolumeAutomation"]);
                if (pVar.hasProperty("masterFxChain"))
                    pat->masterFxChain = deserializeFxChain(pVar["masterFxChain"]);
            }

            if (const auto* markersVar = pVar["markers"].getArray())
            {
                for (const auto& mVar : *markersVar)
                {
                    const double step = static_cast<double>(mVar.getProperty("step", 0.0));
                    const std::string mName = mVar.getProperty("name", "Marker").toString().toStdString();
                    song.addMarker(newPatId, step, mName);
                }
            }
        }
    }

    // 3. Load Arrangement
    if (const auto* arrangerVar = json["arrangement"].getArray())
    {
        int arrTrackIdx = 0;
        for (const auto& atVar : *arrangerVar)
        {
            if (arrTrackIdx >= kArrangerTracks)
                break;

            if (auto* track = song.arrangerTrack(arrTrackIdx))
            {
                track->name = atVar.getProperty("name", "").toString().toStdString();
                if (atVar.hasProperty("patternId"))
                    track->patternId = PatternId{static_cast<std::uint32_t>(static_cast<juce::int64>(atVar.getProperty("patternId", 0)))};
                track->mix.volume = static_cast<float>(atVar.getProperty("volume", 1.0f));
                track->mix.pan = static_cast<float>(atVar.getProperty("pan", 0.0f));
                track->muted = atVar.getProperty("muted", false);
                track->solo = atVar.getProperty("solo", false);
            }

            if (const auto* blocksVar = atVar["blocks"].getArray())
            {
                for (const auto& bVar : *blocksVar)
                {
                    const auto oldPatId = static_cast<std::uint32_t>(static_cast<juce::int64>(bVar["patternId"]));
                    const int startBar = bVar["startBar"];

                    if (patternIdMap.contains(oldPatId))
                    {
                        song.placeBlock(arrTrackIdx, patternIdMap[oldPatId], startBar);
                    }
                }
            }
            ++arrTrackIdx;
        }
    }

    return song;
}

} // namespace trami
