/*
  ==============================================================================

    CommandPaletteOverlay.cpp
    Created: 25 Nov 2025
    Author:  LayerCake

  ==============================================================================
*/

#include "CommandPaletteOverlay.h"
#include <cmath>

namespace layercake {

CommandPaletteOverlay::CommandPaletteOverlay(FocusRegistry& reg, std::function<void()> dismissCb)
    : focusRegistry(reg),
      onDismiss(dismissCb)
{
    setAlwaysOnTop(true);
    
    searchBox.setMultiLine(false);
    searchBox.setReturnKeyStartsNewLine(false);
    searchBox.addListener(this);
    searchBox.setJustification(juce::Justification::centred);
    searchBox.setFont(juce::Font(20.0f));
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(searchBox);

    // Define some default radial items (top level categories)
    radialItems.add({"LFOs", "lfo_group", 0.0f});
    radialItems.add({"Main", "main_params", juce::MathConstants<float>::pi * 0.5f});
    radialItems.add({"Seq", "sequencer", juce::MathConstants<float>::pi});
    radialItems.add({"Env", "envelopes", juce::MathConstants<float>::pi * 1.5f});
}

CommandPaletteOverlay::~CommandPaletteOverlay()
{
    searchBox.removeListener(this);
}

void CommandPaletteOverlay::show()
{
    setVisible(true);
    toFront(true);
    searchBox.setText("");
    searchBox.grabKeyboardFocus();
    updateSearchResults();
    startTimerHz(60); // Animation
}

void CommandPaletteOverlay::hide()
{
    if (!isVisible()) return;
    setVisible(false);
    stopTimer();
    // Do NOT call onDismiss() here if onDismiss() is just going to call hide()
    // onDismiss is intended for external notification if needed, but since we are 
    // self-hiding, we rely on MainComponent to be purely reactive or handle focus restoration.
    // If onDismiss contains logic like "hide me", calling it here creates recursion.
    
    // Only call if it's NOT a hide command, or ensure the callback is safe.
    // For now, safe to skip if the parent just uses it to hide.
    // If parent uses it to restore focus, we might need it.
    // Let's assume we only call it if we were visible.
    
    if (onDismiss) onDismiss();
}

void CommandPaletteOverlay::textEditorTextChanged(juce::TextEditor& editor)
{
    updateSearchResults();
    selectedIndex = 0;
    repaint();
}

void CommandPaletteOverlay::updateSearchResults()
{
    juce::String text = searchBox.getText();
    if (text.isEmpty())
    {
        searchResults.clear();
        isRadialMode = true;
    }
    else
    {
        isRadialMode = false;
        searchResults = focusRegistry.findTargets(text);
    }
}

void CommandPaletteOverlay::textEditorReturnKeyPressed(juce::TextEditor&)
{
    if (!searchResults.isEmpty() && selectedIndex >= 0 && selectedIndex < searchResults.size())
    {
        focusRegistry.setFocus(searchResults[selectedIndex]);
        hide();
    }
}

void CommandPaletteOverlay::textEditorEscapeKeyPressed(juce::TextEditor&)
{
    hide();
}

bool CommandPaletteOverlay::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        hide();
        return true;
    }

    if (isRadialMode)
    {
        // Radial navigation logic (arrows)
        // TODO: Map arrows to radial sectors
    }
    else
    {
        // List navigation
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            selectedIndex = juce::jmin(selectedIndex + 1, searchResults.size() - 1);
            repaint();
            return true;
        }
        else if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            selectedIndex = juce::jmax(0, selectedIndex - 1);
            repaint();
            return true;
        }
    }
    
    return false;
}

void CommandPaletteOverlay::timerCallback()
{
    if (isRadialMode)
    {
        // Hit test for radial items
        auto mouse = getMouseXYRelative().toFloat();
        auto center = getLocalBounds().toFloat().getCentre();
        
        // Simple distance check + angle
        if (mouse.getDistanceFrom(center) > 60.0f)
        {
            float angle = std::atan2(mouse.y - center.y, mouse.x - center.x);
            if (angle < 0) angle += juce::MathConstants<float>::twoPi;
            
            // Find closest item
            int bestItem = -1;
            float minDiff = 100.0f;
            
            for (int i = 0; i < radialItems.size(); ++i)
            {
                float diff = std::abs(radialItems[i].angle - angle);
                if (diff > juce::MathConstants<float>::pi) diff = juce::MathConstants<float>::twoPi - diff;
                
                if (diff < minDiff)
                {
                    minDiff = diff;
                    bestItem = i;
                }
            }
            
            if (hoveredRadialItem != bestItem)
            {
                hoveredRadialItem = bestItem;
                repaint();
            }
        }
        else
        {
            if (hoveredRadialItem != -1)
            {
                hoveredRadialItem = -1;
                repaint();
            }
        }
    }
}

void CommandPaletteOverlay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.fillRect(bounds);
    
    auto center = bounds.toFloat().getCentre();
    
    if (isRadialMode)
    {
        drawRadialMenu(g);
    }
    else
    {
        // Draw list results
        int y = center.y + 40;
        int rowHeight = 24;
        
        for (int i = 0; i < searchResults.size(); ++i)
        {
            auto rowBounds = juce::Rectangle<int>(bounds.getCentreX() - 200, y, 400, rowHeight);
            
            if (i == selectedIndex)
            {
                g.setColour(juce::Colours::cyan.withAlpha(0.3f));
                g.fillRect(rowBounds);
            }
            
            g.setColour(juce::Colours::white);
            g.drawText(searchResults[i]->getDisplayName(), rowBounds.reduced(5, 0), juce::Justification::centredLeft);
            
            y += rowHeight;
            if (y > bounds.getBottom() - 20) break;
        }
    }
}

void CommandPaletteOverlay::drawRadialMenu(juce::Graphics& g)
{
    auto center = getLocalBounds().toFloat().getCentre();
    float radius = 120.0f;
    
    // Draw hub
    g.setColour(juce::Colours::darkgrey);
    g.fillEllipse(center.x - 50, center.y - 50, 100, 100);
    
    // Draw items
    for (int i = 0; i < radialItems.size(); ++i)
    {
        float angle = radialItems[i].angle;
        float x = center.x + std::cos(angle) * radius;
        float y = center.y + std::sin(angle) * radius;
        
        bool isHovered = (i == hoveredRadialItem);
        
        g.setColour(isHovered ? juce::Colours::cyan : juce::Colours::white);
        g.drawText(radialItems[i].label, 
                   (int)x - 40, (int)y - 15, 80, 30, 
                   juce::Justification::centred);
                   
        if (isHovered)
        {
            g.drawLine(center.x, center.y, x, y, 2.0f);
        }
    }
}

void CommandPaletteOverlay::resized()
{
    auto center = getLocalBounds().getCentre();
    searchBox.setBounds(center.x - 100, center.y - 15, 200, 30);
}

} // namespace layercake
