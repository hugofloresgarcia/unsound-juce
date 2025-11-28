#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
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

class LayerCakeProcessor;

using namespace layercake;

class LayerCakeComponent : public juce::AudioProcessorEditor,
                           public juce::DragAndDropContainer,
                           public juce::KeyListener,
                           private juce::Timer
{
public:
    explicit LayerCakeComponent(LayerCakeProcessor& processor);
    ~LayerCakeComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    
    std::function<void()> onSettingsRequested;

private:
    void timerCallback() override;

    void adjust_record_layer(int delta);
    void toggle_record_enable();
    void trigger_manual_grain();
    GrainState build_manual_grain_state();
    void update_record_labels();
    void update_meter();
    void open_library_window();
    LayerCakePresetData capture_knobset_data() const;
    LayerBufferArray capture_layer_buffers() const;
    void apply_knobset(const LayerCakePresetData& data);
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
    
    void load_settings();
    void save_settings();

    LayerCakeProcessor& m_processor;
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
    std::vector<LayerCakeKnob*> m_lfo_enabled_knobs;
    
    LfoTriggerButton m_trigger_button;
    juce::TextButton m_record_button;
    juce::TextButton m_clock_button;
    juce::TextButton m_link_button;
    double m_last_pattern_bpm{-1.0};

    LayerCakeDisplay m_display;
    
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
    static constexpr size_t kNumLfoSlots = 8;
    std::array<LfoSlot, kNumLfoSlots> m_lfo_slots;
    std::array<std::atomic<float>, kNumLfoSlots> m_lfo_last_values;
    
    LfoConnectionOverlay m_lfo_connection_overlay;
    int m_hovered_lfo_index{-1};
    int m_selected_lfo_index{-1};

    // Keyboard Control
    FocusRegistry m_focus_registry;
    KeyboardCommandRouter m_command_router;
    StatusHUDComponent m_status_hud;
    CommandPaletteOverlay m_command_palette;
    HelpOverlay m_help_overlay;

    // APVTS Attachments
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<Attachment>> m_attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayerCakeComponent)
};

} // namespace LayerCakeApp
