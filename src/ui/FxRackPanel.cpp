#include "ui/FxRackPanel.h"

#include "model/Timing.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace trami
{

// ============================================================================
// Searchable VST3/AU Plugin Browser List (left pane of FX Rack)
// ============================================================================
class FxRackPanel::PluginBrowserList final : public juce::Component
{
public:
    std::function<void(const juce::PluginDescription&)> onAddPlugin;

    void setPlugins(std::vector<juce::PluginDescription> plugins)
    {
        plugins_ = std::move(plugins);
        hoveredIndex_ = -1;
        selectedIndex_ = plugins_.empty() ? -1 : 0;
        updateSize();
        repaint();
    }

    void setWidthHint(int width)
    {
        if (widthHint_ != width)
        {
            widthHint_ = std::max(100, width);
            updateSize();
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::background);

        if (plugins_.empty())
        {
            g.setColour(Palette::textDim.withAlpha(0.7f));
            g.setFont(Typography::regular(11.0f));
            g.drawText("No matching plugins.\nClick \"Scan...\" to scan VST3/AU.",
                       getLocalBounds().reduced(10), juce::Justification::centred, true);
            return;
        }

        const auto clip = g.getClipBounds();
        for (int i = 0; i < static_cast<int>(plugins_.size()); ++i)
        {
            const juce::Rectangle<int> row(0, i * kRowH, getWidth(), kRowH);
            if (!row.intersects(clip))
                continue;

            const auto& d = plugins_[static_cast<std::size_t>(i)];
            const bool isSel = (i == selectedIndex_);
            const bool isHov = (i == hoveredIndex_);

            if (isSel)
            {
                g.setColour(Palette::accent.withAlpha(0.22f));
                g.fillRect(row);
            }
            else if (isHov)
            {
                g.setColour(juce::Colours::white.withAlpha(0.06f));
                g.fillRect(row);
            }

            g.setColour(Palette::gridLine.withAlpha(0.45f));
            g.drawHorizontalLine(row.getBottom() - 1, 4.0f, static_cast<float>(getWidth() - 4));

            // Right "+" pill button
            const auto addBtn = addButtonRectForRow(row);
            g.setColour((isHov || isSel) ? Palette::accent.withAlpha(0.32f) : Palette::panelHeader);
            g.fillRoundedRectangle(addBtn.toFloat(), 3.0f);
            g.setColour((isHov || isSel) ? Palette::accent : Palette::textDim);
            g.setFont(Typography::bold(12.0f));
            g.drawText("+", addBtn, juce::Justification::centred, false);

            // Format badge (VST3 / AU)
            const auto fmtRect = juce::Rectangle<int>(addBtn.getX() - 36, row.getY() + 6, 32, 14);
            g.setColour(Palette::panelHeader);
            g.fillRoundedRectangle(fmtRect.toFloat(), 2.5f);
            g.setColour(Palette::textDim);
            g.setFont(Typography::bold(8.5f));
            const juce::String shortFmt = d.pluginFormatName.containsIgnoreCase("AudioUnit") ? "AU" : d.pluginFormatName;
            g.drawText(shortFmt, fmtRect, juce::Justification::centred, false);

            // Name + manufacturer
            auto textArea = row.withTrimmedLeft(8).withRight(fmtRect.getX() - 4);
            g.setColour(Palette::text);
            g.setFont(Typography::bold(11.0f));
            g.drawText(d.name, textArea.removeFromTop(15), juce::Justification::centredLeft, true);

            if (d.manufacturerName.isNotEmpty())
            {
                g.setColour(Palette::textDim);
                g.setFont(Typography::regular(9.5f));
                g.drawText(d.manufacturerName, textArea, juce::Justification::centredLeft, true);
            }
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int idx = hitRow(e.y);
        if (idx != hoveredIndex_)
        {
            hoveredIndex_ = idx;
            setMouseCursor(idx >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoveredIndex_ != -1)
        {
            hoveredIndex_ = -1;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int idx = hitRow(e.y);
        if (idx < 0 || idx >= static_cast<int>(plugins_.size()))
            return;

        selectedIndex_ = idx;
        repaint();

        const juce::Rectangle<int> row(0, idx * kRowH, getWidth(), kRowH);
        if (addButtonRectForRow(row).expanded(3).contains(e.getPosition()))
        {
            if (onAddPlugin)
                onAddPlugin(plugins_[static_cast<std::size_t>(idx)]);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        const int idx = hitRow(e.y);
        if (idx >= 0 && idx < static_cast<int>(plugins_.size()) && onAddPlugin)
            onAddPlugin(plugins_[static_cast<std::size_t>(idx)]);
    }

private:
    static constexpr int kRowH = 30;

    static juce::Rectangle<int> addButtonRectForRow(const juce::Rectangle<int>& row)
    {
        return {row.getRight() - 26, row.getY() + 5, 20, 20};
    }

    int hitRow(int y) const
    {
        if (y < 0 || plugins_.empty())
            return -1;
        const int idx = y / kRowH;
        return (idx >= 0 && idx < static_cast<int>(plugins_.size())) ? idx : -1;
    }

    void updateSize()
    {
        const int totalH = std::max(80, static_cast<int>(plugins_.size()) * kRowH);
        setSize(widthHint_, totalH);
    }

    std::vector<juce::PluginDescription> plugins_;
    int hoveredIndex_ = -1;
    int selectedIndex_ = -1;
    int widthHint_ = 220;
};

// ============================================================================
// Hardware-Style FX Rack Module Card (one card per loaded plugin slot)
// ============================================================================
class FxRackPanel::RackModuleCard final : public juce::Component
{
public:
    RackModuleCard(int idx, const PluginSlot& slot, juce::Colour accent,
                   std::function<void()> onBypass, std::function<void()> onEdit,
                   std::function<void()> onLeft, std::function<void()> onRight,
                   std::function<void()> onRemove, bool canLeft, bool canRight)
        : index_(idx), slot_(slot), accent_(accent), onEdit_(std::move(onEdit))
    {
        powerBtn_.setButtonText(slot.bypassed ? "BYP" : "ON");
        powerBtn_.setClickingTogglesState(true);
        powerBtn_.setToggleState(!slot.bypassed, juce::dontSendNotification);
        powerBtn_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff06b6d4).withAlpha(0.38f));
        powerBtn_.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff22d3ee));
        powerBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb45309).withAlpha(0.42f));
        powerBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffbbf24));
        powerBtn_.setTooltip(slot.bypassed ? "Plugin Bypassed (click to enable)" : "Plugin Active (click to bypass)");
        powerBtn_.onClick = std::move(onBypass);
        addAndMakeVisible(powerBtn_);

        leftBtn_.setEnabled(canLeft);
        leftBtn_.setTooltip("Move earlier in signal chain");
        leftBtn_.onClick = std::move(onLeft);
        addAndMakeVisible(leftBtn_);

        rightBtn_.setEnabled(canRight);
        rightBtn_.setTooltip("Move later in signal chain");
        rightBtn_.onClick = std::move(onRight);
        addAndMakeVisible(rightBtn_);

        removeBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff87171));
        removeBtn_.setTooltip("Remove plugin from rack");
        removeBtn_.onClick = std::move(onRemove);
        addAndMakeVisible(removeBtn_);

        editBtn_.setButtonText("UI");
        editBtn_.setTooltip("Open / close the native VST3/AU plugin editor window (click again to close)");
        editBtn_.onClick = [this]()
        {
            if (onEdit_)
                onEdit_();
        };
        addAndMakeVisible(editBtn_);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (onEdit_)
            onEdit_();
    }

    void paint(juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced(1.0f);

        // Brushed hardware module faceplate
        juce::ColourGradient grad(
            slot_.bypassed ? juce::Colour(0xff25252b) : juce::Colour(0xff30313a),
            0.0f, b.getY(),
            slot_.bypassed ? juce::Colour(0xff1c1c21) : juce::Colour(0xff22232a),
            0.0f, b.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(b, 6.0f);

        // Top accent bar (dimmed when bypassed)
        g.setColour(slot_.bypassed ? juce::Colour(0xffb45309).withAlpha(0.65f) : accent_);
        g.fillRoundedRectangle(b.getX() + 1.0f, b.getY() + 1.0f, b.getWidth() - 2.0f, 4.0f, 2.0f);

        // Outer bezel border
        g.setColour(slot_.bypassed ? Palette::gridLine : accent_.withAlpha(0.45f));
        g.drawRoundedRectangle(b, 6.0f, 1.0f);

        // Subtle rack corner screws
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        const float r = 2.0f;
        g.fillEllipse(b.getX() + 5.0f, b.getBottom() - 9.0f, r * 2.0f, r * 2.0f);
        g.fillEllipse(b.getRight() - 9.0f, b.getBottom() - 9.0f, r * 2.0f, r * 2.0f);

        // Slot number badge
        g.setColour(Palette::textDim);
        g.setFont(Typography::bold(10.0f));
        g.drawText("#" + juce::String(index_ + 1), 8, 9, 24, 18, juce::Justification::centredLeft, false);

        // Header divider
        g.setColour(Palette::gridLine.withAlpha(0.8f));
        g.drawHorizontalLine(31, b.getX() + 4.0f, b.getRight() - 4.0f);

        // Plugin name (centered in module body)
        auto body = getLocalBounds().reduced(10, 0).withTop(36).withBottom(getHeight() - 36);
        g.setColour(slot_.bypassed ? Palette::textDim : Palette::text);
        g.setFont(Typography::bold(12.5f));
        g.drawFittedText(juce::String(slot_.name), body.withTrimmedBottom(18), juce::Justification::centred, 2, 0.85f);

        // Format + Latency pill badge
        if (!slot_.format.empty() || slot_.latencySamples > 0)
        {
            juce::String fmt = juce::String(slot_.format).containsIgnoreCase("AudioUnit") ? "AU" : juce::String(slot_.format);
            if (slot_.latencySamples > 0)
            {
                if (fmt.isNotEmpty())
                    fmt += " · ";
                fmt += juce::String(slot_.latencySamples) + " smp";
            }
            const float pillW = (slot_.latencySamples > 0) ? 94.0f : 44.0f;
            const auto pill = juce::Rectangle<float>(static_cast<float>(getWidth()) * 0.5f - pillW * 0.5f,
                                                     static_cast<float>(body.getBottom() - 16), pillW, 14.0f);
            g.setColour(Palette::background.withAlpha(0.85f));
            g.fillRoundedRectangle(pill, 3.0f);
            g.setColour(Palette::textDim);
            g.setFont(Typography::bold(8.5f));
            g.drawText(fmt, pill.toNearestInt(), juce::Justification::centred, false);
        }
    }

    void resized() override
    {
        auto top = juce::Rectangle<int>(30, 8, getWidth() - 36, 19);
        removeBtn_.setBounds(top.removeFromRight(20));
        top.removeFromRight(2);
        rightBtn_.setBounds(top.removeFromRight(20));
        top.removeFromRight(2);
        leftBtn_.setBounds(top.removeFromRight(20));
        top.removeFromRight(4);
        powerBtn_.setBounds(top.removeFromLeft(36));

        editBtn_.setBounds(14, getHeight() - 30, getWidth() - 28, 22);
    }

private:
    int index_ = 0;
    PluginSlot slot_;
    juce::Colour accent_;
    std::function<void()> onEdit_;

    juce::TextButton powerBtn_;
    juce::TextButton leftBtn_{"<"};
    juce::TextButton rightBtn_{">"};
    juce::TextButton removeBtn_{"x"};
    juce::TextButton editBtn_;
};

