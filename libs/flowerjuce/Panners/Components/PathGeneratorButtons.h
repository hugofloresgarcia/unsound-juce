#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// Custom LookAndFeel that doesn't draw anything (for custom-drawn toggle buttons)
class EmptyToggleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override
    {
        // Draw nothing - we handle drawing in the parent component
    }
};

// Reusable component for panner path generation buttons
class PathGeneratorButtons : public juce::Component
{
public:
    PathGeneratorButtons();
    ~PathGeneratorButtons() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Callback when a path button is toggled
    // pathType: the type of path ("circle", "random", etc.)
    // isOn: true if button was toggled on, false if toggled off
    std::function<void(const juce::String& pathType, bool isOn)> onPathButtonToggled;
    
    // Reset all buttons to off state
    void resetAllButtons();
    
    // Trigger a random path (programmatically activate a random button)
    void triggerRandomPath();

private:
    juce::ToggleButton circlePathButton;
    juce::ToggleButton randomPathButton;
    juce::ToggleButton wanderPathButton;
    juce::ToggleButton swirlsPathButton;
    juce::ToggleButton bouncePathButton;
    juce::ToggleButton spiralPathButton;
    juce::ToggleButton horizontalLinePathButton;
    juce::ToggleButton verticalLinePathButton;
    
    // Empty look and feel for toggle buttons (so they don't draw themselves)
    EmptyToggleLookAndFeel emptyToggleLookAndFeel;
    
    // Store colors for each button
    juce::Colour circleColor{0xfff36e27};   // Orange
    juce::Colour randomColor{0xff4a90e2};   // Blue
    juce::Colour wanderColor{0xff1eb19d};   // Teal
    juce::Colour swirlsColor{0xffed1683};   // Pink
    juce::Colour bounceColor{0xff00ff00};   // Green
    juce::Colour spiralColor{0xfff3d430};   // Yellow
    juce::Colour horizontalLineColor{0xff00ffff}; // Cyan
    juce::Colour verticalLineColor{0xffff00ff};   // Magenta
    
    void drawCustomPathButton(juce::Graphics& g, juce::ToggleButton& button, 
                              const juce::String& label, juce::Rectangle<int> bounds,
                              juce::Colour color);
    
    void handleButtonToggle(juce::ToggleButton& clickedButton, const juce::String& pathType);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PathGeneratorButtons)
};

