/*
  ==============================================================================

    HelpOverlay.cpp
    Created: 25 Nov 2025
    Author:  LayerCake

  ==============================================================================
*/

#include "HelpOverlay.h"

namespace layercake {

HelpOverlay::HelpOverlay(std::function<void()> dismissCb)
    : onDismiss(dismissCb)
{
    setAlwaysOnTop(true);
    setWantsKeyboardFocus(true);
    
    shortcuts.add({"?", "show this help"});
    shortcuts.add({"space", "command palette"});
    shortcuts.add({"esc", "close"});
    shortcuts.add({"l + 1-8", "focus lfo"});
    shortcuts.add({"m", "focus main params"});
    shortcuts.add({"t", "tap tempo"});
    shortcuts.add({"r", "toggle record"});
    shortcuts.add({"g then r", "randomize"});
    shortcuts.add({"arrows", "navigate / adjust"});
    shortcuts.add({"shift + arrows", "coarse adjust"});
    shortcuts.add({"alt + arrows", "fine adjust"});
    shortcuts.add({"enter", "type value"});
    shortcuts.add({"[ / ]", "step value"});
}

HelpOverlay::~HelpOverlay()
{
}

void HelpOverlay::show()
{
    setVisible(true);
    toFront(true);
    
    // Force focus grab
    grabKeyboardFocus();
    
    // Ensure we are focused by also requesting focus for self
    if (!hasKeyboardFocus(true))
    {
        grabKeyboardFocus(); 
    }
}

void HelpOverlay::hide()
{
    if (!isVisible()) return;
    setVisible(false);
    // Do NOT call onDismiss here to avoid potential recursion if parent callback calls hide()
    // Only call if it's safe or needed for other logic.
    
    if (onDismiss) onDismiss();
}

bool HelpOverlay::keyPressed(const juce::KeyPress& key, juce::Component* origin)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        hide();
        return true;
    }
    hide();
    return true; // Consume any key to dismiss
}

void HelpOverlay::mouseUp(const juce::MouseEvent& e)
{
    // Click to dismiss is also expected
    hide();
}

void HelpOverlay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.fillRect(bounds);
    
    // Header - lowercase per style guide
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(24.0f).boldened());
    g.drawText("keyboard controls", bounds.removeFromTop(60), juce::Justification::centred, false);
    
    g.setFont(juce::Font(16.0f)); // Regular weight
    
    const int rowHeight = 28;
    const int colGap = 20;
    const int keyWidth = 140;
    const int descWidth = 200;
    
    // Calculate total content height to center vertically
    const int totalHeight = shortcuts.size() * rowHeight;
    int y = (bounds.getHeight() - totalHeight) / 2;
    
    // Center horizontally
    const int totalWidth = keyWidth + colGap + descWidth;
    const int leftX = (bounds.getWidth() - totalWidth) / 2;
    
    for (const auto& s : shortcuts)
    {
        // Keys in cyan/accent
        g.setColour(juce::Colours::cyan);
        g.drawText(s.key, leftX, y, keyWidth, rowHeight, juce::Justification::right, false);
        
        // Descriptions in white, lowercase
        g.setColour(juce::Colours::white);
        g.drawText(s.description, leftX + keyWidth + colGap, y, descWidth, rowHeight, juce::Justification::left, false);
        
        y += rowHeight;
    }
    
    // Footer
    g.setColour(juce::Colours::grey);
    g.setFont(juce::Font(14.0f).italicised());
    g.drawText("press any key to close", bounds.removeFromBottom(50), juce::Justification::centred, false);
}

void HelpOverlay::resized()
{
}

} // namespace layercake
