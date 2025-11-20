#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../../Engine/MultiTrackLooperEngine.h"
#include "../Shared/DualWaveformDisplay.h"
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
#include "ModelParamsPopup.h"
#include <memory>
#include <functional>
#include <utility>
#include <map>
#include <vector>
#include <cmath>

namespace WhAM
{

class RightClickSafeTextButton : public juce::TextButton
{
public:
    RightClickSafeTextButton(const juce::String& text = {})
        : juce::TextButton(text)
    {
    }

    std::function<void()> onLeftClick;

    void mouseDown(const juce::MouseEvent& event) override
    {
        suppressNextClick = event.mods.isPopupMenu();
        juce::TextButton::mouseDown(event);
    }

    void clicked() override
    {
        if (!suppressNextClick && onLeftClick)
            onLeftClick();
        suppressNextClick = false;
    }

private:
    bool suppressNextClick { false };
};

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
    
    juce::var getKnobState() const;
    void applyKnobState(const juce::var& state, juce::NotificationType notification = juce::sendNotificationSync);

    juce::var getCustomParams() const { return customVampNetParams; }
    void setCustomParams(const juce::var& params, juce::NotificationType notification = juce::dontSendNotification);

    // Mic button API
    bool isMicEnabled() const;
    void setMicEnabled(bool enabled);
    void updateMicButtonAvailability();
    int getSelectedInputChannel() const;
    void setSelectedInputChannel(int channel);
    int getSelectedOutputChannel() const;
    void setSelectedOutputChannel(int channel);
    bool hasInputAudio() const;
    bool hasOutputAudio() const;
    juce::Result loadInputAudioFromFile(const juce::File& file);
    juce::Result loadOutputAudioFromFile(const juce::File& file);
    juce::Result saveInputAudioToFile(const juce::File& file) const;
    juce::Result saveOutputAudioToFile(const juce::File& file) const;
    float getHighPassCutoffHz() const;
    void setHighPassCutoffHz(float hz);
    float getLowPassCutoffHz() const;
    void setLowPassCutoffHz(float hz);

    bool isAutogenEnabled() const { return loopModeButton.getToggleState(); }
    void setAutogenEnabled(bool enabled);

    bool isUseOutputAsInputEnabled() const { return useOutputAsInputEnabled; }
    void setUseOutputAsInputEnabled(bool enabled) { useOutputAsInputEnabled = enabled; }

    double getLevelDb() const;
    void setLevelDb(double value, juce::NotificationType notification);

    juce::var getPannerState() const;
    void applyPannerState(const juce::var& state);

    juce::String getTrackIdPrefix() const { return trackIdPrefix; }
    int getTrackIndex() const { return trackIndex; }
    juce::Colour getAccentColour() const { return accentColour; }

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
    juce::DrawableButton micIconButton;
    Shared::InputSelector inputSelector;
    Shared::OutputSelector outputSelector;
    
    // VampNet-specific UI
    juce::Label trackLabel;
    RightClickSafeTextButton resetButton;
    juce::TextButton generateButton;
    juce::TextButton configureParamsButton;
    juce::DrawableButton loopModeButton;
    RightClickSafeTextButton loadInputButton;
    RightClickSafeTextButton saveOutputButton;
    juce::Colour accentColour;
    bool useOutputAsInputEnabled { false };
    
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
    std::map<juce::String, juce::String> vampParamToKnobId;

    std::unique_ptr<ModelParamsPopup> modelParamsPopup;

    void applyLookAndFeel();

    void recordEnableButtonToggled(bool enabled);
    void playButtonClicked(bool shouldPlay);
    void muteButtonToggled(bool muted);
    void resetButtonClicked();
    void generateButtonClicked();
    void configureParamsButtonClicked();
    void loadInputButtonClicked();
    void saveOutputButtonClicked();
    
    void onVampNetComplete(juce::Result result, juce::File outputFile);
    
    void timerCallback() override;
    void initializeModelParameterKnobs();
    void syncCustomParamsToKnobs();
    double getCustomParamAsDouble(const juce::String& key, double defaultValue) const;
    void addModelParameterKnob(const juce::String& key,
                               const juce::String& label,
                               double min,
                               double max,
                               double interval,
                               double defaultValue,
                               bool isInteger,
                               bool isBoolean = false,
                               const juce::String& suffix = {});

    // MIDI learn support
    Shared::MidiLearnManager* midiLearnManager = nullptr;
    std::unique_ptr<Shared::MidiLearnable> generateButtonLearnable;
    std::unique_ptr<Shared::MidiLearnMouseListener> generateButtonMouseListener;
    std::unique_ptr<Shared::MidiLearnable> resetButtonLearnable;
    std::unique_ptr<Shared::MidiLearnMouseListener> resetButtonMouseListener;
    std::unique_ptr<Shared::MidiLearnable> micToggleLearnable;
    std::unique_ptr<Shared::MidiLearnMouseListener> micToggleMouseListener;
    juce::String trackIdPrefix;
    juce::String lastLoadedInputPath;
    juce::String lastSavedOutputPath;
    juce::String highPassParamId;
    juce::String lowPassParamId;

    Shared::ParameterKnobs* getModelParameterKnobComponent();
    const Shared::ParameterKnobs* getModelParameterKnobComponent() const;
    void showModelParamsPopup();
    void hideModelParamsPopup();
    void updateGenerateButtonMode();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperTrack)
};

} // namespace WhAM

