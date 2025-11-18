#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include "../../Engine/MultiTrackLooperEngine.h"
#include "../Shared/MidiLearnManager.h"
#include "../Shared/MidiLearnComponent.h"

namespace WhAM
{

// SynthsWindow - Combined UI window for Beep and Sampler
class SynthsWindow : public juce::DialogWindow
{
public:
    SynthsWindow(VampNetMultiTrackLooperEngine& engine, int numTracks, Shared::MidiLearnManager* midiManager = nullptr);
    ~SynthsWindow() override;

    // Serialize/restore synth configuration for session configs
    juce::var getState() const;
    void applyState(const juce::var& state);

    void closeButtonPressed() override;

    int getSelectedTrack() const;
    bool isEnabled() const;

private:
    class ContentComponent : public juce::Component
    {
    public:
        ContentComponent(VampNetMultiTrackLooperEngine& engine, int numTracks, Shared::MidiLearnManager* midiManager);
        ~ContentComponent() override;
        
        void paint(juce::Graphics& g) override;
        void resized() override;

        int getSelectedTrack() const;
        bool isEnabled() const;

        juce::var getState() const;
        void applyState(const juce::var& state);

    private:
        VampNetMultiTrackLooperEngine& looperEngine;
        Shared::MidiLearnManager* midiLearnManager;
        
        // Tab buttons
        juce::TextButton clickSynthTabButton;
        juce::TextButton samplerTabButton;
        std::atomic<bool> showingClickSynth{false};  // false = showing Sampler first
        
        // Beep (Click Synth) controls
        juce::ToggleButton clickSynthEnableButton;
        juce::Label clickSynthTrackLabel;
        juce::ComboBox clickSynthTrackSelector;
        juce::Label frequencyLabel;
        juce::Slider frequencySlider;
        juce::Label durationLabel;
        juce::Slider durationSlider;
        juce::Label amplitudeLabel;
        juce::Slider amplitudeSlider;
        juce::TextButton clickSynthTriggerButton;
        juce::Label clickSynthInstructionsLabel;
        std::atomic<bool> clickSynthEnabled{false};
        std::unique_ptr<Shared::MidiLearnable> clickSynthTriggerLearnable;
        std::unique_ptr<Shared::MidiLearnMouseListener> clickSynthTriggerMouseListener;
        juce::String clickSynthParameterId;
        
        // Sampler controls
        juce::ToggleButton samplerEnableButton;
        juce::Label samplerTrackLabel;
        juce::ComboBox samplerTrackSelector;
        juce::TextButton loadSampleButton;
        juce::TextButton samplerTriggerButton;
        juce::Label sampleNameLabel;
        juce::Label samplerInstructionsLabel;
        std::atomic<bool> samplerEnabled{false};
        std::unique_ptr<Shared::MidiLearnable> samplerTriggerLearnable;
        std::unique_ptr<Shared::MidiLearnMouseListener> samplerTriggerMouseListener;
        juce::String samplerParameterId;
        
        // Shared
        std::atomic<int> selectedTrack{0};
        juce::String lastSampleFilePath;

        void tabButtonClicked(int tabIndex);
        void clickSynthEnableButtonChanged();
        void clickSynthTrackSelectorChanged();
        void frequencySliderChanged();
        void durationSliderChanged();
        void amplitudeSliderChanged();
        void clickSynthTriggerButtonClicked();
        void samplerEnableButtonChanged();
        void samplerTrackSelectorChanged();
        void loadSampleButtonClicked();
        void samplerTriggerButtonClicked();
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ContentComponent)
    };
    
    ContentComponent* contentComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthsWindow)
};

} // namespace WhAM

