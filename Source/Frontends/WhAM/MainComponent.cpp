#include "MainComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
// Ensure VampNet types are fully defined (they're forward declared in VampNetTrackEngine.h)
#include "../VampNet/ClickSynth.h"
#include "../VampNet/Sampler.h"

using namespace WhAM;

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
      gradioSettingsButton("gradio settings"),
      midiSettingsButton("midi settings"),
      clickSynthButton("click synth"),
      samplerButton("sampler"),
      vizButton("viz"),
      overflowButton("..."),
      titleLabel("Title", "tape looper - wham"),
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
    sharedModelParams = LooperTrack::createDefaultVampNetParams();
    std::function<juce::String()> gradioUrlProvider = [this]() { return getGradioUrl(); };
    for (int i = 0; i < actualNumTracks; ++i)
    {
        DBG_SEGFAULT("Creating LooperTrack " + juce::String(i));
        bool showModelParameterControls = (i == 0);
        tracks.push_back(std::make_unique<LooperTrack>(looperEngine,
                                                       i,
                                                       gradioUrlProvider,
                                                       &midiLearnManager,
                                                       pannerType,
                                                       &sharedModelParams,
                                                       showModelParameterControls));
        DBG_SEGFAULT("Adding LooperTrack " + juce::String(i) + " to container");
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
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_wham.xml");
    if (midiMappingsFile.existsAsFile())
        midiLearnManager.loadMappings(midiMappingsFile);
    
    loadDefaultSessionConfig();
    
    // Set size based on number of tracks
    // WhAM has 3 knobs instead of 2, so slightly wider tracks
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

    // Setup title label (add first so it's behind other components)
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions()
                                  .withName(juce::Font::getDefaultMonospacedFontName())
                                  .withHeight(20.0f)));
    addAndMakeVisible(titleLabel);
    
    gitInfo = Shared::GitInfoProvider::query();
    gitInfoButton.setButtonText("(i)");
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
    gitInfoButton.setEnabled(gitInfo.isValid());
    addAndMakeVisible(gitInfoButton);
    
    // Setup audio device debug label (top right corner)
    audioDeviceDebugLabel.setJustificationType(juce::Justification::topRight);
    audioDeviceDebugLabel.setFont(juce::Font(juce::FontOptions()
                                             .withName(juce::Font::getDefaultMonospacedFontName())
                                             .withHeight(11.0f)));
    audioDeviceDebugLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(audioDeviceDebugLabel);

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
    
    // Setup viz button
    vizButton.onClick = [this] { showVizWindow(); };
    addAndMakeVisible(vizButton);
    
    saveConfigButton.setButtonText("save config");
    saveConfigButton.onClick = [this]() { saveConfigButtonClicked(); };
    addAndMakeVisible(saveConfigButton);

    loadConfigButton.setButtonText("load config");
    loadConfigButton.onClick = [this]() { loadConfigButtonClicked(); };
    addAndMakeVisible(loadConfigButton);
    
    // Setup overflow button
    overflowButton.onClick = [this] { showOverflowMenu(); };
    addAndMakeVisible(overflowButton);
    
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
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_wham.xml");
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

    // Title + git info
    auto titleArea = bounds.removeFromTop(40);
    auto infoArea = titleArea.removeFromLeft(40);
    gitInfoButton.setBounds(infoArea.reduced(5));
    titleLabel.setBounds(titleArea);
    bounds.removeFromTop(10);

    // Control buttons with overflow logic
    auto controlArea = bounds.removeFromTop(40);
    const int buttonSpacing = 10;
    const int overflowButtonWidth = 60;

    struct ButtonInfo
    {
        juce::TextButton* button;
        int width;
    };

    std::vector<ButtonInfo> buttons = {
        {&syncButton, 120},
        {&gradioSettingsButton, 180},
        {&midiSettingsButton, 120},
        {&clickSynthButton, 120},
        {&samplerButton, 120},
        {&vizButton, 120},
        {&saveConfigButton, 130},
        {&loadConfigButton, 130}
    };

    int availableWidth = controlArea.getWidth();
    int usedWidth = 0;
    int visibleButtonCount = 0;

    for (size_t i = 0; i < buttons.size(); ++i)
    {
        int buttonWidth = buttons[i].width;
        int spacing = (visibleButtonCount > 0) ? buttonSpacing : 0;
        int widthNeeded = usedWidth + spacing + buttonWidth;

        if (widthNeeded <= availableWidth)
        {
            usedWidth = widthNeeded;
            visibleButtonCount++;
        }
        else
        {
            break;
        }
    }

    bool hasOverflow = visibleButtonCount < static_cast<int>(buttons.size());
    if (hasOverflow)
    {
        int overflowSpace = buttonSpacing + overflowButtonWidth;
        visibleButtonCount = 0;
        usedWidth = 0;

        for (size_t i = 0; i < buttons.size(); ++i)
        {
            int buttonWidth = buttons[i].width;
            int spacing = (visibleButtonCount > 0) ? buttonSpacing : 0;
            int widthNeeded = usedWidth + spacing + buttonWidth;

            if (widthNeeded + overflowSpace <= availableWidth)
            {
                usedWidth = widthNeeded;
                visibleButtonCount++;
            }
            else
            {
                break;
            }
        }
        hasOverflow = visibleButtonCount < static_cast<int>(buttons.size());
    }

    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i].button->setVisible(false);

    int xPos = controlArea.getX();
    int yPos = controlArea.getY();
    for (int i = 0; i < visibleButtonCount; ++i)
    {
        if (i > 0)
            xPos += buttonSpacing;

        buttons[i].button->setBounds(xPos, yPos, buttons[i].width, controlArea.getHeight());
        buttons[i].button->setVisible(true);
        xPos += buttons[i].width;
    }

    for (size_t i = visibleButtonCount; i < buttons.size(); ++i)
        buttons[i].button->setVisible(false);

    if (hasOverflow)
    {
        xPos += buttonSpacing;
        overflowButton.setBounds(xPos, yPos, overflowButtonWidth, controlArea.getHeight());
        overflowButton.setVisible(true);
    }
    else
    {
        overflowButton.setVisible(false);
    }

    bounds.removeFromTop(10);
    trackViewport.setBounds(bounds);
    layoutTracks();

    midiLearnOverlay.setBounds(getLocalBounds());

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
    // Handle 1-8 keys for track selection
    int keyCode = key.getKeyCode();
    if (keyCode >= '1' && keyCode <= '8')
    {
        int trackNum = keyCode - '1';  // 0-7
        if (trackNum < static_cast<int>(tracks.size()))
        {
            activeTrackIndex = trackNum;
            DBG("Selected track " + juce::String(trackNum + 1));
            // Visual feedback: repaint all tracks to show selection
            for (auto& track : tracks)
                track->repaint();
        }
        return true;
    }
    
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

