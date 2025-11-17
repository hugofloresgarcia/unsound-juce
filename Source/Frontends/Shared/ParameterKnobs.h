#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "MidiLearnManager.h"
#include "MidiLearnComponent.h"
#include <functional>
#include <vector>

namespace Shared
{

struct KnobConfig
{
    juce::String label;
    double minValue;
    double maxValue;
    double defaultValue;
    double interval;
    juce::String suffix;
    std::function<void(double)> onChange;
    juce::String parameterId;  // Optional: for MIDI learn
};

class ParameterKnobs : public juce::Component
{
public:
    ParameterKnobs();
    ParameterKnobs(MidiLearnManager* midiManager, const juce::String& trackPrefix);
    ~ParameterKnobs() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Add a knob with configuration
    void addKnob(const KnobConfig& config);

    // Get slider value by index
    double getKnobValue(int index) const;
    
    // Set slider value by index
    void setKnobValue(int index, double value, juce::NotificationType notification);

    double getKnobValue(const juce::String& parameterId) const;
    void setKnobValue(const juce::String& parameterId, double value, juce::NotificationType notification);

    juce::Slider* getSliderForParameter(const juce::String& parameterId);
    int getRequiredHeight(int availableWidth) const;

    // Serialize knob state (parameterId -> value)
    juce::var getState() const;

    // Restore knob state from serialized object
    void applyState(const juce::var& state, juce::NotificationType notification);

    // Convenience helpers for config save/load
    std::vector<juce::String> getParameterIds() const;

private:
    struct KnobControl
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        juce::String parameterId;
        double minValue;
        double maxValue;
        std::unique_ptr<MidiLearnable> learnable;
        std::unique_ptr<MidiLearnMouseListener> mouseListener;
    };
    
    std::vector<KnobControl> knobs;
    
    // MIDI learn support
    MidiLearnManager* midiLearnManager = nullptr;
    juce::String trackIdPrefix;

    KnobControl* findKnobById(const juce::String& parameterId);
    const KnobControl* findKnobById(const juce::String& parameterId) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterKnobs)
};

} // namespace Shared

