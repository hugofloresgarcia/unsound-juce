#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <flowerjuce/Engine/MultiTrackLooperEngine.h>
#include <flowerjuce/Components/WaveformDisplay.h>
#include <flowerjuce/Components/TransportControls.h>
#include <flowerjuce/Components/ParameterKnobs.h>
#include <flowerjuce/Components/LevelControl.h>
#include <flowerjuce/Components/OutputSelector.h>
#include <flowerjuce/Components/InputSelector.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Panners/Panner.h>
#include <flowerjuce/Panners/StereoPanner.h>
#include <flowerjuce/Panners/QuadPanner.h>
#include <flowerjuce/Panners/CLEATPanner.h>
#include <flowerjuce/Panners/Panner2DComponent.h>
#include <memory>

namespace Basic
{

class LooperTrack : public juce::Component, public juce::Timer
{
public:
    LooperTrack(MultiTrackLooperEngine& engine, int trackIndex, Shared::MidiLearnManager* midiManager = nullptr, const juce::String& pannerType = "Stereo");
    ~LooperTrack() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setPlaybackSpeed(float speed);
    float getPlaybackSpeed() const;
    
    // Update channel selectors based on current audio device
    void updateChannelSelectors();

private:
    MultiTrackLooperEngine& looperEngine;
    int trackIndex;

    // Shared components
    Shared::WaveformDisplay waveformDisplay;
    Shared::TransportControls transportControls;
    Shared::ParameterKnobs parameterKnobs;
    Shared::LevelControl levelControl;
    Shared::InputSelector inputSelector;
    Shared::OutputSelector outputSelector;
    
    // Track-specific UI
    juce::Label trackLabel;
    juce::TextButton resetButton;
    
    // Panner
    juce::String pannerType;
    std::unique_ptr<Panner> panner;
    std::unique_ptr<Panner2DComponent> panner2DComponent;
    juce::Slider stereoPanSlider; // For stereo panner
    juce::Label panLabel;
    juce::Label panCoordLabel; // Shows pan coordinates (x, y)
    
    void applyLookAndFeel();

    void recordEnableButtonToggled(bool enabled);
    void playButtonClicked(bool shouldPlay);
    void muteButtonToggled(bool muted);
    void resetButtonClicked();
    
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperTrack)
};

} // namespace Basic
