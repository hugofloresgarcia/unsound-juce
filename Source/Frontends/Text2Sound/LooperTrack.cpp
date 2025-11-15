#include "LooperTrack.h"
#include "../Shared/ModelParameterDialog.h"
#include "../Shared/GradioUtilities.h"
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
                    onComplete(saveResult, juce::File(), trackIndex);
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

    // Step 3: Process request on background thread
    // Ensure we have valid params (use defaults if customParams is invalid)
    juce::var paramsToUse = customParams.isObject() ? customParams : Text2Sound::LooperTrack::getDefaultText2SoundParams();
    
    juce::File outputFile;
    auto result = gradioClient.processRequest(tempAudioFile, textPrompt, outputFile, paramsToUse);

    // Notify completion on message thread
    juce::MessageManager::callAsync([this, result, outputFile]()
    {
        if (onComplete)
            onComplete(result, outputFile, trackIndex);
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
      outputSelector(),
      trackLabel("Track", "track " + juce::String(index + 1)),
      resetButton("x"),
      generateButton("generate"),
      textPromptLabel("TextPrompt", "text prompt"),
      autogenToggle("autogen"),
      gradioUrlProvider(std::move(gradioUrlGetter)),
      midiLearnManager(midiManager),
      trackIdPrefix("track" + juce::String(index)),
      pannerType(pannerType),
      stereoPanSlider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox),
      panLabel("pan", "pan"),
      panCoordLabel("coord", "0.50, 0.50")
{
    // Initialize custom params with defaults
    customText2SoundParams = getDefaultText2SoundParams();
    
    // Create parameter dialog (non-modal)
    parameterDialog = std::make_unique<Shared::ModelParameterDialog>(
        "Text2Sound",
        customText2SoundParams,
        [this](const juce::var& newParams) {
            customText2SoundParams = newParams;
            DBG("Text2Sound custom parameters updated");
        }
    );
    
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
    
    // Setup configure params button
    configureParamsButton.setButtonText("configure other model parameters...");
    configureParamsButton.onClick = [this] { configureParamsButtonClicked(); };
    addAndMakeVisible(configureParamsButton);
    
    // Setup text prompt editor
    textPromptEditor.setMultiLine(false);
    textPromptEditor.setReturnKeyStartsNewLine(false);
    textPromptEditor.setTextToShowWhenEmpty("enter text prompt...", juce::Colours::grey);
    addAndMakeVisible(textPromptEditor);
    addAndMakeVisible(textPromptLabel);
    
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
    
    // Setup "autogen" toggle
    autogenToggle.setButtonText("autogen");
    autogenToggle.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(autogenToggle);
    
    // Setup input selector
    inputSelector.onChannelChange = [this](int channel) {
        looperEngine.getTrack(trackIndex).writeHead.setInputChannel(channel);
    };
    addAndMakeVisible(inputSelector);
    
    // Setup output selector
    outputSelector.onChannelChange = [this](int channel) {
        looperEngine.getTrack(trackIndex).readHead.setOutputChannel(channel);
    };
    addAndMakeVisible(outputSelector);
    
    // Initialize channel selectors (will show "all" if device not ready yet)
    // They will be updated again after device is initialized via updateChannelSelectors()
    inputSelector.updateChannels(looperEngine.getAudioDeviceManager());
    outputSelector.updateChannels(looperEngine.getAudioDeviceManager());
    
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
                stereoPanner->setPan(panValue);
                panCoordLabel.setText(juce::String(panValue, 2), juce::dontSendNotification);
            }
        };
        addAndMakeVisible(stereoPanSlider);
    }
    else if (pannerTypeLower == "quad")
    {
        panner = std::make_unique<QuadPanner>();
        panner2DComponent = std::make_unique<Panner2DComponent>();
        panner2DComponent->setPanPosition(0.5f, 0.5f); // Center
        panner2DComponent->onPanChange = [this](float x, float y) {
            if (auto* quadPanner = dynamic_cast<QuadPanner*>(panner.get()))
            {
                quadPanner->setPan(x, y);
                panCoordLabel.setText(juce::String(x, 2) + ", " + juce::String(y, 2), juce::dontSendNotification);
            }
        };
        addAndMakeVisible(panner2DComponent.get());
    }
    else if (pannerTypeLower == "cleat")
    {
        panner = std::make_unique<CLEATPanner>();
        panner2DComponent = std::make_unique<Panner2DComponent>();
        panner2DComponent->setPanPosition(0.5f, 0.5f); // Center
        panner2DComponent->onPanChange = [this](float x, float y) {
            if (auto* cleatPanner = dynamic_cast<CLEATPanner*>(panner.get()))
            {
                cleatPanner->setPan(x, y);
                panCoordLabel.setText(juce::String(x, 2) + ", " + juce::String(y, 2), juce::dontSendNotification);
            }
        };
        addAndMakeVisible(panner2DComponent.get());
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
        configureParamsButton.setLookAndFeel(&laf);
        textPromptEditor.setLookAndFeel(&laf);
        textPromptLabel.setLookAndFeel(&laf);
        autogenToggle.setLookAndFeel(&laf);
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
    
    // Draw MIDI indicator on generate button if mapped
    if (generateButtonLearnable && generateButtonLearnable->hasMidiMapping())
    {
        auto buttonBounds = generateButton.getBounds();
        g.setColour(juce::Colour(0xffed1683));  // Pink
        g.fillEllipse(buttonBounds.getRight() - 8.0f, buttonBounds.getY() + 2.0f, 6.0f, 6.0f);
    }
    
    // Draw arrow between input and output selectors
    const int componentMargin = 5;
    const int trackLabelHeight = 20;
    const int spacingSmall = 5;
    const int channelSelectorHeight = 30;
    
    auto bounds = getLocalBounds().reduced(componentMargin);
    bounds.removeFromTop(trackLabelHeight + spacingSmall);
    auto channelSelectorArea = bounds.removeFromTop(channelSelectorHeight);
    const int selectorWidth = (channelSelectorArea.getWidth() - 40) / 2;
    channelSelectorArea.removeFromLeft(selectorWidth + spacingSmall);
    auto arrowArea = channelSelectorArea.removeFromLeft(40);
    
    g.setColour(juce::Colours::grey);
    g.setFont(juce::Font(14.0f));
    g.drawText("-->", arrowArea, juce::Justification::centred);
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
    const int configureButtonHeight = 30;
    const int channelSelectorHeight = 30;
    const int knobAreaHeight = 140;
    const int controlsHeight = 160;
    
    const int labelHeight = 15;
    const int pannerHeight = 150; // 2D panner height
    const int totalBottomHeight = textPromptHeight + spacingSmall +
                                   channelSelectorHeight + spacingSmall +
                                   knobAreaHeight + spacingSmall + 
                                   controlsHeight + spacingSmall +
                                   generateButtonHeight + spacingSmall + 
                                   configureButtonHeight + spacingSmall +
                                   buttonHeight + spacingSmall +
                                   labelHeight + spacingSmall +
                                   pannerHeight;
    
    auto bounds = getLocalBounds().reduced(componentMargin);
    
    // Track label at top with reset button in top right corner
    auto trackLabelArea = bounds.removeFromTop(trackLabelHeight);
    resetButton.setBounds(trackLabelArea.removeFromRight(resetButtonSize));
    trackLabelArea.removeFromRight(spacingSmall);
    trackLabel.setBounds(trackLabelArea);
    bounds.removeFromTop(spacingSmall);
    
    // Channel selectors: [input] --> [output]
    auto channelSelectorArea = bounds.removeFromTop(channelSelectorHeight);
    const int selectorWidth = (channelSelectorArea.getWidth() - 40) / 2; // Leave space for arrow
    const int arrowWidth = 40;
    
    inputSelector.setBounds(channelSelectorArea.removeFromLeft(selectorWidth));
    channelSelectorArea.removeFromLeft(spacingSmall);
    
    // Draw arrow in the middle
    auto arrowArea = channelSelectorArea.removeFromLeft(arrowWidth);
    
    outputSelector.setBounds(channelSelectorArea.removeFromLeft(selectorWidth));
    bounds.removeFromTop(spacingSmall);
    
    // Reserve space for controls at bottom
    auto bottomArea = bounds.removeFromBottom(totalBottomHeight);
    
    // Waveform area is now the remaining space
    waveformDisplay.setBounds(bounds);
    
    // Text prompt at top of bottom area
    auto textPromptArea = bottomArea.removeFromTop(textPromptHeight);
    const int textPromptLabelWidth = 100;
    textPromptLabel.setBounds(textPromptArea.removeFromLeft(textPromptLabelWidth));
    textPromptArea.removeFromLeft(spacingSmall);
    textPromptEditor.setBounds(textPromptArea);
    bottomArea.removeFromTop(spacingSmall);
    
    // Knobs area
    auto knobArea = bottomArea.removeFromTop(knobAreaHeight);
    parameterKnobs.setBounds(knobArea);
    bottomArea.removeFromTop(spacingSmall);
    
    // Level control and VU meter with autogen toggle
    auto controlsArea = bottomArea.removeFromTop(controlsHeight);
    levelControl.setBounds(controlsArea.removeFromLeft(115)); // 80 + 5 + 30
    controlsArea.removeFromLeft(spacingSmall);
    autogenToggle.setBounds(controlsArea.removeFromLeft(100)); // Toggle button width
    bottomArea.removeFromTop(spacingSmall);
    
    // Generate button
    generateButton.setBounds(bottomArea.removeFromTop(generateButtonHeight));
    bottomArea.removeFromTop(spacingSmall);
    
    // Configure params button
    configureParamsButton.setBounds(bottomArea.removeFromTop(configureButtonHeight));
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

void LooperTrack::generateButtonClicked()
{
    auto& track = looperEngine.getTrack(trackIndex);
    
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

    // Determine if we have audio - if not, pass empty File (will be treated as null)
    juce::File audioFile;
    if (track.tapeLoop.hasRecorded.load())
    {
        // We have audio - pass a sentinel file (the worker thread will save the buffer)
        audioFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("has_audio");
        DBG("LooperTrack: Has audio - passing sentinel file: " + audioFile.getFullPathName());
    }
    else
    {
        DBG("LooperTrack: No audio - passing empty file");
    }

    // Create and start background worker thread
    gradioWorkerThread = std::make_unique<GradioWorkerThread>(looperEngine,
                                                              trackIndex,
                                                              audioFile,
                                                              textPrompt,
                                                              customText2SoundParams,
                                                              gradioUrlProvider);
    gradioWorkerThread->onComplete = [this](juce::Result result, juce::File outputFile, int trackIdx)
    {
        onGradioComplete(result, outputFile);
    };
    
    gradioWorkerThread->startThread();
}

void LooperTrack::configureParamsButtonClicked()
{
    if (parameterDialog != nullptr)
    {
        // Update the dialog with current params in case they changed
        parameterDialog->updateParams(customText2SoundParams);
        
        // Show the dialog (non-modal)
        parameterDialog->setVisible(true);
        parameterDialog->toFront(true);
    }
}

juce::var LooperTrack::getDefaultText2SoundParams()
{
    // Create default parameters object (excluding text prompt and audio which are in UI)
    juce::DynamicObject::Ptr params = new juce::DynamicObject();
    
    // New API parameters (indices 2-6):
    params->setProperty("seed", juce::var(3));                          // [2] seed (number, 0 or empty for random)
    params->setProperty("median_filter_length", juce::var(0));          // [3] median filter length (0 for none)
    params->setProperty("normalize_db", juce::var(-24));                // [4] normalize dB (0 for none)
    params->setProperty("duration", juce::var(0));                      // [5] duration in seconds (0 for auto)
    
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

void LooperTrack::onGradioComplete(juce::Result result, juce::File outputFile)
{
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

    // Load the generated audio back into the track
    auto& trackEngine = looperEngine.getTrackEngine(trackIndex);
    
    if (trackEngine.loadFromFile(outputFile))
    {
        repaint(); // Refresh waveform display
        
        // Check if autogen is enabled - if so, automatically trigger next generation
        if (autogenToggle.getToggleState())
        {
            DBG("LooperTrack: Autogen enabled - automatically triggering next generation");
            // Use MessageManager::callAsync to ensure the UI updates and the file is fully loaded
            juce::MessageManager::callAsync([this]()
            {
                generateButtonClicked();
            });
        }
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                              "load failed",
                                              "generated audio saved to: " + outputFile.getFullPathName() + "\n"
                                              "but failed to load it into the track.");
    }
}

void LooperTrack::resetButtonClicked()
{
    auto& track = looperEngine.getTrack(trackIndex);
    
    // Stop any ongoing generation
    if (gradioWorkerThread != nullptr)
    {
        gradioWorkerThread->stopThread(1000);
        gradioWorkerThread.reset();
    }
    generateButton.setEnabled(true);
    generateButton.setButtonText("generate");
    
    // Stop playback
    track.isPlaying.store(false);
    track.readHead.setPlaying(false);
    transportControls.setPlayState(false);
    
    // Disable recording
    track.writeHead.setRecordEnable(false);
    transportControls.setRecordState(false);
    
    // Clear buffer
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
    
    // Clear text prompt
    textPromptEditor.clear();
    
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

void LooperTrack::updateChannelSelectors()
{
    // Update channel selectors based on current audio device
    inputSelector.updateChannels(looperEngine.getAudioDeviceManager());
    outputSelector.updateChannels(looperEngine.getAudioDeviceManager());
}
