#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../../Engine/MultiTrackLooperEngine.h"
#include "../../GradioClient/GradioClient.h"
#include "../Shared/WaveformDisplay.h"
#include "../Shared/TransportControls.h"
#include "../Shared/ParameterKnobs.h"
#include "../Shared/LevelControl.h"
#include "../Shared/OutputSelector.h"
#include "../Shared/InputSelector.h"
#include "../Shared/MidiLearnManager.h"
#include "../Shared/MidiLearnComponent.h"
#include "../../Panners/Panner.h"
#include "../../Panners/StereoPanner.h"
#include "../../Panners/QuadPanner.h"
#include "../../Panners/CLEATPanner.h"
#include "../../Panners/Panner2DComponent.h"
#include <memory>
#include <functional>
#include <utility>

namespace Shared
{
    class ModelParameterDialog;
}

namespace Text2Sound
{

// Background thread for Gradio API calls
class GradioWorkerThread : public juce::Thread
{
public:
    GradioWorkerThread(MultiTrackLooperEngine& engine,
                       int trackIndex,
                       const juce::File& audioFile,
                       const juce::String& textPrompt,
                       const juce::var& customParams,
                       std::function<juce::String()> gradioUrlProvider)
        : Thread("GradioWorkerThread"),
          looperEngine(engine),
          trackIndex(trackIndex),
          audioFile(audioFile),
          textPrompt(textPrompt),
          customParams(customParams),
          gradioUrlProvider(std::move(gradioUrlProvider))
    {
    }

    void run() override;

    std::function<void(juce::Result, juce::File, int)> onComplete;

private:
    MultiTrackLooperEngine& looperEngine;
    int trackIndex;
    juce::File audioFile;
    juce::String textPrompt;
    juce::var customParams;
    GradioClient gradioClient;
    std::function<juce::String()> gradioUrlProvider;
    
    juce::Result saveBufferToFile(int trackIndex, juce::File& outputFile);
};

class LooperTrack : public juce::Component, public juce::Timer
{
public:
    LooperTrack(MultiTrackLooperEngine& engine, int trackIndex, std::function<juce::String()> gradioUrlProvider, Shared::MidiLearnManager* midiManager = nullptr, const juce::String& pannerType = "Stereo");
    ~LooperTrack() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setPlaybackSpeed(float speed);
    float getPlaybackSpeed() const;
    
    juce::String getTextPrompt() const { return textPromptEditor.getText(); }
    
    // Update channel selectors based on current audio device
    void updateChannelSelectors();
    
    // Public static method to get default parameters
    static juce::var getDefaultText2SoundParams();

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
    
    // Text2Sound-specific UI
    juce::Label trackLabel;
    juce::TextButton resetButton;
    juce::TextButton generateButton;
    juce::TextButton configureParamsButton;
    juce::TextEditor textPromptEditor;
    juce::Label textPromptLabel;
    juce::ToggleButton autogenToggle;
    
    // Panner
    juce::String pannerType;
    std::unique_ptr<Panner> panner;
    std::unique_ptr<Panner2DComponent> panner2DComponent;
    juce::Slider stereoPanSlider; // For stereo panner
    juce::Label panLabel;
    juce::Label panCoordLabel; // Shows pan coordinates (x, y)
    
    std::unique_ptr<GradioWorkerThread> gradioWorkerThread;
    std::function<juce::String()> gradioUrlProvider;
    
    // Custom Text2Sound parameters (excluding text prompt which is in UI)
    juce::var customText2SoundParams;
    
    // Parameter configuration dialog
    std::unique_ptr<Shared::ModelParameterDialog> parameterDialog;
    
    void applyLookAndFeel();

    void recordEnableButtonToggled(bool enabled);
    void playButtonClicked(bool shouldPlay);
    void muteButtonToggled(bool muted);
    void resetButtonClicked();
    void generateButtonClicked();
    void configureParamsButtonClicked();
    
    void onGradioComplete(juce::Result result, juce::File outputFile);
    
    void timerCallback() override;
    
    // MIDI learn support
    Shared::MidiLearnManager* midiLearnManager = nullptr;
    std::unique_ptr<Shared::MidiLearnable> generateButtonLearnable;
    std::unique_ptr<Shared::MidiLearnMouseListener> generateButtonMouseListener;
    juce::String trackIdPrefix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperTrack)
};

} // namespace Text2Sound
