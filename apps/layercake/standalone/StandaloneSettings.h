#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <flowerjuce/LayerCakeEngine/LayerCakeEngine.h>
#include "../core/LayerCakeSettings.h"

namespace LayerCakeApp
{

class StandaloneSettingsComponent : public juce::Component
{
public:
    StandaloneSettingsComponent(juce::AudioDeviceManager& deviceManager, LayerCakeEngine& engine)
        : m_device_manager(deviceManager), m_engine(engine)
    {
        m_audio_section_label.setText("Audio Device", juce::dontSendNotification);
        m_audio_section_label.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
        addAndMakeVisible(m_audio_section_label);
        
        m_device_selector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            m_device_manager,
            0, 256, 0, 256, false, false, true, false
        );
        addAndMakeVisible(m_device_selector.get());
        
        // Add explicit enable button just in case device selector isn't clear enough
        // Actually AudioDeviceSelectorComponent handles opening/closing device when you select one.
        // But we want a big "ENABLE AUDIO" toggle like before?
        // The previous one had m_audio_enable_toggle.
        m_audio_enable_toggle.setButtonText("Enable Audio Processing");
        m_audio_enable_toggle.setToggleState(true, juce::dontSendNotification); // Default true for standalone?
        // Actually standalone starts with audio enabled in main.cpp: m_device_manager.initialise
        // So this toggle should reflect that.
        // But AudioDeviceManager doesn't have a simple "enabled" bool, it has a current device.
        // If current device is null, it's disabled.
        
        m_audio_enable_toggle.onClick = [this] {
            // This logic is tricky with AudioDeviceSelectorComponent also managing state.
            // Let's just make it a dummy "Re-initialize" button or similar if needed?
            // Or just trust the selector.
            // User asked for "Enable Audio" button.
            // If we treat it as a master switch:
            if (m_audio_enable_toggle.getToggleState())
            {
                // Re-init with defaults if no device
                if (!m_device_manager.getCurrentAudioDevice())
                    m_device_manager.initialise(2, 2, nullptr, true);
            }
            else
            {
                m_device_manager.closeAudioDevice();
            }
        };
        // Check initial state
        m_audio_enable_toggle.setToggleState(m_device_manager.getCurrentAudioDevice() != nullptr, juce::dontSendNotification);
        addAndMakeVisible(m_audio_enable_toggle);

        m_input_label.setText("Record Input:", juce::dontSendNotification);
        addAndMakeVisible(m_input_label);
        m_input_selector.onChange = [this] { apply_selected_input_channels(); };
        addAndMakeVisible(m_input_selector);
        
        m_normalize_toggle.setButtonText("Normalize Audio on Import");
        m_normalize_toggle.setToggleState(m_engine.get_normalize_on_load(), juce::dontSendNotification);
        m_normalize_toggle.onClick = [this] {
            m_engine.set_normalize_on_load(m_normalize_toggle.getToggleState());
        };
        addAndMakeVisible(m_normalize_toggle);

        m_main_sens_label.setText("Main Knob Sensitivity", juce::dontSendNotification);
        addAndMakeVisible(m_main_sens_label);
        m_main_sens_slider.setRange(10.0, 1000.0, 10.0);
        m_main_sens_slider.setValue(LayerCakeSettings::mainKnobSensitivity, juce::dontSendNotification);
        m_main_sens_slider.onValueChange = [this] {
            LayerCakeSettings::mainKnobSensitivity = m_main_sens_slider.getValue();
        };
        addAndMakeVisible(m_main_sens_slider);

        m_lfo_sens_label.setText("LFO Drag Sensitivity", juce::dontSendNotification);
        addAndMakeVisible(m_lfo_sens_label);
        m_lfo_sens_slider.setRange(10.0, 1000.0, 10.0);
        m_lfo_sens_slider.setValue(LayerCakeSettings::lfoKnobSensitivity, juce::dontSendNotification);
        m_lfo_sens_slider.onValueChange = [this] {
            LayerCakeSettings::lfoKnobSensitivity = m_lfo_sens_slider.getValue();
        };
        addAndMakeVisible(m_lfo_sens_slider);
        
