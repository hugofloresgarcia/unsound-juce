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

double ParameterKnobs::getKnobValue(const juce::String& parameterId) const
{
    if (auto* knob = findKnobById(parameterId))
        return knob->slider->getValue();
    return 0.0;
}

void ParameterKnobs::setKnobValue(const juce::String& parameterId, double value, juce::NotificationType notification)
{
    if (auto* knob = findKnobById(parameterId))
        knob->slider->setValue(value, notification);
}

juce::Slider* ParameterKnobs::getSliderForParameter(const juce::String& parameterId)
{
    if (auto* knob = findKnobById(parameterId))
        return knob->slider.get();
    return nullptr;
}

juce::var ParameterKnobs::getState() const
{
    juce::DynamicObject::Ptr state = new juce::DynamicObject();
    
    for (const auto& knob : knobs)
    {
        if (knob.parameterId.isNotEmpty() && knob.slider != nullptr)
        {
            state->setProperty(knob.parameterId, knob.slider->getValue());
        }
    }
    
    return juce::var(state);
}

void ParameterKnobs::applyState(const juce::var& state, juce::NotificationType notification)
{
    if (!state.isObject())
        return;
    
    auto* stateObj = state.getDynamicObject();
    if (stateObj == nullptr)
        return;
    
    auto& props = stateObj->getProperties();
    for (int i = 0; i < props.size(); ++i)
    {
        const auto propertyName = props.getName(i).toString();
        if (auto* knob = findKnobById(propertyName))
        {
            if (knob->slider != nullptr)
                knob->slider->setValue(props.getValueAt(i), notification);
        }
    }
}

std::vector<juce::String> ParameterKnobs::getParameterIds() const
{
    std::vector<juce::String> ids;
    ids.reserve(knobs.size());
    for (const auto& knob : knobs)
    {
        if (knob.parameterId.isNotEmpty())
            ids.push_back(knob.parameterId);
    }
    return ids;
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

ParameterKnobs::KnobControl* ParameterKnobs::findKnobById(const juce::String& parameterId)
{
    if (parameterId.isEmpty())
        return nullptr;
    
    for (auto& knob : knobs)
    {
        if (knob.parameterId == parameterId)
            return &knob;
    }
    return nullptr;
}

const ParameterKnobs::KnobControl* ParameterKnobs::findKnobById(const juce::String& parameterId) const
{
    if (parameterId.isEmpty())
        return nullptr;
    
    for (const auto& knob : knobs)
    {
        if (knob.parameterId == parameterId)
            return &knob;
    }
    return nullptr;
}

void ParameterKnobs::resized()
{
    if (knobs.empty())
        return;
    
    auto bounds = getLocalBounds();
    const int minKnobWidth = 90;
    const int knobSpacing = 12;
    const int labelHeight = 16;
    const int rowPadding = 12;
    const int sliderHeight = 90;
    
    const int availableWidth = bounds.getWidth();
    const int numKnobs = static_cast<int>(knobs.size());
    const int knobsPerRow = juce::jmax(1, availableWidth / (minKnobWidth + knobSpacing));
    const int rowHeight = sliderHeight + labelHeight + rowPadding;
    const int numRows = (numKnobs + knobsPerRow - 1) / knobsPerRow;
    
    const int totalRowHeight = numRows * rowHeight;
    int yOffset = (bounds.getHeight() - totalRowHeight) > 0 ? (bounds.getHeight() - totalRowHeight) / 2 : 0;
    
    const int actualKnobWidth = juce::jmax(minKnobWidth, (availableWidth - knobSpacing * (knobsPerRow - 1)) / knobsPerRow);
    const int totalWidthUsed = knobsPerRow * actualKnobWidth + knobSpacing * (knobsPerRow - 1);
    const int startX = juce::jmax(0, (availableWidth - totalWidthUsed) / 2);
    
    for (size_t idx = 0; idx < knobs.size(); ++idx)
    {
        int row = static_cast<int>(idx) / knobsPerRow;
        int col = static_cast<int>(idx) % knobsPerRow;
        
        int x = startX + col * (actualKnobWidth + knobSpacing);
        int y = yOffset + row * rowHeight;
        
        auto knobArea = juce::Rectangle<int>(x, y, actualKnobWidth, rowHeight);
        auto labelArea = knobArea.removeFromTop(labelHeight);
        knobs[idx].label->setBounds(labelArea);
        knobArea.removeFromTop(4);
        knobs[idx].slider->setBounds(knobArea.removeFromTop(sliderHeight));
    }
}

int ParameterKnobs::getRequiredHeight(int availableWidth) const
{
    if (knobs.empty())
        return 0;
    
    const int minKnobWidth = 90;
    const int knobSpacing = 12;
    const int rowHeight = 90 + 16 + 12;
    
    int knobsPerRow = juce::jmax(1, availableWidth / (minKnobWidth + knobSpacing));
    int numRows = (static_cast<int>(knobs.size()) + knobsPerRow - 1) / knobsPerRow;
    return numRows * rowHeight;
}

