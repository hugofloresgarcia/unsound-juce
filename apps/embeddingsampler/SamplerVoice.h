#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace EmbeddingSpaceSampler
{

// SamplerSound represents a single audio sample that can be played
class SamplerSound : public juce::SynthesiserSound
{
public:
    SamplerSound(const juce::String& name, juce::AudioBuffer<float>& audio_data, double sample_rate);
    ~SamplerSound() override = default;
    
    bool appliesToNote(int midi_note_number) override { return true; }
    bool appliesToChannel(int midi_channel) override { return true; }
    
    const juce::AudioBuffer<float>* get_audio_data() const { return audio_data.get(); }
    double get_sample_rate() const { return sample_rate; }
    int get_length() const { return audio_data != nullptr ? audio_data->getNumSamples() : 0; }
    juce::String get_name() const { return name; }
    
private:
    juce::String name;
    std::unique_ptr<juce::AudioBuffer<float>> audio_data;
    double sample_rate;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerSound)
};

// SamplerVoice plays a single SamplerSound
class SamplerVoice : public juce::SynthesiserVoice
{
public:
    SamplerVoice();
    ~SamplerVoice() override = default;
    
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    
    void startNote(int midi_note_number, float velocity, juce::SynthesiserSound* sound, int pitch_wheel) override;
    void stopNote(float velocity, bool allow_tail_off) override;
    
    void pitchWheelMoved(int new_value) override {}
    void controllerMoved(int controller_number, int new_value) override {}
    
    void renderNextBlock(juce::AudioBuffer<float>& output_buffer, int start_sample, int num_samples) override;
    
    // Set playback speed (1.0 = normal, 2.0 = double speed, 0.5 = half speed)
    void set_playback_speed(float speed);
    
    // Set gain (0.0 to 1.0+)
    void set_gain(float gain);
    
private:
    double pitch_ratio = 1.0;
    double source_sample_position = 0.0;
    float left_gain = 0.0f;
    float right_gain = 0.0f;
    float playback_speed = 1.0f;
    float gain_level = 1.0f;
    
    juce::ADSR adsr;
    static constexpr float attack_time = 0.01f;
    static constexpr float decay_time = 0.1f;
    static constexpr float sustain_level = 0.7f;
    static constexpr float release_time = 0.3f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerVoice)
};

} // namespace EmbeddingSpaceSampler

