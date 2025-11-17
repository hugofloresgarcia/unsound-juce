#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <flowerjuce/Engine/MultiTrackLooperEngine.h>
#include <flowerjuce/Components/DualWaveformDisplay.h>
#include <flowerjuce/Components/TransportControls.h>
#include <flowerjuce/Components/ParameterKnobs.h>
#include <flowerjuce/Components/LevelControl.h>
#include <flowerjuce/Components/OutputSelector.h>
#include <flowerjuce/Components/InputSelector.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Components/MidiLearnComponent.h>
#include <flowerjuce/Panners/Panner.h>
#include <flowerjuce/Panners/StereoPanner.h>
#include <flowerjuce/Panners/QuadPanner.h>
#include <flowerjuce/Panners/CLEATPanner.h>
#include <flowerjuce/Panners/Panner2DComponent.h>
#include <memory>
#include <functional>
#include <utility>

namespace Shared
{
    class ModelParameterDialog;
}

namespace WhAM
{

// Background thread for VampNet Gradio API calls
class VampNetWorkerThread : public juce::Thread
{
public:
    VampNetWorkerThread(VampNetMultiTrackLooperEngine& engine,
                       int trackIndex,
                       const juce::File& audioFile,
                       float periodicPrompt,
                       const juce::var& customParams,
                       std::function<juce::String()> gradioUrlProvider,
                       bool useOutputBuffer = false)
        : Thread("VampNetWorkerThread"),
          looperEngine(engine),
          trackIndex(trackIndex),
          audioFile(audioFile),
          periodicPrompt(periodicPrompt),
          customParams(customParams),
          gradioUrlProvider(std::move(gradioUrlProvider)),
          useOutputBuffer(useOutputBuffer)
    {
    }

    void run() override;

    std::function<void(juce::Result, juce::File, int)> onComplete;

private:
    VampNetMultiTrackLooperEngine& looperEngine;
    int trackIndex;
    juce::File audioFile;
    float periodicPrompt;
    juce::var customParams;
    std::function<juce::String()> gradioUrlProvider;
    bool useOutputBuffer;
    
    juce::Result saveBufferToFile(int trackIndex, juce::File& outputFile);
    juce::Result callVampNetAPI(const juce::File& inputAudioFile, float periodicPrompt, const juce::var& customParams, juce::File& outputFile);
};

class LooperTrack : public juce::Component, public juce::Timer
{
public:
    LooperTrack(VampNetMultiTrackLooperEngine& engine, int trackIndex, std::function<juce::String()> gradioUrlProvider, Shared::MidiLearnManager* midiManager = nullptr, const juce::String& pannerType = "Stereo");
    ~LooperTrack() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setPlaybackSpeed(float speed);
    float getPlaybackSpeed() const;
    
    float getPeriodicPrompt() const;
    
    // Update channel selectors based on current audio device
    void updateChannelSelectors();
    
    // Public static method to get default parameters
    static juce::var getDefaultVampNetParams();
    
    // Check if generation is currently in progress
    bool isGenerating() const;
    
    // Trigger generation programmatically (e.g., from keyboard shortcut)
    void triggerGeneration();

private:
    VampNetMultiTrackLooperEngine& looperEngine;
    int trackIndex;

    // Shared components
    Shared::DualWaveformDisplay waveformDisplay;
    Shared::TransportControls transportControls;
    Shared::ParameterKnobs parameterKnobs;
    Shared::LevelControl levelControl;
    Shared::InputSelector inputSelector;
    Shared::OutputSelector outputSelector;
    
    // VampNet-specific UI
    juce::Label trackLabel;
    juce::TextButton resetButton;
    juce::TextButton generateButton;
    juce::TextButton configureParamsButton;
    juce::ToggleButton useOutputAsInputToggle;
    juce::ToggleButton autogenToggle;
    
    // Panner
    juce::String pannerType;
    std::unique_ptr<Panner> panner;
    std::unique_ptr<Panner2DComponent> panner2DComponent;
    juce::Slider stereoPanSlider; // For stereo panner
    juce::Label panLabel;
    juce::Label panCoordLabel; // Shows pan coordinates (x, y)
    
    std::unique_ptr<VampNetWorkerThread> vampNetWorkerThread;
    std::function<juce::String()> gradioUrlProvider;
    
    // Custom VampNet parameters (excluding periodic prompt which is in UI)
    juce::var customVampNetParams;
    
    // Parameter configuration dialog
    std::unique_ptr<Shared::ModelParameterDialog> parameterDialog;
    
    void applyLookAndFeel();

    void recordEnableButtonToggled(bool enabled);
    void playButtonClicked(bool shouldPlay);
    void muteButtonToggled(bool muted);
    void resetButtonClicked();
    void generateButtonClicked();
    void configureParamsButtonClicked();
    
    void onVampNetComplete(juce::Result result, juce::File outputFile);
    
    void timerCallback() override;
    
    // MIDI learn support
    Shared::MidiLearnManager* midiLearnManager = nullptr;
    std::unique_ptr<Shared::MidiLearnable> generateButtonLearnable;
    std::unique_ptr<Shared::MidiLearnMouseListener> generateButtonMouseListener;
    juce::String trackIdPrefix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperTrack)
};

} // namespace WhAM

