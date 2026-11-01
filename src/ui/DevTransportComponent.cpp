#include "ui/DevTransportComponent.h"

#include "engine/Synth.h"
#include "model/Timing.h"
#include "ui/TramiLookAndFeel.h"

namespace trami
{

DevTransportComponent::DevTransportComponent(AudioEngine& engine) : engine_(engine)
{
    setSize(1280, 800);
    setWantsKeyboardFocus(true);
    buildDemoSong();
    updateBankAndSnapshot();
    startTimerHz(30);
}

void DevTransportComponent::buildDemoSong()
{
    const double rate = engine_.sampleRate();

    auto addOneShot = [&](const char* name, AudioData audio) {
        Sample s;
        s.name = name;
        s.kind = SampleKind::OneShot;
        s.durationSeconds = static_cast<double>(audio.numFrames()) / rate;
        const auto id = song_.addSample(s);
        stretchCache_.setRawAudio(id, std::make_shared<const AudioData>(std::move(audio)));
        return id;
    };

    auto addLoop = [&](const char* name, AudioData audio, int loopBars) {
        Sample s;
        s.name = name;
        s.kind = SampleKind::Loop;
        s.loopBars = loopBars;
        s.durationSeconds = static_cast<double>(audio.numFrames()) / rate;
        const auto id = song_.addSample(s);
        stretchCache_.setRawAudio(id, std::make_shared<const AudioData>(std::move(audio)));
        return id;
    };

    const auto kick = addOneShot("Kick", synth::kick(rate));
    const auto snare = addOneShot("Snare", synth::snare(rate));
    const auto hihat = addOneShot("Hihat", synth::hihat(rate));
    const auto bass = addLoop("Bass Loop (1 bar)", synth::bassLoop(rate), 1);

    pattern_ = song_.addPattern("Demo", 1);

    // Drum one-shots
    for (int step : {0, 6, 8, 11})
        song_.placeClip(pattern_, 0, kick, step);
    for (int step : {4, 12})
        song_.placeClip(pattern_, 1, snare, step);
    for (int step = 0; step < kStepsPerBar; step += 2)
        song_.placeClip(pattern_, 2, hihat, step);

    // 1-bar time-stretched loop on track 3
    song_.placeClip(pattern_, 3, bass, 0);
}

void DevTransportComponent::updateBankAndSnapshot()
{
    isStretching_ = true;
    repaint();

    stretchCache_.requestUpdateAsync(song_, engine_.sampleRate(), [this](SampleBank newBank) {
        juce::MessageManager::callAsync([this, bank = std::move(newBank)]() mutable {
            activeBank_ = std::move(bank);
            engine_.publish(SnapshotBuilder::buildPattern(song_, pattern_, activeBank_, engine_.sampleRate()));
            isStretching_ = false;
            repaint();
        });
    });
}

bool DevTransportComponent::keyPressed(const juce::KeyPress& key)
{
    auto& seq = engine_.sequencer();

    if (key == juce::KeyPress::spaceKey)
        seq.requestPlaying(!seq.isPlaying());
    else if (key == juce::KeyPress::returnKey)
        seq.requestRewind();
    else if (key.getTextCharacter() == 'm')
        seq.setMetronomeEnabled(!seq.isMetronomeEnabled());
    else if (key.getTextCharacter() == 's')
    {
        song_.setSwing(song_.swing() > 0.0 ? 0.0 : 0.5);
        updateBankAndSnapshot();
    }
    else if (key == juce::KeyPress::upKey)
    {
        song_.setBpm(song_.bpm() + 5.0);
        updateBankAndSnapshot();
    }
    else if (key == juce::KeyPress::downKey)
    {
        song_.setBpm(song_.bpm() - 5.0);
        updateBankAndSnapshot();
    }
    else
        return false;

    repaint();
    return true;
}

void DevTransportComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    auto& seq = engine_.sequencer();
    const juce::String stretchStatus = isStretching_ ? "  [stretching...]" : "";
    const juce::String status = juce::String(seq.isPlaying() ? "Playing" : "Stopped") + "   |   " +
                                juce::String(song_.bpm(), 0) + " BPM" + stretchStatus + "   |   swing " +
                                juce::String(song_.swing() > 0.0 ? "on" : "off") + "   |   metronome " +
                                juce::String(seq.isMetronomeEnabled() ? "on" : "off");

    auto area = getLocalBounds().withSizeKeepingCentre(760, 160);
    g.setColour(Palette::text);
    g.setFont(juce::FontOptions(22.0f));
    g.drawText(status, area.removeFromTop(40), juce::Justification::centred);

    // Playhead across the 16 steps
    auto bar = area.removeFromTop(40).reduced(0, 10);
    const double rate = engine_.sampleRate();
    const double samplesPerStep = 60.0 / song_.bpm() * rate / kStepsPerBeat;
    const int currentStep = static_cast<int>(static_cast<double>(seq.position()) / samplesPerStep) % kStepsPerBar;
    const float stepWidth = static_cast<float>(bar.getWidth()) / kStepsPerBar;
    for (int i = 0; i < kStepsPerBar; ++i)
    {
        const auto cell = juce::Rectangle<float>(static_cast<float>(bar.getX()) + static_cast<float>(i) * stepWidth,
                                                 static_cast<float>(bar.getY()), stepWidth - 3.0f,
                                                 static_cast<float>(bar.getHeight()));
        g.setColour(seq.isPlaying() && i == currentStep ? Palette::accent
                    : (i % kStepsPerBeat == 0)          ? Palette::gridLine.brighter(0.3f)
                                                        : Palette::gridLine);
        g.fillRoundedRectangle(cell, 3.0f);
    }

    g.setColour(Palette::textDim);
    g.setFont(juce::FontOptions(15.0f));
    g.drawText(juce::String::fromUTF8(
                   "Space play/stop  ·  Enter return to start  ·  M metronome  ·  S swing  ·  Up/Down tempo (stretches loop)"),
               area.removeFromTop(40), juce::Justification::centred);
}

} // namespace trami
