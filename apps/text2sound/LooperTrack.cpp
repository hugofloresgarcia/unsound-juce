#include "LooperTrack.h"
#include <flowerjuce/Components/GradioUtilities.h>
#include <flowerjuce/Components/ConfigManager.h>
#include <flowerjuce/Panners/PanningUtils.h>
#include <juce_audio_formats/juce_audio_formats.h>

using namespace Text2Sound;

// GradioWorkerThread implementation
void GradioWorkerThread::run()
{
    // Step 1: Save buffer to file on background thread (if audio exists)
    juce::File tempAudioFile;
    juce::Result saveResult = juce::Result::ok();
    
    // Check if we have audio (audioFile is not empty and not a sentinel)
    bool isSentinel = audioFile.getFileName() == "has_audio";
    
    if (isSentinel)
    {
        // Notify status update: saving to file
        DBG("GradioWorkerThread: Status update - Saving to file...");
        juce::MessageManager::callAsync([this]()
        {
            if (onStatusUpdate)
                onStatusUpdate("Saving to file...");
        });
        
        // Actually save the buffer to file
        DBG("GradioWorkerThread: Saving input audio to file: " + tempAudioFile.getFullPathName());
        saveResult = saveBufferToFile(trackIndex, tempAudioFile);
        DBG("GradioWorkerThread: Save input audio result: " + saveResult.getErrorMessage());

        
        if (saveResult.failed())
        {
            DBG("GradioWorkerThread: Save input audio failed: " + saveResult.getErrorMessage());
            // Notify save failure on message thread
            juce::MessageManager::callAsync([this, saveResult]()
            {
                if (onComplete)
                    onComplete(saveResult, juce::Array<juce::File>(), trackIndex);
            });
            return;
        }
    }
    else
    {
        // No audio - tempAudioFile remains empty, will be treated as null
        tempAudioFile = juce::File();
    }

    // Step 2: Set up Gradio space info
    GradioClient::SpaceInfo spaceInfo;
    const juce::String defaultUrl = "https://opensound-ezaudio-controlnet.hf.space/";
    juce::String configuredUrl = defaultUrl;

    if (gradioUrlProvider)
    {
        juce::String providedUrl = gradioUrlProvider();
        if (providedUrl.isNotEmpty())
            configuredUrl = providedUrl;
    }

    spaceInfo.gradio = configuredUrl;
    gradioClient.setSpaceInfo(spaceInfo);

    // Step 3: Upload file (if we have audio)
    if (isSentinel && tempAudioFile.existsAsFile())
    {
        // Notify status update: uploading
        DBG("GradioWorkerThread: Status update - Uploading...");
        juce::MessageManager::callAsync([this]()
        {
            if (onStatusUpdate)
                onStatusUpdate("Uploading...");
        });
    }

    // Step 4: Process request on background thread (get all variations)
    // Ensure we have valid params (use defaults if customParams is invalid)
    juce::var paramsToUse = customParams.isObject() ? customParams : Text2Sound::LooperTrack::getDefaultText2SoundParams();
    
    juce::Array<juce::File> outputFiles;
    
    // Notify status update: processing
    DBG("GradioWorkerThread: Status update - Processing...");
    juce::MessageManager::callAsync([this]()
    {
        if (onStatusUpdate)
            onStatusUpdate("Processing...");
    });
    
    auto result = gradioClient.processRequestMultiple(tempAudioFile, textPrompt, outputFiles, paramsToUse);

    // Step 5: Download variations (if successful)
    if (!result.failed() && outputFiles.size() > 0)
    {
        // Notify status update: downloading
        juce::MessageManager::callAsync([this, outputFiles]()
        {
            if (onStatusUpdate)
            {
                juce::String statusText = "Downloading variations...";
                if (outputFiles.size() > 1)
                    statusText += " (" + juce::String(outputFiles.size()) + " files)";
                DBG("GradioWorkerThread: Status update - " + statusText);
                onStatusUpdate(statusText);
            }
        });
    }

    // Notify completion on message thread
    juce::MessageManager::callAsync([this, result, outputFiles]()
    {
        if (onComplete)
            onComplete(result, outputFiles, trackIndex);
    });
}

juce::Result GradioWorkerThread::saveBufferToFile(int trackIndex, juce::File& outputFile)
{
    return Shared::saveTrackBufferToWavFile(looperEngine, trackIndex, outputFile, "gradio_input");
}

