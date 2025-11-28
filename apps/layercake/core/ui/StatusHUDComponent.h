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
#include <functional>

namespace layercake {

class StatusHUDComponent : public juce::Component,
                           public juce::ChangeListener
{
public:
    StatusHUDComponent(FocusRegistry& reg);
    ~StatusHUDComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    
    // Audio status updates
    void set_audio_status(bool enabled, const juce::String& deviceName);
    
    // Callback when audio status area is clicked
    std::function<void()> onAudioStatusClicked;
    
    // ChangeListener callback
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    FocusRegistry& focusRegistry;
    
    juce::String focusName;
    juce::String valueText;
    juce::String helpText;
    
    // Audio status
    bool audioEnabled{false};
    juce::String audioDeviceName{"No Device"};
    juce::Rectangle<int> audioStatusArea;

    void updateStatus();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusHUDComponent)
};

} // namespace layercake