// ============================================================================
// Horizontal Signal-Flow Rack Canvas (draws IN -> [Card] -> [Card] -> OUT)
// ============================================================================
class FxRackPanel::RackCanvas final : public juce::Component
{
public:
    void setMeta(int numModules, juce::Colour accent, juce::String emptyMsg)
    {
        numModules_ = numModules;
        accent_ = accent;
        emptyMsg_ = std::move(emptyMsg);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::background);

        if (numModules_ == 0)
        {
            g.setColour(Palette::textDim.withAlpha(0.75f));
            g.setFont(Typography::regular(12.0f));
            g.drawText(emptyMsg_, getLocalBounds().reduced(20), juce::Justification::centred, true);
            return;
        }

        const float cy = static_cast<float>(getHeight()) * 0.5f;

        auto drawBadge = [&](int x, const juce::String& text)
        {
            const juce::Rectangle<float> r(static_cast<float>(x), cy - 11.0f, 30.0f, 22.0f);
            g.setColour(Palette::panelHeader);
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(Palette::gridLine);
            g.drawRoundedRectangle(r, 4.0f, 1.0f);
            g.setColour(Palette::textDim);
            g.setFont(Typography::bold(9.5f));
            g.drawText(text, r.toNearestInt(), juce::Justification::centred, false);
        };

