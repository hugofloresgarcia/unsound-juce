#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "TapeLoop.h"
#include "LooperWriteHead.h"
#include "LooperReadHead.h"
#include "OutputBus.h"
#include <atomic>
#include <functional>

// LooperTrackEngine handles processing for a single looper track
class LooperTrackEngine
{
public:
    struct TrackState
    {
        TapeLoop tapeLoop;
        LooperWriteHead writeHead;
        LooperReadHead readHead;
        OutputBus outputBus;
        
        // UI state (these could eventually be moved to the UI layer)
        std::atomic<bool> isPlaying{false};
        
        TrackState() : writeHead(tapeLoop), readHead(tapeLoop) {}
        
        // Non-copyable
        TrackState(const TrackState&) = delete;
        TrackState& operator=(const TrackState&) = delete;
    };

    LooperTrackEngine();
    ~LooperTrackEngine() = default;

    // Initialize the track with sample rate and buffer duration
    void initialize(double sampleRate, double maxBufferDurationSeconds);

    // Process a block of audio samples for this track
    // Returns true if recording was finalized during this block
    bool processBlock(const float* const* inputChannelData,
                     int numInputChannels,
                     float* const* outputChannelData,
                     int numOutputChannels,
                     int numSamples,
                     bool shouldDebug = false);

    // Handle audio device starting (update sample rate)
    void audioDeviceAboutToStart(double sampleRate);

    // Handle audio device stopping
    void audioDeviceStopped();

    // Reset playhead to start
    void reset();

    // Load audio file into the loop
    // Returns true if successful, false otherwise
    bool loadFromFile(const juce::File& audioFile);

    // Access to track state
    TrackState& getTrackState() { return trackState; }
    const TrackState& getTrackState() const { return trackState; }
    
    // Set callback for audio samples (for onset detection, etc.)
    void setAudioSampleCallback(std::function<void(float)> callback) { audioSampleCallback = callback; }

protected:
    // Helper methods factored out for reuse by VampNetTrackEngine
    void processRecording(TrackState& track, const float* const* inputChannelData, 
                         int numInputChannels, float currentPosition, int sample, bool isFirstCall);
    float processPlayback(TrackState& track, bool isFirstCall);
    bool finalizeRecordingIfNeeded(TrackState& track, bool wasRecording, bool isPlaying, 
                                   bool hasExistingAudio, bool& recordingFinalized);

private:
    TrackState trackState;
    bool wasRecording{false};
    bool wasPlaying{false};
    static constexpr double maxBufferDurationSeconds = 10.0;
    
    juce::AudioFormatManager formatManager;
    std::function<void(float)> audioSampleCallback; // Callback for audio samples (for onset detection)
};

