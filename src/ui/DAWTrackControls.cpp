#include "ui/DAWTrackControls.h"

#include "ui/TramiLookAndFeel.h"

#include <algorithm>

namespace trami
{

// ==============================================================================
// DAWMuteButton
// ==============================================================================
DAWMuteButton::DAWMuteButton()
    : juce::Button("Mute")
{
    setClickingTogglesState(true);
    setTooltip("Mute Track");
}

void DAWMuteButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    constexpr float radius = 3.5f;

    juce::Path p;
    // Rounded on top-left and bottom-left, flat on the right
    p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius, radius, true, false, true, false);

    const bool isOn = getToggleState();
    juce::Colour bgCol;
    juce::Colour borderCol;
    juce::Colour textCol;

    if (isOn)
    {
        bgCol = juce::Colour(0xffd32f2f);
        borderCol = juce::Colour(0xffef5350);
        textCol = juce::Colours::white;
    }
    else
    {
        bgCol = shouldDrawButtonAsDown ? juce::Colour(0xff22252a)
              : shouldDrawButtonAsHighlighted ? juce::Colour(0xff3c4149)
              : juce::Colour(0xff30343a);
        borderCol = juce::Colour(0xff434952);
        textCol = shouldDrawButtonAsHighlighted ? juce::Colours::white : juce::Colour(0xffc5cad2);
    }

    g.setColour(bgCol);
    g.fillPath(p);

    g.setColour(borderCol);
    g.strokePath(p, juce::PathStrokeType(1.0f));

    g.setColour(textCol);
    g.setFont(Typography::bold(12.0f));
    g.drawText("M", getLocalBounds(), juce::Justification::centred, false);
}

// ==============================================================================
// DAWSoloButton
// ==============================================================================
DAWSoloButton::DAWSoloButton()
    : juce::Button("Solo")
{
    setClickingTogglesState(true);
    setTooltip("Solo Track");
}

void DAWSoloButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    constexpr float radius = 3.5f;

    juce::Path p;
    // Flat on left, rounded on top-right and bottom-right
    p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius, radius, false, true, false, true);

    const bool isOn = getToggleState();
    juce::Colour bgCol;
    juce::Colour borderCol;
    juce::Colour textCol;

    if (isOn)
    {
        bgCol = juce::Colour(0xfffbc02d);
        borderCol = juce::Colour(0xffffee58);
        textCol = juce::Colour(0xff111418);
    }
    else
    {
        bgCol = shouldDrawButtonAsDown ? juce::Colour(0xff22252a)
              : shouldDrawButtonAsHighlighted ? juce::Colour(0xff3c4149)
              : juce::Colour(0xff30343a);
        borderCol = juce::Colour(0xff434952);
        textCol = shouldDrawButtonAsHighlighted ? juce::Colours::white : juce::Colour(0xffc5cad2);
    }

    g.setColour(bgCol);
    g.fillPath(p);

    g.setColour(borderCol);
    g.strokePath(p, juce::PathStrokeType(1.0f));

    g.setColour(textCol);
    g.setFont(Typography::bold(12.0f));
    g.drawText("S", getLocalBounds(), juce::Justification::centred, false);
}

// ==============================================================================
// DAWArmButton
// ==============================================================================
DAWArmButton::DAWArmButton()
    : juce::Button("Arm")
{
    setClickingTogglesState(true);
    setTooltip("Arm Track for Recording");
}

void DAWArmButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto b = getLocalBounds().toFloat();
    const float diameter = std::min(b.getWidth(), b.getHeight()) - 2.0f;
    const auto r = juce::Rectangle<float>(b.getCentreX() - diameter * 0.5f,
                                          b.getCentreY() - diameter * 0.5f,
                                          diameter, diameter);

    const bool isOn = getToggleState();
    juce::Colour bgCol;
    juce::Colour borderCol;
    juce::Colour dotCol;

    if (isOn)
    {
        bgCol = juce::Colour(0xff3d1c1e);
        borderCol = juce::Colour(0xffe53935);
        dotCol = juce::Colour(0xffe53935);
    }
    else
    {
        bgCol = shouldDrawButtonAsDown ? juce::Colour(0xff22252a)
              : shouldDrawButtonAsHighlighted ? juce::Colour(0xff3c4149)
              : juce::Colour(0xff30343a);
        borderCol = juce::Colour(0xff434952);
        dotCol = shouldDrawButtonAsHighlighted ? juce::Colour(0xffb0b8c4) : juce::Colour(0xff7c838e);
    }

    g.setColour(bgCol);
    g.fillEllipse(r);

    g.setColour(borderCol);
    g.drawEllipse(r, 1.0f);

    const float dotRadius = 3.2f;
    g.setColour(dotCol);
    g.fillEllipse(r.getCentreX() - dotRadius, r.getCentreY() - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
}