// LooperTrack implementation
LooperTrack::LooperTrack(MultiTrackLooperEngine& engine, int index, std::function<juce::String()> gradioUrlGetter, Shared::MidiLearnManager* midiManager, const juce::String& pannerType)
    : looperEngine(engine), 
      trackIndex(index),
      waveformDisplay(engine, index),
      transportControls(midiManager, "track" + juce::String(index)),
      parameterKnobs(midiManager, "track" + juce::String(index)),
      levelControl(engine, index, midiManager, "track" + juce::String(index)),
      inputSelector(),
      trackLabel("Track", "track " + juce::String(index + 1)),
      resetButton("x"),
      generateButton("generate"),
      textPromptLabel("TextPrompt", "query"),
      autogenToggle("autogen"),
      gradioUrlProvider(std::move(gradioUrlGetter)),
      midiLearnManager(midiManager),
      trackIdPrefix("track" + juce::String(index)),
      pannerType(pannerType),
      stereoPanSlider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox),
      panLabel("pan", "pan"),
      panCoordLabel("coord", "0.50, 0.50")
{
    // Initialize custom params with defaults (will be updated by MainComponent)
    customText2SoundParams = getDefaultText2SoundParams();
    
    // Initialize variations (allocate TapeLoops for each variation)
    auto& track = looperEngine.get_track(trackIndex);
    double sample_rate = track.m_write_head.get_sample_rate();
    if (sample_rate <= 0.0)
        sample_rate = 44100.0; // Default sample rate
    
    variations.clear();
    for (int i = 0; i < numVariations; ++i)
    {
        auto variation = std::make_unique<TapeLoop>();
        variation->allocate_buffer(sample_rate, 10.0); // 10 second max duration
        variations.push_back(std::move(variation));
    }
    
    // Setup variation selector
    variationSelector.setNumVariations(numVariations);
    variationSelector.setSelectedVariation(0);
    variationSelector.onVariationSelected = [this](int variationIndex) {
        switchToVariation(variationIndex);
    };
    addAndMakeVisible(variationSelector);
    
    
    // Setup track label
    trackLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(trackLabel);
    
    // Setup pan label
    panLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(panLabel);
    
    // Setup pan coordinate label
    panCoordLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(panCoordLabel);
    
    // Setup trajectory toggle button [tr]
    trajectoryToggle.setButtonText("");
    trajectoryToggle.setLookAndFeel(&emptyToggleLookAndFeel);
    trajectoryToggle.onClick = [this] {
        bool isOn = trajectoryToggle.getToggleState();
        if (panner2DComponent != nullptr)
        {
            panner2DComponent->set_trajectory_recording_enabled(isOn);
            trajectoryPlaying.store(panner2DComponent->is_playing()); // Update cached state
            
            // If [tr] is turned on, cancel any pregen path
            if (isOn && pathGeneratorButtons != nullptr)
            {
                pathGeneratorButtons->resetAllButtons();
                panner2DComponent->stop_playback();
            }
        }
    };
    addAndMakeVisible(trajectoryToggle);
    
    // Setup onset toggle button [o]
    onsetToggle.setButtonText("");
    onsetToggle.setLookAndFeel(&emptyToggleLookAndFeel);
    onsetToggle.setToggleState(true, juce::dontSendNotification); // Default to on
    onsetToggle.onClick = [this] {
        bool enabled = onsetToggle.getToggleState();
        onsetToggleEnabled.store(enabled); // Update atomic flag for audio thread
        if (panner2DComponent != nullptr)
        {
            panner2DComponent->set_onset_triggering_enabled(enabled);
            trajectoryPlaying.store(panner2DComponent->is_playing()); // Update cached state
        }
    };
    addAndMakeVisible(onsetToggle);
    
    // Setup save trajectory button [sv~]
    saveTrajectoryButton.setButtonText("[sv~]");
    saveTrajectoryButton.onClick = [this] {
        saveTrajectory();
    };
    addAndMakeVisible(saveTrajectoryButton);
    
    // Initialize onset triggering to enabled (since toggle defaults to on)
    // Note: panner2DComponent will be created later, so we'll set this after it's created
    onsetToggleEnabled.store(true);
    
    // Setup audio sample callback for onset detection
    looperEngine.get_track_engine(trackIndex).set_audio_sample_callback([this](float sample) {
        feedAudioSample(sample);
    });
    
    // Setup reset button
    resetButton.onClick = [this] { resetButtonClicked(); };
    addAndMakeVisible(resetButton);
    
    // Setup generate button
    generateButton.onClick = [this] { generateButtonClicked(); };
    addAndMakeVisible(generateButton);
    
    // Setup MIDI learn for generate button
    if (midiLearnManager)
    {
        generateButtonLearnable = std::make_unique<Shared::MidiLearnable>(*midiLearnManager, trackIdPrefix + "_generate");
        
        // Create mouse listener for right-click handling
        generateButtonMouseListener = std::make_unique<Shared::MidiLearnMouseListener>(*generateButtonLearnable, this);
        generateButton.addMouseListener(generateButtonMouseListener.get(), false);
        
        midiLearnManager->registerParameter({
            trackIdPrefix + "_generate",
            [this](float value) {
                if (value > 0.5f && generateButton.isEnabled())
                    generateButtonClicked();
            },
            [this]() { return 0.0f; },
            trackIdPrefix + " Generate",
            true  // Toggle control
        });
    }
    
    
    // Setup text prompt editor
    textPromptEditor.setMultiLine(false);
    textPromptEditor.setReturnKeyStartsNewLine(false);
    textPromptEditor.setTextToShowWhenEmpty("enter text prompt...", juce::Colours::grey);
    textPromptEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    textPromptEditor.onReturnKey = [this]() {
        // Pressing Enter triggers generate
        if (generateButton.isEnabled())
            generateButtonClicked();
    };
    addAndMakeVisible(textPromptEditor);
    addAndMakeVisible(textPromptLabel);
    
    // Setup waveform display
    addAndMakeVisible(waveformDisplay);
    
    // Setup transport controls (no record button for Text2Sound)
    transportControls.setRecordButtonVisible(false);
    transportControls.onPlayToggle = [this](bool shouldPlay) { playButtonClicked(shouldPlay); };
    transportControls.onMuteToggle = [this](bool muted) { muteButtonToggled(muted); };
    transportControls.onReset = [this]() { resetButtonClicked(); };
    addAndMakeVisible(transportControls);
    
    // Setup parameter knobs (speed and duration)
    parameterKnobs.addKnob({
        "speed",
        0.25, 4.0, 1.0, 0.01,
        "x",
        [this](double value) {
            looperEngine.get_track(trackIndex).m_read_head.set_speed(static_cast<float>(value));
        },
        ""  // parameterId - will be auto-generated
    });
    
    parameterKnobs.addKnob({
        "duration",
        0.0, 8.0, 5.0, 0.01,
        "s",
        [this](double value) {
            auto& track = looperEngine.get_track(trackIndex);
            double sample_rate = track.m_write_head.get_sample_rate();
            if (sample_rate > 0.0)
            {
                // Convert duration (seconds) to samples and set WrapPos
                size_t wrap_pos = static_cast<size_t>(value * sample_rate);
                track.m_write_head.set_wrap_pos(wrap_pos);
                
                // Repaint waveform display to show updated bounds
                waveformDisplay.repaint();
            }
            
            // Update duration parameter for gradio endpoint
            auto* obj = customText2SoundParams.getDynamicObject();
            if (obj != nullptr)
            {
                obj->setProperty("duration", juce::var(value));
            }
        },
        ""  // parameterId - will be auto-generated
    });
    
    // Initialize duration to 5.0 seconds (default value)
    {
        auto& track_init = looperEngine.get_track(trackIndex);
        double sample_rate_init = track_init.m_write_head.get_sample_rate();
        if (sample_rate_init <= 0.0)
            sample_rate_init = 44100.0; // Default sample rate
        
        if (sample_rate_init > 0.0)
        {
            size_t wrap_pos = static_cast<size_t>(5.0 * sample_rate_init);
            track_init.m_write_head.set_wrap_pos(wrap_pos);
        }
        
        // Update duration parameter for gradio endpoint
        auto* obj = customText2SoundParams.getDynamicObject();
        if (obj != nullptr)
        {
            obj->setProperty("duration", juce::var(5.0));
        }
    }
    
    addAndMakeVisible(parameterKnobs);
    
    // Setup level control
    levelControl.onLevelChange = [this](double value) {
        looperEngine.get_track(trackIndex).m_read_head.set_level_db(static_cast<float>(value));
    };
    addAndMakeVisible(levelControl);
    
    // Setup "autogen" toggle
    autogenToggle.setButtonText("autogen");
    autogenToggle.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(autogenToggle);
    
    // Setup input selector
    inputSelector.onChannelChange = [this](int channel) {
        looperEngine.get_track(trackIndex).m_write_head.set_input_channel(channel);
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
            // Update cached trajectory playing state
            if (panner2DComponent != nullptr)
            {
                trajectoryPlaying.store(panner2DComponent->is_playing());
            }
        };
        addAndMakeVisible(panner2DComponent.get());
        
        // Initialize onset triggering now that panner2DComponent is created
        panner2DComponent->set_onset_triggering_enabled(true);
    }
    else if (pannerTypeLower == "cleat")
    {
        panner = std::make_unique<CLEATPanner>();
        panner2DComponent = std::make_unique<Panner2DComponent>();
        panner2DComponent->set_pan_position(0.5f, 0.5f); // Center
        panner2DComponent->m_on_pan_change = [this](float x, float y) {
            if (auto* cleatPanner = dynamic_cast<CLEATPanner*>(panner.get()))
            {
                cleatPanner->set_pan(x, y);
                panCoordLabel.setText(juce::String(x, 2) + ", " + juce::String(y, 2), juce::dontSendNotification);
            }
            // Update cached trajectory playing state
            if (panner2DComponent != nullptr)
            {
                trajectoryPlaying.store(panner2DComponent->is_playing());
            }
        };
        addAndMakeVisible(panner2DComponent.get());
        
        // Initialize onset triggering now that panner2DComponent is created (for cleat)
        panner2DComponent->set_onset_triggering_enabled(true);
    }
    
    // Connect panner to engine for audio processing
    if (panner != nullptr)
    {
        looperEngine.get_track_engine(trackIndex).set_panner(panner.get());
    }
    
    // Setup path generation buttons and knobs for any 2D panner (quad or cleat)
    if (panner2DComponent != nullptr)
    {
        // Setup path generation buttons component
        pathGeneratorButtons = std::make_unique<PathGeneratorButtons>();
        pathGeneratorButtons->onPathButtonToggled = [this](const juce::String& pathType, bool isOn) {
            if (isOn)
            {
                // Cancel trajectory recording if active
                if (trajectoryToggle.getToggleState())
                {
                    trajectoryToggle.setToggleState(false, juce::dontSendNotification);
                    if (panner2DComponent != nullptr)
                    {
                        panner2DComponent->set_trajectory_recording_enabled(false);
                    }
                }
                
                // Generate new path when toggled on
                generatePath(pathType);
            }
            else
            {
                // Stop playback when toggled off
                if (panner2DComponent != nullptr)
                {
                    panner2DComponent->stop_playback();
                }
            }
        };
        addAndMakeVisible(pathGeneratorButtons.get());
        
        // Setup path speed knob (rotary)
        pathSpeedKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        pathSpeedKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pathSpeedKnob.setRange(0.1, 2.0, 0.1);
        pathSpeedKnob.setValue(1.0);
        pathSpeedKnob.setDoubleClickReturnValue(true, 1.0);
        pathSpeedKnob.onValueChange = [this] {
            if (panner2DComponent != nullptr)
            {
                panner2DComponent->set_playback_speed(static_cast<float>(pathSpeedKnob.getValue()));
            }
        };
        addAndMakeVisible(pathSpeedKnob);
        pathSpeedLabel.setText("speed", juce::dontSendNotification);
        pathSpeedLabel.setJustificationType(juce::Justification::centred);
        pathSpeedLabel.setFont(juce::FontOptions(11.0f));
        addAndMakeVisible(pathSpeedLabel);
        
        // Setup path scale knob (rotary)
        pathScaleKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        pathScaleKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pathScaleKnob.setRange(0.0, 2.0, 0.1);
        pathScaleKnob.setValue(1.0);
        pathScaleKnob.setDoubleClickReturnValue(true, 1.0);
        pathScaleKnob.onValueChange = [this] {
            if (panner2DComponent != nullptr)
            {
                panner2DComponent->set_trajectory_scale(static_cast<float>(pathScaleKnob.getValue()));
            }
        };
        addAndMakeVisible(pathScaleKnob);
        pathScaleLabel.setText("scale", juce::dontSendNotification);
        pathScaleLabel.setJustificationType(juce::Justification::centred);
        pathScaleLabel.setFont(juce::FontOptions(11.0f));
        addAndMakeVisible(pathScaleLabel);
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
        generateButton.setLookAndFeel(&laf);
        textPromptEditor.setLookAndFeel(&laf);
        textPromptLabel.setLookAndFeel(&laf);
        autogenToggle.setLookAndFeel(&laf);
    }
}

