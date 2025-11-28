/*
  ==============================================================================

    KeyboardCommandRouter.cpp
    Created: 25 Nov 2025
    Author:  LayerCake

  ==============================================================================
*/

#include "KeyboardCommandRouter.h"

namespace layercake {

KeyboardCommandRouter::KeyboardCommandRouter(FocusRegistry& reg)
    : focusRegistry(reg)
{
    tapTempo.setTempoCallback([this](float bpm) {
        if (onTempoChanged)
            onTempoChanged(bpm);
    });
}

bool KeyboardCommandRouter::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    checkSequenceTimeout();

    // 1. Handle Global Shortcuts that override everything (e.g. Command Palette, Help, Esc)
    if (handleGlobalShortcuts(key))
        return true;

    // 2. If we are in a sequence state (e.g. user pressed 'g'), handle the next key
    if (waitingForSequence)
    {
        waitingForSequence = false; // reset state
        auto charCode = key.getTextCharacter();
        
        if (lastChar == 'g' && charCode == 'r')
        {
            if (onRandomize) onRandomize();
            return true;
        }
        else if (lastChar == 'l')
        {
            if (charCode >= '1' && charCode <= '8')
            {
                juce::String lfoId = "lfo_" + juce::String((int)(charCode - '0'));
                focusRegistry.setFocusByID(lfoId);
                return true;
            }
        }
        
        // If sequence not matched, maybe fall through or just consume?
        // For now, let's treat it as consumed but failed sequence.
        return true; 
    }

    // 3. Check if the currently focused target wants to handle it
    auto* currentFocus = focusRegistry.getCurrentFocus();
    if (currentFocus && currentFocus->handleKeyPressed(key))
        return true;

    // 4. Handle Navigation (Tab, Arrows if not consumed)
    if (handleNavigation(key))
        return true;
        
    // 5. Handle Single Key Global Commands
    auto charCode = key.getTextCharacter();
    
    // Space and ? are handled in handleGlobalShortcuts
    
    if (charCode == 'l')
    {
        waitingForSequence = true;
        lastChar = 'l';
        lastKeyTime = juce::Time::getMillisecondCounterHiRes();
        return true;
    }
    else if (charCode == 'g')
    {
        waitingForSequence = true;
        lastChar = 'g';
        lastKeyTime = juce::Time::getMillisecondCounterHiRes();
        return true;
    }
    else if (charCode == 't')
    {
        tapTempo.tap();
        return true;
    }
    else if (charCode == 'r')
    {
        if (onToggleRecord) onToggleRecord();
        return true;
    }
    else if (charCode == 'm')
    {
        // Focus main params
        focusRegistry.setFocusByID("main_params"); // ID convention
        return true;
    }

    return false;
}

void KeyboardCommandRouter::checkSequenceTimeout()
{
    if (waitingForSequence)
    {
        auto now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastKeyTime > 1000.0) // 1 second timeout
        {
            waitingForSequence = false;
            // Optionally: trigger the action for the single key if it had one
        }
    }
}

bool KeyboardCommandRouter::handleGlobalShortcuts(const juce::KeyPress& key)
{
    // Esc to cancel/close overlays
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (onCancel) onCancel();
        return true;
    }

    // Space for command palette
    if (key.getTextCharacter() == ' ')
    {
        if (onShowCommandPalette) onShowCommandPalette();
        return true;
    }
    
    // ? for help
    if (key.getTextCharacter() == '?')
    {
        if (onShowHelp) onShowHelp();
        return true;
    }

    return false;
}

bool KeyboardCommandRouter::handleNavigation(const juce::KeyPress& key)
{
    // Tab traversal could be implemented here or rely on JUCE's default if we hook into it.
    // However, since we have our own FocusRegistry, we might want to handle Tab manually 
    // to cycle through our registered targets.
    
    if (key.getKeyCode() == juce::KeyPress::tabKey)
    {
        auto& targets = focusRegistry.getAllTargets();
        if (targets.isEmpty()) return false;
        
        auto* current = focusRegistry.getCurrentFocus();
        int index = targets.indexOf(current);
        
        if (key.getModifiers().isShiftDown())
        {
            index--;
            if (index < 0) index = targets.size() - 1;
        }
        else
        {
            index++;
            if (index >= targets.size()) index = 0;
        }
        
        focusRegistry.setFocus(targets[index]);
        return true;
    }
    
    return false;
}

} // namespace layercake
