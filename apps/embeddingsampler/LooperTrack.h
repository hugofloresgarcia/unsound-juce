#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <flowerjuce/Engine/MultiTrackLooperEngine.h>
#include "CLAP/CLAPSearchWorkerThread.h"
#include "CLAP/ONNXModelManager.h"
#include <flowerjuce/Components/WaveformDisplay.h>
#include <flowerjuce/Components/TransportControls.h>
#include <flowerjuce/Components/ParameterKnobs.h>
#include <flowerjuce/Components/LevelControl.h>
#include <flowerjuce/Components/InputSelector.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Components/MidiLearnComponent.h>
#include <flowerjuce/Components/VariationSelector.h>
#include <flowerjuce/Engine/TapeLoop.h>
#include <flowerjuce/Panners/Panner.h>
#include <flowerjuce/Panners/StereoPanner.h>
#include <flowerjuce/Panners/QuadPanner.h>
#include <flowerjuce/Panners/CLEATPanner.h>
#include <flowerjuce/Panners/Panner2DComponent.h>
#include <flowerjuce/Panners/PathGeneratorButtons.h>
#include <flowerjuce/DSP/OnsetDetector.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <functional>
#include <array>

namespace Unsound4All
{

class LooperTrack : public juce::Component, public juce::Timer, public juce::AsyncUpdater
{
public:
    LooperTrack(MultiTrackLooperEngine& engine, int trackIndex, std::function<juce::String()> soundPalettePathProvider, Shared::MidiLearnManager* midiManager = nullptr, const juce::String& pannerType = "Stereo", ONNXModelManager* sharedModelManager = nullptr);
    ~LooperTrack() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void set_playback_speed(float speed);
    float get_playback_speed() const;
    
    juce::String getTextPrompt() const { return textPromptEditor.getText(); }
    
    // Update channel selectors based on current audio device
    void updateChannelSelectors();
    
    // Set panner smoothing time (called from MainComponent when settings change)
    void setPannerSmoothingTime(double smoothingTime);
    
    // Set CLEAT gain power (called from MainComponent when settings change)
    void setCLEATGainPower(float gainPower);
    
    // Get current pan position (returns false if panner not available)
    bool getPanPosition(float& x, float& y) const;
    
    // Clear LookAndFeel references from all child components
    void clearLookAndFeel();

private:
    MultiTrackLooperEngine& looperEngine;
    int trackIndex;

    // Shared components
    Shared::WaveformDisplay waveformDisplay;
    Shared::TransportControls transportControls;
    Shared::ParameterKnobs parameterKnobs;
    Shared::LevelControl levelControl;
    Shared::InputSelector inputSelector;
    Shared::VariationSelector variationSelector;
    
    // Unsound4All-specific UI
    juce::Label trackLabel;
    juce::TextButton resetButton;
    juce::TextButton generateButton;
    juce::TextEditor textPromptEditor;
    juce::Label textPromptLabel;
    juce::ToggleButton autogenToggle;
    
    // Progress display
    juce::String clapStatusText;
    
    // Panner
    juce::String pannerType;
    std::unique_ptr<Panner> panner;
    std::unique_ptr<Panner2DComponent> panner2DComponent;
    juce::Slider stereoPanSlider;
    juce::Label panLabel;
    juce::Label panCoordLabel;
    juce::ToggleButton trajectoryToggle;
    juce::ToggleButton onsetToggle;
    juce::TextButton saveTrajectoryButton;
    
    // Path generation buttons component
    std::unique_ptr<PathGeneratorButtons> pathGeneratorButtons;
    
    // Path control knobs
    juce::Slider pathSpeedKnob;
    juce::Label pathSpeedLabel;
    juce::Slider pathScaleKnob;
    juce::Label pathScaleLabel;
    
    // Filter cutoff knob
    juce::Slider cutoffKnob;
    juce::Label cutoffLabel;
    
    // Onset detector
    OnsetDetector onsetDetector;
    
    // Audio buffer for onset detection
    static constexpr int audioBufferSize = 1024;
    juce::AbstractFifo audioFifo{audioBufferSize};
    std::array<float, audioBufferSize> audioBuffer;
    std::atomic<bool> onsetDetected{false};
    std::atomic<bool> pendingTrajectoryAdvance{false};
    
    // Onset indicator LED state
    std::atomic<double> onsetLEDBrightness{0.0};
    std::atomic<double> lastOnsetLEDTime{0.0};
    static constexpr double onsetLEDDecayTime{0.2};
    
    // Onset detection processing state
    static constexpr int onsetBlockSize = 128;
    std::array<float, onsetBlockSize> onsetProcessingBuffer;
    std::atomic<int> onsetBufferFill{0};
    double lastOnsetSampleRate{44100.0};
    
    // Thread-safe flags
    std::atomic<bool> onsetToggleEnabled{false};
    std::atomic<bool> trajectoryPlaying{false};
    
    // Custom toggle button look and feel
    Shared::EmptyToggleLookAndFeel emptyToggleLookAndFeel;
    
    void feedAudioSample(float sample);
    
    std::unique_ptr<CLAPSearchWorkerThread> clapSearchWorkerThread;
    std::function<juce::String()> soundPalettePathProvider;
    ONNXModelManager* m_sharedModelManager;  // Optional shared model manager (for caching)
    
    void applyLookAndFeel();

    void playButtonClicked(bool shouldPlay);
    void muteButtonToggled(bool muted);
    void resetButtonClicked();
    void generateButtonClicked();
    void saveTrajectory();
    
    void onCLAPSearchComplete(juce::Result result, juce::Array<juce::File> outputFiles);
    
    void timerCallback() override;
    void handleAsyncUpdate() override;
    
    void drawCustomToggleButton(juce::Graphics& g, juce::ToggleButton& button, 
                                const juce::String& letter, juce::Rectangle<int> bounds,
                                juce::Colour onColor, juce::Colour offColor,
                                bool showMidiIndicator = false);
    
    void generatePath(const juce::String& pathType);
    
    // Variation management
    void switchToVariation(int variationIndex);
    void cycleToNextVariation();
    void loadVariationFromFile(int variationIndex, const juce::File& audioFile);
    void applyVariationsFromFiles(const juce::Array<juce::File>& outputFiles);
    
    // Storage for variations
    std::vector<std::unique_ptr<TapeLoop>> variations;
    int currentVariationIndex = 0;
    int numVariations = 4; // Default to 4 variations (top-4 matches from CLAP)
    bool autoCycleVariations = true;
    float m_last_read_head_position = 0.0f;
    
    // Pending variations waiting for loop end
    juce::Array<juce::File> pendingVariationFiles;
    bool hasPendingVariations = false;
    
    // Flag to wait for loop end before updating (when playing)
    bool waitForLoopEndBeforeUpdate = true;
    
    // MIDI learn support
    Shared::MidiLearnManager* midiLearnManager = nullptr;
    std::unique_ptr<Shared::MidiLearnable> generateButtonLearnable;
    std::unique_ptr<Shared::MidiLearnMouseListener> generateButtonMouseListener;
    juce::String trackIdPrefix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperTrack)
};

} // namespace Unsound4All

