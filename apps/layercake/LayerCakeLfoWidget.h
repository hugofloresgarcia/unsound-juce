#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <flowerjuce/DSP/LfoUGen.h>
#include "LayerCakeKnob.h"
#include <functional>
#include <vector>

namespace LayerCakeApp
{

class LayerCakeLfoWidget : public juce::Component,
                           private juce::ComboBox::Listener,
                           private juce::Timer
{
public:
    LayerCakeLfoWidget(int lfo_index, flower::LayerCakeLfoUGen& generator, juce::Colour accent);

    void paint(juce::Graphics& g) override;
    void resized() override;

    float get_depth() const noexcept;
    juce::Colour get_accent_colour() const noexcept { return m_accent_colour; }

    void refresh_wave_preview();
    void set_drag_label(const juce::String& label);
    void set_on_settings_changed(std::function<void()> callback);
    void sync_controls_from_generator();

private:
    class WavePreview : public juce::Component
    {
    public:
        explicit WavePreview(LayerCakeLfoWidget& owner);
        void paint(juce::Graphics& g) override;
        void resized() override;
        void set_points(const std::vector<float>& points);
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        void begin_drag(const juce::MouseEvent& event);
        LayerCakeLfoWidget& m_owner;
        std::vector<float> m_points;
        bool m_is_dragging{false};
    };

    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void update_generator_settings(bool from_knob_change);
    void notify_settings_changed();
    void configure_knob(LayerCakeKnob& knob, bool isRateKnob);
    void timerCallback() override;
    double quantize_rate(double value) const;
    void handle_octave_button();
    void update_rate_switch_bounds();

    flower::LayerCakeLfoUGen& m_generator;
    juce::Colour m_accent_colour;
    int m_lfo_index{0};
    juce::Label m_title_label;
    juce::ComboBox m_mode_selector;
    std::unique_ptr<LayerCakeKnob> m_rate_knob;
    std::unique_ptr<LayerCakeKnob> m_depth_knob;
    std::unique_ptr<juce::TextButton> m_rate_octave_button;
    std::unique_ptr<WavePreview> m_wave_preview;
    juce::String m_drag_label;
    std::function<void()> m_settings_changed_callback;
    float m_last_rate{ -1.0f };
    float m_last_depth{ -1.0f };
    int m_last_mode{ -1 };
    bool m_quantize_rate{false};
    bool m_updating_rate_slider{false};
};

} // namespace LayerCakeApp



