#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../LooperEngine/MultiTrackLooperEngine.h"

namespace Shared
{

class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay(MultiTrackLooperEngine& engine, int trackIndex);
    // WaveformDisplay(VampNetMultiTrackLooperEngine& engine, int trackIndex); // Commented out - VampNetTrackEngine doesn't exist
    ~WaveformDisplay() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    enum EngineType { Basic }; // Removed VampNet since it doesn't exist
    EngineType engineType;
    MultiTrackLooperEngine* looperEngine; // Simplified - no union needed
    // union {
    //     MultiTrackLooperEngine* basicEngine;
    //     VampNetMultiTrackLooperEngine* vampNetEngine;
    // } looperEngine;
    int trackIndex;

    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> area);
    void drawPlayhead(juce::Graphics& g, juce::Rectangle<int> waveformArea);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};

} // namespace Shared

