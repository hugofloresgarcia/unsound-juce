#include "LayerCakeEngine.h"
#include <flowerjuce/Sync/LinkSyncStrategy.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>

namespace
{
float decibels_to_gain(float db)
{
    return juce::Decibels::decibelsToGain(db);
}

void copy_lfo_settings(const flower::LayerCakeLfoUGen& source, flower::LayerCakeLfoUGen& dest)
{
    dest.set_mode(source.get_mode());
    dest.set_rate_hz(source.get_rate_hz());
    dest.set_clock_division(source.get_clock_division());
    dest.set_pattern_length(source.get_pattern_length());
    dest.set_pattern_buffer(source.get_pattern_buffer());
    dest.set_level(source.get_level());
    dest.set_width(source.get_width());
    dest.set_phase_offset(source.get_phase_offset());
    dest.set_delay(source.get_delay());
    dest.set_delay_div(source.get_delay_div());
    dest.set_slop(source.get_slop());
    dest.set_euclidean_steps(source.get_euclidean_steps());
    dest.set_euclidean_triggers(source.get_euclidean_triggers());
    dest.set_euclidean_rotation(source.get_euclidean_rotation());
    dest.set_random_skip(source.get_random_skip());
    dest.set_loop_beats(source.get_loop_beats());
    dest.set_bipolar(source.get_bipolar());
    dest.set_random_seed(source.get_random_seed());
}
} // namespace

LayerCakeEngine::GrainTriggerQueue::GrainTriggerQueue()
    : m_fifo(kCapacity)
{
    for (auto& slot : m_buffer)
        slot = GrainState{};
}

bool LayerCakeEngine::GrainTriggerQueue::push(const GrainState& state)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    m_fifo.prepareToWrite(1, start1, size1, start2, size2);
    const int total = size1 + size2;
    if (total == 0)
        return false;

    if (size1 > 0)
        m_buffer[static_cast<size_t>(start1)] = state;
    else if (size2 > 0)
        m_buffer[static_cast<size_t>(start2)] = state;

    m_fifo.finishedWrite(total);
    return true;
}

bool LayerCakeEngine::GrainTriggerQueue::pop(GrainState& out_state)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    m_fifo.prepareToRead(1, start1, size1, start2, size2);
    const int total = size1 + size2;
    if (total == 0)
        return false;

    if (size1 > 0)
        out_state = m_buffer[static_cast<size_t>(start1)];
    else if (size2 > 0)
        out_state = m_buffer[static_cast<size_t>(start2)];

    m_fifo.finishedRead(total);
    return true;
}

void LayerCakeEngine::GrainTriggerQueue::clear()
{
    m_fifo.reset();
}

LayerCakeEngine::LayerCakeEngine()
{
    DBG("LayerCakeEngine ctor");
    // Initialize with LinkSyncStrategy enabled by default or ready to be enabled
    m_sync = std::make_unique<flower::LinkSyncStrategy>(120.0);
    
    m_audio_format_manager.registerBasicFormats();
    for (size_t voice = 0; voice < kNumVoices; ++voice)
    {
        m_voices[voice] = std::make_unique<GrainVoice>(voice);
    }

    for (auto& value : m_lfo_visuals.values)
        value.store(0.0f, std::memory_order_relaxed);

    for (auto& dirty : m_lfo_dirty_flags)
        dirty.store(false, std::memory_order_relaxed);

    for (auto& runtime : m_lfo_runtime)
        runtime.enabled.store(false, std::memory_order_relaxed);

    m_manual_trigger_template.should_trigger = false;
}

LayerCakeEngine::~LayerCakeEngine() = default;

void LayerCakeEngine::prepare(double sample_rate, int block_size, int num_output_channels)
{
    DBG("LayerCakeEngine::prepare sample_rate=" + juce::String(sample_rate)
        + " block=" + juce::String(block_size)
        + " outputs=" + juce::String(num_output_channels));

    m_sample_rate = sample_rate;
    m_block_size = block_size;
    m_num_output_channels = num_output_channels;

    if (m_sync)
        m_sync->prepare(sample_rate, block_size);

    allocate_layers(sample_rate);

    for (auto& voice : m_voices)
        voice->prepare(sample_rate);

    rebuild_write_head();

    m_is_prepared.store(true);
}

