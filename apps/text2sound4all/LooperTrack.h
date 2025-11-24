#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <flowerjuce/LooperEngine/MultiTrackLooperEngine.h>
#include <flowerjuce/GradioClient/GradioClient.h>
#include <flowerjuce/Components/WaveformDisplay.h>
#include <flowerjuce/Components/TransportControls.h>
#include <flowerjuce/Components/ParameterKnobs.h>
#include <flowerjuce/Components/LevelControl.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Components/MidiLearnComponent.h>
#include <flowerjuce/Components/VariationSelector.h>
#include <flowerjuce/LooperEngine/TapeLoop.h>
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
#include <utility>
#include <array>

namespace Shared
{
    class ModelParameterDialog;
}

namespace Text2Sound
{
    // Panner type enumeration
    enum class PannerType
    {
        Stereo,
        Quad,
        CLEAT
    };
    
    // Helper function to convert string to PannerType enum
    inline PannerType stringToPannerType(const juce::String& pannerTypeStr)
    {
        auto lower = pannerTypeStr.toLowerCase();
        if (lower == "stereo")
            return PannerType::Stereo;
        else if (lower == "quad")
            return PannerType::Quad;
        else if (lower == "cleat")
            return PannerType::CLEAT;
        else
            return PannerType::Stereo; // Default fallback
    }

    // Background thread for Gradio API calls
class GradioWorkerThread : public juce::Thread
{
public:
    GradioWorkerThread(MultiTrackLooperEngine& engine,
                       int trackIndex,
                       const juce::File& audioFile,
                       const juce::String& textPrompt,
                       const juce::var& customText2SoundParams,
                       std::function<juce::String()> gradioUrlProvider)
        : Thread("GradioWorkerThread"),
          looperEngine(engine),
          trackIndex(trackIndex),
          audioFile(audioFile),
          textPrompt(textPrompt),
          customText2SoundParams(customText2SoundParams),
          gradioUrlProvider(std::move(gradioUrlProvider))
    {
    }

    void run() override;

    std::function<void(juce::Result, juce::Array<juce::File>, int)> onComplete;
    std::function<void(const juce::String& statusText)> onStatusUpdate;

private:
    MultiTrackLooperEngine& looperEngine;
    int trackIndex;
    juce::File audioFile;
    juce::String textPrompt;
    juce::var customText2SoundParams;
    GradioClient gradioClient;
    std::function<juce::String()> gradioUrlProvider;
    
    juce::Result saveBufferToFile(int trackIndex, juce::File& outputFile);
};

class LooperTrack : public juce::Component, public juce::Timer, public juce::AsyncUpdater
{
public:
    LooperTrack(MultiTrackLooperEngine& engine, int trackIndex, std::function<juce::String()> gradioUrlProvider, Shared::MidiLearnManager* midiManager = nullptr, const juce::String& pannerTypeStr = "Stereo");
    ~LooperTrack() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void set_playback_speed(float speed);
    float get_playback_speed() const;
    
    juce::String getTextPrompt() const { return textPromptEditor.getText(); }
    
    // Update model parameters (called from MainComponent when shared params change)
    void updateModelParams(const juce::var& newParams);
    
    // Set panner smoothing time (called from MainComponent when settings change)
    void setPannerSmoothingTime(double smoothingTime);
    
    // Set CLEAT gain power (called from MainComponent when settings change)
    void setCLEATGainPower(float gainPower);
    
    // Set generate triggers new path setting (called from MainComponent when settings change)
    void setGenerateTriggersNewPath(bool enabled);
    
    // Get current pan position (returns false if panner not available)
    bool getPanPosition(float& x, float& y) const;
    
    // Public static method to get default parameters
    static juce::var getDefaultText2SoundParams();
    
    // Clear LookAndFeel references from all child components
    // Called by MainComponent during shutdown to prevent assertion
    void clearLookAndFeel();

private:
    MultiTrackLooperEngine& looperEngine;
    int trackIndex;

    // Shared components
    Shared::WaveformDisplay waveformDisplay;
    Shared::TransportControls transportControls;
    Shared::ParameterKnobs parameterKnobs;
    Shared::LevelControl levelControl;
    Shared::VariationSelector variationSelector;
    
    // Text2Sound-specific UI
    juce::Label trackLabel;
    juce::TextButton resetButton;
    juce::TextButton generateButton;
    juce::TextEditor textPromptEditor;
    juce::Label textPromptLabel;
    juce::ToggleButton autogenToggle;
    
    // Progress display
    juce::String gradioStatusText;
    
