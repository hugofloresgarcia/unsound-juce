#include "MainComponent.h"
#include "VizWindow.h"
#include <flowerjuce/Components/ModelParameterDialog.h>
#include <flowerjuce/Components/SettingsDialog.h>
#include <flowerjuce/Components/ConfigManager.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>

using namespace Text2Sound;

// TODO: Remove this debug macro after fixing segmentation fault
#define DEBUG_SEGFAULT 1
#if DEBUG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#else
#define DBG_SEGFAULT(msg)
#endif

MainComponent::MainComponent(int numTracks, const juce::String& pannerType)
    : syncButton("sync all"),
      modelParamsButton("model params"),
      settingsButton("settings"),
      sinksButton("sinks"),
      vizButton("viz"),
      titleLabel("Title", "neural tape looper"),
      audioDeviceDebugLabel("AudioDebug", ""),
      midiLearnOverlay(midiLearnManager),
      sharedModelParams(Text2Sound::LooperTrack::getDefaultText2SoundParams())
{
    DBG_SEGFAULT("ENTRY: MainComponent::MainComponent, numTracks=" + juce::String(numTracks));
    // Apply custom look and feel
    DBG_SEGFAULT("Setting look and feel");
    setLookAndFeel(&customLookAndFeel);
    
    // Initialize MIDI learn
    DBG_SEGFAULT("Initializing MIDI learn");
    midiLearnManager.setMidiInputEnabled(true);

    // Create looper tracks (limit to available engines, max 4 for now)
    DBG_SEGFAULT("Creating tracks, numTracks=" + juce::String(numTracks));
    int actualNumTracks = juce::jmin(numTracks, looperEngine.get_num_tracks());
    DBG_SEGFAULT("actualNumTracks=" + juce::String(actualNumTracks) + " (limited by engine max=" + juce::String(looperEngine.get_num_tracks()) + ")");
    std::function<juce::String()> gradioUrlProvider = [this]() { return getGradioUrl(); };
    for (int i = 0; i < actualNumTracks; ++i)
    {
        DBG_SEGFAULT("Creating LooperTrack " + juce::String(i));
        tracks.push_back(std::make_shared<LooperTrack>(looperEngine, i, gradioUrlProvider, &midiLearnManager, pannerType));
        // Initialize track with shared model params
        tracks[i]->updateModelParams(sharedModelParams);
        // Initialize track with current smoothing time
        tracks[i]->setPannerSmoothingTime(pannerSmoothingTime);
        // Initialize track with generate triggers new path setting
        tracks[i]->setGenerateTriggersNewPath(generateTriggersNewPath);
        DBG_SEGFAULT("Adding LooperTrack " + juce::String(i) + " to view");
        addAndMakeVisible(tracks[i].get());
    }
    DBG_SEGFAULT("All tracks created");
    
    // Load MIDI mappings AFTER tracks are created (so parameters are registered)
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_text2sound.xml");
    if (midiMappingsFile.existsAsFile())
        midiLearnManager.loadMappings(midiMappingsFile);
    
    // Don't load Gradio URL from config - use the default URL for text2sound4all
    // The default URL is set in MainComponent.h: "https://hugggof-saos.hf.space/"
    DBG("MainComponent: Using default Gradio URL: " + gradioUrl);
    
    // Load trajectory directory from config (default: ~/Documents/unsound-objects/trajectories)
    auto defaultTrajectoryDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                    .getChildFile("unsound-objects")
                                    .getChildFile("trajectories")
                                    .getFullPathName();
    trajectoryDir = Shared::ConfigManager::loadStringValue("text2sound", "trajectoryDir", defaultTrajectoryDir);
    DBG("MainComponent: Loaded trajectory directory from config: " + trajectoryDir);
    
    // Set size based on number of tracks
    // Each track has a fixed width, and window adjusts to fit all tracks
    DBG_SEGFAULT("Setting size");
    const int fixedTrackWidth = 220;  // Fixed width per track
    const int trackSpacing = 5;       // Space between tracks
    const int horizontalMargin = 20;  // Left + right margins
    const int topControlsHeight = 40 + 10 + 40 + 10; // Title + spacing + buttons + spacing
    const bool useCompactStereoLayout = pannerType.equalsIgnoreCase("stereo");
    const int fixedTrackHeight = useCompactStereoLayout ? 520 : 900; // Stereo panner needs less vertical space
    const int verticalMargin = 20;    // Top + bottom margins
    
    int windowWidth = (fixedTrackWidth * actualNumTracks) + (trackSpacing * (actualNumTracks - 1)) + horizontalMargin;
    int windowHeight = topControlsHeight + fixedTrackHeight + verticalMargin;
    
    setSize(windowWidth, windowHeight);

    // Setup sync button
    syncButton.onClick = [this] { syncButtonClicked(); };
    addAndMakeVisible(syncButton);

    // Setup model params button
    modelParamsButton.onClick = [this] { modelParamsButtonClicked(); };
    addAndMakeVisible(modelParamsButton);
    
    // Setup settings button
    settingsButton.onClick = [this] { settingsButtonClicked(); };
    addAndMakeVisible(settingsButton);
    
    // Setup sinks button
    sinksButton.onClick = [this] { sinksButtonClicked(); };
    addAndMakeVisible(sinksButton);
    
    // Setup viz button
    vizButton.onClick = [this] { vizButtonClicked(); };
    addAndMakeVisible(vizButton);
    
    // Load generate triggers new path setting from config
    generateTriggersNewPath = Shared::ConfigManager::loadBoolValue("text2sound", "generateTriggersNewPath", false);
    DBG("MainComponent: Loaded generate triggers new path setting from config: " + juce::String(generateTriggersNewPath ? "true" : "false"));
    
    // Create settings dialog
    settingsDialog = std::make_unique<Shared::SettingsDialog>(
        pannerSmoothingTime,
        [this](double smoothingTime) {
            pannerSmoothingTime = smoothingTime;
            DBG("MainComponent: Panner smoothing time updated to " + juce::String(smoothingTime) + " seconds");
            // Apply smoothing to all panner components
            for (auto& track : tracks)
            {
                track->setPannerSmoothingTime(smoothingTime);
            }
        },
        gradioUrl,
        [this](const juce::String& newUrl) {
            setGradioUrl(newUrl);
            // Don't save Gradio URL to config - always use the default URL for text2sound4all
            DBG("MainComponent: Gradio URL changed to: " + newUrl + " (not saved to config)");
        },
        &midiLearnManager,
        trajectoryDir,
        [this](const juce::String& newDir) {
            trajectoryDir = newDir;
            // Save to config immediately when changed
            Shared::ConfigManager::saveStringValue("text2sound", "trajectoryDir", newDir);
            DBG("MainComponent: Saved trajectory directory to config: " + newDir);
        },
        cleatGainPower,
        [this](float gainPower) {
            setCLEATGainPower(gainPower);
        },
        15, // DBScanEps (not used for text2sound)
        nullptr, // onDBScanEpsChanged (not used)
        3, // DBScanMinPts (not used for text2sound)
        nullptr, // onDBScanMinPtsChanged (not used)
        generateTriggersNewPath,
        [this](bool enabled) {
            generateTriggersNewPath = enabled;
            // Save to config immediately when changed
            Shared::ConfigManager::saveBoolValue("text2sound", "generateTriggersNewPath", enabled);
            DBG("MainComponent: Saved generate triggers new path setting to config: " + juce::String(enabled ? "true" : "false"));
            // Apply to all tracks
            for (auto& track : tracks)
            {
                track->setGenerateTriggersNewPath(enabled);
            }
        }
    );
    
    // Create model params dialog
    modelParamsDialog = std::make_unique<Shared::ModelParameterDialog>(
        "Text2Sound",
        sharedModelParams,
        [this](const juce::var& newParams) {
            sharedModelParams = newParams;
            DBG("MainComponent: Shared model parameters updated");
            // Notify all tracks to use the new params
            for (auto& track : tracks)
            {
                track->updateModelParams(sharedModelParams);
            }
        }
    );

    // Setup title label
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions()
                                  .withName(juce::Font::getDefaultMonospacedFontName())
                                  .withHeight(20.0f))); // Monospaced, slightly smaller, no bold
    addAndMakeVisible(titleLabel);
    
    // Setup audio device debug label (top right corner)
    audioDeviceDebugLabel.setJustificationType(juce::Justification::topRight);
    audioDeviceDebugLabel.setFont(juce::Font(juce::FontOptions()
                                             .withName(juce::Font::getDefaultMonospacedFontName())
                                             .withHeight(11.0f)));
    audioDeviceDebugLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(audioDeviceDebugLabel);
    
    // Setup MIDI learn overlay (covers entire window when active)
    addAndMakeVisible(midiLearnOverlay);
    addKeyListener(&midiLearnOverlay);

    // Note: Audio processing will be started by MainWindow after setup is complete

    // Start timer to update UI
    startTimer(50); // Update every 50ms
}