void LooperTrack::paint(juce::Graphics& g)
{
    auto& track = looperEngine.get_track(trackIndex);
    
    // Background - pitch black
    g.fillAll(juce::Colours::black);

    // Border - use teal color
    g.setColour(juce::Colour(0xff1eb19d));
    g.drawRect(getLocalBounds(), 1);

    // Visual indicator for recording/playing
    if (track.m_write_head.get_record_enable())
    {
        g.setColour(juce::Colour(0xfff04e36).withAlpha(0.2f)); // Red-orange
        g.fillRect(getLocalBounds());
    }
    else if (track.m_is_playing.load() && track.m_tape_loop.m_has_recorded.load())
    {
        g.setColour(juce::Colour(0xff1eb19d).withAlpha(0.15f)); // Teal
        g.fillRect(getLocalBounds());
    }
    
    // Draw MIDI indicator on generate button if mapped
    if (generateButtonLearnable && generateButtonLearnable->hasMidiMapping())
    {
        auto buttonBounds = generateButton.getBounds();
        g.setColour(juce::Colour(0xffed1683));  // Pink
        g.fillEllipse(buttonBounds.getRight() - 8.0f, buttonBounds.getY() + 2.0f, 6.0f, 6.0f);
    }
    
    
    // Draw custom toggle buttons for trajectory and onset
    if (panner2DComponent != nullptr && panner2DComponent->isVisible())
    {
        // Draw [tr] toggle button (orange)
        drawCustomToggleButton(g, trajectoryToggle, "tr", trajectoryToggle.getBounds(),
                              juce::Colour(0xfff36e27), juce::Colour(0xfff36e27), false);
        
        // Draw [o] toggle button (teal)
        drawCustomToggleButton(g, onsetToggle, "o", onsetToggle.getBounds(),
                              juce::Colour(0xff1eb19d), juce::Colour(0xff1eb19d), false);
        
        // Draw onset indicator LED next to [o] button
        if (onsetToggle.isVisible())
        {
            auto ledBounds = onsetToggle.getBounds();
            ledBounds = ledBounds.translated(ledBounds.getWidth() + 3, 0); // Position to the right
            ledBounds.setWidth(8);
            ledBounds.setHeight(8);
            
            // Draw LED background (dark circle)
            g.setColour(juce::Colours::black);
            g.fillEllipse(ledBounds.toFloat());
            
            // Draw LED glow if onset detected
            double currentBrightness = onsetLEDBrightness.load();
            if (currentBrightness > 0.0)
            {
                float brightness = static_cast<float>(currentBrightness);
                juce::Colour ledColor = juce::Colour(0xff00ff00).withAlpha(brightness); // Green LED
                g.setColour(ledColor);
                g.fillEllipse(ledBounds.toFloat());
                
                // Draw outer glow
                g.setColour(ledColor.withAlpha(brightness * 0.3f));
                g.fillEllipse(ledBounds.toFloat().expanded(2.0f));
            }
            
            // Draw LED border
            g.setColour(juce::Colour(0xff1eb19d).withAlpha(0.5f)); // Teal border to match [o] button
            g.drawEllipse(ledBounds.toFloat(), 1.0f);
        }
        
        // Draw knob value labels
        if (pathSpeedKnob.isVisible() && pathSpeedKnob.getWidth() > 0)
        {
            auto knobBounds = pathSpeedKnob.getBounds();
            juce::String speedText = juce::String(pathSpeedKnob.getValue(), 1) + "x";
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(10.0f));
            g.drawText(speedText, knobBounds, juce::Justification::centred);
        }
        
        if (pathScaleKnob.isVisible() && pathScaleKnob.getWidth() > 0)
        {
            auto knobBounds = pathScaleKnob.getBounds();
            juce::String scaleText = juce::String(pathScaleKnob.getValue(), 1);
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(10.0f));
            g.drawText(scaleText, knobBounds, juce::Justification::centred);
        }
    }
}

