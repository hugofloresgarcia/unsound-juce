#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "MidiLearnManager.h"
#include "MidiLearnComponent.h"

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
    TransportControls(MidiLearnManager* midiManager, const juce::String& trackPrefix);
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

    // Show/hide record button
    void setRecordButtonVisible(bool visible);

private:
    juce::ToggleButton recordEnableButton;
    juce::ToggleButton playButton;
    juce::ToggleButton muteButton;
    juce::TextButton resetButton;
    
    EmptyToggleLookAndFeel emptyToggleLookAndFeel;
    
    // MIDI learn support
    MidiLearnManager* midiLearnManager = nullptr;
    juce::String trackIdPrefix;
    
    // Record button visibility
    bool recordButtonVisible = true;
    std::unique_ptr<MidiLearnable> recordLearnable;
    std::unique_ptr<MidiLearnable> playLearnable;
    std::unique_ptr<MidiLearnable> muteLearnable;
    std::unique_ptr<MidiLearnMouseListener> recordMouseListener;
    std::unique_ptr<MidiLearnMouseListener> playMouseListener;
    std::unique_ptr<MidiLearnMouseListener> muteMouseListener;

    void drawCustomToggleButton(juce::Graphics& g, juce::ToggleButton& button, 
                                const juce::String& letter, juce::Rectangle<int> bounds,
                                juce::Colour onColor, juce::Colour offColor,
                                bool showMidiIndicator = false);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportControls)
};

} // namespace Shared

