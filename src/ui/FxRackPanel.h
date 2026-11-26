#pragma once

#include "engine/AudioEngine.h"
#include "engine/PluginHost.h"
#include "model/Ids.h"
#include "model/Pattern.h"
#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace trami
{

/// Docked hardware-style FX Rack view supporting Track FX, Single Clip FX, and Master Bus FX.
/// Left pane: searchable VST3/AU plugin browser (double-click or click + to insert).
/// Right pane: horizontal signal-flow rack of modular plugin units (IN -> [FX 1] -> [FX 2] -> OUT).
class FxRackPanel : public juce::Component, private juce::Timer
{
public:
    enum class Scope
    {
        Track = 0,
        Clip = 1,
        Master = 2
    };

    FxRackPanel(Song& song, AudioEngine& engine, PatternId patternId);
    ~FxRackPanel() override;

    void setPattern(PatternId patternId);
    void refreshFromModel();

    void showTrackFx(int trackIndex);
    void showClipFx(ClipId clipId);
    void showMasterFx();

    void notifyTrackSelected(int trackIndex);
    void notifyClipSelected(ClipId clipId, int trackIndex);

    Scope currentScope() const noexcept { return scope_; }
    juce::String currentSummaryText() const;

    std::function<void()> onPushUndo;
    std::function<void()> onChainModified;
    std::function<void(int track)> onJumpToTrack;
    std::function<void(ClipId clipId)> onJumpToClip;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class RackModuleCard;
    class RackCanvas;
    class PluginBrowserList;

    void setScope(Scope newScope);
    void rebuildTargetSelector();
    void loadChainFromModel();
    void commitChain(std::vector<PluginSlot> newChain);
    void refreshAvailablePlugins();
    void applyPluginFilter();
    void toggleScanPlugins();
    void addPluginFromDescription(const juce::PluginDescription& desc);
    void rebuildRackModules();
    void layoutRackModules();
    void updateNavigationControls();
    void timerCallback() override;

    int findTrackForClip(ClipId clipId) const;
    juce::Colour currentAccentColour() const;
    PluginHost::TargetKind currentHostTargetKind() const;
    std::uint32_t currentHostTargetId() const;
    bool hasValidTarget() const;

    Song& song_;
    AudioEngine& engine_;
    PatternId patternId_;

    Scope scope_ = Scope::Track;
    int targetTrack_ = 0;
    ClipId targetClip_{};
    bool isUpdatingSelectors_ = false;

    // Top scope + target bar
    juce::TextButton trackScopeBtn_{"Track FX"};
    juce::TextButton clipScopeBtn_{"Clip FX"};
    juce::TextButton masterScopeBtn_{"Master FX"};
    juce::ComboBox targetSelector_;
    juce::Label masterTargetLabel_;
    juce::TextButton jumpButton_{"Show in Timeline"};
    juce::TextButton switchPartnerButton_;
    juce::TextButton pdcToggleButton_{"PDC"};
    juce::TextButton bypassAllButton_{"Bypass All"};

    // Left pane: Searchable VST3/AU Plugin Browser
    juce::Label browserTitleLabel_;
    juce::TextButton scanButton_{"Scan..."};
    juce::TextEditor searchBox_;
    juce::Array<juce::PluginDescription> available_;
    std::unique_ptr<PluginBrowserList> browserList_;
    juce::Viewport browserViewport_;
    juce::Label statusLabel_;
    int scanSkipped_ = 0;

    // Right pane: Horizontal Signal-Flow FX Rack
    std::vector<PluginSlot> chain_;
    std::unique_ptr<RackCanvas> rackCanvas_;
    juce::Viewport rackViewport_;
    std::vector<std::unique_ptr<RackModuleCard>> moduleCards_;

    // Clip entry list corresponding to targetSelector_ IDs in Clip scope
    std::vector<ClipId> clipChoices_;
};

} // namespace trami
