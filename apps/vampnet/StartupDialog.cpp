#include "StartupDialog.h"

StartupDialog::StartupDialog(juce::AudioDeviceManager& deviceManager)
    : audioDeviceManager(deviceManager),
      titleLabel("Title", "tape looper setup"),
      numTracksLabel("Tracks", "number of tracks"),
      numTracksSlider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight),
      pannerLabel("Panner", "panner type"),
      audioDeviceSelector(deviceManager, 0, 256, 0, 256, true, true, true, false),
      okButton("ok")
{
    // Setup title
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions()
                                  .withName(juce::Font::getDefaultMonospacedFontName())
                                  .withHeight(20.0f))); // Monospaced, no bold
    addAndMakeVisible(titleLabel);
    
    // Setup number of tracks slider
    numTracksSlider.setRange(1, 8, 1);
    numTracksSlider.setValue(4);
    numTracks = 4; // Initialize to match slider default
    numTracksSlider.onValueChange = [this]
    {
        numTracks = static_cast<int>(numTracksSlider.getValue());
    };
    addAndMakeVisible(numTracksSlider);
    addAndMakeVisible(numTracksLabel);
    
    // Setup panner selector
    pannerCombo.addItem("Stereo", 1);
    pannerCombo.addItem("Quad", 2);
    pannerCombo.addItem("CLEAT", 3);
    pannerCombo.setSelectedId(1); // Default to "Stereo"
    pannerCombo.onChange = [this]
    {
        selectedPanner = pannerCombo.getText();
    };
    addAndMakeVisible(pannerCombo);
    addAndMakeVisible(pannerLabel);
    
    // Setup audio device selector
    addAndMakeVisible(audioDeviceSelector);
    
    // Setup OK button
    okButton.addListener(this);
    addAndMakeVisible(okButton);
    
    setSize(600, 710); // Reduced height since we removed frontend selection
}

void StartupDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    
    // Title at top
    titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(20);
    
    // Number of tracks section
    auto tracksArea = bounds.removeFromTop(40);
    numTracksLabel.setBounds(tracksArea.removeFromLeft(150));
    tracksArea.removeFromLeft(10);
    numTracksSlider.setBounds(tracksArea);
    bounds.removeFromTop(20);
    
    // Panner selection section
    auto pannerArea = bounds.removeFromTop(40);
    pannerLabel.setBounds(pannerArea.removeFromLeft(150));
    pannerArea.removeFromLeft(10);
    pannerCombo.setBounds(pannerArea.removeFromLeft(200));
    bounds.removeFromTop(20);
    
    // OK button at bottom
    auto buttonArea = bounds.removeFromBottom(40);
    okButton.setBounds(buttonArea.removeFromRight(100).reduced(5));
    bounds.removeFromBottom(10);
    
    // Audio device selector takes remaining space
    audioDeviceSelector.setBounds(bounds);
}

void StartupDialog::buttonClicked(juce::Button* button)
{
    if (button == &okButton)
    {
        DBG("[StartupDialog] OK button clicked");
        
        // Update numTracks and selectedPanner from UI values when OK is clicked
        numTracks = static_cast<int>(numTracksSlider.getValue());
        selectedPanner = pannerCombo.getText();
        
        DBG("[StartupDialog] numTracks=" << numTracks << ", panner=" << selectedPanner);
        
        // Get current device setup BEFORE modifying it
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        audioDeviceManager.getAudioDeviceSetup(setup);
        
        DBG("[StartupDialog] Current device setup:");
        DBG("  outputDeviceName: " << setup.outputDeviceName);
        DBG("  inputDeviceName: " << setup.inputDeviceName);
        DBG("  sampleRate: " << setup.sampleRate);
        DBG("  bufferSize: " << setup.bufferSize);
        DBG("  useDefaultInputChannels: " << (setup.useDefaultInputChannels ? "true" : "false"));
        DBG("  useDefaultOutputChannels: " << (setup.useDefaultOutputChannels ? "true" : "false"));
        DBG("  inputChannels bits: " << setup.inputChannels.toString(2));
        DBG("  outputChannels bits: " << setup.outputChannels.toString(2));
        
        // Get current device to check available channels
        auto* device = audioDeviceManager.getCurrentAudioDevice();
        if (device != nullptr)
        {
            DBG("[StartupDialog] Current device: " << device->getName());
            DBG("[StartupDialog] Device type: " << device->getTypeName());
            
            // Get the number of available channels
            int numInputChannels = device->getInputChannelNames().size();
            int numOutputChannels = device->getOutputChannelNames().size();
            
            DBG("[StartupDialog] Available channels - Input: " << numInputChannels << ", Output: " << numOutputChannels);
            
            // Enable all input channels
            if (numInputChannels > 0)
            {
                setup.inputChannels.clear();
                for (int i = 0; i < numInputChannels; ++i)
                {
                    setup.inputChannels.setBit(i, true);
                }
                setup.useDefaultInputChannels = false;
                DBG("[StartupDialog] Enabled all " << numInputChannels << " input channels");
                DBG("[StartupDialog] Input channels bits: " << setup.inputChannels.toString(2));
            }
            else
            {
                DBG("[StartupDialog] No input channels available");
            }
            
            // Enable all output channels
            if (numOutputChannels > 0)
            {
                setup.outputChannels.clear();
                for (int i = 0; i < numOutputChannels; ++i)
                {
                    setup.outputChannels.setBit(i, true);
                }
                setup.useDefaultOutputChannels = false;
                DBG("[StartupDialog] Enabled all " << numOutputChannels << " output channels");
                DBG("[StartupDialog] Output channels bits: " << setup.outputChannels.toString(2));
            }
            else
            {
                DBG("[StartupDialog] No output channels available");
            }
            
            // Apply the setup
            DBG("[StartupDialog] Applying device setup...");
            auto error = audioDeviceManager.setAudioDeviceSetup(setup, true);
            if (error.isNotEmpty())
            {
                DBG("[StartupDialog] ERROR applying device setup: " << error);
            }
            else
            {
                DBG("[StartupDialog] Device setup applied successfully");
                
                // Verify the setup was applied
                juce::AudioDeviceManager::AudioDeviceSetup verifySetup;
                audioDeviceManager.getAudioDeviceSetup(verifySetup);
                auto* verifyDevice = audioDeviceManager.getCurrentAudioDevice();
                
                DBG("[StartupDialog] Verification after applying setup:");
                DBG("  outputDeviceName: " << verifySetup.outputDeviceName);
                DBG("  inputDeviceName: " << verifySetup.inputDeviceName);
                if (verifyDevice != nullptr)
                {
                    DBG("  Current device: " << verifyDevice->getName());
                    DBG("  Active input channels: " << verifyDevice->getActiveInputChannels().countNumberOfSetBits());
                    DBG("  Active output channels: " << verifyDevice->getActiveOutputChannels().countNumberOfSetBits());
                }
            }
        }
        else
        {
            DBG("[StartupDialog] WARNING: No current audio device!");
        }
        
        okClicked = true;
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    }
}

juce::AudioDeviceManager::AudioDeviceSetup StartupDialog::getDeviceSetup() const
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    audioDeviceManager.getAudioDeviceSetup(setup);
    return setup;
}

void StartupDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

