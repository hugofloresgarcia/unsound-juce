#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../../Engine/MultiTrackLooperEngine.h"
#include "LooperTrack.h"
#include "../../CustomLookAndFeel.h"

namespace VampNet
{

class MainComponent : public juce::Component,
                      public juce::Timer
{
public:
    MainComponent(int numTracks = 8);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    
    MultiTrackLooperEngine& getLooperEngine() { return looperEngine; }

private:
    MultiTrackLooperEngine looperEngine;
    std::vector<std::unique_ptr<VampNet::LooperTrack>> tracks;
    
    juce::TextButton syncButton;
    juce::TextButton audioSettingsButton;
    juce::TextButton gradioSettingsButton;
    juce::Label titleLabel;

    juce::DialogWindow* audioSettingsWindow = nullptr;
    CustomLookAndFeel customLookAndFeel;
    juce::String gradioUrl { "https://hugggof-vampnet-music.hf.space/" };
    mutable juce::CriticalSection gradioSettingsLock;

    void syncButtonClicked();
    void audioSettingsButtonClicked();
    void showAudioSettings();
    void gradioSettingsButtonClicked();
    void showGradioSettings();
    void setGradioUrl(const juce::String& newUrl);
    juce::String getGradioUrl() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace VampNet

