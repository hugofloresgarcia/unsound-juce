#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <flowerjuce/LayerCakeEngine/LayerCakeEngine.h>
#include <array>
#include <unordered_map>
#include <vector>
#include <atomic>

class LayerCakeDisplay : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    explicit LayerCakeDisplay(LayerCakeEngine& engine);
    ~LayerCakeDisplay() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void set_record_layer(int layer_index) { m_record_layer = layer_index; }
    void set_position_indicator(float normalized_position);

private:
    void timerCallback() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void refresh_waveforms();
    void refresh_grains();
    juce::Rectangle<float> get_display_area() const;
    int layer_at_point(juce::Point<int> point) const;
    juce::Rectangle<float> lane_bounds_for_index(int layer_index) const;
    bool has_supported_audio_extension(const juce::File& file) const;
    juce::Colour colour_for_voice(size_t voice_index);
    void update_invaders(float width, float height);
    void regenerate_funfetti_texture(int width, int height);
    void animate_funfetti_texture();

    LayerCakeEngine& m_engine;
    int m_record_layer{0};

    std::array<std::vector<float>, LayerCakeEngine::kNumLayers> m_waveform_cache;
    std::vector<GrainVisualState> m_grain_states;
    std::array<juce::Colour, 8> m_palette;
    std::unordered_map<size_t, juce::Colour> m_voice_colours;
    std::atomic<float> m_position_indicator{ -1.0f };

    struct Invader
    {
        juce::Point<float> position;
        juce::Point<float> velocity;
    };
    std::vector<Invader> m_invaders;
    int m_waveform_counter{0};
    juce::Image m_funfetti_texture;
    juce::Point<float> m_noise_scroll{0.0f, 0.0f};
    juce::Point<float> m_noise_velocity{0.18f, -0.11f};
    double m_noise_phase{0.0};
};


