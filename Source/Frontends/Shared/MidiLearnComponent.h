#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MidiLearnManager.h"

namespace Shared
{

/**
 * A mixin class that adds MIDI learn functionality to any Component.
 * Right-click on a component to show a menu with MIDI Learn option.
 */
class MidiLearnable
{
public:
    MidiLearnable(MidiLearnManager& manager, const juce::String& parameterId)
        : midiLearnManager(manager), paramId(parameterId)
    {
    }
    
    virtual ~MidiLearnable() = default;
    
    // Call this from the component's mouseDown handler
    void handleMouseDown(const juce::MouseEvent& event, juce::Component* component)
    {
        if (event.mods.isRightButtonDown() || event.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            
            int currentCC = midiLearnManager.getMappingForParameter(paramId);
            
            if (currentCC >= 0)
            {
                menu.addItem(1, "MIDI Learn... (Currently CC " + juce::String(currentCC) + ")");
                menu.addItem(2, "Clear MIDI Mapping");
            }
            else
            {
                menu.addItem(1, "MIDI Learn...");
            }
            
            menu.showMenuAsync(juce::PopupMenu::Options(),
                [this, component](int result)
                {
                    if (result == 1)
                    {
                        midiLearnManager.startLearning(paramId);
                        // Repaint top level component to show overlay
                        if (auto* topLevel = component->getTopLevelComponent())
                            topLevel->repaint();
                    }
                    else if (result == 2)
                    {
                        midiLearnManager.clearMapping(paramId);
                        component->repaint();
                    }
                });
        }
    }
    
    // Check if this component is currently being learned
    bool isCurrentlyLearning() const
    {
        return midiLearnManager.isLearning() && 
               midiLearnManager.getLearningParameterId() == paramId;
    }
    
    // Check if this component has a MIDI mapping
    bool hasMidiMapping() const
    {
        return midiLearnManager.getMappingForParameter(paramId) >= 0;
    }
    
protected:
    MidiLearnManager& midiLearnManager;
    juce::String paramId;
};

/**
 * Helper class that adds right-click MIDI learn handling to any JUCE component.
 * Attach this as a MouseListener to buttons, sliders, etc.
 */
class MidiLearnMouseListener : public juce::MouseListener
{
public:
    MidiLearnMouseListener(MidiLearnable& learnable, juce::Component* targetComponent)
        : midiLearnable(learnable), target(targetComponent)
    {
    }
    
    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isRightButtonDown() || event.mods.isPopupMenu())
        {
            midiLearnable.handleMouseDown(event, target);
        }
    }
    
private:
    MidiLearnable& midiLearnable;
    juce::Component* target;
};

/**
 * Visual overlay shown when MIDI learn mode is active.
 * Changes the background color and intercepts mouse/keyboard to exit learn mode.
 */
class MidiLearnOverlay : public juce::Component,
                         public juce::Timer,
                         public juce::KeyListener
{
public:
    MidiLearnOverlay(MidiLearnManager& manager)
        : midiLearnManager(manager)
    {
        setInterceptsMouseClicks(false, false);  // Start transparent to clicks
        startTimer(50);  // Update at 20Hz
    }
    
    void paint(juce::Graphics& g) override
    {
        if (!midiLearnManager.isLearning())
            return;
        
        auto bounds = getLocalBounds();
        
        // Pulsing semi-transparent overlay
        float alpha = 0.3f + 0.1f * std::sin(pulsePhase);
        g.setColour(juce::Colour(0xffed1683).withAlpha(alpha));  // Pink overlay
        g.fillRect(bounds);
        
        // Center text message
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions().withHeight(24.0f)));
        
        juce::String text = "MIDI LEARN MODE\n\nMove a MIDI controller for:\n\"" + 
                           midiLearnManager.getLearningParameterId() + 
                           "\"\n\n(Click anywhere or press ESC to cancel)";
        
        // Create a centered text area
        auto textBounds = bounds.reduced(40);  // Add some margin
        g.drawFittedText(text, textBounds, juce::Justification::centred, 10);  // Allow up to 10 lines
    }
    
    void mouseDown(const juce::MouseEvent&) override
    {
        if (midiLearnManager.isLearning())
        {
            midiLearnManager.stopLearning();
            if (auto* parent = getParentComponent())
                parent->repaint();
        }
    }
    
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        if (key == juce::KeyPress::escapeKey && midiLearnManager.isLearning())
        {
            midiLearnManager.stopLearning();
            if (auto* parent = getParentComponent())
                parent->repaint();
            return true;
        }
        return false;
    }
    
    void timerCallback() override
    {
        bool wasLearning = isLearning;
        isLearning = midiLearnManager.isLearning();
        
        // Update mouse interception based on learn state
        setInterceptsMouseClicks(isLearning, isLearning);
        
        if (isLearning)
        {
            pulsePhase += 0.15f;
            if (pulsePhase > juce::MathConstants<float>::twoPi)
                pulsePhase -= juce::MathConstants<float>::twoPi;
        }
        
        if (isLearning != wasLearning || isLearning)
        {
            repaint();
            if (auto* parent = getParentComponent())
                parent->repaint();
        }
    }
    
private:
    MidiLearnManager& midiLearnManager;
    float pulsePhase = 0.0f;
    bool isLearning = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLearnOverlay)
};

} // namespace Shared

