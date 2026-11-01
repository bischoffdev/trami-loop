#include "app/MainWindow.h"

#include "ui/TramiLookAndFeel.h"

namespace trami
{

MainWindow::MainWindow(const juce::String& name, AudioEngine& engine, Song& song, StretchCache& stretchCache)
    : DocumentWindow(name, Palette::background, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    setContentOwned(new PatternView(song, engine, stretchCache), true);
    setResizable(true, true);
    setResizeLimits(800, 600, 3840, 2160);
    centreWithSize(1200, 720);
    setVisible(true);
    getContentComponent()->grabKeyboardFocus();
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

} // namespace trami
