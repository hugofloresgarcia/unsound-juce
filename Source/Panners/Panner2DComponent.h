#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// 2D panning control component (Kaoss-pad style XY pad)
// Provides visual feedback and mouse interaction for 2D panning
class Panner2DComponent : public juce::Component
{
public:
    Panner2DComponent();
    ~Panner2DComponent() override = default;

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

private:
    float panX{0.5f}; // 0.0 = left, 1.0 = right
    float panY{0.5f}; // 0.0 = bottom, 1.0 = top

    bool isDragging{false};

    // Convert component-local coordinates to normalized pan coordinates
    juce::Point<float> componentToPan(juce::Point<float> componentPos) const;
    
    // Convert normalized pan coordinates to component-local coordinates
    juce::Point<float> panToComponent(float x, float y) const;

    // Clamp pan coordinates to valid range
    void clampPan(float& x, float& y) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Panner2DComponent)
};

