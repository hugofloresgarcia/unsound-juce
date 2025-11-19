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
    const juce::String defaultUrl = "https://hugggof-saos.hf.space/";
    juce::String configuredUrl = defaultUrl;

    if (gradioUrlProvider)
    {
        juce::String providedUrl = gradioUrlProvider();
        if (providedUrl.isNotEmpty())
            configuredUrl = providedUrl;
    }

    spaceInfo.gradio = configuredUrl;
    gradioClient.setSpaceInfo(spaceInfo);

    // Step 3: Extract duration from params (new API only needs text prompt and duration)
    juce::var paramsToUse = customText2SoundParams.isObject() ? customText2SoundParams : Text2Sound::LooperTrack::getDefaultText2SoundParams();
    int durationSeconds = 11; // Default duration
    auto* obj = paramsToUse.getDynamicObject();
    if (obj != nullptr && obj->hasProperty("duration"))
    {
        durationSeconds = static_cast<int>(obj->getProperty("duration"));
        // Clamp to valid range (1-11 seconds for stable-audio-open-small)
        durationSeconds = juce::jlimit(1, 11, durationSeconds);
    }
    
    juce::Array<juce::File> outputFiles;
    
    // Notify status update: processing
    DBG("GradioWorkerThread: Status update - Processing...");
    juce::MessageManager::callAsync([this]()
    {
        if (onStatusUpdate)
            onStatusUpdate("Processing...");
    });
    
    // Use new generate_audio API: [textPrompt, durationSeconds]
    auto result = gradioClient.processRequestGenerateAudio(textPrompt, durationSeconds, outputFiles);

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
LooperTrack::LooperTrack(MultiTrackLooperEngine& engine, int index, std::function<juce::String()> gradioUrlGetter, Shared::MidiLearnManager* midiManager, const juce::String& pannerTypeStr)
    : looperEngine(engine), 
      trackIndex(index),
      waveformDisplay(engine, index),
      transportControls(midiManager, "track" + juce::String(index)),
      parameterKnobs(midiManager, "track" + juce::String(index)),
      levelControl(engine, index, midiManager, "track" + juce::String(index)),
      trackLabel("Track", "track " + juce::String(index + 1)),
      resetButton("x"),
      generateButton("generate"),
      textPromptLabel("TextPrompt", "query"),
      autogenToggle("autogen"),
      gradioUrlProvider(std::move(gradioUrlGetter)),
      midiLearnManager(midiManager),
      trackIdPrefix("track" + juce::String(index)),
      pannerType(stringToPannerType(pannerTypeStr)),
      stereoPanSlider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox),
      panLabel("pan", "pan"),
      panCoordLabel("coord", "0.50, 0.50")
{
    // Initialize custom params with defaults (will be updated by MainComponent)
    customText2SoundParams = getDefaultText2SoundParams();
    
    // Initialize variations (allocate TapeLoops for each variation)
    auto& track = looperEngine.get_track_engine(trackIndex);
    double sample_rate = track.get_sample_rate();
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
    // Set label text based on panner type
    if (pannerType == PannerType::CLEAT)
    {
        panLabel.setText("cleatpan", juce::dontSendNotification);
    }
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
            // Update cached trajectory playing state immediately (for audio thread access)
            trajectoryPlaying.store(panner2DComponent->is_playing());
            DBG("LooperTrack: Onset toggle [" + juce::String(enabled ? "ON" : "OFF") + "], trajectory playing: " + (panner2DComponent->is_playing() ? "true" : "false"));
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
    textPromptEditor.setTextToShowWhenEmpty("enter text query...", juce::Colours::grey);
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
            looperEngine.get_track_engine(trackIndex).set_speed(static_cast<float>(value));
        },
        ""  // parameterId - will be auto-generated
    });
    
    parameterKnobs.addKnob({
        "duration",
        0.1, 8.0, 5.0, 0.01,  // Minimum 300ms to prevent audio crashes
        "s",
        [this](double value) {
            // Ensure value is at least 0.3 seconds (300ms) to prevent crashes
            double clamped_value = juce::jmax(0.3, value);
            if (clamped_value != value)
            {
                DBG("LooperTrack: Duration clamped from " + juce::String(value) + " to " + juce::String(clamped_value) + " (minimum 300ms)");
                parameterKnobs.setKnobValue(1, clamped_value, juce::dontSendNotification);
            }
            
            auto& track = looperEngine.get_track_engine(trackIndex);
            double sample_rate = track.get_sample_rate();
            if (sample_rate > 0.0)
            {
                // Convert duration (seconds) to samples and set WrapPos
                size_t loop_end = static_cast<size_t>(clamped_value * sample_rate);
                track.set_loop_end(loop_end);
                
                // Repaint waveform display to show updated bounds
                waveformDisplay.repaint();
            }
            
            // Update duration parameter for gradio endpoint
            auto* obj = customText2SoundParams.getDynamicObject();
            if (obj != nullptr)
            {
                obj->setProperty("duration", juce::var(clamped_value));
            }
        },
        ""  // parameterId - will be auto-generated
    });
    
    // Initialize duration to 5.0 seconds (default value)
    {
        auto& track_init = looperEngine.get_track_engine(trackIndex);
        double sample_rate_init = track_init.get_sample_rate();
        if (sample_rate_init <= 0.0)
            sample_rate_init = 44100.0; // Default sample rate
        
        if (sample_rate_init > 0.0)
        {
            size_t loop_end = static_cast<size_t>(5.0 * sample_rate_init);
            track_init.set_loop_end(loop_end);
        }
        
        // Update duration parameter for gradio endpoint
        auto* obj = customText2SoundParams.getDynamicObject();
        if (obj != nullptr)
        {
            obj->setProperty("duration", juce::var(5.0));
        }
    }
    
    addAndMakeVisible(parameterKnobs);
    
    // Setup cutoff knob (above level control)
    cutoffKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    cutoffKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    cutoffKnob.setRange(20.0, 20000.0, 1.0);
    cutoffKnob.setValue(4000.0); // Default to 4kHz
    cutoffKnob.setDoubleClickReturnValue(true, 4000.0);
    cutoffKnob.onValueChange = [this] {
        looperEngine.get_track_engine(trackIndex).set_filter_cutoff(static_cast<float>(cutoffKnob.getValue()));
    };
    // Initialize filter with default cutoff value
    looperEngine.get_track_engine(trackIndex).set_filter_cutoff(4000.0f);
    addAndMakeVisible(cutoffKnob);
    cutoffLabel.setText("cutoff", juce::dontSendNotification);
    cutoffLabel.setJustificationType(juce::Justification::centred);
    cutoffLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(cutoffLabel);
    
    // Setup level control
    levelControl.onLevelChange = [this](double value) {
        looperEngine.get_track_engine(trackIndex).set_level_db(static_cast<float>(value));
    };
    addAndMakeVisible(levelControl);
    
    // Setup "autogen" toggle
    autogenToggle.setButtonText("autogen");
    autogenToggle.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(autogenToggle);
    
    // Setup panner based on type
    if (pannerType == PannerType::Stereo)
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
    else if (pannerType == PannerType::Quad)
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
    else if (pannerType == PannerType::CLEAT)
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

