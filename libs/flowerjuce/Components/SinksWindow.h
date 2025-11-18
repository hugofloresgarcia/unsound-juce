#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <flowerjuce/Panners/CLEATPanner.h>
#include <flowerjuce/Panners/PanningUtils.h>
#include <array>
#include <atomic>

namespace flowerjuce
{

class SinksWindow : public juce::Component, public juce::Timer
{
public:
    // Constructor for use with CLEATPanner (supports pink box display)
    SinksWindow(CLEATPanner* panner, const std::array<std::atomic<float>, 16>& channelLevels);
    
    // Constructor for use without CLEATPanner (no pink box support)
    SinksWindow(const std::array<std::atomic<float>, 16>& channelLevels);
    
    ~SinksWindow() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    CLEATPanner* cleatPanner; // nullptr if not using CLEAT panner
    const std::array<std::atomic<float>, 16>& channelLevels;
    
    // Toggle for showing advanced info (only relevant if cleatPanner is not nullptr)
    juce::ToggleButton showPinkBoxesToggle;
    bool showPinkBoxes{true}; // Default to true for backward compatibility
    
    // Channel level meters (peak detection with decay)
    // 0.89 per 50ms frame ≈ 0.7 second decay time constant (faster, more responsive)
    static constexpr float levelDecayFactor{0.89f};
    
    // Internal peak-hold levels with decay (separate from engine levels)
    std::array<float, 16> peakLevels{};
    
    // Store meters area for paint()
    juce::Rectangle<int> metersArea;
    
    // Helper to draw a single channel level meter
    void drawChannelMeter(juce::Graphics& g, juce::Rectangle<int> area, int channel, float level, float gain, float panX, float panY);
    
    // Helper to convert dB to linear
    float linearToDb(float linear) const;
    
    // Helper to compute phases for a given channel
    void computeChannelPhases(int channel, float panX, float panY, float& xPhase, float& yPhase) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SinksWindow)
};

} // namespace flowerjuce

