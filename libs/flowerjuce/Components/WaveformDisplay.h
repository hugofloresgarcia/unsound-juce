#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Engine/MultiTrackLooperEngine.h"

namespace Shared
{

class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay(MultiTrackLooperEngine& engine, int trackIndex);
    WaveformDisplay(VampNetMultiTrackLooperEngine& engine, int trackIndex);
    ~WaveformDisplay() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    enum EngineType { Basic, VampNet };
    EngineType engineType;
    union {
        MultiTrackLooperEngine* basicEngine;
        VampNetMultiTrackLooperEngine* vampNetEngine;
    } looperEngine;
    int trackIndex;

    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> area);
    void drawPlayhead(juce::Graphics& g, juce::Rectangle<int> waveformArea);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};

} // namespace Shared

