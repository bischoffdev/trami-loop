#include "ui/TransportBar.h"

#include "model/Timing.h"

#include <cmath>

namespace trami
{

// ==============================================================================
// TransportButton Implementation
// ==============================================================================
TransportBar::TransportButton::TransportButton(TransportIcon icon, const juce::String& tooltipText)
    : juce::Button("btn"), icon_(icon)
{
    setTooltip(tooltipText);
}

void TransportBar::TransportButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    const auto r = getLocalBounds().toFloat();
    const bool isOn = getToggleState();

    juce::Colour bg = shouldDrawButtonAsDown ? juce::Colour(0xff1d2024)
                    : shouldDrawButtonAsHighlighted ? juce::Colour(0xff343940)
                    : juce::Colour(0xff272b30);

    g.setColour(bg);
    g.fillRect(r);

    if (borderLeft_)
    {
        g.setColour(juce::Colour(0xff393e46));
        g.drawVerticalLine(0, 0.0f, r.getHeight());
    }
    if (borderRight_)
    {
        g.setColour(juce::Colour(0xff393e46));
        g.drawVerticalLine(static_cast<int>(r.getWidth()) - 1, 0.0f, r.getHeight());
    }

    const float cx = r.getCentreX();
    const float cy = r.getCentreY();
    const juce::Colour activeColour(0xff4fc3f7);
    const juce::Colour inactiveColour = shouldDrawButtonAsHighlighted ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff98a0ac);

    switch (icon_)
    {
    case TransportIcon::Record:
    {
        const float dotRadius = 5.2f;
        g.setColour(isOn ? juce::Colour(0xffe53935) : inactiveColour);
        g.fillEllipse(cx - dotRadius, cy - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
        break;
    }
    case TransportIcon::Play:
    {
        juce::Path p;
        p.startNewSubPath(cx - 5.5f, cy - 6.5f);
        p.lineTo(cx + 6.5f, cy);
        p.lineTo(cx - 5.5f, cy + 6.5f);
        p.closeSubPath();
        g.setColour(isOn ? activeColour : inactiveColour);
        g.fillPath(p);
        break;
    }
    case TransportIcon::Rewind:
    {
        juce::Path p;
        p.startNewSubPath(cx - 1.0f, cy);
        p.lineTo(cx + 5.5f, cy - 5.5f);
        p.lineTo(cx + 5.5f, cy + 5.5f);
        p.closeSubPath();
        p.startNewSubPath(cx - 7.5f, cy);
        p.lineTo(cx - 1.0f, cy - 5.5f);
        p.lineTo(cx - 1.0f, cy + 5.5f);
        p.closeSubPath();
        g.setColour(shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown ? juce::Colours::white : inactiveColour);
        g.fillPath(p);
        break;
    }
    case TransportIcon::Stop:
    {
        constexpr float s = 10.0f;
        g.setColour(shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown ? juce::Colours::white : inactiveColour);
        g.fillRect(cx - s * 0.5f, cy - s * 0.5f, s, s);
        break;
    }
    case TransportIcon::Loop:
    {
        g.setColour(isOn ? activeColour : inactiveColour);
        constexpr float rArc = 5.5f;

        // Top arc
        juce::Path topArc;
        topArc.addArc(cx - rArc, cy - rArc, rArc * 2.0f, rArc * 2.0f,
                      -juce::MathConstants<float>::pi * 0.75f, -juce::MathConstants<float>::pi * 0.1f, true);
        g.strokePath(topArc, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Top arrow
        juce::Path topArrow;
        topArrow.startNewSubPath(cx + rArc - 1.0f, cy - rArc - 3.0f);
        topArrow.lineTo(cx + rArc + 3.0f, cy - rArc + 1.0f);
        topArrow.lineTo(cx + rArc - 2.0f, cy - rArc + 4.0f);
        topArrow.closeSubPath();
        g.fillPath(topArrow);

        // Bottom arc
        juce::Path botArc;
        botArc.addArc(cx - rArc, cy - rArc, rArc * 2.0f, rArc * 2.0f,
                      juce::MathConstants<float>::pi * 0.25f, juce::MathConstants<float>::pi * 0.9f, true);
        g.strokePath(botArc, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Bottom arrow
        juce::Path botArrow;
        botArrow.startNewSubPath(cx - rArc + 1.0f, cy + rArc + 3.0f);
        botArrow.lineTo(cx - rArc - 3.0f, cy + rArc - 1.0f);
        botArrow.lineTo(cx - rArc + 2.0f, cy + rArc - 4.0f);
        botArrow.closeSubPath();
        g.fillPath(botArrow);
        break;
    }
    case TransportIcon::CountIn:
    {
        g.setColour(isOn ? activeColour : inactiveColour);
        constexpr float dotR = 2.4f;
        g.fillEllipse(cx - dotR, cy - 6.5f - dotR, dotR * 2.0f, dotR * 2.0f);

        g.setFont(Typography::bold(9.0f));
        g.drawText("1 2 3 4", getLocalBounds().withTrimmedTop(14), juce::Justification::centred, false);
        break;
    }
    case TransportIcon::Metronome:
    {
        g.setColour(isOn ? activeColour : inactiveColour);
        // Studio One metronome body
        juce::Path m;
        m.startNewSubPath(cx - 3.5f, cy - 7.0f);
        m.lineTo(cx + 3.5f, cy - 7.0f);
        m.lineTo(cx + 6.5f, cy + 7.0f);
        m.lineTo(cx - 6.5f, cy + 7.0f);
        m.closeSubPath();
        g.strokePath(m, juce::PathStrokeType(1.4f));

        // Pendulum arm angled to the right
        juce::Path arm;
        arm.startNewSubPath(cx, cy + 5.5f);
        arm.lineTo(cx + 4.5f, cy - 5.0f);
        g.strokePath(arm, juce::PathStrokeType(1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

        // Weight on arm
        g.fillRect(cx + 2.0f, cy - 2.0f, 3.0f, 3.0f);
        break;
    }
    case TransportIcon::Groove:
    {
        g.setColour(isOn ? activeColour : inactiveColour);
        // Notehead
        g.fillEllipse(cx - 6.5f, cy + 1.0f, 5.0f, 4.0f);
        // Stem
        g.fillRect(cx - 2.5f, cy - 7.0f, 1.4f, 10.0f);
        // Flag
        juce::Path flag;
        flag.startNewSubPath(cx - 1.1f, cy - 7.0f);
        flag.quadraticTo(cx + 2.5f, cy - 5.0f, cx + 1.0f, cy - 1.5f);
        g.strokePath(flag, juce::PathStrokeType(1.3f));

        // Clock badge
        constexpr float clockR = 4.2f;
        const float clockX = cx + 4.5f;
        const float clockY = cy + 3.5f;
        g.drawEllipse(clockX - clockR, clockY - clockR, clockR * 2.0f, clockR * 2.0f, 1.2f);
        g.drawLine(clockX, clockY, clockX, clockY - 2.5f, 1.2f);
        g.drawLine(clockX, clockY, clockX + 2.0f, clockY, 1.2f);
        break;
    }
    }
}

// ==============================================================================
// StepperButton Implementation
// ==============================================================================
TransportBar::StepperButton::StepperButton(bool isUp)
    : juce::Button(isUp ? "Up" : "Down"), isUp_(isUp)
{
    setTooltip(isUp ? "Increase Tempo (+1 BPM)" : "Decrease Tempo (-1 BPM)");
}

void TransportBar::StepperButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    const auto r = getLocalBounds().toFloat();
    const juce::Colour bg = shouldDrawButtonAsDown ? juce::Colour(0xff1d2024)
                          : shouldDrawButtonAsHighlighted ? juce::Colour(0xff343940)
                          : juce::Colour(0xff272b30);

    g.setColour(bg);
    g.fillRect(r);

    const float cx = r.getCentreX();
    const float cy = r.getCentreY();
    juce::Path p;

    if (isUp_)
    {
        p.startNewSubPath(cx - 3.5f, cy + 1.5f);
        p.lineTo(cx, cy - 2.5f);
        p.lineTo(cx + 3.5f, cy + 1.5f);
    }
    else
    {
        p.startNewSubPath(cx - 3.5f, cy - 1.5f);
        p.lineTo(cx, cy + 2.5f);
        p.lineTo(cx + 3.5f, cy - 1.5f);
    }
    p.closeSubPath();

    g.setColour(shouldDrawButtonAsHighlighted ? juce::Colours::white : juce::Colour(0xff98a0ac));
    g.fillPath(p);
}

// ==============================================================================
// TimeSigButton Implementation
// ==============================================================================
TransportBar::TimeSigButton::TimeSigButton()
    : juce::Button("Time Signature")
{
    setTooltip("Time Signature: 4/4 meter (Click for details)");
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void TransportBar::TimeSigButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat();
    if (shouldDrawButtonAsDown)
        g.setColour(Palette::surface.brighter(0.12f));
    else if (shouldDrawButtonAsHighlighted)
        g.setColour(Palette::surface.brighter(0.06f));
    else
        g.setColour(Palette::surface);

    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(shouldDrawButtonAsHighlighted ? Palette::accent : juce::Colour(0xff2d333b));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    g.setFont(Typography::bold(11.0f));
    g.setColour(shouldDrawButtonAsHighlighted ? juce::Colours::white : Palette::text);
    g.drawText("4", 0, 1, getWidth(), 14, juce::Justification::centred);

    g.setColour(Palette::gridLine.brighter(0.2f));
    g.drawHorizontalLine(16, 4.0f, static_cast<float>(getWidth() - 4));

    g.setColour(shouldDrawButtonAsHighlighted ? juce::Colours::white : Palette::text);
    g.drawText("4", 0, 16, getWidth(), 15, juce::Justification::centred);
}

// ==============================================================================
// SwingLabel Implementation
// ==============================================================================
class TransportBar::SwingLabel : public juce::Label
{
public:
    SwingLabel(Song& song, ActionCallback onModified, std::function<void()> onUpdate,
               std::function<void(double)> onSwingChanged = nullptr)
        : song_(song), onModified_(std::move(onModified)), onUpdate_(std::move(onUpdate)),
          onSwingChanged_(std::move(onSwingChanged))
    {
        setFont(Typography::bold(12.0f));
        setColour(juce::Label::textColourId, juce::Colour(0xffe2e8f0));
        setJustificationType(juce::Justification::centred);
        setEditable(true, true, false);
        setTooltip("Swing percentage (0% - 100%). Click to type, drag or scroll to adjust, right-click for presets");
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);

        onEditorShow = [this]() {
            if (auto* ed = getCurrentTextEditor())
            {
                ed->setFont(Typography::bold(12.0f));
                ed->setJustification(juce::Justification::centred);
                const double pct = song_.swing() * 100.0;
                if (std::abs(pct - std::round(pct)) < 0.05)
                    ed->setText(juce::String(juce::roundToInt(pct)), false);
                else
                    ed->setText(juce::String(pct, 1), false);
                ed->selectAll();
            }
        };

        onTextChange = [this]() {
            juce::String text = getText().trim().replace("%", "");
            const double val = text.getDoubleValue();
            const double sw = std::clamp(val / 100.0, 0.0, 1.0);
            song_.setSwing(sw);
            if (onSwingChanged_)
                onSwingChanged_(sw);
            else if (onModified_)
                onModified_();
            if (onUpdate_)
                onUpdate_();
        };
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showPresetMenu(e);
            return;
        }

        dragStartSwing_ = song_.swing();
        dragStartY_ = e.getScreenY();
        isDragging_ = false;

        juce::Label::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        const int dy = dragStartY_ - e.getScreenY();
        if (std::abs(dy) > 2 || isDragging_)
        {
            isDragging_ = true;
            const double sensitivity = e.mods.isShiftDown() ? 0.001 : 0.005;
            const double newSwing = std::clamp(dragStartSwing_ + dy * sensitivity, 0.0, 1.0);
            song_.setSwing(newSwing);
            if (onSwingChanged_)
                onSwingChanged_(newSwing);
            else if (onModified_)
                onModified_();
            if (onUpdate_)
                onUpdate_();
            return;
        }

        juce::Label::mouseDrag(e);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (isDragging_)
        {
            isDragging_ = false;
            return;
        }
        juce::Label::mouseUp(e);
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        juce::ignoreUnused(e);
        song_.setSwing(0.0);
        if (onSwingChanged_)
            onSwingChanged_(0.0);
        else if (onModified_)
            onModified_();
        if (onUpdate_)
            onUpdate_();
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const double step = (e.mods.isShiftDown() ? 0.05 : 0.01);
        const double dir = (wheel.deltaY > 0.0f) ? 1.0 : -1.0;
        const double newSwing = std::clamp(song_.swing() + dir * step, 0.0, 1.0);
        song_.setSwing(newSwing);
        if (onSwingChanged_)
            onSwingChanged_(newSwing);
        else if (onModified_)
            onModified_();
        if (onUpdate_)
            onUpdate_();
    }

private:
    void showPresetMenu(const juce::MouseEvent& e)
    {
        juce::PopupMenu m;
        m.addItem(1, "Straight (0%)", true, song_.swing() == 0.0);
        m.addSeparator();
        m.addItem(2, "Subtle (15%)", true, std::abs(song_.swing() - 0.15) < 0.01);
        m.addItem(3, "Light (25%)", true, std::abs(song_.swing() - 0.25) < 0.01);
        m.addItem(4, "Classic Half (50%)", true, std::abs(song_.swing() - 0.50) < 0.01);
        m.addItem(5, "MPC / Triplet (66.7%)", true, std::abs(song_.swing() - (2.0 / 3.0)) < 0.01);
        m.addItem(6, "Heavy (75%)", true, std::abs(song_.swing() - 0.75) < 0.01);
        m.addItem(7, "Hard (85%)", true, std::abs(song_.swing() - 0.85) < 0.01);
        m.addSeparator();
        m.addItem(10, "Reset to 0%");

        m.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this).withTargetScreenArea(
                            juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                        [this](int result) {
                            if (result == 1) song_.setSwing(0.0);
                            else if (result == 2) song_.setSwing(0.15);
                            else if (result == 3) song_.setSwing(0.25);
                            else if (result == 4) song_.setSwing(0.50);
                            else if (result == 5) song_.setSwing(2.0 / 3.0);
                            else if (result == 6) song_.setSwing(0.75);
                            else if (result == 7) song_.setSwing(0.85);
                            else if (result == 10) song_.setSwing(0.0);
                            else return;

                            if (onSwingChanged_) onSwingChanged_(song_.swing());
                            else if (onModified_) onModified_();
                            if (onUpdate_) onUpdate_();
                        });
    }

    Song& song_;
    ActionCallback onModified_;
    std::function<void()> onUpdate_;
    std::function<void(double)> onSwingChanged_;
    double dragStartSwing_ = 0.0;
    int dragStartY_ = 0;
    bool isDragging_ = false;
};