void LooperTrack::resized()
{
    // Layout constants
    const int componentMargin = 5;
    const int trackLabelHeight = 20;
    const int resetButtonSize = 20;
    const int spacingSmall = 5;
    const int textPromptHeight = 30;
    const int buttonHeight = 30;
    const int generateButtonHeight = 30;
    const int channelSelectorHeight = 30;
    const int knobAreaHeight = 140;
    const int controlsHeight = 160;
    
    const int labelHeight = 15;
    const int textPromptLabelHeight = 15;
    const int variationSelectorHeight = 25; // Smaller height for smaller font
    const int pannerHeight = 150; // 2D panner height
    const int totalBottomHeight = textPromptLabelHeight + spacingSmall +
                                  textPromptHeight + spacingSmall +
                                  channelSelectorHeight + spacingSmall +
                                  knobAreaHeight + spacingSmall + 
                                  controlsHeight + spacingSmall +
                                  generateButtonHeight + spacingSmall +
                                  buttonHeight + spacingSmall +
                                  labelHeight + spacingSmall +
                                  pannerHeight;
    
    // Increase waveform display height by reducing bottom area
    const int waveformExtraHeight = 50;  // Make waveform taller
    
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
    
    // Reserve space for controls at bottom (reduced to make waveform taller)
    auto bottomArea = bounds.removeFromBottom(totalBottomHeight - waveformExtraHeight);
    
    // Waveform area - remove space for variation selector below it
    auto waveformArea = bounds.removeFromBottom(variationSelectorHeight + spacingSmall);
    variationSelector.setBounds(waveformArea.removeFromBottom(variationSelectorHeight));
    waveformDisplay.setBounds(bounds);
    
    // Text prompt at top of bottom area (label above editor)
    textPromptLabel.setBounds(bottomArea.removeFromTop(textPromptLabelHeight));
    bottomArea.removeFromTop(spacingSmall);
    textPromptEditor.setBounds(bottomArea.removeFromTop(textPromptHeight));
    bottomArea.removeFromTop(spacingSmall);
    
    // Level control and VU meter with knobs and autogen toggle
    auto controlsArea = bottomArea.removeFromTop(controlsHeight);
    
    // Left side: VU meter (levelControl)
    levelControl.setBounds(controlsArea.removeFromLeft(115)); // 80 + 5 + 30
    controlsArea.removeFromLeft(spacingSmall);
    
    // Right side: knobs above autogen toggle
    auto rightSide = controlsArea;
    auto knobArea = rightSide.removeFromTop(knobAreaHeight);
    parameterKnobs.setBounds(knobArea);
    rightSide.removeFromTop(spacingSmall);
    autogenToggle.setBounds(rightSide.removeFromTop(30)); // Toggle button height
    bottomArea.removeFromTop(spacingSmall);
    
    // Generate button
    generateButton.setBounds(bottomArea.removeFromTop(generateButtonHeight));
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
        
        // Add toggle buttons between panLabel and panCoordLabel
        const int buttonWidth = 30;
        const int buttonSpacing = 5;
        if (panner2DComponent != nullptr && panner2DComponent->isVisible())
        {
            trajectoryToggle.setBounds(panLabelArea.removeFromLeft(buttonWidth));
            panLabelArea.removeFromLeft(buttonSpacing);
            onsetToggle.setBounds(panLabelArea.removeFromLeft(buttonWidth));
            panLabelArea.removeFromLeft(buttonSpacing);
        }
        else
        {
            // Hide toggles if 2D panner is not visible
            trajectoryToggle.setBounds(0, 0, 0, 0);
            onsetToggle.setBounds(0, 0, 0, 0);
        }
        
        panCoordLabel.setBounds(panLabelArea); // Coordinates on right
        bottomArea.removeFromTop(spacingSmall);
        
        // Save trajectory button in new row below panCoordLabel
        if (panner2DComponent != nullptr && panner2DComponent->isVisible())
        {
            auto saveButtonArea = bottomArea.removeFromTop(labelHeight);
            saveTrajectoryButton.setBounds(saveButtonArea.removeFromLeft(60)); // Button width
            bottomArea.removeFromTop(spacingSmall);
        }
        else
        {
            // Hide save button if 2D panner is not visible
            saveTrajectoryButton.setBounds(0, 0, 0, 0);
        }
        
        auto pannerArea = bottomArea.removeFromTop(pannerHeight);
        if (pannerType.toLowerCase() == "stereo" && stereoPanSlider.isVisible())
        {
            stereoPanSlider.setBounds(pannerArea);
        }
        else if (panner2DComponent != nullptr && panner2DComponent->isVisible())
        {
            panner2DComponent->setBounds(pannerArea);
            
            // Path buttons below panner
            const int pathButtonHeight = 25;
            auto pathButtonArea = bottomArea.removeFromTop(pathButtonHeight);
            if (pathGeneratorButtons != nullptr)
            {
                pathGeneratorButtons->setBounds(pathButtonArea);
            }
            
            bottomArea.removeFromTop(spacingSmall);
            
            // Path control knobs
            const int knobSize = 60;
            const int knobLabelHeight = 15;
            const int knobSpacing = 10;
            auto knobArea = bottomArea.removeFromTop(knobSize + knobLabelHeight);
            
            // Speed knob
            auto speedKnobArea = knobArea.removeFromLeft(knobSize);
            pathSpeedKnob.setBounds(speedKnobArea.removeFromTop(knobSize));
            pathSpeedLabel.setBounds(speedKnobArea);
            knobArea.removeFromLeft(knobSpacing);
            
            // Scale knob
            auto scaleKnobArea = knobArea.removeFromLeft(knobSize);
            pathScaleKnob.setBounds(scaleKnobArea.removeFromTop(knobSize));
            pathScaleLabel.setBounds(scaleKnobArea);
        }
        else
        {
            // Hide path buttons if 2D panner is not visible
            if (pathGeneratorButtons != nullptr)
            {
                pathGeneratorButtons->setBounds(0, 0, 0, 0);
            }
            pathSpeedKnob.setBounds(0, 0, 0, 0);
            pathSpeedLabel.setBounds(0, 0, 0, 0);
            pathScaleKnob.setBounds(0, 0, 0, 0);
            pathScaleLabel.setBounds(0, 0, 0, 0);
        }
    }
}

