#pragma once

#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "model/Ids.h"
#include "model/Song.h"
#include "ui/FxRackPanel.h"
#include "ui/MixerPanel.h"
#include "ui/SampleBrowserPanel.h"
#include "ui/TrackPropertiesDialog.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace trami
{

/// Multi-purpose bottom dock hosting Explorer, Properties, Mixer, and FX tabs
/// so all tools stay cleanly docked inside the main window instead of detached popups.
class BottomDockPanel : public juce::Component
{
public:
    enum class Tab
    {
        Explorer = 0,
        Properties = 1,
        Mixer = 2,
        Fx = 3
    };

    BottomDockPanel(Song& song, AudioEngine& engine, StretchCache& stretchCache, PatternId patternId,
                    SampleBrowserPanel::SampleImportedCallback onSampleImported,
                    TrackPropertiesComponent::PropertiesChangedCallback onPropertiesChanged);
    ~BottomDockPanel() override;

    void setActiveTab(Tab tab);
    Tab activeTab() const noexcept { return activeTab_; }

    void setPattern(PatternId patternId);
    void refreshFromModel();

    void selectTrack(int trackIndex);
    void selectTracks(const std::unordered_set<int>& tracks, int primaryTrack);
    void selectClip(ClipId clipId, int trackIndex);

    void showTrackProperties(int trackIndex);
    void showTrackFx(int trackIndex);
    void showClipFx(ClipId clipId);
    void showMasterFx();

    void setMasterMeterLevels(float l, float r, bool clipped);

    SampleBrowserPanel& explorer() noexcept { return *explorer_; }
    TrackPropertiesComponent& properties() noexcept { return *properties_; }
    MixerPanel& mixer() noexcept { return *mixer_; }
    FxRackPanel& fxRack() noexcept { return *fxRack_; }

    std::function<void()> onCollapseRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class DockTabButton;

    void updateTabStyles();
    void updateContextLabel();

    static constexpr int kTabBarHeight = 28;

    Song& song_;
    PatternId patternId_;
    Tab activeTab_ = Tab::Explorer;
    int selectedTrack_ = 0;

    std::unique_ptr<DockTabButton> explorerTab_;
    std::unique_ptr<DockTabButton> propertiesTab_;
    std::unique_ptr<DockTabButton> mixerTab_;
    std::unique_ptr<DockTabButton> fxTab_;
    juce::Label contextLabel_;
    juce::TextButton collapseButton_{"v"};

    std::unique_ptr<SampleBrowserPanel> explorer_;
    std::unique_ptr<TrackPropertiesComponent> properties_;
    std::unique_ptr<MixerPanel> mixer_;
    std::unique_ptr<FxRackPanel> fxRack_;
};

} // namespace trami