void LayerCakeEngine::allocate_layers(double sample_rate)
{
    for (auto& layer : m_layers)
    {
        layer.allocate_buffer(sample_rate, kMaxLayerDurationSeconds);
        layer.clear_buffer();
    }
}

void LayerCakeEngine::rebuild_write_head()
{
    if (!layer_index_valid(m_record_layer_index))
        m_record_layer_index = 0;

    m_write_head = std::make_unique<LooperWriteHead>(m_layers[m_record_layer_index]);
    m_write_head->set_sample_rate(m_sample_rate);
    m_write_head->set_record_enable(m_record_enabled.load());
    m_write_head->set_input_channel(m_record_input_channel);
    m_record_cursor.store(0);
}

bool LayerCakeEngine::layer_index_valid(int layer_index) const
{
    return layer_index >= 0 && layer_index < static_cast<int>(kNumLayers);
}

void LayerCakeEngine::update_lfo_slot(int slot_index,
                                      const flower::LayerCakeLfoUGen& generator,
                                      bool enabled)
{
    if (slot_index < 0 || slot_index >= static_cast<int>(kNumLfoSlots))
    {
        DBG("LayerCakeEngine::update_lfo_slot invalid index=" + juce::String(slot_index));
        return;
    }

    m_lfo_pending_configs[static_cast<size_t>(slot_index)].generator = generator;
    m_lfo_pending_configs[static_cast<size_t>(slot_index)].enabled = enabled;
    m_lfo_dirty_flags[static_cast<size_t>(slot_index)].store(true, std::memory_order_release);
}

void LayerCakeEngine::set_trigger_lfo_index(int slot_index)
{
    if (slot_index < -1 || slot_index >= static_cast<int>(kNumLfoSlots))
    {
        DBG("LayerCakeEngine::set_trigger_lfo_index invalid index=" + juce::String(slot_index));
        return;
    }
    m_trigger_lfo_index.store(slot_index, std::memory_order_relaxed);
}

void LayerCakeEngine::set_manual_trigger_template(const GrainState& state)
{
    const juce::SpinLock::ScopedLockType lock(m_manual_state_lock);
    m_manual_trigger_template = state;
}

void LayerCakeEngine::set_manual_reverse_probability(float probability)
{
    m_manual_reverse_probability.store(juce::jlimit(0.0f, 1.0f, probability),
                                       std::memory_order_relaxed);
}

void LayerCakeEngine::request_manual_trigger()
{
    m_manual_trigger_requests.fetch_add(1, std::memory_order_release);
}

float LayerCakeEngine::get_lfo_visual_value(int slot_index) const
{
    if (slot_index < 0 || slot_index >= static_cast<int>(kNumLfoSlots))
        return 0.0f;
    return m_lfo_visuals.values[static_cast<size_t>(slot_index)].load(std::memory_order_relaxed);
}

void LayerCakeEngine::sync_lfo_configs()
{
    for (size_t i = 0; i < kNumLfoSlots; ++i)
    {
        if (!m_lfo_dirty_flags[i].exchange(false, std::memory_order_acq_rel))
            continue;

        auto& snapshot = m_lfo_pending_configs[i];
        auto& runtime = m_lfo_runtime[i];
        copy_lfo_settings(snapshot.generator, runtime.generator);
        runtime.enabled.store(snapshot.enabled, std::memory_order_relaxed);
    }
}

void LayerCakeEngine::process_lfo_sample(double master_beats)
{
    const int trigger_index = m_trigger_lfo_index.load(std::memory_order_relaxed);
    bool should_trigger_manual = false;

    for (size_t i = 0; i < kNumLfoSlots; ++i)
    {
        auto& runtime = m_lfo_runtime[i];
        runtime.prev_value = runtime.last_value;

        if (!runtime.enabled.load(std::memory_order_relaxed))
        {
            runtime.last_value = 0.0f;
            m_lfo_visuals.values[i].store(0.0f, std::memory_order_relaxed);
            continue;
        }

        const float scaled = runtime.generator.advance_clocked(master_beats);
        runtime.last_value = scaled;
        m_lfo_visuals.values[i].store(scaled, std::memory_order_relaxed);

        if (static_cast<int>(i) == trigger_index)
        {
            if (runtime.prev_value <= 0.0f && scaled > 0.0f)
                should_trigger_manual = true;
        }
    }

    if (should_trigger_manual)
        fire_manual_trigger();
}

