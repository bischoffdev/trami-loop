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

    // Brushed metallic vertical bevel fill (matching Screenshot 2)
    juce::Colour topCol = shouldDrawButtonAsDown ? juce::Colour(0xff1e1e23)
                        : shouldDrawButtonAsHighlighted ? juce::Colour(0xff4c4c55)
                        : (isOn && icon_ != TransportIcon::Record ? juce::Colour(0xff384654) : juce::Colour(0xff424249));
    juce::Colour botCol = shouldDrawButtonAsDown ? juce::Colour(0xff2c2c32)
                        : shouldDrawButtonAsHighlighted ? juce::Colour(0xff32323a)
                        : (isOn && icon_ != TransportIcon::Record ? juce::Colour(0xff25303c) : juce::Colour(0xff27272d));

    juce::ColourGradient grad(topCol, r.getX(), r.getY(), botCol, r.getX(), r.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRect(r);

    // Subtle top specular highlight
    if (!shouldDrawButtonAsDown)
    {
        g.setColour(juce::Colours::white.withAlpha(0.14f));
        g.drawHorizontalLine(0, r.getX(), r.getRight());
    }

    if (borderLeft_)
    {
        g.setColour(juce::Colour(0xff18181c));
        g.drawVerticalLine(0, 0.0f, r.getHeight());
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawVerticalLine(1, 1.0f, r.getHeight() - 1.0f);
    }
    if (borderRight_)
    {
        g.setColour(juce::Colour(0xff18181c));
        g.drawVerticalLine(static_cast<int>(r.getWidth()) - 1, 0.0f, r.getHeight());
    }

    const float cx = r.getCentreX();
    const float cy = r.getCentreY();
    const juce::Colour activeColour(0xff5ac8fa);
    const juce::Colour inactiveColour = shouldDrawButtonAsHighlighted ? juce::Colours::white : juce::Colour(0xffdcdce4);

    switch (icon_)
    {
    case TransportIcon::Record:
    {
        // Glossy 3D red record sphere (matching Screenshot 2)
        const float dotR = 5.6f;
        if (isOn)
        {
            g.setColour(juce::Colour(0xffff2d2d).withAlpha(0.35f));
            g.fillEllipse(cx - dotR - 2.5f, cy - dotR - 2.5f, (dotR + 2.5f) * 2.0f, (dotR + 2.5f) * 2.0f);
        }
        const juce::Colour coreCol = isOn ? juce::Colour(0xffff3b30) : juce::Colour(0xffc93b3b);
        const juce::Colour edgeCol = isOn ? juce::Colour(0xff9e0b0b) : juce::Colour(0xff6b1717);
        juce::ColourGradient orb(coreCol.brighter(0.45f), cx - 2.0f, cy - 2.5f,
                                 edgeCol, cx + dotR, cy + dotR, true);
        g.setGradientFill(orb);
        g.fillEllipse(cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);

        // Specular highlight dot
        g.setColour(juce::Colours::white.withAlpha(isOn ? 0.72f : 0.45f));
        g.fillEllipse(cx - 2.8f, cy - 3.2f, 3.2f, 2.2f);
        break;
    }
    case TransportIcon::Play:
    {
        juce::Path p;
        p.startNewSubPath(cx - 5.5f, cy - 7.0f);
        p.lineTo(cx + 7.0f, cy);
        p.lineTo(cx - 5.5f, cy + 7.0f);
        p.closeSubPath();
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillPath(p, juce::AffineTransform::translation(0.0f, 1.0f));
        g.setColour(isOn ? juce::Colour(0xff4ade80) : inactiveColour);
        g.fillPath(p);
        break;
    }
    case TransportIcon::Rewind:
    {
        // Return-to-start icon |◀ (exact match to Screenshot 2)
        const juce::Colour col = shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown ? juce::Colours::white : inactiveColour;
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRect(cx - 6.5f, cy - 5.0f, 2.2f, 12.0f);
        g.setColour(col);
        g.fillRect(cx - 6.5f, cy - 6.0f, 2.2f, 12.0f);

        juce::Path p;
        p.startNewSubPath(cx - 3.5f, cy);
        p.lineTo(cx + 6.0f, cy - 6.0f);
        p.lineTo(cx + 6.0f, cy + 6.0f);
        p.closeSubPath();
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillPath(p, juce::AffineTransform::translation(0.0f, 1.0f));
        g.setColour(col);
        g.fillPath(p);
        break;
    }
    case TransportIcon::Stop:
    {
        constexpr float s = 10.0f;
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(cx - s * 0.5f, cy - s * 0.5f + 1.0f, s, s, 1.5f);
        g.setColour(shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown ? juce::Colours::white : inactiveColour);
        g.fillRoundedRectangle(cx - s * 0.5f, cy - s * 0.5f, s, s, 1.5f);
        break;
    }
    case TransportIcon::Loop:
    {
        // Sleek ribbon loop / cycle glyph (matching Screenshot 2)
        g.setColour(isOn ? activeColour : inactiveColour);
        juce::Path ribbon;
        ribbon.addArc(cx - 5.2f, cy - 6.5f, 10.4f, 10.4f,
                      -juce::MathConstants<float>::pi * 0.85f, juce::MathConstants<float>::pi * 0.85f, true);
        g.strokePath(ribbon, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // Crossed tails at bottom
        g.drawLine(cx - 4.2f, cy + 2.5f, cx + 5.8f, cy + 6.5f, 2.0f);
        g.drawLine(cx + 4.2f, cy + 2.5f, cx - 5.8f, cy + 6.5f, 2.0f);
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
        juce::Path m;
        m.startNewSubPath(cx - 3.5f, cy - 7.0f);
        m.lineTo(cx + 3.5f, cy - 7.0f);
        m.lineTo(cx + 6.5f, cy + 7.0f);
        m.lineTo(cx - 6.5f, cy + 7.0f);
        m.closeSubPath();
        g.strokePath(m, juce::PathStrokeType(1.5f));

        juce::Path arm;
        arm.startNewSubPath(cx, cy + 5.5f);
        arm.lineTo(cx + 4.5f, cy - 5.0f);
        g.strokePath(arm, juce::PathStrokeType(1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

        g.fillRect(cx + 2.0f, cy - 2.0f, 3.0f, 3.0f);
        break;
    }
    case TransportIcon::Groove:
    {
        g.setColour(isOn ? activeColour : inactiveColour);
        g.fillEllipse(cx - 6.5f, cy + 1.0f, 5.0f, 4.0f);
        g.fillRect(cx - 2.5f, cy - 7.0f, 1.4f, 10.0f);
        juce::Path flag;
        flag.startNewSubPath(cx - 1.1f, cy - 7.0f);
        flag.quadraticTo(cx + 2.5f, cy - 5.0f, cx + 1.0f, cy - 1.5f);
        g.strokePath(flag, juce::PathStrokeType(1.3f));

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
// MasterMeterSlider (Screenshot 2 recessed LED level meter + brushed-silver thumb)
// ==============================================================================
class TransportBar::MasterMeterSlider : public juce::Component, public juce::SettableTooltipClient
{
public:
    MasterMeterSlider(Song& song, AudioEngine& engine)
        : song_(song), engine_(engine)
    {
        setTooltip("Master Output Level & Volume (Drag slider thumb to adjust, double-click to reset to 100%)");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void updateLevels(float peakL, float peakR, bool clipped)
    {
        peakL_ = std::max(peakL, peakL_ * 0.82f);
        peakR_ = std::max(peakR, peakR_ * 0.82f);
        clipped_ = clipped;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced(0.5f, 3.0f);
        const float radius = r.getHeight() * 0.5f;

        // Recessed dark capsule slot
        juce::ColourGradient slotGrad(juce::Colour(0xff0d0d10), r.getX(), r.getY(),
                                      juce::Colour(0xff1b1b20), r.getX(), r.getBottom(), false);
        g.setGradientFill(slotGrad);
        g.fillRoundedRectangle(r, radius);

        // Subtle inner shadow & bottom lip highlight
        g.setColour(juce::Colour(0xff09090b));
        g.drawRoundedRectangle(r, radius, 1.1f);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawHorizontalLine(juce::roundToInt(r.getBottom() + 0.5f), r.getX() + radius, r.getRight() - radius);

        // Dual stereo LED dot rows (14 segments each, green -> amber -> glowing orange-red)
        constexpr int kNumLeds = 14;
        const float padX = 11.0f;
        const float usableW = std::max(10.0f, r.getWidth() - padX * 2.0f);
        const float stepX = usableW / static_cast<float>(kNumLeds - 1);
        const float rowY[2] = {r.getCentreY() - 3.6f, r.getCentreY() + 3.6f};
        const float levels[2] = {std::clamp(peakL_, 0.0f, 1.0f), std::clamp(peakR_, 0.0f, 1.0f)};

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < kNumLeds; ++i)
            {
                const float frac = static_cast<float>(i + 1) / static_cast<float>(kNumLeds);
                const float lx = r.getX() + padX + static_cast<float>(i) * stepX;
                const float ly = rowY[ch];
                const bool isPeakDot = (i == kNumLeds - 1);
                const bool lit = (levels[ch] >= frac * 0.85f) || (isPeakDot && clipped_);

                juce::Colour dotCol;
                if (isPeakDot)
                    dotCol = lit ? juce::Colour(0xffff5238) : juce::Colour(0xff52251d);
                else if (i >= kNumLeds - 3)
                    dotCol = lit ? juce::Colour(0xffff9f1c) : juce::Colour(0xff2c2c33);
                else
                    dotCol = lit ? juce::Colour(0xff34d399) : juce::Colour(0xff28282f);

                const float dotR = isPeakDot ? 2.4f : 1.7f;
                if (lit)
                {
                    g.setColour(dotCol.withAlpha(0.35f));
                    g.fillEllipse(lx - dotR - 1.5f, ly - dotR - 1.5f, (dotR + 1.5f) * 2.0f, (dotR + 1.5f) * 2.0f);
                }
                g.setColour(dotCol);
                g.fillEllipse(lx - dotR, ly - dotR, dotR * 2.0f, dotR * 2.0f);
            }
        }

        // Brushed-silver circular volume thumb (matching Screenshot 2)
        const float normVol = std::clamp(song_.masterVolume() / 1.5f, 0.0f, 1.0f);
        const float thumbR = r.getHeight() * 0.46f;
        const float minX = r.getX() + thumbR + 2.0f;
        const float maxX = r.getRight() - thumbR - 2.0f;
        const float tx = minX + normVol * (maxX - minX);
        const float ty = r.getCentreY();

        // Soft drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(tx - thumbR, ty - thumbR + 1.2f, thumbR * 2.0f, thumbR * 2.0f);

        // Brushed silver gradient
        juce::ColourGradient thumbGrad(juce::Colour(0xffc6c6ce), tx - thumbR * 0.5f, ty - thumbR,
                                       juce::Colour(0xff64646e), tx + thumbR * 0.5f, ty + thumbR, false);
        thumbGrad.addColour(0.5, juce::Colour(0xff8e8e98));
        g.setGradientFill(thumbGrad);
        g.fillEllipse(tx - thumbR, ty - thumbR, thumbR * 2.0f, thumbR * 2.0f);

        // Subtle inner rim highlight & dark border
        g.setColour(juce::Colours::white.withAlpha(0.38f));
        g.drawEllipse(tx - thumbR + 1.0f, ty - thumbR + 1.0f, (thumbR - 1.0f) * 2.0f, (thumbR - 1.0f) * 2.0f, 0.8f);
        g.setColour(juce::Colour(0xff141418));
        g.drawEllipse(tx - thumbR, ty - thumbR, thumbR * 2.0f, thumbR * 2.0f, 1.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;
        updateVolumeFromX(e.position.x);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        updateVolumeFromX(e.position.x);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        song_.setMasterVolume(1.0f);
        engine_.sequencer().setMasterVolume(1.0f);
        engine_.sequencer().resetClipped();
        repaint();
    }

private:
    void updateVolumeFromX(float x)
    {
        const auto r = getLocalBounds().toFloat().reduced(0.5f, 3.0f);
        const float thumbR = r.getHeight() * 0.46f;
        const float minX = r.getX() + thumbR + 2.0f;
        const float maxX = r.getRight() - thumbR - 2.0f;
        const float norm = std::clamp((x - minX) / std::max(1.0f, maxX - minX), 0.0f, 1.0f);
        const float vol = norm * 1.5f;
        song_.setMasterVolume(vol);
        engine_.sequencer().setMasterVolume(vol);
        repaint();
    }

    Song& song_;
    AudioEngine& engine_;
    float peakL_ = 0.0f;
    float peakR_ = 0.0f;
    bool clipped_ = false;
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
    modeButton_.setColour(juce::TextButton::buttonColourId, Palette::surface);
    modeButton_.setColour(juce::TextButton::textColourOffId, Palette::text);
    modeButton_.setColour(juce::TextButton::buttonOnColourId, Palette::accent.withAlpha(0.35f));
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

    // Master Level & Volume Capsule (Screenshot 2)
    masterMeterSlider_ = std::make_unique<MasterMeterSlider>(song_, engine_);
    addAndMakeVisible(masterMeterSlider_.get());

    // Master Project Key selector
    keyCombo_.setTooltip("Master Project Key (ACID automatic loop key conforming)");
    keyCombo_.addItem("Key: Off", 100);
    static constexpr const char* kPitchNames[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    for (int i = 0; i < 12; ++i)
        keyCombo_.addItem(juce::String("Key: ") + kPitchNames[i], i + 1);
    keyCombo_.setSelectedId(100, juce::dontSendNotification);
    keyCombo_.onChange = [this]() {
        const int id = keyCombo_.getSelectedId();
        if (id == 100)
            song_.setProjectRootNote(-1);
        else if (id >= 1 && id <= 12)
            song_.setProjectRootNote(60 + (id - 1));
        if (onModelModified_)
            onModelModified_();
        if (onShowToast_)
        {
            const int rn = song_.projectRootNote();
            onShowToast_(rn >= 0 ? ("Master Key: " + juce::String(Sample::pitchClassName(rn))) : "Master Key Conforming: OFF");
        }
    };
    addAndMakeVisible(keyCombo_);

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
    constexpr int totalW = 1028;
    const int startX = (getWidth() >= totalW + 24) ? std::max(12, (getWidth() - totalW) / 2) : 8;
    constexpr int barY = 7;
    constexpr int barH = 34;
    const int meterX = startX + 218 + 10;
    const int posBoxX = meterX + 136 + 10;
    const int loopBoxX = posBoxX + 122 + 6;
    const auto loopBox = juce::Rectangle<int>(loopBoxX, barY, 104, barH);

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
    const int rn = song_.projectRootNote();
    keyCombo_.setSelectedId(rn >= 0 ? ((rn % 12) + 1) : 100, juce::dontSendNotification);
    repaint();
}

void TransportBar::paint(juce::Graphics& g)
{
    // Brushed dark-metallic play bar gradient (matching Screenshot 2)
    {
        juce::ColourGradient barGrad(juce::Colour(0xff48484f), 0.0f, 0.0f,
                                     juce::Colour(0xff232328), 0.0f, static_cast<float>(getHeight()), false);
        barGrad.addColour(0.48, juce::Colour(0xff323238));
        g.setGradientFill(barGrad);
        g.fillAll();
    }

    // Crisp top seam & specular highlight
    g.setColour(juce::Colour(0xff141418));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawHorizontalLine(1, 0.0f, static_cast<float>(getWidth()));

    constexpr int totalW = 1028;
    constexpr int barY = 7;
    constexpr int barH = 34;

    const int startX = (getWidth() >= totalW + 24) ? std::max(12, (getWidth() - totalW) / 2) : 8;

    const auto drawRecessedLcd = [&g](juce::Rectangle<int> box) {
        const auto f = box.toFloat();
        juce::ColourGradient lcdGrad(juce::Colour(0xff141418), f.getX(), f.getY(),
                                     juce::Colour(0xff1f1f25), f.getX(), f.getBottom(), false);
        g.setGradientFill(lcdGrad);
        g.fillRoundedRectangle(f, 5.5f);
        g.setColour(juce::Colour(0xff0d0d10));
        g.drawRoundedRectangle(f, 5.5f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.09f));
        g.drawHorizontalLine(box.getBottom(), f.getX() + 5.0f, f.getRight() - 5.0f);
    };

    const auto drawMetallicCapsule = [&g](juce::Rectangle<int> box) {
        const auto f = box.toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(f.translated(0.0f, 1.0f), 6.0f);
        g.setColour(juce::Colour(0xff141418));
        g.fillRoundedRectangle(f, 6.0f);
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(f.expanded(0.5f), 6.2f, 0.8f);
    };

    // 1. Unified Metallic Transport Capsule ([ |◀ | ▶ | ● | ■ | Loop | 1234 ])
    const auto transBox = juce::Rectangle<int>(startX, barY, 218, barH);
    drawMetallicCapsule(transBox);

    // 2. Recessed Master Level & Volume Capsule (drawn by masterMeterSlider_)
    const int meterX = startX + 218 + 10;

    // 3. Position Display LCD
    const int posBoxX = meterX + 136 + 10;
    const auto posBox = juce::Rectangle<int>(posBoxX, barY, 122, barH);
    drawRecessedLcd(posBox);

    // 4. Loop Range LCD (Start / End)
    const int loopBoxX = posBoxX + 122 + 6;
    const auto loopBox = juce::Rectangle<int>(loopBoxX, barY, 104, barH);
    drawRecessedLcd(loopBox);

    g.setColour(juce::Colour(0xff2c2c34));
    g.drawHorizontalLine(barY + 17, static_cast<float>(loopBoxX + 2), static_cast<float>(loopBoxX + 102));
    g.drawVerticalLine(loopBoxX + 40, static_cast<float>(barY + 2), static_cast<float>(barY + barH - 2));

    g.setFont(Typography::bold(9.5f));
    g.setColour(Palette::textDim);
    g.drawText("Start", loopBoxX + 4, barY + 1, 34, 16, juce::Justification::centredLeft);
    g.drawText("End", loopBoxX + 4, barY + 17, 34, 16, juce::Justification::centredLeft);

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
    g.setColour(juce::Colour(0xffe8e8f2));
    g.drawText(startStr, loopBoxX + 42, barY + 1, 58, 16, juce::Justification::centred);
    g.drawText(endStr, loopBoxX + 42, barY + 17, 58, 16, juce::Justification::centred);

    // 5. Mode button is self-drawn
    const int modeBtnX = loopBoxX + 104 + 6;

    // 6. Tempo LCD Box
    const int tempoBoxX = modeBtnX + 68 + 6;
    const auto tempoBox = juce::Rectangle<int>(tempoBoxX, barY, 66, barH);
    drawRecessedLcd(tempoBox);

    g.setFont(Typography::bold(9.0f));
    g.setColour(Palette::textDim);
    g.drawText("BPM", tempoBoxX + 4, barY + 2, 42, 11, juce::Justification::centred);

    g.setColour(juce::Colour(0xff2c2c34));
    g.drawVerticalLine(tempoBoxX + 46, static_cast<float>(barY + 2), static_cast<float>(barY + barH - 2));

    // 7. Metronome metallic capsule
    const int metroBoxX = tempoBoxX + 66 + 6;
    drawMetallicCapsule(juce::Rectangle<int>(metroBoxX, barY, 36, barH));

    // 8. Time Signature is drawn by timeSigButton_
    const int timeSigX = metroBoxX + 36 + 6;

    // 9. Swing / Groove LCD box
    const int swingBoxX = timeSigX + 32 + 6;
    constexpr int swingBoxW = 108;
    drawRecessedLcd(juce::Rectangle<int>(swingBoxX, barY, swingBoxW, barH));

    g.setFont(Typography::bold(9.5f));
    const bool hasSwing = song_.swing() > 0.001;
    g.setColour(hasSwing ? Palette::accent : Palette::textDim);
    g.drawText("Swing", swingBoxX + 4, barY + 2, 42, 10, juce::Justification::centred);

    g.setColour(juce::Colour(0xff2c2c34));
    g.drawVerticalLine(swingBoxX + 48, static_cast<float>(barY + 2), static_cast<float>(barY + barH - 2));
}

void TransportBar::resized()
{
    constexpr int totalW = 1028;
    constexpr int barY = 7;
    constexpr int barH = 34;

    const int startX = (getWidth() >= totalW + 24) ? std::max(12, (getWidth() - totalW) / 2) : 8;

    // Unified Metallic Transport Cluster (Screenshot 2 order: |◀  ▶  ●  ■  Loop  1234)
    int btnX = startX + 1;
    constexpr int btnW = 36;

    rewindButton_.setBounds(btnX, barY + 1, btnW, barH - 2);
    rewindButton_.setBorderSides(false, true);
    btnX += btnW;

    playButton_.setBounds(btnX, barY + 1, btnW, barH - 2);
    playButton_.setBorderSides(false, true);
    btnX += btnW;

    recordButton_.setBounds(btnX, barY + 1, btnW, barH - 2);
    recordButton_.setBorderSides(false, true);
    btnX += btnW;

    stopButton_.setBounds(btnX, barY + 1, btnW, barH - 2);
    stopButton_.setBorderSides(false, true);
    btnX += btnW;

    loopButton_.setBounds(btnX, barY + 1, btnW, barH - 2);
    loopButton_.setBorderSides(false, true);
    btnX += btnW;

    countInButton_.setBounds(btnX, barY + 1, btnW, barH - 2);
    countInButton_.setBorderSides(false, false);

    // Recessed Master Level & Volume Capsule (Screenshot 2)
    const int meterX = startX + 218 + 10;
    if (masterMeterSlider_ != nullptr)
        masterMeterSlider_->setBounds(meterX, barY, 136, barH);

    // Position LCD label
    const int posBoxX = meterX + 136 + 10;
    positionLabel_.setBounds(posBoxX + 4, barY + 1, 114, barH - 2);

    // Loop box + Mode button
    const int loopBoxX = posBoxX + 122 + 6;
    const int modeBtnX = loopBoxX + 104 + 6;
    modeButton_.setBounds(modeBtnX, barY, 68, barH);

    // Tempo controls
    const int tempoBoxX = modeBtnX + 68 + 6;
    tempoLabel_.setBounds(tempoBoxX + 2, barY + 13, 44, barH - 14);
    tempoUp_.setBounds(tempoBoxX + 47, barY + 1, 18, 16);
    tempoDown_.setBounds(tempoBoxX + 47, barY + 17, 18, 16);

    // Metronome
    const int metroBoxX = tempoBoxX + 66 + 6;
    metroButton_.setBounds(metroBoxX + 1, barY + 1, 34, barH - 2);

    // Time Signature
    const int timeSigX = metroBoxX + 36 + 6;
    timeSigButton_.setBounds(timeSigX, barY, 32, barH);

    // Swing controls
    const int swingBoxX = timeSigX + 32 + 6;
    if (swingLabel_ != nullptr)
        swingLabel_->setBounds(swingBoxX + 2, barY + 13, 44, barH - 14);
    if (swingSlider_ != nullptr)
        swingSlider_->setBounds(swingBoxX + 51, barY + 7, 52, barH - 14);

    // Master Key selector
    const int keyBoxX = swingBoxX + 108 + 6;
    keyCombo_.setBounds(keyBoxX, barY, 90, barH);
}

void TransportBar::timerCallback()
{
    const bool isPlaying = engine_.sequencer().isPlaying();
    playButton_.setToggleState(isPlaying, juce::dontSendNotification);

    const auto recState = engine_.recorder().state();
    recordButton_.setToggleState(recState != RecordState::Idle, juce::dontSendNotification);

    if (masterMeterSlider_ != nullptr)
    {
        const auto lvl = engine_.sequencer().outputLevels();
        masterMeterSlider_->updateLevels(lvl.peakL, lvl.peakR, lvl.clipped);
    }

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