class TransportBar::SwingSlider : public juce::Slider
{
public:
    SwingSlider(Song& song, ActionCallback onModified, std::function<void()> onUpdate,
                std::function<void(double)> onSwingChanged = nullptr)
        : juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox),
          song_(song), onModified_(std::move(onModified)), onUpdate_(std::move(onUpdate)),
          onSwingChanged_(std::move(onSwingChanged))
    {
        setRange(0.0, 100.0, 0.5);
        setDoubleClickReturnValue(true, 0.0);
        setTooltip("Swing Groove (0% - 100%). Drag to adjust, double-click: 0%, right-click: Presets");
        setColour(juce::Slider::thumbColourId, Palette::accent);
        setColour(juce::Slider::trackColourId, Palette::accent.withAlpha(0.35f));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1e2228));

        onValueChange = [this]() {
            const double val = getValue() / 100.0;
            song_.setSwing(val);
            if (onSwingChanged_)
                onSwingChanged_(val);
            else if (onModified_)
                onModified_();
            if (onUpdate_)
                onUpdate_();
        };
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showPresetMenu(e);
            return;
        }
        juce::Slider::mouseDown(e);
    }

private:
    void showPresetMenu(const juce::MouseEvent& e)
    {
        juce::PopupMenu m;
        m.addItem(1, "Straight (0%)", true, song_.swing() == 0.0);
        m.addSeparator();
        m.addItem(2, "Subtle (15%)", true, std::abs(song_.swing() - 0.15) < 0.01);
        m.addItem(3, "Light (25%)", true, std::abs(song_.swing() - 0.25) < 0.01);
        m.addItem(4, "Classic Half (50%)", true, std::abs(song_.swing() - 0.50) < 0.01);
        m.addItem(5, "MPC / Triplet (66.7%)", true, std::abs(song_.swing() - (2.0 / 3.0)) < 0.01);
        m.addItem(6, "Heavy (75%)", true, std::abs(song_.swing() - 0.75) < 0.01);
        m.addItem(7, "Hard (85%)", true, std::abs(song_.swing() - 0.85) < 0.01);
        m.addSeparator();
        m.addItem(10, "Reset to 0%");

        m.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this).withTargetScreenArea(
                            juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                        [this](int result) {
                            if (result == 1) song_.setSwing(0.0);
                            else if (result == 2) song_.setSwing(0.15);
                            else if (result == 3) song_.setSwing(0.25);
                            else if (result == 4) song_.setSwing(0.50);
                            else if (result == 5) song_.setSwing(2.0 / 3.0);
                            else if (result == 6) song_.setSwing(0.75);
                            else if (result == 7) song_.setSwing(0.85);
                            else if (result == 10) song_.setSwing(0.0);
                            else return;

                            setValue(song_.swing() * 100.0, juce::dontSendNotification);
                            if (onSwingChanged_) onSwingChanged_(song_.swing());
                            else if (onModified_) onModified_();
                            if (onUpdate_) onUpdate_();
                        });
    }

    Song& song_;
    ActionCallback onModified_;
    std::function<void()> onUpdate_;
    std::function<void(double)> onSwingChanged_;
};

