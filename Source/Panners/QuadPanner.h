#pragma once

#include "Panner.h"
#include "PanningUtils.h"
#include <atomic>

// Quad panner: processes mono input to 4-channel output (FL, FR, BL, BR)
// Pan control: (x, y) coordinates, both 0.0 to 1.0
// x: 0.0 = left, 1.0 = right
// y: 0.0 = back, 1.0 = front
class QuadPanner : public Panner
{
public:
    QuadPanner();
    ~QuadPanner() override = default;

    // Panner interface
    void processBlock(const float* const* inputChannelData,
                     int numInputChannels,
                     float* const* outputChannelData,
                     int numOutputChannels,
                     int numSamples) override;

    int getNumInputChannels() const override { return 1; }
    int getNumOutputChannels() const override { return 4; }

    // Pan control (both 0.0 to 1.0)
    void setPan(float x, float y);
    float getPanX() const;
    float getPanY() const;

private:
    std::atomic<float> panX{0.5f}; // Default to center
    std::atomic<float> panY{0.5f}; // Default to center
};

