#include "Panner2DComponent.h"
#include "../CustomLookAndFeel.h"
#include <algorithm>

Panner2DComponent::Panner2DComponent()
{
    setOpaque(true);
    m_pan_x = 0.5f;
    m_pan_y = 0.5f;
    m_is_dragging = false;
    m_recording_state = Idle;
    m_trajectory_recording_enabled = false;
    m_onset_triggering_enabled = false;
    m_smoothing_time = 0.0;
    m_smoothed_pan_x.setCurrentAndTargetValue(0.5f);
    m_smoothed_pan_y.setCurrentAndTargetValue(0.5f);
    m_last_sample_rate = 44100.0;
}

Panner2DComponent::~Panner2DComponent()
{
    stopTimer();
}

void Panner2DComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Fill background
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(bounds, 4.0f);
    
    // Draw bright border
    g.setColour(juce::Colour(0xfff3d430)); // Bright yellow from CustomLookAndFeel
    g.drawRoundedRectangle(bounds, 4.0f, 3.0f);
    
    // Draw dense grid (16x16)
    g.setColour(juce::Colour(0xff333333));
    const int grid_divisions = 16;
    float grid_spacing_x = bounds.getWidth() / grid_divisions;
    float grid_spacing_y = bounds.getHeight() / grid_divisions;
    for (int i = 1; i < grid_divisions; ++i)
    {
        // Vertical lines
        g.drawLine(bounds.getX() + i * grid_spacing_x, bounds.getY(),
                   bounds.getX() + i * grid_spacing_x, bounds.getBottom(), 0.5f);
        // Horizontal lines
        g.drawLine(bounds.getX(), bounds.getY() + i * grid_spacing_y,
                   bounds.getRight(), bounds.getY() + i * grid_spacing_y, 0.5f);
    }
    
    // Draw center crosshair
    g.setColour(juce::Colour(0xff555555));
    float center_x = bounds.getCentreX();
    float center_y = bounds.getCentreY();
    float crosshair_size = 8.0f;
    g.drawLine(center_x - crosshair_size, center_y, center_x + crosshair_size, center_y, 1.0f);
    g.drawLine(center_x, center_y - crosshair_size, center_x, center_y + crosshair_size, 1.0f);
    
    // Draw pan indicator
    auto pan_pos = pan_to_component(m_pan_x, m_pan_y);
    float indicator_radius = 8.0f;
    
    // Draw indicator shadow
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillEllipse(pan_pos.x - indicator_radius + 1.0f, pan_pos.y - indicator_radius + 1.0f,
                  indicator_radius * 2.0f, indicator_radius * 2.0f);
    
    // Draw indicator
    g.setColour(juce::Colour(0xffed1683)); // Pink from CustomLookAndFeel
    g.fillEllipse(pan_pos.x - indicator_radius, pan_pos.y - indicator_radius,
                  indicator_radius * 2.0f, indicator_radius * 2.0f);
    
    // Draw indicator outline
    g.setColour(juce::Colour(0xfff3d430)); // Yellow from CustomLookAndFeel
    g.drawEllipse(pan_pos.x - indicator_radius, pan_pos.y - indicator_radius,
                  indicator_radius * 2.0f, indicator_radius * 2.0f, 2.0f);
}

void Panner2DComponent::resized()
{
    // Trigger repaint when resized
    repaint();
}

void Panner2DComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown())
    {
        m_is_dragging = true;
        auto pan_pos = component_to_pan(e.position);
        set_pan_position(pan_pos.x, pan_pos.y, juce::sendNotification);
        
        // Start recording if trajectory recording is enabled
        if (m_trajectory_recording_enabled && m_recording_state == Idle)
        {
            start_recording();
        }
    }
}

void Panner2DComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (m_is_dragging && e.mods.isLeftButtonDown())
    {
        auto pan_pos = component_to_pan(e.position);
        set_pan_position(pan_pos.x, pan_pos.y, juce::sendNotification);
        
        // Record trajectory point if recording
        if (m_recording_state == Recording)
        {
            double current_time = juce::Time::getMillisecondCounterHiRes() / 1000.0;
            double elapsed_time = current_time - m_recording_start_time;
            
            // Record at 10fps (every 100ms)
            if (current_time - m_last_record_time >= m_record_interval)
            {
                TrajectoryPoint point;
                point.x = pan_pos.x;
                point.y = pan_pos.y;
                point.time = elapsed_time;
                m_trajectory.push_back(point);
                m_original_trajectory.push_back(point);
                m_last_record_time = current_time;
            }
        }
    }
}