        refresh_input_channel_selector();
        setSize(500, 600);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::darkgrey);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        m_audio_section_label.setBounds(area.removeFromTop(30));
        
        m_audio_enable_toggle.setBounds(area.removeFromTop(24));
        area.removeFromTop(8);
        
        if (m_device_selector) m_device_selector->setBounds(area.removeFromTop(280));
        area.removeFromTop(16);
        
        auto inputRow = area.removeFromTop(30);
        m_input_label.setBounds(inputRow.removeFromLeft(100));
        inputRow.removeFromLeft(10);
        m_input_selector.setBounds(inputRow);
        area.removeFromTop(8);
        
        m_normalize_toggle.setBounds(area.removeFromTop(30));
        area.removeFromTop(16);
        
        m_main_sens_label.setBounds(area.removeFromTop(24));
        m_main_sens_slider.setBounds(area.removeFromTop(24));
        area.removeFromTop(8);
        m_lfo_sens_label.setBounds(area.removeFromTop(24));
        m_lfo_sens_slider.setBounds(area.removeFromTop(24));
    }
    
    void refresh_input_channel_selector()
    {
        m_input_selector.clear();
        auto* device = m_device_manager.getCurrentAudioDevice();
        if (!device) { m_input_selector.addItem("No Inputs", 1); m_input_selector.setEnabled(false); return; }
        auto names = device->getInputChannelNames();
        if (names.isEmpty()) { m_input_selector.addItem("No Inputs", 1); m_input_selector.setEnabled(false); return; }
        
        m_input_selector.setEnabled(true);
        for (int i = 0; i < names.size(); ++i)
            m_input_selector.addItem(juce::String(i + 1) + ". " + names[i], i + 1);
            
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        m_device_manager.getAudioDeviceSetup(setup);
        int activeIndex = -1;
        if (!setup.useDefaultInputChannels && setup.inputChannels.getHighestBit() >= 0) {
            for (int i = 0; i < names.size(); ++i) {
                if (setup.inputChannels[i]) { activeIndex = i; break; }
            }
        }
        m_input_selector.setSelectedId(activeIndex >= 0 ? activeIndex + 1 : 1, juce::dontSendNotification);
    }
    
    void apply_selected_input_channels()
    {
        int id = m_input_selector.getSelectedId();
        if (id <= 0) return;
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        m_device_manager.getAudioDeviceSetup(setup);
        setup.inputChannels.clear();
        setup.inputChannels.setBit(id - 1, true);
        setup.useDefaultInputChannels = false;
        m_device_manager.setAudioDeviceSetup(setup, true);
    }

private:
    juce::AudioDeviceManager& m_device_manager;
    LayerCakeEngine& m_engine;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> m_device_selector;
    juce::Label m_audio_section_label;
    juce::ToggleButton m_audio_enable_toggle;
    juce::Label m_input_label;
    juce::ComboBox m_input_selector;
    juce::ToggleButton m_normalize_toggle;
    juce::Label m_main_sens_label;
    juce::Slider m_main_sens_slider;
    juce::Label m_lfo_sens_label;
    juce::Slider m_lfo_sens_slider;
};

class StandaloneSettingsWindow : public juce::DialogWindow
{
public:
    StandaloneSettingsWindow(juce::AudioDeviceManager& deviceManager, LayerCakeEngine& engine)
        : juce::DialogWindow("Settings", juce::Colours::darkgrey, true, true)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new StandaloneSettingsComponent(deviceManager, engine), true);
        setResizable(true, true);
        centreWithSize(500, 600);
    }

    void closeButtonPressed() override { setVisible(false); }
};

} // namespace LayerCakeApp