void LayerCakeEngine::fire_manual_trigger()
{
    GrainState manual_state;
    {
        const juce::SpinLock::ScopedLockType lock(m_manual_state_lock);
        manual_state = m_manual_trigger_template;
    }

    if (!manual_state.should_trigger)
        return;

    apply_direction_randomization(manual_state,
                                  m_manual_reverse_probability.load(std::memory_order_relaxed));
    start_grain_immediate(manual_state);
}

void LayerCakeEngine::start_grain_immediate(const GrainState& state)
{
    if (!state.is_valid())
        return;

    auto* voice = find_free_voice();
    if (voice == nullptr)
    {
        DBG("LayerCakeEngine::start_grain_immediate voice steal");
        voice = m_voices.front().get();
        voice->force_stop();
    }

    const int layer_index = juce::jlimit(0, static_cast<int>(kNumLayers) - 1, state.layer);
    auto& loop = m_layers[static_cast<size_t>(layer_index)];
    if (!voice->trigger(state, loop, m_sample_rate))
        DBG("LayerCakeEngine::start_grain_immediate trigger failed");
}
void LayerCakeEngine::set_record_layer(int layer_index)
{
    if (!layer_index_valid(layer_index))
    {
        DBG("LayerCakeEngine::set_record_layer invalid layer=" + juce::String(layer_index));
        return;
    }

    if (layer_index == m_record_layer_index){
        DBG("LayerCakeEngine::set_record_layer same layer=" + juce::String(layer_index));
        return;
    }

    const juce::SpinLock::ScopedLockType lock(m_record_lock);
    m_record_layer_index = layer_index;
    rebuild_write_head();
    // DBG("LayerCakeEngine::set_record_layer index=" + juce::String(layer_index));
}

void LayerCakeEngine::set_record_enable(bool should_record)
{
    const juce::SpinLock::ScopedLockType lock(m_record_lock);

    if (m_write_head == nullptr)
    {
        DBG("LayerCakeEngine::set_record_enable called before prepare");
        return;
    }

    if (should_record == m_record_enabled.load())
        return;

    m_record_enabled.store(should_record);
    m_write_head->set_record_enable(should_record);

    if (should_record)
    {
        auto& layer = m_layers[m_record_layer_index];
        if (!layer.m_has_recorded.load())
            layer.clear_buffer();
        m_record_cursor.store(0);
        DBG("LayerCakeEngine::set_record_enable START record layer=" + juce::String(m_record_layer_index));
    }
    else
    {
        const double buffer_size = static_cast<double>(m_layers[m_record_layer_index].get_buffer_size());
        const double final_position = juce::jmin(buffer_size, static_cast<double>(m_record_cursor.load()));
        m_write_head->finalize_recording(static_cast<float>(final_position));
        DBG("LayerCakeEngine::set_record_enable STOP at samples=" + juce::String(final_position));
    }
}

void LayerCakeEngine::set_sync_strategy(std::unique_ptr<flower::SyncInterface> sync)
{
    if (sync)
    {
        m_sync = std::move(sync);
        if (m_is_prepared)
        {
            m_sync->prepare(m_sample_rate, m_block_size);
        }
    }
}

void LayerCakeEngine::set_bpm(float bpm)
{
    if (m_sync)
        m_sync->set_tempo(static_cast<double>(bpm));
}

float LayerCakeEngine::get_bpm() const
{
    if (m_sync)
        return static_cast<float>(m_sync->get_tempo());
    return 120.0f;
}

double LayerCakeEngine::get_master_beats() const
{
    if (m_sync)
        return m_sync->get_current_beat();
    return 0.0;
}

void LayerCakeEngine::set_transport_playing(bool playing)
{
    if (m_sync)
        m_sync->set_playing(playing);
}

bool LayerCakeEngine::is_transport_playing() const
{
    if (m_sync)
        return m_sync->is_playing();
    return false;
}

void LayerCakeEngine::reset_transport()
{
    // With Link, we might not want to reset unless explicitly asked via session
    // But if this is called from UI to 'stop and reset'
    if (m_sync)
    {
        m_sync->request_reset();
    }
}