    // Panner
    PannerType pannerType;
    std::unique_ptr<Panner> panner;
    std::unique_ptr<Panner2DComponent> panner2DComponent;
    juce::Slider stereoPanSlider; // For stereo panner
    juce::Label panLabel;
    juce::Label panCoordLabel; // Shows pan coordinates (x, y)
    juce::ToggleButton trajectoryToggle; // [tr] toggle for trajectory recording
    juce::ToggleButton onsetToggle; // [o] toggle for onset-based triggering
    juce::TextButton saveTrajectoryButton; // [sv~] button for saving trajectory
    
    // Path generation buttons component
    std::unique_ptr<PathGeneratorButtons> pathGeneratorButtons;
    
    // Path control knobs
    juce::Slider pathSpeedKnob;
    juce::Label pathSpeedLabel;
    juce::Slider pathScaleKnob;
    juce::Label pathScaleLabel;
    
    // Filter cutoff knob (above level control)
    juce::Slider cutoffKnob;
    juce::Label cutoffLabel;
    
    // Onset detector for audio analysis
    OnsetDetector onsetDetector;
    
    // Audio buffer for onset detection (thread-safe circular buffer)
    static constexpr int audioBufferSize = 1024;
    juce::AbstractFifo audioFifo{audioBufferSize};
    std::array<float, audioBufferSize> audioBuffer;
    std::atomic<bool> onsetDetected{false};
    std::atomic<bool> pendingTrajectoryAdvance{false}; // Flag to advance trajectory on message thread
    
    // Onset indicator LED state (for visual feedback)
    std::atomic<double> onsetLEDBrightness{0.0}; // 0.0 to 1.0, fades out over time
    std::atomic<double> lastOnsetLEDTime{0.0};
    static constexpr double onsetLEDDecayTime{0.2}; // LED stays lit for 200ms
    
    // Onset detection processing state (accessed from audio thread)
    static constexpr int onsetBlockSize = 128; // Process in small blocks for low latency (~2.9ms at 44.1kHz)
    std::array<float, onsetBlockSize> onsetProcessingBuffer;
    std::atomic<int> onsetBufferFill{0}; // Thread-safe counter
    double lastOnsetSampleRate{44100.0};
    
    // Thread-safe flags for audio thread access
    std::atomic<bool> onsetToggleEnabled{false}; // Cached from UI thread
    std::atomic<bool> trajectoryPlaying{false}; // Cached from panner state
    
    // Custom toggle button look and feel (similar to TransportControls)
    Shared::EmptyToggleLookAndFeel emptyToggleLookAndFeel;
    
    // Method to feed audio samples to onset detector (called from audio thread)
    void feedAudioSample(float sample);
    
    std::unique_ptr<GradioWorkerThread> gradioWorkerThread;
    std::function<juce::String()> gradioUrlProvider;
    
    // Custom Text2Sound parameters (excluding text prompt which is in UI)
    // These are shared across all tracks and updated by MainComponent
    juce::var customText2SoundParams;
    
    void applyLookAndFeel();

    void playButtonClicked(bool shouldPlay);
    void muteButtonToggled(bool muted);
    void resetButtonClicked();
    void generateButtonClicked();
    void saveTrajectory();
    
    void onGradioComplete(juce::Result result, juce::Array<juce::File> outputFiles);
    
    void timerCallback() override;
    void handleAsyncUpdate() override; // For immediate onset detection updates from audio thread
    
    // Helper method to draw custom toggle buttons (similar to TransportControls)
    void drawCustomToggleButton(juce::Graphics& g, juce::ToggleButton& button, 
                                const juce::String& letter, juce::Rectangle<int> bounds,
                                juce::Colour onColor, juce::Colour offColor,
                                bool showMidiIndicator = false);
    
    // Generate and set a path based on type
    void generatePath(const juce::String& pathType);
    
    // Variation management
    void switchToVariation(int variationIndex);
    void cycleToNextVariation();
    void loadVariationFromFile(int variationIndex, const juce::File& audioFile);
    void applyVariationsFromFiles(const juce::Array<juce::File>& outputFiles);
    
    // Storage for variations (each variation has its own TapeLoop)
    std::vector<std::unique_ptr<TapeLoop>> variations;
    int currentVariationIndex = 0;
    int numVariations = 2; // Default to 2 variations
    bool autoCycleVariations = true;
    float m_last_read_head_position = 0.0f; // Track position for wrap detection
    
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
    
    // Generate triggers new path setting
    bool generateTriggersNewPath{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperTrack)
};

} // namespace Text2Sound
