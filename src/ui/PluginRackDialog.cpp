#include "ui/PluginRackDialog.h"
#include "ui/TramiLookAndFeel.h"

namespace trami
{

namespace
{

class PluginRackContent : public juce::Component, private juce::Timer
{
public:
    PluginRackContent(PluginHost& host, PluginHost::TargetKind targetKind, std::uint32_t targetId,
                      std::vector<PluginSlot> chain, std::function<void(std::vector<PluginSlot>)> onChanged,
                      PluginRackDialog::NavigationActions navActions)
        : host_(host)
        , targetKind_(targetKind)
        , targetId_(targetId)
        , chain_(std::move(chain))
        , onChanged_(std::move(onChanged))
        , navActions_(std::move(navActions))
    {
        if (navActions_.onPrimary)
        {
            addAndMakeVisible(navPrimaryButton_);
            navPrimaryButton_.setButtonText(navActions_.primaryLabel.isNotEmpty() ? navActions_.primaryLabel : "Show Clip");
            navPrimaryButton_.setTooltip("Jump to and highlight this target in the song timeline");
            navPrimaryButton_.onClick = [this] { if (navActions_.onPrimary) navActions_.onPrimary(); };
        }
        if (navActions_.onSecondary)
        {
            addAndMakeVisible(navSecondaryButton_);
            navSecondaryButton_.setButtonText(navActions_.secondaryLabel.isNotEmpty() ? navActions_.secondaryLabel : "Track FX");
            navSecondaryButton_.setTooltip("Jump to the parent track's FX rack");
            navSecondaryButton_.onClick = [this] { if (navActions_.onSecondary) navActions_.onSecondary(); };
        }

        addAndMakeVisible(pluginCombo_);
        pluginCombo_.setTextWhenNothingSelected("Select VST3 / AU Plugin...");
        pluginCombo_.setTextWhenNoChoicesAvailable("No VST3/AU plugins scanned yet");

        addAndMakeVisible(addButton_);
        addButton_.setButtonText("+ Add");
        addButton_.setTooltip("Add selected VST3/AU effect plugin to this FX chain");
        addButton_.onClick = [this] { addSelectedPlugin(); };

        addAndMakeVisible(scanButton_);
        scanButton_.setButtonText("Scan VST3/AU...");
        scanButton_.setTooltip("Scan system VST3 and AudioUnit folders for external effect plugins");
        scanButton_.onClick = [this] { toggleScanPlugins(); };

        addAndMakeVisible(bypassAllButton_);
        bypassAllButton_.setButtonText("Bypass All");
        bypassAllButton_.setClickingTogglesState(true);
        bypassAllButton_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffb45309));
        bypassAllButton_.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        bypassAllButton_.setTooltip("Bypass or enable all plugins in this FX chain");
        bypassAllButton_.onClick = [this]
        {
            if (chain_.empty())
                return;
            const bool bypass = bypassAllButton_.getToggleState();
            for (auto& s : chain_)
                s.bypassed = bypass;
            commit();
        };

        addAndMakeVisible(statusLabel_);
        statusLabel_.setFont(juce::Font(juce::FontOptions(11.5f)));
        statusLabel_.setColour(juce::Label::textColourId, Palette::textDim);
        statusLabel_.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(slotViewport_);
        slotViewport_.setViewedComponent(&slotContainer_, false);
        slotViewport_.setScrollBarsShown(true, false);

        refreshAvailablePlugins();
        rebuildSlotRows();
        setSize(520, 360);
    }

    ~PluginRackContent() override
    {
        stopTimer();
        if (host_.isScanning())
            host_.finishPluginScan();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::surface);

        // Left color accent strip matching the Clip / Track / Master color
        g.setColour(navActions_.accentColour);
        g.fillRect(0, 0, 6, getHeight());