MainComponent::~MainComponent()
{
    stopTimer();
    
    removeKeyListener(&midiLearnOverlay);
    
    // Close sinks window before tracks are destroyed
    // Note: sinksComponent ownership was transferred to sinksWindow, so destroying
    // the window will automatically delete the component
    sinksWindow = nullptr;
    sinksComponent = nullptr; // Already nullptr after release(), but set for clarity
    
    // Save MIDI mappings
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    appDataDir.createDirectory();
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_text2sound.xml");
    midiLearnManager.saveMappings(midiMappingsFile);
    
    // Don't save Gradio URL to config - always use the default URL for text2sound4all
    DBG("MainComponent: Gradio URL: " + gradioUrl + " (not saved to config)");
    
    // Save trajectory directory to config
    Shared::ConfigManager::saveStringValue("text2sound", "trajectoryDir", trajectoryDir);
    DBG("MainComponent: Saved trajectory directory to config: " + trajectoryDir);
    
    // Clear LookAndFeel references from all child components BEFORE clearing our own
    // This prevents the assertion in LookAndFeel destructor about active weak references
    for (auto& track : tracks)
    {
        if (track != nullptr)
        {
            track->clearLookAndFeel();
        }
    }
    
    // Now safe to clear our own LookAndFeel reference
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
    modelParamsButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    settingsButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    sinksButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    vizButton.setBounds(controlArea.removeFromLeft(120));
    bounds.removeFromTop(10);

    // Tracks arranged horizontally (columns) with fixed width
    if (!tracks.empty())
    {
        const int fixedTrackWidth = 220;  // Fixed width per track (matches constructor)
        const int trackSpacing = 5;       // Space between tracks
        
        // Layout tracks horizontally with fixed width
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
        juce::String deviceName = device->getName();
        int numInputChannels = device->getActiveInputChannels().countNumberOfSetBits();
        int numOutputChannels = device->getActiveOutputChannels().countNumberOfSetBits();
        
        juce::String debugText = "IN: " + deviceName + " (" + juce::String(numInputChannels) + " ch)\n"
                               + "OUT: " + deviceName + " (" + juce::String(numOutputChannels) + " ch)";
        audioDeviceDebugLabel.setText(debugText, juce::dontSendNotification);
    }
    else
    {
        audioDeviceDebugLabel.setText("No audio device", juce::dontSendNotification);
    }
}

