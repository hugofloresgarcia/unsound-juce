#include "StartupDialog.h"
#include "CLAP/SoundPaletteCreator.h"
#include "CLAP/PaletteCreationProgressWindow.h"
#include "CLAP/PaletteCreationWorkerThread.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>

StartupDialog::StartupDialog(juce::AudioDeviceManager& deviceManager)
    : audioDeviceManager(deviceManager),
      titleLabel("Title", "claptext2sound tape looper setup"),
      numTracksLabel("Tracks", "number of tracks"),
      numTracksSlider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight),
      pannerLabel("Panner", "panner type"),
      paletteLabel("Sound Palette", "sound palette"),
      createPaletteButton("Create New..."),
      chunkSizeLabel("Chunk Size", "chunk size (seconds)"),
      chunkSizeSlider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight),
      audioDeviceSelector(deviceManager, 0, 256, 0, 256, true, true, true, false),
      okButton("ok")
{
    // Setup title
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions()
                                  .withName(juce::Font::getDefaultMonospacedFontName())
                                  .withHeight(20.0f)));
    addAndMakeVisible(titleLabel);
    
    // Setup number of tracks slider
    numTracksSlider.setRange(1, 8, 1);
    numTracksSlider.setValue(4);
    numTracks = 4;
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
    pannerCombo.setSelectedId(1);
    pannerCombo.onChange = [this]
    {
        selectedPanner = pannerCombo.getText();
    };
    addAndMakeVisible(pannerCombo);
    addAndMakeVisible(pannerLabel);
    
    // Setup sound palette selector
    refreshPaletteList();
    paletteCombo.addListener(this);
    addAndMakeVisible(paletteCombo);
    addAndMakeVisible(paletteLabel);
    
    // Setup create palette button
    createPaletteButton.addListener(this);
    addAndMakeVisible(createPaletteButton);
    
    // Setup chunk size slider
    chunkSizeSlider.setRange(1, 30, 1);
    chunkSizeSlider.setValue(10);
    chunkSizeSlider.onValueChange = [this]() {};
    addAndMakeVisible(chunkSizeSlider);
    addAndMakeVisible(chunkSizeLabel);
    
    // Setup audio device selector
    addAndMakeVisible(audioDeviceSelector);
    
    // Setup OK button
    okButton.addListener(this);
    addAndMakeVisible(okButton);
    
    setSize(600, 800);
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
    
    // Sound palette section
    auto paletteArea = bounds.removeFromTop(40);
    paletteLabel.setBounds(paletteArea.removeFromLeft(150));
    paletteArea.removeFromLeft(10);
    paletteCombo.setBounds(paletteArea.removeFromLeft(250));
    paletteArea.removeFromLeft(10);
    createPaletteButton.setBounds(paletteArea.removeFromLeft(120));
    bounds.removeFromTop(10);
    
    // Chunk size (only visible when creating new palette)
    auto chunkArea = bounds.removeFromTop(40);
    chunkSizeLabel.setBounds(chunkArea.removeFromLeft(150));
    chunkArea.removeFromLeft(10);
    chunkSizeSlider.setBounds(chunkArea);
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
    if (button == &createPaletteButton)
    {
        createNewPalette();
    }
    else if (button == &okButton)
    {
        DBG("[StartupDialog] OK button clicked");
        
        numTracks = static_cast<int>(numTracksSlider.getValue());
        selectedPanner = pannerCombo.getText();
        
        // Get selected palette path
        int selectedId = paletteCombo.getSelectedId();
        if (selectedId > 0 && selectedId <= static_cast<int>(discoveredPalettes.size()))
        {
            selectedPalettePath = discoveredPalettes[selectedId - 1].path.getFullPathName();
        }
        
        DBG("[StartupDialog] numTracks=" << numTracks << ", panner=" << selectedPanner);
        DBG("[StartupDialog] palette=" << selectedPalettePath);
        
        // Get current device setup
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        audioDeviceManager.getAudioDeviceSetup(setup);
        
        // Enable all channels (same as text2sound)
        auto* device = audioDeviceManager.getCurrentAudioDevice();
        if (device != nullptr)
        {
            int numInputChannels = device->getInputChannelNames().size();
            int numOutputChannels = device->getOutputChannelNames().size();
            
            if (numInputChannels > 0)
            {
                setup.inputChannels.clear();
                for (int i = 0; i < numInputChannels; ++i)
                {
                    setup.inputChannels.setBit(i, true);
                }
                setup.useDefaultInputChannels = false;
            }
            
            if (numOutputChannels > 0)
            {
                setup.outputChannels.clear();
                for (int i = 0; i < numOutputChannels; ++i)
                {
                    setup.outputChannels.setBit(i, true);
                }
                setup.useDefaultOutputChannels = false;
            }
            
            audioDeviceManager.setAudioDeviceSetup(setup, true);
        }
        
        okClicked = true;
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    }
}

