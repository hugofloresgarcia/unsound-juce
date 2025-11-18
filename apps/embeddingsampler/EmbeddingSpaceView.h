#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <functional>
#include "ColorPalette.h"

namespace EmbeddingSpaceSampler
{

class EmbeddingSpaceView : public juce::Component, public juce::Timer
{
public:
    EmbeddingSpaceView();
    ~EmbeddingSpaceView() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;
    
    // Load palette data
    bool load_palette(const juce::File& palette_dir);
    
    // Set callback for when a sample should be triggered
    // Parameters: chunk_index, velocity (0.0 to 1.0)
    void set_sample_trigger_callback(std::function<void(int, float)> callback);
    
    // Get audio file for a chunk index
    juce::File get_audio_file(int chunk_index) const;
    
    // Set trigger threshold distance (in normalized coordinates, 0.0 to 1.0)
    void set_trigger_threshold(float threshold) { trigger_threshold = threshold; }
    
    // Set point size
    void set_point_size(float size) { point_size = size; repaint(); }
    
    // Recompute clusters with new DBScan parameters
    void recompute_clusters(double eps, int min_pts);
    
private:
    struct EmbeddingPoint
    {
        juce::Point<double> position;  // t-SNE coordinates (normalized 0-1)
        int cluster_id;
        int chunk_index;
        juce::File audio_file;
    };
    
    std::vector<EmbeddingPoint> points;
    std::vector<juce::File> chunk_files;
    
    // View transform (zoom and pan)
    float zoom_level = 1.0f;
    juce::Point<float> pan_offset{0.0f, 0.0f};
    
    // Mouse interaction
    bool is_dragging = false;
    bool is_panning = false;  // Space bar + drag for panning
    juce::Point<int> last_mouse_pos;
    int last_triggered_point = -1;
    int hovered_point = -1;  // Currently hovered point index
    float trigger_threshold = 0.02f;  // 2% of view width
    float point_size = 8.0f;  // Point size in pixels
    
    // Hover animation
    float hover_animation_time = 0.0f;
    static constexpr float hover_animation_duration = 0.2f;  // seconds
    
    // Color palette
    ColorPalette& color_palette;
    
    // Sample trigger callback
    std::function<void(int, float)> sample_trigger_callback;
    
    // Normalize t-SNE coordinates to 0-1 range
    void normalize_coordinates(std::vector<juce::Point<double>>& coordinates);
    
    // Convert screen coordinates to normalized embedding space
    juce::Point<double> screen_to_embedding_space(const juce::Point<int>& screen_pos) const;
    
    // Find nearest point to a position in embedding space
    int find_nearest_point(const juce::Point<double>& pos, float max_distance) const;
    
    // Get bounds of all points
    juce::Rectangle<double> get_points_bounds() const;
    
    void timerCallback() override;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EmbeddingSpaceView)
};

} // namespace EmbeddingSpaceSampler

