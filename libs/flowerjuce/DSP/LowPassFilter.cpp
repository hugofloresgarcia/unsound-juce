#include "LowPassFilter.h"

LowPassFilter::LowPassFilter()
{
    // Initialize filter with default coefficients (no filtering at 20kHz)
    m_filter_coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(44100.0, 20000.0f);
    m_low_pass_filter.coefficients = m_filter_coefficients;
    m_current_sample_rate = 44100.0;
}

void LowPassFilter::prepare(double sample_rate, int maximum_block_size)
{
    m_current_sample_rate = sample_rate;
    float current_cutoff = m_filter_cutoff.load();
    
    // Update filter coefficients
    m_filter_coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sample_rate, current_cutoff);
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sample_rate;
    spec.maximumBlockSize = static_cast<juce::uint32>(maximum_block_size);
    spec.numChannels = 1;
    m_low_pass_filter.prepare(spec);
    m_low_pass_filter.coefficients = m_filter_coefficients;
}

void LowPassFilter::set_cutoff(float cutoff_hz)
{
    // Clamp cutoff to valid range (20Hz to Nyquist)
    float nyquist = static_cast<float>(m_current_sample_rate * 0.5);
    cutoff_hz = juce::jlimit(20.0f, nyquist, cutoff_hz);
    
    m_filter_cutoff.store(cutoff_hz);
    
    // Update filter coefficients
    if (m_current_sample_rate > 0.0)
    {
        m_filter_coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(m_current_sample_rate, cutoff_hz);
        m_low_pass_filter.coefficients = m_filter_coefficients;
    }
}

void LowPassFilter::process_block(float* channel_data, int num_samples)
{
    if (channel_data == nullptr || num_samples <= 0)
    {
        DBG("LowPassFilter::process_block: Invalid parameters");
        return;
    }
    
    // Create array of channel pointers for AudioBlock (mono = 1 channel)
    // AudioBlock expects SampleType* const* (pointer to const pointer to SampleType)
    float* const channelDataArray[1] = { channel_data };
    float* const* channelData = channelDataArray; // Explicit pointer to array
    juce::dsp::AudioBlock<float> block(channelData, 1, 0, static_cast<size_t>(num_samples));
    juce::dsp::ProcessContextReplacing<float> context(block);
    m_low_pass_filter.process(context);
}

