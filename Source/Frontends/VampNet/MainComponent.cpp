#include "MainComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>

using namespace VampNet;

// TODO: Remove this debug macro after fixing segmentation fault
#ifndef DEBUG_SEGFAULT
#define DEBUG_SEGFAULT 1
#endif

#ifdef DBG_SEGFAULT
#undef DBG_SEGFAULT
#endif

#if DEBUG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#else
#define DBG_SEGFAULT(msg)
#endif

MainComponent::MainComponent(int numTracks, const juce::String& pannerType)
    : syncButton("sync all"),
      gradioSettingsButton("gradio"),
      midiSettingsButton("midi"),
      clickSynthButton("click synth"),
      samplerButton("sampler"),
      saveConfigButton("save config"),
      loadConfigButton("load config"),
      gitInfoButton("(i)"),
      titleLabel("Title", "tape looper - vampnet"),
      audioDeviceDebugLabel("AudioDebug", ""),
      midiLearnOverlay(midiLearnManager)
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
    int actualNumTracks = juce::jmin(numTracks, looperEngine.getNumTracks());
    DBG_SEGFAULT("actualNumTracks=" + juce::String(actualNumTracks) + " (limited by engine max=" + juce::String(looperEngine.getNumTracks()) + ")");
    std::function<juce::String()> gradioUrlProvider = [this]() { return getGradioUrl(); };
    for (int i = 0; i < actualNumTracks; ++i)
    {
        DBG_SEGFAULT("Creating LooperTrack " + juce::String(i));
        tracks.push_back(std::make_unique<LooperTrack>(looperEngine, i, gradioUrlProvider, &midiLearnManager, pannerType));
        DBG_SEGFAULT("Adding LooperTrack " + juce::String(i) + " to view");
        tracksContainer.addAndMakeVisible(tracks[i].get());
    }
    DBG_SEGFAULT("All tracks created");
    
    tracksContainer.setInterceptsMouseClicks(false, true);
    trackViewport.setViewedComponent(&tracksContainer, false);
    trackViewport.setScrollBarsShown(false, true);
    addAndMakeVisible(trackViewport);
    
    // Load MIDI mappings AFTER tracks are created (so parameters are registered)
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_vampnet.xml");
    if (midiMappingsFile.existsAsFile())
        midiLearnManager.loadMappings(midiMappingsFile);

    loadDefaultSessionConfig();
    
    // Set size based on number of tracks
    // VampNet has 3 knobs instead of 2, so slightly wider tracks
    DBG_SEGFAULT("Setting size");
    const int fixedTrackWidth = 260;  // Slightly wider for 3 knobs
    const int trackSpacing = 5;
    const int horizontalMargin = 20;
    const int topControlsHeight = 40 + 10 + 40 + 10;
    const int fixedTrackHeight = 720; // Height adjusted for panner (was 650, added 70 for panner)
    const int verticalMargin = 20;
    
    int windowWidth = (fixedTrackWidth * actualNumTracks) + (trackSpacing * (actualNumTracks - 1)) + horizontalMargin;
    int windowHeight = topControlsHeight + fixedTrackHeight + verticalMargin;
    
    setSize(windowWidth, windowHeight);

    // Setup sync button
    syncButton.onClick = [this] { syncButtonClicked(); };
    addAndMakeVisible(syncButton);

    // Setup Gradio settings button
    gradioSettingsButton.onClick = [this] { gradioSettingsButtonClicked(); };
    addAndMakeVisible(gradioSettingsButton);
    
    // Setup MIDI settings button
    midiSettingsButton.onClick = [this] { midiSettingsButtonClicked(); };
    addAndMakeVisible(midiSettingsButton);
    
    // Setup click synth button
    clickSynthButton.onClick = [this] { showClickSynthWindow(); };
    addAndMakeVisible(clickSynthButton);
    
    // Setup sampler button
    samplerButton.onClick = [this] { showSamplerWindow(); };
    addAndMakeVisible(samplerButton);

    // Setup title label
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions()
                                  .withName(juce::Font::getDefaultMonospacedFontName())
                                  .withHeight(20.0f)));
    addAndMakeVisible(titleLabel);
    
    gitInfo = Shared::GitInfoProvider::query();
    gitInfoButton.setTooltip("show git info");
    gitInfoButton.onClick = [this]()
    {
        juce::String message;
        if (gitInfo.isValid())
        {
            message << "branch: " << gitInfo.branch << "\n"
                    << "commit: " << gitInfo.commit << "\n"
                    << "time: " << gitInfo.timestamp;
        }
        else
        {
            message = gitInfo.error.isNotEmpty() ? gitInfo.error : "git info unavailable";
        }
        
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                               "git info",
                                               message);
    };
    addAndMakeVisible(gitInfoButton);
    gitInfoButton.setEnabled(gitInfo.isValid());

    saveConfigButton.onClick = [this]() { saveConfigButtonClicked(); };
    addAndMakeVisible(saveConfigButton);

    loadConfigButton.onClick = [this]() { loadConfigButtonClicked(); };
    addAndMakeVisible(loadConfigButton);
    
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
    
    // Setup keyboard listener for click synth
    addKeyListener(this); // Listen for 'k' key

    // Start timer to update UI
    startTimer(50); // Update every 50ms

    layoutTracks();
}

