#include "VariationSelector.h"

using namespace Shared;

VariationSelector::VariationSelector()
{
}

void VariationSelector::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);
    
    for (int i = 0; i < numVariations; ++i)
    {
        auto bounds = getBoxBounds(i);
        bool isSelected = (i == selectedVariation);
        bool isDisabled = disabledVariations.find(i) != disabledVariations.end();
        
        // Background color - teal if selected, dark gray if not, darker if disabled
        juce::Colour bgColor;
        if (isDisabled)
        {
            bgColor = juce::Colour(0xff1a1a1a); // Very dark gray for disabled
        }
        else
        {
            bgColor = isSelected ? juce::Colour(0xff1eb19d) : juce::Colour(0xff333333);
        }
        g.setColour(bgColor);
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
        
        // Border - darker if disabled
        juce::Colour borderColor;
        if (isDisabled)
        {
            borderColor = juce::Colour(0xff0a0a0a); // Very dark border for disabled
        }
        else
        {
            borderColor = isSelected ? juce::Colour(0xff1eb19d) : juce::Colour(0xff666666);
        }
        g.setColour(borderColor);
        g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 2.0f);
        
        // Text - grayed out if disabled
        juce::Colour textColor;
        if (isDisabled)
        {
            textColor = juce::Colour(0xff444444); // Dark gray text for disabled
        }
        else
        {
            textColor = isSelected ? juce::Colours::black : juce::Colour(0xfff3d430);
        }
        g.setColour(textColor);
        g.setFont(juce::Font(juce::FontOptions()
                            .withName(juce::Font::getDefaultMonospacedFontName())
                            .withHeight(12.0f)));
        g.drawText("[" + juce::String(i + 1) + "]", bounds, juce::Justification::centred);
    }
}

void VariationSelector::resized()
{
    // Component size is determined by number of variations
    // No need to resize child components since we draw everything ourselves
}

void VariationSelector::mouseDown(const juce::MouseEvent& e)
{
    for (int i = 0; i < numVariations; ++i)
    {
        if (getBoxBounds(i).contains(e.getPosition()))
        {
            // Command-click (or Ctrl-click) toggles disabled state
            bool isCommandDown = e.mods.isCommandDown() || e.mods.isCtrlDown();
            if (isCommandDown)
            {
                bool currentlyDisabled = disabledVariations.find(i) != disabledVariations.end();
                setVariationEnabled(i, currentlyDisabled); // Toggle disabled state
            }
            else
            {
                // Normal click selects variation (only if enabled)
                if (disabledVariations.find(i) == disabledVariations.end())
                {
                    setSelectedVariation(i);
                    if (onVariationSelected)
                        onVariationSelected(i);
                }
            }
            repaint();
            break;
        }
    }
}

void VariationSelector::setNumVariations(int numVariations)
{
    this->numVariations = juce::jmax(1, numVariations);
    selectedVariation = juce::jmin(selectedVariation, this->numVariations - 1);
    
    // Remove disabled flags for variations that no longer exist
    auto it = disabledVariations.begin();
    while (it != disabledVariations.end())
    {
        if (*it >= this->numVariations)
        {
            it = disabledVariations.erase(it);
        }
        else
        {
            ++it;
        }
    }
    
    // Resize component to fit all boxes
    int totalWidth = (boxWidth * this->numVariations) + (boxSpacing * (this->numVariations - 1));
    setSize(totalWidth, boxHeight);
    
    repaint();
}

void VariationSelector::setSelectedVariation(int variationIndex)
{
    if (variationIndex >= 0 && variationIndex < numVariations)
    {
        selectedVariation = variationIndex;
        repaint();
    }
}

juce::Rectangle<int> VariationSelector::getBoxBounds(int index) const
{
    if (index < 0 || index >= numVariations)
        return juce::Rectangle<int>();
    
    int x = index * (boxWidth + boxSpacing);
    return juce::Rectangle<int>(x, 0, boxWidth, boxHeight);
}

void VariationSelector::setVariationEnabled(int variationIndex, bool enabled)
{
    if (variationIndex < 0 || variationIndex >= numVariations)
        return;
    
    if (enabled)
    {
        disabledVariations.erase(variationIndex);
    }
    else
    {
        disabledVariations.insert(variationIndex);
        // If we disabled the currently selected variation, switch to next enabled one
        if (variationIndex == selectedVariation)
        {
            int nextIndex = getNextEnabledVariation(variationIndex);
            if (nextIndex >= 0)
            {
                setSelectedVariation(nextIndex);
                if (onVariationSelected)
                    onVariationSelected(nextIndex);
            }
        }
    }
    repaint();
}

bool VariationSelector::isVariationEnabled(int variationIndex) const
{
    if (variationIndex < 0 || variationIndex >= numVariations)
        return false;
    return disabledVariations.find(variationIndex) == disabledVariations.end();
}

int VariationSelector::getNextEnabledVariation(int currentIndex) const
{
    // Start searching from the next index
    for (int i = 1; i < numVariations; ++i)
    {
        int nextIndex = (currentIndex + i) % numVariations;
        if (disabledVariations.find(nextIndex) == disabledVariations.end())
        {
            return nextIndex;
        }
    }
    // If no enabled variation found, return -1
    return -1;
}

