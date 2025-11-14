#pragma once

#include <juce_core/juce_core.h>
#include "TapeLoop.h"
#include <atomic>

// LooperWriteHead handles recording to a TapeLoop
class LooperWriteHead
{
public:
    LooperWriteHead(TapeLoop& tapeLoop);
    ~LooperWriteHead() = default;
    
    // Recording control
    void setRecordEnable(bool enable) { recordEnable.store(enable); }
    bool getRecordEnable() const { return recordEnable.load(); }
    
    // Overdub control
    void setOverdubMix(float mix) { overdubMix.store(mix); } // 0.0 = all new, 1.0 = all old
    float getOverdubMix() const { return overdubMix.load(); }
    
    // Process recording for a single sample
    // Returns true if a sample was written
    bool processSample(float inputSample, float currentPosition);
    
    // Finalize recording (set recordedLength when recording stops)
    void finalizeRecording(float finalPosition);
    
    // Reset for new recording
    void reset();

    // Get write position
    void setPos(size_t pos) { this->pos.store(pos); }
    size_t getPos() const { return pos.load(); }

    void setWrapPos(size_t wrapPos) { this->wrapPos.store(wrapPos); }
    size_t getWrapPos() const { return wrapPos.load(); }
    
    // Set sample rate (call when audio device starts)
    void setSampleRate(double sampleRate) { this->sampleRate.store(sampleRate); }
    double getSampleRate() const { return sampleRate.load(); }
    
private:
    // Write position tracking
    std::atomic<size_t> pos{0}; // Maximum position written to
    std::atomic<size_t> wrapPos{0}; // Wrap position / end of loop
    
    TapeLoop& tapeLoop;
    std::atomic<bool> recordEnable{false}; // Actually recording (recordEnable && isPlaying)
    std::atomic<bool> isPlaying{false};
    std::atomic<float> overdubMix{0.5f};
    std::atomic<double> sampleRate{44100.0}; // Current sample rate
};