MainComponent::~MainComponent()
{
    stopTimer();
    
    removeKeyListener(&midiLearnOverlay);
    removeKeyListener(this);
    
    // Save MIDI mappings
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    appDataDir.createDirectory();
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_vampnet.xml");
    midiLearnManager.saveMappings(midiMappingsFile);
    
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Title + git info button
    auto titleArea = bounds.removeFromTop(40);
    auto infoArea = titleArea.removeFromLeft(40);
    gitInfoButton.setBounds(infoArea.reduced(5));
    titleLabel.setBounds(titleArea);
    bounds.removeFromTop(10);

    // Control buttons
    auto controlArea = bounds.removeFromTop(40);
    syncButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    gradioSettingsButton.setBounds(controlArea.removeFromLeft(180));
    controlArea.removeFromLeft(10);
    midiSettingsButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    clickSynthButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    samplerButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    saveConfigButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    loadConfigButton.setBounds(controlArea.removeFromLeft(120));
    bounds.removeFromTop(10);

    // Tracks arranged horizontally with fixed width
    trackViewport.setBounds(bounds);
    layoutTracks();
    
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
    looperEngine.syncAllTracks();
}

void MainComponent::updateAudioDeviceDebugInfo()
{
    auto* device = looperEngine.getAudioDeviceManager().getCurrentAudioDevice();
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

void MainComponent::gradioSettingsButtonClicked()
{
    showGradioSettings();
}

void MainComponent::showGradioSettings()
{
    juce::AlertWindow settingsWindow("gradio settings",
                                     "enter the gradio space url for vampnet generation.",
                                     juce::AlertWindow::NoIcon);

    settingsWindow.addTextEditor("gradioUrl", getGradioUrl(), "gradio url:");
    settingsWindow.addButton("cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    settingsWindow.addButton("save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    settingsWindow.centreAroundComponent(this, 450, 200);

    if (settingsWindow.runModalLoop() == 1)
    {
        juce::String newUrl = settingsWindow.getTextEditorContents("gradioUrl").trim();

        if (newUrl.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "invalid url",
                                                   "the gradio url cannot be empty.");
            return;
        }

        juce::URL parsedUrl(newUrl);
        if (!parsedUrl.isWellFormed() || parsedUrl.getScheme().isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "invalid url",
                                                   "please enter a valid gradio url, including the protocol (e.g., https://).");
            return;
        }

        if (!newUrl.endsWithChar('/'))
            newUrl += "/";

        setGradioUrl(newUrl);
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

void MainComponent::midiSettingsButtonClicked()
{
    showMidiSettings();
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    // Handle 'k' key for click synth or sampler
    if (key.getKeyCode() == 'k' || key.getKeyCode() == 'K')
    {
        // Check sampler first (if enabled)
        if (samplerWindow != nullptr && samplerWindow->isEnabled())
        {
            int selectedTrack = samplerWindow->getSelectedTrack();
            
            // Trigger sampler on selected track(s)
            if (selectedTrack >= 0 && selectedTrack < static_cast<int>(tracks.size()))
            {
                // Single track selected
                auto& trackEngine = looperEngine.getTrackEngine(selectedTrack);
                if (trackEngine.getSampler().hasSample())
                {
                    trackEngine.getSampler().trigger();
                    
                    auto& track = looperEngine.getTrack(selectedTrack);
                    if (!track.writeHead.getRecordEnable())
                    {
                        track.writeHead.setRecordEnable(true);
                        tracks[selectedTrack]->repaint();
                    }
                }
            }
            else if (selectedTrack == -1)
            {
                // All tracks - trigger sampler on all tracks
                for (size_t i = 0; i < tracks.size(); ++i)
                {
                    auto& trackEngine = looperEngine.getTrackEngine(static_cast<int>(i));
                    if (trackEngine.getSampler().hasSample())
                    {
                        trackEngine.getSampler().trigger();
                        
                        auto& track = looperEngine.getTrack(static_cast<int>(i));
                        if (!track.writeHead.getRecordEnable())
                        {
                            track.writeHead.setRecordEnable(true);
                            tracks[i]->repaint();
                        }
                    }
                }
            }
        }
        // Check click synth if sampler is not enabled
        else if (clickSynthWindow != nullptr && clickSynthWindow->isEnabled())
        {
            int selectedTrack = clickSynthWindow->getSelectedTrack();
            
            // Trigger click on selected track(s)
            if (selectedTrack >= 0 && selectedTrack < static_cast<int>(tracks.size()))
            {
                // Single track selected
                auto& trackEngine = looperEngine.getTrackEngine(selectedTrack);
                trackEngine.getClickSynth().triggerClick();
                
                auto& track = looperEngine.getTrack(selectedTrack);
                if (!track.writeHead.getRecordEnable())
                {
                    track.writeHead.setRecordEnable(true);
                    tracks[selectedTrack]->repaint();
                }
            }
            else if (selectedTrack == -1)
            {
                // All tracks - trigger click on all tracks
                for (size_t i = 0; i < tracks.size(); ++i)
                {
                    auto& trackEngine = looperEngine.getTrackEngine(static_cast<int>(i));
                    trackEngine.getClickSynth().triggerClick();
                    
                    auto& track = looperEngine.getTrack(static_cast<int>(i));
                    if (!track.writeHead.getRecordEnable())
                    {
                        track.writeHead.setRecordEnable(true);
                        tracks[i]->repaint();
                    }
                }
            }
        }
        return true;
    }
    
    return false;
}

void MainComponent::showClickSynthWindow()
{
    if (clickSynthWindow == nullptr)
    {
        int numTracks = static_cast<int>(tracks.size());
        clickSynthWindow = std::make_unique<ClickSynthWindow>(looperEngine, numTracks, &midiLearnManager);
    }
    
    clickSynthWindow->setVisible(true);
    clickSynthWindow->toFront(true);
}

void MainComponent::showSamplerWindow()
{
    if (samplerWindow == nullptr)
    {
        int numTracks = static_cast<int>(tracks.size());
        samplerWindow = std::make_unique<SamplerWindow>(looperEngine, numTracks, &midiLearnManager);
    }
    
    samplerWindow->setVisible(true);
    samplerWindow->toFront(true);
}

void MainComponent::showMidiSettings()
{
    auto devices = midiLearnManager.getAvailableMidiDevices();
    
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "MIDI Learn",
        "MIDI Learn is enabled!\n\n"
        "How to use:\n"
        "1. Right-click any control (transport, level, knobs, generate)\n"
        "2. Select 'MIDI Learn...' from the menu\n"
        "3. Move a MIDI controller to assign it\n"
        "   (or click/press ESC to cancel)\n\n"
        "Available MIDI devices:\n" + 
        (devices.isEmpty() ? "  (none)" : "  " + devices.joinIntoString("\n  ")) + "\n\n"
        "Current mappings: " + juce::String(midiLearnManager.getAllMappings().size()),
        "OK"
    );
}

void MainComponent::saveConfigButtonClicked()
{
    SessionConfig config = buildSessionConfig();
    juce::FileChooser chooser("Save VampNet config", getConfigDirectory(), "*.json");
    if (!chooser.browseForFileToSave(true))
        return;

    auto file = chooser.getResult();
    auto result = config.saveToFile(file);
    if (result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "save failed",
                                               result.getErrorMessage());
        return;
    }

    juce::AlertWindow prompt("config saved",
                             "Config saved to:\n" + file.getFullPathName(),
                             juce::AlertWindow::NoIcon);
    auto* toggle = new juce::ToggleButton("Set as default");
    prompt.addCustomComponent(toggle);
    prompt.addButton("done", 1, juce::KeyPress(juce::KeyPress::returnKey));
    if (prompt.runModalLoop() == 1 && toggle->getToggleState())
    {
        config.saveToFile(getDefaultConfigFile());
    }
}

