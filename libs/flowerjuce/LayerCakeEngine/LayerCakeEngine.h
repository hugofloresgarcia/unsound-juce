#pragma once

#include "GrainVoice.h"
#include "LayerCakeTypes.h"
#include <flowerjuce/DSP/LfoUGen.h>
#include <flowerjuce/LooperEngine/LooperWriteHead.h>
#include <flowerjuce/Sync/SyncInterface.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include <juce_core/juce_core.h>

class LayerCakeEngine
{
public:
    static constexpr size_t kNumLayers = 6;
    static constexpr size_t kNumVoices = 16;
    static constexpr size_t kNumLfoSlots = 8;
    static constexpr double kMaxLayerDurationSeconds = 10.0;

    LayerCakeEngine();
    ~LayerCakeEngine();

    void prepare(double sample_rate, int block_size, int num_output_channels);

    void process_block(const float* const* input_channel_data,
                       int num_input_channels,
                       float* const* output_channel_data,
                       int num_output_channels,
                       int num_samples);

    void trigger_grain(const GrainState& state);
    void update_lfo_slot(int slot_index, const flower::LayerCakeLfoUGen& generator, bool enabled);
    void set_trigger_lfo_index(int slot_index);
    void set_manual_trigger_template(const GrainState& state);
    void set_manual_reverse_probability(float probability);
    void request_manual_trigger();
    float get_lfo_visual_value(int slot_index) const;

    void set_record_layer(int layer_index);
    int get_record_layer() const { return m_record_layer_index; }

    void set_record_enable(bool should_record);
    bool is_record_enabled() const { return m_record_enabled.load(); }

    void set_record_input_channel(int channel) { m_record_input_channel = channel; }
    int get_record_input_channel() const { return m_record_input_channel; }

    void set_master_gain_db(float db) { m_master_gain_db.store(db); }
    float get_master_gain_db() const { return m_master_gain_db.load(); }
    double get_sample_rate() const { return m_sample_rate; }

    void set_normalize_on_load(bool normalize) { m_normalize_on_load.store(normalize); }
    bool get_normalize_on_load() const { return m_normalize_on_load.load(); }

    // Clock / Transport
    void set_sync_strategy(std::unique_ptr<flower::SyncInterface> sync);
    flower::SyncInterface* get_sync_strategy() const { return m_sync.get(); }

    void set_bpm(float bpm);
    float get_bpm() const;
    double get_master_beats() const;
    void set_transport_playing(bool playing);
    bool is_transport_playing() const;
    void reset_transport();

    std::array<TapeLoop, kNumLayers>& get_layers() { return m_layers; }
    const std::array<TapeLoop, kNumLayers>& get_layers() const { return m_layers; }

    void get_active_grains(std::vector<GrainVisualState>& out_states) const;
    void capture_layer_snapshot(int layer_index, LayerBufferSnapshot& snapshot) const;
    void capture_all_layer_snapshots(std::array<LayerBufferSnapshot, kNumLayers>& snapshots) const;
    void apply_layer_snapshot(int layer_index, const LayerBufferSnapshot& snapshot);
    bool load_layer_from_file(int layer_index, const juce::File& audio_file);

    void apply_spread_randomization(GrainState& state, float spread_amount);
    void apply_direction_randomization(GrainState& state, float reverse_prob);
    
    // Public random accessor for UI randomization
    juce::Random& get_random() { return m_random; }

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
    void sync_lfo_configs();
    void process_lfo_sample(double master_beats);
    void fire_manual_trigger();
    void start_grain_immediate(const GrainState& state);

    std::array<TapeLoop, kNumLayers> m_layers;
    std::array<std::unique_ptr<GrainVoice>, kNumVoices> m_voices;
    std::unique_ptr<LooperWriteHead> m_write_head;

    class GrainTriggerQueue
    {
    public:
        GrainTriggerQueue();

        bool push(const GrainState& state);
        bool pop(GrainState& out_state);
        void clear();

    private:
        static constexpr int kCapacity = 512;
        juce::AbstractFifo m_fifo;
        std::array<GrainState, static_cast<size_t>(kCapacity)> m_buffer;
    };

    struct LfoSnapshot
    {
        flower::LayerCakeLfoUGen generator;
        bool enabled{true};
    };

    struct LfoRuntimeState
    {
        flower::LayerCakeLfoUGen generator;
        std::atomic<bool> enabled{true};
        float prev_value{0.0f};
        float last_value{0.0f};
    };

    struct UiLfoMirror
    {
        std::array<std::atomic<float>, kNumLfoSlots> values;
    };

    GrainTriggerQueue m_pending_grains;

    std::atomic<bool> m_is_prepared{false};
    std::atomic<bool> m_record_enabled{false};
    std::atomic<float> m_master_gain_db{0.0f};
    std::atomic<bool> m_normalize_on_load{false};

    double m_sample_rate{44100.0};
    int m_block_size{0};
    int m_num_output_channels{2};
    int m_record_layer_index{0};
    int m_record_input_channel{-1}; // -1 = follow first channel
    std::atomic<size_t> m_record_cursor{0};

    juce::SpinLock m_record_lock;
    juce::Random m_random;
    juce::AudioFormatManager m_audio_format_manager;
    
    // Sync
    std::unique_ptr<flower::SyncInterface> m_sync;

    // LFO runtime + UI mirrors
    std::array<LfoSnapshot, kNumLfoSlots> m_lfo_pending_configs;
    std::array<std::atomic<bool>, kNumLfoSlots> m_lfo_dirty_flags;
    std::array<LfoRuntimeState, kNumLfoSlots> m_lfo_runtime;
    UiLfoMirror m_lfo_visuals;
    std::atomic<int> m_trigger_lfo_index{-1};

    // Manual trigger template + randomness
    juce::SpinLock m_manual_state_lock;
    GrainState m_manual_trigger_template;
    std::atomic<float> m_manual_reverse_probability{0.0f};
    std::atomic<int> m_manual_trigger_requests{0};
};
