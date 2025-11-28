/*
  ==============================================================================

    KeyboardCommandRouter.h
    Created: 25 Nov 2025
    Author:  LayerCake

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../focus/FocusRegistry.h"
#include "TapTempoController.h"

namespace layercake {

class KeyboardCommandRouter : public juce::KeyListener
{
public:
    KeyboardCommandRouter(FocusRegistry& focusRegistry);
    ~KeyboardCommandRouter() override = default;

    bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;

    // Command callbacks
    std::function<void()> onToggleRecord;
    std::function<void()> onRandomize;
    std::function<void()> onShowCommandPalette;
    std::function<void()> onShowHelp;
    std::function<void()> onCancel; // Esc key handler
    std::function<void(float)> onTempoChanged;

    TapTempoController& getTapTempoController() { return tapTempo; }

private:
    FocusRegistry& focusRegistry;
    TapTempoController tapTempo;

    // State for multi-key sequences
    bool waitingForSequence = false;
    juce::juce_wchar lastChar = 0;
    double lastKeyTime = 0;
    
    // Helper to check for sequence timeout
    void checkSequenceTimeout();
    
    bool handleGlobalShortcuts(const juce::KeyPress& key);
    bool handleNavigation(const juce::KeyPress& key);
    bool handleFocusSpecific(const juce::KeyPress& key);
};

} // namespace layercake