void LooperTrack::playButtonClicked(bool shouldPlay)
{
    auto& track = looperEngine.get_track(trackIndex);
    
    if (shouldPlay)
    {
        track.m_is_playing.store(true);
        track.m_read_head.set_playing(true);
        
        if (track.m_write_head.get_record_enable() && !track.m_tape_loop.m_has_recorded.load())
        {
            const juce::ScopedLock sl(track.m_tape_loop.m_lock);
            track.m_tape_loop.clear_buffer();
            track.m_write_head.reset();
            track.m_read_head.reset();
        }
    }
    else
    {
        track.m_is_playing.store(false);
        track.m_read_head.set_playing(false);
        
        // If playback stopped and we have pending variations, apply them now
        if (hasPendingVariations)
        {
            DBG("LooperTrack: Playback stopped, applying pending variations immediately");
            applyVariationsFromFiles(pendingVariationFiles);
            pendingVariationFiles.clear();
            hasPendingVariations = false;
        }
        
        if (track.m_write_head.get_record_enable())
        {
            track.m_write_head.finalize_recording(track.m_write_head.get_pos());
            juce::Logger::writeToLog("~~~ Playback just stopped, finalized recording");
        }
    }
    
    repaint();
}

void LooperTrack::muteButtonToggled(bool muted)
{
    auto& track = looperEngine.get_track(trackIndex);
    track.m_read_head.set_muted(muted);
}

void LooperTrack::generateButtonClicked()
{
    auto& track = looperEngine.get_track(trackIndex);
    
    // Get text prompt from the track
    juce::String textPrompt = getTextPrompt();
    if (textPrompt.isEmpty())
    {
        textPrompt = "Hello!!"; // Default prompt
    }

    DBG("LooperTrack: Starting generation with text prompt: " + textPrompt);

    // Stop any existing worker thread
    if (gradioWorkerThread != nullptr)
    {
        gradioWorkerThread->stopThread(1000);
        gradioWorkerThread.reset();
    }

    // Disable generate button during processing
    generateButton.setEnabled(false);
    generateButton.setButtonText("generating...");
    
    // Reset status text
    gradioStatusText = "";

    // Always pass empty File (null) to gradio - audio is never sent
    juce::File audioFile; // Always empty - audio is always null
    DBG("LooperTrack: Always passing empty file (null audio) to gradio");

    // Create and start background worker thread
    gradioWorkerThread = std::make_unique<GradioWorkerThread>(looperEngine,
                                                              trackIndex,
                                                              audioFile,
                                                              textPrompt,
                                                              customText2SoundParams,
                                                              gradioUrlProvider);
    gradioWorkerThread->onComplete = [this](juce::Result result, juce::Array<juce::File> outputFiles, int trackIdx)
    {
        onGradioComplete(result, outputFiles);
    };
    
    gradioWorkerThread->onStatusUpdate = [this](const juce::String& statusText)
    {
        DBG("LooperTrack: Received status update - " + statusText);
        gradioStatusText = statusText;
        generateButton.setButtonText(statusText);
        repaint();
    };
    
    gradioWorkerThread->startThread();
}

void LooperTrack::updateModelParams(const juce::var& newParams)
{
    customText2SoundParams = newParams;
    DBG("LooperTrack: Model parameters updated for track " + juce::String(trackIndex));
}

void LooperTrack::setPannerSmoothingTime(double smoothingTime)
{
    if (panner2DComponent != nullptr)
    {
        panner2DComponent->set_smoothing_time(smoothingTime);
        DBG("LooperTrack: Panner smoothing time set to " + juce::String(smoothingTime) + " seconds for track " + juce::String(trackIndex));
    }
}


juce::var LooperTrack::getDefaultText2SoundParams()
{
    // Create default parameters object (excluding text prompt and audio which are in UI)
    juce::DynamicObject::Ptr params = new juce::DynamicObject();
    
    // New API parameters (indices 2-6):
    params->setProperty("seed", juce::var());                          // [2] seed (null for random)
    params->setProperty("median_filter_length", juce::var(0));          // [3] median filter length (0 for none)
    params->setProperty("normalize_db", juce::var(-24));                // [4] normalize dB (0 for none)
    params->setProperty("duration", juce::var(5.0));                     // [5] duration in seconds (default 5.0)
    
    // Create inference parameters as Python dict literal string
    // The API expects Python dict syntax (single quotes), not JSON (double quotes)
    juce::String inferenceParamsString = 
        "{'guidance_scale': 3.0, "
        "'logsnr_max': 5.0, "
        "'logsnr_min': -8, "
        "'num_seconds': 8.0, "
        "'num_steps': 24, "
        "'rho': 7.0, "
        "'sampler': 'dpmpp-2m-sde', "
        "'schedule': 'karras'}";
    
    params->setProperty("inference_params", juce::var(inferenceParamsString));  // [6] inference parameters as Python dict string
    
    return juce::var(params);
}

void LooperTrack::onGradioComplete(juce::Result result, juce::Array<juce::File> outputFiles)
{
    // Reset status text
    gradioStatusText = "";
    
    // Re-enable button
    generateButton.setEnabled(true);
    generateButton.setButtonText("generate");

    // Clean up worker thread
    if (gradioWorkerThread != nullptr)
    {
        gradioWorkerThread->stopThread(1000);
        gradioWorkerThread.reset();
    }

    if (result.failed())
    {
        juce::String errorTitle = "generation failed";
        juce::String errorMessage = "failed to generate audio: " + result.getErrorMessage();
        
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                              errorTitle,
                                              errorMessage);
        return;
    }

    auto& track = looperEngine.get_track(trackIndex);
    bool is_playing = track.m_is_playing.load();
    
    // Check if we should wait for current variation's loop end before updating
    if (waitForLoopEndBeforeUpdate && is_playing)
    {
        // Store pending variations and wait for current variation's loop to wrap
        pendingVariationFiles = outputFiles;
        hasPendingVariations = true;
        DBG("LooperTrack: Generation complete, waiting for current variation's loop end before updating (playing variation " + juce::String(currentVariationIndex + 1) + ")");
        return;
    }

    // Apply variations immediately
    applyVariationsFromFiles(outputFiles);
    
    // Start playback if not already playing
    if (!is_playing)
    {
        track.m_is_playing.store(true);
        track.m_read_head.set_playing(true);
        transportControls.setPlayState(true);
    }
    
    // Check if autogen is enabled - if so, automatically trigger next generation
    if (autogenToggle.getToggleState())
    {
        DBG("LooperTrack: Autogen enabled - automatically triggering next generation");
        juce::MessageManager::callAsync([this]()
        {
            generateButtonClicked();
        });
    }
}

