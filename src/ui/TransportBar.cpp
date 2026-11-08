#include "ui/TransportBar.h"

#include "model/Timing.h"

#include <cmath>

namespace trami
{

TransportBar::TransportBar(Song& song, AudioEngine& engine,
                           ActionCallback onModelModified,
                           ActionCallback onRecordToggle)
    : song_(song), engine_(engine),
      onModelModified_(std::move(onModelModified)),
      onRecordToggle_(std::move(onRecordToggle))
{
    // Rewind
    rewindButton_.onClick = [this]() {
        engine_.sequencer().requestRewind();
    };
    addAndMakeVisible(rewindButton_);

    // Play / Stop
    playButton_.setActiveColour(juce::Colour(0xff43a047));
    playButton_.onClick = [this]() {
        auto& seq = engine_.sequencer();
        seq.requestPlaying(!seq.isPlaying());
    };
    addAndMakeVisible(playButton_);

    // Record
    recordButton_.setActiveColour(juce::Colour(0xffe53935));
    recordButton_.onClick = [this]() {
        if (onRecordToggle_)
            onRecordToggle_();
    };
    addAndMakeVisible(recordButton_);

    // Loop toggle
    loopButton_.setActiveColour(juce::Colour(0xffffb300));
    loopButton_.setClickingTogglesState(true);
    loopButton_.setToggleState(isLooping_, juce::dontSendNotification);
    loopButton_.setTooltip("Toggle Playback Loop");
    loopButton_.onClick = [this]() {
        setLooping(loopButton_.getToggleState());
    };
    addAndMakeVisible(loopButton_);

    // Click / Metronome
    metroButton_.setClickingTogglesState(true);
    metroButton_.setToggleState(engine_.sequencer().isMetronomeEnabled(), juce::dontSendNotification);
    metroButton_.setTooltip("Toggle Metronome Click");
    metroButton_.onClick = [this]() {
        engine_.sequencer().setMetronomeEnabled(metroButton_.getToggleState());
    };
    addAndMakeVisible(metroButton_);

    // Time / Position Display
    timeLabel_.setFont(Typography::bold(17.0f));
    timeLabel_.setColour(juce::Label::textColourId, Palette::accent);
    timeLabel_.setColour(juce::Label::backgroundColourId, Palette::background);
    timeLabel_.setColour(juce::Label::outlineColourId, Palette::gridLine);
    timeLabel_.setJustificationType(juce::Justification::centred);
    timeLabel_.setText("1.1.01", juce::dontSendNotification);
    addAndMakeVisible(timeLabel_);

    // Tempo controls
    tempoDown_.onClick = [this]() {
        song_.setBpm(song_.bpm() - 1.0);
        updateDisplay();
        if (onModelModified_)
            onModelModified_();
    };
    addAndMakeVisible(tempoDown_);

    tempoLabel_.setFont(Typography::bold(14.5f));
    tempoLabel_.setColour(juce::Label::textColourId, Palette::text);
    tempoLabel_.setJustificationType(juce::Justification::centred);
    tempoLabel_.setEditable(true, true, false);
    tempoLabel_.setTooltip("Tempo in BPM. Click to type a new value, or use - / +");
    tempoLabel_.onEditorShow = [this]() {
        if (auto* ed = tempoLabel_.getCurrentTextEditor())
        {
            ed->setText(juce::String(std::round(song_.bpm())), false);
            ed->selectAll();
        }
    };
    tempoLabel_.onTextChange = [this]() {
        const juce::String text = tempoLabel_.getText().trim();
        const double val = text.getDoubleValue();
        if (val > 0.0)
        {
            song_.setBpm(juce::jlimit(20.0, 300.0, val));
            if (onModelModified_)
                onModelModified_();
        }
        updateDisplay();
    };
    addAndMakeVisible(tempoLabel_);

    tempoUp_.onClick = [this]() {
        song_.setBpm(song_.bpm() + 1.0);
        updateDisplay();
        if (onModelModified_)
            onModelModified_();
    };
    addAndMakeVisible(tempoUp_);

    // Swing
    swingLabel_.setFont(Typography::bold(12.0f));
    swingLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    swingLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(swingLabel_);

    swingSlider_.setSliderStyle(juce::Slider::LinearBar);
    swingSlider_.setRange(0.0, 1.0, 0.01);
    swingSlider_.setValue(song_.swing(), juce::dontSendNotification);
    swingSlider_.setColour(juce::Slider::trackColourId, Palette::accent);
    swingSlider_.setColour(juce::Slider::backgroundColourId, Palette::background);
    swingSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    swingSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 55, 22);
    swingSlider_.textFromValueFunction = [](double val) {
        return juce::String(juce::roundToInt(val * 100.0)) + "%";
    };
    swingSlider_.onValueChange = [this]() {
        song_.setSwing(swingSlider_.getValue());
        if (onModelModified_)
            onModelModified_();
    };
    swingSlider_.setTooltip("Swing Groove (0% - 100%)");
    addAndMakeVisible(swingSlider_);

