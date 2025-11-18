#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "StartupDialog.h"
#include "MainComponent.h"
#include <flowerjuce/CustomLookAndFeel.h>

class EmbeddingSpaceSamplerApplication : public juce::JUCEApplication
{
public:
    EmbeddingSpaceSamplerApplication() {}

    const juce::String getApplicationName() override { return "Embedding Space Sampler"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        // Show startup dialog before creating main window
        int numTracks = 8; // Default value, will be updated from dialog
        juce::String selectedPanner = "Stereo"; // Default panner
        juce::String soundPalettePath; // Sound palette path
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
            dialogOptions.dialogTitle = "Embedding Space Sampler Setup";
            dialogOptions.dialogBackgroundColour = juce::Colours::black;
            dialogOptions.escapeKeyTriggersCloseButton = false;
            dialogOptions.useNativeTitleBar = false;
            dialogOptions.resizable = false;
            
            #if JUCE_MODAL_LOOPS_PERMITTED
            dialogOptions.componentToCentreAround = juce::TopLevelWindow::getActiveTopLevelWindow();
            juce::Process::makeForegroundProcess();
            int result = dialogOptions.runModal();
            
            DBG("[Main] Dialog result: " << result);
            
            // Access the dialog component to read the value
            if (result == 1 && dialogPtr != nullptr)
            {
                if (dialogPtr->wasOkClicked())
                {
                    numTracks = dialogPtr->getNumTracks();
                    selectedPanner = dialogPtr->getSelectedPanner();
                    soundPalettePath = dialogPtr->getSoundPalettePath();
                    juce::Logger::writeToLog("Selected number of tracks: " + juce::String(numTracks));
                    juce::Logger::writeToLog("Selected panner: " + selectedPanner);
                    juce::Logger::writeToLog("Sound palette path: " + soundPalettePath);
                    
                    // Get device setup from the dialog (which has the updated setup with all channels enabled)
                    DBG("[Main] Getting device setup from StartupDialog...");
                    deviceSetup = dialogPtr->getDeviceSetup();
                }
                else
                {
                    juce::Logger::writeToLog("Dialog OK not clicked, exiting application");
                    DBG("[Main] Dialog OK not clicked, exiting");
                    if (dialogPtr != nullptr)
                    {
                        delete dialogPtr;
                    }
                    quit();
                    return;
                }
            }
            else
            {
                juce::Logger::writeToLog("Dialog cancelled (result=" + juce::String(result) + "), exiting application");
                DBG("[Main] Dialog cancelled or dialogPtr is null, exiting");
                if (dialogPtr != nullptr)
                {
                    delete dialogPtr;
                }
                quit();
                return;
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
            #endif
        }
        
        mainWindow.reset(new MainWindow(getApplicationName(), numTracks, selectedPanner, soundPalettePath, deviceSetup));
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
        MainWindow(juce::String name, int numTracks, const juce::String& pannerType, const juce::String& soundPalettePath, const juce::AudioDeviceManager::AudioDeviceSetup& deviceSetup)
            : DocumentWindow(name,
                            juce::Desktop::getInstance().getDefaultLookAndFeel()
                                .findColour(juce::ResizableWindow::backgroundColourId),
                            DocumentWindow::allButtons)
        {
            DBG("[MainWindow] Constructor called");
            DBG("[MainWindow] numTracks: " << numTracks);
            DBG("[MainWindow] soundPalettePath: " << soundPalettePath);
            
            setUsingNativeTitleBar(true);
            
            // Create EmbeddingSpaceSampler frontend component
            DBG("[MainWindow] Creating EmbeddingSpaceSampler frontend...");
            auto* samplerComponent = new EmbeddingSpaceSampler::MainComponent(numTracks, pannerType, soundPalettePath);
            
            DBG("[MainWindow] Setting device setup on EmbeddingSpaceSampler engine...");
            auto& deviceManager = samplerComponent->getLooperEngine().get_audio_device_manager();
            
            // CRITICAL: Set device type first, otherwise setAudioDeviceSetup will fail silently
            juce::String deviceType;
            const auto& deviceTypes = deviceManager.getAvailableDeviceTypes();
            for (int i = 0; i < deviceTypes.size(); ++i)
            {
                auto* type = deviceTypes[i];
                auto outputDevices = type->getDeviceNames(false);
                auto inputDevices = type->getDeviceNames(true);
                
                bool foundDevice = false;
                if (deviceSetup.outputDeviceName.isNotEmpty())
                    foundDevice = outputDevices.contains(deviceSetup.outputDeviceName);
                if (!foundDevice && deviceSetup.inputDeviceName.isNotEmpty())
                    foundDevice = inputDevices.contains(deviceSetup.inputDeviceName);
                
                if (foundDevice)
                {
                    deviceType = type->getTypeName();
                    DBG("[MainWindow] Found device type: " << deviceType);
                    break;
                }
            }
            
            if (deviceType.isNotEmpty())
            {
                deviceManager.setCurrentAudioDeviceType(deviceType, false);
            }
            
            auto error = deviceManager.setAudioDeviceSetup(deviceSetup, true);
            if (error.isNotEmpty())
            {
                DBG("[MainWindow] ERROR setting device setup: " << error);
            }
            else
            {
                DBG("[MainWindow] Device setup applied successfully");
            }
            
            DBG("[MainWindow] Starting audio...");
            samplerComponent->getLooperEngine().start_audio();
            
            setContentOwned(samplerComponent, true);

            #if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
            #else
            setResizable(false, false); // Fixed window size
            centreWithSize(samplerComponent->getWidth(), samplerComponent->getHeight());
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

START_JUCE_APPLICATION(EmbeddingSpaceSamplerApplication)

