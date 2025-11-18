#include "SamplerVoice.h"
#include <cmath>

namespace EmbeddingSpaceSampler
{

SamplerSound::SamplerSound(const juce::String& name, juce::AudioBuffer<float>& audio_data, double sample_rate)
    : name(name), sample_rate(sample_rate)
{
    this->audio_data = std::make_unique<juce::AudioBuffer<float>>(audio_data.getNumChannels(), audio_data.getNumSamples());
    this->audio_data->makeCopyOf(audio_data);
}

SamplerVoice::SamplerVoice()
{
    adsr.setSampleRate(44100.0); // Will be updated when note starts
    adsr.setParameters(juce::ADSR::Parameters(attack_time, decay_time, sustain_level, release_time));
}

bool SamplerVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SamplerSound*>(sound) != nullptr;
}

void SamplerVoice::startNote(int midi_note_number, float velocity, juce::SynthesiserSound* sound, int pitch_wheel)
{
    if (auto* sampler_sound = dynamic_cast<SamplerSound*>(sound))
    {
        source_sample_position = 0.0;
        
        // Calculate pitch ratio based on playback speed
        // For now, we'll use playback_speed directly (can be adjusted for MIDI note if needed)
        pitch_ratio = static_cast<double>(playback_speed);
        
        // Set gain based on velocity and gain_level
        float velocity_gain = velocity * gain_level;
        left_gain = velocity_gain;
        right_gain = velocity_gain;
        
        // Update ADSR sample rate
        adsr.setSampleRate(getSampleRate());
        adsr.setParameters(juce::ADSR::Parameters(attack_time, decay_time, sustain_level, release_time));
        adsr.noteOn();
    }
}

void SamplerVoice::stopNote(float velocity, bool allow_tail_off)
{
    if (allow_tail_off)
    {
        adsr.noteOff();
    }
    else
    {
        adsr.reset();
        clearCurrentNote();
    }
}

void SamplerVoice::renderNextBlock(juce::AudioBuffer<float>& output_buffer, int start_sample, int num_samples)
{
    if (auto* playing_sound = dynamic_cast<SamplerSound*>(getCurrentlyPlayingSound().get()))
    {
        const auto* audio_data = playing_sound->get_audio_data();
        if (audio_data == nullptr || audio_data->getNumSamples() == 0)
            return;
        
        const float* const in_left = audio_data->getReadPointer(0);
        const float* const in_right = audio_data->getNumChannels() > 1 ? audio_data->getReadPointer(1) : nullptr;
        
        float* out_left = output_buffer.getWritePointer(0, start_sample);
        float* out_right = output_buffer.getNumChannels() > 1 ? output_buffer.getWritePointer(1, start_sample) : nullptr;
        
        int samples_remaining = num_samples;
        int sample_length = audio_data->getNumSamples();
        
        while (samples_remaining-- > 0)
        {
            if (source_sample_position >= sample_length)
            {
                stopNote(0.0f, false);
                break;
            }
            
            // Linear interpolation
            int pos = static_cast<int>(source_sample_position);
            float alpha = static_cast<float>(source_sample_position - pos);
            float inv_alpha = 1.0f - alpha;
            
            // Clamp position to valid range
            pos = juce::jlimit(0, sample_length - 2, pos);
            
            float left_sample = in_left[pos] * inv_alpha + in_left[pos + 1] * alpha;
            float right_sample = in_right != nullptr 
                ? (in_right[pos] * inv_alpha + in_right[pos + 1] * alpha)
                : left_sample;
            
            // Apply ADSR envelope
            float envelope_value = adsr.getNextSample();
            
            left_sample *= left_gain * envelope_value;
            right_sample *= right_gain * envelope_value;
            
            if (out_right != nullptr)
            {
                *out_left++ += left_sample;
                *out_right++ += right_sample;
            }
            else
            {
                *out_left++ += (left_sample + right_sample) * 0.5f;
            }
            
            source_sample_position += pitch_ratio;
            
            // Check if note should stop
            if (!adsr.isActive())
            {
                stopNote(0.0f, false);
                break;
            }
        }
    }
}

void SamplerVoice::set_playback_speed(float speed)
{
    playback_speed = juce::jmax(0.1f, speed); // Clamp to reasonable range
    if (isVoiceActive())
    {
        pitch_ratio = static_cast<double>(playback_speed);
    }
}

void SamplerVoice::set_gain(float gain)
{
    gain_level = juce::jmax(0.0f, gain);
}

} // namespace EmbeddingSpaceSampler

