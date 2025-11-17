#include "Panner2DComponent.h"
#include "../CustomLookAndFeel.h"
#include <algorithm>

Panner2DComponent::Panner2DComponent()
{
    setOpaque(true);
    panX = 0.5f;
    panY = 0.5f;
    isDragging = false;
    recordingState = Idle;
    trajectoryRecordingEnabled = false;
    onsetTriggeringEnabled = false;
    smoothingTime = 0.0;
    smoothedPanX.setCurrentAndTargetValue(0.5f);
    smoothedPanY.setCurrentAndTargetValue(0.5f);
    lastSampleRate = 44100.0;
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
    const int gridDivisions = 16;
    float gridSpacingX = bounds.getWidth() / gridDivisions;
    float gridSpacingY = bounds.getHeight() / gridDivisions;
    for (int i = 1; i < gridDivisions; ++i)
    {
        // Vertical lines
        g.drawLine(bounds.getX() + i * gridSpacingX, bounds.getY(),
                   bounds.getX() + i * gridSpacingX, bounds.getBottom(), 0.5f);
        // Horizontal lines
        g.drawLine(bounds.getX(), bounds.getY() + i * gridSpacingY,
                   bounds.getRight(), bounds.getY() + i * gridSpacingY, 0.5f);
    }
    
    // Draw center crosshair
    g.setColour(juce::Colour(0xff555555));
    float centerX = bounds.getCentreX();
    float centerY = bounds.getCentreY();
    float crosshairSize = 8.0f;
    g.drawLine(centerX - crosshairSize, centerY, centerX + crosshairSize, centerY, 1.0f);
    g.drawLine(centerX, centerY - crosshairSize, centerX, centerY + crosshairSize, 1.0f);
    
    // Draw pan indicator
    auto panPos = panToComponent(panX, panY);
    float indicatorRadius = 8.0f;
    
    // Draw indicator shadow
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillEllipse(panPos.x - indicatorRadius + 1.0f, panPos.y - indicatorRadius + 1.0f,
                  indicatorRadius * 2.0f, indicatorRadius * 2.0f);
    
    // Draw indicator
    g.setColour(juce::Colour(0xffed1683)); // Pink from CustomLookAndFeel
    g.fillEllipse(panPos.x - indicatorRadius, panPos.y - indicatorRadius,
                  indicatorRadius * 2.0f, indicatorRadius * 2.0f);
    
    // Draw indicator outline
    g.setColour(juce::Colour(0xfff3d430)); // Yellow from CustomLookAndFeel
    g.drawEllipse(panPos.x - indicatorRadius, panPos.y - indicatorRadius,
                  indicatorRadius * 2.0f, indicatorRadius * 2.0f, 2.0f);
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
        isDragging = true;
        auto panPos = componentToPan(e.position);
        setPanPosition(panPos.x, panPos.y, juce::sendNotification);
        
        // Start recording if trajectory recording is enabled
        if (trajectoryRecordingEnabled && recordingState == Idle)
        {
            startRecording();
        }
    }
}

void Panner2DComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (isDragging && e.mods.isLeftButtonDown())
    {
        auto panPos = componentToPan(e.position);
        setPanPosition(panPos.x, panPos.y, juce::sendNotification);
        
        // Record trajectory point if recording
        if (recordingState == Recording)
        {
            double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
            double elapsedTime = currentTime - recordingStartTime;
            
            // Record at 10fps (every 100ms)
            if (currentTime - lastRecordTime >= recordInterval)
            {
                TrajectoryPoint point;
                point.x = panPos.x;
                point.y = panPos.y;
                point.time = elapsedTime;
                trajectory.push_back(point);
                originalTrajectory.push_back(point);
                lastRecordTime = currentTime;
            }
        }
    }
}

void Panner2DComponent::mouseUp(const juce::MouseEvent& e)
{
    if (isDragging)
    {
        isDragging = false;
        
        // Stop recording and start playback if we were recording
        if (recordingState == Recording && !trajectory.empty())
        {
            stopRecording();
            startPlayback();
        }
    }
}

