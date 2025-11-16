#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "StartupDialog.h"

// Frontend includes - all included but namespaced to avoid conflicts
#include "Frontends/Basic/MainComponent.h"
#include "Frontends/Text2Sound/MainComponent.h"
#include "Frontends/VampNet/MainComponent.h"
#include "Frontends/WhAM/MainComponent.h"

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
        juce::String selectedPanner = "Stereo"; // Default panner
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
            
            DBG("[Main] Dialog result: " << result);
            
            // Access the dialog component to read the value
            if (result == 1 && dialogPtr != nullptr)
            {
                if (dialogPtr->wasOkClicked())
                {
                    numTracks = dialogPtr->getNumTracks();
                    selectedFrontend = dialogPtr->getSelectedFrontend();
                    selectedPanner = dialogPtr->getSelectedPanner();
                    juce::Logger::writeToLog("Selected number of tracks: " + juce::String(numTracks));
                    juce::Logger::writeToLog("Selected frontend: " + selectedFrontend);
                    juce::Logger::writeToLog("Selected panner: " + selectedPanner);
                    
                    // Get device setup from the dialog (which has the updated setup with all channels enabled)
                    DBG("[Main] Getting device setup from StartupDialog...");
                    deviceSetup = dialogPtr->getDeviceSetup();
                    
                    DBG("[Main] Device setup retrieved from StartupDialog:");
                    DBG("  outputDeviceName: " << deviceSetup.outputDeviceName);
                    DBG("  inputDeviceName: " << deviceSetup.inputDeviceName);
                    DBG("  sampleRate: " << deviceSetup.sampleRate);
                    DBG("  bufferSize: " << deviceSetup.bufferSize);
                    DBG("  useDefaultInputChannels: " << (deviceSetup.useDefaultInputChannels ? "true" : "false"));
                    DBG("  useDefaultOutputChannels: " << (deviceSetup.useDefaultOutputChannels ? "true" : "false"));
                    DBG("  inputChannels bits: " << deviceSetup.inputChannels.toString(2));
                    DBG("  outputChannels bits: " << deviceSetup.outputChannels.toString(2));
                    
                    // Also verify from tempDeviceManager for comparison
                    juce::AudioDeviceManager::AudioDeviceSetup tempSetup;
                    tempDeviceManager.getAudioDeviceSetup(tempSetup);
                    DBG("[Main] Device setup from tempDeviceManager for comparison:");
                    DBG("  outputDeviceName: " << tempSetup.outputDeviceName);
                    DBG("  inputDeviceName: " << tempSetup.inputDeviceName);
                    
                    // Verify current device
                    auto* currentDevice = tempDeviceManager.getCurrentAudioDevice();
                    if (currentDevice != nullptr)
                    {
                        DBG("[Main] Current device in tempDeviceManager: " << currentDevice->getName());
                        DBG("[Main] Active input channels: " << currentDevice->getActiveInputChannels().countNumberOfSetBits());
                        DBG("[Main] Active output channels: " << currentDevice->getActiveOutputChannels().countNumberOfSetBits());
                    }
                    else
                    {
                        DBG("[Main] WARNING: No current device in tempDeviceManager!");
                    }
                }
                else
                {
                    juce::Logger::writeToLog("Dialog OK not clicked, exiting application");
                    DBG("[Main] Dialog OK not clicked, exiting");
                    // Clean up the dialog component manually since we set auto-delete to false
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
                // Clean up the dialog component manually since we set auto-delete to false
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
            // Note: In async mode, we can't reliably get the result here
            // For now, default to 4 tracks
            #endif
        }
        
        mainWindow.reset(new MainWindow(getApplicationName(), numTracks, selectedFrontend, selectedPanner, deviceSetup));
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
        MainWindow(juce::String name, int numTracks, const juce::String& frontend, const juce::String& pannerType, const juce::AudioDeviceManager::AudioDeviceSetup& deviceSetup)
            : DocumentWindow(name,
                            juce::Desktop::getInstance().getDefaultLookAndFeel()
                                .findColour(juce::ResizableWindow::backgroundColourId),
                            DocumentWindow::allButtons)
        {
            DBG("[MainWindow] Constructor called");
            DBG("[MainWindow] Frontend: " << frontend << ", numTracks: " << numTracks);
            DBG("[MainWindow] Device setup received:");
            DBG("  outputDeviceName: " << deviceSetup.outputDeviceName);
            DBG("  inputDeviceName: " << deviceSetup.inputDeviceName);
            DBG("  sampleRate: " << deviceSetup.sampleRate);
            DBG("  bufferSize: " << deviceSetup.bufferSize);
            DBG("  useDefaultInputChannels: " << (deviceSetup.useDefaultInputChannels ? "true" : "false"));
            DBG("  useDefaultOutputChannels: " << (deviceSetup.useDefaultOutputChannels ? "true" : "false"));
            DBG("  inputChannels bits: " << deviceSetup.inputChannels.toString(2));
            DBG("  outputChannels bits: " << deviceSetup.outputChannels.toString(2));
            
            setUsingNativeTitleBar(true);
            
            // Create the appropriate frontend component based on selection
            juce::Component* mainComponent = nullptr;
            
            // Convert to lowercase for case-insensitive comparison
            auto frontendLower = frontend.toLowerCase();
            
            if (frontendLower == "basic")
            {
                DBG("[MainWindow] Creating Basic frontend...");
                auto* basicComponent = new Basic::MainComponent(numTracks, pannerType);
                mainComponent = basicComponent;
                
                DBG("[MainWindow] Setting device setup on Basic looper engine...");
                auto& deviceManager = basicComponent->getLooperEngine().getAudioDeviceManager();
                
                // CRITICAL: Set device type first, otherwise setAudioDeviceSetup will fail silently
                // Find the device type that contains our device
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
                        DBG("[MainWindow] Found device type for TX-6: " << deviceType);
                        break;
                    }
                }
                
                if (deviceType.isNotEmpty())
                {
                    DBG("[MainWindow] Setting device type to: " << deviceType);
                    deviceManager.setCurrentAudioDeviceType(deviceType, false);
                }
                else
                {
                    DBG("[MainWindow] WARNING: Could not find device type, using default");
                }
                
                auto error = deviceManager.setAudioDeviceSetup(deviceSetup, true);
                if (error.isNotEmpty())
                {
                    DBG("[MainWindow] ERROR setting device setup: " << error);
                }
                else
                {
                    DBG("[MainWindow] Device setup applied successfully");
                    
                    // Verify device after setup
                    auto* verifyDevice = deviceManager.getCurrentAudioDevice();
                    if (verifyDevice != nullptr)
                    {
                        DBG("[MainWindow] Device after setup: " << verifyDevice->getName());
                        DBG("[MainWindow] Active input channels: " << verifyDevice->getActiveInputChannels().countNumberOfSetBits());
                        DBG("[MainWindow] Active output channels: " << verifyDevice->getActiveOutputChannels().countNumberOfSetBits());
                    }
                    else
                    {
                        DBG("[MainWindow] WARNING: No device after setup!");
                    }
                }
                
                DBG("[MainWindow] Starting audio...");
                basicComponent->getLooperEngine().startAudio();
                
                // Update channel selectors now that device is initialized
                basicComponent->updateAllChannelSelectors();
                
                // Verify device after startAudio
                auto* finalDevice = deviceManager.getCurrentAudioDevice();
                if (finalDevice != nullptr)
                {
                    DBG("[MainWindow] Final device after startAudio: " << finalDevice->getName());
                    DBG("[MainWindow] Final active input channels: " << finalDevice->getActiveInputChannels().countNumberOfSetBits());
                    DBG("[MainWindow] Final active output channels: " << finalDevice->getActiveOutputChannels().countNumberOfSetBits());
                }
            }
            else if (frontendLower == "text2sound")
            {
                DBG("[MainWindow] Creating Text2Sound frontend...");
                auto* text2SoundComponent = new Text2Sound::MainComponent(numTracks, pannerType);
                mainComponent = text2SoundComponent;
                
                DBG("[MainWindow] Setting device setup on Text2Sound looper engine...");
                auto& deviceManager = text2SoundComponent->getLooperEngine().getAudioDeviceManager();
                
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
                    
                    // Verify device after setup
                    auto* verifyDevice = deviceManager.getCurrentAudioDevice();
                    if (verifyDevice != nullptr)
                    {
                        DBG("[MainWindow] Device after setup: " << verifyDevice->getName());
                        DBG("[MainWindow] Active input channels: " << verifyDevice->getActiveInputChannels().countNumberOfSetBits());
                        DBG("[MainWindow] Active output channels: " << verifyDevice->getActiveOutputChannels().countNumberOfSetBits());
                    }
                }
                
                DBG("[MainWindow] Starting audio...");
                text2SoundComponent->getLooperEngine().startAudio();
                
                // Verify device after startAudio
                auto* finalDevice = deviceManager.getCurrentAudioDevice();
                if (finalDevice != nullptr)
                {
                    DBG("[MainWindow] Final device after startAudio: " << finalDevice->getName());
                }
            }
            else if (frontendLower == "vampnet")
            {
                DBG("[MainWindow] Creating VampNet frontend...");
                auto* vampNetComponent = new VampNet::MainComponent(numTracks, pannerType);
                mainComponent = vampNetComponent;
                
                DBG("[MainWindow] Setting device setup on VampNet looper engine...");
                auto& deviceManager = vampNetComponent->getLooperEngine().getAudioDeviceManager();
                
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
                    
                    // Verify device after setup
                    auto* verifyDevice = deviceManager.getCurrentAudioDevice();
                    if (verifyDevice != nullptr)
                    {
                        DBG("[MainWindow] Device after setup: " << verifyDevice->getName());
                        DBG("[MainWindow] Active input channels: " << verifyDevice->getActiveInputChannels().countNumberOfSetBits());
                        DBG("[MainWindow] Active output channels: " << verifyDevice->getActiveOutputChannels().countNumberOfSetBits());
                    }
                }
                
                DBG("[MainWindow] Starting audio...");
                vampNetComponent->getLooperEngine().startAudio();
                
                // Verify device after startAudio
                auto* finalDevice = deviceManager.getCurrentAudioDevice();
                if (finalDevice != nullptr)
                {
                    DBG("[MainWindow] Final device after startAudio: " << finalDevice->getName());
                }
            }
            else if (frontendLower == "wham")
            {
                DBG("[MainWindow] Creating WhAM frontend...");
                auto* whamComponent = new WhAM::MainComponent(numTracks, pannerType);
                mainComponent = whamComponent;
                
                DBG("[MainWindow] Setting device setup on WhAM looper engine...");
                auto& deviceManager = whamComponent->getLooperEngine().getAudioDeviceManager();
                
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
                    
                    // Verify device after setup
                    auto* verifyDevice = deviceManager.getCurrentAudioDevice();
                    if (verifyDevice != nullptr)
                    {
                        DBG("[MainWindow] Device after setup: " << verifyDevice->getName());
                        DBG("[MainWindow] Active input channels: " << verifyDevice->getActiveInputChannels().countNumberOfSetBits());
                        DBG("[MainWindow] Active output channels: " << verifyDevice->getActiveOutputChannels().countNumberOfSetBits());
                    }
                }
                
                DBG("[MainWindow] Starting audio...");
                whamComponent->getLooperEngine().startAudio();
                
                // Verify device after startAudio
                auto* finalDevice = deviceManager.getCurrentAudioDevice();
                if (finalDevice != nullptr)
                {
                    DBG("[MainWindow] Final device after startAudio: " << finalDevice->getName());
                }
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