        auto area = getLocalBounds().withTrimmedLeft(6).reduced(12);
        const bool hasHeader = (navActions_.onPrimary || navActions_.onSecondary || navActions_.targetBanner.isNotEmpty());
        const int topBarH = hasHeader ? 30 : 0;

        if (hasHeader && navActions_.targetBanner.isNotEmpty())
        {
            const int buttonsW = (navActions_.onPrimary ? 122 : 0) + (navActions_.onSecondary ? 122 : 0);
            auto bannerArea = area.withHeight(24).withTrimmedLeft(buttonsW);
            if (bannerArea.getWidth() > 24)
            {
                g.setColour(navActions_.accentColour);
                g.fillEllipse(static_cast<float>(bannerArea.getX() + 4),
                              static_cast<float>(bannerArea.getCentreY() - 4), 8.0f, 8.0f);
                g.setColour(Palette::text);
                g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
                g.drawText(navActions_.targetBanner, bannerArea.withTrimmedLeft(18),
                           juce::Justification::centredLeft, true);
            }
        }

        auto listArea = area.withTrimmedTop(topBarH + 4).withTrimmedBottom(76);
        g.setColour(Palette::background);
        g.fillRoundedRectangle(listArea.toFloat(), 6.0f);
        g.setColour(Palette::gridLine);
        g.drawRoundedRectangle(listArea.toFloat(), 6.0f, 1.0f);

        if (chain_.empty())
        {
            g.setColour(Palette::textDim);
            g.setFont(juce::Font(juce::FontOptions(13.0f)));
            g.drawText("No external VST3/AU plugins loaded.\nSelect a plugin below or click \"Scan VST3/AU...\".",
                       listArea.reduced(16), juce::Justification::centred, true);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedLeft(6).reduced(12);
        const bool hasHeader = (navActions_.onPrimary || navActions_.onSecondary || navActions_.targetBanner.isNotEmpty());
        if (hasHeader)
        {
            auto navRow = area.removeFromTop(24);
            if (navActions_.onPrimary)
            {
                navPrimaryButton_.setBounds(navRow.removeFromLeft(116));
                navRow.removeFromLeft(6);
            }
            if (navActions_.onSecondary)
            {
                navSecondaryButton_.setBounds(navRow.removeFromLeft(116));
            }
            area.removeFromTop(6);
        }
        auto footer = area.removeFromBottom(68);

        slotViewport_.setBounds(area.withTrimmedTop(4).withTrimmedBottom(8).reduced(4));
        layoutSlotRows();

        auto topRow = footer.removeFromTop(28);
        scanButton_.setBounds(topRow.removeFromRight(120));
        topRow.removeFromRight(6);
        addButton_.setBounds(topRow.removeFromRight(68));
        topRow.removeFromRight(6);
        pluginCombo_.setBounds(topRow);

        footer.removeFromTop(6);
        bypassAllButton_.setBounds(footer.removeFromRight(92));
        footer.removeFromRight(8);
        statusLabel_.setBounds(footer);
    }

private:
    struct SlotRow : public juce::Component
    {
        juce::Label indexLabel;
        juce::Label nameLabel;
        juce::TextButton bypassBtn{"Byp"};
        juce::TextButton editBtn{"Edit UI"};
        juce::TextButton upBtn{"^"};
        juce::TextButton downBtn{"v"};
        juce::TextButton removeBtn{"x"};

        SlotRow(int idx, const PluginSlot& slot, std::function<void()> onBypass, std::function<void()> onEdit,
                std::function<void()> onUp, std::function<void()> onDown, std::function<void()> onRemove, bool canUp,
                bool canDown)
        {
            addAndMakeVisible(indexLabel);
            indexLabel.setText(juce::String(idx + 1) + ".", juce::dontSendNotification);
            indexLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            indexLabel.setColour(juce::Label::textColourId, Palette::textDim);

            addAndMakeVisible(nameLabel);
            juce::String label = juce::String(slot.name);
            if (!slot.format.empty())
                label += "  [" + juce::String(slot.format) + "]";
            nameLabel.setText(label, juce::dontSendNotification);
            nameLabel.setFont(juce::Font(juce::FontOptions(12.5f)));
            nameLabel.setColour(juce::Label::textColourId,
                                slot.bypassed ? Palette::textDim : Palette::text);

            addAndMakeVisible(bypassBtn);
            bypassBtn.setClickingTogglesState(true);
            bypassBtn.setToggleState(slot.bypassed, juce::dontSendNotification);
            bypassBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffb45309));
            bypassBtn.setTooltip("Bypass this plugin");
            bypassBtn.onClick = std::move(onBypass);