void Panner2DComponent::setPanPosition(float x, float y, juce::NotificationType notification)
{
    clampPan(x, y);
    
    if (panX != x || panY != y)
    {
        panX = x;
        panY = y;
        
        repaint();
        
        if (notification == juce::sendNotification && onPanChange)
        {
            onPanChange(panX, panY);
        }
    }
}

juce::Point<float> Panner2DComponent::componentToPan(juce::Point<float> componentPos) const
{
    auto bounds = getLocalBounds().toFloat();
    
    // Clamp to component bounds
    componentPos.x = juce::jlimit(bounds.getX(), bounds.getRight(), componentPos.x);
    componentPos.y = juce::jlimit(bounds.getY(), bounds.getBottom(), componentPos.y);
    
    // Convert to normalized coordinates (0-1)
    float x = (componentPos.x - bounds.getX()) / bounds.getWidth();
    float y = (componentPos.y - bounds.getY()) / bounds.getHeight();
    
    // Invert Y axis: 0 = bottom, 1 = top
    y = 1.0f - y;
    
    return {x, y};
}

juce::Point<float> Panner2DComponent::panToComponent(float x, float y) const
{
    auto bounds = getLocalBounds().toFloat();
    
    // Clamp pan coordinates
    x = juce::jlimit(0.0f, 1.0f, x);
    y = juce::jlimit(0.0f, 1.0f, y);
    
    // Invert Y axis: 0 = bottom, 1 = top
    y = 1.0f - y;
    
    // Convert to component coordinates
    float componentX = bounds.getX() + x * bounds.getWidth();
    float componentY = bounds.getY() + y * bounds.getHeight();
    
    return {componentX, componentY};
}

void Panner2DComponent::clampPan(float& x, float& y) const
{
    x = juce::jlimit(0.0f, 1.0f, x);
    y = juce::jlimit(0.0f, 1.0f, y);
}

void Panner2DComponent::startRecording()
{
    DBG("Panner2DComponent: Starting trajectory recording");
    recordingState = Recording;
    trajectory.clear();
    originalTrajectory.clear();
    recordingStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    lastRecordTime = recordingStartTime;
    
    // Record initial position
    TrajectoryPoint initialPoint;
    initialPoint.x = panX;
    initialPoint.y = panY;
    initialPoint.time = 0.0;
    trajectory.push_back(initialPoint);
    originalTrajectory.push_back(initialPoint);
}

void Panner2DComponent::stopRecording()
{
    DBG("Panner2DComponent: Stopping trajectory recording, recorded " + juce::String(trajectory.size()) + " points");
    recordingState = Idle;
}

void Panner2DComponent::startPlayback()
{
    if (trajectory.empty())
    {
        DBG("Panner2DComponent: Cannot start playback, trajectory is empty");
        return;
    }
    
    DBG("Panner2DComponent: Starting trajectory playback, " + juce::String(trajectory.size()) + " points");
    recordingState = Playing;
    currentPlaybackIndex = 0;
    playbackStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    lastPlaybackTime = playbackStartTime;
    
    // Initialize smoothed values to current position
    smoothedPanX.setCurrentAndTargetValue(panX);
    smoothedPanY.setCurrentAndTargetValue(panY);
    
    // Start timer for playback animation
    // If smoothing is enabled, we need timer for smooth updates
    // If onset triggering is disabled, we need timer for trajectory advancement
    if (!onsetTriggeringEnabled || smoothingTime > 0.0)
    {
        startTimer(16); // ~60fps for smooth updates (especially important for smoothing)
    }
    else
    {
        // No timer needed - only onsets will advance, no smoothing
        stopTimer();
    }
}

void Panner2DComponent::stopPlayback()
{
    DBG("Panner2DComponent: Stopping trajectory playback");
    recordingState = Idle;
    stopTimer();
}

void Panner2DComponent::setTrajectoryRecordingEnabled(bool enabled)
{
    trajectoryRecordingEnabled = enabled;
    if (!enabled && recordingState == Recording)
    {
        stopRecording();
    }
    if (!enabled && recordingState == Playing)
    {
        stopPlayback();
    }
}

