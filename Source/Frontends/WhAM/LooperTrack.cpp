#include "LooperTrack.h"
#include "../Shared/GradioUtilities.h"
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

    modelParamsPopup = std::make_unique<ModelParamsPopup>(midiManager, trackIdPrefix);
    modelParamsPopup->onDismissed = [this]()
    {
        configureParamsButton.setToggleState(false, juce::dontSendNotification);
    };

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
    resetButton.onLeftClick = [this] { resetButtonClicked(); };
    addAndMakeVisible(resetButton);

    // Setup generate button
    generateButton.onClick = [this] { generateButtonClicked(); };
    addAndMakeVisible(generateButton);

    // Setup MIDI learn for generate button
    if (midiLearnManager)
    {
        generateButtonLearnable = std::make_unique<Shared::MidiLearnable>(*midiLearnManager, trackIdPrefix + "_generate", true);

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

        resetButtonLearnable = std::make_unique<Shared::MidiLearnable>(*midiLearnManager, trackIdPrefix + "_clear", true);
        resetButtonMouseListener = std::make_unique<Shared::MidiLearnMouseListener>(*resetButtonLearnable, this);
        resetButton.addMouseListener(resetButtonMouseListener.get(), false);

        midiLearnManager->registerParameter({
            trackIdPrefix + "_clear",
            [this](float value) {
                if (value > 0.5f)
                    resetButtonClicked();
            },
            []() { return 0.0f; },
            trackIdPrefix + " Clear",
            true
        });
    }

    // Setup configure params button
    configureParamsButton.setButtonText("model params...");
    configureParamsButton.setClickingTogglesState(true);
    configureParamsButton.setToggleState(false, juce::dontSendNotification);
    configureParamsButton.onClick = [this] { configureParamsButtonClicked(); };
    addAndMakeVisible(configureParamsButton);

    // Setup waveform display
    addAndMakeVisible(waveformDisplay);

    // Setup transport controls
    transportControls.onRecordToggle = [this](bool enabled) { recordEnableButtonToggled(enabled); };
    transportControls.onPlayToggle = [this](bool shouldPlay) { playButtonClicked(shouldPlay); };
    transportControls.onMuteToggle = [this](bool muted) { muteButtonToggled(muted); };
    transportControls.onMicToggle = [this](bool enabled) { setMicEnabled(enabled); };
    transportControls.onReset = [this]() { resetButtonClicked(); };
    addAndMakeVisible(transportControls);

    // WhAM uses the mic button; it should be visible. Availability will track input channels.
    transportControls.setMicButtonVisible(true);

    // Setup parameter knobs (speed, overdub, dry/wet)
    parameterKnobs.addKnob({
        "speed",
        0.25, 4.0, 1.0, 0.01,
        "x",
        [this](double value) {
            auto& track = looperEngine.getTrack(trackIndex);
            track.recordReadHead.setSpeed(static_cast<float>(value));
            track.outputReadHead.setSpeed(static_cast<float>(value));
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
    initializeModelParameterKnobs();
    syncCustomParamsToKnobs();

    // Setup level control (applies to both read heads)
    levelControl.onLevelChange = [this](double value) {
        auto& track = looperEngine.getTrack(trackIndex);
        track.recordReadHead.setLevelDb(static_cast<float>(value));
        track.outputReadHead.setLevelDb(static_cast<float>(value));
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
        looperEngine.getTrack(trackIndex).writeHead.setInputChannel(channel);
    };
    addAndMakeVisible(inputSelector);

    // Setup output selector (applies to both read heads)
    outputSelector.onChannelChange = [this](int channel) {
        auto& track = looperEngine.getTrack(trackIndex);
        track.recordReadHead.setOutputChannel(channel);
        track.outputReadHead.setOutputChannel(channel);
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
    if (track.writeHead.getRecordEnable())
    {
        g.setColour(juce::Colour(0xfff04e36).withAlpha(0.2f)); // Red-orange
        g.fillRect(getLocalBounds());
    }
    else if (track.isPlaying.load() && track.recordBuffer.hasRecorded.load())
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
    const int controlsHeight = 160;

    const int labelHeight = 15;
    const int stereoPannerHeight = 60;
    const int panner2DHeight = 150;

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

    // Leave a spacer before the waveform/controls block
    bounds.removeFromTop(spacingSmall);

    // Remaining area now contains waveform + lower controls
    auto remainingArea = bounds;

    const int knobAreaHeight = parameterKnobs.getRequiredHeight(remainingArea.getWidth());
    auto removeBottomSpacing = [&]() {
        if (remainingArea.getHeight() > spacingSmall)
            remainingArea.removeFromBottom(spacingSmall);
    };

    // Layout panner (bottom-most)
    auto typeLower = pannerType.toLowerCase();
    bool hasStereoPanner = (typeLower == "stereo" && stereoPanSlider.isVisible());
    bool has2DPanner = ((typeLower == "quad" || typeLower == "cleat") && panner2DComponent != nullptr && panner2DComponent->isVisible());

    if (panner != nullptr && (hasStereoPanner || has2DPanner))
    {
        int desiredHeight = hasStereoPanner ? stereoPannerHeight : panner2DHeight;
        auto pannerArea = remainingArea.removeFromBottom(desiredHeight);
        if (hasStereoPanner)
            stereoPanSlider.setBounds(pannerArea);
        else if (has2DPanner && panner2DComponent != nullptr)
            panner2DComponent->setBounds(pannerArea);
        else
            pannerArea.setHeight(0);

        removeBottomSpacing();
        auto panLabelArea = remainingArea.removeFromBottom(labelHeight);
        panLabel.setBounds(panLabelArea.removeFromLeft(50));
        panCoordLabel.setBounds(panLabelArea);
        removeBottomSpacing();
    }
    else
    {
        panLabel.setBounds(0, 0, 0, 0);
        panCoordLabel.setBounds(0, 0, 0, 0);
        stereoPanSlider.setBounds(0, 0, 0, 0);
        if (panner2DComponent != nullptr)
            panner2DComponent->setBounds(0, 0, 0, 0);
    }

    // Transport controls
    auto buttonArea = remainingArea.removeFromBottom(buttonHeight);
    transportControls.setBounds(buttonArea);
    removeBottomSpacing();

    // Configure params button
    auto configureArea = remainingArea.removeFromBottom(configureButtonHeight);
    configureParamsButton.setBounds(configureArea);
    removeBottomSpacing();

    // Generate button
    auto generateArea = remainingArea.removeFromBottom(generateButtonHeight);
    generateButton.setBounds(generateArea);
    removeBottomSpacing();

    // Level control and toggles
    auto controlsArea = remainingArea.removeFromBottom(controlsHeight);
    levelControl.setBounds(controlsArea.removeFromLeft(115)); // 80 + 5 + 30
    controlsArea.removeFromLeft(spacingSmall);
    // Stack toggles vertically: autogen on top, use o as i below
    auto toggleArea = controlsArea.removeFromLeft(100); // Toggle button width
    autogenToggle.setBounds(toggleArea.removeFromTop(30)); // First toggle
    toggleArea.removeFromTop(spacingSmall);
    useOutputAsInputToggle.setBounds(toggleArea.removeFromTop(30)); // Second toggle
    removeBottomSpacing();

    // Knob array (all VampNet controls)
    if (knobAreaHeight > 0)
    {
        auto knobArea = remainingArea.removeFromBottom(knobAreaHeight);
        parameterKnobs.setBounds(knobArea);
        removeBottomSpacing();
    }
    else
    {
        parameterKnobs.setBounds({});
    }

    // Remaining area is waveform
    waveformDisplay.setBounds(remainingArea);
}

void LooperTrack::recordEnableButtonToggled(bool enabled)
{
    auto& track = looperEngine.getTrack(trackIndex);
    DBG("LooperTrack: recordEnableButtonToggled: enabled=" + juce::String(enabled ? "YES" : "NO"));
    track.writeHead.setRecordEnable(enabled);
    repaint();
}

void LooperTrack::playButtonClicked(bool shouldPlay)
{
    auto& track = looperEngine.getTrack(trackIndex);

    if (shouldPlay)
    {
        track.isPlaying.store(true);
        track.recordReadHead.setPlaying(true);
        track.outputReadHead.setPlaying(true);

        if (track.writeHead.getRecordEnable() && !track.recordBuffer.hasRecorded.load())
        {
            const juce::ScopedLock sl(track.recordBuffer.lock);
            track.recordBuffer.clearBuffer();
            track.writeHead.reset();
            track.recordReadHead.reset();
            track.outputReadHead.reset();
        }
    }
    else
    {
        track.isPlaying.store(false);
        track.recordReadHead.setPlaying(false);
        track.outputReadHead.setPlaying(false);
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
    track.recordReadHead.setMuted(muted);
    track.outputReadHead.setMuted(muted);
}

void LooperTrack::generateButtonClicked()
{
    auto& track = looperEngine.getTrack(trackIndex);

    // Get periodic prompt value from model parameters
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
        hasAudio = track.outputBuffer.hasRecorded.load();
        DBG("LooperTrack: Using output buffer as input, hasAudio=" + juce::String(hasAudio ? "YES" : "NO"));
    }
    else
    {
        hasAudio = track.recordBuffer.hasRecorded.load();
        DBG("LooperTrack: Using record buffer as input, hasAudio=" + juce::String(hasAudio ? "YES" : "NO"));
    }

    // If there's no audio, don't proceed with generation
    if (!hasAudio)
    {
        DBG("LooperTrack: No audio - aborting generation");
        generateButton.setEnabled(true);
        generateButton.setButtonText("generate");
        return;
    }

    audioFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("has_audio");
    DBG("LooperTrack: Has audio - passing sentinel file");

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
    if (modelParamsPopup == nullptr)
        return;

    if (!modelParamsPopup->isPopupVisible())
        showModelParamsPopup();
    else
        hideModelParamsPopup();
}

juce::var LooperTrack::getDefaultVampNetParams()
{
    // Create default parameters object (including periodic_prompt)
    juce::DynamicObject::Ptr params = new juce::DynamicObject();

    params->setProperty("sample_temperature", juce::var(0.6));
    params->setProperty("top_p", juce::var(0.0));
    params->setProperty("mask_dropout", juce::var(0.9));
    params->setProperty("time_stretch_factor", juce::var(1));
    params->setProperty("onset_mask_width", juce::var(1));
    params->setProperty("typical_filtering", juce::var(false));
    params->setProperty("typical_mass", juce::var(0.14));
    params->setProperty("typical_min_tokens", juce::var(64));
    params->setProperty("seed", juce::var(0));
    params->setProperty("compression_prompt", juce::var(1));
    params->setProperty("pitch_shift_amount", juce::var(0));
    params->setProperty("sample_cutoff", juce::var(0.9));
    params->setProperty("sampling_steps", juce::var(24));
    params->setProperty("beat_mask_width", juce::var(0));
    params->setProperty("feedback_steps", juce::var(1));
    params->setProperty("periodic_prompt", juce::var(3));
    params->setProperty("model_choice", juce::var("default"));

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
    auto& trackEngine = looperEngine.getTrackEngine(trackIndex);

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
    track.recordReadHead.setPlaying(false);
    track.outputReadHead.setPlaying(false);
    transportControls.setPlayState(false);

    // Disable recording
    track.writeHead.setRecordEnable(false);
    transportControls.setRecordState(false);

    // Clear both buffers and reset heads
    {
        const juce::ScopedLock sl1(track.recordBuffer.lock);
        const juce::ScopedLock sl2(track.outputBuffer.lock);
        track.recordBuffer.clearBuffer();
        track.outputBuffer.clearBuffer();
    }
    track.writeHead.reset();
    track.recordReadHead.reset();
    track.outputReadHead.reset();

    // Also reset the engine's internal state machine flags so r->p behaves like a fresh start
    trackEngine.reset();

    // Leave UI controls (knobs, model params, level, panner, mute, output routing) unchanged.
    // This button now strictly clears the track's audio content.
    repaint();
}

LooperTrack::~LooperTrack()
{
    stopTimer();

    // Remove mouse listener first
    if (generateButtonMouseListener)
        generateButton.removeMouseListener(generateButtonMouseListener.get());
    if (resetButtonMouseListener)
        resetButton.removeMouseListener(resetButtonMouseListener.get());

    // Unregister MIDI parameters
    if (midiLearnManager)
    {
        midiLearnManager->unregisterParameter(trackIdPrefix + "_generate");
        midiLearnManager->unregisterParameter(trackIdPrefix + "_clear");
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
    track.recordReadHead.setSpeed(speed);
    track.outputReadHead.setSpeed(speed);
}

float LooperTrack::getPlaybackSpeed() const
{
    return static_cast<float>(parameterKnobs.getKnobValue(0));
}

float LooperTrack::getPeriodicPrompt() const
{
    if (customVampNetParams.isObject())
    {
        if (auto* obj = customVampNetParams.getDynamicObject())
        {
            auto value = obj->getProperty("periodic_prompt");
            if (value.isDouble() || value.isInt())
                return static_cast<float>(value);
        }
    }
    // Fallback to default periodic_prompt if not present
    return 8.0f;
}

juce::var LooperTrack::getKnobState() const
{
    return parameterKnobs.getState();
}

void LooperTrack::applyKnobState(const juce::var& state, juce::NotificationType notification)
{
    parameterKnobs.applyState(state, notification);
}

void LooperTrack::setCustomParams(const juce::var& params, juce::NotificationType notification)
{
    juce::ignoreUnused(notification);
    if (params.isObject())
        customVampNetParams = params;
    else
        customVampNetParams = getDefaultVampNetParams();

    syncCustomParamsToKnobs();
}

void LooperTrack::setAutogenEnabled(bool enabled)
{
    autogenToggle.setToggleState(enabled, juce::dontSendNotification);
}

void LooperTrack::setUseOutputAsInputEnabled(bool enabled)
{
    useOutputAsInputToggle.setToggleState(enabled, juce::dontSendNotification);
}

bool LooperTrack::isMicEnabled() const
{
    const auto& track = looperEngine.getTrack(trackIndex);
    return track.micEnabled.load();
}

void LooperTrack::setMicEnabled(bool enabled)
{
    auto& track = looperEngine.getTrack(trackIndex);
    track.micEnabled.store(enabled);
    transportControls.setMicState(enabled);
}

void LooperTrack::updateMicButtonAvailability()
{
    auto& track = looperEngine.getTrack(trackIndex);
    bool hasInputInitialized = track.hasInputChannelsInitialized.load();
    bool hasInput = track.hasInputChannels.load();

    // If we haven't seen any audio callbacks yet, don't guess — leave the mic state alone
    // and keep the button enabled so the user can arm it in advance.
    if (!hasInputInitialized)
    {
        transportControls.setMicEnabled(true);
        return;
    }

    if (!hasInput)
    {
        // No input channels: force mic off and disable the button so it appears "stuck off".
        track.micEnabled.store(false);
        transportControls.setMicState(false);
        transportControls.setMicEnabled(false);
    }
    else
    {
        // We have input channels: enable the mic button, but don't force its state
        // (the user may want to keep the mic off).
        transportControls.setMicEnabled(true);
    }
}

Shared::ParameterKnobs* LooperTrack::getModelParameterKnobComponent()
{
    return modelParamsPopup != nullptr ? &modelParamsPopup->getKnobs() : nullptr;
}

const Shared::ParameterKnobs* LooperTrack::getModelParameterKnobComponent() const
{
    return modelParamsPopup != nullptr ? &modelParamsPopup->getKnobs() : nullptr;
}

void LooperTrack::showModelParamsPopup()
{
    if (modelParamsPopup == nullptr)
        return;

    syncCustomParamsToKnobs();

    auto anchor = configureParamsButton.getScreenBounds();
    modelParamsPopup->show(anchor);
    configureParamsButton.setToggleState(true, juce::dontSendNotification);
}

void LooperTrack::hideModelParamsPopup()
{
    if (modelParamsPopup != nullptr)
    {
        modelParamsPopup->dismiss();
        configureParamsButton.setToggleState(false, juce::dontSendNotification);
    }
}

double LooperTrack::getLevelDb() const
{
    return levelControl.getLevelValue();
}

void LooperTrack::setLevelDb(double value, juce::NotificationType notification)
{
    levelControl.setLevelValue(value, notification);
}

juce::var LooperTrack::getPannerState() const
{
    juce::DynamicObject::Ptr state = new juce::DynamicObject();
    state->setProperty("type", pannerType);

    auto typeLower = pannerType.toLowerCase();
    if (typeLower == "stereo")
    {
        state->setProperty("pan", stereoPanSlider.getValue());
    }
    else if ((typeLower == "quad" || typeLower == "cleat") && panner2DComponent != nullptr)
    {
        state->setProperty("x", panner2DComponent->getPanX());
        state->setProperty("y", panner2DComponent->getPanY());
    }

    return juce::var(state);
}

void LooperTrack::applyPannerState(const juce::var& state)
{
    if (!state.isObject())
        return;

    auto* stateObj = state.getDynamicObject();
    if (stateObj == nullptr)
        return;

    auto typeLower = pannerType.toLowerCase();
    if (typeLower == "stereo" && stateObj->hasProperty("pan"))
    {
        stereoPanSlider.setValue(stateObj->getProperty("pan"), juce::sendNotificationSync);
    }
    else if ((typeLower == "quad" || typeLower == "cleat") && panner2DComponent != nullptr)
    {
        float x = static_cast<float>(stateObj->getProperty("x"));
        float y = static_cast<float>(stateObj->getProperty("y"));
        panner2DComponent->setPanPosition(x, y);
    }
}

double LooperTrack::getCustomParamAsDouble(const juce::String& key, double defaultValue) const
{
    if (!customVampNetParams.isObject())
        return defaultValue;

    if (auto* obj = customVampNetParams.getDynamicObject())
    {
        if (obj->hasProperty(key))
        {
            const auto value = obj->getProperty(key);
            if (value.isDouble() || value.isInt())
                return static_cast<double>(value);
            if (value.isBool())
                return value ? 1.0 : 0.0;
        }
    }

    return defaultValue;
}

void LooperTrack::initializeModelParameterKnobs()
{
    vampParamToKnobId.clear();

    if (getModelParameterKnobComponent() == nullptr)
        return;

    addModelParameterKnob("sample_temperature", "temperature", 0.0, 2.0, 0.1, 0.6, false);
    addModelParameterKnob("top_p", "top-p", 0.0, 1.0, 0.1, 0.0, false);
    addModelParameterKnob("mask_dropout", "mask dropout", 0.0, 1.0, 0.1, 0.9, false);
    addModelParameterKnob("time_stretch_factor", "stretch", 0.25, 4.0, 0.01, 1.0, false);
    addModelParameterKnob("onset_mask_width", "onset width", 1.0, 127.0, 1.0, 1.0, true);
    addModelParameterKnob("typical_filtering", "typical filter", 0.0, 1.0, 1.0, 0.0, true, true);
    addModelParameterKnob("typical_mass", "typical mass", 0.0, 1.0, 0.1, 0.14, false);
    addModelParameterKnob("typical_min_tokens", "min tokens", 1.0, 512.0, 1.0, 64.0, true);
    addModelParameterKnob("seed", "seed", 0.0, 100000.0, 1.0, 0.0, true);
    addModelParameterKnob("compression_prompt", "compression", 1.0, 14.0, 1.0, 1.0, true);
    addModelParameterKnob("pitch_shift_amount", "pitch shift", -12.0, 12.0, 1.0, 0.0, true);
    addModelParameterKnob("sample_cutoff", "sample cutoff", 0.0, 1.0, 0.1, 0.9, false);
    addModelParameterKnob("sampling_steps", "steps", 8.0, 64.0, 1.0, 24.0, true);
    addModelParameterKnob("beat_mask_width", "beat mask", 0.0, 1.0, 0.01, 0.0, false);
    addModelParameterKnob("feedback_steps", "feedback", 1.0, 32.0, 1.0, 1.0, true);
    addModelParameterKnob("periodic_prompt", "periodic", 0.0, 127.0, 1.0, 3.0, true);
}

void LooperTrack::syncCustomParamsToKnobs()
{
    if (!customVampNetParams.isObject())
        return;

    auto* obj = customVampNetParams.getDynamicObject();
    for (const auto& entry : vampParamToKnobId)
    {
        if (obj->hasProperty(entry.first))
        {
            double value = getCustomParamAsDouble(entry.first, 0.0);
            if (auto* advancedKnobs = getModelParameterKnobComponent())
                advancedKnobs->setKnobValue(entry.second, value, juce::dontSendNotification);
        }
    }
}

void LooperTrack::addModelParameterKnob(const juce::String& key,
                                        const juce::String& label,
                                        double min,
                                        double max,
                                        double interval,
                                        double defaultValue,
                                        bool isInteger,
                                        bool isBoolean,
                                        const juce::String& suffix)
{
    double initialValue = getCustomParamAsDouble(key, defaultValue);
    auto paramId = trackIdPrefix + "_" + key;
    vampParamToKnobId[key] = paramId;

    auto* advancedKnobs = getModelParameterKnobComponent();
    if (advancedKnobs == nullptr)
        return;

    advancedKnobs->addKnob({
        label,
        min,
        max,
        initialValue,
        interval,
        suffix,
        [this, key, isInteger, isBoolean](double rawValue) {
            if (auto* obj = customVampNetParams.getDynamicObject())
            {
                if (isBoolean)
                    obj->setProperty(key, rawValue >= 0.5);
                else if (isInteger)
                    obj->setProperty(key, static_cast<int>(std::round(rawValue)));
                else
                    obj->setProperty(key, rawValue);
            }
        },
        paramId
    });

    if ((isInteger || isBoolean) && advancedKnobs != nullptr)
    {
        if (auto* slider = advancedKnobs->getSliderForParameter(paramId))
        {
            slider->setNumDecimalPlacesToDisplay(0);
            if (isBoolean)
            {
                slider->setRange(min, max, 1.0);
                slider->textFromValueFunction = [](double value) {
                    return value >= 0.5 ? "on" : "off";
                };
                slider->valueFromTextFunction = [](const juce::String& text) {
                    return text.equalsIgnoreCase("on") ? 1.0 : 0.0;
                };
            }
        }
    }
}
void LooperTrack::timerCallback()
{
    // Sync button states with model state
    auto& track = looperEngine.getTrack(trackIndex);

    bool modelRecordEnable = track.writeHead.getRecordEnable();
    transportControls.setRecordState(modelRecordEnable);

    bool modelIsPlaying = track.isPlaying.load();
    transportControls.setPlayState(modelIsPlaying);

    // Update mic availability based on whether there are input channels
    updateMicButtonAvailability();

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

