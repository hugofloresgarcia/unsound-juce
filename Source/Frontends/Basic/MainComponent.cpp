#include "MainComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>

using namespace Basic;

// TODO: Remove this debug macro after fixing segmentation fault
#define DEBUG_SEGFAULT 1
#if DEBUG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#else
#define DBG_SEGFAULT(msg)
#endif

MainComponent::MainComponent(int numTracks, const juce::String& pannerType)
    : syncButton("sync all"),
      midiSettingsButton("midi settings"),
      titleLabel("Title", "tape looper"),
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
    for (int i = 0; i < actualNumTracks; ++i)
    {
        DBG_SEGFAULT("Creating LooperTrack " + juce::String(i));
        tracks.push_back(std::make_unique<LooperTrack>(looperEngine, i, &midiLearnManager, pannerType));
        DBG_SEGFAULT("Adding LooperTrack " + juce::String(i) + " to view");
        addAndMakeVisible(tracks[i].get());
    }
    DBG_SEGFAULT("All tracks created");
    
    // Load MIDI mappings AFTER tracks are created (so parameters are registered)
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings.xml");
    if (midiMappingsFile.existsAsFile())
        midiLearnManager.loadMappings(midiMappingsFile);
    
    // Set size based on number of tracks
    // Each track has a fixed width, and window adjusts to fit all tracks
    DBG_SEGFAULT("Setting size");
    const int fixedTrackWidth = 220;  // Fixed width per track
    const int trackSpacing = 5;       // Space between tracks
    const int horizontalMargin = 20;  // Left + right margins
    const int topControlsHeight = 40 + 10 + 40 + 10; // Title + spacing + buttons + spacing
    const int fixedTrackHeight = 720; // Height adjusted for panner (was 650, added 70 for panner)
    const int verticalMargin = 20;    // Top + bottom margins
    
    int windowWidth = (fixedTrackWidth * actualNumTracks) + (trackSpacing * (actualNumTracks - 1)) + horizontalMargin;
    int windowHeight = topControlsHeight + fixedTrackHeight + verticalMargin;
    
    setSize(windowWidth, windowHeight);

    // Setup sync button
    syncButton.onClick = [this] { syncButtonClicked(); };
    addAndMakeVisible(syncButton);
    
    // Setup MIDI settings button
    midiSettingsButton.onClick = [this] { midiSettingsButtonClicked(); };
    addAndMakeVisible(midiSettingsButton);

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
    
    // Save MIDI mappings
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    appDataDir.createDirectory();
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings.xml");
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
    midiSettingsButton.setBounds(controlArea.removeFromLeft(120));
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

void MainComponent::updateAllChannelSelectors()
{
    for (auto& track : tracks)
    {
        track->updateChannelSelectors();
    }
}

void MainComponent::midiSettingsButtonClicked()
{
    showMidiSettings();
}

void MainComponent::showMidiSettings()
{
    auto devices = midiLearnManager.getAvailableMidiDevices();
    
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "MIDI Learn",
        "MIDI Learn is enabled!\n\n"
        "How to use:\n"
        "1. Right-click any control (transport, level, knobs)\n"
        "2. Select 'MIDI Learn...' from the menu\n"
        "3. Move a MIDI controller to assign it\n"
        "   (or click/press ESC to cancel)\n\n"
        "Available MIDI devices:\n" + 
        (devices.isEmpty() ? "  (none)" : "  " + devices.joinIntoString("\n  ")) + "\n\n"
        "Current mappings: " + juce::String(midiLearnManager.getAllMappings().size()),
        "OK"
    );
}

