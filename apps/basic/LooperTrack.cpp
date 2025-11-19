#include "LooperTrack.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace Basic;

LooperTrack::LooperTrack(MultiTrackLooperEngine& engine, int index, Shared::MidiLearnManager* midiManager, const juce::String& pannerType)
    : looperEngine(engine), 
      trackIndex(index),
      waveformDisplay(engine, index),
      transportControls(midiManager, "track" + juce::String(index)),
      parameterKnobs(midiManager, "track" + juce::String(index)),
      levelControl(engine, index, midiManager, "track" + juce::String(index)),
      inputSelector(),
      trackLabel("Track", "track " + juce::String(index + 1)),
      resetButton("x"),
      pannerType(pannerType),
      stereoPanSlider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox),
      panLabel("pan", "pan"),
      panCoordLabel("coord", "0.50, 0.50")
{
    // Setup track label
    trackLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(trackLabel);
    
    // Setup pan label
    panLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(panLabel);
    
    // Setup pan coordinate label
    panCoordLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(panCoordLabel);
    
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
            looperEngine.get_track_engine(trackIndex).set_speed(static_cast<float>(value));
        },
        ""  // parameterId - will be auto-generated
    });
    
    parameterKnobs.addKnob({
        "overdub",
        0.0, 1.0, 0.5, 0.01,
        "",
        [this](double value) {
            looperEngine.get_track_engine(trackIndex).set_overdub_mix(static_cast<float>(value));
        },
        ""  // parameterId - will be auto-generated
    });
    addAndMakeVisible(parameterKnobs);
    
    // Setup level control
    levelControl.onLevelChange = [this](double value) {
        looperEngine.get_track_engine(trackIndex).set_level_db(static_cast<float>(value));
    };
    addAndMakeVisible(levelControl);
    
    // Setup input selector
    inputSelector.onChannelChange = [this](int channel) {
        looperEngine.get_track_engine(trackIndex).set_input_channel(channel);
    };
    addAndMakeVisible(inputSelector);
    
    // Initialize channel selector (will show "all" if device not ready yet)
    // It will be updated again after device is initialized via updateChannelSelectors()
    inputSelector.updateChannels(looperEngine.get_audio_device_manager());
    
    // Setup panner based on type
    auto pannerTypeLower = pannerType.toLowerCase();
    if (pannerTypeLower == "stereo")
    {
        panner = std::make_unique<StereoPanner>();
        stereoPanSlider.setRange(0.0, 1.0, 0.01);
        stereoPanSlider.setValue(0.5); // Center
        stereoPanSlider.onValueChange = [this] {
            if (auto* stereoPanner = dynamic_cast<StereoPanner*>(panner.get()))
            {
                float panValue = static_cast<float>(stereoPanSlider.getValue());
                stereoPanner->set_pan(panValue);
                panCoordLabel.setText(juce::String(panValue, 2), juce::dontSendNotification);
            }
        };
        addAndMakeVisible(stereoPanSlider);
    }
    else if (pannerTypeLower == "quad")
    {
        panner = std::make_unique<QuadPanner>();
        panner2DComponent = std::make_unique<Panner2DComponent>();
        panner2DComponent->set_pan_position(0.5f, 0.5f); // Center
        panner2DComponent->m_on_pan_change = [this](float x, float y) {
            if (auto* quadPanner = dynamic_cast<QuadPanner*>(panner.get()))
            {
                quadPanner->set_pan(x, y);
                panCoordLabel.setText(juce::String(x, 2) + ", " + juce::String(y, 2), juce::dontSendNotification);
            }
        };
        addAndMakeVisible(panner2DComponent.get());
    }
    else if (pannerTypeLower == "cleat")
    {
        panner = std::make_unique<CLEATPanner>();
        if (auto* cleatPanner = dynamic_cast<CLEATPanner*>(panner.get()))
        {
            // Prepare panner with default sample rate (will be updated when audio device starts)
            cleatPanner->prepare(44100.0);
        }
        panner2DComponent = std::make_unique<Panner2DComponent>();
        panner2DComponent->set_pan_position(0.5f, 0.5f); // Center
        panner2DComponent->m_on_pan_change = [this](float x, float y) {
            if (auto* cleatPanner = dynamic_cast<CLEATPanner*>(panner.get()))
            {
                cleatPanner->set_pan(x, y);
                panCoordLabel.setText(juce::String(x, 2) + ", " + juce::String(y, 2), juce::dontSendNotification);
            }
        };
        addAndMakeVisible(panner2DComponent.get());
    }
    
    // Connect panner to engine for audio processing
    if (panner != nullptr)
    {
        looperEngine.get_track_engine(trackIndex).set_panner(panner.get());
    }
    
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
    auto& track = looperEngine.get_track_engine(trackIndex);
    
    // Background - pitch black
    g.fillAll(juce::Colours::black);

    // Border - use teal color
    g.setColour(juce::Colour(0xff1eb19d));
    g.drawRect(getLocalBounds(), 1);

    // Visual indicator for recording/playing
    if (track.get_record_enable())
    {
        g.setColour(juce::Colour(0xfff04e36).withAlpha(0.2f)); // Red-orange
        g.fillRect(getLocalBounds());
    }
    else if (track.get_playing() && track.has_recorded())
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
    const int channelSelectorHeight = 30;
    const int knobAreaHeight = 140;
    const int controlsHeight = 160;
    
    const int labelHeight = 15;
    const int pannerHeight = 150; // 2D panner height
    const int totalBottomHeight = buttonHeight + spacingSmall + 
                                   labelHeight + spacingSmall +
                                   pannerHeight + spacingSmall +
                                   channelSelectorHeight + spacingSmall + 
                                   knobAreaHeight + spacingSmall + 
                                   controlsHeight;
    
    auto bounds = getLocalBounds().reduced(componentMargin);
    
    // Track label at top with reset button in top right corner
    auto trackLabelArea = bounds.removeFromTop(trackLabelHeight);
    resetButton.setBounds(trackLabelArea.removeFromRight(resetButtonSize));
    trackLabelArea.removeFromRight(spacingSmall);
    trackLabel.setBounds(trackLabelArea);
    bounds.removeFromTop(spacingSmall);
    
    // Input selector
    auto channelSelectorArea = bounds.removeFromTop(channelSelectorHeight);
    inputSelector.setBounds(channelSelectorArea);
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
    auto buttonArea = bottomArea.removeFromBottom(buttonHeight);
    transportControls.setBounds(buttonArea);
    bottomArea.removeFromTop(spacingSmall);
    
    // Panner UI (below transport controls)
    if (panner != nullptr)
    {
        auto panLabelArea = bottomArea.removeFromTop(labelHeight);
        panLabel.setBounds(panLabelArea.removeFromLeft(50)); // "pan" label on left
        panCoordLabel.setBounds(panLabelArea); // Coordinates on right
        bottomArea.removeFromTop(spacingSmall);
        
        auto pannerArea = bottomArea.removeFromTop(pannerHeight);
        if (pannerType.toLowerCase() == "stereo" && stereoPanSlider.isVisible())
        {
            stereoPanSlider.setBounds(pannerArea);
        }
        else if (panner2DComponent != nullptr && panner2DComponent->isVisible())
        {
            panner2DComponent->setBounds(pannerArea);
        }
    }
}

