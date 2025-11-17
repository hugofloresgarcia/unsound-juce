#include "LooperTrack.h"
#include <flowerjuce/Components/ModelParameterDialog.h>
#include <flowerjuce/Components/GradioUtilities.h>
#include <juce_audio_formats/juce_audio_formats.h>

using namespace WhAM;

// VampNetWorkerThread implementation
void VampNetWorkerThread::run()
{
    juce::File tempAudioFile;
    juce::Result saveResult = juce::Result::ok();
    
    bool isSentinel = audioFile.getFileName() == "has_audio";
    
    if (isSentinel)
    {
        DBG("VampNetWorkerThread: Saving input audio to file");
        saveResult = saveBufferToFile(trackIndex, tempAudioFile);
        DBG("VampNetWorkerThread: Save result: " + saveResult.getErrorMessage());
        
        if (saveResult.failed())
        {
            DBG("VampNetWorkerThread: Save failed: " + saveResult.getErrorMessage());
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
        tempAudioFile = juce::File();
    }

    // Call VampNet API
    juce::File outputFile;
    auto result = callVampNetAPI(tempAudioFile, periodicPrompt, customParams, outputFile);

    juce::MessageManager::callAsync([this, result, outputFile]()
    {
        if (onComplete)
            onComplete(result, outputFile, trackIndex);
    });
}

juce::Result VampNetWorkerThread::saveBufferToFile(int trackIndex, juce::File& outputFile)
{
    if (useOutputBuffer)
    {
        return Shared::saveVampNetOutputBufferToWavFile(looperEngine, trackIndex, outputFile, "vampnet_input");
    }
    else
    {
        return Shared::saveTrackBufferToWavFile(looperEngine, trackIndex, outputFile, "vampnet_input");
    }
}

juce::Result VampNetWorkerThread::callVampNetAPI(const juce::File& inputAudioFile, float periodicPrompt, const juce::var& customParams, juce::File& outputFile)
{
    // VampNet API endpoint
    const juce::String defaultUrl = "https://hugggof-vampnet-music.hf.space/";
    juce::String configuredUrl = defaultUrl;

    if (gradioUrlProvider)
    {
        juce::String providedUrl = gradioUrlProvider();
        if (providedUrl.isNotEmpty())
            configuredUrl = providedUrl;
    }

    juce::URL gradioEndpoint(configuredUrl);
    
    // Step 1: Upload input audio file if provided
    juce::String uploadedFilePath;
    bool hasAudio = inputAudioFile != juce::File() && inputAudioFile.existsAsFile();
    
    if (hasAudio)
    {
        auto uploadResult = Shared::uploadFileToGradio(configuredUrl, inputAudioFile, uploadedFilePath);
        if (uploadResult.failed())
            return juce::Result::fail("Failed to upload audio file: " + uploadResult.getErrorMessage());
        
        DBG("VampNetWorkerThread: File uploaded successfully. Path: " + uploadedFilePath);
    }

    // Step 2: Prepare JSON payload with all 18 parameters
    juce::Array<juce::var> dataItems;
    
    // [0] Input audio file
    if (hasAudio)
    {
        juce::DynamicObject::Ptr fileObj = new juce::DynamicObject();
        fileObj->setProperty("path", juce::var(uploadedFilePath));
        
        juce::DynamicObject::Ptr metaObj = new juce::DynamicObject();
        metaObj->setProperty("_type", juce::var("gradio.FileData"));
        fileObj->setProperty("meta", juce::var(metaObj));
        
        dataItems.add(juce::var(fileObj));
    }
    else
    {
        dataItems.add(juce::var());  // null for no audio
    }
    
    // VampNet parameters - use custom params if provided, otherwise use defaults
    juce::var paramsToUse = customParams.isObject() ? customParams : WhAM::LooperTrack::getDefaultVampNetParams();
    
    auto* obj = paramsToUse.getDynamicObject();
    if (obj != nullptr)
    {
        dataItems.add(obj->getProperty("sample_temperature"));       // [1]
        dataItems.add(obj->getProperty("top_p"));                   // [2]
        dataItems.add(juce::var(static_cast<int>(periodicPrompt))); // [3] periodic prompt (from UI) - force convert to int
        dataItems.add(obj->getProperty("mask_dropout"));            // [4]
        dataItems.add(obj->getProperty("time_stretch_factor"));     // [5]
        dataItems.add(obj->getProperty("onset_mask_width"));        // [6]
        dataItems.add(obj->getProperty("typical_filtering"));       // [7]
        dataItems.add(obj->getProperty("typical_mass"));            // [8]
        dataItems.add(obj->getProperty("typical_min_tokens"));      // [9]
        dataItems.add(obj->getProperty("seed"));                    // [10]
        dataItems.add(obj->getProperty("model_choice"));            // [11]
        dataItems.add(obj->getProperty("compression_prompt"));      // [12]
        dataItems.add(obj->getProperty("pitch_shift_amount"));      // [13]
        dataItems.add(obj->getProperty("sample_cutoff"));           // [14]
        dataItems.add(obj->getProperty("sampling_steps"));          // [15]
        dataItems.add(obj->getProperty("beat_mask_width"));         // [16]
        dataItems.add(obj->getProperty("feedback_steps"));          // [17]
    }
    
    juce::DynamicObject::Ptr payloadObj = new juce::DynamicObject();
    payloadObj->setProperty("data", juce::var(dataItems));
    
    juce::String jsonBody = juce::JSON::toString(juce::var(payloadObj), false);
    
    DBG("VampNetWorkerThread: POST payload: " + jsonBody);

    // Step 3: Make POST request to get event ID
    juce::URL requestEndpoint = gradioEndpoint.getChildURL("gradio_api")
                                       .getChildURL("call")
                                       .getChildURL("vamp");

    // Print curl equivalent for POST request to get event ID
    DBG("=== CURL EQUIVALENT FOR EVENT ID REQUEST ===");
    DBG("curl -X POST \\");
    DBG("  -H \"Content-Type: application/json\" \\");
    DBG("  -H \"User-Agent: JUCE-VampNet/1.0\" \\");
    DBG("  -d '" + jsonBody + "' \\");
    DBG("  \"" + requestEndpoint.toString(false) + "\"");
    DBG("============================================");

    juce::URL postEndpoint = requestEndpoint.withPOSTData(jsonBody);

    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders("Content-Type: application/json\r\nUser-Agent: JUCE-VampNet/1.0\r\n")
                       .withConnectionTimeoutMs(30000)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5)
                       .withHttpRequestCmd("POST");

    std::unique_ptr<juce::InputStream> stream(postEndpoint.createInputStream(options));

    DBG("VampNetWorkerThread: POST request status code: " + juce::String(statusCode));

    if (stream == nullptr || statusCode != 200)
        return juce::Result::fail("Failed to make POST request. Status: " + juce::String(statusCode));

    juce::String response = stream->readEntireStreamAsString();
    DBG("VampNetWorkerThread: POST response: " + response);

    juce::var parsedResponse;
    auto parseResult = juce::JSON::parse(response, parsedResponse);
    if (parseResult.failed() || !parsedResponse.isObject())
        return juce::Result::fail("Failed to parse POST response: " + parseResult.getErrorMessage() + "\nResponse was: " + response);

    juce::DynamicObject* responseObj = parsedResponse.getDynamicObject();
    if (responseObj == nullptr || !responseObj->hasProperty("event_id"))
    {
        DBG("VampNetWorkerThread: Response object properties:");
        if (responseObj != nullptr)
        {
            auto& props = responseObj->getProperties();
            for (int i = 0; i < props.size(); ++i)
                DBG("  " + props.getName(i).toString() + ": " + props.getValueAt(i).toString());
        }
        return juce::Result::fail("Response does not contain 'event_id'");
    }

    juce::String eventID = responseObj->getProperty("event_id").toString();
    if (eventID.isEmpty())
        return juce::Result::fail("event_id is empty");

    DBG("VampNetWorkerThread: Got event ID: " + eventID);

    // Step 4: Poll for response
    juce::URL getEndpoint = gradioEndpoint.getChildURL("gradio_api")
                                  .getChildURL("call")
                                  .getChildURL("vamp")
                                  .getChildURL(eventID);

    // Print curl equivalent for polling request
    DBG("=== CURL EQUIVALENT FOR POLLING REQUEST ===");
    DBG("curl -N \\");
    DBG("  -H \"Accept: text/event-stream\" \\");
    DBG("  -H \"Cache-Control: no-cache\" \\");
    DBG("  -H \"Connection: keep-alive\" \\");
    DBG("  \"" + getEndpoint.toString(false) + "\"");
    DBG("===========================================");

    juce::StringPairArray getResponseHeaders;
    int getStatusCode = 0;
    
    // Match curl's default headers for SSE streaming
    juce::String sseHeaders = "Accept: text/event-stream\r\n"
                              "Cache-Control: no-cache\r\n"
                              "Connection: keep-alive\r\n";
    
    auto getOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(sseHeaders)
                       .withConnectionTimeoutMs(120000)  // 2 minute timeout for generation
                       .withResponseHeaders(&getResponseHeaders)
                       .withStatusCode(&getStatusCode)
                       .withNumRedirectsToFollow(5)
                       .withHttpRequestCmd("GET");

    DBG("VampNetWorkerThread: Creating streaming connection...");
    std::unique_ptr<juce::InputStream> getStream(getEndpoint.createInputStream(getOptions));
    
    DBG("VampNetWorkerThread: Status code: " + juce::String(getStatusCode));
    
    // Log response headers
    DBG("VampNetWorkerThread: Response headers:");
    for (int i = 0; i < getResponseHeaders.size(); ++i)
    {
        DBG("  " + getResponseHeaders.getAllKeys()[i] + ": " + getResponseHeaders.getAllValues()[i]);
    }

    if (getStream == nullptr)
        return juce::Result::fail("Failed to create GET stream. Status code: " + juce::String(getStatusCode));
    
    // Check if we got a valid status code
    if (getStatusCode != 0 && getStatusCode != 200)
    {
        DBG("VampNetWorkerThread: Non-200 status code: " + juce::String(getStatusCode));
        // Don't fail immediately - SSE might still work
    }

    // Use shared SSE parsing utility
    juce::String eventResponse;
    auto sseParseResult = Shared::parseSSEStream(getStream.get(), eventResponse, 
        [this]() { return threadShouldExit(); });
    
    if (sseParseResult.failed())
        return sseParseResult;

    // Step 5: Extract data from response
    if (!eventResponse.contains("data:"))
        return juce::Result::fail("Response does not contain 'data:'");

    juce::String responseData = eventResponse.fromFirstOccurrenceOf("data:", false, false).trim();

    juce::var parsedData;
    parseResult = juce::JSON::parse(responseData, parsedData);
    if (parseResult.failed() || !parsedData.isArray())
        return juce::Result::fail("Failed to parse response data");

    juce::Array<juce::var>* dataArray = parsedData.getArray();
    if (dataArray == nullptr || dataArray->isEmpty())
        return juce::Result::fail("Data array is empty");

    // VampNet returns 3 elements: [output_audio_1, output_audio_2, mask_image]
    // We'll use the first audio output
    juce::var firstElement = dataArray->getFirst();
    if (!firstElement.isObject())
        return juce::Result::fail("First element is not an object");

    juce::DynamicObject* outputObj = firstElement.getDynamicObject();
    if (outputObj == nullptr || !outputObj->hasProperty("url"))
        return juce::Result::fail("Output object does not have 'url' property");

    juce::String fileURL = outputObj->getProperty("url").toString();
    DBG("VampNetWorkerThread: Output file URL: " + fileURL);

    // Step 6: Download the output file
    juce::URL outputURL(fileURL);
    auto downloadResult = Shared::downloadFileFromURL(outputURL, outputFile);
    if (downloadResult.failed())
        return juce::Result::fail("Failed to download output file: " + downloadResult.getErrorMessage());

    DBG("VampNetWorkerThread: File downloaded to: " + outputFile.getFullPathName());
    return juce::Result::ok();
}