// ==============================================================================
// TransportBar Implementation
// ==============================================================================
TransportBar::TransportBar(Song& song, AudioEngine& engine,
                           ActionCallback onModelModified,
                           ActionCallback onRecordToggle)
    : song_(song), engine_(engine),
      onModelModified_(std::move(onModelModified)),
      onRecordToggle_(std::move(onRecordToggle))
{
    // Record
    recordButton_.onClick = [this]() {
        if (onRecordToggle_)
            onRecordToggle_();
    };
    addAndMakeVisible(recordButton_);

    // Play
    playButton_.onClick = [this]() {
        auto& seq = engine_.sequencer();
        seq.requestPlaying(!seq.isPlaying());
    };
    addAndMakeVisible(playButton_);

    // Rewind
    rewindButton_.onClick = [this]() {
        engine_.sequencer().requestRewind();
    };
    addAndMakeVisible(rewindButton_);

    // Stop
    stopButton_.onClick = [this]() {
        if (engine_.recorder().state() != RecordState::Idle)
            engine_.recorder().stop();
        engine_.sequencer().requestPlaying(false);
    };
    addAndMakeVisible(stopButton_);

    // Loop
    loopButton_.setClickingTogglesState(true);
    setLooping(isLooping_);
    loopButton_.onClick = [this]() {
        setLooping(loopButton_.getToggleState());
    };
    addAndMakeVisible(loopButton_);

    // Count-in
    countInButton_.setClickingTogglesState(true);
    countInButton_.setToggleState(isCountIn_, juce::dontSendNotification);
    countInButton_.onClick = [this]() {
        isCountIn_ = countInButton_.getToggleState();
    };
    addAndMakeVisible(countInButton_);

    // Position display
    positionLabel_.setFont(Typography::bold(17.0f));
    positionLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffedf2f7));
    positionLabel_.setJustificationType(juce::Justification::centred);
    positionLabel_.setText("1 : 1 : 000", juce::dontSendNotification);
    positionLabel_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(positionLabel_);

    // Mode button
    modeButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3ea2c5));
    modeButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0d151c));
    modeButton_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff4fb5d6));
    modeButton_.setTooltip("Toggle Time Display Mode (Measure / Seconds)");
    modeButton_.onClick = [this]() {
        showSeconds_ = !showSeconds_;
        modeButton_.setButtonText(showSeconds_ ? "Seconds" : "Measure");
        timerCallback();
    };
    addAndMakeVisible(modeButton_);

    // Tempo Steppers & Label
    tempoUp_.onClick = [this]() {
        song_.setBpm(std::min(300.0, song_.bpm() + 1.0));
        updateDisplay();
        if (onModelModified_)
            onModelModified_();
    };
    addAndMakeVisible(tempoUp_);

    tempoDown_.onClick = [this]() {
        song_.setBpm(std::max(20.0, song_.bpm() - 1.0));
        updateDisplay();
        if (onModelModified_)
            onModelModified_();
    };
    addAndMakeVisible(tempoDown_);

    tempoLabel_.setFont(Typography::bold(13.5f));
    tempoLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffe2e8f0));
    tempoLabel_.setJustificationType(juce::Justification::centred);
    tempoLabel_.setEditable(true, true, false);
    tempoLabel_.setTooltip("Tempo in BPM. Click to type a new value, or use arrow buttons");
    tempoLabel_.onEditorShow = [this]() {
        if (auto* ed = tempoLabel_.getCurrentTextEditor())
        {
            ed->setText(juce::String(std::round(song_.bpm())), false);
            ed->selectAll();
        }
    };
    tempoLabel_.onTextChange = [this]() {
        const double val = tempoLabel_.getText().trim().getDoubleValue();
        if (val > 0.0)
        {
            song_.setBpm(juce::jlimit(20.0, 300.0, val));
            if (onModelModified_)
                onModelModified_();
        }
        updateDisplay();
    };
    addAndMakeVisible(tempoLabel_);

    // Metronome Click
    metroButton_.setClickingTogglesState(true);
    metroButton_.setToggleState(engine_.sequencer().isMetronomeEnabled(), juce::dontSendNotification);
    metroButton_.onClick = [this]() {
        engine_.sequencer().setMetronomeEnabled(metroButton_.getToggleState());
    };
    addAndMakeVisible(metroButton_);

    // Swing controls (percentage text field + slider)
    swingLabel_ = std::make_unique<SwingLabel>(song_, onModelModified_, [this]() { updateDisplay(); },
                                               [this](double s) { if (onSwingChanged_) onSwingChanged_(s); });
    addAndMakeVisible(swingLabel_.get());

    swingSlider_ = std::make_unique<SwingSlider>(song_, onModelModified_, [this]() { updateDisplay(); },
                                                 [this](double s) { if (onSwingChanged_) onSwingChanged_(s); });
    swingSlider_->setValue(song_.swing() * 100.0, juce::dontSendNotification);
    addAndMakeVisible(swingSlider_.get());

    addAndMakeVisible(timeSigButton_);
    timeSigButton_.onClick = [this]() {
        juce::PopupMenu menu;
        menu.addItem(1, "Time Signature: 4/4 (Active)", true, true);
        menu.addSeparator();
        menu.addItem(2, "3/4 (Fixed 4/4 in v1.0)", false);
        menu.addItem(3, "6/8 (Fixed 4/4 in v1.0)", false);
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&timeSigButton_), [this](int) {
            if (onShowToast_)
                onShowToast_("Project locked to standard 4/4 time (32 steps per bar)");
        });
    };

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

