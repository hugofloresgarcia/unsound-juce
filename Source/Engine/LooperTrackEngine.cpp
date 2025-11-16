#include "LooperTrackEngine.h"
#include <cmath>

// TODO: Remove this debug macro after fixing segmentation fault
#define DEBUG_SEGFAULT 1
#if DEBUG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#else
#define DBG_SEGFAULT(msg)
#endif

LooperTrackEngine::LooperTrackEngine()
{
    formatManager.registerBasicFormats();
}

void LooperTrackEngine::initialize(double sampleRate, double maxBufferDurationSeconds)
{
    trackState.tapeLoop.allocateBuffer(sampleRate, maxBufferDurationSeconds);
}

void LooperTrackEngine::audioDeviceAboutToStart(double sampleRate)
{
    trackState.tapeLoop.allocateBuffer(sampleRate, maxBufferDurationSeconds);
    trackState.writeHead.setSampleRate(sampleRate);
    trackState.readHead.setSampleRate(sampleRate);
    trackState.writeHead.reset();
    trackState.readHead.reset();
}

void LooperTrackEngine::audioDeviceStopped()
{
    trackState.isPlaying.store(false);
    trackState.readHead.setPlaying(false);
}

void LooperTrackEngine::reset()
{
    trackState.readHead.reset();
}

