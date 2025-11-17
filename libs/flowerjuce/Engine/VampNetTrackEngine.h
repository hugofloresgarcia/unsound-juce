#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "TapeLoop.h"
#include "LooperWriteHead.h"
#include "LooperReadHead.h"
#include "OutputBus.h"
#include <flowerjuce/Panners/Panner.h>
#include <atomic>

// Forward declarations
namespace VampNet { class ClickSynth; class Sampler; }

// VampNetTrackEngine handles processing for a single VampNet track with dual buffers
// Uses two LooperReadHead instances (one per buffer) and one LooperWriteHead
class VampNetTrackEngine
{
public:
    struct TrackState
    {
        TapeLoop recordBuffer;  // Records input audio
        TapeLoop outputBuffer;  // Stores generated outputs
        LooperWriteHead writeHead;  // writes to recordBuffer
        LooperReadHead recordReadHead;  // reads from recordBuffer
        LooperReadHead outputReadHead;  // reads from outputBuffer
        OutputBus outputBus;
        Panner* panner{nullptr}; // Panner for spatial audio distribution
        
        // UI state (these could eventually be moved to the UI layer)
        std::atomic<bool> isPlaying{false};
        std::atomic<float> dryWetMix{0.5f};  // 0.0 = all dry (record buffer), 1.0 = all wet (output buffer)
        
        TrackState() : writeHead(recordBuffer), recordReadHead(recordBuffer), outputReadHead(outputBuffer) {}
        
        // Non-copyable
        TrackState(const TrackState&) = delete;
        TrackState& operator=(const TrackState&) = delete;
    };

    VampNetTrackEngine();
    ~VampNetTrackEngine();

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

    // Get the click synth for this track
    VampNet::ClickSynth& getClickSynth() { return *clickSynth; }
    const VampNet::ClickSynth& getClickSynth() const { return *clickSynth; }
    
    // Get the sampler for this track
    VampNet::Sampler& getSampler() { return *sampler; }
    const VampNet::Sampler& getSampler() const { return *sampler; }

    // Handle audio device starting (update sample rate)
    void audioDeviceAboutToStart(double sampleRate);

    // Handle audio device stopping
    void audioDeviceStopped();

    // Reset playhead to start
    void reset();

    // Load audio file into the output buffer
    // Returns true if successful, false otherwise
    bool loadFromFile(const juce::File& audioFile);

    // Access to track state
    TrackState& getTrackState() { return trackState; }
    const TrackState& getTrackState() const { return trackState; }
    
    // Set panner for spatial audio distribution
    void setPanner(Panner* panner) { trackState.panner = panner; }

private:
    TrackState trackState;
    bool wasRecording{false};
    bool wasPlaying{false};
    static constexpr double maxBufferDurationSeconds = 10.0;
    
    juce::AudioFormatManager formatManager;
    
    // Click synth owned by this track
    std::unique_ptr<VampNet::ClickSynth> clickSynth;
    
    // Sampler owned by this track
    std::unique_ptr<VampNet::Sampler> sampler;
};