        auto drawArrow = [&](float x1, float x2)
        {
            g.setColour(accent_.withAlpha(0.55f));
            g.drawLine(x1, cy, x2 - 4.0f, cy, 1.5f);
            juce::Path head;
            head.startNewSubPath(x2 - 5.0f, cy - 3.5f);
            head.lineTo(x2, cy);
            head.lineTo(x2 - 5.0f, cy + 3.5f);
            head.closeSubPath();
            g.fillPath(head);
        };

        drawBadge(kMarginX, "IN");
        int curX = kMarginX + 30;
        for (int i = 0; i < numModules_; ++i)
        {
            drawArrow(static_cast<float>(curX + 2), static_cast<float>(curX + kArrowW - 2));
            curX += kArrowW + kCardW;
        }
        drawArrow(static_cast<float>(curX + 2), static_cast<float>(curX + kArrowW - 2));
        drawBadge(curX + kArrowW, "OUT");
    }

    static constexpr int kMarginX = 12;
    static constexpr int kInOutW = 30;
    static constexpr int kArrowW = 20;
    static constexpr int kCardW = 166;

private:
    int numModules_ = 0;
    juce::Colour accent_{Palette::accent};
    juce::String emptyMsg_;
};

// ============================================================================
// FxRackPanel
// ============================================================================
FxRackPanel::FxRackPanel(Song& song, AudioEngine& engine, PatternId patternId)
    : song_(song), engine_(engine), patternId_(patternId)
{
    trackScopeBtn_.setTooltip("Show and edit Track FX chain");
    trackScopeBtn_.onClick = [this]() { setScope(Scope::Track); };
    addAndMakeVisible(trackScopeBtn_);

    clipScopeBtn_.setTooltip("Show and edit Single Clip FX chain");
    clipScopeBtn_.onClick = [this]() { setScope(Scope::Clip); };
    addAndMakeVisible(clipScopeBtn_);

    masterScopeBtn_.setTooltip("Show and edit Master Bus FX chain");
    masterScopeBtn_.onClick = [this]() { setScope(Scope::Master); };
    addAndMakeVisible(masterScopeBtn_);

    targetSelector_.setTextWhenNothingSelected("Select target...");
    targetSelector_.setTextWhenNoChoicesAvailable("No targets available");
    targetSelector_.onChange = [this]()
    {
        if (isUpdatingSelectors_)
            return;
        const int id = targetSelector_.getSelectedId();
        if (scope_ == Scope::Track && id > 0)
        {
            targetTrack_ = id - 1;
            loadChainFromModel();
        }
        else if (scope_ == Scope::Clip && id > 0 && id <= static_cast<int>(clipChoices_.size()))
        {
            targetClip_ = clipChoices_[static_cast<std::size_t>(id - 1)];
            const int trk = findTrackForClip(targetClip_);
            if (trk >= 0)
                targetTrack_ = trk;
            loadChainFromModel();
        }
    };
    addAndMakeVisible(targetSelector_);

    masterTargetLabel_.setText("Master Bus Output", juce::dontSendNotification);
    masterTargetLabel_.setFont(Typography::bold(12.0f));
    masterTargetLabel_.setColour(juce::Label::textColourId, juce::Colour(0xfffca5a5));
    addAndMakeVisible(masterTargetLabel_);

    jumpButton_.setTooltip("Jump to and highlight this Track or Clip in the timeline");
    jumpButton_.onClick = [this]()
    {
        if (scope_ == Scope::Clip && targetClip_.isValid() && onJumpToClip)
            onJumpToClip(targetClip_);
        else if (scope_ == Scope::Track && targetTrack_ >= 0 && onJumpToTrack)
            onJumpToTrack(targetTrack_);
    };
    addAndMakeVisible(jumpButton_);

    switchPartnerButton_.onClick = [this]()
    {
        if (scope_ == Scope::Clip)
        {
            const int trk = findTrackForClip(targetClip_);
            if (trk >= 0)
                showTrackFx(trk);
        }
        else if (scope_ == Scope::Track && targetClip_.isValid())
        {
            showClipFx(targetClip_);
        }
    };
    addAndMakeVisible(switchPartnerButton_);

    pdcToggleButton_.setClickingTogglesState(true);
    pdcToggleButton_.setToggleState(song_.pdcEnabled(), juce::dontSendNotification);
    pdcToggleButton_.setColour(juce::TextButton::buttonOnColourId, Palette::accent.withAlpha(0.35f));
    pdcToggleButton_.setColour(juce::TextButton::textColourOnId, Palette::accent);
    pdcToggleButton_.setTooltip("Plugin Delay Compensation (PDC): Automatically time-align channels for plugin & channel latency");
    pdcToggleButton_.onClick = [this]()
    {
        if (onPushUndo)
            onPushUndo();
        const bool on = pdcToggleButton_.getToggleState();
        song_.setPdcEnabled(on);
        engine_.sequencer().setPdcEnabled(on);
        rebuildRackModules();
        if (onChainModified)
            onChainModified();
        repaint();
    };
    addAndMakeVisible(pdcToggleButton_);

    bypassAllButton_.setClickingTogglesState(true);
    bypassAllButton_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffb45309));
    bypassAllButton_.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    bypassAllButton_.setTooltip("Bypass or enable all plugins in this FX chain");
    bypassAllButton_.onClick = [this]()
    {
        if (chain_.empty() || !hasValidTarget())
            return;
        const bool bypass = bypassAllButton_.getToggleState();
        auto next = chain_;
        for (auto& s : next)
            s.bypassed = bypass;
        commitChain(std::move(next));
    };
    addAndMakeVisible(bypassAllButton_);

    // Left pane: Plugin Browser
    browserTitleLabel_.setText("PLUGINS", juce::dontSendNotification);
    browserTitleLabel_.setFont(Typography::bold(10.5f));
    browserTitleLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    addAndMakeVisible(browserTitleLabel_);

    scanButton_.setTooltip("Scan system VST3 and AudioUnit folders for external effect plugins");
    scanButton_.onClick = [this]() { toggleScanPlugins(); };
    addAndMakeVisible(scanButton_);

    searchBox_.setTextToShowWhenEmpty("Search VST3 / AU...", Palette::textDim.withAlpha(0.6f));
    searchBox_.setColour(juce::TextEditor::backgroundColourId, Palette::background);
    searchBox_.setColour(juce::TextEditor::textColourId, Palette::text);
    searchBox_.setColour(juce::TextEditor::outlineColourId, Palette::gridLine);
    searchBox_.onTextChange = [this]() { applyPluginFilter(); };
    addAndMakeVisible(searchBox_);

    browserList_ = std::make_unique<PluginBrowserList>();
    browserList_->onAddPlugin = [this](const juce::PluginDescription& desc) { addPluginFromDescription(desc); };
    browserViewport_.setViewedComponent(browserList_.get(), false);
    browserViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(browserViewport_);

    statusLabel_.setFont(Typography::regular(10.0f));
    statusLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel_);

    // Right pane: Horizontal Signal-Flow Rack
    rackCanvas_ = std::make_unique<RackCanvas>();
    rackViewport_.setViewedComponent(rackCanvas_.get(), false);
    rackViewport_.setScrollBarsShown(false, true, false, true);
    addAndMakeVisible(rackViewport_);

    refreshAvailablePlugins();
    setScope(Scope::Track);
}

