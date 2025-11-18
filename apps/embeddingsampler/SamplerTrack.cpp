#include "SamplerTrack.h"
#include <flowerjuce/Panners/PanningUtils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

namespace EmbeddingSpaceSampler
{

SamplerTrack::SamplerTrack(MultiTrackLooperEngine& engine, int track_index, Shared::MidiLearnManager* midi_manager, const juce::String& panner_type)
    : looper_engine(engine),
      track_index(track_index),
      level_control(engine, track_index, midi_manager, "track" + juce::String(track_index)),
      speed_slider(juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow),
      speed_label("speed", "speed"),
      panner_type(panner_type),
      stereo_pan_slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox),
      pan_label("pan", "pan"),
      pan_coord_label("coord", "0.50, 0.50"),
      track_label("Track", "track " + juce::String(track_index + 1)),
      mute_button("M"),
      midi_learn_manager(midi_manager),
      track_id_prefix("track" + juce::String(track_index))
{
    format_manager.registerBasicFormats();
    
    // Add 8 voices to the sampler
    for (int i = 0; i < num_voices; ++i)
    {
        sampler.addVoice(new SamplerVoice());
    }
    
    sampler.setCurrentPlaybackSampleRate(44100.0); // Will be updated when audio device starts
    
    // Setup level control
    addAndMakeVisible(level_control);
    
    // Setup speed slider
    speed_slider.setRange(0.25, 4.0, 0.01);
    speed_slider.setValue(1.0);
    speed_slider.onValueChange = [this]() { speed_slider_value_changed(); };
    addAndMakeVisible(speed_slider);
    addAndMakeVisible(speed_label);
    
    // Setup track label
    track_label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(track_label);
    
    // Setup mute button
    mute_button.onClick = [this]() { mute_button_toggled(mute_button.getToggleState()); };
    addAndMakeVisible(mute_button);
    
    // Setup panner
    if (panner_type == "Stereo")
    {
        panner = std::make_unique<StereoPanner>();
    }
    else if (panner_type == "Quad")
    {
        panner = std::make_unique<QuadPanner>();
        panner2DComponent = std::make_unique<Panner2DComponent>();
        panner2DComponent->set_pan_position(0.5f, 0.5f); // Center
        panner2DComponent->m_on_pan_change = [this](float x, float y) {
            if (auto* quad_panner = dynamic_cast<QuadPanner*>(panner.get()))
            {
                quad_panner->set_pan(x, y);
                pan_coord_label.setText(juce::String::formatted("%.2f, %.2f", x, y), juce::dontSendNotification);
            }
        };
        addAndMakeVisible(panner2DComponent.get());
    }
    else if (panner_type == "CLEAT")
    {
        panner = std::make_unique<CLEATPanner>();
        panner2DComponent = std::make_unique<Panner2DComponent>();
        panner2DComponent->set_pan_position(0.5f, 0.5f); // Center
        panner2DComponent->m_on_pan_change = [this](float x, float y) {
            if (auto* cleat_panner = dynamic_cast<CLEATPanner*>(panner.get()))
            {
                cleat_panner->set_pan(x, y);
                pan_coord_label.setText(juce::String::formatted("%.2f, %.2f", x, y), juce::dontSendNotification);
            }
        };
        addAndMakeVisible(panner2DComponent.get());
    }
    
    if (panner != nullptr)
    {
        auto& track = looper_engine.get_track(track_index);
        double sample_rate = track.m_write_head.get_sample_rate();
        if (sample_rate <= 0.0)
            sample_rate = 44100.0;
        
        // Only CLEATPanner needs prepare()
        if (auto* cleat_panner = dynamic_cast<CLEATPanner*>(panner.get()))
        {
            cleat_panner->prepare(sample_rate);
        }
    }
    
    // Setup stereo pan slider (for Stereo panner only)
    if (panner_type == "Stereo")
    {
        stereo_pan_slider.setRange(-1.0, 1.0, 0.01);
        stereo_pan_slider.setValue(0.0);
        stereo_pan_slider.onValueChange = [this]() { pan_slider_value_changed(); };
        addAndMakeVisible(stereo_pan_slider);
        addAndMakeVisible(pan_label);
        addAndMakeVisible(pan_coord_label);
    }
    else
    {
        // For Quad and CLEAT, show pan label and coord label
        addAndMakeVisible(pan_label);
        addAndMakeVisible(pan_coord_label);
    }
    
    // Initialize buffers
    mono_buffer.setSize(1, 512);
    sampler_output_buffer.setSize(2, 512);
    
    // Start timer for UI updates
    startTimer(30); // ~30 FPS
}

