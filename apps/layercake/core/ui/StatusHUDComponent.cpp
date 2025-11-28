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
    setInterceptsMouseClicks(true, false); // Intercept clicks for audio status
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
        focusName = "";
        valueText = "";
        helpText = "Press '?' for help";
    }
}

void StatusHUDComponent::set_audio_status(bool enabled, const juce::String& deviceName)
{
    audioEnabled = enabled;
    audioDeviceName = deviceName;
    repaint();
}

void StatusHUDComponent::mouseDown(const juce::MouseEvent& event)
{
    // Check if click is in audio status area
    if (audioStatusArea.contains(event.getPosition()))
    {
        if (onAudioStatusClicked)
            onAudioStatusClicked();
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
    const int audioStatusWidth = 180;
    
    auto r = b.reduced(margin, 0);
    
    // Audio status on the RIGHT side (lower right corner)
    audioStatusArea = r.removeFromRight(audioStatusWidth);
    
    if (audioEnabled)
    {
        // Audio ON - show device name in green
        g.setColour(juce::Colour(0xff58f858));  // NES green
        g.setFont(12.0f);
        
        // Truncate device name if too long
        juce::String displayName = audioDeviceName;
        if (displayName.length() > 20)
            displayName = displayName.substring(0, 17) + "...";
        
        g.drawText("AUDIO: " + displayName, audioStatusArea, juce::Justification::centredRight, true);
    }
    else
    {
        // Audio OFF - show warning in red
        g.setColour(juce::Colour(0xfffc4040));  // NES red
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.drawText("AUDIO OFF (click to enable)", audioStatusArea, juce::Justification::centredRight, true);
    }
    
    r.removeFromRight(margin);  // Spacing between audio status and focus info
    
    // Focus info on the left side
    if (focusName.isNotEmpty())
    {
        const int nameWidth = 150;
        const int valueWidth = 100;
        
        // Draw Focus Name
        g.setColour(juce::Colours::yellow);
        g.setFont(14.0f);
        g.drawText(focusName, r.removeFromLeft(nameWidth), juce::Justification::centredLeft, true);
        
        // Draw Value
        g.setColour(juce::Colours::white);
        g.drawText(valueText, r.removeFromLeft(valueWidth), juce::Justification::centredLeft, true);
        
        // Draw Help
        g.setColour(juce::Colours::grey);
        g.drawText(helpText, r, juce::Justification::centredLeft, true);
    }
    else
    {
        // No focus - show general help
        g.setColour(juce::Colours::grey);
        g.setFont(12.0f);
        g.drawText(helpText, r, juce::Justification::centredLeft, true);
    }
}

void StatusHUDComponent::resized()
{
    // Nothing to do here for now
}

} // namespace layercake
