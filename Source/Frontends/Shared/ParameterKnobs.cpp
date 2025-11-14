#include "ParameterKnobs.h"

using namespace Shared;

ParameterKnobs::ParameterKnobs()
    : ParameterKnobs(nullptr, "")
{
}

ParameterKnobs::ParameterKnobs(MidiLearnManager* midiManager, const juce::String& trackPrefix)
    : midiLearnManager(midiManager), trackIdPrefix(trackPrefix)
{
}

ParameterKnobs::~ParameterKnobs()
{
    // Remove mouse listeners first
    for (auto& knob : knobs)
    {
        if (knob.mouseListener && knob.slider)
            knob.slider->removeMouseListener(knob.mouseListener.get());
    }
    
    if (midiLearnManager)
    {
        for (const auto& knob : knobs)
        {
            if (knob.parameterId.isNotEmpty())
                midiLearnManager->unregisterParameter(knob.parameterId);
        }
    }
}

void ParameterKnobs::addKnob(const KnobConfig& config)
{
    KnobControl control;
    
    // Store parameter info for MIDI learn
    control.minValue = config.minValue;
    control.maxValue = config.maxValue;
    
    // Generate parameter ID if not provided
    if (config.parameterId.isNotEmpty())
        control.parameterId = config.parameterId;
    else if (midiLearnManager && trackIdPrefix.isNotEmpty())
        control.parameterId = trackIdPrefix + "_" + config.label.toLowerCase().replaceCharacter(' ', '_');
    
    // Create slider
    control.slider = std::make_unique<juce::Slider>(
        juce::Slider::RotaryHorizontalVerticalDrag, 
        juce::Slider::TextBoxBelow
    );
    control.slider->setRange(config.minValue, config.maxValue, config.interval);
    control.slider->setValue(config.defaultValue);
    if (config.suffix.isNotEmpty())
        control.slider->setTextValueSuffix(config.suffix);
    
    // Make text box smaller and more compact
    control.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    
    if (config.onChange)
    {
        control.slider->onValueChange = [slider = control.slider.get(), onChange = config.onChange]()
        {
            onChange(slider->getValue());
        };
    }
    
    // Create label with smaller font
    control.label = std::make_unique<juce::Label>("", config.label);
    control.label->setJustificationType(juce::Justification::centred);
    control.label->setFont(juce::FontOptions(11.0f));
    
    addAndMakeVisible(control.slider.get());
    addAndMakeVisible(control.label.get());
    
    // Setup MIDI learn for this knob
    if (midiLearnManager && control.parameterId.isNotEmpty())
    {
        control.learnable = std::make_unique<MidiLearnable>(*midiLearnManager, control.parameterId);
        
        // Create mouse listener for right-click handling
        control.mouseListener = std::make_unique<MidiLearnMouseListener>(*control.learnable, this);
        control.slider->addMouseListener(control.mouseListener.get(), false);
        
        // Capture values needed for lambda
        auto slider = control.slider.get();
        auto minVal = config.minValue;
        auto maxVal = config.maxValue;
        auto onChange = config.onChange;
        
        midiLearnManager->registerParameter({
            control.parameterId,
            [slider, minVal, maxVal, onChange](float normalizedValue) {
                // Map 0.0-1.0 to knob range
                double value = minVal + normalizedValue * (maxVal - minVal);
                slider->setValue(value, juce::dontSendNotification);
                if (onChange) onChange(value);
            },
            [slider, minVal, maxVal]() {
                // Map knob range back to 0.0-1.0
                double value = slider->getValue();
                return static_cast<float>((value - minVal) / (maxVal - minVal));
            },
            trackIdPrefix + " " + config.label,
            false  // Continuous control
        });
    }
    
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

void ParameterKnobs::paint(juce::Graphics& g)
{
    // Draw MIDI indicators on knobs that have mappings
    for (const auto& knob : knobs)
    {
        if (knob.learnable && knob.learnable->hasMidiMapping())
        {
            auto sliderBounds = knob.slider->getBounds();
            g.setColour(juce::Colour(0xffed1683));  // Pink
            g.fillEllipse(sliderBounds.getRight() - 8.0f, sliderBounds.getY() + 2.0f, 6.0f, 6.0f);
        }
    }
}

void ParameterKnobs::resized()
{
    if (knobs.empty())
        return;
    
    auto bounds = getLocalBounds();
    
    // Compact dimensions for labels and spacing
    const int knobLabelHeight = 12;    // Reduced from 15
    const int knobLabelSpacing = 1;    // Reduced from 5 to minimize spacing
    
    // Preferred dimensions
    const int preferredKnobSize = 110;
    const int preferredKnobSpacing = 15;
    
    // Calculate how much space we actually have
    const int availableWidth = bounds.getWidth();
    const int numKnobs = static_cast<int>(knobs.size());
    
    // Calculate what fits
    int preferredTotalWidth = (preferredKnobSize * numKnobs) + (preferredKnobSpacing * (numKnobs - 1));
    
    int knobSize;
    int knobSpacing;
    
    if (preferredTotalWidth <= availableWidth)
    {
        // We have enough space, use preferred sizes
        knobSize = preferredKnobSize;
        knobSpacing = preferredKnobSpacing;
    }
    else
    {
        // Scale down to fit available width
        // Start with smaller spacing
        knobSpacing = juce::jmax(5, preferredKnobSpacing / 2);
        
        // Calculate knob size that fits
        int totalSpacing = knobSpacing * (numKnobs - 1);
        knobSize = (availableWidth - totalSpacing) / numKnobs;
        
        // Clamp to reasonable minimum
        knobSize = juce::jmax(70, knobSize);  // Increased minimum from 60 to 70
        
        // Recalculate spacing if knobs are now too small
        if (knobSize == 70)
        {
            totalSpacing = availableWidth - (knobSize * numKnobs);
            knobSpacing = (numKnobs > 1) ? totalSpacing / (numKnobs - 1) : 0;
        }
    }
    
    // Calculate total width and center knobs
    const int totalKnobWidth = (knobSize * numKnobs) + (knobSpacing * (numKnobs - 1));
    const int knobStartX = (bounds.getWidth() - totalKnobWidth) / 2;
    
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        int xPos = knobStartX + static_cast<int>(i) * (knobSize + knobSpacing);
        
        // Total area for this knob column
        auto knobArea = juce::Rectangle<int>(xPos, bounds.getY(), knobSize, bounds.getHeight());
        
        // Label at top (smaller)
        auto labelArea = knobArea.removeFromTop(knobLabelHeight);
        knobs[i].label->setBounds(labelArea);
        
        // Small spacing
        knobArea.removeFromTop(knobLabelSpacing);
        
        // Give the slider the remaining area - JUCE will handle text box positioning
        // The slider's TextBoxBelow style will place the text box at the bottom
        knobs[i].slider->setBounds(knobArea);
    }
}

