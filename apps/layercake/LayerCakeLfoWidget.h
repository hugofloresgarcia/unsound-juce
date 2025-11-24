#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <flowerjuce/DSP/LfoUGen.h>
#include "LayerCakeKnob.h"
#include "LayerCakeLookAndFeel.h"
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
    ~LayerCakeLfoWidget() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    float get_depth() const noexcept;
    juce::Colour get_accent_colour() const noexcept { return m_accent_colour; }

    void refresh_wave_preview();
    void set_drag_label(const juce::String& label);
    void set_on_settings_changed(std::function<void()> callback);
    void sync_controls_from_generator();
    void set_tempo_provider(std::function<double()> tempo_bpm_provider);
    void set_tempo_sync_enabled(bool enabled, bool forceUpdate = true);
    bool is_tempo_sync_enabled() const noexcept;
    void refresh_tempo_sync();
    void set_tempo_sync_callback(std::function<void(bool)> callback);

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
    double get_tempo_bpm() const;
    double quantize_rate(double desiredRateHz, bool updateSlider);
    void sync_to_lock();
    void apply_tempo_sync_if_needed(bool resyncPhase);

    class SmallButtonLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    };

    flower::LayerCakeLfoUGen& m_generator;
    juce::Colour m_accent_colour;
    int m_lfo_index{0};
    juce::Label m_title_label;
    juce::ComboBox m_mode_selector;
    std::unique_ptr<LayerCakeKnob> m_rate_knob;
    std::unique_ptr<LayerCakeKnob> m_depth_knob;
    std::unique_ptr<WavePreview> m_wave_preview;
    juce::String m_drag_label;
    std::function<void()> m_settings_changed_callback;
    juce::TextButton m_tempo_sync_button;
    SmallButtonLookAndFeel m_tempo_button_lnf;
    bool m_tempo_sync_enabled{false};
    std::function<double()> m_tempo_bpm_provider;
    std::function<void(bool)> m_tempo_sync_callback;
    float m_last_rate{ -1.0f };
    float m_last_depth{ -1.0f };
    int m_last_mode{ -1 };
};

} // namespace LayerCakeApp



