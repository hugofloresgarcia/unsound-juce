#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <flowerjuce/LayerCakeEngine/LayerCakeEngine.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Components/MidiLearnComponent.h>
#include "LayerCakeLookAndFeel.h"
#include "LayerCakeDisplay.h"
#include "LayerCakeLibraryManager.h"
#include "LibraryBrowserWindow.h"
#include "LayerCakeKnob.h"
#include "LayerCakeLfoWidget.h"
#include <array>
#include <optional>
#include <vector>
#include <atomic>

namespace LayerCakeApp
{

class MultiChannelMeter : public juce::Component
{
public:
    static constexpr int kMaxChannels = 8;

    void setLevels(const std::vector<double>& levels);
    void paint(juce::Graphics& g) override;

private:
    juce::Colour colour_for_db(double db) const;

    std::array<double, kMaxChannels> m_levels{};
    int m_active_channels{1};
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
    void apply_pattern_settings(bool request_rearm = false);
    void update_pattern_labels();
    void open_library_window();
    LayerCakePresetData capture_knobset_data() const;
    LayerCakePresetData capture_pattern_data() const;
    LayerBufferArray capture_layer_buffers() const;
    void apply_knobset(const LayerCakePresetData& data, bool update_pattern_engine = true);
    void apply_pattern_snapshot(const LayerCakePresetData& data);
    void apply_layer_buffers(const LayerBufferArray& buffers);
    void sync_manual_state_from_controls();
    void update_auto_grain_settings();
    double get_layer_recorded_seconds(int layer_index) const;
    void begin_pattern_parameter_edit();
    void end_pattern_parameter_edit();
    void request_pattern_rearm();
    void rearm_pattern_clock();
    void handle_pattern_button();
    void advance_lfos(double now_ms);
    void register_knob_for_lfo(LayerCakeKnob* knob);
    void assign_lfo_to_knob(int lfo_index, LayerCakeKnob& knob);
    void remove_lfo_from_knob(LayerCakeKnob& knob);
    void update_all_modulation_overlays();
    double get_effective_knob_value(const LayerCakeKnob* knob) const;
    void update_record_layer_from_lfo();
    void update_master_gain_from_knob();
    void capture_lfo_state(LayerCakePresetData& data) const;
    void apply_lfo_state(const LayerCakePresetData& data);
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void refresh_input_channel_selector();
    void rebuild_input_channel_buttons();
    void update_input_channel_layout();
    void sync_input_toggle_states();
    void set_all_input_channels(bool shouldEnable);
    void apply_selected_input_channels();
    int count_selected_input_channels() const;

    LayerCakeEngine m_engine;
    juce::AudioDeviceManager m_device_manager;
    Shared::MidiLearnManager m_midi_learn_manager;
    Shared::MidiLearnOverlay m_midi_learn_overlay;
    LayerCakeLookAndFeel m_custom_look_and_feel;

    juce::Label m_title_label;
    juce::Label m_record_layer_label;
    juce::Label m_record_status_label;
    std::unique_ptr<LayerCakeKnob> m_master_gain_knob;
    MultiChannelMeter m_master_meter;

    std::unique_ptr<LayerCakeKnob> m_loop_start_knob;
    std::unique_ptr<LayerCakeKnob> m_duration_knob;
    std::unique_ptr<LayerCakeKnob> m_rate_knob;
    std::unique_ptr<LayerCakeKnob> m_env_knob;
    std::unique_ptr<LayerCakeKnob> m_direction_knob;
    std::unique_ptr<LayerCakeKnob> m_pan_knob;
    std::unique_ptr<LayerCakeKnob> m_layer_select_knob;
    juce::TextButton m_trigger_button;
    juce::TextButton m_record_button;

    juce::TextButton m_clock_button;
    juce::TextButton m_pattern_button;
    juce::Label m_pattern_status_label;
    std::unique_ptr<LayerCakeKnob> m_pattern_length_knob;
    std::unique_ptr<LayerCakeKnob> m_pattern_skip_knob;
    std::unique_ptr<LayerCakeKnob> m_pattern_tempo_knob;
    std::unique_ptr<LayerCakeKnob> m_pattern_subdiv_knob;

    LayerCakeDisplay m_display;
    std::array<std::atomic<float>, MultiChannelMeter::kMaxChannels> m_meter_levels;
    std::atomic<int> m_meter_channel_count{1};
    bool m_device_ready{false};
    LayerCakeLibraryManager m_library_manager;
    std::unique_ptr<LibraryBrowserComponent> m_preset_panel;
    bool m_preset_panel_visible{true};
    GrainState m_manual_state;
    juce::File m_midi_mappings_file;
    int m_pattern_edit_depth{0};
    bool m_pattern_rearm_requested{false};
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
    std::vector<LayerCakeKnob*> m_lfo_enabled_knobs;
    juce::Label m_input_section_label;
    juce::Label m_input_section_hint;
    juce::TextButton m_input_select_all_button;
    juce::TextButton m_input_clear_button;
    juce::Viewport m_input_viewport;
    juce::Component m_input_toggle_container;
    juce::OwnedArray<juce::ToggleButton> m_input_channel_buttons;
    std::vector<char> m_input_channel_selection;
    juce::StringArray m_input_channel_names;
    bool m_updating_input_toggles{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace LayerCakeApp


