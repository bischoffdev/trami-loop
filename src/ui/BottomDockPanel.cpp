#include "ui/BottomDockPanel.h"

#include "ui/TramiLookAndFeel.h"

namespace trami
{

class BottomDockPanel::DockTabButton final : public juce::Button
{
public:
    DockTabButton(const juce::String& name, const juce::String& tooltip)
        : juce::Button(name)
    {
        setTooltip(tooltip);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setActive(bool active)
    {
        if (active_ != active)
        {
            active_ = active;
            repaint();
        }
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool /*down*/) override
    {
        const auto b = getLocalBounds().toFloat();
        if (active_)
        {
            g.setColour(Palette::surface);
            g.fillRect(b);
            g.setColour(Palette::accent);
            g.fillRect(b.getX(), b.getY(), b.getWidth(), 2.5f);
        }
        else
        {
            g.setColour(highlighted ? Palette::panelHeader.brighter(0.08f) : Palette::panelBackground);
            g.fillRect(b);
        }

        g.setColour(Palette::gridLine);
        g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));
        if (!active_)
            g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));

        g.setColour(active_ ? Palette::text : (highlighted ? Palette::text.withAlpha(0.85f) : Palette::textDim));
        g.setFont(active_ ? Typography::bold(11.5f) : Typography::regular(11.5f));
        g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred, false);
    }

private:
    bool active_ = false;
};

BottomDockPanel::BottomDockPanel(Song& song, AudioEngine& engine, StretchCache& stretchCache, PatternId patternId,
                                 SampleBrowserPanel::SampleImportedCallback onSampleImported,
                                 TrackPropertiesComponent::PropertiesChangedCallback onPropertiesChanged)
    : song_(song), patternId_(patternId)
{
    explorerTab_ = std::make_unique<DockTabButton>("Explorer", "Browse and audition audio files");
    explorerTab_->onClick = [this]() { setActiveTab(Tab::Explorer); };
    addAndMakeVisible(explorerTab_.get());

    propertiesTab_ = std::make_unique<DockTabButton>("Properties", "Edit selected track properties, stretch markers & metadata");
    propertiesTab_->onClick = [this]() { setActiveTab(Tab::Properties); };
    addAndMakeVisible(propertiesTab_.get());

    mixerTab_ = std::make_unique<DockTabButton>("Mixer", "Side-by-side track channel strips & Master mixer");
    mixerTab_->onClick = [this]() { setActiveTab(Tab::Mixer); };
    addAndMakeVisible(mixerTab_.get());

    fxTab_ = std::make_unique<DockTabButton>("FX", "Track, Single Clip, and Master Bus VST3/AU FX chains");
    fxTab_->onClick = [this]() { setActiveTab(Tab::Fx); };
    addAndMakeVisible(fxTab_.get());

    contextLabel_.setFont(Typography::regular(11.0f));
    contextLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    contextLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(contextLabel_);

    collapseButton_.setTooltip("Collapse Bottom Dock (B)");
    collapseButton_.onClick = [this]()
    {
        if (onCollapseRequested)
            onCollapseRequested();
    };
    addAndMakeVisible(collapseButton_);

    // 1. Explorer
    explorer_ = std::make_unique<SampleBrowserPanel>(song_, engine, stretchCache, std::move(onSampleImported));
    addChildComponent(explorer_.get());

    // 2. Properties
    properties_ = std::make_unique<TrackPropertiesComponent>(song_, stretchCache, patternId_, 0, std::move(onPropertiesChanged));
    addChildComponent(properties_.get());

    // 3. Mixer
    mixer_ = std::make_unique<MixerPanel>(song_, engine, patternId_);
    addChildComponent(mixer_.get());

    // 4. FX Rack
    fxRack_ = std::make_unique<FxRackPanel>(song_, engine, patternId_);
    addChildComponent(fxRack_.get());

    setActiveTab(Tab::Explorer);
}

BottomDockPanel::~BottomDockPanel() = default;

void BottomDockPanel::setActiveTab(Tab tab)
{
    activeTab_ = tab;
    explorer_->setVisible(tab == Tab::Explorer);
    properties_->setVisible(tab == Tab::Properties);
    mixer_->setVisible(tab == Tab::Mixer);
    fxRack_->setVisible(tab == Tab::Fx);

    if (tab == Tab::Properties)
        properties_->refreshFromModel();
    else if (tab == Tab::Mixer)
        mixer_->refreshFromModel();
    else if (tab == Tab::Fx)
        fxRack_->refreshFromModel();

    updateTabStyles();
    updateContextLabel();
    resized();
    repaint();
}

void BottomDockPanel::setPattern(PatternId patternId)
{
    patternId_ = patternId;
    selectedTrack_ = 0;
    properties_->setTrack(patternId_, 0);
    mixer_->setPattern(patternId_);
    fxRack_->setPattern(patternId_);
    updateContextLabel();
}

void BottomDockPanel::refreshFromModel()
{
    properties_->refreshFromModel();
    mixer_->refreshFromModel();
    fxRack_->refreshFromModel();
    updateContextLabel();
}

