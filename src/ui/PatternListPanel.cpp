#include "ui/PatternListPanel.h"

#include <algorithm>

namespace trami
{

class PatternContentComp : public juce::Component
{
public:
    explicit PatternContentComp(PatternListPanel& owner) : owner_(owner) {}
    void paint(juce::Graphics& g) override { owner_.paintContent(g); }
    void mouseDown(const juce::MouseEvent& e) override { owner_.contentMouseDown(e); }
    void mouseDrag(const juce::MouseEvent& e) override { owner_.contentMouseDrag(e); }
private:
    PatternListPanel& owner_;
};

PatternListPanel::PatternListPanel(Song& song, PatternId activePattern,
                                   PatternSelectedCallback onSelected,
                                   PatternModifiedCallback onModified)
    : song_(song), activePattern_(activePattern),
      onSelected_(std::move(onSelected)), onModified_(std::move(onModified))
{
    titleLabel_.setFont(Typography::bold(13.0f));
    titleLabel_.setColour(juce::Label::textColourId, Palette::text);
    titleLabel_.setText("PATTERNS", juce::dontSendNotification);
    addAndMakeVisible(titleLabel_);

    newPatternButton_.setTooltip("Create a new empty pattern");
    newPatternButton_.onClick = [this]() { addNewPattern(); };
    addAndMakeVisible(newPatternButton_);

    contentComp_ = std::make_unique<PatternContentComp>(*this);
    viewport_.setViewedComponent(contentComp_.get(), false);
    viewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport_);

    updateFromModel();
}

void PatternListPanel::setActivePattern(PatternId id)
{
    if (activePattern_ != id)
    {
        activePattern_ = id;
        repaint();
        if (contentComp_ != nullptr)
            contentComp_->repaint();
    }
}

void PatternListPanel::updateFromModel()
{
    rows_.clear();
    if (contentComp_ == nullptr)
        return;

    contentComp_->deleteAllChildren();

    const auto& patterns = song_.patterns();
    constexpr int rowHeight = 44;
    const int totalHeight = static_cast<int>(patterns.size()) * rowHeight;
    contentComp_->setSize(std::max(160, getWidth()), totalHeight);

    for (std::size_t i = 0; i < patterns.size(); ++i)
    {
        const auto& p = patterns[i];
        PatternRow row;
        row.id = p.id;

        const int y = static_cast<int>(i) * rowHeight;
        row.bounds = juce::Rectangle<int>(0, y, contentComp_->getWidth(), rowHeight);

        // Name label (editable on click or double-click)
        row.nameLabel = std::make_unique<juce::Label>("", p.name);
        row.nameLabel->setFont(Typography::bold(12.5f));
        row.nameLabel->setColour(juce::Label::textColourId, Palette::text);
        row.nameLabel->setEditable(false, true, false);
        row.nameLabel->setTooltip("Double-click to rename pattern");
        row.nameLabel->onTextChange = [this, id = p.id, lbl = row.nameLabel.get()]() {
            if (lbl != nullptr)
            {
                const juce::String newName = lbl->getText().trim();
                if (newName.isNotEmpty())
                {
                    song_.renamePattern(id, newName.toStdString());
                    if (onModified_)
                        onModified_();
                }
            }
        };
        row.nameLabel->addMouseListener(contentComp_.get(), false);
        contentComp_->addAndMakeVisible(row.nameLabel.get());

        // Info label: bars and track count
        const juce::String infoText = juce::String(p.lengthBars) + (p.lengthBars == 1 ? " Bar" : " Bars") +
                                      " \xe2\x80\xa2 " + juce::String(p.tracks.size()) +
                                      (p.tracks.size() == 1 ? " Track" : " Tracks");
        row.infoLabel = std::make_unique<juce::Label>("", infoText);
        row.infoLabel->setFont(Typography::regular(10.5f));
        row.infoLabel->setColour(juce::Label::textColourId, Palette::textDim);
        row.infoLabel->setInterceptsMouseClicks(false, false);
        contentComp_->addAndMakeVisible(row.infoLabel.get());

        // Rename / edit button
        row.editButton = std::make_unique<IconButton>("Ren", IconButton::Icon::Edit, "");
        row.editButton->setTooltip("Rename this pattern");
        row.editButton->onClick = [lbl = row.nameLabel.get()]() {
            if (lbl != nullptr)
                lbl->showEditor();
        };
        contentComp_->addAndMakeVisible(row.editButton.get());

        // Duplicate button
        row.duplicateButton = std::make_unique<IconButton>("Dup", IconButton::Icon::Duplicate, "");
        row.duplicateButton->setTooltip("Duplicate this pattern");
        row.duplicateButton->onClick = [this, id = p.id]() { duplicatePattern(id); };
        contentComp_->addAndMakeVisible(row.duplicateButton.get());

        // Delete button
        row.deleteButton = std::make_unique<IconButton>("Del", IconButton::Icon::Delete, "");
        row.deleteButton->setTooltip("Delete this pattern");
        row.deleteButton->setEnabled(patterns.size() > 1);
        row.deleteButton->onClick = [this, id = p.id]() { deletePattern(id); };
        contentComp_->addAndMakeVisible(row.deleteButton.get());

        rows_.push_back(std::move(row));
    }

    resized();
    repaint();
}