            addAndMakeVisible(editBtn);
            editBtn.setTooltip("Open native VST3/AU plugin editor window");
            editBtn.onClick = std::move(onEdit);

            addAndMakeVisible(upBtn);
            upBtn.setEnabled(canUp);
            upBtn.setTooltip("Move plugin earlier in chain");
            upBtn.onClick = std::move(onUp);

            addAndMakeVisible(downBtn);
            downBtn.setEnabled(canDown);
            downBtn.setTooltip("Move plugin later in chain");
            downBtn.onClick = std::move(onDown);

            addAndMakeVisible(removeBtn);
            removeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff87171));
            removeBtn.setTooltip("Remove plugin from chain");
            removeBtn.onClick = std::move(onRemove);
        }

        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().reduced(2, 2).toFloat();
            g.setColour(Palette::panelHeader);
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(Palette::gridLine);
            g.drawRoundedRectangle(r, 4.0f, 1.0f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced(6, 5);
            indexLabel.setBounds(r.removeFromLeft(22));
            removeBtn.setBounds(r.removeFromRight(26));
            r.removeFromRight(4);
            downBtn.setBounds(r.removeFromRight(24));
            r.removeFromRight(2);
            upBtn.setBounds(r.removeFromRight(24));
            r.removeFromRight(6);
            editBtn.setBounds(r.removeFromRight(62));
            r.removeFromRight(4);
            bypassBtn.setBounds(r.removeFromRight(40));
            r.removeFromRight(8);
            nameLabel.setBounds(r);
        }
    };

    void refreshAvailablePlugins()
    {
        available_ = host_.availableEffectPlugins();
        pluginCombo_.clear(juce::dontSendNotification);
        for (int i = 0; i < available_.size(); ++i)
        {
            const auto& d = available_.getReference(i);
            juce::String entry = d.name;
            if (d.manufacturerName.isNotEmpty())
                entry += " - " + d.manufacturerName;
            entry += " (" + d.pluginFormatName + ")";
            pluginCombo_.addItem(entry, i + 1);
        }
        if (!available_.isEmpty())
            pluginCombo_.setSelectedId(1, juce::dontSendNotification);
        addButton_.setEnabled(!available_.isEmpty());

        if (available_.isEmpty())
            statusLabel_.setText("0 VST3/AU effect plugins cached. Click \"Scan VST3/AU...\" to scan.",
                                 juce::dontSendNotification);
        else
            statusLabel_.setText(juce::String(available_.size()) + " external VST3/AU effect plugin(s) available.",
                                 juce::dontSendNotification);
    }

    void toggleScanPlugins()
    {
        if (host_.isScanning())
        {
            stopTimer();
            const int count = host_.finishPluginScan();
            scanButton_.setButtonText("Scan VST3/AU...");
            refreshAvailablePlugins();
            statusLabel_.setText("Scan stopped: " + juce::String(count) + " effect plugin(s) available.",
                                 juce::dontSendNotification);
            return;
        }

        scanSkipped_ = 0;
        scanButton_.setButtonText("Stop Scan");
        statusLabel_.setText("Scanning system VST3 / AU plugin directories...", juce::dontSendNotification);
        host_.beginDefaultPluginScan();
        startTimer(10);
    }

    void timerCallback() override
    {
        if (!host_.isScanning())
        {
            stopTimer();
            return;
        }

        juce::String status;
        int skipped = 0;
        const bool more = host_.scanNextPlugin(status, skipped);
        scanSkipped_ = skipped;
        if (status.isNotEmpty())
            statusLabel_.setText(status, juce::dontSendNotification);

        if (!more)
        {
            stopTimer();
            const int count = host_.finishPluginScan();
            scanButton_.setButtonText("Scan VST3/AU...");
            refreshAvailablePlugins();
            juce::String summary = "Scan complete: " + juce::String(count) + " effect plugin(s) available.";
            if (scanSkipped_ > 0)
                summary += " (" + juce::String(scanSkipped_) + " incompatible/crashing skipped)";
            statusLabel_.setText(summary, juce::dontSendNotification);
        }
    }

    void addSelectedPlugin()
    {
        int idx = pluginCombo_.getSelectedId() - 1;
        if (idx < 0 || idx >= available_.size())
            return;
        const auto& desc = available_.getReference(idx);
        PluginSlot slot;
        slot.name = desc.name.toStdString();
        slot.format = desc.pluginFormatName.toStdString();
        slot.fileOrIdentifier = desc.fileOrIdentifier.toStdString();
        slot.identifierString = desc.createIdentifierString().toStdString();
        slot.bypassed = false;
        const int newSlotIndex = static_cast<int>(chain_.size());
        chain_.push_back(std::move(slot));
        commit();
        // Automatically open the newly added plugin's native UI so the user can tweak it immediately
        host_.openPluginEditor(targetKind_, targetId_, newSlotIndex);
    }

    void commit()
    {
        if (onChanged_)
            onChanged_(chain_);
        rebuildSlotRows();
        repaint();
    }

    void rebuildSlotRows()
    {
        slotRows_.clear();
        slotContainer_.removeAllChildren();
        for (int i = 0; i < static_cast<int>(chain_.size()); ++i)
        {
            auto row = std::make_unique<SlotRow>(
                i, chain_[static_cast<std::size_t>(i)],
                [this, i]
                {
                    if (i < static_cast<int>(chain_.size()))
                    {
                        chain_[static_cast<std::size_t>(i)].bypassed = !chain_[static_cast<std::size_t>(i)].bypassed;
                        commit();
                    }
                },
                [this, i]
                {
                    if (!host_.openPluginEditor(targetKind_, targetId_, i))
                        statusLabel_.setText("Could not open plugin editor (plugin may not be installed or has no UI).",
                                             juce::dontSendNotification);
                },
                [this, i]
                {
                    if (i > 0 && i < static_cast<int>(chain_.size()))
                    {
                        std::swap(chain_[static_cast<std::size_t>(i)], chain_[static_cast<std::size_t>(i - 1)]);
                        commit();
                    }
                },
                [this, i]
                {
                    if (i >= 0 && i + 1 < static_cast<int>(chain_.size()))
                    {
                        std::swap(chain_[static_cast<std::size_t>(i)], chain_[static_cast<std::size_t>(i + 1)]);
                        commit();
                    }
                },
                [this, i]
                {
                    if (i >= 0 && i < static_cast<int>(chain_.size()))
                    {
                        chain_.erase(chain_.begin() + i);
                        commit();
                    }
                },
                i > 0, i + 1 < static_cast<int>(chain_.size()));
            slotContainer_.addAndMakeVisible(row.get());
            slotRows_.push_back(std::move(row));
        }
        layoutSlotRows();

        const bool hasSlots = !chain_.empty();
        const bool allBypassed = hasSlots && std::all_of(chain_.begin(), chain_.end(),
                                                         [](const PluginSlot& s) { return s.bypassed; });
        bypassAllButton_.setEnabled(hasSlots);
        bypassAllButton_.setToggleState(allBypassed, juce::dontSendNotification);
    }

    void layoutSlotRows()
    {
        constexpr int kRowH = 38;
        int w = std::max(100, slotViewport_.getMaximumVisibleWidth());
        int totalH = static_cast<int>(slotRows_.size()) * kRowH;
        slotContainer_.setSize(w, std::max(totalH, slotViewport_.getHeight()));
        for (int i = 0; i < static_cast<int>(slotRows_.size()); ++i)
            slotRows_[static_cast<std::size_t>(i)]->setBounds(0, i * kRowH, w, kRowH);
    }

    PluginHost& host_;
    PluginHost::TargetKind targetKind_;
    std::uint32_t targetId_;
    std::vector<PluginSlot> chain_;
    std::function<void(std::vector<PluginSlot>)> onChanged_;
    PluginRackDialog::NavigationActions navActions_;

    juce::TextButton navPrimaryButton_;
    juce::TextButton navSecondaryButton_;
    juce::Array<juce::PluginDescription> available_;
    juce::ComboBox pluginCombo_;
    juce::TextButton addButton_;
    juce::TextButton scanButton_;
    juce::TextButton bypassAllButton_;
    juce::Label statusLabel_;
    juce::Viewport slotViewport_;
    juce::Component slotContainer_;
    std::vector<std::unique_ptr<SlotRow>> slotRows_;
    int scanSkipped_ = 0;
};

