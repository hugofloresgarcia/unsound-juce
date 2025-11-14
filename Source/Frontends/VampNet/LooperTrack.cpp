#include "LooperTrack.h"
#include "../Shared/ModelParameterDialog.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace VampNet;

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
    auto& track = looperEngine.getTrack(trackIndex);
    
    const juce::ScopedLock sl(track.tapeLoop.lock);
    const auto& buffer = track.tapeLoop.getBuffer();
    
    if (buffer.empty())
        return juce::Result::fail("Buffer is empty");

    size_t wrapPos = track.writeHead.getWrapPos();
    if (wrapPos == 0)
        wrapPos = track.tapeLoop.recordedLength.load();
    if (wrapPos == 0)
        wrapPos = buffer.size();
    
    wrapPos = juce::jmin(wrapPos, buffer.size());
    
    if (wrapPos == 0)
        return juce::Result::fail("No audio data to save");

    double sampleRate = track.writeHead.getSampleRate();
    if (sampleRate <= 0)
        sampleRate = 44100.0;

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    outputFile = tempDir.getChildFile("vampnet_input_" + juce::Uuid().toString() + ".wav");

    outputFile.deleteFile();
    std::unique_ptr<juce::OutputStream> fileStream(outputFile.createOutputStream());
    if (fileStream == nullptr)
        return juce::Result::fail("Failed to create output file");
    
    auto* fileOutputStream = dynamic_cast<juce::FileOutputStream*>(fileStream.get());
    if (fileOutputStream != nullptr && !fileOutputStream->openedOk())
        return juce::Result::fail("Failed to open output file");

    juce::WavAudioFormat wavFormat;
    using Opts = juce::AudioFormatWriterOptions;
    auto options = Opts{}.withSampleRate(sampleRate)
                          .withNumChannels(1)
                          .withBitsPerSample(16);

    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(fileStream, options));
    if (writer == nullptr)
        return juce::Result::fail("Failed to create WAV writer");

    juce::AudioBuffer<float> audioBuffer(1, static_cast<int>(wrapPos));
    const float* source = buffer.data();
    float* dest = audioBuffer.getWritePointer(0);
    
    for (size_t i = 0; i < wrapPos; ++i)
        dest[i] = source[i];

    if (!writer->writeFromAudioSampleBuffer(audioBuffer, 0, audioBuffer.getNumSamples()))
        return juce::Result::fail("Failed to write audio data");

    writer.reset();

    DBG("VampNetWorkerThread: Saved " + juce::String(wrapPos) + " samples to " + outputFile.getFullPathName());
    return juce::Result::ok();
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
        juce::URL uploadEndpoint = gradioEndpoint.getChildURL("gradio_api").getChildURL("upload");
        
        // Print curl equivalent for upload request
        DBG("=== CURL EQUIVALENT FOR UPLOAD ===");
        DBG("curl -X POST \\");
        DBG("  -H \"User-Agent: JUCE-VampNet/1.0\" \\");
        DBG("  -F \"files=@" + inputAudioFile.getFullPathName() + "\" \\");
        DBG("  \"" + uploadEndpoint.toString(false) + "\"");
        DBG("===================================");
        
        juce::StringPairArray responseHeaders;
        int statusCode = 0;
        juce::String mimeType = "audio/wav";

        auto postEndpoint = uploadEndpoint.withFileToUpload("files", inputAudioFile, mimeType);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                           .withExtraHeaders("User-Agent: JUCE-VampNet/1.0\r\n")
                           .withConnectionTimeoutMs(30000)
                           .withResponseHeaders(&responseHeaders)
                           .withStatusCode(&statusCode)
                           .withNumRedirectsToFollow(5)
                           .withHttpRequestCmd("POST");

        std::unique_ptr<juce::InputStream> stream(postEndpoint.createInputStream(options));

        if (stream == nullptr || statusCode != 200)
            return juce::Result::fail("Failed to upload audio file. Status: " + juce::String(statusCode));

        juce::String response = stream->readEntireStreamAsString();
        DBG("VampNetWorkerThread: Upload response: " + response);

        juce::var parsedResponse;
        auto parseResult = juce::JSON::parse(response, parsedResponse);
        if (parseResult.failed() || !parsedResponse.isArray())
            return juce::Result::fail("Failed to parse upload response: " + parseResult.getErrorMessage());

        juce::Array<juce::var>* responseArray = parsedResponse.getArray();
        if (responseArray == nullptr || responseArray->isEmpty())
            return juce::Result::fail("Upload response is empty");

        uploadedFilePath = responseArray->getFirst().toString();
        if (uploadedFilePath.isEmpty())
            return juce::Result::fail("Uploaded file path is empty");

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
    juce::var paramsToUse = customParams.isObject() ? customParams : VampNet::LooperTrack::getDefaultVampNetParams();
    
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

    juce::String eventResponse;
    juce::String lastDataLine;
    juce::String currentEventType;
    int lineCount = 0;
    
    DBG("VampNetWorkerThread: Starting to read SSE stream...");
    
    while (!getStream->isExhausted())
    {
        if (threadShouldExit())
        {
            DBG("VampNetWorkerThread: Thread exit requested");
            return juce::Result::fail("Thread was stopped");
        }
        
        juce::String line = getStream->readNextLine();
        lineCount++;
        
        // Skip empty lines (SSE uses blank lines as message separators)
        if (line.trim().isEmpty())
        {
            DBG("VampNetWorkerThread: Empty line (message separator)");
            continue;
        }
        
        DBG("VampNetWorkerThread: SSE line #" + juce::String(lineCount) + ": " + line);

        // Check for event type
        if (line.startsWith("event:"))
        {
            currentEventType = line.substring(6).trim();
            DBG("VampNetWorkerThread: Event type: " + currentEventType);
        }
        else if (line.startsWith("data:"))
        {
            juce::String dataContent = line.substring(5).trim();
            lastDataLine = line;
            
            DBG("VampNetWorkerThread: Data content: " + dataContent.substring(0, juce::jmin(200, dataContent.length())) + "...");
            
            // Check if we just got a complete or error event
            if (currentEventType == "complete")
            {
                eventResponse = line;
                DBG("VampNetWorkerThread: Got complete event with data");
                break;
            }
            else if (currentEventType == "error")
            {
                DBG("VampNetWorkerThread: Got error event with data: " + dataContent);
                
                // Try to read any additional error details
                juce::String additionalInfo;
                while (!getStream->isExhausted())
                {
                    juce::String extraLine = getStream->readNextLine();
                    if (extraLine.isNotEmpty())
                    {
                        additionalInfo += extraLine + "\n";
                        DBG("VampNetWorkerThread: Additional error info: " + extraLine);
                    }
                    if (additionalInfo.length() > 1000) break; // Don't read too much
                }
                
                juce::String errorMsg = "VampNet API returned error";
                if (dataContent != "null" && dataContent.isNotEmpty())
                    errorMsg += ": " + dataContent;
                if (additionalInfo.isNotEmpty())
                    errorMsg += "\nAdditional info: " + additionalInfo;
                    
                return juce::Result::fail(errorMsg);
            }
            
            // Clear the event type after processing
            currentEventType = "";
        }
        // Legacy: check if the line itself contains complete/error
        else if (line.contains("complete"))
        {
            eventResponse = getStream->readNextLine();
            DBG("VampNetWorkerThread: Complete response line (legacy): " + eventResponse);
            break;
        }
        else if (line.contains("error"))
        {
            juce::String errorPayload = getStream->readEntireStreamAsString();
            DBG("VampNetWorkerThread: Error payload (legacy): " + errorPayload);
            return juce::Result::fail("VampNet API error: " + errorPayload);
        }
    }
    
    DBG("VampNetWorkerThread: Finished reading SSE stream. Total lines: " + juce::String(lineCount));
    
    // If we didn't get eventResponse from event:complete, use the last data line
    if (eventResponse.isEmpty() && lastDataLine.isNotEmpty())
    {
        eventResponse = lastDataLine;
        DBG("VampNetWorkerThread: Using last data line as response");
    }
    
    if (eventResponse.isEmpty())
    {
        DBG("VampNetWorkerThread: No valid response received from stream");
        return juce::Result::fail("No response received from VampNet API");
    }

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
    
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::String fileName = outputURL.getFileName();
    juce::String baseName = juce::File::createFileWithoutCheckingPath(fileName).getFileNameWithoutExtension();
    juce::String extension = juce::File::createFileWithoutCheckingPath(fileName).getFileExtension();
    if (extension.isEmpty())
        extension = ".wav";
    
    outputFile = tempDir.getChildFile(baseName + "_" + juce::Uuid().toString() + extension);

    // Print curl equivalent for download request
    DBG("=== CURL EQUIVALENT FOR DOWNLOAD REQUEST ===");
    DBG("curl -X GET \\");
    DBG("  -H \"User-Agent: JUCE-VampNet/1.0\" \\");
    DBG("  -o \"" + outputFile.getFullPathName() + "\" \\");
    DBG("  \"" + outputURL.toString(false) + "\"");
    DBG("=============================================");

    juce::StringPairArray downloadResponseHeaders;
    int downloadStatusCode = 0;
    auto downloadOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders("User-Agent: JUCE-VampNet/1.0\r\n")
                       .withConnectionTimeoutMs(30000)
                       .withResponseHeaders(&downloadResponseHeaders)
                       .withStatusCode(&downloadStatusCode)
                       .withNumRedirectsToFollow(5);

    std::unique_ptr<juce::InputStream> downloadStream(outputURL.createInputStream(downloadOptions));

    if (downloadStream == nullptr || downloadStatusCode != 200)
        return juce::Result::fail("Failed to download output file. Status: " + juce::String(downloadStatusCode));

    outputFile.deleteFile();
    std::unique_ptr<juce::FileOutputStream> fileOutput(outputFile.createOutputStream());
    if (fileOutput == nullptr || !fileOutput->openedOk())
        return juce::Result::fail("Failed to create output file: " + outputFile.getFullPathName());

    fileOutput->writeFromInputStream(*downloadStream, downloadStream->getTotalLength());

    DBG("VampNetWorkerThread: File downloaded to: " + outputFile.getFullPathName());
    return juce::Result::ok();
}

