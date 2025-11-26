#include "Panner2DComponent.h"
#include "../../CustomLookAndFeel.h"
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
    
    // Initialize PathPlayer with default settings
    // Note: 60Hz is the UI timer rate used in this component
    const double ui_update_rate = 60.0;
    m_path_player.set_smoothing_time(m_smoothing_time, ui_update_rate);
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
        
        // If playing, start adjusting offset instead of setting pan position
        if (m_recording_state == Playing)
        {
            m_is_adjusting_offset = true;
            m_drag_start_position = component_to_pan(e.position);
            // Store current offset as starting point for relative adjustment is not directly needed
            // because we accumulate changes in mouseDrag
        }
        else
        {
            // Normal behavior: set pan position directly
            auto pan_pos = component_to_pan(e.position);
            set_pan_position(pan_pos.x, pan_pos.y, juce::sendNotification);
            
            // Start recording if trajectory recording is enabled
            if (m_trajectory_recording_enabled && m_recording_state == Idle)
            {
                start_recording();
            }
        }
    }
}

void Panner2DComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (m_is_dragging && e.mods.isLeftButtonDown())
    {
        // If playing and adjusting offset, update offset based on drag delta
        if (m_recording_state == Playing && m_is_adjusting_offset)
        {
            auto current_pan_pos = component_to_pan(e.position);
            
            // Calculate delta from drag start position
            float delta_x = current_pan_pos.x - m_drag_start_position.x;
            float delta_y = current_pan_pos.y - m_drag_start_position.y;
            
            // Get current offset
            auto [current_offset_x, current_offset_y] = m_path_player.get_offset();
            
            // Update offset (additive)
            float new_offset_x = current_offset_x + delta_x;
            float new_offset_y = current_offset_y + delta_y;
            
            // Set new offset (PathPlayer clamps internally if we wanted, or we clamp here)
            m_path_player.set_offset(new_offset_x, new_offset_y);
            
            // Update drag start position for next delta calculation
            m_drag_start_position = current_pan_pos;
            
            // Force update to reflect changes immediately
            m_path_player.update();
            auto [x, y] = m_path_player.get_current_position();
            set_pan_position(x, y, juce::sendNotification);
        }
        else
        {
            // Normal behavior: set pan position directly
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
                    m_recording_buffer.push_back(point);
                    m_last_record_time = current_time;
                }
            }
        }
    }
}