FxRackPanel::~FxRackPanel()
{
    stopTimer();
    if (engine_.pluginHost().isScanning())
        engine_.pluginHost().finishPluginScan();
}

void FxRackPanel::setPattern(PatternId patternId)
{
    patternId_ = patternId;
    targetClip_ = {};
    targetTrack_ = 0;
    refreshFromModel();
}

void FxRackPanel::refreshFromModel()
{
    rebuildTargetSelector();
    loadChainFromModel();
    updateNavigationControls();
    repaint();
}

void FxRackPanel::showTrackFx(int trackIndex)
{
    targetTrack_ = trackIndex;
    setScope(Scope::Track);
}

void FxRackPanel::showClipFx(ClipId clipId)
{
    targetClip_ = clipId;
    const int trk = findTrackForClip(clipId);
    if (trk >= 0)
        targetTrack_ = trk;
    setScope(Scope::Clip);
}

void FxRackPanel::showMasterFx()
{
    setScope(Scope::Master);
}

void FxRackPanel::notifyTrackSelected(int trackIndex)
{
    targetTrack_ = trackIndex;
    if (scope_ == Scope::Track)
        refreshFromModel();
}

void FxRackPanel::notifyClipSelected(ClipId clipId, int trackIndex)
{
    targetClip_ = clipId;
    if (trackIndex >= 0)
        targetTrack_ = trackIndex;

    if (scope_ == Scope::Clip)
    {
        refreshFromModel();
    }
    else if (scope_ == Scope::Track)
    {
        if (trackIndex >= 0)
            targetTrack_ = trackIndex;
        refreshFromModel();
    }
}

void FxRackPanel::setScope(Scope newScope)
{
    scope_ = newScope;

    const bool isTrk = (scope_ == Scope::Track);
    const bool isClp = (scope_ == Scope::Clip);
    const bool isMst = (scope_ == Scope::Master);

    trackScopeBtn_.setColour(juce::TextButton::buttonColourId, isTrk ? Palette::accent.withAlpha(0.32f) : Palette::surface);
    clipScopeBtn_.setColour(juce::TextButton::buttonColourId, isClp ? juce::Colour(0xff22d3ee).withAlpha(0.32f) : Palette::surface);
    masterScopeBtn_.setColour(juce::TextButton::buttonColourId, isMst ? juce::Colour(0xffef4444).withAlpha(0.38f) : Palette::surface);

    targetSelector_.setVisible(!isMst);
    masterTargetLabel_.setVisible(isMst);

    rebuildTargetSelector();
    loadChainFromModel();
    updateNavigationControls();
    resized();
    repaint();
}

