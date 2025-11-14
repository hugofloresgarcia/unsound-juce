#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "StartupDialog.h"

// Frontend includes - all included but namespaced to avoid conflicts
#include "Frontends/Basic/MainComponent.h"
#include "Frontends/Text2Sound/MainComponent.h"
#include "Frontends/VampNet/MainComponent.h"

class TapeLooperApplication : public juce::JUCEApplication
{
public:
    TapeLooperApplication() {}

    const juce::String getApplicationName() override { return "Tape Looper"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        // Show startup dialog before creating main window
        int numTracks = 8; // Default value, will be updated from dialog
        juce::String selectedFrontend = "basic"; // Default frontend
        juce::AudioDeviceManager::AudioDeviceSetup deviceSetup;
        
        {
            juce::AudioDeviceManager tempDeviceManager;
            // Initialize with default devices so the dialog shows current audio setup
            tempDeviceManager.initialiseWithDefaultDevices(2, 2);
            
            auto startupDialog = std::make_unique<StartupDialog>(tempDeviceManager);
            StartupDialog* dialogPtr = startupDialog.get(); // Store pointer before releasing
            
            CustomLookAndFeel customLookAndFeel;
            startupDialog->setLookAndFeel(&customLookAndFeel);
            
            juce::DialogWindow::LaunchOptions dialogOptions;
            dialogOptions.content.setNonOwned(startupDialog.release()); // Don't auto-delete, we'll manage it
            dialogOptions.dialogTitle = "Tape Looper Setup";
            dialogOptions.dialogBackgroundColour = juce::Colours::black;
            dialogOptions.escapeKeyTriggersCloseButton = false;
            dialogOptions.useNativeTitleBar = false;
            dialogOptions.resizable = false;
            
            #if JUCE_MODAL_LOOPS_PERMITTED
            dialogOptions.componentToCentreAround = juce::TopLevelWindow::getActiveTopLevelWindow();
            juce::Process::makeForegroundProcess();
            int result = dialogOptions.runModal();
            // Access the dialog component to read the value
            if (result == 1 && dialogPtr != nullptr)
            {
                if (dialogPtr->wasOkClicked())
                {
                    numTracks = dialogPtr->getNumTracks();
                    selectedFrontend = dialogPtr->getSelectedFrontend();
                    juce::Logger::writeToLog("Selected number of tracks: " + juce::String(numTracks));
                    juce::Logger::writeToLog("Selected frontend: " + selectedFrontend);
                    // Copy audio device settings from temporary manager to the one we'll use
                    tempDeviceManager.getAudioDeviceSetup(deviceSetup);
                }
                else
                {
                    juce::Logger::writeToLog("Dialog OK not clicked, using default 8 tracks");
                }
            }
            else
            {
                juce::Logger::writeToLog("Dialog cancelled (result=" + juce::String(result) + "), using default 8 tracks");
            }
            
            // Clean up the dialog component manually since we set auto-delete to false
            if (dialogPtr != nullptr)
            {
                delete dialogPtr;
            }
            #else
            // Fallback if modal loops not permitted - use async
            auto* dialogWindow = dialogOptions.launchAsync();
            if (dialogWindow != nullptr)
            {
                dialogWindow->setAlwaysOnTop(true);
                dialogWindow->toFront(true);
                dialogWindow->enterModalState(true, nullptr, true);
            }
            // Note: In async mode, we can't reliably get the result here
            // For now, default to 4 tracks
            #endif
        }
        
        mainWindow.reset(new MainWindow(getApplicationName(), numTracks, selectedFrontend, deviceSetup));
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
        MainWindow(juce::String name, int numTracks, const juce::String& frontend, const juce::AudioDeviceManager::AudioDeviceSetup& deviceSetup)
            : DocumentWindow(name,
                            juce::Desktop::getInstance().getDefaultLookAndFeel()
                                .findColour(juce::ResizableWindow::backgroundColourId),
                            DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            
            // Create the appropriate frontend component based on selection
            juce::Component* mainComponent = nullptr;
            
            // Convert to lowercase for case-insensitive comparison
            auto frontendLower = frontend.toLowerCase();
            
            if (frontendLower == "basic")
            {
                auto* basicComponent = new Basic::MainComponent(numTracks);
                mainComponent = basicComponent;
                basicComponent->getLooperEngine().getAudioDeviceManager().setAudioDeviceSetup(deviceSetup, true);
                basicComponent->getLooperEngine().startAudio();
            }
            else if (frontendLower == "text2sound")
            {
                auto* text2SoundComponent = new Text2Sound::MainComponent(numTracks);
                mainComponent = text2SoundComponent;
                text2SoundComponent->getLooperEngine().getAudioDeviceManager().setAudioDeviceSetup(deviceSetup, true);
                text2SoundComponent->getLooperEngine().startAudio();
            }
            else if (frontendLower == "vampnet")
            {
                auto* vampNetComponent = new VampNet::MainComponent(numTracks);
                mainComponent = vampNetComponent;
                vampNetComponent->getLooperEngine().getAudioDeviceManager().setAudioDeviceSetup(deviceSetup, true);
                vampNetComponent->getLooperEngine().startAudio();
            }
            
            setContentOwned(mainComponent, true);

            #if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
            #else
            setResizable(false, false); // Fixed window size
            // Use the content component's size instead of a fixed size
            centreWithSize(mainComponent->getWidth(), mainComponent->getHeight());
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

START_JUCE_APPLICATION(TapeLooperApplication)

