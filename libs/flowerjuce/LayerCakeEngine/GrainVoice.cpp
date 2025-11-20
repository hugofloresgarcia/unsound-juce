#include "GrainVoice.h"
#include <cmath>

namespace
{
constexpr float kMinPlaybackRatio = 0.0625f; // -24 st
constexpr float kMaxPlaybackRatio = 4.0f;    // +24 st

float semitones_to_ratio(float semitones)
{
    return std::pow(2.0f, semitones / 12.0f);
}
} // namespace

GrainVoice::GrainVoice(size_t voice_index)
    : m_voice_index(voice_index)
{
    DBG("GrainVoice ctor voice_index=" + juce::String(static_cast<int>(voice_index)));
}

void GrainVoice::prepare(double sample_rate)
{
    m_sample_rate = sample_rate;
    m_envelope.prepare(sample_rate);
}

bool GrainVoice::layer_has_audio(const TapeLoop& loop) const
{
    return loop.m_has_recorded.load() && loop.m_recorded_length.load() > 0;
}

void GrainVoice::rebind_read_head(TapeLoop& loop)
{
    m_current_loop = &loop;
    m_read_head = std::make_unique<LooperReadHead>(loop);
    m_read_head->prepare(m_sample_rate);
}

bool GrainVoice::trigger(const GrainState& state, TapeLoop& loop, double sample_rate)
{
    juce::SpinLock::ScopedLockType lock(m_voice_lock);

    if (!layer_has_audio(loop))
    {
        DBG("GrainVoice::trigger - layer has no audio, layer=" + juce::String(state.layer));
        return false;
    }

    if (m_read_head == nullptr || m_current_loop != &loop)
    {
        rebind_read_head(loop);
    }

    m_sample_rate = sample_rate;
    m_envelope.prepare(sample_rate);
    m_envelope.set_attack_ms(state.env_attack_ms);
    m_envelope.set_release_ms(state.env_release_ms);
    m_envelope.reset();

    const size_t recorded_length = loop.m_recorded_length.load();
    if (recorded_length == 0)
    {
        DBG("GrainVoice::trigger - recorded length is 0");
        return false;
    }

    const float loop_start_samples = juce::jlimit(
        0.0f,
        static_cast<float>(recorded_length - 1),
        state.loop_start_seconds * static_cast<float>(sample_rate));

    const float duration_samples = juce::jmax(
        1.0f,
        state.duration_ms * 0.001f * static_cast<float>(sample_rate));

    float loop_end_samples = loop_start_samples + duration_samples;
    const float max_loop_end = static_cast<float>(recorded_length);
    if (loop_end_samples > max_loop_end)
        loop_end_samples = max_loop_end;

    if (loop_end_samples <= loop_start_samples + 1.0f)
    {
        DBG("GrainVoice::trigger - invalid loop range");
        return false;
    }

    m_loop_start_samples = loop_start_samples;
    m_loop_end_samples = loop_end_samples;
    m_recorded_length_samples = static_cast<float>(recorded_length);

    m_read_head->set_loop_start(loop_start_samples);
    m_read_head->set_loop_end(loop_end_samples);

    if (state.play_forward)
    {
        m_read_head->set_pos(loop_start_samples);
        m_read_head->set_direction_forward(true);
    }
    else
    {
        m_read_head->set_pos(loop_end_samples - 1.0f);
        m_read_head->set_direction_forward(false);
    }

    float playback_ratio = juce::jlimit(kMinPlaybackRatio, kMaxPlaybackRatio, semitones_to_ratio(state.rate_semitones));
    m_read_head->set_speed(playback_ratio);
    m_read_head->set_playing(true);

    m_pan = juce::jlimit(0.0f, 1.0f, state.pan);

    m_state = state;
    m_state.should_trigger = true;

    m_last_env_value = 0.0f;
    m_last_normalized_position = 0.0f;
    m_envelope.note_on();
    m_active.store(true);

    DBG("GrainVoice::trigger success voice=" + juce::String(static_cast<int>(m_voice_index)));
    return true;
}

std::array<float, 2> GrainVoice::get_next_sample()
{
    std::array<float, 2> output{0.0f, 0.0f};
    juce::SpinLock::ScopedLockType lock(m_voice_lock);

    if (!m_active.load() || m_read_head == nullptr)
        return output;

    bool wrapped = false;
    const float loop_sample = m_read_head->process_sample(wrapped);
    const float env = m_envelope.get_next_sample();
    const float mono_sample = loop_sample * env;
    m_last_env_value = env;

    const auto gains = PanningUtils::compute_stereo_gains(m_pan);
    output[0] = mono_sample * gains.first;
    output[1] = mono_sample * gains.second;

    const float loop_span = juce::jmax(1.0f, m_loop_end_samples - m_loop_start_samples);
    const float current_pos = m_read_head != nullptr ? m_read_head->get_pos() : 0.0f;
    m_last_normalized_position = juce::jlimit(0.0f, 1.0f, (current_pos - m_loop_start_samples) / loop_span);

    if (!m_envelope.is_active() || wrapped)
    {
        m_active.store(false);
        m_state.should_trigger = false;
        m_read_head->set_playing(false);
    }

    return output;
}

bool GrainVoice::is_active() const
{
    return m_active.load();
}

void GrainVoice::force_stop()
{
    juce::SpinLock::ScopedLockType lock(m_voice_lock);
    m_active.store(false);
    if (m_read_head != nullptr)
        m_read_head->set_playing(false);
    m_state.should_trigger = false;
}

bool GrainVoice::get_visual_state(GrainVisualState& state) const
{
    juce::SpinLock::ScopedLockType lock(m_voice_lock);
    if (!m_active.load())
        return false;

    state.is_active = true;
    state.layer = m_state.layer;
    state.voice_index = m_voice_index;
    state.loop_start_samples = m_loop_start_samples;
    state.loop_end_samples = m_loop_end_samples;
    state.recorded_length_samples = m_recorded_length_samples;
    state.rate_semitones = m_state.rate_semitones;
    state.play_forward = m_state.play_forward;
    state.pan = m_pan;
    state.envelope_value = m_last_env_value;
    state.normalized_position = m_last_normalized_position;
    return true;
}