void LooperTrack::saveTrajectory()
{
    // Check if panner2DComponent exists and has a trajectory
    if (panner2DComponent == nullptr)
    {
        DBG("LooperTrack: Cannot save trajectory - panner2DComponent is null");
        return;
    }
    
    auto trajectory = panner2DComponent->get_trajectory();
    if (trajectory.empty())
    {
        DBG("LooperTrack: Cannot save trajectory - trajectory is empty");
        return;
    }
    
    // Get trajectory directory from config (with default)
    auto defaultTrajectoryDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                    .getChildFile("unsound-objects")
                                    .getChildFile("trajectories")
                                    .getFullPathName();
    juce::String trajectoryDir = Shared::ConfigManager::loadStringValue("text2sound", "trajectoryDir", defaultTrajectoryDir);
    
    // Create directory if it doesn't exist
    juce::File dir(trajectoryDir);
    juce::Result dirResult = dir.createDirectory();
    if (!dirResult.wasOk() && !dir.isDirectory())
    {
        DBG("LooperTrack: Failed to create trajectory directory: " + dirResult.getErrorMessage());
        return;
    }
    
    // Get text prompt
    juce::String prompt = textPromptEditor.getText();
    
    // Get duration from parameter knobs (index 1 is duration)
    double duration = parameterKnobs.getKnobValue(1);
    
    // Get other trajectory parameters
    double playbackSpeed = pathSpeedKnob.getValue();
    double trajectoryScale = pathScaleKnob.getValue();
    double smoothing_time = panner2DComponent->get_smoothing_time();
    
    // Create JSON object
    juce::var jsonObj;
    jsonObj.getDynamicObject()->setProperty("date", juce::Time::getCurrentTime().toISO8601(true));
    jsonObj.getDynamicObject()->setProperty("prompt", prompt);
    jsonObj.getDynamicObject()->setProperty("duration", duration);
    jsonObj.getDynamicObject()->setProperty("playbackSpeed", playbackSpeed);
    jsonObj.getDynamicObject()->setProperty("trajectoryScale", trajectoryScale);
    jsonObj.getDynamicObject()->setProperty("smoothingTime", smoothing_time);
    
    // Create coords array
    juce::var coordsArray = juce::var(juce::Array<juce::var>());
    for (const auto& point : trajectory)
    {
        juce::var coordObj;
        coordObj.getDynamicObject()->setProperty("x", point.x);
        coordObj.getDynamicObject()->setProperty("y", point.y);
        coordObj.getDynamicObject()->setProperty("t", point.time);
        coordsArray.getArray()->add(coordObj);
    }
    jsonObj.getDynamicObject()->setProperty("coords", coordsArray);
    
    // Generate unique filename with timestamp
    juce::Time now = juce::Time::getCurrentTime();
    juce::String filename = "trajectory_" + 
                            now.formatted("%Y%m%d_%H%M%S") + 
                            ".json";
    juce::File outputFile = dir.getChildFile(filename);
    
    // Write JSON to file
    juce::String jsonString = juce::JSON::toString(jsonObj, true);
    bool writeSuccess = outputFile.replaceWithText(jsonString);
    
    if (writeSuccess)
    {
        DBG("LooperTrack: Successfully saved trajectory to: " + outputFile.getFullPathName());
    }
    else
    {
        DBG("LooperTrack: Failed to save trajectory to: " + outputFile.getFullPathName());
    }
}

void LooperTrack::resetButtonClicked()
{
    auto& track = looperEngine.get_track(trackIndex);
    
    // Stop any ongoing generation
    if (gradioWorkerThread != nullptr)
    {
        gradioWorkerThread->stopThread(1000);
        gradioWorkerThread.reset();
    }
    generateButton.setEnabled(true);
    generateButton.setButtonText("generate");
    
    // Stop playback
    track.m_is_playing.store(false);
    track.m_read_head.set_playing(false);
    transportControls.setPlayState(false);
    
    // Clear buffer
    const juce::ScopedLock sl(track.m_tape_loop.m_lock);
    track.m_tape_loop.clear_buffer();
    track.m_write_head.reset();
    track.m_read_head.reset();
    
    // Reset controls to defaults
    parameterKnobs.setKnobValue(0, 1.0, juce::dontSendNotification); // speed
    track.m_read_head.set_speed(1.0f);
    
    parameterKnobs.setKnobValue(1, 5.0, juce::dontSendNotification); // duration (default 5.0)
    // Reset duration parameter and WrapPos
    auto* obj = customText2SoundParams.getDynamicObject();
    if (obj != nullptr)
    {
        obj->setProperty("duration", juce::var(5.0));
    }
    double sample_rate = track.m_write_head.get_sample_rate();
    if (sample_rate > 0.0)
    {
        track.m_write_head.set_wrap_pos(static_cast<size_t>(5.0 * sample_rate));
    }
    
    levelControl.setLevelValue(0.0, juce::dontSendNotification);
    track.m_read_head.set_level_db(0.0f);
    
    // Unmute
    track.m_read_head.set_muted(false);
    transportControls.setMuteState(false);
    
    
    // Clear text prompt
    textPromptEditor.clear();
    
    // Reset panner position to center and stop any path playback
    if (panner2DComponent != nullptr)
    {
        panner2DComponent->stop_playback();
        panner2DComponent->set_pan_position(0.5f, 0.5f, juce::sendNotification);
    }
    else if (pannerType.toLowerCase() == "stereo" && stereoPanSlider.isVisible())
    {
        stereoPanSlider.setValue(0.5, juce::sendNotification);
    }
    
    // Reset path generator buttons
    if (pathGeneratorButtons != nullptr)
    {
        pathGeneratorButtons->resetAllButtons();
    }
    
    repaint();
}

LooperTrack::~LooperTrack()
{
    stopTimer();
    
    // Remove mouse listener first
    if (generateButtonMouseListener)
        generateButton.removeMouseListener(generateButtonMouseListener.get());
    
    // Unregister MIDI parameters
    if (midiLearnManager)
    {
        midiLearnManager->unregisterParameter(trackIdPrefix + "_generate");
    }
    
    // Stop and wait for background thread to finish
    if (gradioWorkerThread != nullptr)
    {
        gradioWorkerThread->stopThread(5000); // Wait up to 5 seconds
        gradioWorkerThread.reset();
    }
}

void LooperTrack::set_playback_speed(float speed)
{
    parameterKnobs.setKnobValue(0, speed, juce::dontSendNotification);
    looperEngine.get_track(trackIndex).m_read_head.set_speed(speed);
}

float LooperTrack::get_playback_speed() const
{
    return static_cast<float>(parameterKnobs.getKnobValue(0));
}

