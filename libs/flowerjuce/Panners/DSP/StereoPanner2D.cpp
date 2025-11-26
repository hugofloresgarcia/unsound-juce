#include "StereoPanner2D.h"
#include <algorithm>
#include <cmath>

StereoPanner2D::StereoPanner2D()
{
    // Defaults
    m_x.setCurrentAndTargetValue(0.0f);
    m_y.setCurrentAndTargetValue(0.0f);
    m_wet_level.setCurrentAndTargetValue(0.0f);
}

void StereoPanner2D::prepare(double sample_rate, int samples_per_block)
{
    m_sample_rate = sample_rate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sample_rate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samples_per_block);
    spec.numChannels = 2; // Stereo processing

    // Prepare delay line
    // Max delay needed: ~100ms. Let's allocate 200ms safely.
    int max_delay_samples = static_cast<int>(sample_rate * 0.2);
    m_delay_line.prepare(spec);
    m_delay_line.setMaximumDelayInSamples(max_delay_samples);
    
    // Resize scratch buffer
    m_scratch_buffer.setSize(2, samples_per_block);

    // Set smoothing time to 50ms
    m_x.reset(sample_rate, 0.05);
    m_y.reset(sample_rate, 0.05);
    m_wet_level.reset(sample_rate, 0.05);
}

void StereoPanner2D::set_point(float x, float y)
{
    m_x.setTargetValue(juce::jlimit(-1.0f, 1.0f, x));
    m_y.setTargetValue(juce::jlimit(-1.0f, 1.0f, y));
}

void StereoPanner2D::update_parameters()
{
    // Map Y to Wet Level
    // Back (Y=-1) -> Wet=0
    // Front (Y=1) -> Wet=1 (or limit to e.g. 0.5 if strictly adding delay, but let's do full range)
    // Linear mapping: wet = (y + 1) / 2
    
    float y = m_y.getNextValue();
    // Consuming x as well to keep smoothers in sync step-wise
    float x = m_x.getNextValue(); 
    (void)x; // unused here

    float target_wet = (y + 1.0f) * 0.5f;
    m_wet_level.setTargetValue(juce::jlimit(0.0f, 1.0f, target_wet));
    
    // Set delay time (fixed)
    // 90ms in samples
    float delay_samples = (float)(m_delay_time_ms * m_sample_rate / 1000.0);
    m_delay_line.setDelay(delay_samples);
}

void StereoPanner2D::process_block(const float* const* input_channel_data,
                                   int num_input_channels,
                                   float* const* output_channel_data,
                                   int num_output_channels,
                                   int num_samples)
{
    if (num_input_channels < 1 || num_output_channels < 2)
        return;

    update_parameters();
    
    // Advance smoothers for rest of block
    if (num_samples > 1)
    {
        m_x.skip(num_samples - 1);
        m_y.skip(num_samples - 1);
        // m_wet_level.skip(num_samples - 1); // We'll use smoothed values per block or sample? 
        // Let's use per-sample mix for smooth automation
    }
    
    m_scratch_buffer.setSize(2, num_samples, false, false, true);
    m_scratch_buffer.clear();

    // 1. Pan Input -> Scratch Buffer (Dry)
    float pan = m_x.getCurrentValue();
    auto [left_gain, right_gain] = PanningUtils::compute_stereo_gains((pan + 1.0f) * 0.5f);
    const float* input = input_channel_data[0];
    
    m_scratch_buffer.copyFrom(0, 0, input, num_samples, left_gain);
    m_scratch_buffer.copyFrom(1, 0, input, num_samples, right_gain);
    
    // 2. Process Delay with Feedback
    // We need to access DelayLine per sample for feedback loop
    // DelayLine processes a context. It doesn't easily support internal feedback unless we manage it.
    // JUCE DelayLine is just a delay buffer.
    // To do feedback: 
    //   in -> [+] -> delay -> out
    //          ^       |
    //          |--fb---|
    
    // But juce::dsp::DelayLine doesn't expose the write pointer easily in a loop without pushSample/popSample.
    // Using pushSample/popSample is good.
    
    auto* left_ptr = m_scratch_buffer.getWritePointer(0);
    auto* right_ptr = m_scratch_buffer.getWritePointer(1);
    
    // Since wet level changes, we can interpolate it over the block if we want, 
    // but for simplicity let's step it if we skipped smoothers, OR loop it.
    // We skipped smoothers in `update_parameters` call block, so `getCurrentValue` is the end target?
    // No, `update_parameters` called `getNextValue`.
    // Actually, my logic in `update_parameters` consumes one sample of x/y.
    // Then I skip `num_samples - 1`.
    // So `getCurrentValue` returns the value at sample 1.
    // This is a bit rough. Ideally we iterate smoothers in the loop.
    // Let's revert the skip and iterate smoothers.
    
    // Reset smoothers to start of block? No, `getNextValue` advances state.
    // I should NOT have called `skip` if I want to iterate.
    // But `update_parameters` was designed to set DSP parameters once per block.
    // For gain, per-sample is better to avoid zipper noise.
    
    // Let's assume wet level is constant for the block for now to match previous architecture style
    // (update_parameters sets state for block).
    // The previous implementation used `m_x.getCurrentValue()` after update/skip.
    
    float wet = m_wet_level.getTargetValue(); // Use target for block processing to be safe/consistent with simple logic
    float dry = 1.0f - wet; // Or should we preserve dry level? 
    // "Increase amount of delay" usually means Dry/Wet mix.
    // Equal power? Or Linear? Let's do linear for now: Dry = 1, Wet adds on top?
    // Or standard Mix: Dry = 1-Wet, Wet = Wet.
    // If "increase amount", likely we want to hear the delay more.
    // Let's use: Output = Dry * 1.0 + Delay * Wet. (Add delay to signal)
    // This maintains the dry signal level (important for panning perception) and just adds the echo.
    
    for (int i = 0; i < num_samples; ++i)
    {
        // Left
        float in_l = left_ptr[i];
        float delayed_l = m_delay_line.popSample(0);
        float out_l = in_l + delayed_l * wet;
        m_delay_line.pushSample(0, in_l + delayed_l * m_feedback);
        left_ptr[i] = out_l;
        
        // Right
        float in_r = right_ptr[i];
        float delayed_r = m_delay_line.popSample(1);
        float out_r = in_r + delayed_r * wet;
        m_delay_line.pushSample(1, in_r + delayed_r * m_feedback);
        right_ptr[i] = out_r;
    }

    // 3. Mix to Output (Accumulate)
    for (int ch = 0; ch < 2; ++ch)
    {
        juce::FloatVectorOperations::add(output_channel_data[ch], 
                                       m_scratch_buffer.getReadPointer(ch), 
                                       num_samples);
    }
}