void PatternListPanel::paint(juce::Graphics& g)
{
    g.fillAll(Palette::panelBackground);

    constexpr int kHeaderHeight = 32;
    g.setColour(Palette::panelHeader);
    g.fillRect(0, 0, getWidth(), kHeaderHeight);

    // Right border dividing from Track headers / Arranger
    g.setColour(Palette::gridLine);
    g.drawVerticalLine(getWidth() - 2, 0.0f, static_cast<float>(getHeight()));
    g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));

    // Header bottom line
    g.drawHorizontalLine(kHeaderHeight, 0.0f, static_cast<float>(getWidth()));
}

void PatternListPanel::resized()
{
    constexpr int kHeaderHeight = 32;
    titleLabel_.setBounds(10, 4, 100, 24);
    newPatternButton_.setBounds(getWidth() - 66, 4, 60, 24);

    viewport_.setBounds(0, kHeaderHeight, getWidth() - 2, getHeight() - kHeaderHeight);

    if (contentComp_ != nullptr)
    {
        constexpr int rowHeight = 44;
        const int w = viewport_.getWidth();
        contentComp_->setSize(w, static_cast<int>(rows_.size()) * rowHeight);

        for (std::size_t i = 0; i < rows_.size(); ++i)
        {
            auto& row = rows_[i];
            const int y = static_cast<int>(i) * rowHeight;
            row.bounds = juce::Rectangle<int>(0, y, w, rowHeight);

            // Left name & info
            row.nameLabel->setBounds(8, y + 2, w - 82, 19);
            row.infoLabel->setBounds(8, y + 21, w - 82, 16);

            // Right action buttons: Edit, Duplicate, Delete
            row.editButton->setBounds(w - 74, y + 10, 22, 22);
            row.duplicateButton->setBounds(w - 50, y + 10, 22, 22);
            row.deleteButton->setBounds(w - 26, y + 10, 22, 22);
        }
    }
}

void PatternListPanel::paintContent(juce::Graphics& g)
{
    g.fillAll(Palette::panelBackground);

    for (const auto& row : rows_)
    {
        const bool isSelected = (row.id == activePattern_);
        if (isSelected)
        {
            g.setColour(Palette::accent.withAlpha(0.18f));
            g.fillRect(row.bounds);
            g.setColour(Palette::accent);
            g.fillRect(row.bounds.getX(), row.bounds.getY(), 3, row.bounds.getHeight());
        }

        // Row bottom separator
        g.setColour(Palette::gridLine.withAlpha(0.45f));
        g.drawHorizontalLine(row.bounds.getBottom() - 1, 0.0f, static_cast<float>(row.bounds.getWidth()));
    }
}

void PatternListPanel::contentMouseDown(const juce::MouseEvent& e)
{
    const auto pt = e.getEventRelativeTo(contentComp_.get()).getPosition();
    for (auto& row : rows_)
    {
        if (row.bounds.contains(pt))
        {
            setActivePattern(row.id);
            if (onSelected_)
                onSelected_(row.id);

            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m;
                m.addItem(1, "Rename Pattern");
                m.addItem(2, "Duplicate Pattern");
                m.addItem(3, "Delete Pattern", song_.patterns().size() > 1);
                m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(contentComp_.get()).withTargetScreenArea(
                                    juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                                [this, id = row.id, lbl = row.nameLabel.get()](int res) {
                                    if (res == 1 && lbl != nullptr)
                                        lbl->showEditor();
                                    else if (res == 2)
                                        duplicatePattern(id);
                                    else if (res == 3)
                                        deletePattern(id);
                                });
                return;
            }

            if (e.getNumberOfClicks() >= 2 && row.nameLabel != nullptr)
            {
                row.nameLabel->showEditor();
            }
            break;
        }
    }
}

