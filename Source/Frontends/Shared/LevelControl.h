#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Engine/MultiTrackLooperEngine.h"
#include <functional>

namespace Shared
{

class LevelControl : public juce::Component
{
public:
    LevelControl(MultiTrackLooperEngine& engine, int trackIndex);
    ~LevelControl() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Callback for level changes
    std::function<void(double)> onLevelChange;

    // Get/set level value
    double getLevelValue() const;
    void setLevelValue(double value, juce::NotificationType notification);

private:
    MultiTrackLooperEngine& looperEngine;
    int trackIndex;

    juce::Slider levelSlider;
    juce::Label levelLabel;

    void drawVUMeter(juce::Graphics& g, juce::Rectangle<int> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelControl)
};

} // namespace Shared

