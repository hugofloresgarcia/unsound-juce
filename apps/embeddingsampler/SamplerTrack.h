#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include <flowerjuce/Engine/MultiTrackLooperEngine.h>
#include "SamplerVoice.h"
#include <flowerjuce/Components/LevelControl.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Panners/Panner.h>
#include <flowerjuce/Panners/StereoPanner.h>
#include <flowerjuce/Panners/QuadPanner.h>
#include <flowerjuce/Panners/CLEATPanner.h>
#include <memory>
#include <atomic>

namespace EmbeddingSpaceSampler
{

class SamplerTrack : public juce::Component, public juce::Timer
{
public:
    SamplerTrack(MultiTrackLooperEngine& engine, int track_index, Shared::MidiLearnManager* midi_manager = nullptr, const juce::String& panner_type = "Stereo");
    ~SamplerTrack() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Trigger a sample from an audio file
    void trigger_sample(const juce::File& audio_file, float velocity = 1.0f);
    
    // Set playback speed (0.25 to 4.0)
    void set_playback_speed(float speed);
    float get_playback_speed() const { return playback_speed.load(); }
    
    // Set level/gain (0.0 to 1.0+)
    void set_level(float level);
    float get_level() const { return level.load(); }
    
    // Set panner smoothing time
    void set_panner_smoothing_time(double smoothing_time);
    
    // Set CLEAT gain power
    void set_cleat_gain_power(float gain_power);
    
    // Get current pan position
    bool get_pan_position(float& x, float& y) const;
    
    // Process audio block (called from audio thread)
    void process_audio_block(const float* const* input_channels, int num_input_channels,
                            float* const* output_channels, int num_output_channels,
                            int num_samples);
    
    // Set sample rate (called when audio device starts)
    void set_sample_rate(double sample_rate);
    
    // Clear LookAndFeel references
    void clear_look_and_feel();
    
private:
    MultiTrackLooperEngine& looper_engine;
    int track_index;
    
    // Polyphonic sampler (8 voices)
    static constexpr int num_voices = 8;
    juce::Synthesiser sampler;
    std::vector<std::unique_ptr<SamplerSound>> loaded_sounds;
    juce::AudioFormatManager format_manager;
    
    // Level control
    Shared::LevelControl level_control;
    std::atomic<float> level{1.0f};
    
    // Speed control
    std::atomic<float> playback_speed{1.0f};
    juce::Slider speed_slider;
    juce::Label speed_label;
    
    // Panner
    juce::String panner_type;
    std::unique_ptr<Panner> panner;
    juce::Slider stereo_pan_slider;
    juce::Label pan_label;
    juce::Label pan_coord_label;
    
    // Track label
    juce::Label track_label;
    
    // Mute button
    juce::ToggleButton mute_button;
    
    // Audio processing
    juce::AudioBuffer<float> mono_buffer;
    juce::AudioBuffer<float> sampler_output_buffer;
    
    // MIDI learn support
    Shared::MidiLearnManager* midi_learn_manager;
    juce::String track_id_prefix;
    
    void apply_look_and_feel();
    void speed_slider_value_changed();
    void pan_slider_value_changed();
    void mute_button_toggled(bool muted);
    
    void timerCallback() override;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerTrack)
};

} // namespace EmbeddingSpaceSampler

