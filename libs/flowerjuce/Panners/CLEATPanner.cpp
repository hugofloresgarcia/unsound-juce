#include "CLEATPanner.h"
#include <algorithm>

CLEATPanner::CLEATPanner()
{
    panX.store(0.5f); // Default to center
    panY.store(0.5f); // Default to center
}

void CLEATPanner::setPan(float x, float y)
{
    x = juce::jlimit(0.0f, 1.0f, x);
    y = juce::jlimit(0.0f, 1.0f, y);
    panX.store(x);
    panY.store(y);
}

float CLEATPanner::getPanX() const
{
    return panX.load();
}

float CLEATPanner::getPanY() const
{
    return panY.load();
}

void CLEATPanner::processBlock(const float* const* inputChannelData,
                               int numInputChannels,
                               float* const* outputChannelData,
                               int numOutputChannels,
                               int numSamples)
{
    // Verify we have the expected channels
    if (numInputChannels < 1 || numOutputChannels < 16)
        return;

    // Get current pan position
    float x = panX.load();
    float y = panY.load();
    
    // Compute panning gains (16 channels, row-major)
    auto gains = PanningUtils::computeCLEATGains(x, y);
    
    // Get input channel (mono)
    const float* input = inputChannelData[0];
    
    // Process samples
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float inputSample = input[sample];
        
        // Apply gains to all 16 output channels
        for (int channel = 0; channel < 16; ++channel)
        {
            if (outputChannelData[channel] != nullptr)
            {
                outputChannelData[channel][sample] = inputSample * gains[channel];
            }
        }
    }
}

