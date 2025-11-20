#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Components/MidiLearnComponent.h>

namespace LayerCakeApp
{

class LayerCakeKnob : public juce::Component,
                      private juce::Slider::Listener
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
    };

    LayerCakeKnob(const Config& config, Shared::MidiLearnManager* midiManager);
    ~LayerCakeKnob() override;

    juce::Slider& slider() { return m_slider; }
    const juce::Slider& slider() const { return m_slider; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void register_midi_parameter();
    void sliderValueChanged(juce::Slider* slider) override;

    Config m_config;
    Shared::MidiLearnManager* m_midi_manager{nullptr};
    void update_value_label();

    juce::Slider m_slider;
    juce::Label m_label;
    juce::Label m_value_label;
    std::unique_ptr<Shared::MidiLearnable> m_learnable;
    std::unique_ptr<Shared::MidiLearnMouseListener> m_mouse_listener;
    juce::String m_registered_parameter_id;
};

} // namespace LayerCakeApp


