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

        const auto pattern = song_.addPattern("Pattern 1", 1);
        song_.setPatternTrackSample(pattern, 0, kick);
        song_.setPatternTrackSample(pattern, 1, snare);
        song_.setPatternTrackSample(pattern, 2, hihat);
        song_.setPatternTrackSample(pattern, 3, bass);

        for (int step : {0, 6, 8, 11})
            song_.placeClip(pattern, 0, kick, step);
        for (int step : {4, 12})
            song_.placeClip(pattern, 1, snare, step);
        for (int step = 0; step < kStepsPerBar; step += 2)
            song_.placeClip(pattern, 2, hihat, step);
        song_.placeClip(pattern, 3, bass, 0);

        song_.placeBlock(0, pattern, 0);
    }

    TramiLookAndFeel lookAndFeel_;
    Song song_;
    StretchCache stretchCache_;
    std::unique_ptr<AudioEngine> engine_;
    std::unique_ptr<MainWindow> mainWindow_;
};

} // namespace trami

START_JUCE_APPLICATION(trami::TramiApplication)
