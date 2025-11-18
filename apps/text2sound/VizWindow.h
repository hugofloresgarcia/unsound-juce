#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <flowerjuce/Engine/MultiTrackLooperEngine.h>
#include "LooperTrack.h"
#include <array>
#include <vector>

namespace Text2Sound
{

class VizWindow : public juce::Component, public juce::Timer
{
public:
    VizWindow(MultiTrackLooperEngine& engine, std::vector<std::weak_ptr<LooperTrack>> tracks);
    ~VizWindow() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    MultiTrackLooperEngine& looperEngine;
    std::vector<std::weak_ptr<LooperTrack>> tracks; // Use weak_ptr for safe access
    
    // Multi-track panner view area
    juce::Rectangle<int> pannerViewArea;
    
    // Track colors (matching UI theme)
    static constexpr int numTrackColors = 8;
    std::array<juce::Colour, numTrackColors> trackColors;
    
    // Helper to draw multi-track panner view
    void drawMultiTrackPanner(juce::Graphics& g, juce::Rectangle<int> area);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VizWindow)
};

} // namespace Text2Sound

