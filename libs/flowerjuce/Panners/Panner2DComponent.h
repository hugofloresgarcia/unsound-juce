#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <vector>

// 2D panning control component (Kaoss-pad style XY pad)
// Provides visual feedback and mouse interaction for 2D panning
class Panner2DComponent : public juce::Component, public juce::Timer
{
public:
    // Trajectory point structure
    struct TrajectoryPoint
    {
        float x;
        float y;
        double time; // Time in seconds relative to start of recording
    };

    Panner2DComponent();
    ~Panner2DComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Mouse interaction
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // Pan position control (both 0.0 to 1.0)
    void set_pan_position(float x, float y, juce::NotificationType notification = juce::sendNotification);
    float get_pan_x() const { return m_pan_x; }
    float get_pan_y() const { return m_pan_y; }

    // Callback when pan position changes
    std::function<void(float x, float y)> m_on_pan_change;

    // Trajectory recording/playback control
    void start_recording();
    void stop_recording();
    void start_playback();
    void stop_playback();
    bool is_recording() const { return m_recording_state == Recording; }
    bool is_playing() const { return m_recording_state == Playing; }
    
    // Set whether trajectory recording is enabled (called when [tr] toggle changes)
    void set_trajectory_recording_enabled(bool enabled);
    
    // Advance trajectory playback by one step (called when onset detected with [o] enabled)
    void advance_trajectory_onset();
    
    // Advance trajectory playback by one step (for timer-based playback)
    void advance_trajectory_timer();
    
    // Set whether onset-based triggering is enabled
    void set_onset_triggering_enabled(bool enabled);
    
    // Set smoothing time in seconds (0 = no smoothing)
    void set_smoothing_time(double smoothing_time_seconds);
    double get_smoothing_time() const { return m_smoothing_time; }
    
    // Set trajectory from external source (for premade paths)
    void set_trajectory(const std::vector<TrajectoryPoint>& points, bool start_playback_immediately = true);
    
    // Get current trajectory (returns original unscaled trajectory)
    std::vector<TrajectoryPoint> get_trajectory() const;
    
    // Set playback speed multiplier (0.1 to 2.0, default 1.0)
    void set_playback_speed(float speed);
    float get_playback_speed() const { return m_playback_speed; }
    
    // Set trajectory scale (0.0 to 2.0, default 1.0) - scales radially from center (0.5, 0.5)
    void set_trajectory_scale(float scale);
    float get_trajectory_scale() const { return m_trajectory_scale; }
    
    // Timer callback for playback animation
    void timerCallback() override;

private:
    enum RecordingState
    {
        Idle,
        Recording,
        Playing
    };

    float m_pan_x{0.5f}; // 0.0 = left, 1.0 = right
    float m_pan_y{0.5f}; // 0.0 = bottom, 1.0 = top

    bool m_is_dragging{false};
    
    // Trajectory recording/playback state
    RecordingState m_recording_state{Idle};
    bool m_trajectory_recording_enabled{false};
    bool m_onset_triggering_enabled{false};
    std::vector<TrajectoryPoint> m_trajectory;
    std::vector<TrajectoryPoint> m_original_trajectory; // Store unscaled trajectory for scaling operations
    double m_recording_start_time{0.0};
    double m_last_record_time{0.0};
    static constexpr double m_record_interval{0.1}; // 100ms = 10fps
    
    // Playback state
    size_t m_current_playback_index{0};
    double m_playback_start_time{0.0};
    double m_last_playback_time{0.0};
    static constexpr double m_base_playback_interval{0.1}; // 100ms = 10fps base interval
    double m_playback_interval{m_base_playback_interval}; // Actual interval adjusted by speed
    float m_playback_speed{1.0f}; // Speed multiplier (0.1 to 2.0)
    float m_trajectory_scale{1.0f}; // Scale factor for trajectory (0.0 to 2.0)
    
    // Smoothing for trajectory playback
    double m_smoothing_time{0.0}; // Smoothing time in seconds (0 = no smoothing)
    juce::SmoothedValue<float> m_smoothed_pan_x{0.5f};
    juce::SmoothedValue<float> m_smoothed_pan_y{0.5f};
    double m_last_sample_rate{44100.0};
    
    // Counter for periodic repaints when onset triggering is enabled
    int m_repaint_counter{0};
    
    // Global offset for trajectory playback (applied to all trajectory points)
    float m_trajectory_offset_x{0.0f};
    float m_trajectory_offset_y{0.0f};
    juce::Point<float> m_drag_start_position; // Initial mouse position when starting drag during playback
    bool m_is_adjusting_offset{false}; // True when dragging to adjust offset during playback

    // Convert component-local coordinates to normalized pan coordinates
    juce::Point<float> component_to_pan(juce::Point<float> component_pos) const;
    
    // Convert normalized pan coordinates to component-local coordinates
    juce::Point<float> pan_to_component(float x, float y) const;

    // Clamp pan coordinates to valid range
    void clamp_pan(float& x, float& y) const;
    
    // Interpolate between two trajectory points
    TrajectoryPoint interpolate_trajectory(const TrajectoryPoint& p1, const TrajectoryPoint& p2, float t) const;
    
    // Update pan position with smoothing applied
    void update_pan_position_with_smoothing(float x, float y);
    
    // Apply scale to trajectory points (scales radially from center)
    void apply_trajectory_scale();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Panner2DComponent)
};

