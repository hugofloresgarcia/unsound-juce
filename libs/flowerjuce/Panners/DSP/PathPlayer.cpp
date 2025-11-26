#include "PathPlayer.h"

PathPlayer::PathPlayer()
{
    m_smoothed_x.setCurrentAndTargetValue(0.5f);
    m_smoothed_y.setCurrentAndTargetValue(0.5f);
}

void PathPlayer::set_trajectory(const std::vector<TrajectoryPoint>& points)
{
    m_original_trajectory = points;
    apply_transformations();
}

void PathPlayer::start_playback()
{
    if (m_trajectory.empty())
        return;

    m_is_playing = true;
    m_current_index = 0;
    
    // Reset smoothing to start at the first point to avoid jump
    // Or should we smooth from current position? 
    // Panner2DComponent logic: "Initialize smoothed values to current position"
    // But PathPlayer doesn't know "current position" outside of itself.
    // We'll assume the user wants to smooth *to* the first point if we are far away,
    // or if we are restarting, maybe snap?
    // For now, let's update targets to the first point.
    
    update_targets();
}

void PathPlayer::stop_playback()
{
    m_is_playing = false;
}

void PathPlayer::advance()
{
    if (!m_is_playing || m_trajectory.empty())
        return;

    m_current_index++;
    if (m_current_index >= m_trajectory.size())
    {
        m_current_index = 0;
    }
    
    update_targets();
}

std::pair<float, float> PathPlayer::get_current_position() const
{
    return { m_smoothed_x.getCurrentValue(), m_smoothed_y.getCurrentValue() };
}

void PathPlayer::set_playback_speed(float speed)
{
    m_playback_speed = juce::jlimit(0.1f, 2.0f, speed);
}

void PathPlayer::set_scale(float scale)
{
    m_scale = juce::jlimit(0.0f, 2.0f, scale);
    apply_transformations();
    
    // If playing, update targets to reflect new scale immediately
    if (m_is_playing)
    {
        update_targets();
    }
}

void PathPlayer::set_offset(float x, float y)
{
    m_offset_x = juce::jlimit(-1.0f, 1.0f, x);
    m_offset_y = juce::jlimit(-1.0f, 1.0f, y);
    update_targets();
}

void PathPlayer::set_smoothing_time(double time_seconds, double sample_rate)
{
    if (sample_rate > 0)
    {
        m_smoothed_x.reset(sample_rate, time_seconds);
        m_smoothed_y.reset(sample_rate, time_seconds);
    }
}

bool PathPlayer::update()
{
    float x = m_smoothed_x.getNextValue();
    float y = m_smoothed_y.getNextValue();
    
    // Return true if we are essentially still moving or if the value is different from target
    // Simplified: just return true. Caller can check values if they care about optimization.
    // Or checking if active:
    return m_smoothed_x.isSmoothing() || m_smoothed_y.isSmoothing();
}

void PathPlayer::apply_transformations()
{
    if (m_original_trajectory.empty())
    {
        m_trajectory.clear();
        return;
    }

    m_trajectory.clear();
    m_trajectory.reserve(m_original_trajectory.size());

    const float center_x = 0.5f;
    const float center_y = 0.5f;

    for (const auto& point : m_original_trajectory)
    {
        TrajectoryPoint transformed_point = point;

        // Apply scale (centered)
        float rel_x = point.x - center_x;
        float rel_y = point.y - center_y;
        
        rel_x *= m_scale;
        rel_y *= m_scale;
        
        transformed_point.x = juce::jlimit(0.0f, 1.0f, center_x + rel_x);
        transformed_point.y = juce::jlimit(0.0f, 1.0f, center_y + rel_y);
        
        m_trajectory.push_back(transformed_point);
    }
}

void PathPlayer::update_targets()
{
    if (m_trajectory.empty())
        return;
        
    if (m_current_index >= m_trajectory.size())
        m_current_index = 0;

    const auto& point = m_trajectory[m_current_index];
    
    float target_x = point.x + m_offset_x;
    float target_y = point.y + m_offset_y;
    
    // Clamp
    target_x = juce::jlimit(0.0f, 1.0f, target_x);
    target_y = juce::jlimit(0.0f, 1.0f, target_y);
    
    m_smoothed_x.setTargetValue(target_x);
    m_smoothed_y.setTargetValue(target_y);
}