void MainComponent::loadConfigButtonClicked()
{
    juce::FileChooser chooser("Load VampNet config", getConfigDirectory(), "*.json");
    if (!chooser.browseForFileToOpen())
        return;

    auto file = chooser.getResult();
    SessionConfig config;
    auto result = SessionConfig::loadFromFile(file, config);
    if (result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "load failed",
                                               result.getErrorMessage());
        return;
    }

    juce::AlertWindow prompt("load config",
                             "Load this config?\n" + file.getFullPathName(),
                             juce::AlertWindow::NoIcon);
    auto* toggle = new juce::ToggleButton("Set as default");
    prompt.addCustomComponent(toggle);
    prompt.addButton("load", 1, juce::KeyPress(juce::KeyPress::returnKey));
    prompt.addButton("cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    if (prompt.runModalLoop() == 1)
    {
        applySessionConfig(config, true);
        if (toggle->getToggleState())
        {
            config.saveToFile(getDefaultConfigFile());
        }
    }
}

void MainComponent::loadDefaultSessionConfig()
{
    auto defaultFile = getDefaultConfigFile();
    if (!defaultFile.existsAsFile())
        return;

    SessionConfig config;
    auto result = SessionConfig::loadFromFile(defaultFile, config);
    if (result.wasOk())
    {
        applySessionConfig(config, false);
    }
    else
    {
        juce::Logger::writeToLog("Failed to load default VampNet config: " + result.getErrorMessage());
    }
}

