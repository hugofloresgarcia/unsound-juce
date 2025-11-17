#include "MainComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include <flowerjuce/Components/SettingsDialog.h>

using namespace VampNet;

// TODO: Remove this debug macro after fixing segmentation fault
#define DEBUG_SEGFAULT 1
#if DEBUG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#else
#define DBG_SEGFAULT(msg)
#endif

MainComponent::MainComponent(int numTracks, const juce::String& pannerType)
    : syncButton("sync all"),
      settingsButton("settings"),
      clickSynthButton("click synth"),
      samplerButton("sampler"),
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
    int actualNumTracks = juce::jmin(numTracks, looperEngine.get_num_tracks());
    DBG_SEGFAULT("actualNumTracks=" + juce::String(actualNumTracks) + " (limited by engine max=" + juce::String(looperEngine.get_num_tracks()) + ")");
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
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_vampnet.xml");
    if (midiMappingsFile.existsAsFile())
        midiLearnManager.loadMappings(midiMappingsFile);
    
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

    // Setup settings button
    settingsButton.onClick = [this] { settingsButtonClicked(); };
    addAndMakeVisible(settingsButton);
    
    // Create settings dialog
    settingsDialog = std::make_unique<Shared::SettingsDialog>(
        0.0, // No panner smoothing for VampNet
        nullptr, // No smoothing callback
        gradioUrl,
        [this](const juce::String& newUrl) {
            setGradioUrl(newUrl);
        },
        &midiLearnManager
    );
    
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

    // Title at top
    titleLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(10);

    // Control buttons
    auto controlArea = bounds.removeFromTop(40);
    syncButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    settingsButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    clickSynthButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    samplerButton.setBounds(controlArea.removeFromLeft(120));
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

void MainComponent::settingsButtonClicked()
{
    showSettings();
}

void MainComponent::showSettings()
{
    if (settingsDialog != nullptr)
    {
        settingsDialog->updateGradioUrl(getGradioUrl());
        settingsDialog->refreshMidiInfo();
        settingsDialog->setVisible(true);
        settingsDialog->toFront(true);
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
                auto& trackEngine = looperEngine.get_track_engine(selectedTrack);
                if (trackEngine.get_sampler().hasSample())
                {
                    trackEngine.get_sampler().trigger();
                    
                    auto& track = looperEngine.get_track(selectedTrack);
                    if (!track.m_write_head.get_record_enable())
                    {
                        track.m_write_head.set_record_enable(true);
                        tracks[selectedTrack]->repaint();
                    }
                }
            }
            else if (selectedTrack == -1)
            {
                // All tracks - trigger sampler on all tracks
                for (size_t i = 0; i < tracks.size(); ++i)
                {
                    auto& trackEngine = looperEngine.get_track_engine(static_cast<int>(i));
                    if (trackEngine.get_sampler().hasSample())
                    {
                        trackEngine.get_sampler().trigger();
                        
                        auto& track = looperEngine.get_track(static_cast<int>(i));
                        if (!track.m_write_head.get_record_enable())
                        {
                            track.m_write_head.set_record_enable(true);
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
                auto& trackEngine = looperEngine.get_track_engine(selectedTrack);
                trackEngine.get_click_synth().triggerClick();
                
                auto& track = looperEngine.get_track(selectedTrack);
                if (!track.m_write_head.get_record_enable())
                {
                    track.m_write_head.set_record_enable(true);
                    tracks[selectedTrack]->repaint();
                }
            }
            else if (selectedTrack == -1)
            {
                // All tracks - trigger click on all tracks
                for (size_t i = 0; i < tracks.size(); ++i)
                {
                    auto& trackEngine = looperEngine.get_track_engine(static_cast<int>(i));
                    trackEngine.get_click_synth().triggerClick();
                    
                    auto& track = looperEngine.get_track(static_cast<int>(i));
                    if (!track.m_write_head.get_record_enable())
                    {
                        track.m_write_head.set_record_enable(true);
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