void LooperTrack::clearLookAndFeel()
{
    // Clear LookAndFeel references from all components that use it
    // This must be called before the parent's LookAndFeel is destroyed
    trackLabel.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
    generateButton.setLookAndFeel(nullptr);
    textPromptEditor.setLookAndFeel(nullptr);
    textPromptLabel.setLookAndFeel(nullptr);
    autogenToggle.setLookAndFeel(nullptr);
    // Also clear references to our own emptyToggleLookAndFeel
    trajectoryToggle.setLookAndFeel(nullptr);
    onsetToggle.setLookAndFeel(nullptr);
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
    
    // Draw cutoff knob value label
    if (cutoffKnob.isVisible() && cutoffKnob.getWidth() > 0)
    {
        auto knobBounds = cutoffKnob.getBounds();
        double cutoffValue = cutoffKnob.getValue();
        juce::String cutoffText;
        if (cutoffValue >= 1000.0)
            cutoffText = juce::String(cutoffValue / 1000.0, 1) + "k";
        else
            cutoffText = juce::String(static_cast<int>(cutoffValue));
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(cutoffText, knobBounds, juce::Justification::centred);
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
    const int knobAreaHeight = 210; // Increased to fit 3 knobs (speed, duration, cutoff)
    const int controlsHeight = 230; // Increased to accommodate level control + 3 knobs + autogen toggle
    const int cutoffKnobSize = 50; // Smaller to match path control knobs
    const int cutoffLabelHeight = 12;
    
    const int labelHeight = 15;
    const int textPromptLabelHeight = 15;
    const int variationSelectorHeight = 25; // Smaller height for smaller font
    const int pannerHeight = 150; // 2D panner height
    const int totalBottomHeight = textPromptLabelHeight + spacingSmall +
                                  textPromptHeight + spacingSmall +
                                  controlsHeight + spacingSmall +
                                  generateButtonHeight + spacingSmall +
                                  buttonHeight + spacingSmall +
                                  labelHeight + spacingSmall +
                                  pannerHeight;
    
    // Maximum waveform height - prevents waveform from growing too large
    const int maxWaveformHeight = 50;
    
    auto bounds = getLocalBounds().reduced(componentMargin);
    
    // Track label at top with reset button in top right corner
    auto trackLabelArea = bounds.removeFromTop(trackLabelHeight);
    resetButton.setBounds(trackLabelArea.removeFromRight(resetButtonSize));
    trackLabelArea.removeFromRight(spacingSmall);
    trackLabel.setBounds(trackLabelArea);
    bounds.removeFromTop(spacingSmall);
    
    
    // Calculate extra space available (beyond minimum required)
    const int minRequiredHeight = totalBottomHeight + maxWaveformHeight + variationSelectorHeight + spacingSmall;
    const int availableHeight = bounds.getHeight();
    const int extraHeight = juce::jmax(0, availableHeight - minRequiredHeight);
    
    // Add extra space to panner height
    const int actualPannerHeight = pannerHeight + extraHeight;
    const int actualTotalBottomHeight = totalBottomHeight - pannerHeight + actualPannerHeight;
    
    // Reserve space for controls at bottom (with extra space for panner)
    auto bottomArea = bounds.removeFromBottom(actualTotalBottomHeight);
    
    // Waveform area - limit maximum height so panner gets priority when window grows
    auto waveformArea = bounds.removeFromBottom(variationSelectorHeight + spacingSmall);
    variationSelector.setBounds(waveformArea.removeFromBottom(variationSelectorHeight));
    // Limit waveform height to maxWaveformHeight
    if (bounds.getHeight() > maxWaveformHeight)
    {
        bounds.setHeight(maxWaveformHeight);
    }
    waveformDisplay.setBounds(bounds);
    
    // Text prompt at top of bottom area (label above editor)
    textPromptLabel.setBounds(bottomArea.removeFromTop(textPromptLabelHeight));
    bottomArea.removeFromTop(spacingSmall);
    textPromptEditor.setBounds(bottomArea.removeFromTop(textPromptHeight));
    bottomArea.removeFromTop(spacingSmall);
    
    // Level control and VU meter with knobs and autogen toggle
    auto controlsArea = bottomArea.removeFromTop(controlsHeight);
    
    // Left column: level control
    const int leftColumnWidth = 115; // Match level control width
    auto leftColumn = controlsArea.removeFromLeft(leftColumnWidth);
    levelControl.setBounds(leftColumn);
    controlsArea.removeFromLeft(spacingSmall);
    
    // Right side: parameter knobs + cutoff knob + autogen toggle
    auto rightSide = controlsArea;
    auto knobsArea = rightSide.removeFromTop(knobAreaHeight);
    
    // Parameter knobs (speed, duration) in top part of knobs area
    auto parameterKnobsArea = knobsArea.removeFromTop(knobAreaHeight - cutoffKnobSize - cutoffLabelHeight - spacingSmall);
    parameterKnobs.setBounds(parameterKnobsArea);
    
    // Cutoff knob below parameter knobs (in same column, aligned with parameter knobs)
    knobsArea.removeFromTop(spacingSmall);
    auto cutoffKnobArea = knobsArea; // Use full width to match parameter knobs alignment
    cutoffKnob.setBounds(cutoffKnobArea.removeFromTop(cutoffKnobSize));
    cutoffLabel.setBounds(cutoffKnobArea); // Label below knob
    
    // Autogen toggle below knobs
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
        
        // Limit panner height to its width (keep it square)
        const int pannerMaxHeight = bottomArea.getWidth();
        const int finalPannerHeight = juce::jmin(actualPannerHeight, pannerMaxHeight);
        auto pannerArea = bottomArea.removeFromTop(finalPannerHeight);
        if (pannerType == PannerType::Stereo && stereoPanSlider.isVisible())
        {
            stereoPanSlider.setBounds(pannerArea);
        }
        else if (panner2DComponent != nullptr && panner2DComponent->isVisible())
        {
            panner2DComponent->setBounds(pannerArea);
            
            // Path buttons below panner (two rows)
            const int pathButtonHeight = 54; // Height for two rows (25 + 4 spacing + 25)
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
    auto& track = looperEngine.get_track_engine(trackIndex);
    
    if (shouldPlay)
    {
        track.set_playing(true);
        track.set_playing(true);
        
        if (track.get_record_enable() && !track.has_recorded())
        {
            const juce::ScopedLock sl(track.get_buffer_lock());
            track.clear_buffer();
            track.reset();
            track.reset();
        }
    }
    else
    {
        track.set_playing(false);
        track.set_playing(false);
        
        // If playback stopped and we have pending variations, apply them now
        if (hasPendingVariations)
        {
            DBG("LooperTrack: Playback stopped, applying pending variations immediately");
            applyVariationsFromFiles(pendingVariationFiles);
            pendingVariationFiles.clear();
            hasPendingVariations = false;
        }
        
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

void LooperTrack::generateButtonClicked()
{
    auto& track = looperEngine.get_track_engine(trackIndex);
    
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

void LooperTrack::setCLEATGainPower(float gainPower)
{
    if (panner != nullptr && pannerType == PannerType::CLEAT)
    {
        if (auto* cleatPanner = dynamic_cast<CLEATPanner*>(panner.get()))
        {
            cleatPanner->set_gain_power(gainPower);
            DBG("LooperTrack: CLEAT gain power set to " + juce::String(gainPower) + " for track " + juce::String(trackIndex));
        }
    }
}

void LooperTrack::setGenerateTriggersNewPath(bool enabled)
{
    generateTriggersNewPath = enabled;
    DBG("LooperTrack: Generate triggers new path set to " + juce::String(enabled ? "true" : "false") + " for track " + juce::String(trackIndex));
}

bool LooperTrack::getPanPosition(float& x, float& y) const
{
    if (panner2DComponent != nullptr)
    {
        // Get smoothed pan position if available
        x = panner2DComponent->get_pan_x();
        y = panner2DComponent->get_pan_y();
        return true;
    }
    else if (pannerType == PannerType::Stereo && panner != nullptr)
    {
        // For stereo panner, get pan value and map to 2D
        if (auto* stereoPanner = dynamic_cast<const StereoPanner*>(panner.get()))
        {
            float panValue = stereoPanner->get_pan();
            x = panValue; // Map to x coordinate (0.0 = left, 1.0 = right)
            y = 0.5f; // Center y position for stereo
            return true;
        }
    }
    
    // No panner available
    x = 0.5f;
    y = 0.5f;
    return false;
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

    auto& track = looperEngine.get_track_engine(trackIndex);
    bool is_playing = track.get_playing();
    
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
        track.set_playing(true);
        track.set_playing(true);
        transportControls.setPlayState(true);
    }
    
    // Check if generate triggers new path is enabled - if so, trigger a random path
    if (generateTriggersNewPath && pathGeneratorButtons != nullptr && panner2DComponent != nullptr)
    {
        DBG("LooperTrack: Generate triggers new path enabled - triggering random path");
        juce::MessageManager::callAsync([this]()
        {
            if (pathGeneratorButtons != nullptr)
            {
                pathGeneratorButtons->triggerRandomPath();
            }
        });
    }
    
    // Note: Autogen is now handled in timerCallback when loop wraps
    // This ensures generation triggers when audio finishes playing, not immediately after completion
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
    auto& track = looperEngine.get_track_engine(trackIndex);
    
    // Stop any ongoing generation
    if (gradioWorkerThread != nullptr)
    {
        gradioWorkerThread->stopThread(1000);
        gradioWorkerThread.reset();
    }
    generateButton.setEnabled(true);
    generateButton.setButtonText("generate");
    
    // Stop playback
    track.set_playing(false);
    track.set_playing(false);
    transportControls.setPlayState(false);
    
    // Clear buffer
    const juce::ScopedLock sl(track.get_buffer_lock());
    track.clear_buffer();
    track.reset();
    track.reset();
    
    // Reset controls to defaults
    cutoffKnob.setValue(4000.0, juce::dontSendNotification); // cutoff (default 4kHz)
    looperEngine.get_track_engine(trackIndex).set_filter_cutoff(4000.0f);
    
    parameterKnobs.setKnobValue(0, 1.0, juce::dontSendNotification); // speed
    track.set_speed(1.0f);
    
    parameterKnobs.setKnobValue(1, 5.0, juce::dontSendNotification); // duration (default 5.0)
    // Reset duration parameter and WrapPos
    auto* obj = customText2SoundParams.getDynamicObject();
    if (obj != nullptr)
    {
        obj->setProperty("duration", juce::var(5.0));
    }
    double sample_rate = track.get_sample_rate();
    if (sample_rate > 0.0)
    {
        track.set_loop_end(static_cast<size_t>(5.0 * sample_rate));
    }
    
    levelControl.setLevelValue(0.0, juce::dontSendNotification);
    track.set_level_db(0.0f);
    
    // Unmute
    track.set_muted(false);
    transportControls.setMuteState(false);
    
    
    // Clear text prompt
    textPromptEditor.clear();
    
    // Reset panner position to center and stop any path playback
    if (panner2DComponent != nullptr)
    {
        panner2DComponent->stop_playback();
        panner2DComponent->set_pan_position(0.5f, 0.5f, juce::sendNotification);
    }
    else if (pannerType == PannerType::Stereo && stereoPanSlider.isVisible())
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
    looperEngine.get_track_engine(trackIndex).set_speed(speed);
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
    else if (pathTypeLower == "hl")
    {
        coords = PanningUtils::generate_horizontal_line_path();
    }
    else if (pathTypeLower == "vl")
    {
        coords = PanningUtils::generate_vertical_line_path();
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
    bool onsetEnabled = onsetToggleEnabled.load();
    bool trajPlaying = trajectoryPlaying.load();
    
    // Debug: Log state periodically (every 10000 samples ~= every 0.2s at 44.1kHz)
    // static int sample_count = 0;
    // sample_count++;
    // if (sample_count % 10000 == 0)
    // {
    //     // DBG("LooperTrack[" + juce::String(trackIndex) + "]: feedAudioSample - onsetEnabled=" + 
    //         (onsetEnabled ? "true" : "false") + ", trajPlaying=" + (trajPlaying ? "true" : "false"));
    // }
    
    if (!onsetEnabled || !trajPlaying)
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
            auto& track = looperEngine.get_track_engine(trackIndex);
            double sample_rate = track.get_sample_rate();
            if (sample_rate <= 0.0)
                sample_rate = 44100.0;
            lastOnsetSampleRate = sample_rate;
            
            // Process block for onset detection
            bool detected = onsetDetector.processBlock(onsetProcessingBuffer.data(), onsetBlockSize, sample_rate);
            
            // Debug: Log loudness periodically to see if we're getting audio
            static int block_count = 0;
            block_count++;
            if (block_count % 100 == 0) // Every 100 blocks (~12.8s at 128 samples/block, 44.1kHz)
            {
                // Calculate loudness manually for debugging
                float sumSquares = 0.0f;
                for (int i = 0; i < onsetBlockSize; ++i)
                {
                    sumSquares += onsetProcessingBuffer[i] * onsetProcessingBuffer[i];
                }
                float rms = std::sqrt(sumSquares / static_cast<float>(onsetBlockSize));
                float peak = 0.0f;
                for (int i = 0; i < onsetBlockSize; ++i)
                {
                    peak = juce::jmax(peak, std::abs(onsetProcessingBuffer[i]));
                }
                // DBG("LooperTrack[" + juce::String(trackIndex) + "]: Audio levels - RMS=" + juce::String(rms, 4) + 
                    // " Peak=" + juce::String(peak, 4) + " Threshold=" + juce::String(onsetDetector.getThreshold(), 4));
            }
            
            if (detected)
            {
                // DBG("LooperTrack[" + juce::String(trackIndex) + "]: Onset detected! (audio thread) - advancing trajectory");
                
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
    auto& track = looperEngine.get_track_engine(trackIndex);
    
    bool modelIsPlaying = track.get_playing();
    transportControls.setPlayState(modelIsPlaying);
    
    // Update cached trajectory playing state (for audio thread access)
    // Only update if state actually changed to avoid excessive logging
    if (panner2DComponent != nullptr)
    {
        bool isPlaying = panner2DComponent->is_playing();
        bool currentStoredState = trajectoryPlaying.load();
        
        // Only update and log if state actually changed
        if (isPlaying != currentStoredState)
        {
            trajectoryPlaying.store(isPlaying);
            DBG("LooperTrack[" + juce::String(trackIndex) + "]: Trajectory playing state changed: " + (isPlaying ? "PLAYING" : "STOPPED"));
        }
    }
    
    // Note: Onset detection is now processed directly in feedAudioSample() from audio thread
    // for low latency. Timer callback only handles LED fade-out.
    
    float current_pos = track.get_pos();
    float loop_end = static_cast<float>(track.get_loop_end());
    bool wrapped = false;
    
    // Detect wrap: if we were near the end and now we're near the start
    if (loop_end > 0.0f)
    {
        float wrap_threshold = loop_end * 0.1f; // 10% threshold
        bool was_near_end = m_last_read_head_position > (loop_end - wrap_threshold);
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
    
    // Check for autogen - trigger new generation when loop wraps (only if not already generating)
    if (autogenToggle.getToggleState() && modelIsPlaying && wrapped && !hasPendingVariations)
    {
        // Only trigger if generate button is enabled (not currently generating)
        if (generateButton.isEnabled())
        {
            DBG("LooperTrack: Autogen enabled - loop wrapped, triggering next generation");
            juce::MessageManager::callAsync([this]()
            {
                generateButtonClicked();
            });
        }
        else
        {
            DBG("LooperTrack: Autogen skipped - generation already in progress");
        }
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
        DBG("LooperTrack[" + juce::String(trackIndex) + "]: handleAsyncUpdate - advancing trajectory");
        if (panner2DComponent != nullptr)
        {
            bool wasPlaying = panner2DComponent->is_playing();
            DBG("LooperTrack[" + juce::String(trackIndex) + "]: panner2DComponent->is_playing()=" + (wasPlaying ? "true" : "false"));
            panner2DComponent->advance_trajectory_onset();
        }
        else
        {
            DBG("LooperTrack[" + juce::String(trackIndex) + "]: ERROR - panner2DComponent is null!");
        }
    }
    
    // Force immediate repaint to show LED
    repaint();
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
    auto& track = looperEngine.get_track_engine(trackIndex);
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
    
    // Get duration from parameter knobs (index 1 is duration)
    double durationSeconds = parameterKnobs.getKnobValue(1);
    
    // Calculate how many samples we need for the desired duration
    double sampleRate = reader->sampleRate;
    if (sampleRate <= 0.0)
        sampleRate = 44100.0; // Default fallback
    
    juce::int64 durationSamples = static_cast<juce::int64>(durationSeconds * sampleRate);
    juce::int64 fileLengthSamples = reader->lengthInSamples;
    
    // Determine how many samples to read (limited by buffer size, file length, and duration)
    juce::int64 numSamplesToRead = juce::jmin(durationSamples, static_cast<juce::int64>(buffer.size()), fileLengthSamples);
    
    if (numSamplesToRead <= 0)
    {
        DBG("Variation file has no samples or duration too small");
        return;
    }
    
    DBG("LooperTrack: Loading variation - duration: " + juce::String(durationSeconds) + "s, " +
        "samples: " + juce::String(numSamplesToRead) + " (file has " + juce::String(fileLengthSamples) + " samples)");
    
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
    
    // Trim trailing silence
    const float silenceThreshold = 0.001f; // -60dB RMS threshold
    const int windowSize = static_cast<int>(sampleRate * 0.01); // 10ms window for RMS calculation
    int actualLength = static_cast<int>(numSamplesToRead);
    
    // Scan backwards from the end to find where audio becomes non-silent
    for (int endPos = actualLength - windowSize; endPos >= windowSize; endPos -= windowSize / 2)
    {
        // Calculate RMS for this window
        float sumSquares = 0.0f;
        int windowStart = juce::jmax(0, endPos - windowSize);
        int windowEnd = juce::jmin(actualLength, endPos);
        int windowSamples = windowEnd - windowStart;
        
        if (windowSamples <= 0)
            break;
        
        for (int i = windowStart; i < windowEnd; ++i)
        {
            float sample = buffer[i];
            sumSquares += sample * sample;
        }
        
        float rms = std::sqrt(sumSquares / static_cast<float>(windowSamples));
        
        // If this window is not silent, we found the end of the audio
        if (rms >= silenceThreshold)
        {
            // Trim to end of this window
            actualLength = windowEnd;
            DBG("LooperTrack: Trimmed trailing silence - original: " + juce::String(numSamplesToRead) + 
                " samples, trimmed: " + juce::String(actualLength) + " samples");
            break;
        }
    }
    
    // Clear any samples beyond the trimmed length
    if (actualLength < static_cast<int>(numSamplesToRead))
    {
        for (int i = actualLength; i < static_cast<int>(numSamplesToRead); ++i)
        {
            buffer[i] = 0.0f;
        }
    }
    
    // Update variation metadata with trimmed length
    size_t loadedLength = static_cast<size_t>(actualLength);
    variation->m_recorded_length.store(loadedLength);
    variation->m_has_recorded.store(true);
    
    DBG("Loaded variation " + juce::String(variationIndex + 1) + " from file: " + audioFile.getFileName() + 
        " (length: " + juce::String(loadedLength) + " samples)");
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
        auto& track = looperEngine.get_track_engine(trackIndex);
        double sample_rate = track.get_sample_rate();
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
    auto& track = looperEngine.get_track_engine(trackIndex);
    auto& trackEngine = looperEngine.get_track_engine(trackIndex);
    
    // Copy variation buffer to active track buffer
    {
        const juce::ScopedLock slVar(variation->m_lock);
        const juce::ScopedLock sl_track(track.get_buffer_lock());
        
        auto& var_buffer = variation->get_buffer();
        auto& track_buffer = track.get_buffer();
        
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
        track.set_recorded_length(copy_length);
        track.set_has_recorded(true);
        
        // Update wrapPos
        track.set_loop_end(copy_length);
        track.set_write_pos(copy_length);
    }
    
    // Reset read head to start
    track.reset();
    track.set_pos(0.0f);
    
    currentVariationIndex = variationIndex;
    variationSelector.setSelectedVariation(variationIndex);
    
    repaint();
    
    DBG("Switched to variation " + juce::String(variationIndex + 1));
}

void LooperTrack::cycleToNextVariation()
{
    if (!autoCycleVariations || variations.empty())
        return;
    
    // Use VariationSelector's method to get next enabled variation
    int nextIndex = variationSelector.getNextEnabledVariation(currentVariationIndex);
    if (nextIndex >= 0)
    {
        switchToVariation(nextIndex);
    }
    // If no enabled variation found, don't cycle (stay on current)
}