void MainComponent::setGradioUrl(const juce::String& newUrl)
{
    const juce::ScopedLock lock(gradioSettingsLock);
    gradioUrl = newUrl;
}

juce::String MainComponent::getGradioUrl() const
{
    const juce::ScopedLock lock(gradioSettingsLock);
    return gradioUrl;
}

void MainComponent::setCLEATGainPower(float gainPower)
{
    cleatGainPower = gainPower;
    DBG("MainComponent: CLEAT gain power updated to " + juce::String(gainPower));
    // Apply to all CLEAT panners in all tracks
    for (auto& track : tracks)
    {
        track->setCLEATGainPower(gainPower);
    }
}

void MainComponent::modelParamsButtonClicked()
{
    showModelParams();
}

void MainComponent::showModelParams()
{
    if (modelParamsDialog != nullptr)
    {
        // Update the dialog with current params in case they changed
        modelParamsDialog->updateParams(sharedModelParams);
        
        // Show the dialog (non-modal)
        modelParamsDialog->setVisible(true);
        modelParamsDialog->toFront(true);
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
        // Update the dialog with current values
        settingsDialog->updateSmoothingTime(pannerSmoothingTime);
        settingsDialog->updateGradioUrl(getGradioUrl());
        settingsDialog->updateTrajectoryDir(trajectoryDir);
        settingsDialog->updateCLEATGainPower(cleatGainPower);
        settingsDialog->updateGenerateTriggersNewPath(generateTriggersNewPath);
        settingsDialog->refreshMidiInfo();
        
        // Show the dialog (non-modal)
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
        const auto& channelLevels = looperEngine.get_channel_levels();
        sinksComponent = std::make_unique<flowerjuce::SinksWindow>(channelLevels);
        
        // Set LookAndFeel on sinks component before transferring ownership
        sinksComponent->setLookAndFeel(&customLookAndFeel);
        
        // Create dialog window
        sinksWindow = std::make_unique<SinksDialogWindow>(
            "Sinks",
            juce::Colours::black
        );
        
        // Set LookAndFeel on dialog window as well
        sinksWindow->setLookAndFeel(&customLookAndFeel);
        
        // Transfer ownership to DialogWindow (release from unique_ptr)
        sinksWindow->setContentOwned(sinksComponent.release(), true);
        sinksWindow->setResizable(true, true);
        sinksWindow->setSize(500, 500);
        sinksWindow->centreWithSize(500, 500);
        sinksWindow->setVisible(true);
        sinksWindow->toFront(true);
    }
    else
    {
        // Window already exists and is visible, bring it to front
        sinksWindow->toFront(true);
    }
}