class ModelessPluginRackWindow final : public juce::DocumentWindow
{
public:
    ModelessPluginRackWindow(const juce::String& title, PluginRackContent* content,
                             std::function<void(ModelessPluginRackWindow*)> onClose)
        : juce::DocumentWindow(title, Palette::surface, juce::DocumentWindow::closeButton)
        , onClose_(std::move(onClose))
    {
        setUsingNativeTitleBar(true);
        setContentOwned(content, true);
        setResizable(true, true);
        setResizeLimits(420, 260, 1200, 900);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
        toFront(true);
    }

    void closeButtonPressed() override
    {
        if (onClose_)
            onClose_(this);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            closeButtonPressed();
            return true;
        }
        return juce::DocumentWindow::keyPressed(key);
    }

private:
    std::function<void(ModelessPluginRackWindow*)> onClose_;
};

class PluginRackWindowManager final : private juce::DeletedAtShutdown
{
public:
    static PluginRackWindowManager& instance()
    {
        if (instance_ == nullptr)
            instance_ = new PluginRackWindowManager();
        return *instance_;
    }

    ~PluginRackWindowManager() override
    {
        window_.reset();
        if (instance_ == this)
            instance_ = nullptr;
    }

    void open(const juce::String& title, PluginRackContent* content)
    {
        // Always maintain a single FX Rack window: reuse existing window in place if already open
        if (window_ != nullptr)
        {
            const auto prevBounds = window_->getBounds();
            window_->setName(title);
            window_->setContentOwned(content, true);
            window_->setBounds(prevBounds);
            window_->setVisible(true);
            window_->toFront(true);
            return;
        }

        window_ = std::make_unique<ModelessPluginRackWindow>(
            title, content,
            [this](ModelessPluginRackWindow*)
            {
                window_.reset();
            });
    }

private:
    static PluginRackWindowManager* instance_;
    std::unique_ptr<ModelessPluginRackWindow> window_;
};

PluginRackWindowManager* PluginRackWindowManager::instance_ = nullptr;

} // namespace

void PluginRackDialog::showAsync(const juce::String& title, PluginHost& pluginHost, PluginHost::TargetKind targetKind,
                                 std::uint32_t targetId, std::vector<PluginSlot> initialChain,
                                 std::function<void(std::vector<PluginSlot>)> onChainChanged,
                                 NavigationActions navActions)
{
    auto* content = new PluginRackContent(pluginHost, targetKind, targetId, std::move(initialChain),
                                          std::move(onChainChanged), std::move(navActions));
    PluginRackWindowManager::instance().open(title, content);
}

} // namespace trami
