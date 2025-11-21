#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Components/MidiLearnComponent.h>
#include <flowerjuce/DSP/KnobSweepRecorder.h>
#include "KnobRecorderButton.h"

namespace LayerCakeApp
{

class LayerCakeKnob : public juce::Component,
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

    bool sweep_recorder_enabled() const noexcept { return m_config.enableSweepRecorder; }
    void show_recorder_menu(const juce::MouseEvent& event);
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

    Config m_config;
    Shared::MidiLearnManager* m_midi_manager{nullptr};
    void update_value_label();

    juce::Slider m_slider;
    juce::Label m_label;
    juce::Label m_value_label;
    std::unique_ptr<Shared::MidiLearnable> m_learnable;
    std::unique_ptr<Shared::MidiLearnMouseListener> m_mouse_listener;
    juce::String m_registered_parameter_id;

    KnobSweepRecorder m_sweep_recorder;
    RecorderState m_recorder_state{RecorderState::Idle};
    std::unique_ptr<KnobRecorderButton> m_recorder_button;
    bool m_is_applying_loop_value{false};
    bool m_blink_visible{false};
    double m_last_blink_toggle_ms{0.0};
};

} // namespace LayerCakeApp