bool MainComponent::keyStateChanged(bool isKeyDown, juce::Component* originatingComponent)
{
    // Handle 'r' key for hold-to-record
    if (juce::KeyPress::isKeyCurrentlyDown('r') || juce::KeyPress::isKeyCurrentlyDown('R'))
    {
        if (!isRecordingHeld)
        {
            // 'r' key just pressed - start recording on active track
            isRecordingHeld = true;
            
            if (activeTrackIndex < static_cast<int>(tracks.size()))
            {
                auto& track = looperEngine.getTrack(activeTrackIndex);
                
                // Enable recording
                track.writeHead.setRecordEnable(true);
                
                // Start playback if not already playing
                if (!track.isPlaying.load())
                {
                    track.isPlaying = true;
                }
                
                DBG("Started recording on track " + juce::String(activeTrackIndex + 1));
                tracks[activeTrackIndex]->repaint();
            }
        }
        return true;
    }
    else if (isRecordingHeld)
    {
        // 'r' key just released - stop recording and trigger generate
        isRecordingHeld = false;
        
        if (activeTrackIndex < static_cast<int>(tracks.size()))
        {
            auto& track = looperEngine.getTrack(activeTrackIndex);
            
            // Disable recording
            track.writeHead.setRecordEnable(false);
            
            DBG("Stopped recording on track " + juce::String(activeTrackIndex + 1) + ", triggering generation");
            
            // Trigger generation using the public method  (broken)
            // juce::MessageManager::callAsync([this]() {
            //     if (activeTrackIndex < static_cast<int>(tracks.size()))
            //     {
            //         tracks[activeTrackIndex]->triggerGeneration();
            //     }
            // });

            
            tracks[activeTrackIndex]->repaint();
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

void MainComponent::showVizWindow()
{
    if (vizWindow == nullptr)
    {
        int numTracks = static_cast<int>(tracks.size());
        // Convert unique_ptr vector to pointer vector for TokenVisualizerWindow
        std::vector<WhAM::LooperTrack*> trackPointers;
        trackPointers.reserve(tracks.size());
        for (auto& track : tracks)
        {
            trackPointers.push_back(track.get());
        }
        vizWindow = std::make_unique<TokenVisualizerWindow>(looperEngine, numTracks, trackPointers);
    }
    
    vizWindow->setVisible(true);
    vizWindow->toFront(true);
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

void MainComponent::showOverflowMenu()
{
    DBG("showOverflowMenu called");
    
    // Get the control area bounds to check which buttons should be visible
    auto bounds = getLocalBounds().reduced(10);
    bounds.removeFromTop(40 + 10); // Title + spacing
    auto controlArea = bounds.removeFromTop(40);
    int controlAreaRight = controlArea.getRight();
    
    // Define all buttons with their actions
    struct ButtonMenuItem {
        juce::TextButton* button;
        juce::String label;
        int menuId;
    };
    
    std::vector<ButtonMenuItem> allButtons = {
        {&syncButton, "sync all", 1},
        {&gradioSettingsButton, "gradio settings", 2},
        {&midiSettingsButton, "midi settings", 3},
        {&clickSynthButton, "click synth", 4},
        {&samplerButton, "sampler", 5},
        {&vizButton, "viz", 6},
        {&saveConfigButton, "save config", 7},
        {&loadConfigButton, "load config", 8}
    };
    
    // Add buttons to menu if they're outside the visible control area
    // Check bounds instead of isVisible() since visibility might be reset
    int hiddenCount = 0;
    juce::PopupMenu menu;
    DBG("[showOverflowMenu] Checking button bounds vs controlAreaRight=" + juce::String(controlAreaRight));
    
    // Calculate which buttons should be visible based on the same logic as resized()
    const int buttonSpacing = 10;
    const int overflowButtonWidth = 60;
    int availableWidth = controlArea.getWidth();
    
    // Calculate visible button count (same logic as resized())
    int visibleButtonCount = 0;
    int usedWidth = 0;
    std::vector<int> buttonWidths = {120, 180, 120, 120, 120, 120, 130, 130};
    
    // First pass: how many fit without overflow
    for (size_t i = 0; i < buttonWidths.size(); ++i)
    {
        int spacing = (visibleButtonCount > 0) ? buttonSpacing : 0;
        int widthNeeded = usedWidth + spacing + buttonWidths[i];
        if (widthNeeded <= availableWidth)
        {
            usedWidth = widthNeeded;
            visibleButtonCount++;
        }
        else
        {
            break;
        }
    }
    
    // If overflow, recalculate with overflow space
    if (visibleButtonCount < static_cast<int>(buttonWidths.size()))
    {
        int overflowSpace = buttonSpacing + overflowButtonWidth;
        visibleButtonCount = 0;
        usedWidth = 0;
        for (size_t i = 0; i < buttonWidths.size(); ++i)
        {
            int spacing = (visibleButtonCount > 0) ? buttonSpacing : 0;
            int widthNeeded = usedWidth + spacing + buttonWidths[i];
            if (widthNeeded + overflowSpace <= availableWidth)
            {
                usedWidth = widthNeeded;
                visibleButtonCount++;
            }
            else
            {
                break;
            }
        }
    }
    
    DBG("[showOverflowMenu] Calculated visibleButtonCount=" + juce::String(visibleButtonCount));
    
    // Add buttons that are beyond the visible count
    for (size_t i = 0; i < allButtons.size(); ++i)
    {
        if (i >= static_cast<size_t>(visibleButtonCount))
        {
            DBG("Adding to menu (hidden): " + allButtons[i].label);
            menu.addItem(allButtons[i].menuId, allButtons[i].label);
            hiddenCount++;
        }
    }
    
    DBG("Hidden buttons count: " + juce::String(hiddenCount));
    
    if (hiddenCount == 0)
    {
        DBG("No hidden buttons to show in menu - hiding overflow button");
        // If overflow button was clicked but there are no hidden buttons,
        // hide the overflow button (this shouldn't happen, but handle it gracefully)
        overflowButton.setVisible(false);
        return;
    }
    
    // Show menu below the overflow button
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&overflowButton),
                       [this](int result) {
                           DBG("Menu result: " + juce::String(result));
                           if (result == 0)
                               return; // Menu dismissed
                           
                           // Handle menu selection
                           switch (result)
                           {
                               case 1: syncButtonClicked(); break;
                               case 2: gradioSettingsButtonClicked(); break;
                               case 3: midiSettingsButtonClicked(); break;
                               case 4: showClickSynthWindow(); break;
                               case 5: showSamplerWindow(); break;
                               case 6: showVizWindow(); break;
                               case 7: saveConfigButtonClicked(); break;
                               case 8: loadConfigButtonClicked(); break;
                           }
                       });
}

void MainComponent::saveConfigButtonClicked()
{
    SessionConfig config = buildSessionConfig();
    juce::FileChooser chooser("Save WhAM config", getConfigDirectory(), "*.json");
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
    juce::FileChooser chooser("Load WhAM config", getConfigDirectory(), "*.json");
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
        applySessionConfig(config, false);
    else
        juce::Logger::writeToLog("Failed to load default WhAM config: " + result.getErrorMessage());
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
                juce::Logger::writeToLog("Config refers to missing track index " + juce::String(trackState.trackIndex));
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

juce::File MainComponent::getConfigDirectory() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("TapeLooper");
    dir.createDirectory();
    return dir;
}

juce::File MainComponent::getDefaultConfigFile() const
{
    return getConfigDirectory().getChildFile("wham_default_config.json");
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