// ==============================================================================
// DAWInputButton  (single recording source selector)
// ==============================================================================
DAWInputButton::DAWInputButton()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void DAWInputButton::setInputState(int inputChannel, bool stereo, bool armed)
{
    const int clampedCh = std::max(0, inputChannel);
    if (inputChannel_ != clampedCh || stereo_ != stereo || armed_ != armed)
    {
        inputChannel_ = clampedCh;
        stereo_ = stereo;
        armed_ = armed;
        repaint();
    }
}

void DAWInputButton::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    constexpr float radius = 3.5f;

    const juce::Colour baseBg = armed_ ? juce::Colour(0xff3a1d20) : juce::Colour(0xff262930);
    const juce::Colour hoverBg = armed_ ? juce::Colour(0xff4d2529) : juce::Colour(0xff343943);
    const juce::Colour borderCol = armed_ ? juce::Colour(0xffe53935).withAlpha(0.75f) : juce::Colour(0xff434952);

    g.setColour(hover_ ? hoverBg : baseBg);
    g.fillRoundedRectangle(r, radius);
    g.setColour(borderCol);
    g.drawRoundedRectangle(r, radius, 1.0f);

    // Icon: single ring = mono source, two interlocking rings = stereo source
    const juce::Colour iconCol = armed_
        ? juce::Colour(0xffffb4ab)
        : (stereo_ ? juce::Colour(0xff7dd3fc) : juce::Colour(0xfffcd34d));
    g.setColour(iconCol);
    const float cy = r.getCentreY();
    const float circleR = 3.4f;
    const float iconCx = 11.0f;
    if (!stereo_)
    {
        g.drawEllipse(iconCx - circleR, cy - circleR, circleR * 2.0f, circleR * 2.0f, 1.35f);
    }
    else
    {
        const float offset = 2.3f;
        g.drawEllipse(iconCx - offset - circleR, cy - circleR, circleR * 2.0f, circleR * 2.0f, 1.25f);
        g.drawEllipse(iconCx + offset - circleR, cy - circleR, circleR * 2.0f, circleR * 2.0f, 1.25f);
    }

    // Source text: "In 3" (mono) or "In 3+4" (stereo)
    const juce::String text = stereo_
        ? ("In " + juce::String(inputChannel_ + 1) + "+" + juce::String(inputChannel_ + 2))
        : ("In " + juce::String(inputChannel_ + 1));
    g.setColour(armed_ ? juce::Colours::white : juce::Colour(0xffdcdce4));
    g.setFont(Typography::bold(9.5f));
    g.drawText(text, juce::Rectangle<int>(21, 0, std::max(10, getWidth() - 21 - 12), getHeight()),
               juce::Justification::centredLeft, false);

    // Dropdown chevron
    const float chevX = static_cast<float>(getWidth()) - 6.0f;
    juce::Path chev;
    chev.startNewSubPath(chevX - 2.2f, cy - 1.0f);
    chev.lineTo(chevX + 2.2f, cy - 1.0f);
    chev.lineTo(chevX, cy + 1.8f);
    chev.closeSubPath();
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.fillPath(chev);
}

void DAWInputButton::mouseEnter(const juce::MouseEvent&)
{
    hover_ = true;
    repaint();
}

void DAWInputButton::mouseExit(const juce::MouseEvent&)
{
    hover_ = false;
    repaint();
}

void DAWInputButton::mouseDown(const juce::MouseEvent&)
{
    if (onSelectInputChannel)
        onSelectInputChannel();
}

