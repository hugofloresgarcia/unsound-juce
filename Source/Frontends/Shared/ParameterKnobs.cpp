#include "ParameterKnobs.h"

using namespace Shared;

ParameterKnobs::ParameterKnobs()
{
}

void ParameterKnobs::addKnob(const KnobConfig& config)
{
    KnobControl control;
    
    // Create slider
    control.slider = std::make_unique<juce::Slider>(
        juce::Slider::RotaryHorizontalVerticalDrag, 
        juce::Slider::TextBoxBelow
    );
    control.slider->setRange(config.minValue, config.maxValue, config.interval);
    control.slider->setValue(config.defaultValue);
    if (config.suffix.isNotEmpty())
        control.slider->setTextValueSuffix(config.suffix);
    
    if (config.onChange)
    {
        control.slider->onValueChange = [slider = control.slider.get(), onChange = config.onChange]()
        {
            onChange(slider->getValue());
        };
    }
    
    // Create label
    control.label = std::make_unique<juce::Label>("", config.label);
    control.label->setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(control.slider.get());
    addAndMakeVisible(control.label.get());
    
    knobs.push_back(std::move(control));
    
    resized();
}

double ParameterKnobs::getKnobValue(int index) const
{
    if (index >= 0 && index < static_cast<int>(knobs.size()))
        return knobs[index].slider->getValue();
    return 0.0;
}

void ParameterKnobs::setKnobValue(int index, double value, juce::NotificationType notification)
{
    if (index >= 0 && index < static_cast<int>(knobs.size()))
        knobs[index].slider->setValue(value, notification);
}

void ParameterKnobs::resized()
{
    if (knobs.empty())
        return;
    
    auto bounds = getLocalBounds();
    
    const int knobSize = 110;
    const int knobSpacing = 15;
    const int knobLabelHeight = 15;
    const int knobLabelSpacing = 5;
    
    // Calculate total width and center knobs
    const int totalKnobWidth = (knobSize * knobs.size()) + (knobSpacing * (knobs.size() - 1));
    const int knobStartX = (bounds.getWidth() - totalKnobWidth) / 2;
    
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        int xPos = knobStartX + static_cast<int>(i) * (knobSize + knobSpacing);
        
        auto knobArea = juce::Rectangle<int>(xPos, bounds.getY(), knobSize, knobSize);
        auto labelArea = knobArea.removeFromTop(knobLabelHeight);
        knobs[i].label->setBounds(labelArea);
        knobArea.removeFromTop(knobLabelSpacing);
        knobs[i].slider->setBounds(knobArea);
    }
}

