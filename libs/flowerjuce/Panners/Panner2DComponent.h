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
    void setPanPosition(float x, float y, juce::NotificationType notification = juce::sendNotification);
    float getPanX() const { return panX; }
    float getPanY() const { return panY; }

    // Callback when pan position changes
    std::function<void(float x, float y)> onPanChange;

    // Trajectory recording/playback control
    void startRecording();
    void stopRecording();
    void startPlayback();
    void stopPlayback();
    bool isRecording() const { return recordingState == Recording; }
    bool isPlaying() const { return recordingState == Playing; }
    
    // Set whether trajectory recording is enabled (called when [tr] toggle changes)
    void setTrajectoryRecordingEnabled(bool enabled);
    
    // Advance trajectory playback by one step (called when onset detected with [o] enabled)
    void advanceTrajectoryOnset();
    
    // Advance trajectory playback by one step (for timer-based playback)
    void advanceTrajectoryTimer();
    
    // Set whether onset-based triggering is enabled
    void setOnsetTriggeringEnabled(bool enabled);
    
    // Set smoothing time in seconds (0 = no smoothing)
    void setSmoothingTime(double smoothingTimeSeconds);
    double getSmoothingTime() const { return smoothingTime; }
    
    // Set trajectory from external source (for premade paths)
    void setTrajectory(const std::vector<TrajectoryPoint>& points, bool startPlaybackImmediately = true);
    
    // Get current trajectory (returns original unscaled trajectory)
    std::vector<TrajectoryPoint> getTrajectory() const;
    
    // Set playback speed multiplier (0.1 to 2.0, default 1.0)
    void setPlaybackSpeed(float speed);
    float getPlaybackSpeed() const { return playbackSpeed; }
    
    // Set trajectory scale (0.0 to 2.0, default 1.0) - scales radially from center (0.5, 0.5)
    void setTrajectoryScale(float scale);
    float getTrajectoryScale() const { return trajectoryScale; }
    
    // Timer callback for playback animation
    void timerCallback() override;

private:
    enum RecordingState
    {
        Idle,
        Recording,
        Playing
    };

    float panX{0.5f}; // 0.0 = left, 1.0 = right
    float panY{0.5f}; // 0.0 = bottom, 1.0 = top

    bool isDragging{false};
    
    // Trajectory recording/playback state
    RecordingState recordingState{Idle};
    bool trajectoryRecordingEnabled{false};
    bool onsetTriggeringEnabled{false};
    std::vector<TrajectoryPoint> trajectory;
    std::vector<TrajectoryPoint> originalTrajectory; // Store unscaled trajectory for scaling operations
    double recordingStartTime{0.0};
    double lastRecordTime{0.0};
    static constexpr double recordInterval{0.1}; // 100ms = 10fps
    
    // Playback state
    size_t currentPlaybackIndex{0};
    double playbackStartTime{0.0};
    double lastPlaybackTime{0.0};
    static constexpr double basePlaybackInterval{0.1}; // 100ms = 10fps base interval
    double playbackInterval{basePlaybackInterval}; // Actual interval adjusted by speed
    float playbackSpeed{1.0f}; // Speed multiplier (0.1 to 2.0)
    float trajectoryScale{1.0f}; // Scale factor for trajectory (0.0 to 2.0)
    
    // Smoothing for trajectory playback
    double smoothingTime{0.0}; // Smoothing time in seconds (0 = no smoothing)
    juce::SmoothedValue<float> smoothedPanX{0.5f};
    juce::SmoothedValue<float> smoothedPanY{0.5f};
    double lastSampleRate{44100.0};

    // Convert component-local coordinates to normalized pan coordinates
    juce::Point<float> componentToPan(juce::Point<float> componentPos) const;
    
    // Convert normalized pan coordinates to component-local coordinates
    juce::Point<float> panToComponent(float x, float y) const;

    // Clamp pan coordinates to valid range
    void clampPan(float& x, float& y) const;
    
    // Interpolate between two trajectory points
    TrajectoryPoint interpolateTrajectory(const TrajectoryPoint& p1, const TrajectoryPoint& p2, float t) const;
    
    // Update pan position with smoothing applied
    void updatePanPositionWithSmoothing(float x, float y);
    
    // Apply scale to trajectory points (scales radially from center)
    void applyTrajectoryScale();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Panner2DComponent)
};