// ==============================================================================
// DAWInputSources
// ==============================================================================
namespace DAWInputSources
{
namespace
{
bool isRedundantChannelName(const juce::String& raw, int ch)
{
    const auto s = raw.trim().toLowerCase();
    if (s.isEmpty())
        return true;
    const auto num = juce::String(ch + 1);
    return s == num || s == ("input " + num) || s == ("in " + num) ||
           s == ("channel " + num) || s == ("ch " + num) || s == ("input" + num) || s == ("in" + num);
}

juce::String meaningfulChannelName(const juce::StringArray& names, int ch)
{
    if (ch >= 0 && ch < names.size() && !isRedundantChannelName(names[ch], ch))
        return names[ch].trim();
    return {};
}
} // namespace

juce::String sourceLabel(int channel, bool stereo, const juce::StringArray& deviceNames)
{
    if (!stereo)
    {
        const auto n = meaningfulChannelName(deviceNames, channel);
        return "Input " + juce::String(channel + 1) + " (Mono)" + (n.isNotEmpty() ? ("  -  " + n) : juce::String());
    }
    const auto n1 = meaningfulChannelName(deviceNames, channel);
    const auto n2 = meaningfulChannelName(deviceNames, channel + 1);
    return "Inputs " + juce::String(channel + 1) + "+" + juce::String(channel + 2) + " (Stereo)" +
           ((n1.isNotEmpty() && n2.isNotEmpty()) ? ("  -  " + n1 + " & " + n2) : juce::String());
}

int channelCount(const juce::StringArray& deviceNames)
{
    return deviceNames.size();
}

void populateMenu(juce::PopupMenu& menu, const juce::StringArray& deviceNames, int currentChannel, bool currentStereo,
                  const juce::String& deviceName, bool includeAudioSettings)
{
    menu.addSectionHeader(deviceName.isNotEmpty() ? ("Recording Source - " + deviceName) : "Recording Source");

    const int n = channelCount(deviceNames);
    if (n <= 0)
    {
        menu.addItem(-1, "No active input channels", false, false);
    }
    else
    {
        for (int ch = 0; ch < n; ++ch)
        {
            const bool isCurrent = (!currentStereo && currentChannel == ch) || (n == 1 && ch == 0);
            menu.addItem(kMonoBase + ch, sourceLabel(ch, false, deviceNames), true, isCurrent);
        }
        if (n >= 2)
        {
            menu.addSeparator();
            for (int ch = 0; ch + 1 < n; ch += 2)
                menu.addItem(kStereoBase + ch, sourceLabel(ch, true, deviceNames), true,
                             currentStereo && currentChannel == ch);
        }
    }

    if (includeAudioSettings)
    {
        menu.addSeparator();
        menu.addItem(kAudioSettingsId, "Audio Settings...");
    }
}

bool decode(int id, int& channel, bool& stereo)
{
    if (id >= kMonoBase && id < kStereoBase)
    {
        channel = id - kMonoBase;
        stereo = false;
        return true;
    }
    if (id >= kStereoBase && id < kStereoBase + 100)
    {
        channel = id - kStereoBase;
        stereo = true;
        return true;
    }
    return false;
}
} // namespace DAWInputSources


// ==============================================================================
// DAWTrackIcons
// ==============================================================================
namespace DAWTrackIcons
{

void drawTrackFoldChevron(juce::Graphics& g, float cx, float cy, float size)
{
    juce::Path chevron;
    const float halfW = size * 0.5f;
    const float h = size * 0.55f;
    chevron.startNewSubPath(cx - halfW, cy - h * 0.5f);
    chevron.lineTo(cx + halfW, cy - h * 0.5f);
    chevron.lineTo(cx, cy + h * 0.5f);
    chevron.closeSubPath();
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillPath(chevron);
}

void drawAudioWaveformIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
{
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    constexpr float barW = 1.8f;
    constexpr float heights[] = { 4.5f, 9.5f, 15.0f, 9.0f, 4.0f };
    constexpr float offsets[] = { -5.5f, -2.75f, 0.0f, 2.75f, 5.5f };

    g.setColour(colour);
    for (int i = 0; i < 5; ++i)
    {
        const float h = heights[i];
        g.fillRoundedRectangle(cx + offsets[i] - barW * 0.5f, cy - h * 0.5f, barW, h, barW * 0.5f);
    }
}

} // namespace DAWTrackIcons

namespace DAWLoopRuler
{

void drawStudioOneLoopIndicator(juce::Graphics& g, int loopStartX, int loopEndX, int rulerHeight, bool active)
{
    juce::ignoreUnused(rulerHeight);
    const float sx = static_cast<float>(loopStartX);
    const float ex = static_cast<float>(loopEndX);
    const float w = std::max(8.0f, ex - sx);

    const juce::Colour fillCol = active ? juce::Colour(0xff42b4e6) : juce::Colour(0xff5a5a66);
    const juce::Colour strokeCol = active ? fillCol.brighter(0.2f) : juce::Colour(0xff727280);
    constexpr float topH = 6.0f;
    constexpr float flagH = 15.0f;
    constexpr float flagW = 3.5f;
    constexpr float chamferW = 5.0f;

    juce::Path p;
    p.startNewSubPath(sx, 0.0f);
    p.lineTo(sx + w, 0.0f);
    p.lineTo(sx + w, flagH);
    p.lineTo(sx + w - flagW, flagH);
    p.lineTo(sx + w - flagW - chamferW, topH);
    p.lineTo(sx + flagW + chamferW, topH);
    p.lineTo(sx + flagW, flagH);
    p.lineTo(sx, flagH);
    p.closeSubPath();

    g.setColour(fillCol);
    g.fillPath(p);

    g.setColour(strokeCol);
    g.strokePath(p, juce::PathStrokeType(0.75f));
}

} // namespace DAWLoopRuler

} // namespace trami

