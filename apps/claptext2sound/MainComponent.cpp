#include "MainComponent.h"
#include <flowerjuce/Components/SettingsDialog.h>
#include <flowerjuce/Components/ConfigManager.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>

namespace Shared
{
    class SettingsDialog;
}

using namespace CLAPText2Sound;

MainComponent::MainComponent(int numTracks, const juce::String& pannerType, const juce::String& soundPalettePath)
    : syncButton("sync all"),
      settingsButton("settings"),
      sinksButton("sinks"),
      titleLabel("Title", "claptext2sound tape looper"),
      audioDeviceDebugLabel("AudioDebug", ""),
      midiLearnOverlay(midiLearnManager),
      soundPalettePath(soundPalettePath)
{
    DBG("ENTRY: MainComponent::MainComponent, numTracks=" + juce::String(numTracks));
    DBG("Sound palette path: " + soundPalettePath);
    
    // Initialize cached ONNX model manager (shared across all tracks for performance)
    cachedModelManager = std::make_unique<CLAPText2Sound::ONNXModelManager>();
    
    // Find ONNX models in app bundle Resources (macOS) or executable directory (other platforms)
    auto executableFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::File audioModelPath, textModelPath;
    
    #if JUCE_MAC
        // On macOS, look in app bundle Resources folder
        auto resourcesDir = executableFile.getParentDirectory()
                              .getParentDirectory()
                              .getChildFile("Resources");
        audioModelPath = resourcesDir.getChildFile("clap_audio_encoder.onnx");
        textModelPath = resourcesDir.getChildFile("clap_text_encoder.onnx");
        
        // Fallback to executable directory if not found in Resources
        if (!audioModelPath.existsAsFile())
            audioModelPath = executableFile.getParentDirectory().getChildFile("clap_audio_encoder.onnx");
        if (!textModelPath.existsAsFile())
            textModelPath = executableFile.getParentDirectory().getChildFile("clap_text_encoder.onnx");
    #else
        // On other platforms, look in executable directory
        audioModelPath = executableFile.getParentDirectory().getChildFile("clap_audio_encoder.onnx");
        textModelPath = executableFile.getParentDirectory().getChildFile("clap_text_encoder.onnx");
    #endif
    
    if (cachedModelManager->initialize(audioModelPath, textModelPath))
    {
        DBG("MainComponent: Successfully initialized cached ONNX model manager");
    }
    else
    {
        DBG("MainComponent: WARNING - Failed to initialize cached ONNX model manager (will create per-thread instances)");
        cachedModelManager.reset(); // Clear it so threads will create their own
    }
    
    // Apply custom look and feel
    setLookAndFeel(&customLookAndFeel);
    
    // Initialize MIDI learn
    midiLearnManager.setMidiInputEnabled(true);

    // Create looper tracks
    int actualNumTracks = juce::jmin(numTracks, looperEngine.get_num_tracks());
    DBG("actualNumTracks=" + juce::String(actualNumTracks));
    
    std::function<juce::String()> palettePathProvider = [this]() { return this->soundPalettePath; };
    for (int i = 0; i < actualNumTracks; ++i)
    {
        DBG("Creating LooperTrack " + juce::String(i));
        tracks.push_back(std::make_shared<LooperTrack>(looperEngine, i, palettePathProvider, &midiLearnManager, pannerType, cachedModelManager.get()));
        // Initialize track with current smoothing time
        tracks[i]->setPannerSmoothingTime(pannerSmoothingTime);
        DBG("Adding LooperTrack " + juce::String(i) + " to view");
        addAndMakeVisible(tracks[i].get());
    }
    DBG("All tracks created");
    
    // Load MIDI mappings AFTER tracks are created (so parameters are registered)
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_claptext2sound.xml");
    if (midiMappingsFile.existsAsFile())
        midiLearnManager.loadMappings(midiMappingsFile);
    
    // Load trajectory directory from config
    auto defaultTrajectoryDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                    .getChildFile("unsound-objects")
                                    .getChildFile("trajectories")
                                    .getFullPathName();
    trajectoryDir = Shared::ConfigManager::loadStringValue("claptext2sound", "trajectoryDir", defaultTrajectoryDir);
    DBG("MainComponent: Loaded trajectory directory from config: " + trajectoryDir);
    
    // Set size based on number of tracks
    const int fixedTrackWidth = 220;
    const int trackSpacing = 5;
    const int horizontalMargin = 20;
    const int topControlsHeight = 40 + 10 + 40 + 10;
    const int fixedTrackHeight = 800;
    const int verticalMargin = 20;
    
    int windowWidth = (fixedTrackWidth * actualNumTracks) + (trackSpacing * (actualNumTracks - 1)) + horizontalMargin;
    int windowHeight = topControlsHeight + fixedTrackHeight + verticalMargin;
    
    setSize(windowWidth, windowHeight);

    // Setup sync button
    syncButton.onClick = [this] { syncButtonClicked(); };
    addAndMakeVisible(syncButton);
    
    // Setup settings button
    settingsButton.onClick = [this] { settingsButtonClicked(); };
    addAndMakeVisible(settingsButton);
    
    // Setup sinks button
    sinksButton.onClick = [this] { sinksButtonClicked(); };
    addAndMakeVisible(sinksButton);
    
    // Create settings dialog
    settingsDialog = std::make_unique<Shared::SettingsDialog>(
        pannerSmoothingTime,
        [this](double smoothingTime) {
            pannerSmoothingTime = smoothingTime;
            DBG("MainComponent: Panner smoothing time updated to " + juce::String(smoothingTime) + " seconds");
            for (auto& track : tracks)
            {
                track->setPannerSmoothingTime(smoothingTime);
            }
        },
        juce::String(), // No Gradio URL for CLAP version
        [this](const juce::String&) {}, // No-op for Gradio URL
        &midiLearnManager,
        trajectoryDir,
        [this](const juce::String& newDir) {
            trajectoryDir = newDir;
            Shared::ConfigManager::saveStringValue("claptext2sound", "trajectoryDir", newDir);
            DBG("MainComponent: Saved trajectory directory to config: " + newDir);
        },
        cleatGainPower,
        [this](float gainPower) {
            setCLEATGainPower(gainPower);
        }
    );
    
    // Setup title label
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions()
                                  .withName(juce::Font::getDefaultMonospacedFontName())
                                  .withHeight(20.0f)));
    addAndMakeVisible(titleLabel);
    
    // Setup audio device debug label
    audioDeviceDebugLabel.setJustificationType(juce::Justification::topRight);
    audioDeviceDebugLabel.setFont(juce::Font(juce::FontOptions()
                                             .withName(juce::Font::getDefaultMonospacedFontName())
                                             .withHeight(11.0f)));
    audioDeviceDebugLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(audioDeviceDebugLabel);
    
    // Setup MIDI learn overlay
    addAndMakeVisible(midiLearnOverlay);
    addKeyListener(&midiLearnOverlay);

    // Start timer to update UI
    startTimer(50);
}

