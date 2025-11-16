#include "VampNetTrackEngine.h"
#include "../Frontends/VampNet/ClickSynth.h"
#include "../Frontends/VampNet/Sampler.h"
#include <cmath>

// TODO: Remove this debug macro after fixing segmentation fault
#ifndef DEBUG_SEGFAULT
#define DEBUG_SEGFAULT 1
#endif
#if DEBUG_SEGFAULT
#ifndef DBG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#endif
#else
#ifndef DBG_SEGFAULT
#define DBG_SEGFAULT(msg)
#endif
#endif

VampNetTrackEngine::VampNetTrackEngine()
    : clickSynth(std::make_unique<VampNet::ClickSynth>()),
      sampler(std::make_unique<VampNet::Sampler>())
{
    formatManager.registerBasicFormats();
}

VampNetTrackEngine::~VampNetTrackEngine() = default;

void VampNetTrackEngine::initialize(double sampleRate, double maxBufferDurationSeconds)
{
    trackState.recordBuffer.allocateBuffer(sampleRate, maxBufferDurationSeconds);
    trackState.outputBuffer.allocateBuffer(sampleRate, maxBufferDurationSeconds);
}

void VampNetTrackEngine::audioDeviceAboutToStart(double sampleRate)
{
    trackState.recordBuffer.allocateBuffer(sampleRate, maxBufferDurationSeconds);
    trackState.outputBuffer.allocateBuffer(sampleRate, maxBufferDurationSeconds);
    trackState.writeHead.setSampleRate(sampleRate);
    trackState.recordReadHead.setSampleRate(sampleRate);
    trackState.outputReadHead.setSampleRate(sampleRate);
    trackState.writeHead.reset();
    trackState.recordReadHead.reset();
    trackState.outputReadHead.reset();
}

void VampNetTrackEngine::audioDeviceStopped()
{
    trackState.isPlaying.store(false);
    trackState.recordReadHead.setPlaying(false);
    trackState.outputReadHead.setPlaying(false);
}

void VampNetTrackEngine::reset()
{
    trackState.recordReadHead.reset();
    trackState.outputReadHead.reset();
}

bool VampNetTrackEngine::loadFromFile(const juce::File& audioFile)
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

    // Get the record buffer's length to truncate output buffer to match
    size_t recordBufferLength = trackState.recordBuffer.recordedLength.load();
    size_t recordBufferWrapPos = trackState.writeHead.getWrapPos();
    if (recordBufferWrapPos > 0)
    {
        recordBufferLength = recordBufferWrapPos;
    }
    
    // If no record buffer length, we can't truncate - return error
    if (recordBufferLength == 0)
    {
        DBG("Cannot load output buffer: record buffer has no recorded audio");
        return false;
    }

    const juce::ScopedLock sl(trackState.outputBuffer.lock);
    auto& buffer = trackState.outputBuffer.getBuffer();
    
    if (buffer.empty())
    {
        DBG("Output buffer not allocated. Call initialize() first.");
        return false;
    }

    // Clear the output buffer first
    trackState.outputBuffer.clearBuffer();

    // Determine how many samples to read - truncate to record buffer length
    juce::int64 numSamplesToRead = juce::jmin(
        reader->lengthInSamples, 
        static_cast<juce::int64>(buffer.size()),
        static_cast<juce::int64>(recordBufferLength)
    );
    
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

    // Convert to mono and write to output buffer
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

    // Update output buffer metadata - always truncate to record buffer length
    // This ensures output buffer matches input buffer bounds exactly
    size_t loadedLength = static_cast<size_t>(numSamplesToRead);
    
    // Always use record buffer length as the final length (truncate if needed)
    // This ensures both buffers have the same playback length
    trackState.outputBuffer.recordedLength.store(recordBufferLength);
    trackState.outputBuffer.hasRecorded.store(true);
    
    // If the loaded audio is shorter than the record buffer, pad with silence
    // If longer, it's already truncated by numSamplesToRead calculation above
    if (loadedLength < recordBufferLength)
    {
        // Zero out the remaining samples
        for (size_t i = loadedLength; i < recordBufferLength && i < buffer.size(); ++i)
        {
            buffer[i] = 0.0f;
        }
    }
    
    // Reset read heads to start (both buffers share the same playhead position)
    trackState.recordReadHead.reset();
    trackState.outputReadHead.reset();
    trackState.recordReadHead.setPos(0.0f);
    trackState.outputReadHead.setPos(0.0f);

    DBG("Loaded audio file into output buffer: " << audioFile.getFileName() 
        << " (" << numSamplesToRead << " samples, "
        << (numSamplesToRead / reader->sampleRate) << " seconds)");

    return true;
}

