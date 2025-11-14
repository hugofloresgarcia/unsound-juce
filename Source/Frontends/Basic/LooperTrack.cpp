#include "LooperTrack.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace Basic;

LooperTrack::LooperTrack(MultiTrackLooperEngine& engine, int index, Shared::MidiLearnManager* midiManager)
    : looperEngine(engine), 
      trackIndex(index),
      waveformDisplay(engine, index),
      transportControls(midiManager, "track" + juce::String(index)),
      parameterKnobs(midiManager, "track" + juce::String(index)),
      levelControl(engine, index, midiManager, "track" + juce::String(index)),
      outputSelector(),
      trackLabel("Track", "track " + juce::String(index + 1)),
      resetButton("x")
{
    // Setup track label
    trackLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(trackLabel);
    
    // Setup reset button
    resetButton.onClick = [this] { resetButtonClicked(); };
    addAndMakeVisible(resetButton);
    
    // Setup waveform display
    addAndMakeVisible(waveformDisplay);
    
    // Setup transport controls
    transportControls.onRecordToggle = [this](bool enabled) { recordEnableButtonToggled(enabled); };
    transportControls.onPlayToggle = [this](bool shouldPlay) { playButtonClicked(shouldPlay); };
    transportControls.onMuteToggle = [this](bool muted) { muteButtonToggled(muted); };
    transportControls.onReset = [this]() { resetButtonClicked(); };
    addAndMakeVisible(transportControls);
    
    // Setup parameter knobs (speed and overdub)
    parameterKnobs.addKnob({
        "speed",
        0.25, 4.0, 1.0, 0.01,
        "x",
        [this](double value) {
            looperEngine.getTrack(trackIndex).readHead.setSpeed(static_cast<float>(value));
        },
        ""  // parameterId - will be auto-generated
    });
    
    parameterKnobs.addKnob({
        "overdub",
        0.0, 1.0, 0.5, 0.01,
        "",
        [this](double value) {
            looperEngine.getTrack(trackIndex).writeHead.setOverdubMix(static_cast<float>(value));
        },
        ""  // parameterId - will be auto-generated
    });
    addAndMakeVisible(parameterKnobs);
    
    // Setup level control
    levelControl.onLevelChange = [this](double value) {
        looperEngine.getTrack(trackIndex).readHead.setLevelDb(static_cast<float>(value));
    };
    addAndMakeVisible(levelControl);
    
    // Setup output selector
    outputSelector.onChannelChange = [this](int channel) {
        looperEngine.getTrack(trackIndex).readHead.setOutputChannel(channel);
    };
    addAndMakeVisible(outputSelector);
    
    // Apply custom look and feel to all child components
    applyLookAndFeel();
    
    // Start timer for VU meter updates (30Hz)
    startTimer(33);
}

void LooperTrack::applyLookAndFeel()
{
    // Get the parent's look and feel (should be CustomLookAndFeel from MainComponent)
    if (auto* parent = getParentComponent())
    {
        juce::LookAndFeel& laf = parent->getLookAndFeel();
        trackLabel.setLookAndFeel(&laf);
        resetButton.setLookAndFeel(&laf);
        // Note: shared components will get look and feel from their own children
    }
}

void LooperTrack::paint(juce::Graphics& g)
{
    auto& track = looperEngine.getTrack(trackIndex);
    
    // Background - pitch black
    g.fillAll(juce::Colours::black);

    // Border - use teal color
    g.setColour(juce::Colour(0xff1eb19d));
    g.drawRect(getLocalBounds(), 1);

    // Visual indicator for recording/playing
    if (track.writeHead.getRecordEnable())
    {
        g.setColour(juce::Colour(0xfff04e36).withAlpha(0.2f)); // Red-orange
        g.fillRect(getLocalBounds());
    }
    else if (track.isPlaying.load() && track.tapeLoop.hasRecorded.load())
    {
        g.setColour(juce::Colour(0xff1eb19d).withAlpha(0.15f)); // Teal
        g.fillRect(getLocalBounds());
    }
}