void LayerCakeEngine::process_block(const float* const* input_channel_data,
                                   int num_input_channels,
                                   float* const* output_channel_data,
                                   int num_output_channels,
                                   int num_samples)
{
    if (!m_is_prepared.load())
    {
        DBG("LayerCakeEngine::process_block called before prepare");
        return;
    }

    if (output_channel_data == nullptr || num_output_channels == 0)
    {
        DBG("LayerCakeEngine::process_block missing output buffers");
        return;
    }

    sync_lfo_configs();
    
    if (m_sync)
        m_sync->process(num_samples, m_sample_rate);

    int manual_requests = m_manual_trigger_requests.exchange(0, std::memory_order_acq_rel);
    while (manual_requests-- > 0)
        fire_manual_trigger();

    drain_pending_grains();

    for (int channel = 0; channel < num_output_channels; ++channel)
    {
        if (output_channel_data[channel] != nullptr)
            juce::FloatVectorOperations::clear(output_channel_data[channel], num_samples);
    }

    // Calculate beat progression for this block
    double current_beat = 0.0;
    double beats_per_sample = 0.0;
    bool transport_playing = false;

    if (m_sync)
    {
        transport_playing = m_sync->is_playing();
        current_beat = m_sync->get_current_beat();
        const double bpm = m_sync->get_tempo();
        const double samples_per_second = m_sample_rate > 0 ? m_sample_rate : 44100.0;
        if (transport_playing)
        {
             beats_per_sample = (bpm / 60.0) / samples_per_second;
        }
    }

    const float master_gain = decibels_to_gain(m_master_gain_db.load());

    size_t recorded_samples = 0;
    const size_t block_cursor = m_record_cursor.load();

    for (int sample = 0; sample < num_samples; ++sample)
    {
        // Advance beat for LFOs
        // We start at current_beat (start of block) and increment
        double sample_beat = current_beat;
        if (transport_playing)
        {
            sample_beat += static_cast<double>(sample) * beats_per_sample;
        }

        process_lfo_sample(sample_beat);

        if (m_record_enabled.load())
        {
            process_recording_sample(input_channel_data,
                                     num_input_channels,
                                     sample,
                                     block_cursor + recorded_samples);
            ++recorded_samples;
        }

        float left_mix = 0.0f;
        float right_mix = 0.0f;

        for (auto& voice : m_voices)
        {
            const auto sample_pair = voice->get_next_sample();
            left_mix += sample_pair[0];
            right_mix += sample_pair[1];
        }

        left_mix *= master_gain;
        right_mix *= master_gain;

        if (num_output_channels > 0 && output_channel_data[0] != nullptr)
            output_channel_data[0][sample] += left_mix;

        if (num_output_channels > 1 && output_channel_data[1] != nullptr)
            output_channel_data[1][sample] += right_mix;

        for (int channel = 2; channel < num_output_channels; ++channel)
        {
            if (output_channel_data[channel] != nullptr)
                output_channel_data[channel][sample] += (left_mix + right_mix) * 0.5f;
        }
    }

    if (recorded_samples > 0)
        m_record_cursor.store(block_cursor + recorded_samples);
}

void LayerCakeEngine::process_recording_sample(const float* const* input_channel_data,
                                               int num_input_channels,
                                               int buffer_sample_index,
                                               size_t absolute_sample_index)
{
    static std::atomic<bool> logged_missing_write_head{false};
    if (m_write_head == nullptr)
    {
        if (!logged_missing_write_head.exchange(true))
            DBG("LayerCakeEngine::process_recording_sample missing write head");
        return;
    }

    static std::atomic<bool> logged_missing_input{false};
    if (input_channel_data == nullptr || num_input_channels == 0)
    {
        if (!logged_missing_input.exchange(true))
            DBG("LayerCakeEngine::process_recording_sample missing input channels");
        return;
    }

    const int channel = (m_record_input_channel >= 0 && m_record_input_channel < num_input_channels)
                            ? m_record_input_channel
                            : 0;

    const float* input = input_channel_data[channel];
    if (input == nullptr)
    {
        static std::atomic<bool> logged_null_channel{false};
        if (!logged_null_channel.exchange(true))
            DBG("LayerCakeEngine::process_recording_sample null input buffer");
        return;
    }

    const float input_sample = input[buffer_sample_index];
    const float record_position = static_cast<float>(absolute_sample_index);
    m_write_head->process_sample(input_sample, record_position);
}

