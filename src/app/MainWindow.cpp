#include "app/MainWindow.h"

#include "BinaryData.h"
#include "ui/MainContainer.h"
#include "ui/TramiLookAndFeel.h"

namespace trami
{

MainWindow::MainWindow(const juce::String& name, AudioEngine& engine, Song& song, StretchCache& stretchCache)
    : DocumentWindow(name, Palette::background, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    const auto appIcon = juce::ImageCache::getFromMemory(BinaryData::icon128_png, BinaryData::icon128_pngSize);
    if (appIcon.isValid())
        setIcon(appIcon);
    setContentOwned(new MainContainer(song, engine, stretchCache), true);
    setResizable(true, true);
    setResizeLimits(920, 600, 3840, 2160);
    centreWithSize(1200, 720);
    setVisible(true);
    if (appIcon.isValid())
    {
        if (auto* peer = getPeer())
            peer->setIcon(appIcon);
    }
    getContentComponent()->grabKeyboardFocus();
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

} // namespace trami
