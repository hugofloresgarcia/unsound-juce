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

MainComponent::MainComponent(int numTracks)
    : syncButton("sync all"),
      audioSettingsButton("audio settings"),
      titleLabel("Title", "tape looper")
{
    DBG_SEGFAULT("ENTRY: MainComponent::MainComponent, numTracks=" + juce::String(numTracks));
    // Apply custom look and feel
    DBG_SEGFAULT("Setting look and feel");
    setLookAndFeel(&customLookAndFeel);

    // Create looper tracks (limit to available engines, max 4 for now)
    DBG_SEGFAULT("Creating tracks, numTracks=" + juce::String(numTracks));
    int actualNumTracks = juce::jmin(numTracks, looperEngine.getNumTracks());
    DBG_SEGFAULT("actualNumTracks=" + juce::String(actualNumTracks) + " (limited by engine max=" + juce::String(looperEngine.getNumTracks()) + ")");
    for (int i = 0; i < actualNumTracks; ++i)
    {
        DBG_SEGFAULT("Creating LooperTrack " + juce::String(i));
        tracks.push_back(std::make_unique<LooperTrack>(looperEngine, i));
        DBG_SEGFAULT("Adding LooperTrack " + juce::String(i) + " to view");
        addAndMakeVisible(tracks[i].get());
    }
    DBG_SEGFAULT("All tracks created");
    
    // Set size based on number of tracks
    // Each track has a fixed width, and window adjusts to fit all tracks
    DBG_SEGFAULT("Setting size");
    const int fixedTrackWidth = 220;  // Fixed width per track
    const int trackSpacing = 5;       // Space between tracks
    const int horizontalMargin = 20;  // Left + right margins
    const int topControlsHeight = 40 + 10 + 40 + 10; // Title + spacing + buttons + spacing
    const int fixedTrackHeight = 650; // Increased height for better layout
    const int verticalMargin = 20;    // Top + bottom margins
    
    int windowWidth = (fixedTrackWidth * actualNumTracks) + (trackSpacing * (actualNumTracks - 1)) + horizontalMargin;
    int windowHeight = topControlsHeight + fixedTrackHeight + verticalMargin;
    
    setSize(windowWidth, windowHeight);

    // Setup sync button
    syncButton.onClick = [this] { syncButtonClicked(); };
    addAndMakeVisible(syncButton);

    // Setup audio settings button
    audioSettingsButton.onClick = [this] { audioSettingsButtonClicked(); };
    addAndMakeVisible(audioSettingsButton);

    // Setup title label
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions()
                                  .withName(juce::Font::getDefaultMonospacedFontName())
                                  .withHeight(20.0f))); // Monospaced, slightly smaller, no bold
    addAndMakeVisible(titleLabel);

    // Note: Audio processing will be started by MainWindow after setup is complete

    // Start timer to update UI
    startTimer(50); // Update every 50ms
}

MainComponent::~MainComponent()
{
    stopTimer();
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

    // Control buttons (no Gradio settings button)
    auto controlArea = bounds.removeFromTop(40);
    syncButton.setBounds(controlArea.removeFromLeft(120));
    controlArea.removeFromLeft(10);
    audioSettingsButton.setBounds(controlArea.removeFromLeft(150));
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
}

void MainComponent::timerCallback()
{
    // Repaint tracks to show recording/playing state
    for (auto& track : tracks)
    {
        track->repaint();
    }
}

void MainComponent::syncButtonClicked()
{
    looperEngine.syncAllTracks();
}

void MainComponent::audioSettingsButtonClicked()
{
    showAudioSettings();
}

void MainComponent::showAudioSettings()
{
    if (audioSettingsWindow != nullptr)
    {
        audioSettingsWindow->toFront(true);
        return;
    }

    auto* audioSettingsComponent = new juce::AudioDeviceSelectorComponent(
        looperEngine.getAudioDeviceManager(),
        0, 256, 0, 256, true, true, true, false);

    audioSettingsComponent->setSize(500, 400);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(audioSettingsComponent);
    options.dialogTitle = "audio settings";
    options.dialogBackgroundColour = juce::Colours::black;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;

    audioSettingsWindow = options.launchAsync();
    
    if (audioSettingsWindow != nullptr)
    {
        audioSettingsWindow->enterModalState(true, nullptr, true);
    }
}