void MainComponent::vizButtonClicked()
{
    // Check if window needs to be created or recreated (if closed by user)
    if (vizWindow == nullptr || vizComponent == nullptr || 
        (vizWindow != nullptr && !vizWindow->isVisible()))
    {
        // If window exists but was closed, clean it up first
        if (vizWindow != nullptr)
        {
            vizWindow = nullptr;
            vizComponent = nullptr;
        }
        
        // Create vector of weak_ptr to tracks (safe access)
        std::vector<std::weak_ptr<LooperTrack>> trackWeakPtrs;
        trackWeakPtrs.reserve(tracks.size());
        for (size_t i = 0; i < tracks.size(); ++i)
        {
            if (tracks[i] != nullptr)
            {
                trackWeakPtrs.push_back(tracks[i]); // Implicit conversion from shared_ptr to weak_ptr
            }
        }
        
        // Create viz component
        vizComponent = std::make_unique<VizWindow>(looperEngine, std::move(trackWeakPtrs));
        
        // Create dialog window
        vizWindow = std::make_unique<VizDialogWindow>(
            "Viz",
            juce::Colours::black
        );
        
        // Transfer ownership to DialogWindow (release from unique_ptr)
        vizWindow->setContentOwned(vizComponent.release(), true);
        vizWindow->setResizable(true, true);
        vizWindow->setSize(800, 800);
        vizWindow->centreWithSize(800, 800);
        vizWindow->setVisible(true);
        vizWindow->toFront(true);
    }
    else
    {
        // Window already exists and is visible, bring it to front
        vizWindow->toFront(true);
    }
}
