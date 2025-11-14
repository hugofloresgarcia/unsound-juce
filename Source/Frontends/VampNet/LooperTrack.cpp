#include "LooperTrack.h"
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
    auto result = callVampNetAPI(tempAudioFile, periodicPrompt, outputFile);

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

juce::Result VampNetWorkerThread::callVampNetAPI(const juce::File& inputAudioFile, float periodicPrompt, juce::File& outputFile)
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

        juce::var parsedResponse;
        auto parseResult = juce::JSON::parse(response, parsedResponse);
        if (parseResult.failed() || !parsedResponse.isArray())
            return juce::Result::fail("Failed to parse upload response");

        juce::Array<juce::var>* responseArray = parsedResponse.getArray();
        if (responseArray == nullptr || responseArray->isEmpty())
            return juce::Result::fail("Upload response is empty");

        uploadedFilePath = responseArray->getFirst().toString();
        if (uploadedFilePath.isEmpty())
            return juce::Result::fail("Uploaded file path is empty");

        DBG("VampNetWorkerThread: File uploaded: " + uploadedFilePath);
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
    
    // VampNet parameters (using defaults from API spec, except periodic prompt)
    dataItems.add(juce::var(0.1));     // [1] sample temperature
    dataItems.add(juce::var(0));       // [2] top p
    dataItems.add(juce::var(periodicPrompt));  // [3] periodic prompt (user-controlled)
    dataItems.add(juce::var(0));       // [4] mask dropout
    dataItems.add(juce::var(0));       // [5] time stretch factor
    dataItems.add(juce::var(0));       // [6] onset mask width
    dataItems.add(juce::var(true));    // [7] typical filtering
    dataItems.add(juce::var(0.01));    // [8] typical mass
    dataItems.add(juce::var(1));       // [9] typical min tokens
    dataItems.add(juce::var(3));       // [10] seed (0 for random, using 3 as default)
    dataItems.add(juce::var("cat10")); // [11] model choice
    dataItems.add(juce::var(1));       // [12] compression prompt
    dataItems.add(juce::var(-12));     // [13] pitch shift amount
    dataItems.add(juce::var(0));       // [14] sample cutoff
    dataItems.add(juce::var(1));       // [15] sampling steps
    dataItems.add(juce::var(1));       // [16] beat mask width
    dataItems.add(juce::var(1));       // [17] feedback steps
    
    juce::DynamicObject::Ptr payloadObj = new juce::DynamicObject();
    payloadObj->setProperty("data", juce::var(dataItems));
    
    juce::String jsonBody = juce::JSON::toString(juce::var(payloadObj), false);
    
    DBG("VampNetWorkerThread: POST payload: " + jsonBody);

    // Step 3: Make POST request to get event ID
    juce::URL requestEndpoint = gradioEndpoint.getChildURL("gradio_api")
                                       .getChildURL("call")
                                       .getChildURL("vamp");

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

    if (stream == nullptr || statusCode != 200)
        return juce::Result::fail("Failed to make POST request. Status: " + juce::String(statusCode));

    juce::String response = stream->readEntireStreamAsString();

    juce::var parsedResponse;
    auto parseResult = juce::JSON::parse(response, parsedResponse);
    if (parseResult.failed() || !parsedResponse.isObject())
        return juce::Result::fail("Failed to parse POST response");

    juce::DynamicObject* obj = parsedResponse.getDynamicObject();
    if (obj == nullptr || !obj->hasProperty("event_id"))
        return juce::Result::fail("Response does not contain 'event_id'");

    juce::String eventID = obj->getProperty("event_id").toString();
    if (eventID.isEmpty())
        return juce::Result::fail("event_id is empty");

    DBG("VampNetWorkerThread: Got event ID: " + eventID);

    // Step 4: Poll for response
    juce::URL getEndpoint = gradioEndpoint.getChildURL("gradio_api")
                                  .getChildURL("call")
                                  .getChildURL("vamp")
                                  .getChildURL(eventID);

    juce::StringPairArray getResponseHeaders;
    int getStatusCode = 0;
    auto getOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders("User-Agent: JUCE-VampNet/1.0\r\n")
                       .withConnectionTimeoutMs(60000)  // Longer timeout for generation
                       .withResponseHeaders(&getResponseHeaders)
                       .withStatusCode(&getStatusCode)
                       .withNumRedirectsToFollow(5);

    std::unique_ptr<juce::InputStream> getStream(getEndpoint.createInputStream(getOptions));

    if (getStream == nullptr)
        return juce::Result::fail("Failed to create GET stream");

    juce::String eventResponse;
    while (!getStream->isExhausted())
    {
        eventResponse = getStream->readNextLine();
        
        DBG("VampNetWorkerThread: Response line: " + eventResponse);

        if (eventResponse.contains("complete"))
        {
            eventResponse = getStream->readNextLine();
            break;
        }
        else if (eventResponse.contains("error"))
        {
            juce::String errorPayload = getStream->readNextLine();
            return juce::Result::fail("VampNet API error: " + errorPayload);
        }
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
    // Setup track label
    trackLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(trackLabel);
    
    // Setup reset button
    resetButton.onClick = [this] { resetButtonClicked(); };
    addAndMakeVisible(resetButton);
    
    // Setup generate button
    generateButton.onClick = [this] { generateButtonClicked(); };
    addAndMakeVisible(generateButton);
    
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
        0.0, 1.0, 0.0, 0.01,
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
    const int outputSelectorHeight = 30;
    const int knobAreaHeight = 140;
    const int controlsHeight = 160;
    
    const int totalBottomHeight = knobAreaHeight + spacingSmall + 
                                   controlsHeight + spacingSmall +
                                   generateButtonHeight + spacingSmall + 
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
                                                                gradioUrlProvider);
    vampNetWorkerThread->onComplete = [this](juce::Result result, juce::File outputFile, int trackIdx)
    {
        onVampNetComplete(result, outputFile);
    };
    
    vampNetWorkerThread->startThread();
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
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                              "success",
                                              "generated audio loaded into track " + juce::String(trackIndex + 1));
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
    
    parameterKnobs.setKnobValue(2, 0.0, juce::dontSendNotification); // periodic prompt
    
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
