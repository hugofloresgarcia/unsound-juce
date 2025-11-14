#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace Shared
{

// Custom LookAndFeel that doesn't draw anything (for custom-drawn toggle buttons)
class EmptyToggleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override
    {
        // Draw nothing - we handle drawing in the parent component
    }
};

class TransportControls : public juce::Component
{
public:
    TransportControls();
    ~TransportControls() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Callbacks for button actions
    std::function<void(bool)> onRecordToggle;
    std::function<void(bool)> onPlayToggle;
    std::function<void(bool)> onMuteToggle;
    std::function<void()> onReset;

    // Sync button states from external source
    void setRecordState(bool enabled);
    void setPlayState(bool playing);
    void setMuteState(bool muted);

private:
    juce::ToggleButton recordEnableButton;
    juce::ToggleButton playButton;
    juce::ToggleButton muteButton;
    juce::TextButton resetButton;
    
    EmptyToggleLookAndFeel emptyToggleLookAndFeel;

    void drawCustomToggleButton(juce::Graphics& g, juce::ToggleButton& button, 
                                const juce::String& letter, juce::Rectangle<int> bounds,
                                juce::Colour onColor, juce::Colour offColor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportControls)
};

} // namespace Shared

