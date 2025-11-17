#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "MainComponent.h"
#include <flowerjuce/CustomLookAndFeel.h>

class CLEATPinkNoiseTestApplication : public juce::JUCEApplication
{
public:
    CLEATPinkNoiseTestApplication() {}

    const juce::String getApplicationName() override { return "CLEAT Pink Noise Test"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                            juce::Desktop::getInstance().getDefaultLookAndFeel()
                                .findColour(juce::ResizableWindow::backgroundColourId),
                            DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            
            auto* testComponent = new CLEATPinkNoiseTest::MainComponent();
            setContentOwned(testComponent, true);

            #if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
            #else
            setResizable(true, true);
            centreWithSize(400, 300);
            #endif

            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(CLEATPinkNoiseTestApplication)