void LooperTrack::recordEnableButtonToggled(bool enabled)
{
    auto& track = looperEngine.get_track_engine(trackIndex);
    track.set_record_enable(enabled);
    repaint();
}

void LooperTrack::playButtonClicked(bool shouldPlay)
{
    auto& track = looperEngine.get_track_engine(trackIndex);
    
    if (shouldPlay)
    {
        track.set_playing(true);
        
        if (track.get_record_enable() && !track.has_recorded())
        {
            track.clear_buffer();
            track.reset();
        }
    }
    else
    {
        track.set_playing(false);
        if (track.get_record_enable())
        {
            track.finalize_recording(track.get_write_pos());
            juce::Logger::writeToLog("~~~ Playback just stopped, finalized recording");
        }
    }
    
    repaint();
}

void LooperTrack::muteButtonToggled(bool muted)
{
    auto& track = looperEngine.get_track_engine(trackIndex);
    track.set_muted(muted);
}

void LooperTrack::resetButtonClicked()
{
    auto& track = looperEngine.get_track_engine(trackIndex);
    
    // Stop playback
    track.set_playing(false);
    track.set_playing(false);
    transportControls.setPlayState(false);
    
    // Disable recording
    track.set_record_enable(false);
    transportControls.setRecordState(false);
    
    // Clear buffer
    const juce::ScopedLock sl(track.get_buffer_lock());
    track.clear_buffer();
    track.reset();
    track.reset();
    
    // Reset controls to defaults
    parameterKnobs.setKnobValue(0, 1.0, juce::dontSendNotification); // speed
    track.set_speed(1.0f);
    
    parameterKnobs.setKnobValue(1, 0.5, juce::dontSendNotification); // overdub
    track.set_overdub_mix(0.5f);
    
    levelControl.setLevelValue(0.0, juce::dontSendNotification);
    track.set_level_db(0.0f);
    
    // Unmute
    track.set_muted(false);
    transportControls.setMuteState(false);
    
    repaint();
}

LooperTrack::~LooperTrack()
{
    stopTimer();
}

void LooperTrack::set_playback_speed(float speed)
{
    parameterKnobs.setKnobValue(0, speed, juce::dontSendNotification);
    looperEngine.get_track_engine(trackIndex).set_speed(speed);
}

float LooperTrack::get_playback_speed() const
{
    return static_cast<float>(parameterKnobs.getKnobValue(0));
}

void LooperTrack::timerCallback()
{
    // Sync button states with model state
    auto& track = looperEngine.get_track_engine(trackIndex);
    
    bool model_record_enable = track.get_record_enable();
    transportControls.setRecordState(model_record_enable);
    
    bool model_is_playing = track.get_playing();
    transportControls.setPlayState(model_is_playing);
    
    // Update displays
    waveformDisplay.repaint();
    levelControl.repaint();
    repaint();
}

void LooperTrack::updateChannelSelectors()
{
    // Update channel selector based on current audio device
    inputSelector.updateChannels(looperEngine.get_audio_device_manager());
}
