#include "LinkSyncStrategy.h"
#include <juce_core/juce_core.h>
#include <cmath>

namespace flower
{

LinkSyncStrategy::LinkSyncStrategy(double initial_bpm)
    : m_link(std::make_unique<ableton::Link>(initial_bpm))
{
    m_link->enable(false); // Start disabled, user must enable
}

LinkSyncStrategy::~LinkSyncStrategy()
{
    m_link->enable(false);
}

void LinkSyncStrategy::prepare(double sample_rate, int block_size)
{
    // Reset host time filter or other state if needed
    // Currently nothing critical, but good practice
    juce::ignoreUnused(sample_rate, block_size);
}

double LinkSyncStrategy::get_current_beat()
{
    // This returns the beat at the START of the current block
    // which was calculated in process()
    return m_block_start_beat;
}

double LinkSyncStrategy::get_tempo() const
{
    return m_cached_bpm;
}

void LinkSyncStrategy::set_tempo(double bpm)
{
    // Queue the request to be handled in process()
    m_pending_request.bpm = bpm;
}

bool LinkSyncStrategy::is_playing() const
{
    return m_cached_playing;
}

void LinkSyncStrategy::set_playing(bool playing)
{
    m_pending_request.playing = playing;
}

void LinkSyncStrategy::request_reset()
{
    m_pending_request.reset = true;
}

void LinkSyncStrategy::enable_link(bool enabled)
{
    m_link->enable(enabled);
}

bool LinkSyncStrategy::is_link_enabled() const
{
    return m_link->isEnabled();
}

int LinkSyncStrategy::get_num_peers() const
{
    return static_cast<int>(m_link->numPeers());
}

void LinkSyncStrategy::calculate_output_time(double sample_rate, int buffer_size)
{
    // Synchronize host time to reference the point when its output reaches the speaker.
    // Note: We assume sample_time is incremented by buffer_size each block.
    // In a real plugin, we might use the position info provided by the host.
    // But for a standalone app or internal engine, counting samples is fine.
    
    const auto host_time = m_host_time_filter.sampleTimeToHostTime(static_cast<double>(m_total_samples));
    const auto output_latency = std::chrono::microseconds{ static_cast<long long>(1.0e6 * buffer_size / sample_rate) };
    m_output_time = output_latency + host_time;
}

void LinkSyncStrategy::commit_timeline_changes()
{
    if (m_pending_request.bpm.has_value())
    {
        m_session_state->setTempo(*m_pending_request.bpm, m_output_time);
        m_pending_request.bpm.reset();
    }

    if (m_pending_request.playing.has_value())
    {
        m_session_state->setIsPlaying(*m_pending_request.playing, m_output_time);
        m_pending_request.playing.reset();
    }

    if (m_pending_request.reset)
    {
        // Reset beat to 0 at the current time
        m_session_state->requestBeatAtTime(0.0, m_output_time, 4.0); // Assuming 4/4 quantum
        m_pending_request.reset = false;
    }
    
    m_link->commitAudioSessionState(*m_session_state);
}

void LinkSyncStrategy::process(int num_samples, double sample_rate)
{
    if (sample_rate <= 0.0) return;

    calculate_output_time(sample_rate, num_samples);
    
    m_session_state = std::make_unique<ableton::Link::SessionState>(m_link->captureAudioSessionState());
    
    // Apply any pending changes from UI thread
    commit_timeline_changes();
    
    // Update cached values for this block
    m_cached_bpm = m_session_state->tempo();
    m_cached_playing = m_session_state->isPlaying();
    
    // Calculate beat at the start of this block (output time)
    // We use a quantum of 4.0 (1 bar) generally, though getPhase allows specifying it.
    // For get_current_beat(), we just want the absolute beat.
    m_block_start_beat = m_session_state->beatAtTime(m_output_time, 4.0);
    
    m_total_samples += num_samples;
}

double LinkSyncStrategy::get_phase(double quantum) const
{
    if (!m_session_state) return 0.0;
    return m_session_state->phaseAtTime(m_output_time, quantum);
}

} // namespace flower