void Panner2DComponent::setOnsetTriggeringEnabled(bool enabled)
{
    bool wasEnabled = onsetTriggeringEnabled;
    onsetTriggeringEnabled = enabled;
    
    // If playback is active, update timer state
    if (recordingState == Playing)
    {
        if (enabled)
        {
            // If smoothing is enabled, we still need timer for smooth updates
            // Otherwise, stop timer - will advance ONLY on onsets
            if (smoothingTime > 0.0)
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
            if (smoothingTime > 0.0)
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

void Panner2DComponent::setSmoothingTime(double smoothingTimeSeconds)
{
    smoothingTime = smoothingTimeSeconds;
    
    // Update smoothed values with new smoothing time
    // Use 60Hz update rate for UI smoothing (timer runs at ~60fps)
    const double uiUpdateRate = 60.0;
    smoothedPanX.reset(uiUpdateRate, smoothingTime);
    smoothedPanY.reset(uiUpdateRate, smoothingTime);
    smoothedPanX.setCurrentAndTargetValue(panX);
    smoothedPanY.setCurrentAndTargetValue(panY);
    lastSampleRate = uiUpdateRate; // Store for reference
    
    // If playback is active, update timer state based on smoothing
    if (recordingState == Playing)
    {
        if (smoothingTime > 0.0)
        {
            // Need timer for smooth updates
            startTimer(16); // ~60fps for smooth updates
        }
        else if (!onsetTriggeringEnabled)
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
    
    DBG("Panner2DComponent: Smoothing time set to " + juce::String(smoothingTime) + " seconds");
}

void Panner2DComponent::advanceTrajectoryOnset()
{
    if (recordingState != Playing || trajectory.empty())
        return;
    
    // Advance to next point in trajectory
    currentPlaybackIndex++;
    
    // Loop if we've reached the end
    if (currentPlaybackIndex >= trajectory.size())
    {
        currentPlaybackIndex = 0;
    }
    
    // Update pan position with smoothing
    const auto& point = trajectory[currentPlaybackIndex];
    updatePanPositionWithSmoothing(point.x, point.y);
}

void Panner2DComponent::advanceTrajectoryTimer()
{
    if (recordingState != Playing || trajectory.empty())
        return;
    
    // Advance to next point in trajectory
    currentPlaybackIndex++;
    
    // Loop if we've reached the end
    if (currentPlaybackIndex >= trajectory.size())
    {
        currentPlaybackIndex = 0;
    }
    
    // Update pan position with smoothing
    const auto& point = trajectory[currentPlaybackIndex];
    updatePanPositionWithSmoothing(point.x, point.y);
}

void Panner2DComponent::timerCallback()
{
    if (recordingState != Playing || trajectory.empty())
    {
        stopTimer();
        return;
    }
    
    // Update smoothed values if smoothing is enabled (always check this first)
    bool needsRepaint = false;
    if (smoothingTime > 0.0)
    {
        smoothedPanX.skip(1);
        smoothedPanY.skip(1);
        
        float smoothedX = smoothedPanX.getNextValue();
        float smoothedY = smoothedPanY.getNextValue();
        
        // Only update if values changed (to avoid unnecessary repaints)
        if (std::abs(panX - smoothedX) > 0.001f || std::abs(panY - smoothedY) > 0.001f)
        {
            panX = smoothedX;
            panY = smoothedY;
            needsRepaint = true;
        }
    }
    
    // Advance trajectory on timer ONLY if onset triggering is disabled
    // If onset triggering is enabled, trajectory advances only via advanceTrajectoryOnset()
    if (!onsetTriggeringEnabled)
    {
        double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        
        // Advance at rate determined by playbackInterval (adjusted by speed)
        // If we're running at 60fps for smoothing, we need to check time difference
        if (currentTime - lastPlaybackTime >= playbackInterval)
        {
            advanceTrajectoryTimer();
            lastPlaybackTime = currentTime;
            needsRepaint = true; // Trajectory advanced, will need repaint
        }
    }
    
    // Repaint if needed
    if (needsRepaint)
    {
        repaint();
        if (onPanChange)
        {
            onPanChange(panX, panY);
        }
    }
}

Panner2DComponent::TrajectoryPoint Panner2DComponent::interpolateTrajectory(const TrajectoryPoint& p1, const TrajectoryPoint& p2, float t) const
{
    TrajectoryPoint result;
    result.x = p1.x + (p2.x - p1.x) * t;
    result.y = p1.y + (p2.y - p1.y) * t;
    result.time = p1.time + (p2.time - p1.time) * t;
    return result;
}

void Panner2DComponent::updatePanPositionWithSmoothing(float x, float y)
{
    clampPan(x, y);
    
    if (smoothingTime > 0.0)
    {
        // Use smoothed values - set target, actual update happens in timer callback
        smoothedPanX.setTargetValue(x);
        smoothedPanY.setTargetValue(y);
    }
    else
    {
        // No smoothing - update directly
        if (panX != x || panY != y)
        {
            panX = x;
            panY = y;
            smoothedPanX.setCurrentAndTargetValue(x);
            smoothedPanY.setCurrentAndTargetValue(y);
            repaint();
            if (onPanChange)
            {
                onPanChange(panX, panY);
            }
        }
    }
}

void Panner2DComponent::setTrajectory(const std::vector<TrajectoryPoint>& points, bool startPlaybackImmediately)
{
    DBG("Panner2DComponent: Setting trajectory with " + juce::String(points.size()) + " points");
    
    // Stop any current playback
    if (recordingState == Playing)
    {
        stopPlayback();
    }
    
    // Store original trajectory and apply current scale
    originalTrajectory = points;
    trajectory = points;
    
    // Apply current scale to trajectory
    applyTrajectoryScale();
    
    // Start playback if requested
    if (startPlaybackImmediately && !trajectory.empty())
    {
        startPlayback();
    }
}

std::vector<Panner2DComponent::TrajectoryPoint> Panner2DComponent::getTrajectory() const
{
    // Return original unscaled trajectory
    return originalTrajectory;
}

void Panner2DComponent::setPlaybackSpeed(float speed)
{
    playbackSpeed = juce::jlimit(0.1f, 2.0f, speed);
    playbackInterval = basePlaybackInterval / playbackSpeed;
    DBG("Panner2DComponent: Playback speed set to " + juce::String(playbackSpeed) + "x, interval = " + juce::String(playbackInterval));
}

void Panner2DComponent::setTrajectoryScale(float scale)
{
    trajectoryScale = juce::jlimit(0.0f, 2.0f, scale);
    DBG("Panner2DComponent: Trajectory scale set to " + juce::String(trajectoryScale));
    
    // Apply scale to trajectory if we have one
    if (!originalTrajectory.empty())
    {
        applyTrajectoryScale();
        
        // If currently playing, update current position
        if (recordingState == Playing && currentPlaybackIndex < trajectory.size())
        {
            const auto& point = trajectory[currentPlaybackIndex];
            updatePanPositionWithSmoothing(point.x, point.y);
        }
    }
}

void Panner2DComponent::applyTrajectoryScale()
{
    if (originalTrajectory.empty())
        return;
    
    trajectory.clear();
    trajectory.reserve(originalTrajectory.size());
    
    const float centerX = 0.5f;
    const float centerY = 0.5f;
    
    for (const auto& point : originalTrajectory)
    {
        TrajectoryPoint scaledPoint;
        
        // Convert to center-relative coordinates
        float relX = point.x - centerX;
        float relY = point.y - centerY;
        
        // Scale
        relX *= trajectoryScale;
        relY *= trajectoryScale;
        
        // Convert back and clamp
        scaledPoint.x = juce::jlimit(0.0f, 1.0f, centerX + relX);
        scaledPoint.y = juce::jlimit(0.0f, 1.0f, centerY + relY);
        scaledPoint.time = point.time;
        
        trajectory.push_back(scaledPoint);
    }
}

