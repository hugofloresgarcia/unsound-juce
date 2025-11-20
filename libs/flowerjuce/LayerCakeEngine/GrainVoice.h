#pragma once

#include "LayerCakeEnvelope.h"
#include "LayerCakeTypes.h"
#include <flowerjuce/LooperEngine/LooperReadHead.h>
#include <flowerjuce/LooperEngine/TapeLoop.h>
#include <flowerjuce/Panners/PanningUtils.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <array>

// GrainVoice owns a dedicated read head + envelope so it can stream a single
// grain at a time. Voices are intentionally lightweight so LayerCakeEngine can
// keep a fixed std::array of them.
class GrainVoice
{
public:
    explicit GrainVoice(size_t voice_index);

    void prepare(double sample_rate);
    bool trigger(const GrainState& state, TapeLoop& loop, double sample_rate);
    std::array<float, 2> get_next_sample();
    bool is_active() const;
    void force_stop();
    bool get_visual_state(GrainVisualState& state) const;

    const GrainState& get_state() const { return m_state; }

private:
    void rebind_read_head(TapeLoop& loop);
    bool layer_has_audio(const TapeLoop& loop) const;

    size_t m_voice_index{0};
    LayerCakeEnvelope m_envelope;
    std::unique_ptr<LooperReadHead> m_read_head;
    TapeLoop* m_current_loop{nullptr};
    GrainState m_state;
    double m_sample_rate{44100.0};
    float m_pan{0.5f};
    std::atomic<bool> m_active{false};
    float m_loop_start_samples{0.0f};
    float m_loop_end_samples{0.0f};
    float m_recorded_length_samples{0.0f};
    float m_last_env_value{0.0f};
    float m_last_normalized_position{0.0f};
    juce::SpinLock m_voice_lock;
};