// LooperTrack implementation
LooperTrack::LooperTrack(VampNetMultiTrackLooperEngine& engine, int index, std::function<juce::String()> gradioUrlGetter, Shared::MidiLearnManager* midiManager, const juce::String& pannerType)
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
      useOutputAsInputToggle("use o as i"),
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
    customVampNetParams = getDefaultVampNetParams();
    
    // Create parameter dialog (non-modal)
    parameterDialog = std::make_unique<Shared::ModelParameterDialog>(
        "VampNet",
        customVampNetParams,
        [this](const juce::var& newParams) {
            customVampNetParams = newParams;
            DBG("VampNet custom parameters updated");
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
    
    // Setup waveform display
    addAndMakeVisible(waveformDisplay);
    
    // Setup transport controls
    transportControls.onRecordToggle = [this](bool enabled) { recordEnableButtonToggled(enabled); };
    transportControls.onPlayToggle = [this](bool shouldPlay) { playButtonClicked(shouldPlay); };
    transportControls.onMuteToggle = [this](bool muted) { muteButtonToggled(muted); };
    transportControls.onReset = [this]() { resetButtonClicked(); };
    addAndMakeVisible(transportControls);
    
    // Setup parameter knobs (speed, overdub, periodic prompt, dry/wet)
    parameterKnobs.addKnob({
        "speed",
        0.25, 4.0, 1.0, 0.01,
        "x",
        [this](double value) {
            auto& track = looperEngine.getTrack(trackIndex);
            track.recordReadHead.set_speed(static_cast<float>(value));
            track.outputReadHead.set_speed(static_cast<float>(value));
        },
        ""  // parameterId - will be auto-generated
    });
    
    parameterKnobs.addKnob({
        "overdub",
        0.0, 1.0, 0.5, 0.01,
        "",
        [this](double value) {
            looperEngine.getTrack(trackIndex).writeHead.set_overdub_mix(static_cast<float>(value));
        },
        ""  // parameterId - will be auto-generated
    });
    
    parameterKnobs.addKnob({
        "periodic prompt",
        1.0, 23.0, 8.0, 1.0,
        "",
        [this](double value) {
            // Value is stored in the knob, retrieved when generating
        },
        ""  // parameterId - will be auto-generated
    });
    
    parameterKnobs.addKnob({
        "dry/wet",
        0.0, 1.0, 0.5, 0.01,
        "",
        [this](double value) {
            looperEngine.getTrack(trackIndex).dryWetMix.store(static_cast<float>(value));
        },
        ""  // parameterId - will be auto-generated
    });
    addAndMakeVisible(parameterKnobs);
    
    // Setup level control (applies to both read heads)
    levelControl.onLevelChange = [this](double value) {
        auto& track = looperEngine.getTrack(trackIndex);
        track.recordReadHead.set_level_db(static_cast<float>(value));
        track.outputReadHead.set_level_db(static_cast<float>(value));
    };
    addAndMakeVisible(levelControl);
    
    // Setup "use o as i" toggle
    useOutputAsInputToggle.setButtonText("use o as i");
    useOutputAsInputToggle.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(useOutputAsInputToggle);
    
    // Setup "autogen" toggle
    autogenToggle.setButtonText("autogen");
    autogenToggle.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(autogenToggle);
    
    // Setup input selector
    inputSelector.onChannelChange = [this](int channel) {
        looperEngine.getTrack(trackIndex).writeHead.set_input_channel(channel);
    };
    addAndMakeVisible(inputSelector);
    
    // Setup output selector (applies to both read heads)
    outputSelector.onChannelChange = [this](int channel) {
        auto& track = looperEngine.getTrack(trackIndex);
        track.recordReadHead.set_output_channel(channel);
        track.outputReadHead.set_output_channel(channel);
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
        useOutputAsInputToggle.setLookAndFeel(&laf);
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
    if (track.writeHead.get_record_enable())
    {
        g.setColour(juce::Colour(0xfff04e36).withAlpha(0.2f)); // Red-orange
        g.fillRect(getLocalBounds());
    }
    else if (track.isPlaying.load() && track.recordBuffer.m_has_recorded.load())
    {
        g.setColour(juce::Colour(0xff1eb19d).withAlpha(0.15f)); // Teal
        g.fillRect(getLocalBounds());
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
    const int buttonHeight = 30;
    const int generateButtonHeight = 30;
    const int configureButtonHeight = 30;
    const int channelSelectorHeight = 30;
    const int knobAreaHeight = 140;
    const int controlsHeight = 160;
    
    const int labelHeight = 15;
    const int pannerHeight = 150; // 2D panner height
    const int totalBottomHeight = channelSelectorHeight + spacingSmall +
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
    
    // Knobs area (now includes periodic prompt)
    auto knobArea = bottomArea.removeFromTop(knobAreaHeight);
    parameterKnobs.setBounds(knobArea);
    bottomArea.removeFromTop(spacingSmall);
    
    // Level control and VU meter with toggles
    auto controlsArea = bottomArea.removeFromTop(controlsHeight);
    levelControl.setBounds(controlsArea.removeFromLeft(115)); // 80 + 5 + 30
    controlsArea.removeFromLeft(spacingSmall);
    // Stack toggles vertically: autogen on top, use o as i below
    auto toggleArea = controlsArea.removeFromLeft(100); // Toggle button width
    autogenToggle.setBounds(toggleArea.removeFromTop(30)); // First toggle
    toggleArea.removeFromTop(spacingSmall);
    useOutputAsInputToggle.setBounds(toggleArea.removeFromTop(30)); // Second toggle
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
    DBG("LooperTrack: recordEnableButtonToggled: enabled=" + juce::String(enabled ? "YES" : "NO"));
    track.writeHead.set_record_enable(enabled);
    repaint();
}

void LooperTrack::playButtonClicked(bool shouldPlay)
{
    auto& track = looperEngine.getTrack(trackIndex);
    
    if (shouldPlay)
    {
        track.isPlaying.store(true);
        track.recordReadHead.set_playing(true);
        track.outputReadHead.set_playing(true);
        
        if (track.writeHead.get_record_enable() && !track.recordBuffer.m_has_recorded.load())
        {
            const juce::ScopedLock sl(track.recordBuffer.m_lock);
            track.recordBuffer.clear_buffer();
            track.writeHead.reset();
            track.recordReadHead.reset();
            track.outputReadHead.reset();
        }
    }
    else
    {
        track.isPlaying.store(false);
        track.recordReadHead.set_playing(false);
        track.outputReadHead.set_playing(false);
        if (track.writeHead.get_record_enable())
        {
            track.writeHead.finalize_recording(track.writeHead.get_pos());
            juce::Logger::writeToLog("~~~ Playback just stopped, finalized recording");
        }
    }
    
    repaint();
}

void LooperTrack::muteButtonToggled(bool muted)
{
    auto& track = looperEngine.getTrack(trackIndex);
    track.recordReadHead.set_muted(muted);
    track.outputReadHead.set_muted(muted);
}

void LooperTrack::generateButtonClicked()
{
    auto& track = looperEngine.getTrack(trackIndex);
    
    // Get periodic prompt value from knob
    float periodicPrompt = static_cast<float>(getPeriodicPrompt());

    DBG("LooperTrack: Starting VampNet generation with periodic prompt: " + juce::String(periodicPrompt));

    // Stop any existing worker thread
    if (vampNetWorkerThread != nullptr)
    {
        vampNetWorkerThread->stopThread(1000);
        vampNetWorkerThread.reset();
    }

    // Disable generate button during processing
    generateButton.setEnabled(false);
    generateButton.setButtonText("generating...");

    // Check if we should use output buffer as input
    bool useOutputAsInput = useOutputAsInputToggle.getToggleState();
    
    // Determine if we have audio (check appropriate buffer based on toggle)
    juce::File audioFile;
    bool hasAudio = false;
    if (useOutputAsInput)
    {
        hasAudio = track.outputBuffer.m_has_recorded.load();
        DBG("LooperTrack: Using output buffer as input, hasAudio=" + juce::String(hasAudio ? "YES" : "NO"));
    }
    else
    {
        hasAudio = track.recordBuffer.m_has_recorded.load();
        DBG("LooperTrack: Using record buffer as input, hasAudio=" + juce::String(hasAudio ? "YES" : "NO"));
    }
    
    if (hasAudio)
    {
        audioFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("has_audio");
        DBG("LooperTrack: Has audio - passing sentinel file");
    }
    else
    {
        DBG("LooperTrack: No audio - passing empty file");
    }

    // Create and start background worker thread
    vampNetWorkerThread = std::make_unique<VampNetWorkerThread>(looperEngine,
                                                                trackIndex,
                                                                audioFile,
                                                                periodicPrompt,
                                                                customVampNetParams,
                                                                gradioUrlProvider,
                                                                useOutputAsInput);
    vampNetWorkerThread->onComplete = [this](juce::Result result, juce::File outputFile, int trackIdx)
    {
        onVampNetComplete(result, outputFile);
    };
    
    vampNetWorkerThread->startThread();
}

void LooperTrack::configureParamsButtonClicked()
{
    if (parameterDialog != nullptr)
    {
        // Update the dialog with current params in case they changed
        parameterDialog->updateParams(customVampNetParams);
        
        // Show the dialog (non-modal)
        parameterDialog->setVisible(true);
        parameterDialog->toFront(true);
    }
}

juce::var LooperTrack::getDefaultVampNetParams()
{
    // Create default parameters object (excluding periodic_prompt which is in UI)
    juce::DynamicObject::Ptr params = new juce::DynamicObject();
    
    params->setProperty("sample_temperature", juce::var(1.0));
    params->setProperty("top_p", juce::var(0));
    params->setProperty("mask_dropout", juce::var(0));
    params->setProperty("time_stretch_factor", juce::var(1));
    params->setProperty("onset_mask_width", juce::var(0));
    params->setProperty("typical_filtering", juce::var(true));
    params->setProperty("typical_mass", juce::var(0.15));
    params->setProperty("typical_min_tokens", juce::var(64));
    params->setProperty("seed", juce::var(0));
    params->setProperty("model_choice", juce::var("default"));
    params->setProperty("compression_prompt", juce::var(3));
    params->setProperty("pitch_shift_amount", juce::var(0));
    params->setProperty("sample_cutoff", juce::var(0.9));
    params->setProperty("sampling_steps", juce::var(12));
    params->setProperty("beat_mask_width", juce::var(0));
    params->setProperty("feedback_steps", juce::var(1));
    
    return juce::var(params);
}

void LooperTrack::onVampNetComplete(juce::Result result, juce::File outputFile)
{
    // Re-enable button
    generateButton.setEnabled(true);
    generateButton.setButtonText("generate");

    // Clean up worker thread
    if (vampNetWorkerThread != nullptr)
    {
        vampNetWorkerThread->stopThread(1000);
        vampNetWorkerThread.reset();
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
            // Use a short delay to ensure the UI updates and the file is fully loaded
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
    if (vampNetWorkerThread != nullptr)
    {
        vampNetWorkerThread->stopThread(1000);
        vampNetWorkerThread.reset();
    }
    generateButton.setEnabled(true);
    generateButton.setButtonText("generate");
    
    // Stop playback
    track.isPlaying.store(false);
    track.recordReadHead.set_playing(false);
    track.outputReadHead.set_playing(false);
    transportControls.setPlayState(false);
    
    // Disable recording
    track.writeHead.set_record_enable(false);
    transportControls.setRecordState(false);
    
    // Clear both buffers
    const juce::ScopedLock sl1(track.recordBuffer.m_lock);
    const juce::ScopedLock sl2(track.outputBuffer.m_lock);
    track.recordBuffer.clear_buffer();
    track.outputBuffer.clear_buffer();
    track.writeHead.reset();
    track.recordReadHead.reset();
    track.outputReadHead.reset();
    
    // Reset controls to defaults
    parameterKnobs.setKnobValue(0, 1.0, juce::dontSendNotification); // speed
    track.recordReadHead.set_speed(1.0f);
    track.outputReadHead.set_speed(1.0f);
    
    parameterKnobs.setKnobValue(1, 0.5, juce::dontSendNotification); // overdub
    track.writeHead.set_overdub_mix(0.5f);
    
    parameterKnobs.setKnobValue(2, 8.0, juce::dontSendNotification); // periodic prompt
    
    parameterKnobs.setKnobValue(3, 0.5, juce::dontSendNotification); // dry/wet
    track.dryWetMix.store(0.5f);
    
    levelControl.setLevelValue(0.0, juce::dontSendNotification);
    track.recordReadHead.set_level_db(0.0f);
    track.outputReadHead.set_level_db(0.0f);
    
    // Unmute
    track.recordReadHead.set_muted(false);
    track.outputReadHead.set_muted(false);
    transportControls.setMuteState(false);
    
    // Reset output channel to all
    outputSelector.setSelectedChannel(1, juce::dontSendNotification);
    track.recordReadHead.set_output_channel(-1);
    track.outputReadHead.set_output_channel(-1);
    
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
    if (vampNetWorkerThread != nullptr)
    {
        vampNetWorkerThread->stopThread(5000); // Wait up to 5 seconds
        vampNetWorkerThread.reset();
    }
}

void LooperTrack::setPlaybackSpeed(float speed)
{
    parameterKnobs.setKnobValue(0, speed, juce::dontSendNotification);
    auto& track = looperEngine.getTrack(trackIndex);
    track.recordReadHead.set_speed(speed);
    track.outputReadHead.set_speed(speed);
}

float LooperTrack::getPlaybackSpeed() const
{
    return static_cast<float>(parameterKnobs.getKnobValue(0));
}

float LooperTrack::getPeriodicPrompt() const
{
    return static_cast<float>(parameterKnobs.getKnobValue(2));
}

void LooperTrack::timerCallback()
{
    // Sync button states with model state
    auto& track = looperEngine.getTrack(trackIndex);
    
    bool modelRecordEnable = track.writeHead.get_record_enable();
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

bool LooperTrack::isGenerating() const
{
    return vampNetWorkerThread != nullptr && vampNetWorkerThread->isThreadRunning();
}

void LooperTrack::triggerGeneration()
{
    // Only trigger if not already generating and button is enabled
    if (!isGenerating() && generateButton.isEnabled())
    {
        generateButtonClicked();
    }
}

