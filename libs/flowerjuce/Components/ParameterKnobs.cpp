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
    
    // Create slider with text box inside the knob
    control.slider = std::make_unique<juce::Slider>(
        juce::Slider::RotaryHorizontalVerticalDrag, 
        juce::Slider::NoTextBox
    );
    control.slider->setRange(config.minValue, config.maxValue, config.interval);
    control.slider->setValue(config.defaultValue);
    if (config.suffix.isNotEmpty())
        control.slider->setTextValueSuffix(config.suffix);
    
    // Create a label to display the value below the title (no border)
    control.valueLabel = std::make_unique<juce::Label>("", "");
    control.valueLabel->setJustificationType(juce::Justification::centred);
    control.valueLabel->setFont(juce::FontOptions(4.0f));  // Very small font
    control.valueLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    control.valueLabel->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    control.valueLabel->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    
    // Update value label when slider changes
    auto slider = control.slider.get();
    auto valueLabel = control.valueLabel.get();
    auto suffix = config.suffix;
    auto updateValueLabel = [slider, valueLabel, suffix]() {
        double value = slider->getValue();
        juce::String text;
        // Format based on the interval - if it's an integer step, show as integer
        if (slider->getInterval() >= 1.0)
            text = juce::String(static_cast<int>(value));
        else
            text = juce::String(value, 2);
        if (suffix.isNotEmpty())
            text += suffix;
        // Format as ({value})
        valueLabel->setText("(" + text + ")", juce::dontSendNotification);
    };
    
    control.slider->onValueChange = [slider, updateValueLabel, onChange = config.onChange]() {
        updateValueLabel();
        if (onChange) onChange(slider->getValue());
    };
    
    // Set initial value
    updateValueLabel();
    
    addAndMakeVisible(control.valueLabel.get());
    
    // Create label with smaller font
    control.label = std::make_unique<juce::Label>("", config.label);
    control.label->setJustificationType(juce::Justification::centred);
    control.label->setFont(juce::FontOptions(11.0f));
    
    // Store default value for double-click reset
    control.defaultValue = config.defaultValue;
    
    // Enable double-click to reset to default value
    control.slider->setDoubleClickReturnValue(true, config.defaultValue);
    
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
    {
        knobs[index].slider->setValue(value, notification);
        // The value label will update automatically via the slider's onValueChange callback
    }
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
    const int knobLabelHeight = 12;    // Title label height
    const int valueLabelHeight = 8;    // Value label height (below title)
    const int knobLabelSpacing = 1;    // Reduced from 5 to minimize spacing
    
    // Preferred dimensions (slightly bigger to fit text with smaller font)
    const int preferredKnobSize = 90;  // Increased from 83 to fit smaller text better
    const int preferredKnobSpacing = 11;  // 15 * 0.75 = 11.25, rounded to 11
    
    // Calculate how much space we actually have (vertical layout)
    const int availableHeight = bounds.getHeight();
    const int numKnobs = static_cast<int>(knobs.size());
    
    // Calculate what fits vertically
    int preferredTotalHeight = (preferredKnobSize * numKnobs) + (preferredKnobSpacing * (numKnobs - 1));
    
    int knobSize;
    int knobSpacing;
    
    if (preferredTotalHeight <= availableHeight)
    {
        // We have enough space, use preferred sizes
        knobSize = preferredKnobSize;
        knobSpacing = preferredKnobSpacing;
    }
    else
    {
        // Scale down to fit available height
        // Start with smaller spacing
        knobSpacing = juce::jmax(5, preferredKnobSpacing / 2);
        
        // Calculate knob size that fits
        int totalSpacing = knobSpacing * (numKnobs - 1);
        knobSize = (availableHeight - totalSpacing) / numKnobs;
        
        // Clamp to reasonable minimum
        knobSize = juce::jmax(70, knobSize);  // Increased minimum from 60 to 70
        
        // Recalculate spacing if knobs are now too small
        if (knobSize == 70)
        {
            totalSpacing = availableHeight - (knobSize * numKnobs);
            knobSpacing = (numKnobs > 1) ? totalSpacing / (numKnobs - 1) : 0;
        }
    }
    
    // Calculate total height and center knobs vertically
    const int totalKnobHeight = (knobSize * numKnobs) + (knobSpacing * (numKnobs - 1));
    const int knobStartY = (bounds.getHeight() - totalKnobHeight) / 2;
    
    // Use full width for each knob, stack vertically
    const int knobWidth = bounds.getWidth();
    
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        int yPos = knobStartY + static_cast<int>(i) * (knobSize + knobSpacing);
        
        // Total area for this knob row
        auto knobArea = juce::Rectangle<int>(bounds.getX(), yPos, knobWidth, knobSize);
        
        // Title label at top
        auto labelArea = knobArea.removeFromTop(knobLabelHeight);
        knobs[i].label->setBounds(labelArea);
        
        // Small spacing
        knobArea.removeFromTop(knobLabelSpacing);
        
        // Value label below title (in parentheses)
        auto valueLabelArea = knobArea.removeFromTop(valueLabelHeight);
        knobs[i].valueLabel->setBounds(valueLabelArea);
        
        // Small spacing
        knobArea.removeFromTop(knobLabelSpacing);
        
        // Slider area (rotary knob) - remaining space
        knobs[i].slider->setBounds(knobArea);
    }
}

