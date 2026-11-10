#include "app/MainWindow.h"
#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "engine/Synth.h"
#include "model/Song.h"
#include "model/Timing.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class TramiApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

        engine_ = std::make_unique<AudioEngine>();
        if (const auto error = engine_->start(); error.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Audio device error", error);
            return;
        }

        initDefaultContent();
        mainWindow_ = std::make_unique<MainWindow>(getApplicationName(), *engine_, song_, stretchCache_);
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
    void initDefaultContent()
    {
        const double rate = engine_->sampleRate();

        auto addSample = [this, rate](const char* name, const char* fileName, AudioData audio, SampleKind kind, int loopBars) {
            Sample s;
            s.name = name;
            s.fileName = fileName;
            s.kind = kind;
            s.loopBars = loopBars;
            s.durationSeconds = static_cast<double>(audio.numFrames()) / rate;
            const auto id = song_.addSample(s);
            stretchCache_.setRawAudio(id, std::make_shared<const AudioData>(std::move(audio)));
            return id;
        };

        const auto kick = addSample("Kick", "Kick.wav", synth::kick(rate), SampleKind::OneShot, 1);
        const auto snare = addSample("Snare", "Snare.wav", synth::snare(rate), SampleKind::OneShot, 1);
        const auto hihat = addSample("Hihat", "Hihat.wav", synth::hihat(rate), SampleKind::OneShot, 1);
        const auto bass = addSample("Bass Loop", "Bass_Loop.wav", synth::bassLoop(rate), SampleKind::Loop, 1);

        // Pattern 1: "Main Beat" - 4-track drum & bass pattern (1 bar)
        const auto patBeat = song_.addPattern("Main Beat", 1);
        song_.setPatternTrackSample(patBeat, 0, kick);
        for (int step : {0, 8, 16, 24})
            song_.placeClip(patBeat, 0, kick, step);

        const int snareTrk = song_.addTrackToPattern(patBeat, snare);
        song_.placeClip(patBeat, snareTrk, snare, 8);
        song_.placeClip(patBeat, snareTrk, snare, 24);

        const int hihatTrk = song_.addTrackToPattern(patBeat, hihat);
        for (int step : {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30})
            song_.placeClip(patBeat, hihatTrk, hihat, step);

        const int bassTrk = song_.addTrackToPattern(patBeat, bass);
        song_.placeClip(patBeat, bassTrk, bass, 0);

        // Pattern 2: "Groove" - 2-track pattern
        const auto patGroove = song_.addPattern("Groove", 1);
        song_.setPatternTrackSample(patGroove, 0, hihat);
        for (int step : {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30})
            song_.placeClip(patGroove, 0, hihat, step);

        const int grooveBass = song_.addTrackToPattern(patGroove, bass);
        song_.placeClip(patGroove, grooveBass, bass, 0);

        // Pattern 3: "Drums Only" - 2-track pattern
        const auto patDrums = song_.addPattern("Drums Only", 1);
        song_.setPatternTrackSample(patDrums, 0, kick);
        for (int step : {0, 8, 16, 24})
            song_.placeClip(patDrums, 0, kick, step);
        const int drumSnare = song_.addTrackToPattern(patDrums, snare);
        song_.placeClip(patDrums, drumSnare, snare, 8);
        song_.placeClip(patDrums, drumSnare, snare, 24);

        // Pre-populate arrangement blocks on their dedicated arranger tracks
        // Track 0: Main Beat, Track 1: Groove, Track 2: Drums Only
        song_.setArrangerTrackPattern(0, patBeat);
        song_.setArrangerTrackPattern(1, patGroove);
        song_.setArrangerTrackPattern(2, patDrums);
        song_.placeBlock(1, patGroove, 0);
        song_.placeBlock(0, patBeat, 1);
        song_.placeBlock(0, patBeat, 2);
        song_.placeBlock(2, patDrums, 3);
    }

    TramiLookAndFeel lookAndFeel_;
    Song song_;
    StretchCache stretchCache_;
    std::unique_ptr<AudioEngine> engine_;
    std::unique_ptr<MainWindow> mainWindow_;
};

} // namespace trami

START_JUCE_APPLICATION(trami::TramiApplication)
