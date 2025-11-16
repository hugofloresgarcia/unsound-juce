#include "MainComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
// Ensure VampNet types are fully defined (they're forward declared in VampNetTrackEngine.h)
#include "../VampNet/ClickSynth.h"
#include "../VampNet/Sampler.h"

using namespace WhAM;

// TODO: Remove this debug macro after fixing segmentation fault
#define DEBUG_SEGFAULT 1
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
    std::function<juce::String()> gradioUrlProvider = [this]() { return getGradioUrl(); };
    for (int i = 0; i < actualNumTracks; ++i)
    {
        DBG_SEGFAULT("Creating LooperTrack " + juce::String(i));
        tracks.push_back(std::make_unique<LooperTrack>(looperEngine, i, gradioUrlProvider, &midiLearnManager, pannerType));
        DBG_SEGFAULT("Adding LooperTrack " + juce::String(i) + " to view");
        addAndMakeVisible(tracks[i].get());
    }
    DBG_SEGFAULT("All tracks created");
    
    // Load MIDI mappings AFTER tracks are created (so parameters are registered)
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_wham.xml");
    if (midiMappingsFile.existsAsFile())
        midiLearnManager.loadMappings(midiMappingsFile);
    
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
    static int resizeCallCount = 0;
    resizeCallCount++;
    DBG("[resized] CALL #" + juce::String(resizeCallCount));
    
    auto bounds = getLocalBounds().reduced(10);

    // Title at top
    titleLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(10);

    // Control buttons with overflow logic
    auto controlArea = bounds.removeFromTop(40);
    const int buttonSpacing = 10;
    const int overflowButtonWidth = 60;
    
    // Define buttons in order with their widths
    struct ButtonInfo {
        juce::TextButton* button;
        int width;
    };
    
    std::vector<ButtonInfo> buttons = {
        {&syncButton, 120},
        {&gradioSettingsButton, 180},
        {&midiSettingsButton, 120},
        {&clickSynthButton, 120},
        {&samplerButton, 120},
        {&vizButton, 120}
    };
    
    // Calculate which buttons fit
    int availableWidth = controlArea.getWidth();
    int usedWidth = 0;
    int visibleButtonCount = 0;
    
    // First pass: determine how many buttons fit WITHOUT overflow button
    // (we'll add overflow button space only if needed)
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        int buttonWidth = buttons[i].width;
        int spacing = (visibleButtonCount > 0) ? buttonSpacing : 0;
        int widthNeeded = usedWidth + spacing + buttonWidth;
        
        // Check if this button fits
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
    
    // If not all buttons fit, we need to reserve space for overflow button
    // and potentially show fewer visible buttons
    bool hasOverflow = visibleButtonCount < static_cast<int>(buttons.size());
    if (hasOverflow)
    {
        // Recalculate: reserve space for overflow button and see how many buttons fit
        int overflowSpace = buttonSpacing + overflowButtonWidth;
        visibleButtonCount = 0;
        usedWidth = 0;
        
        for (size_t i = 0; i < buttons.size(); ++i)
        {
            int buttonWidth = buttons[i].width;
            int spacing = (visibleButtonCount > 0) ? buttonSpacing : 0;
            int widthNeeded = usedWidth + spacing + buttonWidth;
            
            // Check if button fits with overflow space reserved
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
        // Recalculate hasOverflow after reserving space for overflow button
        hasOverflow = visibleButtonCount < static_cast<int>(buttons.size());
    }
    
    DBG("[resized] availableWidth=" + juce::String(availableWidth) + 
        ", visibleButtonCount=" + juce::String(visibleButtonCount) + 
        ", hasOverflow=" + juce::String(hasOverflow ? 1 : 0));
    
    // IMPORTANT: First hide ALL buttons, then show only the ones that should be visible
    // This ensures proper state even if resized() is called multiple times
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        buttons[i].button->setVisible(false);
    }
    
    // Layout visible buttons (positioned within controlArea)
    int xPos = controlArea.getX();
    int yPos = controlArea.getY();
    for (int i = 0; i < visibleButtonCount; ++i)
    {
        if (i > 0)
            xPos += buttonSpacing;
        
        buttons[i].button->setBounds(xPos, yPos, buttons[i].width, controlArea.getHeight());
        buttons[i].button->setVisible(true);
        DBG("[resized] Showing button " + juce::String(i) + " at x=" + juce::String(xPos));
        xPos += buttons[i].width;
    }
    
    // Hide overflow buttons (already hidden above, but be explicit)
    for (size_t i = visibleButtonCount; i < buttons.size(); ++i)
    {
        buttons[i].button->setVisible(false);
        DBG("[resized] Hiding button " + juce::String(i));
    }
    
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
    
    // Force a repaint to ensure visibility changes take effect
    repaint();
    
    // Double-check visibility at the end (debug)
    DBG("[resized] END - Final visibility check:");
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        bool isVisible = buttons[i].button->isVisible();
        DBG("[resized] Button " + juce::String(i) + " final isVisible=" + juce::String(isVisible ? 1 : 0));
    }
    
    bounds.removeFromTop(10);

    // Tracks arranged horizontally with fixed width
    if (!tracks.empty())
    {
        const int fixedTrackWidth = 260;  // Matches constructor
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
            
            // Trigger generation using the public method
            juce::MessageManager::callAsync([this]() {
                if (activeTrackIndex < static_cast<int>(tracks.size()))
                {
                    tracks[activeTrackIndex]->triggerGeneration();
                }
            });
            
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
        {&vizButton, "viz", 6}
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
    std::vector<int> buttonWidths = {120, 180, 120, 120, 120, 120};
    
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
                           }
                       });
}