void Panner2DComponent::mouseUp(const juce::MouseEvent& e)
{
    if (m_is_dragging)
    {
        m_is_dragging = false;
        m_is_adjusting_offset = false;
        
        // Stop recording and start playback if we were recording
        if (m_recording_state == Recording && !m_recording_buffer.empty())
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
    m_recording_buffer.clear();
    m_recording_start_time = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    m_last_record_time = m_recording_start_time;
    
    // Record initial position
    TrajectoryPoint initial_point;
    initial_point.x = m_pan_x;
    initial_point.y = m_pan_y;
    initial_point.time = 0.0;
    m_recording_buffer.push_back(initial_point);
}

void Panner2DComponent::stop_recording()
{
    DBG("Panner2DComponent: Stopping trajectory recording, recorded " + juce::String(m_recording_buffer.size()) + " points");
    m_recording_state = Idle;
    
    // Transfer recorded points to PathPlayer
    if (!m_recording_buffer.empty())
    {
        m_path_player.set_trajectory(m_recording_buffer);
    }
}

void Panner2DComponent::start_playback()
{
    if (m_path_player.get_trajectory().empty())
    {
        DBG("Panner2DComponent: Cannot start playback, trajectory is empty");
        return;
    }
    
    DBG("Panner2DComponent: Starting trajectory playback, " + juce::String(m_path_player.get_trajectory().size()) + " points");
    m_recording_state = Playing;
    
    m_playback_start_time = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    m_last_playback_time = m_playback_start_time;
    
    // Reset offset when starting new playback
    m_path_player.set_offset(0.0f, 0.0f);
    
    // Initialize smoothing logic
    // We need to make sure smoothing starts from current position or snaps
    // PathPlayer::start_playback() updates targets to first point.
    m_path_player.start_playback();
    
    // Start timer for playback animation
    if (m_smoothing_time > 0.0)
    {
        startTimer(16); // ~60fps for smooth updates
    }
    else if (m_onset_triggering_enabled)
    {
        startTimer(16); // ~60fps for responsive visual updates
    }
    else
    {
        startTimer(100); // 100ms = 10fps
    }
}

void Panner2DComponent::stop_playback()
{
    DBG("Panner2DComponent: Stopping trajectory playback");
    m_recording_state = Idle;
    m_path_player.stop_playback();
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
    m_onset_triggering_enabled = enabled;
    
    // If playback is active, update timer state
    if (m_recording_state == Playing)
    {
        if (enabled)
        {
            if (m_smoothing_time > 0.0)
            {
                startTimer(16); // ~60fps for smooth updates
                DBG("Panner2DComponent: Onset triggering enabled with smoothing - timer running for smooth updates");
            }
            else
            {
                startTimer(16); // ~60fps for responsive visual updates
                DBG("Panner2DComponent: Onset triggering enabled - timer running for visual updates, trajectory advances only on onsets");
            }
        }
        else
        {
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
    const double ui_update_rate = 60.0;
    m_path_player.set_smoothing_time(m_smoothing_time, ui_update_rate);
    
    // If playback is active, update timer state based on smoothing
    if (m_recording_state == Playing)
    {
        if (m_smoothing_time > 0.0)
        {
            startTimer(16); 
        }
        else if (m_onset_triggering_enabled)
        {
            startTimer(32);
        }
        else
        {
            startTimer(100);
        }
    }
    
    DBG("Panner2DComponent: Smoothing time set to " + juce::String(m_smoothing_time) + " seconds");
}

void Panner2DComponent::advance_trajectory_onset()
{
    if (m_recording_state != Playing)
        return;
    
    m_path_player.advance();
    
    // If no smoothing, update immediately?
    // The timer will pick up the new position via update() and repaint
    // If not running timer fast enough, we might miss it?
    // Timer runs at 16ms or 32ms if onset enabled, so it should be fine.
}

void Panner2DComponent::advance_trajectory_timer()
{
    if (m_recording_state != Playing)
        return;
    
    m_path_player.advance();
}

void Panner2DComponent::timerCallback()
{
    if (m_recording_state != Playing)
    {
        stopTimer();
        return;
    }
    
    // Advance trajectory on timer ONLY if onset triggering is disabled
    if (!m_onset_triggering_enabled)
    {
        double current_time = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        
        // Recalculate interval based on current speed
        m_playback_interval = m_base_playback_interval / m_path_player.get_playback_speed();
        
        if (current_time - m_last_playback_time >= m_playback_interval)
        {
            advance_trajectory_timer();
            m_last_playback_time = current_time;
        }
    }
    
    // Update smoothing and check for changes
    bool position_changed = false;
    
    // Update smoother step
    m_path_player.update();
    
    auto [x, y] = m_path_player.get_current_position();
    
    // Check if visual position needs update
    if (std::abs(m_pan_x - x) > 0.001f || std::abs(m_pan_y - y) > 0.001f)
    {
        m_pan_x = x;
        m_pan_y = y;
        position_changed = true;
    }
    
    if (position_changed)
    {
        repaint();
        if (m_on_pan_change)
        {
            m_on_pan_change(m_pan_x, m_pan_y);
        }
    }
    else if (m_onset_triggering_enabled)
    {
        // Repaint at lower frequency when no changes
        m_repaint_counter++;
        if (m_repaint_counter >= 4)
        {
            m_repaint_counter = 0;
            repaint();
        }
    }
}

void Panner2DComponent::set_trajectory(const std::vector<TrajectoryPoint>& points, bool start_playback_immediately)
{
    DBG("Panner2DComponent: Setting trajectory with " + juce::String(points.size()) + " points");
    
    if (m_recording_state == Playing)
    {
        stop_playback();
    }
    
    m_path_player.set_trajectory(points);
    
    if (start_playback_immediately && !points.empty())
    {
        start_playback();
    }
}

std::vector<Panner2DComponent::TrajectoryPoint> Panner2DComponent::get_trajectory() const
{
    return m_path_player.get_trajectory();
}

void Panner2DComponent::set_playback_speed(float speed)
{
    m_path_player.set_playback_speed(speed);
    // Interval is recalculated in timerCallback
    DBG("Panner2DComponent: Playback speed set to " + juce::String(speed) + "x");
}

void Panner2DComponent::set_trajectory_scale(float scale)
{
    m_path_player.set_scale(scale);
    DBG("Panner2DComponent: Trajectory scale set to " + juce::String(scale));
    
    // If currently playing, update current position immediately to reflect scale
    if (m_recording_state == Playing)
    {
        m_path_player.update(); // Force update targets
        auto [x, y] = m_path_player.get_current_position();
        set_pan_position(x, y, juce::sendNotification);
    }
}