void BottomDockPanel::selectTrack(int trackIndex)
{
    std::unordered_set<int> sel;
    if (trackIndex >= 0)
        sel.insert(trackIndex);
    selectTracks(sel, trackIndex);
}

void BottomDockPanel::selectTracks(const std::unordered_set<int>& tracks, int primaryTrack)
{
    selectedTrack_ = primaryTrack;
    if (primaryTrack >= 0)
    {
        properties_->setTrack(patternId_, primaryTrack);
        fxRack_->notifyTrackSelected(primaryTrack);
    }
    mixer_->setSelectedTracks(tracks, primaryTrack);
    updateContextLabel();
}

void BottomDockPanel::selectClip(ClipId clipId, int trackIndex)
{
    if (trackIndex >= 0)
    {
        selectedTrack_ = trackIndex;
        properties_->setTrack(patternId_, trackIndex);
        mixer_->setSelectedTrack(trackIndex);
    }
    fxRack_->notifyClipSelected(clipId, trackIndex);
    updateContextLabel();
}

void BottomDockPanel::showTrackProperties(int trackIndex)
{
    selectedTrack_ = trackIndex;
    properties_->setTrack(patternId_, trackIndex);
    mixer_->setSelectedTrack(trackIndex);
    setActiveTab(Tab::Properties);
}

void BottomDockPanel::showTrackFx(int trackIndex)
{
    selectedTrack_ = trackIndex;
    mixer_->setSelectedTrack(trackIndex);
    fxRack_->showTrackFx(trackIndex);
    setActiveTab(Tab::Fx);
}

void BottomDockPanel::showClipFx(ClipId clipId)
{
    fxRack_->showClipFx(clipId);
    setActiveTab(Tab::Fx);
}

void BottomDockPanel::showMasterFx()
{
    fxRack_->showMasterFx();
    setActiveTab(Tab::Fx);
}

void BottomDockPanel::setMasterMeterLevels(float l, float r, bool clipped)
{
    if (mixer_ != nullptr)
        mixer_->setMasterMeterLevels(l, r, clipped);
}

void BottomDockPanel::updateTabStyles()
{
    explorerTab_->setActive(activeTab_ == Tab::Explorer);
    propertiesTab_->setActive(activeTab_ == Tab::Properties);
    mixerTab_->setActive(activeTab_ == Tab::Mixer);
    fxTab_->setActive(activeTab_ == Tab::Fx);
}

void BottomDockPanel::updateContextLabel()
{
    if (activeTab_ == Tab::Explorer)
    {
        contextLabel_.setText("Drag audio files onto tracks", juce::dontSendNotification);
    }
    else if (activeTab_ == Tab::Properties)
    {
        const int trk = properties_->currentTrackIndex();
        if (trk >= 0)
        {
            const std::string name = song_.patternTrackName(patternId_, trk);
            contextLabel_.setText("Track " + juce::String(trk + 1) + (name.empty() ? "" : (" · " + juce::String(name))),
                                  juce::dontSendNotification);
        }
        else
        {
            contextLabel_.setText("No track selected", juce::dontSendNotification);
        }
    }
    else if (activeTab_ == Tab::Mixer)
    {
        const auto* pat = song_.findPattern(patternId_);
        const int n = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;
        const int selCount = static_cast<int>(mixer_->selectedTracks().size());
        const juce::String selPrefix = (selCount > 1)
            ? (juce::String(selCount) + " Tracks Selected · ")
            : "";
        contextLabel_.setText(selPrefix + juce::String(n) + (n == 1 ? " Track + Master" : " Tracks + Master"),
                              juce::dontSendNotification);
    }
    else if (activeTab_ == Tab::Fx)
    {
        contextLabel_.setText(fxRack_->currentSummaryText(), juce::dontSendNotification);
    }
}

void BottomDockPanel::paint(juce::Graphics& g)
{
    g.fillAll(Palette::surface);

    // Tab bar background
    g.setColour(Palette::panelBackground);
    g.fillRect(0, 0, getWidth(), kTabBarHeight);

    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
    g.drawHorizontalLine(kTabBarHeight - 1, 0.0f, static_cast<float>(getWidth()));
}

void BottomDockPanel::resized()
{
    auto topBar = juce::Rectangle<int>(0, 0, getWidth(), kTabBarHeight);

    constexpr int kTabW = 94;
    explorerTab_->setBounds(topBar.removeFromLeft(kTabW));
    propertiesTab_->setBounds(topBar.removeFromLeft(kTabW));
    mixerTab_->setBounds(topBar.removeFromLeft(kTabW));
    fxTab_->setBounds(topBar.removeFromLeft(kTabW));

    collapseButton_.setBounds(topBar.removeFromRight(28).reduced(3, 3));
    topBar.removeFromRight(6);
    contextLabel_.setBounds(topBar.reduced(4, 2));

    auto content = getLocalBounds().withTrimmedTop(kTabBarHeight);
    explorer_->setBounds(content);
    properties_->setBounds(content);
    mixer_->setBounds(content);
    fxRack_->setBounds(content);
}

} // namespace trami
