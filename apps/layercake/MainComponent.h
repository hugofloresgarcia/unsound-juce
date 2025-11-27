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
#include "focus/FocusRegistry.h"
#include "input/KeyboardCommandRouter.h"
#include "ui/StatusHUDComponent.h"
#include "ui/CommandPaletteOverlay.h"
#include "ui/HelpOverlay.h"
#include <array>
#include <vector>
#include <atomic>
#include <functional>

namespace LayerCakeApp
{

using namespace layercake; // Import new namespace

class SettingsComponent : public juce::Component
{
public:
    SettingsComponent(juce::AudioDeviceManager& deviceManager, LayerCakeEngine& engine);
    void paint(juce::Graphics& g) override;
    void resized() override;

    void refresh_input_channel_selector();
    void apply_selected_input_channels();
    
    // Audio enable callback - set by parent to enable/disable audio
    std::function<void(bool)> onAudioEnableChanged;
    void set_audio_enabled(bool enabled);

private:
    juce::AudioDeviceManager& m_device_manager;
    LayerCakeEngine& m_engine;

    // Audio device selector
    std::unique_ptr<juce::AudioDeviceSelectorComponent> m_device_selector;
    juce::ToggleButton m_audio_enable_toggle;
    juce::Label m_audio_section_label;
    
    // Input channel selector (mono input selection)
    juce::Label m_input_label;
    juce::ComboBox m_input_selector;
    juce::StringArray m_input_channel_names;
    
    // Other settings
    juce::ToggleButton m_normalize_toggle;

    juce::Label m_main_sens_label;
    juce::Slider m_main_sens_slider;
    juce::Label m_lfo_sens_label;
    juce::Slider m_lfo_sens_slider;
};

class SettingsButtonLookAndFeel : public LayerCakeLookAndFeel
{
public:
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
};

class LayerCakeSettingsWindow : public juce::DialogWindow
{
public:
    LayerCakeSettingsWindow(juce::AudioDeviceManager& deviceManager, LayerCakeEngine& engine,
                            std::function<void(bool)> audioEnableCallback,
                            std::function<bool()> audioEnabledGetter)
        : juce::DialogWindow("settings", juce::Colours::darkgrey, true, true)
    {
        setUsingNativeTitleBar(true);
        auto* content = new SettingsComponent(deviceManager, engine);
        content->onAudioEnableChanged = audioEnableCallback;
        if (audioEnabledGetter)
            content->set_audio_enabled(audioEnabledGetter());
        setContentOwned(content, true);
        setResizable(true, true);
        centreWithSize(500, 600);
    }

    void closeButtonPressed() override { setVisible(false); }
    
    void update_audio_state(bool enabled)
    {
        if (auto* settings = dynamic_cast<SettingsComponent*>(getContentComponent()))
            settings->set_audio_enabled(enabled);
    }
};

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      public juce::AudioIODeviceCallback,
                      public juce::KeyListener,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Audio control - starts/stops audio processing
    bool is_audio_enabled() const { return m_audio_enabled; }
    void set_audio_enabled(bool enabled);
    juce::String get_current_device_name() const;

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

    void initialize_audio_device();
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
    void handle_link_button();
    void register_knob_for_lfo(LayerCakeKnob* knob);
    void handle_knob_hover(LayerCakeKnob* knob, bool hovered);
    void handle_trigger_hover(bool hovered);
    void update_lfo_connection_overlay(int lfo_index, bool hovered);
    void assign_lfo_to_knob(int lfo_index, LayerCakeKnob& knob);
    void remove_lfo_from_knob(LayerCakeKnob& knob);
    void update_all_modulation_overlays();
    double get_effective_knob_value(const LayerCakeKnob* knob) const;
    void update_record_layer_from_lfo();
    void update_master_gain_from_knob();
    void push_lfo_to_engine(int lfo_index);
    void capture_lfo_state(LayerCakePresetData& data) const;
    void apply_lfo_state(const LayerCakePresetData& data);
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void open_settings_window();
    void load_settings();
    void save_settings();

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
    juce::TextButton m_link_button;  // Ableton Link toggle
    double m_last_pattern_bpm{-1.0};

    LayerCakeDisplay m_display;
    std::array<std::atomic<float>, Shared::MultiChannelMeter::kMaxChannels> m_meter_levels;
    std::atomic<int> m_meter_channel_count{1};
    bool m_device_ready{false};
    bool m_audio_enabled{false};
    LayerCakeLibraryManager m_library_manager;
    std::unique_ptr<LibraryBrowserComponent> m_preset_panel;
    bool m_preset_panel_visible{true};
    GrainState m_manual_state;
    juce::File m_midi_mappings_file;
    juce::File m_settings_file;
    bool m_loading_knob_values{false};
    struct LfoSlot
    {
        flower::LayerCakeLfoUGen generator;
        std::unique_ptr<LayerCakeLfoWidget> widget;
        juce::Colour accent;
        juce::String label;
        bool enabled{true};
    };
    static constexpr size_t kNumLfoSlots = LayerCakePresetData::kNumLfos;
    std::array<LfoSlot, kNumLfoSlots> m_lfo_slots;
    std::array<std::atomic<float>, kNumLfoSlots> m_lfo_last_values;
    SettingsButtonLookAndFeel m_settings_button_look_and_feel;
    juce::TextButton m_settings_button;
    std::unique_ptr<LayerCakeSettingsWindow> m_settings_window;
    LfoConnectionOverlay m_lfo_connection_overlay;
    int m_hovered_lfo_index{-1};
    int m_selected_lfo_index{-1};

    // Keyboard Control
    FocusRegistry m_focus_registry;
    KeyboardCommandRouter m_command_router;
    StatusHUDComponent m_status_hud;
    CommandPaletteOverlay m_command_palette;
    HelpOverlay m_help_overlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace LayerCakeApp
