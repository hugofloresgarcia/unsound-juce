#include "PeakMeter.h"

PeakMeter::PeakMeter()
{
    m_peak_level.store(0.0f);
}

void PeakMeter::prepare()
{
    m_peak_level.store(0.0f);
}

void PeakMeter::process_block(const float* channel_data, int num_samples)
{
    if (channel_data == nullptr || num_samples <= 0)
    {
        DBG("PeakMeter::process_block: Invalid parameters");
        return;
    }
    
    // Find peak in the block
    float max_level = 0.0f;
    for (int sample = 0; sample < num_samples; ++sample)
    {
        max_level = juce::jmax(max_level, std::abs(channel_data[sample]));
    }
    
    // Update peak level (peak hold with decay)
    float currentLevel = m_peak_level.load();
    if (max_level > currentLevel)
    {
        m_peak_level.store(max_level);
    }
    // Note: Decay is handled externally if needed, or peak naturally decays when new blocks have lower peaks
}