bool VampNetTrackEngine::processBlock(const float* const* inputChannelData,
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
        DBG_SEGFAULT("ENTRY: VampNetTrackEngine::processBlock, numSamples=" + juce::String(numSamples));
    
    auto& track = trackState;
    if (isFirstCall)
        DBG_SEGFAULT("Got track reference");

    // Safety check: if buffers are not allocated, return early
    {
        const juce::ScopedLock sl1(track.recordBuffer.lock);
        const juce::ScopedLock sl2(track.outputBuffer.lock);
        if (isFirstCall)
            DBG_SEGFAULT("Checking if buffers are empty");
        if (track.recordBuffer.getBuffer().empty() || track.outputBuffer.getBuffer().empty()) {
            juce::Logger::writeToLog("WARNING: Buffers are empty in processBlock");
            if (isFirstCall)
                DBG_SEGFAULT("Buffers are empty, returning false");
            return false;
        }
        if (isFirstCall)
            DBG_SEGFAULT("Buffers are not empty");
    }

    bool isPlaying = track.isPlaying.load();
    bool hasExistingAudio = track.recordBuffer.hasRecorded.load();
    
    if (isFirstCall && shouldDebug)
    {
        DBG("[VampNetTrackEngine] Track state check:");
        DBG("  isPlaying: " << (isPlaying ? "YES" : "NO"));
        DBG("  hasExistingAudio: " << (hasExistingAudio ? "YES" : "NO"));
        DBG("  recordedLength: " << track.recordBuffer.recordedLength.load());
        DBG("  recordEnable: " << (track.writeHead.getRecordEnable() ? "YES" : "NO"));
    }
    size_t recordedLength = track.recordBuffer.recordedLength.load();
    float playheadPos = track.recordReadHead.getPos();

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

        juce::Logger::writeToLog(juce::String("VampNetTrack")
            + "\t - Play: " + (isPlaying ? "YES" : "NO")
            + "\t RecEnable: " + (track.writeHead.getRecordEnable() ? "YES" : "NO")
            + "\t Playhead: " + juce::String(playheadPos)
            + "\t RecordedLen: " + juce::String(recordedLength)
            + "\t HasAudio: " + (hasExistingAudio ? "YES" : "NO")
            + "\t InputLevel: " + juce::String(inputLevel)
            + "\t MaxInput: " + juce::String(maxInput)
            + "\t InputChannels: " + juce::String(numInputChannels)
            + "\t NumSamples: " + juce::String(numSamples)
            + "\t WrapPos: " + juce::String(track.writeHead.getWrapPos())
            + "\t DryWetMix: " + juce::String(track.dryWetMix.load())
        );
    }

    // Check if we just started recording (wasn't recording before, but are now)
    bool thisBlockIsFirstTimeRecording = !wasRecording && track.writeHead.getRecordEnable() && !hasExistingAudio;

    // If we just stopped recording (record enable turned off), finalize the loop
    bool recordingFinalized = false;
    if (wasRecording && !track.writeHead.getRecordEnable() && isPlaying && !hasExistingAudio)
    {
        track.writeHead.finalizeRecording(track.writeHead.getPos());
        track.recordReadHead.reset(); // Reset playhead to start of loop
        track.outputReadHead.reset();
        recordingFinalized = true;
        juce::Logger::writeToLog("~~~ Finalized initial recording");
    }

    // Update wasRecording for next callback
    wasRecording = track.writeHead.getRecordEnable();
    bool playbackJustStopped = wasPlaying && !isPlaying;
    wasPlaying = isPlaying;

    if (isPlaying)
    {
        // If we just started recording, reset everything to 0 BEFORE processing
        if (thisBlockIsFirstTimeRecording) // REC_INIT state
        {
            const juce::ScopedLock sl(track.recordBuffer.lock);
            track.recordBuffer.clearBuffer(); // TODO: should NOT be in callback.
            track.writeHead.reset();
            track.recordReadHead.reset();
            track.outputReadHead.reset();
            juce::Logger::writeToLog("~~~ Reset playhead for new recording");
        }

        // Update read head states
        track.recordReadHead.setPlaying(true);
        track.outputReadHead.setPlaying(true);

        if (isFirstCall)
            DBG_SEGFAULT("Entering sample loop, numSamples=" + juce::String(numSamples));
        
        // OPTIMIZATION: Pre-cache values that don't change during the block
        float wrapPos = static_cast<float>(track.writeHead.getWrapPos());
        bool isRecording = track.writeHead.getRecordEnable() && numInputChannels > 0;
        int inputChannel = track.writeHead.getInputChannel();
        bool clickActive = clickSynth->isClickActive();
        bool samplerActive = sampler->isPlaying();
        double sampleRate = track.writeHead.getSampleRate();
        float mix = track.dryWetMix.load();
        
        // Store positions and input samples for writing (since writeHead locks internally)
        juce::Array<float> writePositions;
        juce::Array<float> writeSamples;
        if (isRecording)
        {
            writePositions.resize(numSamples);
            writeSamples.resize(numSamples);
        }
        
        // OPTIMIZATION: Lock buffers once per block for reading instead of per-sample
        // This dramatically reduces lock contention (from numSamples locks to 1 lock per block)
        {
            const juce::ScopedLock sl1(track.recordBuffer.lock);
            const juce::ScopedLock sl2(track.outputBuffer.lock);
            
            for (int sample = 0; sample < numSamples; ++sample)
            {
                if (isFirstCall && sample == 0)
                    DBG_SEGFAULT("First sample iteration");
                
                float currentPosition = track.recordReadHead.getPos();

                // Prepare input sample for recording (store for writing after releasing locks)
                if (isRecording)
                {
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
                    
                    // Mix in click audio if click synth is active
                    if (clickActive)
                    {
                        float clickSample = clickSynth->getNextSample(sampleRate);
                        inputSample += clickSample; // Mix click into input
                    }
                    
                    // Mix in sampler audio if sampler is playing
                    if (samplerActive)
                    {
                        float samplerSample = sampler->getNextSample();
                        inputSample += samplerSample; // Mix sampler into input
                    }
                    
                    // Store position and sample for later writing
                    writePositions.set(sample, currentPosition);
                    writeSamples.set(sample, inputSample);
                }

                // Playback - read from both buffers and mix dry/wet
                // Sync output read head position to match record read head
                track.outputReadHead.setPos(track.recordReadHead.getPos());
                
                float drySample = track.recordReadHead.processSample();
                float wetSample = track.outputReadHead.processSample();
                
                // Mix dry and wet based on dry/wet mix parameter
                // dryWetMix: 0.0 = all dry (record buffer), 1.0 = all wet (output buffer)
                float sampleValue = drySample * (1.0f - mix) + wetSample * mix;

                // Configure output bus from record read head's channel setting
                int outputChannel = track.recordReadHead.getOutputChannel();
                track.outputBus.setOutputChannel(outputChannel);

                // Route to selected output channel(s)
                // Note: activeChannels check is done in MultiTrackLooperEngine, so we pass nullptr here
                track.outputBus.processSample(outputChannelData, numOutputChannels, sample, sampleValue, nullptr);

                // Advance both read heads together (same playhead position)
                bool wrappedRecord = track.recordReadHead.advance(wrapPos);
                bool wrappedOutput = track.outputReadHead.advance(wrapPos);
                
                // Sync positions in case they diverged
                track.outputReadHead.setPos(track.recordReadHead.getPos());
                
                if (wrappedRecord && !hasExistingAudio)
                {
                    recordingFinalized = true;
                    juce::Logger::writeToLog("~~~ WRAPPED! Finalized recording");
                    track.writeHead.setRecordEnable(false);
                    track.writeHead.finalizeRecording(wrapPos);
                    // Sync output buffer wrapPos to match record buffer
                    track.outputBuffer.recordedLength.store(track.recordBuffer.recordedLength.load());
                }
            }
        } // Locks released here
        
        // Process writes after releasing read locks to avoid deadlock
        // WriteHead locks internally, so we can safely call it now
        if (isRecording)
        {
            for (int sample = 0; sample < numSamples; ++sample)
            {
                float inputSample = writeSamples[sample];
                if (inputSample != 0.0f)
                {
                    float currentWritePos = writePositions[sample];
                    track.writeHead.processSample(inputSample, currentWritePos);
                }
            }
        }
        if (isFirstCall)
            DBG_SEGFAULT("Sample loop completed");
    }
    else
    {
        // Not playing - stop read heads
        track.recordReadHead.setPlaying(false);
        track.outputReadHead.setPlaying(false);

        if (track.writeHead.getRecordEnable() && playbackJustStopped)
        {
            // finalize recording if we were recording and playback just stopped
            track.writeHead.finalizeRecording(track.writeHead.getPos());
            recordingFinalized = true;
            // Record enable is on but playback just stopped - prepare for new recording
            juce::Logger::writeToLog("WARNING: ActuallyRecording but not playing.");
        }
    }

    return recordingFinalized;
}