void Panner2DComponent::mouseUp(const juce::MouseEvent& e)
{
    if (m_is_dragging)
    {
        m_is_dragging = false;
        
        // Stop recording and start playback if we were recording
        if (m_recording_state == Recording && !m_trajectory.empty())
        {
            stop_recording();
            start_playback();
        }
    }
}

void Panner2DComponent::set_pan_position(float x, float y, juce::NotificationType notification)
{
    clamp_pan(x, y);
    
    if (m_pan_x != x || m_pan_y != y)
    {
        m_pan_x = x;
        m_pan_y = y;
        
        repaint();
        
        if (notification == juce::sendNotification && m_on_pan_change)
        {
            m_on_pan_change(m_pan_x, m_pan_y);
        }
    }
}

juce::Point<float> Panner2DComponent::component_to_pan(juce::Point<float> component_pos) const
{
    auto bounds = getLocalBounds().toFloat();
    
    // Clamp to component bounds
    component_pos.x = juce::jlimit(bounds.getX(), bounds.getRight(), component_pos.x);
    component_pos.y = juce::jlimit(bounds.getY(), bounds.getBottom(), component_pos.y);
    
    // Convert to normalized coordinates (0-1)
    float x = (component_pos.x - bounds.getX()) / bounds.getWidth();
    float y = (component_pos.y - bounds.getY()) / bounds.getHeight();
    
    // Invert X axis: 0 = right, 1 = left
    x = 1.0f - x;
    
    // Invert Y axis: 0 = bottom, 1 = top
    y = 1.0f - y;
    
    return {x, y};
}

juce::Point<float> Panner2DComponent::pan_to_component(float x, float y) const
{
    auto bounds = getLocalBounds().toFloat();
    
    // Clamp pan coordinates
    x = juce::jlimit(0.0f, 1.0f, x);
    y = juce::jlimit(0.0f, 1.0f, y);
    
    // Invert X axis: 0 = right, 1 = left
    x = 1.0f - x;
    
    // Invert Y axis: 0 = bottom, 1 = top
    y = 1.0f - y;
    
    // Convert to component coordinates
    float component_x = bounds.getX() + x * bounds.getWidth();
    float component_y = bounds.getY() + y * bounds.getHeight();
    
    return {component_x, component_y};
}

void Panner2DComponent::clamp_pan(float& x, float& y) const
{
    x = juce::jlimit(0.0f, 1.0f, x);
    y = juce::jlimit(0.0f, 1.0f, y);
}

void Panner2DComponent::start_recording()
{
    DBG("Panner2DComponent: Starting trajectory recording");
    m_recording_state = Recording;
    m_trajectory.clear();
    m_original_trajectory.clear();
    m_recording_start_time = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    m_last_record_time = m_recording_start_time;
    
    // Record initial position
    TrajectoryPoint initial_point;
    initial_point.x = m_pan_x;
    initial_point.y = m_pan_y;
    initial_point.time = 0.0;
    m_trajectory.push_back(initial_point);
    m_original_trajectory.push_back(initial_point);
}

void Panner2DComponent::stop_recording()
{
    DBG("Panner2DComponent: Stopping trajectory recording, recorded " + juce::String(m_trajectory.size()) + " points");
    m_recording_state = Idle;
}

void Panner2DComponent::start_playback()
{
    if (m_trajectory.empty())
    {
        DBG("Panner2DComponent: Cannot start playback, trajectory is empty");
        return;
    }
    
    DBG("Panner2DComponent: Starting trajectory playback, " + juce::String(m_trajectory.size()) + " points");
    m_recording_state = Playing;
    m_current_playback_index = 0;
    m_playback_start_time = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    m_last_playback_time = m_playback_start_time;
    
    // Initialize smoothed values to current position
    m_smoothed_pan_x.setCurrentAndTargetValue(m_pan_x);
    m_smoothed_pan_y.setCurrentAndTargetValue(m_pan_y);
    
    // Start timer for playback animation
    // If smoothing is enabled, we need timer for smooth updates
    // If onset triggering is disabled, we need timer for trajectory advancement
    if (!m_onset_triggering_enabled || m_smoothing_time > 0.0)
    {
        startTimer(16); // ~60fps for smooth updates (especially important for smoothing)
    }
    else
    {
        // No timer needed - only onsets will advance, no smoothing
        stopTimer();
    }
}

void Panner2DComponent::stop_playback()
{
    DBG("Panner2DComponent: Stopping trajectory playback");
    m_recording_state = Idle;
    stopTimer();
}

void Panner2DComponent::set_trajectory_recording_enabled(bool enabled)
{
    m_trajectory_recording_enabled = enabled;
    if (!enabled && m_recording_state == Recording)
    {
        stop_recording();
    }
    if (!enabled && m_recording_state == Playing)
    {
        stop_playback();
    }
}