SessionConfig MainComponent::buildSessionConfig() const
{
    SessionConfig config;
    config.gradioUrl = getGradioUrl();

    for (const auto& trackPtr : tracks)
    {
        if (!trackPtr)
            continue;

        SessionConfig::TrackState state;
        state.trackIndex = trackPtr->getTrackIndex();
        state.knobState = trackPtr->getKnobState();
        state.vampNetParams = trackPtr->getCustomParams();
        state.autogenEnabled = trackPtr->isAutogenEnabled();
        state.useOutputAsInput = trackPtr->isUseOutputAsInputEnabled();
        state.levelDb = trackPtr->getLevelDb();
        state.pannerState = trackPtr->getPannerState();

        config.tracks.push_back(state);
    }

    config.midiMappings = midiLearnManager.getAllMappings();
    return config;
}

void MainComponent::applySessionConfig(const SessionConfig& config, bool showErrorsOnFailure)
{
    if (config.gradioUrl.isNotEmpty())
        setGradioUrl(config.gradioUrl);

    for (const auto& trackState : config.tracks)
    {
        if (trackState.trackIndex < 0 || trackState.trackIndex >= static_cast<int>(tracks.size()))
        {
            if (showErrorsOnFailure)
            {
                juce::Logger::writeToLog("Config refers to missing track index " + juce::String(trackState.trackIndex));
            }
            continue;
        }

        auto* track = tracks[trackState.trackIndex].get();
        if (trackState.knobState.isObject())
            track->applyKnobState(trackState.knobState);

        if (trackState.vampNetParams.isObject())
            track->setCustomParams(trackState.vampNetParams);

        track->setAutogenEnabled(trackState.autogenEnabled);
        track->setUseOutputAsInputEnabled(trackState.useOutputAsInput);
        track->setLevelDb(trackState.levelDb, juce::sendNotificationSync);
        track->applyPannerState(trackState.pannerState);
    }

    if (!config.midiMappings.empty())
        midiLearnManager.applyMappings(config.midiMappings);
}

void MainComponent::layoutTracks()
{
    const int fixedTrackWidth = 260;
    const int trackSpacing = 5;
    const int fixedTrackHeight = 720;
    
    int numTracks = static_cast<int>(tracks.size());
    int contentWidth = numTracks > 0
        ? (fixedTrackWidth * numTracks) + (trackSpacing * (numTracks - 1))
        : trackViewport.getWidth();
    contentWidth = juce::jmax(trackViewport.getWidth(), contentWidth);
    
    tracksContainer.setSize(contentWidth, fixedTrackHeight);
    
    int x = 0;
    for (auto& track : tracks)
    {
        track->setBounds(x, 0, fixedTrackWidth, fixedTrackHeight);
        x += fixedTrackWidth + trackSpacing;
    }
}

juce::File MainComponent::getConfigDirectory() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("TapeLooper");
    dir.createDirectory();
    return dir;
}

juce::File MainComponent::getDefaultConfigFile() const
{
    return getConfigDirectory().getChildFile("vampnet_default_config.json");
}
