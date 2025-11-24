#pragma once

#include <juce_core/juce_core.h>
#include <cstddef>
#include <vector>

// GrainState captures the playback parameters for a single grain trigger.
// It intentionally mirrors the TODO specification so the UI/PatternClock
// codepath can emit the same struct.
struct GrainState
{
    float loop_start_seconds{0.0f};
    float duration_ms{120.0f};
    float rate_semitones{0.0f};
    float env_attack_ms{5.0f};
    float env_release_ms{120.0f};
    bool play_forward{true};
    int layer{0};
    float pan{0.5f};                  // 0.0 = left, 1.0 = right
    bool should_trigger{false};       // false indicates a "null" GrainState entry

    bool is_valid() const noexcept { return should_trigger; }
};

struct GrainVisualState
{
    bool is_active{false};
    int layer{0};
    size_t voice_index{0};
    float loop_start_samples{0.0f};
    float loop_end_samples{0.0f};
    float recorded_length_samples{0.0f};
    float rate_semitones{0.0f};
    bool play_forward{true};
    float pan{0.5f};
    float envelope_value{0.0f};
    float normalized_position{0.0f};
};

struct LayerBufferSnapshot
{
    std::vector<float> samples;
    size_t recorded_length{0};
    bool has_audio{false};
};