void TransportBar::setLoopRange(int start, int end, bool isBars)
{
    loopStart_ = start;
    loopEnd_ = end;
    loopIsBars_ = isBars;
    repaint();
}

void TransportBar::mouseDown(const juce::MouseEvent& e)
{
    constexpr int totalW = 764;
    const int startX = std::max(6, (getWidth() - totalW) / 2);
    constexpr int barY = 6;
    constexpr int barH = 32;
    const int posBoxX = startX + 216 + 12;
    const int loopBoxX = posBoxX + 150 + 6;
    const auto loopBox = juce::Rectangle<int>(loopBoxX, barY, 110, barH);

    if (loopBox.contains(e.getPosition()))
    {
        juce::PopupMenu menu;
        if (loopIsBars_)
        {
            menu.addItem(1, "Loop 1 Bar", true, (loopEnd_ - loopStart_) == 1);
            menu.addItem(2, "Loop 2 Bars", true, (loopEnd_ - loopStart_) == 2);
            menu.addItem(3, "Loop 4 Bars", true, (loopEnd_ - loopStart_) == 4);
            menu.addItem(4, "Loop 8 Bars", true, (loopEnd_ - loopStart_) == 8);
            menu.addItem(5, "Loop 16 Bars", true, (loopEnd_ - loopStart_) == 16);
            menu.addSeparator();
            menu.addItem(6, "Expand to Full Length", true, loopStart_ == 0 && loopEnd_ == std::max(2, song_.lengthBars()));
            menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this), [this](int result) {
                if (!onLoopRangeChanged_) return;
                if (result == 1) onLoopRangeChanged_(0, 1);
                else if (result == 2) onLoopRangeChanged_(0, 2);
                else if (result == 3) onLoopRangeChanged_(0, 4);
                else if (result == 4) onLoopRangeChanged_(0, 8);
                else if (result == 5) onLoopRangeChanged_(0, 16);
                else if (result == 6) onLoopRangeChanged_(0, std::max(2, song_.lengthBars()));
            });
        }
        else
        {
            menu.addItem(1, "Loop 1 Bar", true, (loopEnd_ - loopStart_) == kStepsPerBar);
            menu.addItem(2, "Loop 2 Bars", true, (loopEnd_ - loopStart_) == kStepsPerBar * 2);
            menu.addItem(3, "Loop 4 Bars", true, (loopEnd_ - loopStart_) == kStepsPerBar * 4);
            menu.addSeparator();
            menu.addItem(4, "Expand to Full Length");
            menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this), [this](int result) {
                if (!onLoopRangeChanged_) return;
                if (result == 1) onLoopRangeChanged_(0, kStepsPerBar);
                else if (result == 2) onLoopRangeChanged_(0, kStepsPerBar * 2);
                else if (result == 3) onLoopRangeChanged_(0, kStepsPerBar * 4);
                else if (result == 4) onLoopRangeChanged_(0, 0); // entire pattern
            });
        }
        return;
    }

    juce::Component::mouseDown(e);
}