int FxRackPanel::findTrackForClip(ClipId clipId) const
{
    if (!clipId.isValid())
        return -1;
    const auto* pat = song_.findPattern(patternId_);
    if (pat == nullptr)
        return -1;
    for (int t = 0; t < static_cast<int>(pat->tracks.size()); ++t)
    {
        for (const auto& c : pat->tracks[static_cast<std::size_t>(t)].clips)
        {
            if (c.id == clipId)
                return t;
        }
    }
    return -1;
}

bool FxRackPanel::hasValidTarget() const
{
    const auto* pat = song_.findPattern(patternId_);
    if (pat == nullptr)
        return false;
    if (scope_ == Scope::Master)
        return true;
    if (scope_ == Scope::Track)
        return targetTrack_ >= 0 && targetTrack_ < static_cast<int>(pat->tracks.size());
    return targetClip_.isValid() && song_.findClip(patternId_, targetClip_) != nullptr;
}

PluginHost::TargetKind FxRackPanel::currentHostTargetKind() const
{
    if (scope_ == Scope::Clip)
        return PluginHost::TargetKind::Clip;
    if (scope_ == Scope::Master)
        return PluginHost::TargetKind::Master;
    return PluginHost::TargetKind::Track;
}

std::uint32_t FxRackPanel::currentHostTargetId() const
{
    if (scope_ == Scope::Clip)
        return targetClip_.value;
    if (scope_ == Scope::Master)
        return 0;
    return static_cast<std::uint32_t>(std::max(0, targetTrack_));
}

juce::Colour FxRackPanel::currentAccentColour() const
{
    if (scope_ == Scope::Master)
        return juce::Colour(0xffef4444);

    static constexpr std::uint32_t kModernPalette[] = {
        0xff63d988, 0xffb4a2f0, 0xff72c4fa, 0xfff2bf66, 0xfff294ab,
        0xff6ad8cc, 0xff9d94f2, 0xffa2dc6d, 0xffefa274, 0xff85b8f8
    };
    constexpr std::size_t kPaletteCount = sizeof(kModernPalette) / sizeof(kModernPalette[0]);

    const auto* pat = song_.findPattern(patternId_);
    int trkIdx = (scope_ == Scope::Clip) ? findTrackForClip(targetClip_) : targetTrack_;
    if (pat != nullptr && trkIdx >= 0 && trkIdx < static_cast<int>(pat->tracks.size()))
    {
        const auto& trk = pat->tracks[static_cast<std::size_t>(trkIdx)];
        if (trk.colorArgb != 0)
            return juce::Colour(trk.colorArgb);
        SampleId sid = trk.sampleId;
        if (!sid.isValid() && !trk.clips.empty())
            sid = trk.clips.front().sampleId;
        if (sid.isValid())
            return juce::Colour(kModernPalette[(sid.value - 1) % kPaletteCount]);
    }
    return Palette::accent;
}

juce::String FxRackPanel::currentSummaryText() const
{
    if (scope_ == Scope::Master)
        return "Master Bus FX";

    const auto* pat = song_.findPattern(patternId_);
    if (pat == nullptr)
        return "FX";

    if (scope_ == Scope::Track)
    {
        if (targetTrack_ >= 0 && targetTrack_ < static_cast<int>(pat->tracks.size()))
        {
            const auto& trk = pat->tracks[static_cast<std::size_t>(targetTrack_)];
            juce::String name = trk.name.empty() ? ("Track " + juce::String(targetTrack_ + 1))
                                                 : juce::String::fromUTF8(trk.name.c_str());
            return "Track " + juce::String(targetTrack_ + 1) + " FX: " + name;
        }
        return "Track FX";
    }

    if (const Clip* clip = song_.findClip(patternId_, targetClip_))
    {
        const int trkIdx = findTrackForClip(targetClip_);
        const double spb = static_cast<double>(song_.stepsPerBar());
        const double spBeat = static_cast<double>(song_.stepsPerBeat());
        const int bar = static_cast<int>(std::floor(clip->startStep / spb)) + 1;
        const int beat = static_cast<int>(std::floor(std::fmod(std::max(0.0, clip->startStep), spb) / spBeat)) + 1;
        return "Clip FX: T" + juce::String(trkIdx + 1) + " Bar " + juce::String(bar) + "." + juce::String(beat);
    }
    return "Clip FX (No Clip Selected)";
}

