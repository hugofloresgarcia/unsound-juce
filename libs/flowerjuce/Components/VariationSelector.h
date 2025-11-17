#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace Shared
{

class VariationSelector : public juce::Component
{
public:
    VariationSelector();
    ~VariationSelector() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    // Set the number of variations (default 2)
    void setNumVariations(int numVariations);
    
    // Set the currently selected variation (0-indexed)
    void setSelectedVariation(int variationIndex);
    
    // Get the currently selected variation
    int getSelectedVariation() const { return selectedVariation; }
    
    // Callback when a variation is clicked
    std::function<void(int variationIndex)> onVariationSelected;

private:
    int numVariations = 2;
    int selectedVariation = 0;
    
    static constexpr int boxWidth = 35;
    static constexpr int boxHeight = 25;
    static constexpr int boxSpacing = 5;
    
    juce::Rectangle<int> getBoxBounds(int index) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VariationSelector)
};

} // namespace Shared