void Panner2DComponent::set_onset_triggering_enabled(bool enabled)
{
    bool was_enabled = m_onset_triggering_enabled;
    m_onset_triggering_enabled = enabled;
    
    // If playback is active, update timer state
    if (m_recording_state == Playing)
    {
        if (enabled)
        {
            // If smoothing is enabled, we still need timer for smooth updates
            // Otherwise, stop timer - will advance ONLY on onsets
            if (m_smoothing_time > 0.0)
            {
                startTimer(16); // ~60fps for smooth updates
                DBG("Panner2DComponent: Onset triggering enabled with smoothing - timer running for smooth updates only");
            }
            else
            {
                stopTimer();
                DBG("Panner2DComponent: Onset triggering enabled - timer stopped, trajectory will advance only on onsets");
            }
        }
        else
        {
            // Start timer - will advance at fixed rate (and smooth if enabled)
            if (m_smoothing_time > 0.0)
            {
                startTimer(16); // ~60fps for smooth updates
            }
            else
            {
                startTimer(100); // 100ms = 10fps
            }
            DBG("Panner2DComponent: Onset triggering disabled - timer started for fixed-rate playback");
        }
    }
}

void Panner2DComponent::set_smoothing_time(double smoothing_time_seconds)
{
    m_smoothing_time = smoothing_time_seconds;
    
    // Update smoothed values with new smoothing time
    // Use 60Hz update rate for UI smoothing (timer runs at ~60fps)
    const double ui_update_rate = 60.0;
    m_smoothed_pan_x.reset(ui_update_rate, m_smoothing_time);
    m_smoothed_pan_y.reset(ui_update_rate, m_smoothing_time);
    m_smoothed_pan_x.setCurrentAndTargetValue(m_pan_x);
    m_smoothed_pan_y.setCurrentAndTargetValue(m_pan_y);
    m_last_sample_rate = ui_update_rate; // Store for reference
    
    // If playback is active, update timer state based on smoothing
    if (m_recording_state == Playing)
    {
        if (m_smoothing_time > 0.0)
        {
            // Need timer for smooth updates
            startTimer(16); // ~60fps for smooth updates
        }
        else if (!m_onset_triggering_enabled)
        {
            // No smoothing, but timer needed for trajectory advancement
            startTimer(100); // 100ms = 10fps
        }
        else
        {
            // No smoothing, onset triggering enabled - no timer needed
            stopTimer();
        }
    }
    
    DBG("Panner2DComponent: Smoothing time set to " + juce::String(m_smoothing_time) + " seconds");
}

void Panner2DComponent::advance_trajectory_onset()
{
    if (m_recording_state != Playing || m_trajectory.empty())
        return;
    
    // Advance to next point in trajectory
    m_current_playback_index++;
    
    // Loop if we've reached the end
    if (m_current_playback_index >= m_trajectory.size())
    {
        m_current_playback_index = 0;
    }
    
    // Update pan position with smoothing
    const auto& point = m_trajectory[m_current_playback_index];
    update_pan_position_with_smoothing(point.x, point.y);
}

void Panner2DComponent::advance_trajectory_timer()
{
    if (m_recording_state != Playing || m_trajectory.empty())
        return;
    
    // Advance to next point in trajectory
    m_current_playback_index++;
    
    // Loop if we've reached the end
    if (m_current_playback_index >= m_trajectory.size())
    {
        m_current_playback_index = 0;
    }
    
    // Update pan position with smoothing
    const auto& point = m_trajectory[m_current_playback_index];
    update_pan_position_with_smoothing(point.x, point.y);
}

void Panner2DComponent::timerCallback()
{
    if (m_recording_state != Playing || m_trajectory.empty())
    {
        stopTimer();
        return;
    }
    
    // Update smoothed values if smoothing is enabled (always check this first)
    bool needs_repaint = false;
    if (m_smoothing_time > 0.0)
    {
        m_smoothed_pan_x.skip(1);
        m_smoothed_pan_y.skip(1);
        
        float smoothed_x = m_smoothed_pan_x.getNextValue();
        float smoothed_y = m_smoothed_pan_y.getNextValue();
        
        // Only update if values changed (to avoid unnecessary repaints)
        if (std::abs(m_pan_x - smoothed_x) > 0.001f || std::abs(m_pan_y - smoothed_y) > 0.001f)
        {
            m_pan_x = smoothed_x;
            m_pan_y = smoothed_y;
            needs_repaint = true;
        }
    }
    
    // Advance trajectory on timer ONLY if onset triggering is disabled
    // If onset triggering is enabled, trajectory advances only via advance_trajectory_onset()
    if (!m_onset_triggering_enabled)
    {
        double current_time = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        
        // Advance at rate determined by m_playback_interval (adjusted by speed)
        // If we're running at 60fps for smoothing, we need to check time difference
        if (current_time - m_last_playback_time >= m_playback_interval)
        {
            advance_trajectory_timer();
            m_last_playback_time = current_time;
            needs_repaint = true; // Trajectory advanced, will need repaint
        }
    }
    
    // Repaint if needed
    if (needs_repaint)
    {
        repaint();
        if (m_on_pan_change)
        {
            m_on_pan_change(m_pan_x, m_pan_y);
        }
    }
}

