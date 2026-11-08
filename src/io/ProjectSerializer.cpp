#include "io/ProjectSerializer.h"

#include <juce_core/juce_core.h>

namespace trami
{

bool ProjectSerializer::saveProject(const Song& song, const StretchCache& stretchCache,
                                    const std::filesystem::path& projectDir)
{
    std::error_code ec;
    std::filesystem::create_directories(projectDir / "samples", ec);

    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("version", 1);
    root->setProperty("bpm", song.bpm());
    root->setProperty("swing", song.swing());
    root->setProperty("masterVolume", song.masterVolume());

    // 1. Samples
    juce::Array<juce::var> sampleList;
    for (const auto& s : song.samples())
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("id", static_cast<juce::int64>(s.id.value));
        obj->setProperty("name", juce::String(s.name));
        obj->setProperty("fileName", juce::String(s.fileName));
        obj->setProperty("durationSeconds", s.durationSeconds);
        obj->setProperty("kind", s.kind == SampleKind::Loop ? "loop" : "oneshot");
        obj->setProperty("loopBars", s.loopBars);
        sampleList.add(obj.get());

        // Save raw audio to samples/ folder if not already there
        const auto dest = projectDir / "samples" / s.fileName;
        if (!std::filesystem::exists(dest))
        {
            if (auto raw = stretchCache.getRawAudio(s.id))
            {
                juce::File jDest(juce::String::fromUTF8(dest.string().c_str()));
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(jDest);
                auto writer = wav.createWriterFor(
                    stream, juce::AudioFormatWriterOptions{}
                                .withSampleRate(44100.0)
                                .withNumChannels(raw->numChannels())
                                .withBitsPerSample(16));
                if (writer != nullptr)
                {
                    juce::AudioBuffer<float> temp(raw->numChannels(), static_cast<int>(raw->numFrames()));
                    for (int ch = 0; ch < raw->numChannels(); ++ch)
                        temp.copyFrom(ch, 0, raw->channels[static_cast<std::size_t>(ch)].data(),
                                      static_cast<int>(raw->numFrames()));
                    writer->writeFromAudioSampleBuffer(temp, 0, static_cast<int>(raw->numFrames()));
                }
            }
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
            trackObj->setProperty("volume", t.mix.volume);
            trackObj->setProperty("pan", t.mix.pan);
            trackObj->setProperty("muted", t.muted);
            trackObj->setProperty("solo", t.solo);
            if (t.sampleId.isValid())
                trackObj->setProperty("sampleId", static_cast<juce::int64>(t.sampleId.value));

            juce::Array<juce::var> clipList;
            for (const auto& c : t.clips)
            {
                juce::DynamicObject::Ptr clipObj = new juce::DynamicObject();
                clipObj->setProperty("id", static_cast<juce::int64>(c.id.value));
                clipObj->setProperty("sampleId", static_cast<juce::int64>(c.sampleId.value));
                clipObj->setProperty("startStep", c.startStep);
                if (c.lengthSteps > 0)
                    clipObj->setProperty("lengthSteps", c.lengthSteps);
                if (c.offsetSteps > 0)
                    clipObj->setProperty("offsetSteps", c.offsetSteps);
                clipList.add(clipObj.get());
            }
            trackObj->setProperty("clips", clipList);
            trackList.add(trackObj.get());
        }
        patObj->setProperty("tracks", trackList);
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
    const auto projectFile = projectDir / "project.trami";
    const juce::File jFile(juce::String::fromUTF8(projectFile.string().c_str()));
    if (!jFile.existsAsFile())
        return std::nullopt;

    const auto json = juce::JSON::parse(jFile);
    if (!json.isObject())
        return std::nullopt;

    Song song;
    song.setBpm(json.getProperty("bpm", 120.0));
    song.setSwing(json.getProperty("swing", 0.0));
    song.setMasterVolume(static_cast<float>(json.getProperty("masterVolume", 1.0f)));

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
            s.kind = (sVar["kind"].toString() == "loop") ? SampleKind::Loop : SampleKind::OneShot;
            s.loopBars = std::max(1, static_cast<int>(sVar["loopBars"]));

            const auto newId = song.addSample(s);
            sampleIdMap[oldId] = newId;

            // Load decoded audio into stretchCache
            const auto sampleFilePath = projectDir / "samples" / s.fileName;
            if (auto audioOpt = importer.loadAudioData(sampleFilePath, sampleRate))
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
                        auto& trk = pat->tracks[static_cast<std::size_t>(trackIdx)];
                        trk.mix.volume = static_cast<float>(tVar.getProperty("volume", 1.0f));
                        trk.mix.pan = static_cast<float>(tVar.getProperty("pan", 0.0f));
                        trk.muted = tVar.getProperty("muted", false);
                        trk.solo = tVar.getProperty("solo", false);
                        if (tVar.hasProperty("sampleId"))
                        {
                            const auto oldSid = static_cast<std::uint32_t>(static_cast<juce::int64>(tVar["sampleId"]));
                            if (sampleIdMap.contains(oldSid))
                                trk.sampleId = sampleIdMap[oldSid];
                        }
                    }

                    if (const auto* clipsVar = tVar["clips"].getArray())
                    {
                        for (const auto& cVar : *clipsVar)
                        {
                            const auto oldSampleId =
                                static_cast<std::uint32_t>(static_cast<juce::int64>(cVar["sampleId"]));
                            const int startStep = cVar["startStep"];
                            const int lengthSteps = static_cast<int>(cVar.getProperty("lengthSteps", 0));
                            const int offsetSteps = static_cast<int>(cVar.getProperty("offsetSteps", 0));

                            if (sampleIdMap.contains(oldSampleId))
                            {
                                song.placeClip(newPatId, trackIdx, sampleIdMap[oldSampleId], startStep, lengthSteps, offsetSteps);
                            }
                        }
                    }
                    ++trackIdx;
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
