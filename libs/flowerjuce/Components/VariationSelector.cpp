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
        
        // Background color - teal if selected, dark gray if not
        juce::Colour bgColor = isSelected ? juce::Colour(0xff1eb19d) : juce::Colour(0xff333333);
        g.setColour(bgColor);
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
        
        // Border
        g.setColour(isSelected ? juce::Colour(0xff1eb19d) : juce::Colour(0xff666666));
        g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 2.0f);
        
        // Text (smaller font)
        g.setColour(isSelected ? juce::Colours::black : juce::Colour(0xfff3d430));
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
            setSelectedVariation(i);
            if (onVariationSelected)
                onVariationSelected(i);
            repaint();
            break;
        }
    }
}

void VariationSelector::setNumVariations(int numVariations)
{
    this->numVariations = juce::jmax(1, numVariations);
    selectedVariation = juce::jmin(selectedVariation, this->numVariations - 1);
    
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

