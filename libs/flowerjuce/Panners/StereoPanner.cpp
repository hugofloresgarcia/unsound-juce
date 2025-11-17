#include "StereoPanner.h"
#include <algorithm>

StereoPanner::StereoPanner()
{
    panPosition.store(0.5f); // Default to center
}

void StereoPanner::setPan(float pan)
{
    pan = juce::jlimit(0.0f, 1.0f, pan);
    panPosition.store(pan);
}

float StereoPanner::getPan() const
{
    return panPosition.load();
}

void StereoPanner::processBlock(const float* const* inputChannelData,
                                int numInputChannels,
                                float* const* outputChannelData,
                                int numOutputChannels,
                                int numSamples)
{
    // Verify we have the expected channels
    if (numInputChannels < 1 || numOutputChannels < 2)
        return;

    // Get current pan position
    float pan = panPosition.load();
    
    // Compute panning gains
    auto [leftGain, rightGain] = PanningUtils::computeStereoGains(pan);
    
    // Get input channel (mono)
    const float* input = inputChannelData[0];
    
    // Get output channels (stereo)
    float* leftOut = outputChannelData[0];
    float* rightOut = outputChannelData[1];
    
    // Process samples (accumulate for multi-track mixing)
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float inputSample = input[sample];
        leftOut[sample] += inputSample * leftGain;
        rightOut[sample] += inputSample * rightGain;
    }
}