void FxRackPanel::rebuildTargetSelector()
{
    isUpdatingSelectors_ = true;
    targetSelector_.clear(juce::dontSendNotification);
    clipChoices_.clear();

    const auto* pat = song_.findPattern(patternId_);
    if (pat != nullptr)
    {
        if (scope_ == Scope::Track)
        {
            const int n = static_cast<int>(pat->tracks.size());
            if (n > 0)
                targetTrack_ = std::clamp(targetTrack_, 0, n - 1);
            else
                targetTrack_ = -1;

            for (int t = 0; t < n; ++t)
            {
                const auto& trk = pat->tracks[static_cast<std::size_t>(t)];
                juce::String name = trk.name.empty() ? juce::String() : juce::String::fromUTF8(trk.name.c_str());
                if (name.isEmpty())
                {
                    SampleId sid = trk.sampleId;
                    if (!sid.isValid() && !trk.clips.empty())
                        sid = trk.clips.front().sampleId;
                    if (const Sample* s = song_.findSample(sid))
                        name = juce::String::fromUTF8(s->name.c_str());
                }
                juce::String entry = "Track " + juce::String(t + 1);
                if (name.isNotEmpty())
                    entry += " - " + name;
                if (!trk.fxChain.empty())
                    entry += "  [" + juce::String(trk.fxChain.size()) + " FX]";
                targetSelector_.addItem(entry, t + 1);
            }

            if (targetTrack_ >= 0 && targetTrack_ < n)
                targetSelector_.setSelectedId(targetTrack_ + 1, juce::dontSendNotification);
        }
        else if (scope_ == Scope::Clip)
        {
            int selectedItemId = 0;
            int id = 1;
            for (int t = 0; t < static_cast<int>(pat->tracks.size()); ++t)
            {
                const auto& trk = pat->tracks[static_cast<std::size_t>(t)];
                for (const auto& c : trk.clips)
                {
                    const double spb = static_cast<double>(song_.stepsPerBar());
                    const double spBeat = static_cast<double>(song_.stepsPerBeat());
                    const int bar = static_cast<int>(std::floor(c.startStep / spb)) + 1;
                    const int beat = static_cast<int>(std::floor(std::fmod(std::max(0.0, c.startStep), spb) / spBeat)) + 1;
                    juce::String name = trk.name.empty() ? juce::String() : juce::String::fromUTF8(trk.name.c_str());
                    if (name.isEmpty())
                    {
                        if (const Sample* s = song_.findSample(c.sampleId))
                            name = juce::String::fromUTF8(s->name.c_str());
                    }
                    juce::String entry = "T" + juce::String(t + 1) + "  Bar " + juce::String(bar) + "." + juce::String(beat);
                    if (name.isNotEmpty())
                        entry += " (" + name + ")";
                    if (!c.fxChain.empty())
                        entry += "  [" + juce::String(c.fxChain.size()) + " FX]";

                    clipChoices_.push_back(c.id);
                    targetSelector_.addItem(entry, id);
                    if (c.id == targetClip_)
                        selectedItemId = id;
                    ++id;
                }
            }

            if (selectedItemId == 0 && !clipChoices_.empty())
            {
                targetClip_ = clipChoices_.front();
                selectedItemId = 1;
            }
            if (selectedItemId > 0)
                targetSelector_.setSelectedId(selectedItemId, juce::dontSendNotification);
        }
    }

    isUpdatingSelectors_ = false;
}

void FxRackPanel::loadChainFromModel()
{
    if (auto* patMut = song_.findPattern(patternId_))
        engine_.pluginHost().syncLatenciesIntoPattern(*patMut);

    chain_.clear();
    const auto* pat = song_.findPattern(patternId_);
    if (pat != nullptr)
    {
        if (scope_ == Scope::Master)
            chain_ = pat->masterFxChain;
        else if (scope_ == Scope::Track && targetTrack_ >= 0 && targetTrack_ < static_cast<int>(pat->tracks.size()))
            chain_ = pat->tracks[static_cast<std::size_t>(targetTrack_)].fxChain;
        else if (scope_ == Scope::Clip && targetClip_.isValid())
        {
            if (const Clip* c = song_.findClip(patternId_, targetClip_))
                chain_ = c->fxChain;
        }
    }

    pdcToggleButton_.setToggleState(song_.pdcEnabled(), juce::dontSendNotification);
    rebuildRackModules();
    updateNavigationControls();
    repaint();
}

void FxRackPanel::commitChain(std::vector<PluginSlot> newChain)
{
    if (!hasValidTarget())
        return;

    if (onPushUndo)
        onPushUndo();

    chain_ = std::move(newChain);

    if (scope_ == Scope::Master)
        song_.setMasterFxChain(patternId_, chain_);
    else if (scope_ == Scope::Track)
        song_.setTrackFxChain(patternId_, targetTrack_, chain_);
    else if (scope_ == Scope::Clip)
        song_.setClipFxChain(patternId_, targetClip_, chain_);

    if (auto* pat = song_.findPattern(patternId_))
    {
        engine_.pluginHost().syncFromPattern(*pat);
        engine_.pluginHost().syncLatenciesIntoPattern(*pat);
        if (scope_ == Scope::Master)
            chain_ = pat->masterFxChain;
        else if (scope_ == Scope::Track && targetTrack_ >= 0 && targetTrack_ < static_cast<int>(pat->tracks.size()))
            chain_ = pat->tracks[static_cast<std::size_t>(targetTrack_)].fxChain;
        else if (scope_ == Scope::Clip && targetClip_.isValid())
        {
            if (const Clip* c = song_.findClip(patternId_, targetClip_))
                chain_ = c->fxChain;
        }
    }

    rebuildTargetSelector();
    rebuildRackModules();
    if (onChainModified)
        onChainModified();
    repaint();
}

void FxRackPanel::updateNavigationControls()
{
    const bool valid = hasValidTarget();
    jumpButton_.setVisible(scope_ != Scope::Master && valid);
    jumpButton_.setButtonText(scope_ == Scope::Clip ? "Jump to Clip" : "Jump to Track");

    if (scope_ == Scope::Clip && valid)
    {
        const int trk = findTrackForClip(targetClip_);
        switchPartnerButton_.setVisible(trk >= 0);
        switchPartnerButton_.setButtonText("Track " + juce::String(trk + 1) + " FX");
        switchPartnerButton_.setTooltip("Switch to this clip's parent track FX chain");
    }
    else if (scope_ == Scope::Track && targetClip_.isValid() && song_.findClip(patternId_, targetClip_) != nullptr)
    {
        switchPartnerButton_.setVisible(true);
        switchPartnerButton_.setButtonText("Selected Clip FX");
        switchPartnerButton_.setTooltip("Switch to the currently selected clip's FX chain");
    }
    else
    {
        switchPartnerButton_.setVisible(false);
    }
}