void StartupDialog::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &paletteCombo)
    {
        // Palette selection changed
        int selectedId = paletteCombo.getSelectedId();
        if (selectedId > 0 && selectedId <= static_cast<int>(discoveredPalettes.size()))
        {
            selectedPalettePath = discoveredPalettes[selectedId - 1].path.getFullPathName();
        }
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

void StartupDialog::refreshPaletteList()
{
    paletteCombo.clear();
    discoveredPalettes = paletteManager.discoverPalettes();
    
    paletteCombo.addItem("-- Select Palette --", 0);
    paletteCombo.setSelectedId(0);
    
    for (size_t i = 0; i < discoveredPalettes.size(); ++i)
    {
        paletteCombo.addItem(discoveredPalettes[i].name, static_cast<int>(i + 1));
    }
}

void StartupDialog::createNewPalette()
{
    juce::FileChooser chooser("Select audio folder for sound palette...",
                              juce::File(),
                              "*",
                              true);
    
    if (chooser.browseForDirectory())
    {
        auto selectedFolder = chooser.getResult();
        int chunkSize = static_cast<int>(chunkSizeSlider.getValue());
        
        // Show progress window
        juce::File resultPaletteDir;
        bool creationComplete = false;
        bool creationCancelled = false;
        
        CLAPText2Sound::PaletteCreationProgressWindow::showModal(this,
            [&creationCancelled]()
            {
                creationCancelled = true;
            });
        
        // Create worker thread
        auto workerThread = std::make_unique<CLAPText2Sound::PaletteCreationWorkerThread>(selectedFolder, chunkSize);
        
        // Start thread
        workerThread->startThread();
        
        // Wait for completion (with periodic checks for cancellation)
        while (workerThread->isThreadRunning())
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(100); // Process UI events
            
            if (creationCancelled)
            {
                workerThread->signalThreadShouldExit();
                // Wait a bit for thread to exit
                int timeout = 1000; // 1 second timeout
                while (workerThread->isThreadRunning() && timeout > 0)
                {
                    juce::Thread::sleep(10);
                    timeout -= 10;
                }
                break;
            }
        }
        
        // Get result
        if (!creationCancelled)
        {
            resultPaletteDir = workerThread->getResult();
        }
        
        // Clean up thread
        workerThread->waitForThreadToExit(2000);
        workerThread.reset();
        
        // Close progress window
        if (auto* progressWindow = CLAPText2Sound::PaletteCreationProgressWindow::getInstance())
        {
            progressWindow->closeWindow();
        }
        
        // Handle result
        if (creationCancelled)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                  "Palette Creation Cancelled",
                                                  "Sound palette creation was cancelled.");
        }
        else if (resultPaletteDir.exists())
        {
            // Refresh palette list (all palettes are in ~/Documents/claptext2sound/)
            refreshPaletteList();
            
            // Find and select the newly created palette
            for (size_t i = 0; i < discoveredPalettes.size(); ++i)
            {
                if (discoveredPalettes[i].path == resultPaletteDir)
                {
                    paletteCombo.setSelectedId(static_cast<int>(i + 1));
                    break;
                }
            }
            
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                  "Palette Created",
                                                  "Sound palette created successfully!");
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                  "Palette Creation Failed",
                                                  "Failed to create sound palette. Please check the logs.");
        }
    }
}

