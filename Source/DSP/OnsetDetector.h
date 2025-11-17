#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

// Stateless functions for onset detection
namespace OnsetDetection
{
    // Compute RMS level of audio block
    inline float computeRMS(const float* audio, int numSamples)
    {
        if (numSamples == 0) return 0.0f;
        
        float sumSquares = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            sumSquares += audio[i] * audio[i];
        }
        return std::sqrt(sumSquares / static_cast<float>(numSamples));
    }
    
    // Compute peak level of audio block
    inline float computePeak(const float* audio, int numSamples)
    {
        if (numSamples == 0) return 0.0f;
        
        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            peak = juce::jmax(peak, std::abs(audio[i]));
        }
        return peak;
    }
    
    // Detect onset using loudness threshold with hysteresis
    // Returns true if onset detected, false otherwise
    // loudness: current loudness value (RMS or peak)
    // threshold: upper threshold for detection
    // lowerThreshold: lower threshold for reset (hysteresis)
    // wasAboveThreshold: previous state (pass by reference, will be updated)
    inline bool detectOnset(float loudness, float threshold, float lowerThreshold, bool& wasAboveThreshold)
    {
        if (loudness >= threshold && !wasAboveThreshold)
        {
            wasAboveThreshold = true;
            return true; // Onset detected
        }
        else if (loudness < lowerThreshold)
        {
            wasAboveThreshold = false;
        }
        return false;
    }
}

// Wrapper class for onset detection
class OnsetDetector
{
public:
    OnsetDetector()
        : threshold(0.1f), lowerThreshold(0.05f), useRMS(true), wasAboveThreshold(false)
    {
    }
    
    ~OnsetDetector() = default;
    
    // Prepare detector with sample rate and block size
    void prepareToPlay(double sampleRate, int blockSize)
    {
        this->sampleRate = sampleRate;
        this->blockSize = blockSize;
        reset();
    }
    
    // Process audio block and detect onset
    // Returns true if onset detected in this block
    bool processBlock(const float* audio, int numSamples, double sampleRate)
    {
        if (audio == nullptr || numSamples == 0)
            return false;
        
        // Update sample rate if changed
        if (std::abs(this->sampleRate - sampleRate) > 1.0)
        {
            this->sampleRate = sampleRate;
        }
        
        float loudness = useRMS 
            ? OnsetDetection::computeRMS(audio, numSamples)
            : OnsetDetection::computePeak(audio, numSamples);
        
        bool detected = OnsetDetection::detectOnset(loudness, threshold, lowerThreshold, wasAboveThreshold);
        
        if (detected)
        {
            DBG("OnsetDetector: Onset detected! Loudness=" + juce::String(loudness, 4) 
                + " Threshold=" + juce::String(threshold, 4)
                + " RMS=" + juce::String(useRMS ? "yes" : "no"));
        }
        
        return detected;
    }
    
    // Set threshold for onset detection
    void setThreshold(float thresh) { threshold = thresh; }
    float getThreshold() const { return threshold; }
    
    // Set lower threshold for hysteresis
    void setLowerThreshold(float lowerThresh) { lowerThreshold = lowerThresh; }
    float getLowerThreshold() const { return lowerThreshold; }
    
    // Use RMS (true) or peak (false) for loudness calculation
    void setUseRMS(bool use) { useRMS = use; }
    bool getUseRMS() const { return useRMS; }
    
    // Reset detection state
    void reset()
    {
        wasAboveThreshold = false;
    }
    
private:
    // Threshold parameters
    float threshold;
    float lowerThreshold;
    bool useRMS;
    bool wasAboveThreshold;
    
    // Sample rate and block size
    double sampleRate{44100.0};
    int blockSize{0};
};
