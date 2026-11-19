#include "app/MainWindow.h"
#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "model/Song.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class TramiApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

        engine_ = std::make_unique<AudioEngine>();
        if (const auto error = engine_->start(); error.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Audio device warning",
                                                   error + "\nYou can configure audio devices in the Audio settings.");
        }

        // Start with an empty project: 1 default pattern and 1 dedicated arranger track
        const auto defaultPattern = song_.addPattern("Unnamed Song", 1);
        song_.setArrangerTrackPattern(0, defaultPattern);

        mainWindow_ = std::make_unique<MainWindow>(
            getApplicationName() + " v" + getApplicationVersion(), *engine_, song_, stretchCache_);
    }

    void shutdown() override
    {
        mainWindow_.reset();
        stretchCache_.clear();
        engine_.reset();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    TramiLookAndFeel lookAndFeel_;
    Song song_;
    StretchCache stretchCache_;
    std::unique_ptr<AudioEngine> engine_;
    std::unique_ptr<MainWindow> mainWindow_;
};

} // namespace trami

START_JUCE_APPLICATION(trami::TramiApplication)
