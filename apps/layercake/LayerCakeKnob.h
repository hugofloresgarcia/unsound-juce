#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/DSP/KnobSweepRecorder.h>
#include "KnobRecorderButton.h"
#include "lfo/LfoAssignmentButton.h"
#include <functional>
#include <optional>
#include <atomic>

namespace LayerCakeApp
{

class LayerCakeKnob : public juce::Component,
                      public juce::DragAndDropTarget,
                      public juce::SettableTooltipClient,
                      private juce::Slider::Listener,
                      private juce::Timer
{
public:
    struct Config
    {
        juce::String labelText;
        double minValue{0.0};
        double maxValue{1.0};
        double defaultValue{0.0};
        double interval{0.01};
        juce::String suffix;
        juce::String parameterId;
        bool isToggle{false};
        bool enableSweepRecorder{true};
        bool enableLfoAssignment{true};
        bool cliMode{false};  // CLI-style "key: value" display instead of rotary knob
        bool displayAsPercent{false};  // For 0-1 ranges, display as 0-99
        int decimals{2};  // Decimal places for CLI mode
    };

    LayerCakeKnob(const Config& config, Shared::MidiLearnManager* midiManager);
    ~LayerCakeKnob() override;

    juce::Slider& slider() { return m_slider; }
    const juce::Slider& slider() const { return m_slider; }
    const juce::String& parameter_id() const { return m_config.parameterId; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    // juce::DragAndDropTarget
    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragExit(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override;

    void set_context_menu_builder(const std::function<void(juce::PopupMenu&)>& builder);
    void set_lfo_drop_handler(const std::function<void(LayerCakeKnob&, int)>& handler);
    void set_lfo_highlight_colour(juce::Colour colour);
    void set_modulation_indicator(std::optional<float> normalizedValue, juce::Colour colour);
    void clear_modulation_indicator();
    void set_lfo_assignment_index(int index);
    void set_lfo_button_accent(std::optional<juce::Colour> accent);
    void set_lfo_release_handler(const std::function<void()>& handler);
    void set_hover_changed_handler(const std::function<void(bool)>& handler);
    int lfo_assignment_index() const { return m_lfo_assignment_index.load(std::memory_order_relaxed); }
    bool has_lfo_assignment() const { return lfo_assignment_index() >= 0; }
    void set_knob_colour(juce::Colour colour);
    void clear_knob_colour();
    bool is_cli_mode() const { return m_config.cliMode; }
    const Config& config() const { return m_config; }

private:
    enum class RecorderState
    {
        Idle = 0,
        Armed,
        Recording,
        Looping
    };

    void register_midi_parameter();
    void sliderValueChanged(juce::Slider* slider) override;
    void sliderDragStarted(juce::Slider* slider) override;
    void sliderDragEnded(juce::Slider* slider) override;
    void apply_look_and_feel_colours();
    void timerCallback() override;
    bool show_context_menu(const juce::MouseEvent& event);
    void paint_cli_mode(juce::Graphics& g);
    juce::String format_cli_value() const;

    bool sweep_recorder_enabled() const noexcept { return m_config.enableSweepRecorder; }
    void arm_sweep_recorder();
    void clear_sweep_recorder(const juce::String& reason);
    void update_recorder_state(RecorderState next_state);
    void begin_sweep_recording(double now_ms);
    void finish_sweep_recording();
    void handle_touch_begin(bool initiated_by_button);
    void handle_touch_end();
    void update_recorder_button();
    void update_timer_activity();
    void update_blink_state(bool force_reset);
    void sync_recorder_idle_value();
    void refresh_lfo_button_state();
    void update_lfo_tooltip();

    Config m_config;
    Shared::MidiLearnManager* m_midi_manager{nullptr};
    void update_value_label();

    juce::Slider m_slider;
    juce::Label m_label;
    juce::Label m_value_label;
    juce::String m_registered_parameter_id;

    KnobSweepRecorder m_sweep_recorder;
    RecorderState m_recorder_state{RecorderState::Idle};
    std::unique_ptr<KnobRecorderButton> m_recorder_button;
    std::unique_ptr<LfoAssignmentButton> m_lfo_button;
    bool m_is_applying_loop_value{false};
    bool m_blink_visible{false};
    double m_last_blink_toggle_ms{0.0};
    std::function<void(juce::PopupMenu&)> m_context_menu_builder;
    std::function<void(LayerCakeKnob&, int)> m_lfo_drop_handler;
    std::function<void()> m_lfo_release_handler;
    std::function<void(bool)> m_hover_changed_handler;
    juce::Colour m_lfo_highlight_colour;
    juce::Colour m_active_drag_colour;
    bool m_drag_highlight{false};
    std::optional<float> m_modulation_indicator_value;
    juce::Colour m_modulation_indicator_colour;
    std::atomic<int> m_lfo_assignment_index{-1};
    std::optional<juce::Colour> m_custom_knob_colour;
    std::optional<juce::Colour> m_lfo_button_accent;
    juce::Rectangle<float> m_lfo_indicator_bounds;  // For option-click hit testing in CLI mode
    bool m_show_base_value{false};
    bool m_is_hovered{false};
};

} // namespace LayerCakeApp


