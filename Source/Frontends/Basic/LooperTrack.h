#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../../Engine/MultiTrackLooperEngine.h"
#include "../Shared/WaveformDisplay.h"
#include "../Shared/TransportControls.h"
#include "../Shared/ParameterKnobs.h"
#include "../Shared/LevelControl.h"
#include "../Shared/OutputSelector.h"
#include <memory>

namespace Basic
{

class LooperTrack : public juce::Component, public juce::Timer
{
public:
    LooperTrack(MultiTrackLooperEngine& engine, int trackIndex);
    ~LooperTrack() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setPlaybackSpeed(float speed);
    float getPlaybackSpeed() const;

private:
    MultiTrackLooperEngine& looperEngine;
    int trackIndex;

    // Shared components
    Shared::WaveformDisplay waveformDisplay;
    Shared::TransportControls transportControls;
    Shared::ParameterKnobs parameterKnobs;
    Shared::LevelControl levelControl;
    Shared::OutputSelector outputSelector;
    
    // Track-specific UI
    juce::Label trackLabel;
    juce::TextButton resetButton;
    
    void applyLookAndFeel();

    void recordEnableButtonToggled(bool enabled);
    void playButtonClicked(bool shouldPlay);
    void muteButtonToggled(bool muted);
    void resetButtonClicked();
    
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperTrack)
};

} // namespace Basic
