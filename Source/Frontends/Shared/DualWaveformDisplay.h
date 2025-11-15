#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Engine/MultiTrackLooperEngine.h"

namespace Shared
{

// DualWaveformDisplay shows both record buffer and output buffer waveforms for VampNet tracks
class DualWaveformDisplay : public juce::Component
{
public:
    DualWaveformDisplay(VampNetMultiTrackLooperEngine& engine, int trackIndex);
    ~DualWaveformDisplay() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    VampNetMultiTrackLooperEngine& looperEngine;
    int trackIndex;

    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> area, TapeLoop& tapeLoop, LooperReadHead& readHead, LooperWriteHead& writeHead, bool isRecordBuffer);
    void drawPlayhead(juce::Graphics& g, juce::Rectangle<int> waveformArea, TapeLoop& tapeLoop, LooperReadHead& readHead, bool isPlaying);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DualWaveformDisplay)
};

} // namespace Shared



