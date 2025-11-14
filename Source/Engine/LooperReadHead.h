#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "TapeLoop.h"
#include <atomic>

// LooperReadHead handles playback from a TapeLoop
// Multiple read heads can read from the same tape loop simultaneously
class LooperReadHead
{
public:
    LooperReadHead(TapeLoop& tapeLoop);
    ~LooperReadHead() = default;
    
    // Playback control
    void setPlaying(bool playing) { isPlaying.store(playing); }
    bool getPlaying() const { return isPlaying.load(); }
    
    void setMuted(bool muted);
    bool getMuted() const { return isMuted.load(); }
    
    // Set sample rate (call when audio device starts)
    void setSampleRate(double sampleRate);
    
    // Reset mute ramp for new sample rate (call when audio device starts)
    void resetMuteRamp(double sampleRate);
    
    // Playback parameters
    void setSpeed(float speed) { playbackSpeed.store(speed); }
    float getSpeed() const { return playbackSpeed.load(); }
    
    void setLevelDb(float db) { levelDb.store(db); }
    float getLevelDb() const { return levelDb.load(); }
    
    void setOutputChannel(int channel) { outputChannel.store(channel); } // -1 = all channels
    int getOutputChannel() const { return outputChannel.load(); }
    
    // Playhead position
    std::atomic<float> pos{0.0f};
    
    // Level meter (for VU meter display)
    std::atomic<float> levelMeter{0.0f};
    
    // Process playback for a single sample
    // Returns the output sample value, or 0.0f if not playing/muted
    float processSample();
    
    // Advance playhead (call after processSample for each sample)
    // Returns true if the playhead wrapped around the tape loop
    bool advance(float wrapPos);
    
    // Reset playhead to start
    void reset();

    void setPos(float pos) { this->pos.store(pos); }
    float getPos() const { return pos.load(); }
    
    // Sync playhead to a specific position
    void syncTo(float position);
    
private:
    TapeLoop& tapeLoop;
    std::atomic<bool> isPlaying{false};
    std::atomic<bool> isMuted{false};
    std::atomic<float> playbackSpeed{1.0f};
    std::atomic<float> levelDb{0.0f};
    std::atomic<int> outputChannel{-1}; // -1 = all channels, 0+ = specific channel
    std::atomic<double> sampleRate{44100.0}; // Current sample rate
    
    juce::SmoothedValue<float> muteGain{1.0f}; // Smooth mute ramp (10ms)
    
    float interpolateSample(float position) const;
};