SamplerTrack::~SamplerTrack()
{
    stopTimer();
    sampler.clearSounds();
    sampler.clearVoices();
}

void SamplerTrack::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void SamplerTrack::resized()
{
    // Layout constants - smaller sizes to fit embedding space window
    const int componentMargin = 5;
    const int trackLabelHeight = 18;
    const int resetButtonSize = 18;
    const int spacingSmall = 4;
    const int knobSize = 50; // Smaller knob
    const int labelHeight = 12; // Smaller labels
    const int buttonSize = 24; // Smaller button
    const int pannerHeight = 100; // Smaller panner to fit
    const int levelControlWidth = 70; // Appropriate size for level control
    
    auto bounds = getLocalBounds().reduced(componentMargin);
    
    // Track label at top with mute button
    auto trackLabelArea = bounds.removeFromTop(trackLabelHeight);
    mute_button.setBounds(trackLabelArea.removeFromRight(buttonSize));
    trackLabelArea.removeFromRight(spacingSmall);
    track_label.setBounds(trackLabelArea);
    bounds.removeFromTop(spacingSmall);
    
    // Reserve space for panner at bottom
    auto bottomArea = bounds.removeFromBottom(pannerHeight + labelHeight + spacingSmall * 2);
    
    // Main controls area
    auto controlsArea = bounds;
    
    // Left column: level control (properly sized)
    const int leftColumnWidth = levelControlWidth;
    auto leftColumn = controlsArea.removeFromLeft(leftColumnWidth);
    level_control.setBounds(leftColumn);
    controlsArea.removeFromLeft(spacingSmall);
    
    // Right side: speed slider
    auto speedArea = controlsArea;
    speed_label.setBounds(speedArea.removeFromTop(labelHeight));
    speedArea.removeFromTop(spacingSmall);
    speed_slider.setBounds(speedArea.removeFromTop(knobSize));
    
    // Panner area at bottom
    auto panLabelArea = bottomArea.removeFromTop(labelHeight);
    pan_label.setBounds(panLabelArea.removeFromLeft(40));
    pan_coord_label.setBounds(panLabelArea); // Coordinates on right
    bottomArea.removeFromTop(spacingSmall);
    
    // Panner component or slider
    if (panner_type == "Stereo" && stereo_pan_slider.isVisible())
    {
        stereo_pan_slider.setBounds(bottomArea);
    }
    else if (panner2DComponent != nullptr && panner2DComponent->isVisible())
    {
        // Limit panner height to its width (keep it square)
        const int pannerMaxHeight = bottomArea.getWidth();
        const int finalPannerHeight = juce::jmin(pannerHeight, pannerMaxHeight);
        auto pannerArea = bottomArea.removeFromTop(finalPannerHeight);
        panner2DComponent->setBounds(pannerArea);
    }
}

