/*
  ==============================================================================

    HelpOverlay.h
    Created: 25 Nov 2025
    Author:  LayerCake

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace layercake {

class HelpOverlay : public juce::Component,
                    public juce::KeyListener
{
public:
    HelpOverlay(std::function<void()> onDismiss);
    ~HelpOverlay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void show();
    void hide();
    
    // Override focusGained to ensure we keep focus
    void focusGained(juce::Component::FocusChangeType cause) override {}
    
    bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    std::function<void()> onDismiss;
    
    struct Shortcut {
        juce::String key;
        juce::String description;
    };
    juce::Array<Shortcut> shortcuts;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HelpOverlay)
};

} // namespace layercake