bool LooperTrackEngine::loadFromFile(const juce::File& audioFile)
{
    if (!audioFile.existsAsFile())
    {
        DBG("Audio file does not exist: " << audioFile.getFullPathName());
        return false;
    }

    // Create reader for the audio file
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
    if (reader == nullptr)
    {
        DBG("Could not create reader for file: " << audioFile.getFullPathName());
        return false;
    }

    const juce::ScopedLock sl(trackState.tapeLoop.lock);
    auto& buffer = trackState.tapeLoop.getBuffer();
    
    if (buffer.empty())
    {
        DBG("TapeLoop buffer not allocated. Call initialize() first.");
        return false;
    }

    // Clear the buffer first
    trackState.tapeLoop.clearBuffer();

    // Determine how many samples to read (limited by buffer size)
    juce::int64 numSamplesToRead = juce::jmin(reader->lengthInSamples, static_cast<juce::int64>(buffer.size()));
    
    if (numSamplesToRead <= 0)
    {
        DBG("Audio file has no samples or buffer is empty");
        return false;
    }

    // Read audio data
    // If multi-channel, we'll mix down to mono by averaging channels
    juce::AudioBuffer<float> tempBuffer(static_cast<int>(reader->numChannels), static_cast<int>(numSamplesToRead));
    
    if (!reader->read(&tempBuffer, 0, static_cast<int>(numSamplesToRead), 0, true, true))
    {
        DBG("Failed to read audio data from file");
        return false;
    }

    // Convert to mono and write to TapeLoop buffer
    // If single channel, just copy. If multi-channel, average them.
    if (tempBuffer.getNumChannels() == 1)
    {
        // Direct copy
        const float* source = tempBuffer.getReadPointer(0);
        for (int i = 0; i < static_cast<int>(numSamplesToRead); ++i)
        {
            buffer[i] = source[i];
        }
    }
    else
    {
        // Mix down to mono by averaging all channels
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

    // Update wrapPos to reflect the loaded audio length
    size_t loadedLength = static_cast<size_t>(numSamplesToRead);
    trackState.writeHead.setWrapPos(loadedLength);
    trackState.writeHead.setPos(loadedLength);
    
    // Update TapeLoop metadata
    trackState.tapeLoop.recordedLength.store(loadedLength);
    trackState.tapeLoop.hasRecorded.store(true);
    
    // Reset read head to start
    trackState.readHead.reset();
    trackState.readHead.setPos(0.0f);

    DBG("Loaded audio file: " << audioFile.getFileName() 
        << " (" << numSamplesToRead << " samples, "
        << (numSamplesToRead / reader->sampleRate) << " seconds)");

    return true;
}

bool LooperTrackEngine::processBlock(const float* const* inputChannelData,
                                     int numInputChannels,
                                     float* const* outputChannelData,
                                     int numOutputChannels,
                                     int numSamples,
                                     bool shouldDebug)
{
    static int callCount = 0;
    callCount++;
    bool isFirstCall = (callCount == 1);
    
    if (isFirstCall)
        DBG_SEGFAULT("ENTRY: LooperTrackEngine::processBlock, numSamples=" + juce::String(numSamples));
    
    auto& track = trackState;
    if (isFirstCall)
        DBG_SEGFAULT("Got track reference");

    // Safety check: if buffer is not allocated, return early
    {
        const juce::ScopedLock sl(track.tapeLoop.lock);
        if (isFirstCall)
            DBG_SEGFAULT("Checking if buffer is empty");
        if (track.tapeLoop.getBuffer().empty()) {
            juce::Logger::writeToLog("WARNING: TapeLoop buffer is empty in processBlock");
            if (isFirstCall)
                DBG_SEGFAULT("Buffer is empty, returning false");
            return false;
        }
        if (isFirstCall)
            DBG_SEGFAULT("Buffer is not empty, size=" + juce::String(track.tapeLoop.getBuffer().size()));
    }

    bool isPlaying = track.isPlaying.load();
    bool hasExistingAudio = track.tapeLoop.hasRecorded.load();
    
    if (isFirstCall && shouldDebug)
    {
        DBG("[LooperTrackEngine] Track state check:");
        DBG("  isPlaying: " << (isPlaying ? "YES" : "NO"));
        DBG("  hasExistingAudio: " << (hasExistingAudio ? "YES" : "NO"));
        DBG("  recordedLength: " << track.tapeLoop.recordedLength.load());
        DBG("  recordEnable: " << (track.writeHead.getRecordEnable() ? "YES" : "NO"));
    }
    size_t recordedLength = track.tapeLoop.recordedLength.load();
    float playheadPos = track.readHead.getPos();

    // Debug output
    if (shouldDebug)
    {
        float inputLevel = 0.0f;
        float maxInput = 0.0f;
        if (inputChannelData[0] != nullptr && numInputChannels > 0 && numSamples > 0)
        {
            inputLevel = std::abs(inputChannelData[0][0]);
            for (int s = 0; s < numSamples && s < 100; ++s)
            {
                maxInput = juce::jmax(maxInput, std::abs(inputChannelData[0][s]));
            }
        }

        juce::Logger::writeToLog(juce::String("Track")
            + "\t - Play: " + (isPlaying ? "YES" : "NO")
            + "\t RecEnable: " + (track.writeHead.getRecordEnable() ? "YES" : "NO")
            + "\t ActuallyRec: " + (track.writeHead.getRecordEnable() ? "YES" : "NO")
            + "\t Playhead: " + juce::String(playheadPos)
            + "\t RecordedLen: " + juce::String(recordedLength)
            + "\t HasAudio: " + (hasExistingAudio ? "YES" : "NO")
            + "\t InputLevel: " + juce::String(inputLevel)
            + "\t MaxInput: " + juce::String(maxInput)
            + "\t InputChannels: " + juce::String(numInputChannels)
            + "\t NumSamples: " + juce::String(numSamples)
            + "\t WrapPos: " + juce::String(track.writeHead.getWrapPos())
            + "\t LoopEnd: " + juce::String(track.tapeLoop.getBufferSize())
        );
    }

    // Check if we just started recording (wasn't recording before, but are now)
    bool thisBlockIsFirstTimeRecording = !wasRecording && track.writeHead.getRecordEnable() && !hasExistingAudio;

    // Check for recording finalization
    bool recordingFinalized = false;
    bool shouldFinalize = finalizeRecordingIfNeeded(track, wasRecording, isPlaying, hasExistingAudio, recordingFinalized);
    
    // Update wasRecording for next callback
    wasRecording = track.writeHead.getRecordEnable();
    bool playbackJustStopped = wasPlaying && !isPlaying;
    wasPlaying = isPlaying;

    if (isPlaying)
    {
        // If we just started recording, reset everything to 0 BEFORE processing
        if (thisBlockIsFirstTimeRecording) // REC_INIT state
        {
            const juce::ScopedLock sl(track.tapeLoop.lock);
            track.tapeLoop.clearBuffer(); // TODO: should NOT be in callback.
            track.writeHead.reset();
            track.readHead.reset();
            juce::Logger::writeToLog("~~~ Reset playhead for new recording");
        }

        // Update read head state
        track.readHead.setPlaying(true);

        if (isFirstCall)
            DBG_SEGFAULT("Entering sample loop, numSamples=" + juce::String(numSamples));
        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (isFirstCall && sample == 0)
                DBG_SEGFAULT("First sample iteration");
            
            float currentPosition = track.readHead.getPos();

            // Handle recording (overdub or new)
            processRecording(track, inputChannelData, numInputChannels, currentPosition, sample, isFirstCall && sample == 0);

            // Playback (read head processes the sample)
            float sampleValue = processPlayback(track, isFirstCall && sample == 0);

            // Configure output bus from read head's channel setting
            int outputChannel = track.readHead.getOutputChannel();
            if (isFirstCall && sample == 0)
            {
                DBG("[LooperTrackEngine] Output routing:");
                DBG("  ReadHead outputChannel: " << outputChannel);
                DBG("  numOutputChannels: " << numOutputChannels);
                DBG("  sampleValue: " << sampleValue);
            }
            track.outputBus.setOutputChannel(outputChannel);
            
            if (isFirstCall && sample == 0)
            {
                DBG("[LooperTrackEngine] OutputBus outputChannel after set: " << track.outputBus.getOutputChannel());
            }

            // Route to selected output channel(s)
            if (isFirstCall && sample == 0)
                DBG_SEGFAULT("Calling outputBus.processSample");
            // Note: activeChannels check is done in MultiTrackLooperEngine, so we pass nullptr here
            // The active channels are verified at the callback level
            track.outputBus.processSample(outputChannelData, numOutputChannels, sample, sampleValue, nullptr);
            if (isFirstCall && sample == 0)
            {
                DBG_SEGFAULT("outputBus.processSample completed");
                // Verify output was written
                if (outputChannel >= 0 && outputChannel < numOutputChannels && outputChannelData[outputChannel] != nullptr)
                {
                    DBG("[LooperTrackEngine] Verified output written to channel " << outputChannel 
                        << ", value: " << outputChannelData[outputChannel][sample]);
                }
                else if (outputChannel == -1)
                {
                    DBG("[LooperTrackEngine] Verified output written to all channels");
                    for (int ch = 0; ch < juce::jmin(3, numOutputChannels); ++ch)
                    {
                        if (outputChannelData[ch] != nullptr)
                            DBG("  Channel " << ch << " value: " << outputChannelData[ch][sample]);
                    }
                }
            }

            // Advance read head by one sample
            if (isFirstCall && sample == 0)
                DBG_SEGFAULT("Calling readHead.advance, wrapPos=" + juce::String(track.writeHead.getWrapPos()));
            bool wrapped = track.readHead.advance(static_cast<float>(track.writeHead.getWrapPos()));
            if (isFirstCall && sample == 0)
                DBG_SEGFAULT("readHead.advance completed, wrapped=" + juce::String(wrapped ? "YES" : "NO"));
            if (wrapped && !hasExistingAudio)
            {
                track.writeHead.setRecordEnable(false); // Stop recording
                juce::Logger::writeToLog("~~~ WRAPPED! Finalized recording");
            }
        }
        if (isFirstCall)
            DBG_SEGFAULT("Sample loop completed");
    }
    else
    {
        // Not playing - stop read head
        track.readHead.setPlaying(false);

        if (track.writeHead.getRecordEnable() && playbackJustStopped)
        {
            // finalize recording if we were recording and playback just stopped
            track.writeHead.setRecordEnable(false); // Stop recording and update UI
            track.writeHead.setWrapPos(track.writeHead.getPos());
            recordingFinalized = true;
            // Record enable is on but playback just stopped - prepare for new recording
            juce::Logger::writeToLog("WARNING: ActuallyRecording but not playing.");
        }
    }

    return recordingFinalized;
}