// LooperTrack implementation
LooperTrack::LooperTrack(MultiTrackLooperEngine& engine, int index, std::function<juce::String()> gradioUrlGetter)
    : looperEngine(engine), 
      trackIndex(index),
      waveformDisplay(engine, index),
      transportControls(),
      parameterKnobs(),
      levelControl(engine, index),
      outputSelector(),
      trackLabel("Track", "track " + juce::String(index + 1)),
      resetButton("x"),
      generateButton("generate"),
      gradioUrlProvider(std::move(gradioUrlGetter))
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
    
    // Setup reset button
    resetButton.onClick = [this] { resetButtonClicked(); };
    addAndMakeVisible(resetButton);
    
    // Setup generate button
    generateButton.onClick = [this] { generateButtonClicked(); };
    addAndMakeVisible(generateButton);
    
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
    
    // Setup parameter knobs (speed, overdub, periodic prompt)
    parameterKnobs.addKnob({
        "speed",
        0.25, 4.0, 1.0, 0.01,
        "x",
        [this](double value) {
            looperEngine.getTrack(trackIndex).readHead.setSpeed(static_cast<float>(value));
        }
    });
    
    parameterKnobs.addKnob({
        "overdub",
        0.0, 1.0, 0.5, 0.01,
        "",
        [this](double value) {
            looperEngine.getTrack(trackIndex).writeHead.setOverdubMix(static_cast<float>(value));
        }
    });
    
    parameterKnobs.addKnob({
        "periodic prompt",
        1.0, 23.0, 8.0, 1.0,
        "",
        [this](double value) {
            // Value is stored in the knob, retrieved when generating
        }
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
        generateButton.setLookAndFeel(&laf);
        configureParamsButton.setLookAndFeel(&laf);
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
    const int generateButtonHeight = 30;
    const int configureButtonHeight = 30;
    const int outputSelectorHeight = 30;
    const int knobAreaHeight = 140;
    const int controlsHeight = 160;
    
    const int totalBottomHeight = knobAreaHeight + spacingSmall + 
                                   controlsHeight + spacingSmall +
                                   generateButtonHeight + spacingSmall + 
                                   configureButtonHeight + spacingSmall +
                                   buttonHeight + spacingSmall + 
                                   outputSelectorHeight;
    
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
    
    // Knobs area (now includes periodic prompt)
    auto knobArea = bottomArea.removeFromTop(knobAreaHeight);
    parameterKnobs.setBounds(knobArea);
    bottomArea.removeFromTop(spacingSmall);
    
    // Level control and VU meter
    auto controlsArea = bottomArea.removeFromTop(controlsHeight);
    levelControl.setBounds(controlsArea.removeFromLeft(115)); // 80 + 5 + 30
    controlsArea.removeFromLeft(spacingSmall);
    bottomArea.removeFromTop(spacingSmall);
    
    // Generate button
    generateButton.setBounds(bottomArea.removeFromTop(generateButtonHeight));
    bottomArea.removeFromTop(spacingSmall);
    
    // Configure params button
    configureParamsButton.setBounds(bottomArea.removeFromTop(configureButtonHeight));
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

    // Determine if we have audio
    juce::File audioFile;
    if (track.tapeLoop.hasRecorded.load())
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
                                                                gradioUrlProvider);
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
    
    parameterKnobs.setKnobValue(2, 8.0, juce::dontSendNotification); // periodic prompt
    
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
    looperEngine.getTrack(trackIndex).readHead.setSpeed(speed);
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
    
    bool modelRecordEnable = track.writeHead.getRecordEnable();
    transportControls.setRecordState(modelRecordEnable);
    
    bool modelIsPlaying = track.isPlaying.load();
    transportControls.setPlayState(modelIsPlaying);
    
    // Update displays
    waveformDisplay.repaint();
    levelControl.repaint();
    repaint();
}
