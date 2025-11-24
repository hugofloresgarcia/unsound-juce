#include "LayerCakeEnvelope.h"

void LayerCakeEnvelope::prepare(double sample_rate)
{
    m_adsr.setSampleRate(sample_rate);
}

void LayerCakeEnvelope::set_attack_ms(float attack_ms)
{
    m_params.attack = juce::jmax(0.0f, attack_ms) / 1000.0f;
    m_adsr.setParameters(m_params);
}

void LayerCakeEnvelope::set_release_ms(float release_ms)
{
    const float release_seconds = juce::jmax(0.0f, release_ms) / 1000.0f;
    // The TODO spec requests an ASR envelope where JUCE's decay parameter
    // behaves as the release stage and sustain is fixed at 0.
    m_params.decay = release_seconds;
    m_params.sustain = 0.0f;
    m_params.release = 0.0f;
    m_adsr.setParameters(m_params);
}

void LayerCakeEnvelope::reset()
{
    m_adsr.reset();
}

void LayerCakeEnvelope::note_on()
{
    m_adsr.noteOn();
}

float LayerCakeEnvelope::get_next_sample()
{
    return m_adsr.getNextSample();
}

bool LayerCakeEnvelope::is_active() const
{
    return m_adsr.isActive();
}