void LayerCakeEngine::trigger_grain(const GrainState& state)
{
    GrainState queued_state = state;
    queued_state.should_trigger = true;

    if (!layer_index_valid(queued_state.layer))
    {
        DBG("LayerCakeEngine::trigger_grain invalid layer=" + juce::String(queued_state.layer));
        return;
    }

    if (!m_pending_grains.push(queued_state))
        DBG("LayerCakeEngine::trigger_grain queue full");
}

void LayerCakeEngine::apply_spread_randomization(GrainState& state, float spread_amount) 
{
    const float spread = juce::jlimit(0.0f, 1.0f, spread_amount);
    if (spread <= 0.0f)
        return;

    if (!layer_index_valid(state.layer))
        return;

    auto& loop = m_layers[static_cast<size_t>(state.layer)];
    const size_t recorded_samples = loop.m_recorded_length.load();
    if (recorded_samples == 0 || m_sample_rate <= 0.0)
        return;

    const double recorded_seconds = static_cast<double>(recorded_samples) / m_sample_rate;
    const double duration_seconds = juce::jmax(0.0, static_cast<double>(state.duration_ms) * 0.001);
    const double max_start = juce::jmax(0.0, recorded_seconds - duration_seconds);
    if (max_start <= 0.0)
    {
        state.loop_start_seconds = 0.0f;
        return;
    }

    const double max_offset = juce::jmin(max_start, recorded_seconds * spread * 0.5);
    if (max_offset <= 0.0)
        return;

    const double clamped_start = juce::jlimit(0.0, max_start, static_cast<double>(state.loop_start_seconds));
    const double offset = (m_random.nextDouble() * 2.0 - 1.0) * max_offset;
    const double new_start = juce::jlimit(0.0, max_start, clamped_start + offset);
    state.loop_start_seconds = static_cast<float>(new_start);
}

void LayerCakeEngine::apply_direction_randomization(GrainState& state, float reverse_prob) 
{
    const float probability = juce::jlimit(0.0f, 1.0f, reverse_prob);
    if (probability <= 0.0f)
    {
        state.play_forward = true;
        return;
    }

    const bool should_reverse = m_random.nextFloat() < probability;
    state.play_forward = !should_reverse;
}

void LayerCakeEngine::drain_pending_grains()
{
    GrainState state;
    while (m_pending_grains.pop(state))
    {
        if (!state.is_valid())
            continue;
        start_grain_immediate(state);
    }
}

GrainVoice* LayerCakeEngine::find_free_voice()
{
    for (auto& voice : m_voices)
    {
        if (!voice->is_active())
            return voice.get();
    }
    return nullptr;
}

void LayerCakeEngine::get_active_grains(std::vector<GrainVisualState>& out_states) const
{
    out_states.clear();
    for (const auto& voice : m_voices)
    {
        if (voice == nullptr)
            continue;

        GrainVisualState state;
        if (voice->get_visual_state(state))
            out_states.push_back(state);
    }
}

void LayerCakeEngine::capture_layer_snapshot(int layer_index, LayerBufferSnapshot& snapshot) const
{
    if (!layer_index_valid(layer_index))
    {
        DBG("LayerCakeEngine::capture_layer_snapshot invalid layer=" + juce::String(layer_index));
        snapshot.samples.clear();
        snapshot.recorded_length = 0;
        snapshot.has_audio = false;
        return;
    }

    const auto& loop = m_layers[static_cast<size_t>(layer_index)];
    const juce::ScopedLock sl(loop.m_lock);
    const auto& buffer = loop.get_buffer();
    const size_t recorded = juce::jmin(loop.m_recorded_length.load(), buffer.size());

    if (recorded == 0 || !loop.m_has_recorded.load())
    {
        snapshot.samples.clear();
        snapshot.recorded_length = 0;
        snapshot.has_audio = false;
        return;
    }

    snapshot.samples.resize(recorded);
    if (recorded > 0)
        juce::FloatVectorOperations::copy(snapshot.samples.data(), buffer.data(), static_cast<int>(recorded));
    snapshot.recorded_length = recorded;
    snapshot.has_audio = true;
}

void LayerCakeEngine::capture_all_layer_snapshots(std::array<LayerBufferSnapshot, kNumLayers>& snapshots) const
{
    for (size_t i = 0; i < snapshots.size(); ++i)
        capture_layer_snapshot(static_cast<int>(i), snapshots[i]);
}