    updateDisplay();
    startTimerHz(30);
}

TransportBar::~TransportBar()
{
    stopTimer();
}

void TransportBar::setLooping(bool looping)
{
    isLooping_ = looping;
    loopButton_.setToggleState(isLooping_, juce::dontSendNotification);
    engine_.sequencer().setLoopingEnabled(looping);
    if (onModelModified_)
        onModelModified_();
}

void TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(Palette::transportBackground);

    // Top border line
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
    g.drawHorizontalLine(1, 0.0f, static_cast<float>(getWidth()));
}

void TransportBar::resized()
{
    const int totalW = 672;
    const int startX = std::max(6, (getWidth() - totalW) / 2);
    auto area = juce::Rectangle<int>(startX, 7, totalW, getHeight() - 14);

    // Transport buttons cluster (taller and wider controls)
    rewindButton_.setBounds(area.removeFromLeft(50));
    area.removeFromLeft(5);
    playButton_.setBounds(area.removeFromLeft(56));
    area.removeFromLeft(5);
    recordButton_.setBounds(area.removeFromLeft(56));
    area.removeFromLeft(5);
    loopButton_.setBounds(area.removeFromLeft(54));
    area.removeFromLeft(5);
    metroButton_.setBounds(area.removeFromLeft(52));
    area.removeFromLeft(16);

    // Time display
    timeLabel_.setBounds(area.removeFromLeft(96));
    area.removeFromLeft(16);

    // Tempo cluster
    tempoDown_.setBounds(area.removeFromLeft(28));
    tempoLabel_.setBounds(area.removeFromLeft(72));
    tempoUp_.setBounds(area.removeFromLeft(28));
    area.removeFromLeft(16);

    // Swing
    swingLabel_.setBounds(area.removeFromLeft(50));
    area.removeFromLeft(4);
    swingSlider_.setBounds(area.removeFromLeft(80));
}

void TransportBar::updateDisplay()
{
    if (!tempoLabel_.isBeingEdited())
        tempoLabel_.setText(juce::String(song_.bpm(), 0) + " BPM", juce::dontSendNotification);
    swingSlider_.setValue(song_.swing(), juce::dontSendNotification);
    metroButton_.setToggleState(engine_.sequencer().isMetronomeEnabled(), juce::dontSendNotification);
}

void TransportBar::timerCallback()
{
    const bool isPlaying = engine_.sequencer().isPlaying();
    if (isPlaying)
    {
        playButton_.setIcon(IconButton::Icon::Stop);
        playButton_.setLabel("Stop");
        playButton_.setToggleState(true, juce::dontSendNotification);
    }
    else
    {
        playButton_.setIcon(IconButton::Icon::Play);
        playButton_.setLabel("Play");
        playButton_.setToggleState(false, juce::dontSendNotification);
    }

    // Position display in bar.beat.substep
    const std::int64_t pos = engine_.sequencer().position();
    const Timing timing(song_.bpm(), engine_.sampleRate(), song_.swing());
    const double step = static_cast<double>(pos) / timing.samplesPerStep();
    const int bar = static_cast<int>(step / kStepsPerBar) + 1;
    const int beat = (static_cast<int>(step) % kStepsPerBar) / kStepsPerBeat + 1;
    const int sub = (static_cast<int>(step) % kStepsPerBeat) + 1;

    timeLabel_.setText(juce::String::formatted("%d.%d.%02d", bar, beat, sub), juce::dontSendNotification);
}

} // namespace trami

