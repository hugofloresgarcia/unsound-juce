/*
  ==============================================================================

    StatusHUDComponent.h
    Created: 25 Nov 2025
    Author:  LayerCake

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../focus/FocusRegistry.h"

namespace layercake {

class StatusHUDComponent : public juce::Component,
                           public juce::ChangeListener
{
public:
    StatusHUDComponent(FocusRegistry& reg);
    ~StatusHUDComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // ChangeListener callback
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    FocusRegistry& focusRegistry;
    
    juce::String focusName;
    juce::String valueText;
    juce::String helpText;

    void updateStatus();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusHUDComponent)
};

} // namespace layercake

