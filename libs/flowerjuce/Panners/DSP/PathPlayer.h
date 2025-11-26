#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <utility>

class PathPlayer
{
public:
    struct TrajectoryPoint
    {
        float x;
        float y;
        double time; // Time in seconds relative to start
    };

    PathPlayer();
    ~PathPlayer() = default;

    // Set trajectory points. Stores both original for scaling and current working copy.
    void set_trajectory(const std::vector<TrajectoryPoint>& points);
    
    // Get the original trajectory points
    const std::vector<TrajectoryPoint>& get_trajectory() const { return m_original_trajectory; }

    void start_playback();
    void stop_playback();
    
    // Advance to the next point in the trajectory
    void advance();

    // Get current interpolated/smoothed position
    std::pair<float, float> get_current_position() const;
    
    bool is_playing() const { return m_is_playing; }

    void set_playback_speed(float speed);
    float get_playback_speed() const { return m_playback_speed; }

    void set_scale(float scale);
    float get_scale() const { return m_scale; }

    void set_offset(float x, float y);
    std::pair<float, float> get_offset() const { return {m_offset_x, m_offset_y}; }
    
    // Configure smoothing
    void set_smoothing_time(double time_seconds, double sample_rate);
    
    // Update smoothed values (should be called at sample_rate or control rate)
    // Returns true if value changed
    bool update();

private:
    std::vector<TrajectoryPoint> m_trajectory;         // Working copy (scaled)
    std::vector<TrajectoryPoint> m_original_trajectory; // Original copy
    
    size_t m_current_index{0};
    bool m_is_playing{false};
    
    float m_playback_speed{1.0f};
    float m_scale{1.0f};
    float m_offset_x{0.0f};
    float m_offset_y{0.0f};
    
    juce::SmoothedValue<float> m_smoothed_x;
    juce::SmoothedValue<float> m_smoothed_y;
    
    void apply_transformations();
    void update_targets();
};