void PatternListPanel::contentMouseDrag(const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 6)
    {
        const auto pt = e.getEventRelativeTo(contentComp_.get()).getPosition();
        for (const auto& row : rows_)
        {
            if (row.bounds.contains(pt))
            {
                if (const auto* p = song_.findPattern(row.id))
                {
                    startPatternDrag(row.id, p->name);
                }
                break;
            }
        }
    }
}

void PatternListPanel::mouseDown(const juce::MouseEvent& e)
{
    contentMouseDown(e.getEventRelativeTo(contentComp_.get()));
}

void PatternListPanel::mouseDrag(const juce::MouseEvent& e)
{
    contentMouseDrag(e.getEventRelativeTo(contentComp_.get()));
}

void PatternListPanel::startPatternDrag(PatternId id, const juce::String& name)
{
    auto* ddc = juce::DragAndDropContainer::findParentDragContainerFor(this);
    if (ddc == nullptr || ddc->isDragAndDropActive())
        return;

    const auto* p = song_.findPattern(id);
    const int bars = p != nullptr ? p->lengthBars : 1;
    const int tracks = p != nullptr ? static_cast<int>(p->tracks.size()) : 1;

    constexpr int imgW = 160;
    constexpr int imgH = 30;
    juce::Image dragImg(juce::Image::ARGB, imgW, imgH, true);
    {
        juce::Graphics g(dragImg);
        g.setColour(juce::Colour(0xee1e2633));
        g.fillRoundedRectangle(0.5f, 0.5f, imgW - 1.0f, imgH - 1.0f, 5.0f);
        g.setColour(Palette::accent);
        g.drawRoundedRectangle(0.5f, 0.5f, imgW - 1.0f, imgH - 1.0f, 5.0f, 1.5f);

        // Pattern icon
        g.fillRoundedRectangle(6.0f, 6.0f, 7.0f, 7.0f, 1.5f);
        g.fillRoundedRectangle(15.0f, 6.0f, 7.0f, 7.0f, 1.5f);
        g.fillRoundedRectangle(6.0f, 15.0f, 7.0f, 7.0f, 1.5f);
        g.fillRoundedRectangle(15.0f, 15.0f, 7.0f, 7.0f, 1.5f);

        g.setColour(Palette::text);
        g.setFont(Typography::bold(11.5f));
        g.drawText(name, 26, 2, imgW - 30, 14, juce::Justification::centredLeft, true);

        g.setColour(Palette::textDim);
        g.setFont(Typography::regular(10.0f));
        g.drawText(juce::String(bars) + (bars == 1 ? " Bar" : " Bars") + " \xe2\x80\xa2 " + juce::String(tracks) + " Trk",
                   26, 16, imgW - 30, 12, juce::Justification::centredLeft, true);
    }

    const juce::Point<int> offset(-20, -15);
    ddc->startDragging("pattern:" + juce::String(id.value), this, juce::ScaledImage(dragImg, 1.0), true, &offset);
}

void PatternListPanel::addNewPattern()
{
    const int nextNum = static_cast<int>(song_.patterns().size()) + 1;
    const auto newId = song_.addPattern("Pattern " + std::to_string(nextNum), 1);
    setActivePattern(newId);
    updateFromModel();

    if (onModified_)
        onModified_();
    if (onSelected_)
        onSelected_(newId);
}

void PatternListPanel::duplicatePattern(PatternId id)
{
    if (auto newId = song_.duplicatePattern(id))
    {
        setActivePattern(*newId);
        updateFromModel();
        if (onModified_)
            onModified_();
        if (onSelected_)
            onSelected_(*newId);
    }
}

void PatternListPanel::deletePattern(PatternId id)
{
    if (song_.patterns().size() <= 1)
        return;

    song_.removePattern(id);
    if (!song_.patterns().empty())
        setActivePattern(song_.patterns().front().id);

    updateFromModel();
    if (onModified_)
        onModified_();
    if (onSelected_)
        onSelected_(activePattern_);
}

} // namespace trami
