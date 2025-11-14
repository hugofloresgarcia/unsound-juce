#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
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
};

class ParameterKnobs : public juce::Component
{
public:
    ParameterKnobs();
    ~ParameterKnobs() override = default;

    void resized() override;

    // Add a knob with configuration
    void addKnob(const KnobConfig& config);

    // Get slider value by index
    double getKnobValue(int index) const;
    
    // Set slider value by index
    void setKnobValue(int index, double value, juce::NotificationType notification);

private:
    struct KnobControl
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
    };
    
    std::vector<KnobControl> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterKnobs)
};

} // namespace Shared