void TransportBar::updateDisplay()
{
    if (!tempoLabel_.isBeingEdited())
        tempoLabel_.setText(juce::String(juce::roundToInt(song_.bpm())), juce::dontSendNotification);
    if (swingLabel_ != nullptr && !swingLabel_->isBeingEdited())
    {
        const double sw = song_.swing();
        const double pct = sw * 100.0;
        if (std::abs(pct - std::round(pct)) < 0.05)
            swingLabel_->setText(juce::String(juce::roundToInt(pct)) + "%", juce::dontSendNotification);
        else
            swingLabel_->setText(juce::String(pct, 1) + "%", juce::dontSendNotification);

        swingLabel_->setColour(juce::Label::textColourId, sw > 0.001 ? Palette::accent : juce::Colour(0xffe2e8f0));
    }
    if (swingSlider_ != nullptr && !swingSlider_->isMouseButtonDown())
    {
        swingSlider_->setValue(song_.swing() * 100.0, juce::dontSendNotification);
        const double pct = song_.swing() * 100.0;
        const juce::String pctStr = (std::abs(pct - std::round(pct)) < 0.05)
            ? juce::String(juce::roundToInt(pct)) + "%"
            : juce::String(pct, 1) + "%";
        swingSlider_->setTooltip("Swing Groove: " + pctStr + " (Double-click: 0%, Right-click: Presets)");
    }
    metroButton_.setToggleState(engine_.sequencer().isMetronomeEnabled(), juce::dontSendNotification);
    repaint();
}

