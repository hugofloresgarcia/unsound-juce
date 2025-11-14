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
#include <functional>
#include <utility>

namespace Shared
{
    class ModelParameterDialog;
}

namespace VampNet
{

// Background thread for VampNet Gradio API calls
class VampNetWorkerThread : public juce::Thread
{
public:
    VampNetWorkerThread(MultiTrackLooperEngine& engine,
                       int trackIndex,
                       const juce::File& audioFile,
                       float periodicPrompt,
                       const juce::var& customParams,
                       std::function<juce::String()> gradioUrlProvider)
        : Thread("VampNetWorkerThread"),
          looperEngine(engine),
          trackIndex(trackIndex),
          audioFile(audioFile),
          periodicPrompt(periodicPrompt),
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
    float periodicPrompt;
    juce::var customParams;
    std::function<juce::String()> gradioUrlProvider;
    
    juce::Result saveBufferToFile(int trackIndex, juce::File& outputFile);
    juce::Result callVampNetAPI(const juce::File& inputAudioFile, float periodicPrompt, const juce::var& customParams, juce::File& outputFile);
};

class LooperTrack : public juce::Component, public juce::Timer
{
public:
    LooperTrack(MultiTrackLooperEngine& engine, int trackIndex, std::function<juce::String()> gradioUrlProvider);
    ~LooperTrack() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setPlaybackSpeed(float speed);
    float getPlaybackSpeed() const;
    
    float getPeriodicPrompt() const;
    
    // Public static method to get default parameters
    static juce::var getDefaultVampNetParams();

private:
    MultiTrackLooperEngine& looperEngine;
    int trackIndex;

    // Shared components
    Shared::WaveformDisplay waveformDisplay;
    Shared::TransportControls transportControls;
    Shared::ParameterKnobs parameterKnobs;
    Shared::LevelControl levelControl;
    Shared::OutputSelector outputSelector;
    
    // VampNet-specific UI
    juce::Label trackLabel;
    juce::TextButton resetButton;
    juce::TextButton generateButton;
    juce::TextButton configureParamsButton;
    
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperTrack)
};

} // namespace VampNet
