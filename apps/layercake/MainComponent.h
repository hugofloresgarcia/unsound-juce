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

namespace LayerCakeApp
{

class MainComponent : public juce::Component,
                      public juce::AudioIODeviceCallback,
                      public juce::KeyListener,
                      private juce::Timer
{
public:
    MainComponent();
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

    void configure_audio_device();
    void adjust_record_layer(int delta);
    void toggle_record_enable();
    void trigger_manual_grain();
    GrainState build_manual_grain_state();
    void update_record_labels();
    void update_meter();
    void apply_pattern_settings(bool request_rearm = false);
    void update_pattern_labels();
    void open_library_window();
    LayerCakePresetData capture_pattern_data() const;
    LayerBufferArray capture_layer_buffers() const;
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

    LayerCakeEngine m_engine;
    juce::AudioDeviceManager m_device_manager;
    Shared::MidiLearnManager m_midi_learn_manager;
    Shared::MidiLearnOverlay m_midi_learn_overlay;
    LayerCakeLookAndFeel m_custom_look_and_feel;

    juce::Label m_title_label;
    juce::Label m_record_layer_label;
    juce::Label m_record_status_label;
    std::unique_ptr<LayerCakeKnob> m_master_gain_knob;
    juce::ProgressBar m_master_meter;

    std::unique_ptr<LayerCakeKnob> m_loop_start_knob;
    std::unique_ptr<LayerCakeKnob> m_duration_knob;
    std::unique_ptr<LayerCakeKnob> m_rate_knob;
    std::unique_ptr<LayerCakeKnob> m_env_knob;
    std::unique_ptr<LayerCakeKnob> m_spread_knob;
    std::unique_ptr<LayerCakeKnob> m_direction_knob;
    std::unique_ptr<LayerCakeKnob> m_pan_knob;
    std::unique_ptr<LayerCakeKnob> m_layer_select_knob;
    juce::TextButton m_trigger_button;
    juce::TextButton m_record_button;

    juce::TextButton m_preset_button;
    juce::TextButton m_clock_button;
    juce::TextButton m_pattern_button;
    juce::Label m_pattern_status_label;
    std::unique_ptr<LayerCakeKnob> m_pattern_length_knob;
    std::unique_ptr<LayerCakeKnob> m_pattern_skip_knob;
    std::unique_ptr<LayerCakeKnob> m_pattern_tempo_knob;
    std::unique_ptr<LayerCakeKnob> m_pattern_subdiv_knob;

    LayerCakeDisplay m_display;
    std::atomic<float> m_meter_value{0.0f};
    double m_meter_display{0.0};
    bool m_device_ready{false};
    LayerCakeLibraryManager m_library_manager;
    std::unique_ptr<LibraryBrowserWindow> m_library_window;
    GrainState m_manual_state;
    juce::File m_midi_mappings_file;
    int m_pattern_edit_depth{0};
    bool m_pattern_rearm_requested{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace LayerCakeApp