void SamplerTrack::trigger_sample(const juce::File& audio_file, float velocity)
{
    if (!audio_file.existsAsFile())
    {
        DBG("SamplerTrack: File does not exist: " + audio_file.getFullPathName());
        return;
    }
    
    // Load audio file
    std::unique_ptr<juce::AudioFormatReader> reader(format_manager.createReaderFor(audio_file));
    if (reader == nullptr)
    {
        DBG("SamplerTrack: Could not create reader for file: " + audio_file.getFullPathName());
        return;
    }
    
    // Read audio data
    juce::AudioBuffer<float> temp_buffer(static_cast<int>(reader->numChannels), 
                                          static_cast<int>(reader->lengthInSamples));
    
    if (!reader->read(&temp_buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true))
    {
        DBG("SamplerTrack: Failed to read audio data");
        return;
    }
    
    // Create sampler sound
    double sample_rate = reader->sampleRate;
    auto sound = std::make_unique<SamplerSound>(audio_file.getFileName(), temp_buffer, sample_rate);
    
    // Add sound to sampler
    sampler.addSound(sound.get());
    loaded_sounds.push_back(std::move(sound));
    
    // Update playback speed for all voices
    float speed = playback_speed.load();
    for (int i = 0; i < sampler.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<SamplerVoice*>(sampler.getVoice(i)))
        {
            voice->set_playback_speed(speed);
            voice->set_gain(level.load());
        }
    }
    
    // Trigger note (use MIDI note 60 = C4)
    sampler.noteOn(1, 60, velocity);
    
    DBG("SamplerTrack: Triggered sample: " + audio_file.getFileName());
}

void SamplerTrack::set_playback_speed(float speed)
{
    playback_speed.store(speed);
    
    // Update all voices
    for (int i = 0; i < sampler.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<SamplerVoice*>(sampler.getVoice(i)))
        {
            voice->set_playback_speed(speed);
        }
    }
}

void SamplerTrack::set_level(float level_value)
{
    level.store(level_value);
    
    // Update all voices
    for (int i = 0; i < sampler.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<SamplerVoice*>(sampler.getVoice(i)))
        {
            voice->set_gain(level_value);
        }
    }
}

void SamplerTrack::set_panner_smoothing_time(double smoothing_time)
{
    // Set smoothing time on 2D panner component if available
    if (panner2DComponent != nullptr)
    {
        panner2DComponent->set_smoothing_time(smoothing_time);
    }
}

void SamplerTrack::set_cleat_gain_power(float gain_power)
{
    if (auto* cleat_panner = dynamic_cast<CLEATPanner*>(panner.get()))
    {
        cleat_panner->set_gain_power(gain_power);
    }
}

bool SamplerTrack::get_pan_position(float& x, float& y) const
{
    if (panner != nullptr)
    {
        if (auto* stereo_panner = dynamic_cast<StereoPanner*>(panner.get()))
        {
            x = stereo_panner->get_pan();
            y = 0.0f;
            return true;
        }
        else if (auto* quad_panner = dynamic_cast<QuadPanner*>(panner.get()))
        {
            x = quad_panner->get_pan_x();
            y = quad_panner->get_pan_y();
            return true;
        }
        else if (auto* cleat_panner = dynamic_cast<CLEATPanner*>(panner.get()))
        {
            x = cleat_panner->get_smoothed_pan_x();
            y = cleat_panner->get_smoothed_pan_y();
            return true;
        }
    }
    // If we have a 2D panner component, use its position
    if (panner2DComponent != nullptr)
    {
        x = panner2DComponent->get_pan_x();
        y = panner2DComponent->get_pan_y();
        return true;
    }
    return false;
}

