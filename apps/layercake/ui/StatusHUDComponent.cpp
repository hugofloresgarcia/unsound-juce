/*
  ==============================================================================

    StatusHUDComponent.cpp
    Created: 25 Nov 2025
    Author:  LayerCake

  ==============================================================================
*/

#include "StatusHUDComponent.h"

namespace layercake {

StatusHUDComponent::StatusHUDComponent(FocusRegistry& reg)
    : focusRegistry(reg)
{
    focusRegistry.addChangeListener(this);
    updateStatus();
    setInterceptsMouseClicks(false, false); // Pass through clicks
    setAlwaysOnTop(true);
}

StatusHUDComponent::~StatusHUDComponent()
{
    focusRegistry.removeChangeListener(this);
}

void StatusHUDComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &focusRegistry)
    {
        updateStatus();
        repaint();
    }
}

void StatusHUDComponent::updateStatus()
{
    auto* target = focusRegistry.getCurrentFocus();
    if (target)
    {
        focusName = target->getDisplayName();
        valueText = target->getValueString();
        helpText = target->getHelpText();
    }
    else
    {
        focusName = "No Focus";
        valueText = "";
        helpText = "Press '?' for help";
    }
}

void StatusHUDComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds();
    
    // Background - darker and fully opaque to cover anything behind it
    g.setColour(juce::Colours::black);
    g.fillRect(b);
    
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawRect(b, 1.0f);

    // Layout constants
    const int margin = 10;
    const int nameWidth = 150;
    const int valueWidth = 100;
    
    auto r = b.reduced(margin, 0);
    
    // Draw Focus Name
    g.setColour(juce::Colours::yellow);
    g.setFont(14.0f);
    g.drawText(focusName, r.removeFromLeft(nameWidth), juce::Justification::centredLeft, true);
    
    // Draw Value
    g.setColour(juce::Colours::white);
    g.drawText(valueText, r.removeFromLeft(valueWidth), juce::Justification::centredLeft, true);
    
    // Draw Help
    g.setColour(juce::Colours::grey);
    g.drawText(helpText, r, juce::Justification::centredRight, true);
}

void StatusHUDComponent::resized()
{
    // Nothing to do here for now
}

} // namespace layercake