void LooperTrack::drawCustomToggleButton(juce::Graphics& g, juce::ToggleButton& button, 
                                        const juce::String& letter, juce::Rectangle<int> bounds,
                                        juce::Colour onColor, juce::Colour offColor,
                                        bool showMidiIndicator)
{
    bool isOn = button.getToggleState();
    
    // Color scheme - use provided colors
    juce::Colour bgColor = isOn ? onColor : juce::Colours::black;
    juce::Colour textColor = isOn ? juce::Colours::black : offColor;
    juce::Colour borderColor = offColor;
    
    // Draw background
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    
    // Draw border (thicker if MIDI mapped)
    g.setColour(borderColor);
    g.drawRoundedRectangle(bounds.toFloat(), 6.0f, showMidiIndicator ? 3.0f : 2.0f);
    
    // Draw MIDI indicator dot in top right corner
    if (showMidiIndicator)
    {
        g.setColour(juce::Colour(0xffed1683));  // Pink
        g.fillEllipse(bounds.getRight() - 8.0f, bounds.getY() + 2.0f, 4.0f, 4.0f);
    }
    
    // Draw letter
    g.setColour(textColor);
    g.setFont(juce::Font(juce::FontOptions()
                        .withName(juce::Font::getDefaultMonospacedFontName())
                        .withHeight(18.0f)));
    g.drawText(letter, bounds, juce::Justification::centred);
}

void LooperTrack::generatePath(const juce::String& pathType)
{
    if (panner2DComponent == nullptr)
        return;
    
    DBG("LooperTrack: Generating path type: " + pathType);
    
    std::vector<Panner2DComponent::TrajectoryPoint> trajectoryPoints;
    std::vector<std::pair<float, float>> coords;
    
    auto pathTypeLower = pathType.toLowerCase();
    
    if (pathTypeLower == "circle")
    {
        coords = PanningUtils::generate_circle_path();
    }
    else if (pathTypeLower == "random")
    {
        coords = PanningUtils::generate_random_path();
    }
    else if (pathTypeLower == "wander")
    {
        coords = PanningUtils::generate_wander_path();
    }
    else if (pathTypeLower == "swirls")
    {
        coords = PanningUtils::generate_swirls_path();
    }
    else if (pathTypeLower == "bounce")
    {
        coords = PanningUtils::generate_bounce_path();
    }
    else if (pathTypeLower == "spiral")
    {
        coords = PanningUtils::generate_spiral_path();
    }
    else
    {
        DBG("LooperTrack: Unknown path type: " + pathType);
        return;
    }
    
    // Convert to TrajectoryPoint format
    for (const auto& coord : coords)
    {
        Panner2DComponent::TrajectoryPoint point;
        point.x = coord.first;
        point.y = coord.second;
        point.time = 0.0; // Time will be set during playback
        trajectoryPoints.push_back(point);
    }
    
    // Set trajectory and start playback
    panner2DComponent->set_trajectory(trajectoryPoints, true);
    
    DBG("LooperTrack: Generated " + juce::String(trajectoryPoints.size()) + " points for path type: " + pathType);
}

void LooperTrack::feedAudioSample(float sample)
{
    // Feed audio sample to onset detector (called from audio thread)
    // Process onset detection directly here for low latency
    
    // Only process if onset toggle is enabled and trajectory is playing (use atomic flags)
    if (!onsetToggleEnabled.load() || !trajectoryPlaying.load())
        return;
    
    // Add sample to processing buffer (lock-free, single writer from audio thread)
    int currentFill = onsetBufferFill.load();
    if (currentFill < onsetBlockSize)
    {
        onsetProcessingBuffer[currentFill] = sample;
        int newFill = currentFill + 1;
        onsetBufferFill.store(newFill);
        
        // When buffer is full, process for onset detection
        if (newFill >= onsetBlockSize)
        {
            // Get sample rate (cached to avoid repeated atomic reads)
            auto& track = looperEngine.get_track(trackIndex);
            double sample_rate = track.m_write_head.get_sample_rate();
            if (sample_rate <= 0.0)
                sample_rate = 44100.0;
            lastOnsetSampleRate = sample_rate;
            
            // Process block for onset detection
            bool detected = onsetDetector.processBlock(onsetProcessingBuffer.data(), onsetBlockSize, sample_rate);
            
            if (detected)
            {
                // DBG("LooperTrack: Onset detected in track " + juce::String(trackIndex) + " (audio thread) - advancing trajectory");
                
                // Update atomic flags for UI thread
                onsetDetected.store(true);
                double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
                onsetLEDBrightness.store(1.0);
                lastOnsetLEDTime.store(currentTime);
                
                // Set flag to advance trajectory (will be processed on message thread)
                pendingTrajectoryAdvance.store(true);
                
                // Trigger async update for UI repaint and trajectory advancement (non-blocking, safe from audio thread)
                triggerAsyncUpdate();
            }
            
            // Reset buffer
            onsetBufferFill.store(0);
        }
    }
}

void LooperTrack::timerCallback()
{
    // Sync button states with model state
    auto& track = looperEngine.get_track(trackIndex);
    
    bool modelIsPlaying = track.m_is_playing.load();
    transportControls.setPlayState(modelIsPlaying);
    
    // Update cached trajectory playing state (for audio thread access)
    if (panner2DComponent != nullptr)
    {
        trajectoryPlaying.store(panner2DComponent->is_playing());
    }
    
    // Note: Onset detection is now processed directly in feedAudioSample() from audio thread
    // for low latency. Timer callback only handles LED fade-out.
    
    float current_pos = track.m_read_head.get_pos();
    float wrap_pos = static_cast<float>(track.m_write_head.get_wrap_pos());
    bool wrapped = false;
    
    // Detect wrap: if we were near the end and now we're near the start
    if (wrap_pos > 0.0f)
    {
        float wrap_threshold = wrap_pos * 0.1f; // 10% threshold
        bool was_near_end = m_last_read_head_position > (wrap_pos - wrap_threshold);
        bool is_near_start = current_pos < wrap_threshold;
        
        if (was_near_end && is_near_start && m_last_read_head_position != current_pos)
        {
            wrapped = true;
        }
    }
    
    // Check for pending variations and apply them on wrap (before auto-cycling)
    // This ensures we apply new variations at the end of the current variation's loop
    if (hasPendingVariations && wrapped && modelIsPlaying)
    {
        DBG("LooperTrack: Current variation's loop wrapped, applying pending variations");
        applyVariationsFromFiles(pendingVariationFiles);
        pendingVariationFiles.clear();
        hasPendingVariations = false;
        // Don't auto-cycle after applying - the new variations are already loaded
        m_last_read_head_position = current_pos;
        return;
    }
    
    // Check for auto-cycling variations (only if no pending variations)
    if (autoCycleVariations && modelIsPlaying && !variations.empty() && wrapped && !hasPendingVariations)
    {
        // Wrapped around - cycle to next variation
        cycleToNextVariation();
    }
    
    m_last_read_head_position = current_pos;
    
    // Update onset LED brightness (fade out over time)
    double currentLEDBrightness = onsetLEDBrightness.load();
    if (currentLEDBrightness > 0.0)
    {
        double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        double lastLEDTime = lastOnsetLEDTime.load();
        double elapsed = currentTime - lastLEDTime;
        if (elapsed >= onsetLEDDecayTime)
        {
            onsetLEDBrightness.store(0.0);
        }
        else
        {
            // Linear fade out
            double newBrightness = 1.0 - (elapsed / onsetLEDDecayTime);
            onsetLEDBrightness.store(newBrightness);
        }
    }
    
    // Update displays
    waveformDisplay.repaint();
    levelControl.repaint();
    repaint(); // Repaint to update LED fade
}

