#include "app/MainWindow.h"
#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "model/Song.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace trami
{

namespace
{

void populateScreenshotDemoSong(Song& song, StretchCache& stretchCache)
{
    song.setBpm(124.0);
    song.setSwing(0.18);
    song.setProjectRootNote(60); // Key: C
    const auto patId = song.patterns().empty() ? song.addPattern("Neon Horizon", 8) : song.patterns().front().id;
    song.renamePattern(patId, "Neon Horizon");
    song.setPatternLength(patId, 8);

    struct DemoTrackInfo
    {
        const char* name;
        std::uint32_t color;
        double freq;
        int bars;
        float vol;
        float pan;
    };
    static constexpr DemoTrackInfo kTracks[] = {
        {"Breakbeat Drums", 0xff22c55e, 110.0, 2, 0.95f, 0.0f},
        {"Analog Sub Bass", 0xff38bdf8, 55.0,  2, 0.88f, -0.10f},
        {"Warm Rhodes Chords", 0xffa855f7, 220.0, 4, 0.82f, 0.18f},
        {"Pluck Synth Arp", 0xfff59e0b, 440.0, 2, 0.78f, -0.22f}
    };

    for (int t = 0; t < 4; ++t)
    {
        if (t > 0)
            song.addTrackToPattern(patId);

        Sample smp;
        smp.name = kTracks[t].name;
        smp.fileName = std::string(kTracks[t].name) + ".wav";
        smp.kind = SampleKind::Loop;
        smp.loopBars = kTracks[t].bars;
        smp.loopBeats = static_cast<double>(kTracks[t].bars * 4);
        smp.durationSeconds = smp.loopBeats * (60.0 / 124.0);
        smp.acidBpm = 124.0;
        smp.rootNote = (t == 0) ? -1 : 60;
        const auto sid = song.addSample(std::move(smp));

        // Synthesize realistic-looking transient waveform for screenshot
        const int frames = 44100 * kTracks[t].bars * 2;
        AudioData audio;
        audio.channels.resize(2, std::vector<float>(static_cast<std::size_t>(frames), 0.0f));
        for (int i = 0; i < frames; ++i)
        {
            const double pos = static_cast<double>(i) / static_cast<double>(frames);
            const double beatPhase = std::fmod(pos * (kTracks[t].bars * 4 * (t == 3 ? 2 : 1)), 1.0);
            const double env = std::exp(-beatPhase * (t == 2 ? 2.2 : 5.5));
            const float wave = static_cast<float>(
                env * (0.65 * std::sin(2.0 * 3.14159265 * kTracks[t].freq * i / 44100.0) +
                       0.35 * std::sin(2.0 * 3.14159265 * (kTracks[t].freq * 2.5) * i / 44100.0)));
            audio.channels[0][static_cast<std::size_t>(i)] = wave;
            audio.channels[1][static_cast<std::size_t>(i)] = wave * 0.92f;
        }
        stretchCache.setRawAudio(sid, std::make_shared<const AudioData>(std::move(audio)));

        song.setPatternTrackSample(patId, t, sid);
        song.renamePatternTrack(patId, t, kTracks[t].name);
        song.setPatternTrackColor(patId, t, kTracks[t].color);
        if (auto* pat = song.findPattern(patId))
        {
            pat->tracks[static_cast<std::size_t>(t)].mix.volume = kTracks[t].vol;
            pat->tracks[static_cast<std::size_t>(t)].mix.pan = kTracks[t].pan;
        }
    }

    if (auto* pat = song.findPattern(patId))
    {
        song.placeClip(patId, 0, pat->tracks[0].sampleId, 0.0, 128.0);
        song.placeClip(patId, 0, pat->tracks[0].sampleId, 160.0, 96.0);
        song.placeClip(patId, 1, pat->tracks[1].sampleId, 32.0, 224.0);
        if (auto cid = song.placeClip(patId, 2, pat->tracks[2].sampleId, 0.0, 256.0))
        {
            song.setClipEnvelope(patId, *cid, 1.0f, 16.0f, 24.0f);
            song.setClipVolumeEnvelope(patId, *cid, {
                {0.0, 0.6f, EnvelopeCurve::Linear},
                {0.25, 1.15f, EnvelopeCurve::Fast},
                {0.65, 0.85f, EnvelopeCurve::Linear},
                {1.0, 1.05f, EnvelopeCurve::Slow}
            });
        }
        song.placeClip(patId, 3, pat->tracks[3].sampleId, 64.0, 64.0);
        song.placeClip(patId, 3, pat->tracks[3].sampleId, 160.0, 96.0);

        song.addMarker(patId, 0.0, "Intro");
        song.addMarker(patId, 64.0, "Drop");
        song.addMarker(patId, 160.0, "Break");

        song.setMasterAutomation(patId, {
            {0.0, 0.75f, EnvelopeCurve::Linear},
            {64.0, 1.0f, EnvelopeCurve::Linear},
            {160.0, 0.88f, EnvelopeCurve::Linear},
            {192.0, 1.08f, EnvelopeCurve::Linear},
            {256.0, 1.0f, EnvelopeCurve::Linear}
        });
    }
}

} // namespace

class TramiApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

        const auto args = juce::StringArray::fromTokens(commandLine, true);
        juce::String screenshotPath;
        for (int i = 0; i + 1 < args.size(); ++i)
        {
            if (args[i] == "--screenshot")
                screenshotPath = args[i + 1].unquoted();
        }

        engine_ = std::make_unique<AudioEngine>();
        if (screenshotPath.isEmpty())
        {
            if (const auto error = engine_->start(); error.isNotEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Audio device warning",
                                                       error + "\nYou can configure audio devices in the Audio settings.");
            }
        }

        // Start with an empty project: 1 default pattern and 1 dedicated arranger track
        const auto defaultPattern = song_.addPattern("Unnamed Song", 1);
        song_.setArrangerTrackPattern(0, defaultPattern);

        if (screenshotPath.isNotEmpty())
        {
            const juce::File demoRoot("/tmp/TramiLoop Samples");
            demoRoot.deleteRecursively();
            for (const char* folder : {"Basslines", "Chords & Pads", "Drums & Breaks", "FX & Risers", "Synthesizers", "Vintage Analog"})
                demoRoot.getChildFile(folder).createDirectory();
            static constexpr const char* kDemoFiles[] = {
                "808 Sub Groove.wav", "Acid 303 Squelch.wav", "Analog Sub Bass.wav",
                "Arp Come Up 1.wav", "Breakbeat Drums 124.wav", "Crispy Hat Loop.wav",
                "Deep House Chord.wav", "Dub Delay Rimshot.wav", "Filtered Disco Loop.wav",
                "Glitch Perc Loop.wav", "Juno Lush Pad.wav", "Modular Pluck.wav",
                "Neon Horizon Lead.wav", "Pluck Synth Arp.wav", "Punchy Kick Loop.wav",
                "Tape Saturation Bed.wav", "Vintage Rhodes 124.wav", "Warm Rhodes Chords.wav"
            };
            for (const char* fn : kDemoFiles)
            {
                const auto f = demoRoot.getChildFile(fn);
                juce::FileOutputStream out(f);
                if (out.openedOk())
                {
                    // Write minimal valid 16-bit stereo 44.1kHz WAV with a rhythmic waveform
                    const int numSamples = 22050;
                    const int dataBytes = numSamples * 4;
                    out.write("RIFF", 4);
                    out.writeInt(36 + dataBytes);
                    out.write("WAVEfmt ", 8);
                    out.writeInt(16);
                    out.writeShort(1);
                    out.writeShort(2);
                    out.writeInt(44100);
                    out.writeInt(44100 * 4);
                    out.writeShort(4);
                    out.writeShort(16);
                    out.write("data", 4);
                    out.writeInt(dataBytes);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const double p = static_cast<double>(i) / numSamples;
                        const double env = std::exp(-std::fmod(p * 8.0, 1.0) * 4.5);
                        const short s = static_cast<short>(env * std::sin(i * 0.08) * 26000.0);
                        out.writeShort(s);
                        out.writeShort(s);
                    }
                }
            }
            populateScreenshotDemoSong(song_, stretchCache_);
        }

        mainWindow_ = std::make_unique<MainWindow>(
            getApplicationName() + " v" + getApplicationVersion(), *engine_, song_, stretchCache_);

        if (screenshotPath.isNotEmpty())
        {
            juce::Timer::callAfterDelay(550, [this, screenshotPath]()
            {
                if (mainWindow_ != nullptr && mainWindow_->getContentComponent() != nullptr)
                {
                    auto* content = mainWindow_->getContentComponent();
                    const auto img = content->createComponentSnapshot(content->getLocalBounds(), true, 2.0f);
                    const juce::File outFile(screenshotPath);
                    outFile.getParentDirectory().createDirectory();
                    outFile.deleteFile();
                    juce::FileOutputStream stream(outFile);
                    if (stream.openedOk())
                    {
                        juce::PNGImageFormat png;
                        png.writeImageToStream(img, stream);
                    }
                }
                juce::File("/tmp/TramiLoop Samples").deleteRecursively();
                quit();
            });
        }
    }

    void shutdown() override
    {
        mainWindow_.reset();
        stretchCache_.clear();
        engine_.reset();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    TramiLookAndFeel lookAndFeel_;
    Song song_;
    StretchCache stretchCache_;
    std::unique_ptr<AudioEngine> engine_;
    std::unique_ptr<MainWindow> mainWindow_;
};

} // namespace trami

START_JUCE_APPLICATION(trami::TramiApplication)
