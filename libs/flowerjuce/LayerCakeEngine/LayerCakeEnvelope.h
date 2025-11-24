#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Simple ASR-style envelope that wraps JUCE's ADSR so the attack and release
// controls map directly onto the TODO spec.
class LayerCakeEnvelope
{
public:
    LayerCakeEnvelope() = default;

    void prepare(double sample_rate);
    void set_attack_ms(float attack_ms);
    void set_release_ms(float release_ms);
    void reset();
    void note_on();
    float get_next_sample();
    bool is_active() const;

private:
    juce::ADSR m_adsr;
    juce::ADSR::Parameters m_params;
};


