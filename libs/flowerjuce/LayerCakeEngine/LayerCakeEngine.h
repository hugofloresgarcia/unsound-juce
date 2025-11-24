#pragma once

#include "GrainVoice.h"
#include "LayerCakeTypes.h"
#include <flowerjuce/LooperEngine/LooperWriteHead.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <deque>
#include <memory>
#include <vector>
#include <juce_core/juce_core.h>

class PatternClock;

class LayerCakeEngine
{
public:
    static constexpr size_t kNumLayers = 6;
    static constexpr size_t kNumVoices = 16;
    static constexpr double kMaxLayerDurationSeconds = 10.0;

    LayerCakeEngine();
    ~LayerCakeEngine();

    void prepare(double sample_rate, int block_size, int num_output_channels);

    void process_block(const float* const* input_channel_data,
                       int num_input_channels,
                       float* const* output_channel_data,
                       int num_output_channels,
                       int num_samples);

    void trigger_grain(const GrainState& state, bool triggered_by_pattern_clock = false);

    void set_record_layer(int layer_index);
    int get_record_layer() const { return m_record_layer_index; }

    void set_record_enable(bool should_record);
    bool is_record_enabled() const { return m_record_enabled.load(); }

    void set_record_input_channel(int channel) { m_record_input_channel = channel; }
    int get_record_input_channel() const { return m_record_input_channel; }

    void set_master_gain_db(float db) { m_master_gain_db.store(db); }
    float get_master_gain_db() const { return m_master_gain_db.load(); }
    double get_sample_rate() const { return m_sample_rate; }

    std::array<TapeLoop, kNumLayers>& get_layers() { return m_layers; }
    const std::array<TapeLoop, kNumLayers>& get_layers() const { return m_layers; }
    PatternClock* get_pattern_clock() const { return m_pattern_clock.get(); }
    void get_active_grains(std::vector<GrainVisualState>& out_states) const;
    void capture_layer_snapshot(int layer_index, LayerBufferSnapshot& snapshot) const;
    void capture_all_layer_snapshots(std::array<LayerBufferSnapshot, kNumLayers>& snapshots) const;
    void apply_layer_snapshot(int layer_index, const LayerBufferSnapshot& snapshot);
    bool load_layer_from_file(int layer_index, const juce::File& audio_file);

    void apply_spread_randomization(GrainState& state, float spread_amount);
    void apply_direction_randomization(GrainState& state, float reverse_prob);
private:
    void allocate_layers(double sample_rate);
    void rebuild_write_head();
    bool layer_index_valid(int layer_index) const;
    void drain_pending_grains();
    GrainVoice* find_free_voice();
    void process_recording_sample(const float* const* input_channel_data,
                                  int num_input_channels,
                                  int buffer_sample_index,
                                  size_t absolute_sample_index);

    std::array<TapeLoop, kNumLayers> m_layers;
    std::array<std::unique_ptr<GrainVoice>, kNumVoices> m_voices;
    std::unique_ptr<LooperWriteHead> m_write_head;
    std::unique_ptr<PatternClock> m_pattern_clock;

    juce::SpinLock m_grain_queue_lock;
    std::deque<GrainState> m_pending_grains;

    std::atomic<bool> m_is_prepared{false};
    std::atomic<bool> m_record_enabled{false};
    std::atomic<float> m_master_gain_db{0.0f};

    double m_sample_rate{44100.0};
    int m_block_size{0};
    int m_num_output_channels{2};
    int m_record_layer_index{0};
    int m_record_input_channel{-1}; // -1 = follow first channel
    std::atomic<size_t> m_record_cursor{0};

    juce::SpinLock m_record_lock;
    juce::Random m_random;
    juce::AudioFormatManager m_audio_format_manager;
};


