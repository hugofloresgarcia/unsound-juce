#pragma once

#include "Panner.h"
#include "PanningUtils.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

// StereoPanner2D: processes mono input to stereo output with 2D control
// X-axis: Panning (Left-Right)
// Y-axis: Depth (Back-Front)
//   - Back (Y = -1): Dry (No delay)
//   - Front (Y = 1): Wet (Max delay effect)
//   Effect: Short delay (90ms) with Feedback (0.7)
class StereoPanner2D : public Panner
{
public:
    StereoPanner2D();
    ~StereoPanner2D() override = default;

    // Prepare for processing
    void prepare(double sample_rate, int samples_per_block);

    // Panner interface
    void process_block(const float* const* input_channel_data,
                     int num_input_channels,
                     float* const* output_channel_data,
                     int num_output_channels,
                     int num_samples) override;

    int get_num_input_channels() const override { return 1; }
    int get_num_output_channels() const override { return 2; }

    // Set 2D position
    // x: -1.0 (left) to 1.0 (right)
    // y: -1.0 (back) to 1.0 (front)
    void set_point(float x, float y);

    float get_x() const { return m_x.getTargetValue(); }
    float get_y() const { return m_y.getTargetValue(); }

private:
    // Delay Logic
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> m_delay_line;
    static constexpr float m_delay_time_ms = 90.0f;
    static constexpr float m_feedback = 0.7f;
    
    // Scratch buffer for processing
    juce::AudioBuffer<float> m_scratch_buffer;

    // Parameters
    juce::LinearSmoothedValue<float> m_x { 0.0f };
    juce::LinearSmoothedValue<float> m_y { 0.0f };
    juce::LinearSmoothedValue<float> m_wet_level { 0.0f };
    
    double m_sample_rate { 44100.0 };
    
    // Internal state updates
    void update_parameters();
};