// Helper method: Process recording for a single sample
void LooperTrackEngine::processRecording(TrackState& track, const float* const* inputChannelData, 
                                         int numInputChannels, float currentPosition, int sample, bool isFirstCall)
{
    // Note: writeHead.processSample() locks the buffer internally, so we don't hold the lock here
    if (track.writeHead.getRecordEnable() && numInputChannels > 0)
    {
        int inputChannel = track.writeHead.getInputChannel();
        float inputSample = 0.0f;
        
        // Get input sample from selected channel
        if (inputChannel == -1)
        {
            // All channels: use channel 0 (mono sum could be added later)
            if (inputChannelData[0] != nullptr)
                inputSample = inputChannelData[0][sample];
        }
        else if (inputChannel >= 0 && inputChannel < numInputChannels && inputChannelData[inputChannel] != nullptr)
        {
            inputSample = inputChannelData[inputChannel][sample];
        }
        
        if (isFirstCall)
            DBG_SEGFAULT("Calling writeHead.processSample");
        track.writeHead.processSample(inputSample, currentPosition);
        if (isFirstCall)
            DBG_SEGFAULT("writeHead.processSample completed");
    }
}

// Helper method: Process playback for a single sample
float LooperTrackEngine::processPlayback(TrackState& track, bool isFirstCall)
{
    const juce::ScopedLock sl(track.tapeLoop.lock);
    
    if (isFirstCall)
    {
        DBG_SEGFAULT("Calling readHead.processSample");
        DBG("[LooperTrackEngine] Track playback state:");
        DBG("  isPlaying: " << (track.isPlaying.load() ? "YES" : "NO"));
        DBG("  hasRecordedAudio: " << (track.tapeLoop.recordedLength.load() > 0 ? "YES" : "NO"));
        DBG("  recordedLength: " << track.tapeLoop.recordedLength.load());
        DBG("  readHead position: " << track.readHead.getPos());
    }
    float sampleValue = track.readHead.processSample();
    if (isFirstCall)
    {
        DBG_SEGFAULT("readHead.processSample completed, value=" + juce::String(sampleValue));
        DBG("[LooperTrackEngine] Track sampleValue: " << sampleValue);
    }
    
    return sampleValue;
}

// Helper method: Check if recording should be finalized
bool LooperTrackEngine::finalizeRecordingIfNeeded(TrackState& track, bool wasRecording, bool isPlaying, 
                                                   bool hasExistingAudio, bool& recordingFinalized)
{
    // If we just stopped recording (record enable turned off), finalize the loop
    if (wasRecording && !track.writeHead.getRecordEnable() && isPlaying && !hasExistingAudio)
    {
        track.writeHead.finalizeRecording(track.writeHead.getPos());
        recordingFinalized = true;
        juce::Logger::writeToLog("~~~ Finalized initial recording (it was needed)");
        return true;
    }
    return false;
}