void FxRackPanel::refreshAvailablePlugins()
{
    available_ = engine_.pluginHost().availableEffectPlugins();
    applyPluginFilter();

    if (available_.isEmpty())
        statusLabel_.setText("0 plugins cached — click Scan...", juce::dontSendNotification);
    else
        statusLabel_.setText(juce::String(available_.size()) + " VST3/AU plugins", juce::dontSendNotification);
}

void FxRackPanel::applyPluginFilter()
{
    const juce::String query = searchBox_.getText().trim();
    std::vector<juce::PluginDescription> filtered;
    filtered.reserve(static_cast<std::size_t>(available_.size()));

    for (const auto& d : available_)
    {
        if (query.isEmpty() ||
            d.name.containsIgnoreCase(query) ||
            d.manufacturerName.containsIgnoreCase(query) ||
            d.pluginFormatName.containsIgnoreCase(query))
        {
            filtered.push_back(d);
        }
    }

    if (browserList_ != nullptr)
        browserList_->setPlugins(std::move(filtered));
}

void FxRackPanel::toggleScanPlugins()
{
    auto& host = engine_.pluginHost();
    if (host.isScanning())
    {
        stopTimer();
        const int count = host.finishPluginScan();
        scanButton_.setButtonText("Scan...");
        refreshAvailablePlugins();
        statusLabel_.setText("Scan stopped (" + juce::String(count) + " found)", juce::dontSendNotification);
        return;
    }

    scanSkipped_ = 0;
    scanButton_.setButtonText("Stop");
    statusLabel_.setText("Scanning VST3 / AU...", juce::dontSendNotification);
    host.beginDefaultPluginScan();
    startTimer(10);
}

void FxRackPanel::timerCallback()
{
    auto& host = engine_.pluginHost();
    if (!host.isScanning())
    {
        stopTimer();
        return;
    }

    juce::String status;
    int skipped = 0;
    const bool more = host.scanNextPlugin(status, skipped);
    scanSkipped_ = skipped;
    if (status.isNotEmpty())
        statusLabel_.setText(status, juce::dontSendNotification);

    if (!more)
    {
        stopTimer();
        const int count = host.finishPluginScan();
        scanButton_.setButtonText("Scan...");
        refreshAvailablePlugins();
        juce::String summary = juce::String(count) + " plugins found";
        if (scanSkipped_ > 0)
            summary += " (" + juce::String(scanSkipped_) + " skipped)";
        statusLabel_.setText(summary, juce::dontSendNotification);
    }
}

void FxRackPanel::addPluginFromDescription(const juce::PluginDescription& desc)
{
    if (!hasValidTarget())
        return;

    PluginSlot slot;
    slot.name = desc.name.toStdString();
    slot.format = desc.pluginFormatName.toStdString();
    slot.fileOrIdentifier = desc.fileOrIdentifier.toStdString();
    slot.identifierString = desc.createIdentifierString().toStdString();
    slot.bypassed = false;

    auto next = chain_;
    const int newSlotIndex = static_cast<int>(next.size());
    next.push_back(std::move(slot));
    commitChain(std::move(next));

    engine_.pluginHost().openPluginEditor(currentHostTargetKind(), currentHostTargetId(), newSlotIndex);
}

void FxRackPanel::rebuildRackModules()
{
    moduleCards_.clear();
    rackCanvas_->removeAllChildren();

    const juce::Colour accent = currentAccentColour();

    for (int i = 0; i < static_cast<int>(chain_.size()); ++i)
    {
        auto card = std::make_unique<RackModuleCard>(
            i, chain_[static_cast<std::size_t>(i)], accent,
            [this, i]()
            {
                if (i < static_cast<int>(chain_.size()))
                {
                    auto next = chain_;
                    next[static_cast<std::size_t>(i)].bypassed = !next[static_cast<std::size_t>(i)].bypassed;
                    commitChain(std::move(next));
                }
            },
            [this, i]()
            {
                if (!engine_.pluginHost().openPluginEditor(currentHostTargetKind(), currentHostTargetId(), i, true))
                    statusLabel_.setText("Could not open plugin UI", juce::dontSendNotification);
            },
            [this, i]()
            {
                if (i > 0 && i < static_cast<int>(chain_.size()))
                {
                    auto next = chain_;
                    std::swap(next[static_cast<std::size_t>(i)], next[static_cast<std::size_t>(i - 1)]);
                    commitChain(std::move(next));
                }
            },
            [this, i]()
            {
                if (i >= 0 && i + 1 < static_cast<int>(chain_.size()))
                {
                    auto next = chain_;
                    std::swap(next[static_cast<std::size_t>(i)], next[static_cast<std::size_t>(i + 1)]);
                    commitChain(std::move(next));
                }
            },
            [this, i]()
            {
                if (i >= 0 && i < static_cast<int>(chain_.size()))
                {
                    auto next = chain_;
                    next.erase(next.begin() + i);
                    commitChain(std::move(next));
                }
            },
            i > 0, i + 1 < static_cast<int>(chain_.size()));

        rackCanvas_->addAndMakeVisible(card.get());
        moduleCards_.push_back(std::move(card));
    }

    juce::String emptyMsg;
    if (!hasValidTarget())
    {
        emptyMsg = (scope_ == Scope::Clip)
                       ? "No clip selected. Click a clip on the timeline to edit its Clip FX rack."
                       : "No track available. Add a track to edit its Track FX rack.";
    }
    else
    {
        emptyMsg = "Empty " + juce::String(scope_ == Scope::Clip ? "Clip" : (scope_ == Scope::Master ? "Master" : "Track")) +
                   " FX Rack\nDouble-click a plugin in the browser on the left (or click +) to insert it into the rack.";
    }

    rackCanvas_->setMeta(static_cast<int>(moduleCards_.size()), accent, emptyMsg);
    layoutRackModules();

    const bool hasSlots = !chain_.empty();
    const bool allBypassed = hasSlots && std::all_of(chain_.begin(), chain_.end(),
                                                     [](const PluginSlot& s) { return s.bypassed; });
    bypassAllButton_.setEnabled(hasSlots);
    bypassAllButton_.setToggleState(allBypassed, juce::dontSendNotification);

    const int chainLat = activeChainLatencySamples(chain_);
    const int projLat = song_.totalProjectLatencySamples(patternId_);
    const double sr = std::max(8000.0, engine_.sampleRate());
    const double projMs = (static_cast<double>(projLat) * 1000.0) / sr;
    pdcToggleButton_.setButtonText(projLat > 0 ? ("PDC (" + juce::String(projLat) + " smp)") : "PDC");
    pdcToggleButton_.setTooltip("Plugin Delay Compensation: " + juce::String(song_.pdcEnabled() ? "ON" : "OFF") +
                                " | Chain Latency: " + juce::String(chainLat) + " smp" +
                                " | Total Project PDC: " + juce::String(projLat) + " smp (" +
                                juce::String(projMs, 1) + " ms)");
}

