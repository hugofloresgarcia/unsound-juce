/*
  ==============================================================================

    FocusableTarget.h
    Created: 25 Nov 2025
    Author:  LayerCake

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace layercake {

class FocusableTarget
{
public:
    virtual ~FocusableTarget() = default;

    /** unique identifier for this target (e.g., "lfo_1_rate", "main_volume") */
    virtual juce::String getFocusID() const = 0;

    /** human readable name for display in HUD */
    virtual juce::String getDisplayName() const = 0;

    /** handled when this target receives focus */
    virtual void onFocusGain() = 0;

    /** handled when this target loses focus */
    virtual void onFocusLost() = 0;

    /** handle a specific key command when focused. returns true if consumed. */
    virtual bool handleKeyPressed(const juce::KeyPress& key) = 0;

    /** get current value as string for HUD */
    virtual juce::String getValueString() const = 0;
    
    /** get help text for this specific control */
    virtual juce::String getHelpText() const { return ""; }
    
    /** get the component associated with this target (for bounds/highlighting) */
    virtual juce::Component* getComponent() = 0;
};

} // namespace layercake

