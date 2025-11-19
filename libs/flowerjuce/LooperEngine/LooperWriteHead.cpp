#include "LooperWriteHead.h"
#include <cmath>

LooperWriteHead::LooperWriteHead(TapeLoop& tape_loop)
    : m_tape_loop(tape_loop)
{
}

bool LooperWriteHead::process_sample(float input_sample, float current_position)
{
    
    const juce::ScopedLock sl(m_tape_loop.m_lock);
    auto& buffer = m_tape_loop.get_buffer();
    
    if (buffer.empty())
        return false;
    
    // Wrap position to buffer size
    size_t record_pos = static_cast<size_t>(std::fmod(current_position, buffer.size()));
    
    // Overdub: mix new input with existing audio
    float existing_sample = buffer[record_pos];
    float mix = m_overdub_mix.load();
    buffer[record_pos] = existing_sample * mix + input_sample * (1.0f - mix);
    m_tape_loop.m_recorded_length.store(std::max(m_tape_loop.m_recorded_length.load(), record_pos + 1));

    // Update record head to track maximum position written to
    m_pos.store(record_pos + 1);
    
    return true;
}

void LooperWriteHead::finalize_recording(float final_position)
{
    m_tape_loop.m_has_recorded.store(true);
    m_record_enable.store(false); // Turn off record enable so UI reflects the change
    
    set_loop_end(static_cast<size_t>(final_position));
    juce::Logger::writeToLog("~~~ Finalized recording");
}

void LooperWriteHead::reset()
{
    m_pos.store(0);
    juce::Logger::writeToLog("~~~ Reset write head");
    // set loop_end to the length of the tape loop
    set_loop_end(m_tape_loop.get_buffer_size());
}