void FxRackPanel::layoutRackModules()
{
    const int n = static_cast<int>(moduleCards_.size());
    const int viewW = std::max(100, rackViewport_.getMaximumVisibleWidth());
    const int viewH = std::max(90, rackViewport_.getMaximumVisibleHeight());

    const int contentW = (n == 0)
                             ? viewW
                             : std::max(viewW,
                                        RackCanvas::kMarginX * 2 +
                                            RackCanvas::kInOutW * 2 +
                                            RackCanvas::kArrowW * (n + 1) +
                                            RackCanvas::kCardW * n);
    rackCanvas_->setSize(contentW, viewH);

    const int cardH = std::clamp(viewH - 16, 86, 170);
    const int cardY = std::max(6, (viewH - cardH) / 2);
    int x = RackCanvas::kMarginX + RackCanvas::kInOutW + RackCanvas::kArrowW;
    for (int i = 0; i < n; ++i)
    {
        moduleCards_[static_cast<std::size_t>(i)]->setBounds(x, cardY, RackCanvas::kCardW, cardH);
        x += RackCanvas::kCardW + RackCanvas::kArrowW;
    }
}

void FxRackPanel::paint(juce::Graphics& g)
{
    g.fillAll(Palette::surface);

    // Left target accent strip
    g.setColour(currentAccentColour());
    g.fillRect(0, 0, 5, getHeight());

    // Header separator
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(36, 5.0f, static_cast<float>(getWidth()));

    // Vertical divider between Plugin Browser and Horizontal Rack
    const int divX = browserViewport_.getRight() + 5;
    if (divX > 20)
    {
        g.setColour(Palette::gridLine);
        g.drawVerticalLine(divX, 36.0f, static_cast<float>(getHeight()));
    }

    // Frame around rack area
    const auto rackFrame = rackViewport_.getBounds().expanded(1);
    if (!rackFrame.isEmpty())
    {
        g.setColour(Palette::gridLine);
        g.drawRoundedRectangle(rackFrame.toFloat(), 4.0f, 1.0f);
    }
}

void FxRackPanel::resized()
{
    auto area = getLocalBounds().withTrimmedLeft(5);

    // Top scope & target bar
    auto topBar = area.removeFromTop(36).reduced(10, 6);
    trackScopeBtn_.setBounds(topBar.removeFromLeft(74));
    topBar.removeFromLeft(3);
    clipScopeBtn_.setBounds(topBar.removeFromLeft(68));
    topBar.removeFromLeft(3);
    masterScopeBtn_.setBounds(topBar.removeFromLeft(80));
    topBar.removeFromLeft(10);

    bypassAllButton_.setBounds(topBar.removeFromRight(84));
    topBar.removeFromRight(6);
    pdcToggleButton_.setBounds(topBar.removeFromRight(92));
    topBar.removeFromRight(6);

    if (switchPartnerButton_.isVisible())
    {
        switchPartnerButton_.setBounds(topBar.removeFromRight(112));
        topBar.removeFromRight(6);
    }
    if (jumpButton_.isVisible())
    {
        jumpButton_.setBounds(topBar.removeFromRight(104));
        topBar.removeFromRight(8);
    }

    if (scope_ == Scope::Master)
        masterTargetLabel_.setBounds(topBar);
    else
        targetSelector_.setBounds(topBar.withWidth(std::min(320, topBar.getWidth())));

    // Split body: Left Plugin Browser | Right Horizontal Rack
    auto body = area.reduced(8, 6);
    const int browserW = std::clamp(body.getWidth() * 26 / 100, 210, 290);
    auto leftPane = body.removeFromLeft(browserW);
    body.removeFromLeft(10); // divider gap

    auto leftHeader = leftPane.removeFromTop(22);
    scanButton_.setBounds(leftHeader.removeFromRight(60));
    browserTitleLabel_.setBounds(leftHeader);
    leftPane.removeFromTop(4);

    searchBox_.setBounds(leftPane.removeFromTop(22));
    leftPane.removeFromTop(4);

    statusLabel_.setBounds(leftPane.removeFromBottom(16));
    leftPane.removeFromBottom(2);

    browserViewport_.setBounds(leftPane);
    if (browserList_ != nullptr)
        browserList_->setWidthHint(browserViewport_.getMaximumVisibleWidth());

    // Right pane: Signal-flow FX Rack
    rackViewport_.setBounds(body);
    layoutRackModules();
}

} // namespace trami
