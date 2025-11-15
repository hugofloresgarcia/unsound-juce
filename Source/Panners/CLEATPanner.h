#pragma once

#include "Panner.h"
#include "PanningUtils.h"
#include <atomic>

// CLEAT panner: processes mono input to 16-channel output (4x4 grid)
// Pan control: (x, y) coordinates, both 0.0 to 1.0
// x: 0.0 = left, 1.0 = right
// y: 0.0 = bottom, 1.0 = top
// Channels are arranged row-major: channels 0-3 = bottom row left-to-right
class CLEATPanner : public Panner
{
public:
    CLEATPanner();
    ~CLEATPanner() override = default;

    // Panner interface
    void processBlock(const float* const* inputChannelData,
                     int numInputChannels,
                     float* const* outputChannelData,
                     int numOutputChannels,
                     int numSamples) override;

    int getNumInputChannels() const override { return 1; }
    int getNumOutputChannels() const override { return 16; }

    // Pan control (both 0.0 to 1.0)
    void setPan(float x, float y);
    float getPanX() const;
    float getPanY() const;

private:
    std::atomic<float> panX{0.5f}; // Default to center
    std::atomic<float> panY{0.5f}; // Default to center
};