MainComponent::~MainComponent()
{
    stopTimer();
    
    removeKeyListener(&midiLearnOverlay);
    
    // Close sinks window
    sinksWindow = nullptr;
    sinksComponent = nullptr;
    
    // Save MIDI mappings
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    appDataDir.createDirectory();
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_claptext2sound.xml");
    midiLearnManager.saveMappings(midiMappingsFile);
    
    // Save trajectory directory to config
    Shared::ConfigManager::saveStringValue("claptext2sound", "trajectoryDir", trajectoryDir);
    DBG("MainComponent: Saved trajectory directory to config: " + trajectoryDir);
    
    // Clear LookAndFeel references
    for (auto& track : tracks)
    {
        if (track != nullptr)
        {
            track->clearLookAndFeel();
        }
    }
    
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Title at top
    titleLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(10);

    // Control buttons
    auto controlArea = bounds.removeFromTop(40);
    syncButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    settingsButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    sinksButton.setBounds(controlArea.removeFromLeft(120));
    bounds.removeFromTop(10);

    // Tracks arranged horizontally
    if (!tracks.empty())
    {
        const int fixedTrackWidth = 220;
        const int trackSpacing = 5;
        
        for (size_t i = 0; i < tracks.size(); ++i)
        {
            tracks[i]->setBounds(bounds.removeFromLeft(fixedTrackWidth));
            if (i < tracks.size() - 1)
            {
                bounds.removeFromLeft(trackSpacing);
            }
        }
    }
    
    // MIDI learn overlay covers entire window
    midiLearnOverlay.setBounds(getLocalBounds());
    
    // Audio device debug label in top right corner
    auto debugBounds = getLocalBounds().removeFromTop(60).removeFromRight(300);
    audioDeviceDebugLabel.setBounds(debugBounds.reduced(10, 5));
}

