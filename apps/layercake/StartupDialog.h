#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>

class LayerCakeStartupDialog : public juce::Component,
                               private juce::Button::Listener
{
public:
    explicit LayerCakeStartupDialog(juce::AudioDeviceManager& deviceManager);

    bool wasOkClicked() const noexcept { return m_ok_clicked; }
    juce::AudioDeviceManager::AudioDeviceSetup getDeviceSetup() const;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void buttonClicked(juce::Button* button) override;
    juce::AudioDeviceManager& m_device_manager;
    juce::Label m_title_label;
    juce::Label m_hint_label;
    juce::AudioDeviceSelectorComponent m_device_selector;
    juce::TextButton m_ok_button;
    bool m_ok_clicked{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayerCakeStartupDialog)
};