void LayerCakeEngine::apply_layer_snapshot(int layer_index, const LayerBufferSnapshot& snapshot)
{
    if (!layer_index_valid(layer_index))
    {
        DBG("LayerCakeEngine::apply_layer_snapshot invalid layer=" + juce::String(layer_index));
        return;
    }

    auto& loop = m_layers[static_cast<size_t>(layer_index)];
    const juce::ScopedLock sl(loop.m_lock);

    if (!snapshot.has_audio || snapshot.recorded_length == 0 || snapshot.samples.empty())
    {
        DBG("LayerCakeEngine::apply_layer_snapshot clearing layer=" + juce::String(layer_index));
        loop.m_recorded_length.store(0);
        loop.m_has_recorded.store(false);
        auto& buffer = loop.get_buffer();
        if (!buffer.empty())
            juce::FloatVectorOperations::clear(buffer.data(), static_cast<int>(buffer.size()));
        return;
    }

    auto& buffer = loop.get_buffer();
    if (buffer.size() < snapshot.samples.size())
        buffer.resize(snapshot.samples.size(), 0.0f);

    std::copy(snapshot.samples.begin(), snapshot.samples.end(), buffer.begin());
    loop.m_recorded_length.store(snapshot.recorded_length);
    loop.m_has_recorded.store(true);
}

bool LayerCakeEngine::load_layer_from_file(int layer_index, const juce::File& audio_file)
{
    if (!layer_index_valid(layer_index))
    {
        DBG("LayerCakeEngine::load_layer_from_file early return invalid layer=" + juce::String(layer_index));
        return false;
    }

    if (!audio_file.existsAsFile())
    {
        DBG("LayerCakeEngine::load_layer_from_file early return missing file=" + audio_file.getFullPathName());
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(m_audio_format_manager.createReaderFor(audio_file));
    if (reader == nullptr)
    {
        DBG("LayerCakeEngine::load_layer_from_file early return unable to create reader for " + audio_file.getFileName());
        return false;
    }

    auto& loop = m_layers[static_cast<size_t>(layer_index)];
    const juce::ScopedLock sl(loop.m_lock);

    if (loop.get_buffer().empty())
    {
        if (m_sample_rate <= 0.0)
        {
            DBG("LayerCakeEngine::load_layer_from_file early return buffer not allocated sampleRate<=0");
            return false;
        }
        loop.allocate_buffer(m_sample_rate, kMaxLayerDurationSeconds);
    }

    auto& buffer = loop.get_buffer();
    if (buffer.empty())
    {
        DBG("LayerCakeEngine::load_layer_from_file early return buffer still empty after allocate");
        return false;
    }

    const size_t max_samples = buffer.size();
    const size_t reader_samples = static_cast<size_t>(juce::jmax<juce::int64>(0, reader->lengthInSamples));
    const size_t samples_to_copy = juce::jmin(max_samples, reader_samples);
    if (samples_to_copy == 0)
    {
        DBG("LayerCakeEngine::load_layer_from_file early return no samples to copy");
        return false;
    }

    juce::AudioBuffer<float> temp_buffer(static_cast<int>(juce::jmax<juce::uint32>(1, reader->numChannels)),
                                         static_cast<int>(samples_to_copy));
    if (!reader->read(&temp_buffer, 0, static_cast<int>(samples_to_copy), 0, true, true))
    {
        DBG("LayerCakeEngine::load_layer_from_file early return failed to read audio data");
        return false;
    }

    if (temp_buffer.getNumChannels() == 1)
    {
        const float* source = temp_buffer.getReadPointer(0);
        std::copy(source, source + static_cast<int>(samples_to_copy), buffer.begin());
    }
    else
    {
        const int channels = temp_buffer.getNumChannels();
        for (size_t sample = 0; sample < samples_to_copy; ++sample)
        {
            float mixed = 0.0f;
            for (int channel = 0; channel < channels; ++channel)
                mixed += temp_buffer.getSample(channel, static_cast<int>(sample));
            buffer[sample] = mixed / static_cast<float>(channels);
        }
    }

    if (samples_to_copy < max_samples)
        std::fill(buffer.begin() + static_cast<ptrdiff_t>(samples_to_copy), buffer.end(), 0.0f);

    loop.m_recorded_length.store(samples_to_copy);
    loop.m_has_recorded.store(true);

    DBG("LayerCakeEngine::load_layer_from_file loaded "
        + audio_file.getFileName() + " into layer=" + juce::String(layer_index));
    return true;
}