Panner2DComponent::TrajectoryPoint Panner2DComponent::interpolate_trajectory(const TrajectoryPoint& p1, const TrajectoryPoint& p2, float t) const
{
    TrajectoryPoint result;
    result.x = p1.x + (p2.x - p1.x) * t;
    result.y = p1.y + (p2.y - p1.y) * t;
    result.time = p1.time + (p2.time - p1.time) * t;
    return result;
}

void Panner2DComponent::update_pan_position_with_smoothing(float x, float y)
{
    clamp_pan(x, y);
    
    if (m_smoothing_time > 0.0)
    {
        // Use smoothed values - set target, actual update happens in timer callback
        m_smoothed_pan_x.setTargetValue(x);
        m_smoothed_pan_y.setTargetValue(y);
    }
    else
    {
        // No smoothing - update directly
        if (m_pan_x != x || m_pan_y != y)
        {
            m_pan_x = x;
            m_pan_y = y;
            m_smoothed_pan_x.setCurrentAndTargetValue(x);
            m_smoothed_pan_y.setCurrentAndTargetValue(y);
            repaint();
            if (m_on_pan_change)
            {
                m_on_pan_change(m_pan_x, m_pan_y);
            }
        }
    }
}

void Panner2DComponent::set_trajectory(const std::vector<TrajectoryPoint>& points, bool start_playback_immediately)
{
    DBG("Panner2DComponent: Setting trajectory with " + juce::String(points.size()) + " points");
    
    // Stop any current playback
    if (m_recording_state == Playing)
    {
        stop_playback();
    }
    
    // Store original trajectory and apply current scale
    m_original_trajectory = points;
    m_trajectory = points;
    
    // Apply current scale to trajectory
    apply_trajectory_scale();
    
    // Start playback if requested
    if (start_playback_immediately && !m_trajectory.empty())
    {
        start_playback();
    }
}

std::vector<Panner2DComponent::TrajectoryPoint> Panner2DComponent::get_trajectory() const
{
    // Return original unscaled trajectory
    return m_original_trajectory;
}

void Panner2DComponent::set_playback_speed(float speed)
{
    m_playback_speed = juce::jlimit(0.1f, 2.0f, speed);
    m_playback_interval = m_base_playback_interval / m_playback_speed;
    DBG("Panner2DComponent: Playback speed set to " + juce::String(m_playback_speed) + "x, interval = " + juce::String(m_playback_interval));
}

void Panner2DComponent::set_trajectory_scale(float scale)
{
    m_trajectory_scale = juce::jlimit(0.0f, 2.0f, scale);
    DBG("Panner2DComponent: Trajectory scale set to " + juce::String(m_trajectory_scale));
    
    // Apply scale to trajectory if we have one
    if (!m_original_trajectory.empty())
    {
        apply_trajectory_scale();
        
        // If currently playing, update current position
        if (m_recording_state == Playing && m_current_playback_index < m_trajectory.size())
        {
            const auto& point = m_trajectory[m_current_playback_index];
            update_pan_position_with_smoothing(point.x, point.y);
        }
    }
}

void Panner2DComponent::apply_trajectory_scale()
{
    if (m_original_trajectory.empty())
        return;
    
    m_trajectory.clear();
    m_trajectory.reserve(m_original_trajectory.size());
    
    const float center_x = 0.5f;
    const float center_y = 0.5f;
    
    for (const auto& point : m_original_trajectory)
    {
        TrajectoryPoint scaled_point;
        
        // Convert to center-relative coordinates
        float rel_x = point.x - center_x;
        float rel_y = point.y - center_y;
        
        // Scale
        rel_x *= m_trajectory_scale;
        rel_y *= m_trajectory_scale;
        
        // Convert back and clamp
        scaled_point.x = juce::jlimit(0.0f, 1.0f, center_x + rel_x);
        scaled_point.y = juce::jlimit(0.0f, 1.0f, center_y + rel_y);
        scaled_point.time = point.time;
        
        m_trajectory.push_back(scaled_point);
    }
}

