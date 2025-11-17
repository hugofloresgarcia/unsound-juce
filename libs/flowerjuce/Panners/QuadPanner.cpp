#include "QuadPanner.h"
#include <algorithm>

QuadPanner::QuadPanner()
{
    panX.store(0.5f); // Default to center
    panY.store(0.5f); // Default to center
}

void QuadPanner::setPan(float x, float y)
{
    x = juce::jlimit(0.0f, 1.0f, x);
    y = juce::jlimit(0.0f, 1.0f, y);
    panX.store(x);
    panY.store(y);
}

float QuadPanner::getPanX() const
{
    return panX.load();
}

float QuadPanner::getPanY() const
{
    return panY.load();
}

void QuadPanner::processBlock(const float* const* inputChannelData,
                              int numInputChannels,
                              float* const* outputChannelData,
                              int numOutputChannels,
                              int numSamples)
{
    // Verify we have the expected channels
    if (numInputChannels < 1 || numOutputChannels < 4)
        return;

    // Get current pan position
    float x = panX.load();
    float y = panY.load();
    
    // Compute panning gains [FL, FR, BL, BR]
    auto gains = PanningUtils::computeQuadGains(x, y);
    
    // Get input channel (mono)
    const float* input = inputChannelData[0];
    
    // Get output channels (quad: FL, FR, BL, BR)
    float* flOut = outputChannelData[0];
    float* frOut = outputChannelData[1];
    float* blOut = outputChannelData[2];
    float* brOut = outputChannelData[3];
    
    // Process samples (accumulate for multi-track mixing)
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float inputSample = input[sample];
        flOut[sample] += inputSample * gains[0]; // FL
        frOut[sample] += inputSample * gains[1]; // FR
        blOut[sample] += inputSample * gains[2]; // BL
        brOut[sample] += inputSample * gains[3]; // BR
    }
}

