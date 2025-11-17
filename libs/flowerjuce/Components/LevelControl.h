#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Engine/MultiTrackLooperEngine.h"
#include "MidiLearnManager.h"
#include "MidiLearnComponent.h"
#include <functional>

namespace Shared
{

class LevelControl : public juce::Component
{
public:
    LevelControl(MultiTrackLooperEngine& engine, int trackIndex);
    LevelControl(MultiTrackLooperEngine& engine, int trackIndex, MidiLearnManager* midiManager, const juce::String& trackPrefix);
    LevelControl(VampNetMultiTrackLooperEngine& engine, int trackIndex);
    LevelControl(VampNetMultiTrackLooperEngine& engine, int trackIndex, MidiLearnManager* midiManager, const juce::String& trackPrefix);
    ~LevelControl() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Callback for level changes
    std::function<void(double)> onLevelChange;

    // Get/set level value
    double getLevelValue() const;
    void setLevelValue(double value, juce::NotificationType notification);

private:
    enum EngineType { Basic, VampNet };
    EngineType engineType;
    union {
        MultiTrackLooperEngine* basicEngine;
        VampNetMultiTrackLooperEngine* vampNetEngine;
    } looperEngine;
    int trackIndex;

    juce::Slider levelSlider;
    juce::Label levelLabel;
    
    // MIDI learn support
    MidiLearnManager* midiLearnManager = nullptr;
    juce::String trackIdPrefix;
    std::unique_ptr<MidiLearnable> levelLearnable;
    std::unique_ptr<MidiLearnMouseListener> levelMouseListener;

    void drawVUMeter(juce::Graphics& g, juce::Rectangle<int> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelControl)
};

} // namespace Shared