void LooperTrack::resized()
{
    // Layout constants
    const int componentMargin = 5;
    const int trackLabelHeight = 20;
    const int resetButtonSize = 20;
    const int spacingSmall = 5;
    const int buttonHeight = 30;
    const int outputSelectorHeight = 30;
    const int knobAreaHeight = 140;
    const int controlsHeight = 160;
    
    const int totalBottomHeight = buttonHeight + spacingSmall + 
                                   outputSelectorHeight + spacingSmall + 
                                   knobAreaHeight + spacingSmall + 
                                   controlsHeight;
    
    auto bounds = getLocalBounds().reduced(componentMargin);
    
    // Track label at top with reset button in top right corner
    auto trackLabelArea = bounds.removeFromTop(trackLabelHeight);
    resetButton.setBounds(trackLabelArea.removeFromRight(resetButtonSize));
    trackLabelArea.removeFromRight(spacingSmall);
    trackLabel.setBounds(trackLabelArea);
    bounds.removeFromTop(spacingSmall);
    
    // Reserve space for controls at bottom
    auto bottomArea = bounds.removeFromBottom(totalBottomHeight);
    
    // Waveform area is now the remaining space
    waveformDisplay.setBounds(bounds);
    
    // Knobs area
    auto knobArea = bottomArea.removeFromTop(knobAreaHeight);
    parameterKnobs.setBounds(knobArea);
    bottomArea.removeFromTop(spacingSmall);
    
    // Level control and VU meter
    auto controlsArea = bottomArea.removeFromTop(controlsHeight);
    levelControl.setBounds(controlsArea.removeFromLeft(115)); // 80 + 5 + 30
    controlsArea.removeFromLeft(spacingSmall);
    
    // Mute button would go here, but it's now part of transport controls
    // So we skip that space
    bottomArea.removeFromTop(spacingSmall);
    
    // Transport buttons
    auto buttonArea = bottomArea.removeFromBottom(buttonHeight + spacingSmall + outputSelectorHeight);
    auto outputArea = buttonArea.removeFromBottom(outputSelectorHeight);
    buttonArea.removeFromBottom(spacingSmall);
    
    transportControls.setBounds(buttonArea);
    
    // Output channel selector
    outputSelector.setBounds(outputArea);
}

void LooperTrack::recordEnableButtonToggled(bool enabled)
{
    auto& track = looperEngine.getTrack(trackIndex);
    track.writeHead.setRecordEnable(enabled);
    repaint();
}

void LooperTrack::playButtonClicked(bool shouldPlay)
{
    auto& track = looperEngine.getTrack(trackIndex);
    
    if (shouldPlay)
    {
        track.isPlaying.store(true);
        track.readHead.setPlaying(true);
        
        if (track.writeHead.getRecordEnable() && !track.tapeLoop.hasRecorded.load())
        {
            const juce::ScopedLock sl(track.tapeLoop.lock);
            track.tapeLoop.clearBuffer();
            track.writeHead.reset();
            track.readHead.reset();
        }
    }
    else
    {
        track.isPlaying.store(false);
        track.readHead.setPlaying(false);
        if (track.writeHead.getRecordEnable())
        {
            track.writeHead.finalizeRecording(track.writeHead.getPos());
            juce::Logger::writeToLog("~~~ Playback just stopped, finalized recording");
        }
    }
    
    repaint();
}

void LooperTrack::muteButtonToggled(bool muted)
{
    auto& track = looperEngine.getTrack(trackIndex);
    track.readHead.setMuted(muted);
}

void LooperTrack::resetButtonClicked()
{
    auto& track = looperEngine.getTrack(trackIndex);
    
    // Stop playback
    track.isPlaying.store(false);
    track.readHead.setPlaying(false);
    transportControls.setPlayState(false);
    
    // Disable recording
    track.writeHead.setRecordEnable(false);
    transportControls.setRecordState(false);
    
    // Clear the tape loop buffer
    const juce::ScopedLock sl(track.tapeLoop.lock);
    track.tapeLoop.clearBuffer();
    track.writeHead.reset();
    track.readHead.reset();
    
    // Reset controls to defaults
    parameterKnobs.setKnobValue(0, 1.0, juce::dontSendNotification); // speed
    track.readHead.setSpeed(1.0f);
    
    parameterKnobs.setKnobValue(1, 0.5, juce::dontSendNotification); // overdub
    track.writeHead.setOverdubMix(0.5f);
    
    levelControl.setLevelValue(0.0, juce::dontSendNotification);
    track.readHead.setLevelDb(0.0f);
    
    // Unmute
    track.readHead.setMuted(false);
    transportControls.setMuteState(false);
    
    // Reset output channel to all
    outputSelector.setSelectedChannel(1, juce::dontSendNotification);
    track.readHead.setOutputChannel(-1);
    
    repaint();
}

LooperTrack::~LooperTrack()
{
    stopTimer();
}

void LooperTrack::setPlaybackSpeed(float speed)
{
    parameterKnobs.setKnobValue(0, speed, juce::dontSendNotification);
    looperEngine.getTrack(trackIndex).readHead.setSpeed(speed);
}

float LooperTrack::getPlaybackSpeed() const
{
    return static_cast<float>(parameterKnobs.getKnobValue(0));
}

void LooperTrack::timerCallback()
{
    // Sync button states with model state
    auto& track = looperEngine.getTrack(trackIndex);
    
    bool modelRecordEnable = track.writeHead.getRecordEnable();
    transportControls.setRecordState(modelRecordEnable);
    
    bool modelIsPlaying = track.isPlaying.load();
    transportControls.setPlayState(modelIsPlaying);
    
    // Update displays
    waveformDisplay.repaint();
    levelControl.repaint();
    repaint();
}