void TransportBar::paint(juce::Graphics& g)
{
    // Studio One transport bar background
    g.fillAll(juce::Colour(0xff2d3137));

    // Top border line
    g.setColour(juce::Colour(0xff1e2126));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));

    constexpr int totalW = 848;
    constexpr int barY = 6;
    constexpr int barH = 32;

    int startX = 6;
    if (getWidth() >= totalW + 24)
        startX = std::max(12, (getWidth() - totalW) / 2);
    else
        startX = 6;

    const auto drawBox = [&g](juce::Rectangle<int> box) {
        g.setColour(juce::Colour(0xff24282e));
        g.fillRoundedRectangle(box.toFloat(), 3.0f);
        g.setColour(juce::Colour(0xff393e46));
        g.drawRoundedRectangle(box.toFloat(), 3.0f, 1.0f);
    };

    // 1. Transport cluster box
    const auto transBox = juce::Rectangle<int>(startX, barY, 216, barH);
    drawBox(transBox);

    // 2. Position Display box
    const int posBoxX = startX + 216 + 12;
    const auto posBox = juce::Rectangle<int>(posBoxX, barY, 150, barH);
    drawBox(posBox);

    // Cyan strip inside position box on left
    constexpr int cyanStripW = 20;
    const auto cyanRect = juce::Rectangle<float>(static_cast<float>(posBoxX + 1), static_cast<float>(barY + 1),
                                                 static_cast<float>(cyanStripW), static_cast<float>(barH - 2));
    g.setColour(juce::Colour(0xff3aa0c8));
    g.fillRoundedRectangle(cyanRect, 2.0f);

    // Small speaker icon in cyan strip
    {
        const float scx = cyanRect.getCentreX();
        const float scy = cyanRect.getCentreY();

        // Speaker body: base rectangle + expanding cone
        juce::Path spk;
        spk.startNewSubPath(scx - 6.0f, scy - 2.5f);
        spk.lineTo(scx - 3.0f, scy - 2.5f);
        spk.lineTo(scx + 0.5f, scy - 5.5f);
        spk.lineTo(scx + 0.5f, scy + 5.5f);
        spk.lineTo(scx - 3.0f, scy + 2.5f);
        spk.lineTo(scx - 6.0f, scy + 2.5f);
        spk.closeSubPath();
        g.setColour(juce::Colour(0xff1a2630));
        g.fillPath(spk);

        // Sound waves radiating to the right (smooth quadratic arcs)
        const auto stroke = juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        // Inner sound wave
        juce::Path wave1;
        wave1.startNewSubPath(scx + 2.2f, scy - 3.2f);
        wave1.quadraticTo(scx + 4.8f, scy, scx + 2.2f, scy + 3.2f);
        g.strokePath(wave1, stroke);

        // Outer sound wave
        juce::Path wave2;
        wave2.startNewSubPath(scx + 4.6f, scy - 5.5f);
        wave2.quadraticTo(scx + 8.2f, scy, scx + 4.6f, scy + 5.5f);
        g.strokePath(wave2, stroke);
    }

    // 3. Loop Range Box (Start / End)
    const int loopBoxX = posBoxX + 150 + 6;
    const auto loopBox = juce::Rectangle<int>(loopBoxX, barY, 110, barH);
    drawBox(loopBox);

    // Dividers inside Loop box
    g.setColour(juce::Colour(0xff343941));
    g.drawHorizontalLine(barY + 16, static_cast<float>(loopBoxX + 1), static_cast<float>(loopBoxX + 109));
    g.drawVerticalLine(loopBoxX + 44, static_cast<float>(barY + 1), static_cast<float>(barY + barH - 1));

    // Start / End labels & values
    g.setFont(Typography::bold(9.5f));
    g.setColour(Palette::textDim);
    g.drawText("Start", loopBoxX + 4, barY + 1, 38, 15, juce::Justification::centredLeft);
    g.drawText("End", loopBoxX + 4, barY + 16, 38, 15, juce::Justification::centredLeft);

    juce::String startStr, endStr;
    if (loopIsBars_)
    {
        startStr = juce::String::formatted("%d : 1 : 000", loopStart_ + 1);
        endStr = juce::String::formatted("%d : 1 : 000", loopEnd_ + 1);
    }
    else
    {
        const int sBar = (loopStart_ / kStepsPerBar) + 1;
        const int sBeat = ((loopStart_ % kStepsPerBar) / kStepsPerBeat) + 1;
        startStr = juce::String::formatted("%d : %d : 000", sBar, sBeat);

        const int eBar = (loopEnd_ / kStepsPerBar) + 1;
        const int eBeat = ((loopEnd_ % kStepsPerBar) / kStepsPerBeat) + 1;
        endStr = juce::String::formatted("%d : %d : 000", eBar, eBeat);
    }

    g.setFont(Typography::bold(10.5f));
    g.setColour(juce::Colour(0xffe2e8f0));
    g.drawText(startStr, loopBoxX + 46, barY + 1, 60, 15, juce::Justification::centred);
    g.drawText(endStr, loopBoxX + 46, barY + 16, 60, 15, juce::Justification::centred);

    // 4. Mode button box is self-drawn

    // 5. Tempo Box
    const int tempoBoxX = loopBoxX + 110 + 6 + 76 + 6;
    const auto tempoBox = juce::Rectangle<int>(tempoBoxX, barY, 66, barH);
    drawBox(tempoBox);

    g.setFont(Typography::bold(9.0f));
    g.setColour(Palette::textDim);
    g.drawText("Bpm", tempoBoxX + 4, barY + 2, 42, 11, juce::Justification::centred);

    // Vertical divider before steppers
    g.setColour(juce::Colour(0xff393e46));
    g.drawVerticalLine(tempoBoxX + 46, static_cast<float>(barY + 1), static_cast<float>(barY + barH - 1));

    // 6. Metronome box
    const int metroBoxX = tempoBoxX + 66 + 6;
    drawBox(juce::Rectangle<int>(metroBoxX, barY, 36, barH));

    // 7. Time Signature is drawn by timeSigButton_

    // 8. Swing / Groove box
    const int swingBoxX = metroBoxX + 36 + 6 + 32 + 6;
    constexpr int swingBoxW = 120;
    drawBox(juce::Rectangle<int>(swingBoxX, barY, swingBoxW, barH));

    g.setFont(Typography::bold(9.5f));
    const bool hasSwing = song_.swing() > 0.001;
    g.setColour(hasSwing ? Palette::accent : Palette::textDim);
    g.drawText("Swing", swingBoxX + 4, barY + 2, 44, 10, juce::Justification::centred);

    // Vertical divider between text field and slider
    g.setColour(juce::Colour(0xff393e46));
    g.drawVerticalLine(swingBoxX + 50, static_cast<float>(barY + 1), static_cast<float>(barY + barH - 1));
}

