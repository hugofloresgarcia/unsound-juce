#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <flowerjuce/LayerCakeEngine/LayerCakeEngine.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Components/MidiLearnComponent.h>
#include <flowerjuce/Components/MultiChannelMeter.h>
#include "LayerCakeLookAndFeel.h"
#include "LayerCakeDisplay.h"
#include "LayerCakeLibraryManager.h"
#include "LibraryBrowserWindow.h"
#include "LayerCakeKnob.h"
#include "lfo/LayerCakeLfoWidget.h"
#include "lfo/LfoTriggerButton.h"
#include "lfo/LfoConnectionOverlay.h"
#include <array>
#include <optional>
#include <vector>
#include <atomic>

namespace LayerCakeApp
{

class SettingsComponent : public juce::Component
{
public:
    SettingsComponent(juce::AudioDeviceManager& deviceManager);
    void paint(juce::Graphics& g) override;
    void resized() override;

    void refresh_input_channel_selector();
    void apply_selected_input_channels();

private:
    juce::AudioDeviceManager& m_device_manager;

    juce::Label m_input_label;
    juce::ComboBox m_input_selector;
    juce::StringArray m_input_channel_names;
};

class SettingsButtonLookAndFeel : public LayerCakeLookAndFeel
{
public:
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
};

class LayerCakeSettingsWindow : public juce::DialogWindow
{
public:
    LayerCakeSettingsWindow(juce::AudioDeviceManager& deviceManager)
        : juce::DialogWindow("settings", juce::Colours::darkgrey, true, true)
    {
        setUsingNativeTitleBar(true);
        auto* content = new SettingsComponent(deviceManager);
        setContentOwned(content, true);
        setResizable(false, false);
        centreWithSize(300, 200);
    }

    void closeButtonPressed() override { setVisible(false); }
};

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      public juce::AudioIODeviceCallback,
                      public juce::KeyListener,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    explicit MainComponent(std::optional<juce::AudioDeviceManager::AudioDeviceSetup> initialDeviceSetup = std::nullopt);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // juce::AudioIODeviceCallback
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

private:
    void timerCallback() override;

    void configure_audio_device(std::optional<juce::AudioDeviceManager::AudioDeviceSetup> initialSetup);
    void adjust_record_layer(int delta);
    void toggle_record_enable();
    void trigger_manual_grain();
    GrainState build_manual_grain_state();
    void update_record_labels();
    void update_meter();
    void open_library_window();
    LayerCakePresetData capture_knobset_data() const;
    // capture_pattern_data removed
    LayerBufferArray capture_layer_buffers() const;
    void apply_knobset(const LayerCakePresetData& data);
    // apply_pattern_snapshot removed
    void apply_layer_buffers(const LayerBufferArray& buffers);
    void sync_manual_state_from_controls();
    double get_layer_recorded_seconds(int layer_index) const;
    void handle_clock_button();
    void advance_lfos(double now_ms);
    void register_knob_for_lfo(LayerCakeKnob* knob);
    void update_lfo_connection_overlay(int lfo_index, bool hovered);
    void assign_lfo_to_knob(int lfo_index, LayerCakeKnob& knob);
    void remove_lfo_from_knob(LayerCakeKnob& knob);
    void update_all_modulation_overlays();
    double get_effective_knob_value(const LayerCakeKnob* knob) const;
    void update_record_layer_from_lfo();
    void update_master_gain_from_knob();
    void capture_lfo_state(LayerCakePresetData& data) const;
    void apply_lfo_state(const LayerCakePresetData& data);
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void open_settings_window();

    LayerCakeEngine m_engine;
    juce::AudioDeviceManager m_device_manager;
    Shared::MidiLearnManager m_midi_learn_manager;
    Shared::MidiLearnOverlay m_midi_learn_overlay;
    LayerCakeLookAndFeel m_custom_look_and_feel;

    juce::Label m_title_label;
    juce::Label m_record_layer_label;
    juce::Label m_record_status_label;
    std::unique_ptr<LayerCakeKnob> m_master_gain_knob;
    Shared::MultiChannelMeter m_master_meter;

    // CLI-style knobs for grain controls
    std::unique_ptr<LayerCakeKnob> m_position_knob;
    std::unique_ptr<LayerCakeKnob> m_duration_knob;
    std::unique_ptr<LayerCakeKnob> m_rate_knob;
    std::unique_ptr<LayerCakeKnob> m_env_knob;
    std::unique_ptr<LayerCakeKnob> m_direction_knob;
    std::unique_ptr<LayerCakeKnob> m_pan_knob;
    std::unique_ptr<LayerCakeKnob> m_layer_knob;
    std::unique_ptr<LayerCakeKnob> m_tempo_knob;
    std::vector<LayerCakeKnob*> m_lfo_enabled_knobs;  // For LFO assignment iteration
    
    LfoTriggerButton m_trigger_button;
    juce::TextButton m_record_button;
    juce::TextButton m_clock_button; // Transport Play/Stop
    double m_last_pattern_bpm{-1.0};

    LayerCakeDisplay m_display;
    std::array<std::atomic<float>, Shared::MultiChannelMeter::kMaxChannels> m_meter_levels;
    std::atomic<int> m_meter_channel_count{1};
    bool m_device_ready{false};
    LayerCakeLibraryManager m_library_manager;
    std::unique_ptr<LibraryBrowserComponent> m_preset_panel;
    bool m_preset_panel_visible{true};
    GrainState m_manual_state;
    juce::File m_midi_mappings_file;
    bool m_loading_knob_values{false};
    struct LfoSlot
    {
        flower::LayerCakeLfoUGen generator;
        std::unique_ptr<LayerCakeLfoWidget> widget;
        juce::Colour accent;
        juce::String label;
    };
    static constexpr size_t kNumLfoSlots = LayerCakePresetData::kNumLfos;
    std::array<LfoSlot, kNumLfoSlots> m_lfo_slots;
    std::array<std::atomic<float>, kNumLfoSlots> m_lfo_last_values;
    std::array<float, kNumLfoSlots> m_lfo_prev_values{};  // For zero-crossing detection
    SettingsButtonLookAndFeel m_settings_button_look_and_feel;
    juce::TextButton m_settings_button;
    std::unique_ptr<LayerCakeSettingsWindow> m_settings_window;
    LfoConnectionOverlay m_lfo_connection_overlay;
    int m_hovered_lfo_index{-1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace LayerCakeApp
