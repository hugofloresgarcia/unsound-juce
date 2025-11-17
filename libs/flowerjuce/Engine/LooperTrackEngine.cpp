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
    trackState.tapeLoop.allocate_buffer(sampleRate, maxBufferDurationSeconds);
}

void LooperTrackEngine::audioDeviceAboutToStart(double sampleRate)
{
    trackState.tapeLoop.allocate_buffer(sampleRate, maxBufferDurationSeconds);
    trackState.writeHead.set_sample_rate(sampleRate);
    trackState.readHead.set_sample_rate(sampleRate);
    trackState.writeHead.reset();
    trackState.readHead.reset();
}

void LooperTrackEngine::audioDeviceStopped()
{
    trackState.isPlaying.store(false);
    trackState.readHead.set_playing(false);
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

    const juce::ScopedLock sl(trackState.tapeLoop.m_lock);
    auto& buffer = trackState.tapeLoop.get_buffer();
    
    if (buffer.empty())
    {
        DBG("TapeLoop buffer not allocated. Call initialize() first.");
        return false;
    }

    // Clear the buffer first
    trackState.tapeLoop.clear_buffer();

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
    trackState.writeHead.set_wrap_pos(loadedLength);
    trackState.writeHead.set_pos(loadedLength);
    
    // Update TapeLoop metadata
    trackState.tapeLoop.m_recorded_length.store(loadedLength);
    trackState.tapeLoop.m_has_recorded.store(true);
    
    // Reset read head to start
    trackState.readHead.reset();
    trackState.readHead.set_pos(0.0f);

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
        const juce::ScopedLock sl(track.tapeLoop.m_lock);
        if (isFirstCall)
            DBG_SEGFAULT("Checking if buffer is empty");
        if (track.tapeLoop.get_buffer().empty()) {
            juce::Logger::writeToLog("WARNING: TapeLoop buffer is empty in processBlock");
            if (isFirstCall)
                DBG_SEGFAULT("Buffer is empty, returning false");
            return false;
        }
        if (isFirstCall)
            DBG_SEGFAULT("Buffer is not empty, size=" + juce::String(track.tapeLoop.get_buffer().size()));
    }

    bool isPlaying = track.isPlaying.load();
    bool hasExistingAudio = track.tapeLoop.m_has_recorded.load();
    
    if (isFirstCall && shouldDebug)
    {
        DBG("[LooperTrackEngine] Track state check:");
        DBG("  isPlaying: " << (isPlaying ? "YES" : "NO"));
        DBG("  hasExistingAudio: " << (hasExistingAudio ? "YES" : "NO"));
        DBG("  recordedLength: " << track.tapeLoop.m_recorded_length.load());
        DBG("  recordEnable: " << (track.writeHead.get_record_enable() ? "YES" : "NO"));
    }
    size_t recordedLength = track.tapeLoop.m_recorded_length.load();
    float playheadPos = track.readHead.get_pos();

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
            + "\t RecEnable: " + (track.writeHead.get_record_enable() ? "YES" : "NO")
            + "\t ActuallyRec: " + (track.writeHead.get_record_enable() ? "YES" : "NO")
            + "\t Playhead: " + juce::String(playheadPos)
            + "\t RecordedLen: " + juce::String(recordedLength)
            + "\t HasAudio: " + (hasExistingAudio ? "YES" : "NO")
            + "\t InputLevel: " + juce::String(inputLevel)
            + "\t MaxInput: " + juce::String(maxInput)
            + "\t InputChannels: " + juce::String(numInputChannels)
            + "\t NumSamples: " + juce::String(numSamples)
            + "\t WrapPos: " + juce::String(track.writeHead.get_wrap_pos())
            + "\t LoopEnd: " + juce::String(track.tapeLoop.get_buffer_size())
        );
    }

    // Check if we just started recording (wasn't recording before, but are now)
    bool thisBlockIsFirstTimeRecording = !wasRecording && track.writeHead.get_record_enable() && !hasExistingAudio;

    // Check for recording finalization
    bool recordingFinalized = false;
    bool shouldFinalize = finalizeRecordingIfNeeded(track, wasRecording, isPlaying, hasExistingAudio, recordingFinalized);
    
    // Update wasRecording for next callback
    wasRecording = track.writeHead.get_record_enable();
    bool playbackJustStopped = wasPlaying && !isPlaying;
    wasPlaying = isPlaying;

    if (isPlaying)
    {
        // If we just started recording, reset everything to 0 BEFORE processing
        if (thisBlockIsFirstTimeRecording) // REC_INIT state
        {
            const juce::ScopedLock sl(track.tapeLoop.m_lock);
            track.tapeLoop.clear_buffer(); // TODO: should NOT be in callback.
            track.writeHead.reset();
            track.readHead.reset();
            juce::Logger::writeToLog("~~~ Reset playhead for new recording");
        }

        // Update read head state
        track.readHead.set_playing(true);

        // Allocate temporary mono buffer for playback samples
        juce::HeapBlock<float> monoBuffer(numSamples);
        const float* monoInputChannelData[1] = { monoBuffer.getData() };

        if (isFirstCall)
            DBG_SEGFAULT("Entering sample loop, numSamples=" + juce::String(numSamples));
        
        // First pass: collect playback samples and handle recording
        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (isFirstCall && sample == 0)
                DBG_SEGFAULT("First sample iteration");
            
            float currentPosition = track.readHead.get_pos();

            // Handle recording (overdub or new)
            processRecording(track, inputChannelData, numInputChannels, currentPosition, sample, isFirstCall && sample == 0);

            // Playback (read head processes the sample)
            float sampleValue = processPlayback(track, isFirstCall && sample == 0);
            
            // Store sample in mono buffer for panner processing
            monoBuffer[sample] = sampleValue;
            
            // Feed audio sample to callback if set (for onset detection, etc.)
            if (audioSampleCallback)
            {
                audioSampleCallback(sampleValue);
            }

            // Advance read head by one sample
            if (isFirstCall && sample == 0)
                DBG_SEGFAULT("Calling readHead.advance, wrapPos=" + juce::String(track.writeHead.get_wrap_pos()));
            bool wrapped = track.readHead.advance(static_cast<float>(track.writeHead.get_wrap_pos()));
            if (isFirstCall && sample == 0)
                DBG_SEGFAULT("readHead.advance completed, wrapped=" + juce::String(wrapped ? "YES" : "NO"));
            if (wrapped && !hasExistingAudio)
            {
                track.writeHead.set_record_enable(false); // Stop recording
                juce::Logger::writeToLog("~~~ WRAPPED! Finalized recording");
            }
        }
        
        // Second pass: apply panner to distribute audio to all output channels
        if (track.panner != nullptr)
        {
            // Use panner to distribute mono audio to all output channels with proper gains
            track.panner->processBlock(monoInputChannelData, 1, outputChannelData, numOutputChannels, numSamples);
            
            if (isFirstCall && shouldDebug)
            {
                DBG("[LooperTrackEngine] Panner applied - routing to all " << numOutputChannels << " channels");
            }
        }
        else
        {
            // Fallback: route to all channels at unity gain (when panner not set)
            for (int sample = 0; sample < numSamples; ++sample)
            {
                float sampleValue = monoBuffer[sample];
                for (int channel = 0; channel < numOutputChannels; ++channel)
                {
                    if (outputChannelData[channel] != nullptr)
                    {
                        outputChannelData[channel][sample] += sampleValue;
                    }
                }
            }
            
            if (isFirstCall && shouldDebug)
            {
                DBG("[LooperTrackEngine] No panner set - routing to all channels at unity gain");
            }
        }
        
        if (isFirstCall)
            DBG_SEGFAULT("Sample loop completed");
    }
    else
    {
        // Not playing - stop read head
        track.readHead.set_playing(false);

        if (track.writeHead.get_record_enable() && playbackJustStopped)
        {
            // finalize recording if we were recording and playback just stopped
            track.writeHead.set_record_enable(false); // Stop recording and update UI
            track.writeHead.set_wrap_pos(track.writeHead.get_pos());
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
    if (track.writeHead.get_record_enable() && numInputChannels > 0)
    {
        int inputChannel = track.writeHead.get_input_channel();
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
        track.writeHead.process_sample(inputSample, currentPosition);
        if (isFirstCall)
            DBG_SEGFAULT("writeHead.processSample completed");
    }
}

// Helper method: Process playback for a single sample
float LooperTrackEngine::processPlayback(TrackState& track, bool isFirstCall)
{
    const juce::ScopedLock sl(track.tapeLoop.m_lock);
    
    if (isFirstCall)
    {
        DBG_SEGFAULT("Calling readHead.processSample");
        DBG("[LooperTrackEngine] Track playback state:");
        DBG("  isPlaying: " << (track.isPlaying.load() ? "YES" : "NO"));
        DBG("  hasRecordedAudio: " << (track.tapeLoop.m_recorded_length.load() > 0 ? "YES" : "NO"));
        DBG("  recordedLength: " << track.tapeLoop.m_recorded_length.load());
        DBG("  readHead position: " << track.readHead.get_pos());
    }
    float sampleValue = track.readHead.process_sample();
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
    if (wasRecording && !track.writeHead.get_record_enable() && isPlaying && !hasExistingAudio)
    {
        track.writeHead.finalize_recording(track.writeHead.get_pos());
        recordingFinalized = true;
        juce::Logger::writeToLog("~~~ Finalized initial recording (it was needed)");
        return true;
    }
    return false;
}