void LooperTrack::handleAsyncUpdate()
{
    // Called from message thread when onset is detected (triggered from audio thread)
    // Advance trajectory if pending
    if (pendingTrajectoryAdvance.load())
    {
        pendingTrajectoryAdvance.store(false);
        if (panner2DComponent != nullptr)
        {
            panner2DComponent->advance_trajectory_onset();
        }
    }
    
    // Force immediate repaint to show LED
    repaint();
}

void LooperTrack::updateChannelSelectors()
{
    // Update channel selector based on current audio device
    inputSelector.updateChannels(looperEngine.get_audio_device_manager());
}

void LooperTrack::loadVariationFromFile(int variationIndex, const juce::File& audioFile)
{
    if (variationIndex < 0 || variationIndex >= static_cast<int>(variations.size()))
        return;
    
    if (!audioFile.existsAsFile())
    {
        DBG("Variation file does not exist: " + audioFile.getFullPathName());
        return;
    }
    
    auto& variation = variations[variationIndex];
    auto& track = looperEngine.get_track(trackIndex);
    auto& trackEngine = looperEngine.get_track_engine(trackIndex);
    
    // Use the track engine's format manager to read the file
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
    if (reader == nullptr)
    {
        DBG("Could not create reader for variation file: " + audioFile.getFullPathName());
        return;
    }
    
    const juce::ScopedLock sl(variation->m_lock);
    auto& buffer = variation->get_buffer();
    
    if (buffer.empty())
    {
        DBG("Variation buffer not allocated");
        return;
    }
    
    // Clear the buffer first
    variation->clear_buffer();
    
    // Determine how many samples to read
    juce::int64 numSamplesToRead = juce::jmin(reader->lengthInSamples, static_cast<juce::int64>(buffer.size()));
    
    if (numSamplesToRead <= 0)
    {
        DBG("Variation file has no samples");
        return;
    }
    
    // Read audio data
    juce::AudioBuffer<float> tempBuffer(static_cast<int>(reader->numChannels), static_cast<int>(numSamplesToRead));
    
    if (!reader->read(&tempBuffer, 0, static_cast<int>(numSamplesToRead), 0, true, true))
    {
        DBG("Failed to read variation audio data");
        return;
    }
    
    // Convert to mono and write to variation buffer
    if (tempBuffer.getNumChannels() == 1)
    {
        const float* source = tempBuffer.getReadPointer(0);
        for (int i = 0; i < static_cast<int>(numSamplesToRead); ++i)
        {
            buffer[i] = source[i];
        }
    }
    else
    {
        for (int i = 0; i < static_cast<int>(numSamplesToRead); ++i)
        {
            float sum = 0.0f;
            for (int channel = 0; channel < tempBuffer.getNumChannels(); ++channel)
            {
                sum += tempBuffer.getSample(channel, i);
            }
            buffer[i] = sum / static_cast<float>(tempBuffer.getNumChannels());
        }
    }
    
    // Update variation metadata
    size_t loadedLength = static_cast<size_t>(numSamplesToRead);
    variation->m_recorded_length.store(loadedLength);
    variation->m_has_recorded.store(true);
    
    DBG("Loaded variation " + juce::String(variationIndex + 1) + " from file: " + audioFile.getFileName());
}

void LooperTrack::applyVariationsFromFiles(const juce::Array<juce::File>& outputFiles)
{
    // Update number of variations if we got a different number
    int numReceived = outputFiles.size();
    if (numReceived != numVariations)
    {
        numVariations = numReceived;
        variationSelector.setNumVariations(numVariations);
        
        // Reallocate variations if needed
        auto& track = looperEngine.get_track(trackIndex);
        double sample_rate = track.m_write_head.get_sample_rate();
        if (sample_rate <= 0.0)
            sample_rate = 44100.0;
        
        variations.clear();
        for (int i = 0; i < numVariations; ++i)
        {
            auto variation = std::make_unique<TapeLoop>();
            variation->allocate_buffer(sample_rate, 10.0);
            variations.push_back(std::move(variation));
        }
    }

    // Load each variation from its file
    bool allLoaded = true;
    for (int i = 0; i < juce::jmin(numVariations, outputFiles.size()); ++i)
    {
        loadVariationFromFile(i, outputFiles[i]);
        if (!variations[i]->m_has_recorded.load())
            allLoaded = false;
    }

    if (!allLoaded)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                              "load failed",
                                              "some variations failed to load.");
        return;
    }

    // Switch to first variation and load it into the active track
    currentVariationIndex = 0;
    variationSelector.setSelectedVariation(0);
    switchToVariation(0);
    
    repaint(); // Refresh waveform display
}

void LooperTrack::switchToVariation(int variationIndex)
{
    if (variationIndex < 0 || variationIndex >= static_cast<int>(variations.size()))
        return;
    
    if (!variations[variationIndex]->m_has_recorded.load())
        return;
    
    auto& variation = variations[variationIndex];
    auto& track = looperEngine.get_track(trackIndex);
    auto& trackEngine = looperEngine.get_track_engine(trackIndex);
    
    // Copy variation buffer to active track buffer
    {
        const juce::ScopedLock slVar(variation->m_lock);
        const juce::ScopedLock sl_track(track.m_tape_loop.m_lock);
        
        auto& var_buffer = variation->get_buffer();
        auto& track_buffer = track.m_tape_loop.get_buffer();
        
        if (var_buffer.empty() || track_buffer.empty())
            return;
        
        size_t copy_length = juce::jmin(var_buffer.size(), track_buffer.size(), variation->m_recorded_length.load());
        
        // Clear track buffer first
        std::fill(track_buffer.begin(), track_buffer.end(), 0.0f);
        
        // Copy variation data
        for (size_t i = 0; i < copy_length; ++i)
        {
            track_buffer[i] = var_buffer[i];
        }
        
        // Update track metadata
        track.m_tape_loop.m_recorded_length.store(copy_length);
        track.m_tape_loop.m_has_recorded.store(true);
        
        // Update wrapPos
        track.m_write_head.set_wrap_pos(copy_length);
        track.m_write_head.set_pos(copy_length);
    }
    
    // Reset read head to start
    track.m_read_head.reset();
    track.m_read_head.set_pos(0.0f);
    
    currentVariationIndex = variationIndex;
    variationSelector.setSelectedVariation(variationIndex);
    
    repaint();
    
    DBG("Switched to variation " + juce::String(variationIndex + 1));
}

void LooperTrack::cycleToNextVariation()
{
    if (!autoCycleVariations || variations.empty())
        return;
    
    int nextIndex = (currentVariationIndex + 1) % static_cast<int>(variations.size());
    switchToVariation(nextIndex);
}