void MainComponent::timerCallback()
{
    // Repaint tracks to show recording/playing state
    for (auto& track : tracks)
    {
        track->repaint();
    }
    
    // Update audio device debug info
    updateAudioDeviceDebugInfo();
}

void MainComponent::syncButtonClicked()
{
    looperEngine.sync_all_tracks();
}

void MainComponent::updateAudioDeviceDebugInfo()
{
    auto* device = looperEngine.get_audio_device_manager().getCurrentAudioDevice();
    if (device != nullptr)
    {
        juce::String info;
        info += "Device: " + device->getName() + "\n";
        info += "Sample Rate: " + juce::String(device->getCurrentSampleRate(), 0) + " Hz\n";
        info += "Buffer Size: " + juce::String(device->getCurrentBufferSizeSamples()) + " samples\n";
        info += "Input Channels: " + juce::String(device->getActiveInputChannels().countNumberOfSetBits()) + "\n";
        info += "Output Channels: " + juce::String(device->getActiveOutputChannels().countNumberOfSetBits());
        audioDeviceDebugLabel.setText(info, juce::dontSendNotification);
    }
    else
    {
        audioDeviceDebugLabel.setText("No audio device", juce::dontSendNotification);
    }
}

void MainComponent::settingsButtonClicked()
{
    showSettings();
}

void MainComponent::showSettings()
{
    if (settingsDialog != nullptr)
    {
        settingsDialog->setVisible(true);
        settingsDialog->toFront(true);
    }
}

void MainComponent::sinksButtonClicked()
{
    // Check if window needs to be created or recreated (if closed by user)
    if (sinksWindow == nullptr || sinksComponent == nullptr || 
        (sinksWindow != nullptr && !sinksWindow->isVisible()))
    {
        // If window exists but was closed, clean it up first
        if (sinksWindow != nullptr)
        {
            sinksWindow = nullptr;
            sinksComponent = nullptr;
        }
        
        // Create sinks component (without CLEAT panner, so no pink boxes)
        const auto& channelLevels = looperEngine.getChannelLevels();
        sinksComponent = std::make_unique<flowerjuce::SinksWindow>(channelLevels);
        
        // Create dialog window
        sinksWindow = std::make_unique<SinksDialogWindow>(
            "Sinks",
            juce::Colours::black
        );
        
        // Transfer ownership to DialogWindow (release from unique_ptr)
        sinksWindow->setContentOwned(sinksComponent.release(), true);
        sinksWindow->setResizable(true, true);
        sinksWindow->setSize(800, 600);
    }
    
    sinksWindow->setVisible(true);
    sinksWindow->toFront(true);
}

void MainComponent::setCLEATGainPower(float gainPower)
{
    cleatGainPower = gainPower;
    DBG("MainComponent: CLEAT gain power updated to " + juce::String(gainPower));
    // Apply to all tracks if needed
    for (auto& track : tracks)
    {
        track->setCLEATGainPower(gainPower);
    }
}