void SamplerTrack::process_audio_block(const float* const* input_channels, int num_input_channels,
                                      float* const* output_channels, int num_output_channels,
                                      int num_samples)
{
    // Clear output
    for (int channel = 0; channel < num_output_channels; ++channel)
    {
        juce::FloatVectorOperations::clear(output_channels[channel], num_samples);
    }
    
    // Check if muted
    if (mute_button.getToggleState())
    {
        return;
    }
    
    // Ensure buffers are large enough
    if (sampler_output_buffer.getNumSamples() < num_samples)
    {
        sampler_output_buffer.setSize(2, num_samples, false, false, true);
    }
    
    // Clear sampler output buffer
    sampler_output_buffer.clear();
    
    // Render sampler output (need empty MIDI buffer)
    juce::MidiBuffer empty_midi;
    sampler.renderNextBlock(sampler_output_buffer, empty_midi, 0, num_samples);
    
    // Apply level control
    float level_value = level.load();
    sampler_output_buffer.applyGain(level_value);
    
    // Convert to mono for panner
    if (mono_buffer.getNumSamples() < num_samples)
    {
        mono_buffer.setSize(1, num_samples, false, false, true);
    }
    
    mono_buffer.clear();
    const float* left_channel = sampler_output_buffer.getReadPointer(0);
    const float* right_channel = sampler_output_buffer.getNumChannels() > 1 
        ? sampler_output_buffer.getReadPointer(1) 
        : left_channel;
    
    float* mono_data = mono_buffer.getWritePointer(0);
    for (int i = 0; i < num_samples; ++i)
    {
        mono_data[i] = (left_channel[i] + right_channel[i]) * 0.5f;
    }
    
    // Apply panner
    if (panner != nullptr)
    {
        const float* mono_input[1] = { mono_data };
        panner->process_block(mono_input, 1, output_channels, num_output_channels, num_samples);
    }
    else
    {
        // No panner - just copy mono to all output channels
        for (int channel = 0; channel < num_output_channels; ++channel)
        {
            juce::FloatVectorOperations::add(output_channels[channel], mono_data, num_samples);
        }
    }
}

void SamplerTrack::set_sample_rate(double sample_rate)
{
    sampler.setCurrentPlaybackSampleRate(sample_rate);
    
    // Only CLEATPanner needs prepare()
    if (auto* cleat_panner = dynamic_cast<CLEATPanner*>(panner.get()))
    {
        cleat_panner->prepare(sample_rate);
    }
}

void SamplerTrack::clear_look_and_feel()
{
    // Clear look and feel from all components
    track_label.setLookAndFeel(nullptr);
    mute_button.setLookAndFeel(nullptr);
    speed_slider.setLookAndFeel(nullptr);
    speed_label.setLookAndFeel(nullptr);
    stereo_pan_slider.setLookAndFeel(nullptr);
    pan_label.setLookAndFeel(nullptr);
    pan_coord_label.setLookAndFeel(nullptr);
    if (panner2DComponent != nullptr)
    {
        panner2DComponent->setLookAndFeel(nullptr);
    }
    // LevelControl doesn't have clearLookAndFeel - it's a Component so it inherits from juce::Component
}

void SamplerTrack::speed_slider_value_changed()
{
    float speed = static_cast<float>(speed_slider.getValue());
    set_playback_speed(speed);
}

void SamplerTrack::pan_slider_value_changed()
{
    if (panner_type == "Stereo" && panner != nullptr)
    {
        if (auto* stereo_panner = dynamic_cast<StereoPanner*>(panner.get()))
        {
            float pan_value = static_cast<float>(stereo_pan_slider.getValue());
            // Convert from -1..1 range to 0..1 range for StereoPanner
            float normalized_pan = (pan_value + 1.0f) * 0.5f;
            stereo_panner->set_pan(normalized_pan);
            
            // Update coordinate label
            pan_coord_label.setText(juce::String::formatted("%.2f, 0.00", pan_value), juce::dontSendNotification);
        }
    }
}

void SamplerTrack::mute_button_toggled(bool muted)
{
    auto& track = looper_engine.get_track(track_index);
    track.m_read_head.set_muted(muted);
}

void SamplerTrack::timerCallback()
{
    // Update pan coordinate label if panner position changed
    if (panner2DComponent != nullptr && panner2DComponent->isVisible())
    {
        float x = panner2DComponent->get_pan_x();
        float y = panner2DComponent->get_pan_y();
        pan_coord_label.setText(juce::String::formatted("%.2f, %.2f", x, y), juce::dontSendNotification);
    }
    else if (panner != nullptr)
    {
        float x, y;
        if (get_pan_position(x, y))
        {
            pan_coord_label.setText(juce::String::formatted("%.2f, %.2f", x, y), juce::dontSendNotification);
        }
    }
}

} // namespace EmbeddingSpaceSampler