void TransportBar::resized()
{
    constexpr int totalW = 848;
    constexpr int barY = 6;
    constexpr int barH = 32;

    int startX = 6;
    if (getWidth() >= totalW + 24)
        startX = std::max(12, (getWidth() - totalW) / 2);
    else
        startX = 6;

    // Transport buttons inside 216px container
    int btnX = startX + 1;
    constexpr int btnW = 35;

    recordButton_.setBounds(btnX, barY + 1, btnW, barH - 2);
    btnX += btnW;
    recordButton_.setBorderSides(false, true);

    playButton_.setBounds(btnX, barY + 1, btnW, barH - 2);
    btnX += btnW;
    playButton_.setBorderSides(false, true);

    rewindButton_.setBounds(btnX, barY + 1, btnW, barH - 2);
    btnX += btnW;
    rewindButton_.setBorderSides(false, true);

    stopButton_.setBounds(btnX, barY + 1, btnW, barH - 2);
    btnX += btnW;
    stopButton_.setBorderSides(false, true);

    loopButton_.setBounds(btnX, barY + 1, btnW, barH - 2);
    btnX += btnW;
    loopButton_.setBorderSides(false, true);

    countInButton_.setBounds(btnX, barY + 1, btnW + 4, barH - 2);

    // Position label inside position box
    const int posBoxX = startX + 216 + 12;
    positionLabel_.setBounds(posBoxX + 24, barY + 1, 122, barH - 2);

    // Mode button
    const int loopBoxX = posBoxX + 150 + 6;
    const int modeBtnX = loopBoxX + 110 + 6;
    modeButton_.setBounds(modeBtnX, barY, 76, barH);

    // Tempo controls
    const int tempoBoxX = modeBtnX + 76 + 6;
    tempoLabel_.setBounds(tempoBoxX + 2, barY + 12, 44, barH - 14);
    tempoUp_.setBounds(tempoBoxX + 47, barY + 1, 18, 15);
    tempoDown_.setBounds(tempoBoxX + 47, barY + 16, 18, 15);

    // Metronome
    const int metroBoxX = tempoBoxX + 66 + 6;
    metroButton_.setBounds(metroBoxX + 1, barY + 1, 34, barH - 2);

    // Time Signature
    timeSigButton_.setBounds(metroBoxX + 36 + 6, barY, 32, barH);

    // Swing controls
    const int swingBoxX = metroBoxX + 36 + 6 + 32 + 6;
    if (swingLabel_ != nullptr)
        swingLabel_->setBounds(swingBoxX + 2, barY + 12, 46, barH - 14);
    if (swingSlider_ != nullptr)
        swingSlider_->setBounds(swingBoxX + 53, barY + 6, 62, barH - 12);
}

