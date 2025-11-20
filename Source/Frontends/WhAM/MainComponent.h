#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../VampNet/ClickSynth.h"
#include "../VampNet/Sampler.h"
#include "../../Engine/MultiTrackLooperEngine.h"
#include "LooperTrack.h"
#include "Synths.h"
#include "TokenVisualizer.h"
#include "../../CustomLookAndFeel.h"
#include "../Shared/MidiLearnManager.h"
#include "../Shared/MidiLearnComponent.h"
#include "../Shared/GitInfo.h"
#include "SessionConfig.h"

namespace WhAM
{

class MainComponent : public juce::Component,
                      public juce::Timer,
                      public juce::KeyListener
{
public:
    MainComponent(int numTracks = 8, const juce::String& pannerType = "Stereo");
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    bool keyStateChanged(bool isKeyDown, juce::Component* originatingComponent) override;
    
    VampNetMultiTrackLooperEngine& getLooperEngine() { return looperEngine; }

private:
    VampNetMultiTrackLooperEngine looperEngine;
    
    // MIDI learn support - must be declared before tracks so it's destroyed after them
    Shared::MidiLearnManager midiLearnManager;
    
    std::vector<std::unique_ptr<WhAM::LooperTrack>> tracks;
    juce::Viewport trackViewport;
    juce::Component tracksContainer;
    
    // Track selection and recording state
    int activeTrackIndex = 0;  // Currently selected track (0-7)
    bool isRecordingHeld = false;  // True while 'r' key is held
    
    juce::TextButton syncButton;
    juce::TextButton settingsButton;
    juce::TextButton synthsButton;
    juce::TextButton vizButton;
    juce::TextButton saveConfigButton;
    juce::TextButton loadConfigButton;
    juce::TextButton overflowButton;  // "..." button for overflow menu
    juce::ImageButton headerLogoButton;
    juce::Label gitInfoLabel;
    CustomLookAndFeel customLookAndFeel;
    juce::String gradioUrl { "http://127.0.0.1:7860/" };
    mutable juce::CriticalSection gradioSettingsLock;
    juce::Image whamLogoImage;

    Shared::MidiLearnOverlay midiLearnOverlay;

    // Synths window (combines Click Synth and Sampler)
    std::unique_ptr<SynthsWindow> synthsWindow;
    
    // Token visualizer window
    std::unique_ptr<TokenVisualizerWindow> vizWindow;
    Shared::GitInfo gitInfo;
    juce::String audioDeviceSummary { "detecting audio devices..." };

    void syncButtonClicked();
    void showSynthsWindow();
    void showVizWindow();
    void settingsButtonClicked();
    void updateAudioDeviceDebugInfo();
    void showSettings();
    void setGradioUrl(const juce::String& newUrl);
    juce::String getGradioUrl() const;
    void showOverflowMenu();
    void saveConfigButtonClicked();
    void loadConfigButtonClicked();
    void loadDefaultSessionConfig();
    SessionConfig buildSessionConfig() const;
    void applySessionConfig(const SessionConfig& config, bool showErrorsOnFailure);
    void writeSessionAudioAssets(SessionConfig& config, const juce::File& sessionFile);
    juce::File getConfigDirectory() const;
    juce::File getDefaultConfigFile() const;
    void layoutTracks();
    void refreshGitInfoLabel();
    void showAboutDialog();
    juce::String getAudioDeviceSummary() const { return audioDeviceSummary; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace WhAM

