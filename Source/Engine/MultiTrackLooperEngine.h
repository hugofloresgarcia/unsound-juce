#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "LooperTrackEngine.h"
#include <array>
#include <atomic>

// MultiTrackLooperEngine manages multiple looper tracks and the audio device
class MultiTrackLooperEngine : public juce::AudioIODeviceCallback
{
public:
    MultiTrackLooperEngine();
    ~MultiTrackLooperEngine();

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    LooperTrackEngine::TrackState& getTrack(int trackIndex);
    LooperTrackEngine& getTrackEngine(int trackIndex);
    int getNumTracks() const { return numTracks; }
    void setNumTracks(int num);

    void syncAllTracks();

    juce::AudioDeviceManager& getAudioDeviceManager() { return audioDeviceManager; }
    
    // Start audio processing (call after setup is complete)
    void startAudio();

private:
    static constexpr int numTracks = 8;
    static constexpr double maxBufferDurationSeconds = 10.0;

    std::array<LooperTrackEngine, 8> trackEngines;
    juce::AudioDeviceManager audioDeviceManager;
    std::atomic<double> currentSampleRate{44100.0};
};