void TransportBar::timerCallback()
{
    const bool isPlaying = engine_.sequencer().isPlaying();
    playButton_.setToggleState(isPlaying, juce::dontSendNotification);

    const auto recState = engine_.recorder().state();
    recordButton_.setToggleState(recState != RecordState::Idle, juce::dontSendNotification);

    const std::int64_t pos = engine_.sequencer().position();

    if (showSeconds_)
    {
        const double totalSecs = std::max(0.0, static_cast<double>(pos) / engine_.sampleRate());
        const int mins = static_cast<int>(totalSecs) / 60;
        const int secs = static_cast<int>(totalSecs) % 60;
        const int millis = static_cast<int>(std::fmod(totalSecs, 1.0) * 1000.0);
        positionLabel_.setText(juce::String::formatted("%02d : %02d : %03d", mins, secs, millis),
                               juce::dontSendNotification);
    }
    else
    {
        const Timing timing(song_.bpm(), engine_.sampleRate(), song_.swing());
        const double step = static_cast<double>(pos) / timing.samplesPerStep();
        const int bar = static_cast<int>(step / kStepsPerBar) + 1;
        const int beat = (static_cast<int>(step) % kStepsPerBar) / kStepsPerBeat + 1;
        const int tick = juce::roundToInt((step - std::floor(step)) * 240.0);

        positionLabel_.setText(juce::String::formatted("%d : %d : %03d", bar, beat, tick),
                               juce::dontSendNotification);
    }
}

} // namespace trami
