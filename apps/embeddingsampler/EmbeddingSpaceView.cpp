#include "EmbeddingSpaceView.h"
#include "CLAP/PaletteVisualization.h"
#include "CLAP/SoundPaletteManager.h"
#include "DBScan.h"
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <algorithm>

namespace EmbeddingSpaceSampler
{

EmbeddingSpaceView::EmbeddingSpaceView()
    : color_palette(ColorPalette::get_instance())
{
    setOpaque(true);
    setWantsKeyboardFocus(true);  // Enable keyboard focus for space bar panning
    startTimer(30); // Update at ~30 FPS
}

EmbeddingSpaceView::~EmbeddingSpaceView()
{
    stopTimer();
}

void EmbeddingSpaceView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    auto bounds = getLocalBounds().toFloat();
    float width = bounds.getWidth();
    float height = bounds.getHeight();
    
    if (points.empty())
    {
        g.setColour(juce::Colours::grey);
        g.setFont(16.0f);
        g.drawText("No palette loaded", getLocalBounds(), juce::Justification::centred);
        return;
    }
    
    // Draw points with zoom and pan transform
    int visible_count = 0;
    int drawn_count = 0;
    
    // Debug: Draw a test point in center to verify rendering works
    g.setColour(juce::Colours::yellow);
    g.fillEllipse(width / 2.0f - 5.0f, height / 2.0f - 5.0f, 10.0f, 10.0f);
    
    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto& point = points[i];
        
        // Simple transform: points are normalized 0-1, map directly to screen
        // Then apply zoom and pan
        float normalized_x = static_cast<float>(point.position.x);
        float normalized_y = static_cast<float>(point.position.y);
        
        // Apply pan (in normalized space)
        normalized_x += pan_offset.x;
        normalized_y += pan_offset.y;
        
        // Apply zoom (centered around 0.5)
        float zoomed_x = (normalized_x - 0.5f) * zoom_level + 0.5f;
        float zoomed_y = (normalized_y - 0.5f) * zoom_level + 0.5f;
        
        // Convert to screen coordinates
        float x = zoomed_x * width;
        float y = zoomed_y * height;
        
        // Check if point is within visible bounds (with margin for point size)
        float margin = point_size * 2.0f;
        if (x < -margin || x > width + margin || y < -margin || y > height + margin)
            continue;
        
        visible_count++;
        
        // Get color based on cluster
        juce::Colour color = color_palette.get_color(point.cluster_id);
        
        // Debug: Log first few points
        if (drawn_count < 3)
        {
            DBG("EmbeddingSpaceView: Point " + juce::String(i) + 
                " screen=(" + juce::String(x, 1) + ", " + juce::String(y, 1) + 
                ") norm=(" + juce::String(point.position.x, 3) + ", " + juce::String(point.position.y, 3) +
                ") cluster=" + juce::String(point.cluster_id) + 
                " color=" + color.toString());
        }
        
        // Hover animation
        float hover_scale = 1.0f;
        if (static_cast<int>(i) == hovered_point)
        {
            hover_scale = 1.0f + juce::jmin(1.0f, hover_animation_time / hover_animation_duration) * 0.5f;
            color = color.brighter(0.3f);
        }
        
        // Highlight last triggered point
        if (static_cast<int>(i) == last_triggered_point)
        {
            color = color.brighter(0.5f);
            hover_scale = 1.2f;
        }
        
        // Make sure color is bright enough
        float brightness = color.getBrightness();
        if (brightness < 0.3f)
        {
            color = color.brighter(0.7f);
        }
        
        // Draw point - ensure minimum size
        float draw_size = juce::jmax(6.0f, point_size * hover_scale);
        g.setColour(color);
        g.fillEllipse(x - draw_size / 2.0f, y - draw_size / 2.0f, draw_size, draw_size);
        
        // Draw outline for visibility
        g.setColour(color.brighter(0.2f).withAlpha(0.8f));
        g.drawEllipse(x - draw_size / 2.0f - 1.0f, y - draw_size / 2.0f - 1.0f, 
                     draw_size + 2.0f, draw_size + 2.0f, 1.5f);
        
        // Draw outline for hovered point
        if (static_cast<int>(i) == hovered_point)
        {
            g.setColour(juce::Colours::white);
            g.drawEllipse(x - draw_size / 2.0f - 3.0f, y - draw_size / 2.0f - 3.0f, 
                         draw_size + 6.0f, draw_size + 6.0f, 2.0f);
        }
        
        drawn_count++;
    }
    
    // Debug: Show stats
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);
    g.drawText("Points: " + juce::String(points.size()) + " (drawn: " + juce::String(drawn_count) + 
               ", visible: " + juce::String(visible_count) + ")", 
               10, 10, 300, 20, juce::Justification::left);
    g.drawText("Zoom: " + juce::String(zoom_level, 2) + 
               " Pan: (" + juce::String(pan_offset.x, 2) + ", " + juce::String(pan_offset.y, 2) + ")", 
               10, 30, 300, 20, juce::Justification::left);
    
    // Draw hovered audio file name
    if (hovered_point >= 0 && hovered_point < static_cast<int>(points.size()))
    {
        const auto& point = points[hovered_point];
        float normalized_x = static_cast<float>(point.position.x) + pan_offset.x;
        float normalized_y = static_cast<float>(point.position.y) + pan_offset.y;
        float centered_x = (normalized_x - 0.5f) * zoom_level + 0.5f;
        float centered_y = (normalized_y - 0.5f) * zoom_level + 0.5f;
        float x = centered_x * width;
        float y = centered_y * height;
        
        juce::String filename = point.audio_file.getFileName();
        juce::Font font(juce::FontOptions().withHeight(12.0f));
        g.setFont(font);
        
        // Estimate text bounds (approximate width based on character count)
        float font_height = font.getHeight();
        float estimated_width = filename.length() * font_height * 0.6f;  // Approximate character width
        int padding = 6;
        
        juce::Rectangle<float> text_bounds(
            x - estimated_width / 2.0f - padding,
            y - point_size - 15.0f - font_height / 2.0f - padding,
            estimated_width + padding * 2.0f,
            font_height + padding * 2.0f
        );
        
        // Draw semi-transparent background
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(text_bounds, 3.0f);
        
        // Draw text
        g.setColour(juce::Colours::white);
        g.drawText(filename, text_bounds.toNearestInt(), juce::Justification::centred);
    }
}

void EmbeddingSpaceView::resized()
{
    // Reset zoom and pan when resized (points are already normalized to 0-1)
    zoom_level = 1.0f;
    pan_offset = juce::Point<float>(0.0f, 0.0f);
}

void EmbeddingSpaceView::mouseDown(const juce::MouseEvent& e)
{
    // Check if space bar is pressed for panning
    if (e.mods.isShiftDown() || juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey))
    {
        is_panning = true;
        is_dragging = false;
    }
    else
    {
        is_dragging = true;
        is_panning = false;
    }
    last_mouse_pos = e.getPosition();
    last_triggered_point = -1;
}

void EmbeddingSpaceView::mouseDrag(const juce::MouseEvent& e)
{
    auto current_pos = e.getPosition();
    
    if (is_panning)
    {
        // Pan the view (in normalized space)
        auto delta = current_pos - last_mouse_pos;
        pan_offset.x += delta.x / getWidth();
        pan_offset.y += delta.y / getHeight();
        repaint();
    }
    else if (is_dragging && !points.empty())
    {
        // Convert to embedding space
        auto embedding_pos = screen_to_embedding_space(current_pos);
        
        // Find nearest point
        float threshold = trigger_threshold;
        int nearest = find_nearest_point(embedding_pos, threshold);
        
        if (nearest >= 0 && nearest != last_triggered_point)
        {
            last_triggered_point = nearest;
            
            // Calculate velocity based on distance (closer = higher velocity)
            float distance = static_cast<float>(embedding_pos.getDistanceFrom(points[nearest].position));
            float velocity = juce::jmax(0.1f, 1.0f - (distance / threshold));
            
            // Trigger sample
            if (sample_trigger_callback)
            {
                sample_trigger_callback(nearest, velocity);
            }
        }
    }
    
    last_mouse_pos = current_pos;
}

void EmbeddingSpaceView::mouseUp(const juce::MouseEvent& e)
{
    is_dragging = false;
    is_panning = false;
    last_triggered_point = -1;
    repaint();
}

void EmbeddingSpaceView::mouseMove(const juce::MouseEvent& e)
{
    if (points.empty())
        return;
    
    auto current_pos = e.getPosition();
    auto embedding_pos = screen_to_embedding_space(current_pos);
    
    // Find nearest point for hover
    float hover_threshold = trigger_threshold * 2.0f;  // Larger threshold for hover
    int nearest = find_nearest_point(embedding_pos, hover_threshold);
    
    if (nearest != hovered_point)
    {
        hovered_point = nearest;
        hover_animation_time = 0.0f;
        repaint();
    }
}

bool EmbeddingSpaceView::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        // Space bar pressed - will enable panning on mouse drag
        return true;
    }
    return false;
}

bool EmbeddingSpaceView::keyStateChanged(bool isKeyDown)
{
    // Update panning state based on space bar
    if (!isKeyDown && is_panning)
    {
        is_panning = false;
        repaint();
    }
    return false;
}

void EmbeddingSpaceView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Zoom in/out at mouse position (like audiostellar)
    float zoom_factor = 1.0f + wheel.deltaY * 0.1f;
    float old_zoom = zoom_level;
    zoom_level = juce::jlimit(0.1f, 10.0f, zoom_level * zoom_factor);
    
    // Adjust pan to zoom at mouse position
    auto mouse_pos = e.getPosition().toFloat();
    auto bounds = getLocalBounds().toFloat();
    float mouse_x_norm = mouse_pos.x / bounds.getWidth();
    float mouse_y_norm = mouse_pos.y / bounds.getHeight();
    
    // Calculate the point in normalized space before zoom
    float old_centered_x = mouse_x_norm;
    float old_centered_y = mouse_y_norm;
    float old_normalized_x = (old_centered_x - 0.5f) / old_zoom + 0.5f - pan_offset.x;
    float old_normalized_y = (old_centered_y - 0.5f) / old_zoom + 0.5f - pan_offset.y;
    
    // Calculate where it should be after zoom (same normalized position)
    float new_centered_x = (old_normalized_x + pan_offset.x - 0.5f) * zoom_level + 0.5f;
    float new_centered_y = (old_normalized_y + pan_offset.y - 0.5f) * zoom_level + 0.5f;
    
    // Adjust pan to keep mouse position fixed
    float delta_x = (old_centered_x - new_centered_x) / bounds.getWidth();
    float delta_y = (old_centered_y - new_centered_y) / bounds.getHeight();
    pan_offset.x += delta_x;
    pan_offset.y += delta_y;
    
    repaint();
}

bool EmbeddingSpaceView::load_palette(const juce::File& palette_dir)
{
    if (!palette_dir.exists() || !palette_dir.isDirectory())
    {
        DBG("EmbeddingSpaceView: Invalid palette directory: " + palette_dir.getFullPathName());
        return false;
    }
    
    // Load metadata
    juce::File metadata_file = palette_dir.getChildFile("metadata.json");
    if (!metadata_file.existsAsFile())
    {
        DBG("EmbeddingSpaceView: Metadata file not found");
        return false;
    }
    
    juce::var metadata = juce::JSON::parse(metadata_file);
    if (!metadata.isObject())
    {
        DBG("EmbeddingSpaceView: Failed to parse metadata");
        return false;
    }
    
    // Load chunk files first
    if (!metadata.hasProperty("chunks"))
    {
        DBG("EmbeddingSpaceView: No chunks in metadata");
        return false;
    }
    
    auto chunks_var = metadata["chunks"];
    if (!chunks_var.isArray())
    {
        DBG("EmbeddingSpaceView: Chunks is not an array");
        return false;
    }
    
    chunk_files.clear();
    for (int i = 0; i < chunks_var.size(); ++i)
    {
        auto chunk_var = chunks_var[i];
        if (chunk_var.isObject() && chunk_var.hasProperty("path"))
        {
            juce::String path = chunk_var["path"];
            juce::File chunk_file = palette_dir.getChildFile(path);
            chunk_files.push_back(chunk_file);
        }
    }
    
    if (chunk_files.empty())
    {
        DBG("EmbeddingSpaceView: No valid chunk files found");
        return false;
    }
    
    // Load t-SNE coordinates and cluster assignments
    std::vector<juce::Point<double>> tsne_coordinates;
    std::vector<int> cluster_assignments;
    
    bool has_visualization = PaletteVisualization::load_palette_visualization(palette_dir, tsne_coordinates, cluster_assignments);
    
    if (!has_visualization)
    {
        DBG("EmbeddingSpaceView: Visualization data not found.");
        
        // Optional: Compute t-SNE on-demand if embeddings exist
        // For now, use grid layout as fallback for faster loading
        // To enable on-demand computation, uncomment the following:
        /*
        juce::File embeddings_file = palette_dir.getChildFile("embeddings.bin");
        if (embeddings_file.existsAsFile())
        {
            DBG("EmbeddingSpaceView: Computing t-SNE from embeddings on-demand...");
            if (PaletteVisualization::compute_tsne_from_embeddings(palette_dir, nullptr))
            {
                // Reload visualization data
                has_visualization = PaletteVisualization::load_palette_visualization(palette_dir, tsne_coordinates, cluster_assignments);
            }
        }
        */
        
        if (!has_visualization)
        {
            DBG("EmbeddingSpaceView: Creating fallback grid layout.");
            
            // Create a simple grid layout as fallback
            int num_chunks = static_cast<int>(chunk_files.size());
            int grid_size = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(num_chunks))));
            
            tsne_coordinates.clear();
            cluster_assignments.clear();
            
            for (int i = 0; i < num_chunks; ++i)
            {
                int row = i / grid_size;
                int col = i % grid_size;
                double x = static_cast<double>(col) / static_cast<double>(grid_size);
                double y = static_cast<double>(row) / static_cast<double>(grid_size);
                tsne_coordinates.push_back(juce::Point<double>(x, y));
                cluster_assignments.push_back(0); // All in same cluster for grid
            }
        }
    }
    
    // Validate sizes match
    if (chunk_files.size() != tsne_coordinates.size() || chunk_files.size() != cluster_assignments.size())
    {
        DBG("EmbeddingSpaceView: Size mismatch between coordinates (" + juce::String(tsne_coordinates.size()) + 
            "), clusters (" + juce::String(cluster_assignments.size()) + 
            "), and chunks (" + juce::String(chunk_files.size()) + ")");
        return false;
    }
    
    // Normalize coordinates
    normalize_coordinates(tsne_coordinates);
    
    // Create points
    points.clear();
    for (size_t i = 0; i < tsne_coordinates.size(); ++i)
    {
        EmbeddingPoint point;
        point.position = tsne_coordinates[i];
        point.cluster_id = cluster_assignments[i];
        point.chunk_index = static_cast<int>(i);
        if (i < chunk_files.size())
        {
            point.audio_file = chunk_files[static_cast<int>(i)];
        }
        points.push_back(point);
    }
    
    // Reset zoom and pan when loading new palette
    zoom_level = 1.0f;
    pan_offset = juce::Point<float>(0.0f, 0.0f);
    
    DBG("EmbeddingSpaceView: Loaded " + juce::String(points.size()) + " points");
    
    if (!points.empty())
    {
        DBG("EmbeddingSpaceView: First point position: (" + 
            juce::String(points[0].position.x, 4) + ", " + 
            juce::String(points[0].position.y, 4) + "), cluster: " + 
            juce::String(points[0].cluster_id) + 
            ", color: " + color_palette.get_color(points[0].cluster_id).toString());
        DBG("EmbeddingSpaceView: Last point position: (" + 
            juce::String(points.back().position.x, 4) + ", " + 
            juce::String(points.back().position.y, 4) + "), cluster: " + 
            juce::String(points.back().cluster_id) +
            ", color: " + color_palette.get_color(points.back().cluster_id).toString());
        
        // Log cluster ID distribution
        std::map<int, int> cluster_counts;
        for (const auto& point : points)
        {
            cluster_counts[point.cluster_id]++;
        }
        DBG("EmbeddingSpaceView: Cluster distribution:");
        for (const auto& pair : cluster_counts)
        {
            DBG("  Cluster " + juce::String(pair.first) + ": " + juce::String(pair.second) + " points");
        }
    }
    
    // Auto-fit to points
    resized();
    repaint();
    
    return true;
}

void EmbeddingSpaceView::set_sample_trigger_callback(std::function<void(int, float)> callback)
{
    sample_trigger_callback = callback;
}

void EmbeddingSpaceView::normalize_coordinates(std::vector<juce::Point<double>>& coordinates)
{
    if (coordinates.empty())
        return;
    
    // Find min/max
    double min_x = coordinates[0].x;
    double max_x = coordinates[0].x;
    double min_y = coordinates[0].y;
    double max_y = coordinates[0].y;
    
    for (const auto& coord : coordinates)
    {
        min_x = juce::jmin(min_x, coord.x);
        max_x = juce::jmax(max_x, coord.x);
        min_y = juce::jmin(min_y, coord.y);
        max_y = juce::jmax(max_y, coord.y);
    }
    
    DBG("EmbeddingSpaceView: Coordinate range - X: [" + juce::String(min_x, 4) + ", " + juce::String(max_x, 4) + 
        "], Y: [" + juce::String(min_y, 4) + ", " + juce::String(max_y, 4) + "]");
    
    // Normalize to 0-1 range
    double range_x = max_x - min_x;
    double range_y = max_y - min_y;
    
    if (range_x > 0 && range_y > 0)
    {
        for (auto& coord : coordinates)
        {
            coord.x = (coord.x - min_x) / range_x;
            coord.y = (coord.y - min_y) / range_y;
        }
        DBG("EmbeddingSpaceView: Normalized coordinates to [0, 1] range");
    }
    else
    {
        DBG("EmbeddingSpaceView: WARNING - Zero or negative range! range_x=" + juce::String(range_x) + 
            ", range_y=" + juce::String(range_y));
        // If range is zero, set all coordinates to center
        for (auto& coord : coordinates)
        {
            coord.x = 0.5;
            coord.y = 0.5;
        }
    }
}

juce::Point<double> EmbeddingSpaceView::screen_to_embedding_space(const juce::Point<int>& screen_pos) const
{
    auto bounds = getLocalBounds().toFloat();
    float width = bounds.getWidth();
    float height = bounds.getHeight();
    
    // Convert screen coordinates to normalized space (accounting for zoom and pan)
    // Inverse of: centered_x = (normalized_x - 0.5) * zoom_level + 0.5, then x = centered_x * width
    float centered_x = screen_pos.x / width;
    float centered_y = screen_pos.y / height;
    
    // Inverse zoom (centered around 0.5)
    float normalized_x = (centered_x - 0.5f) / zoom_level + 0.5f;
    float normalized_y = (centered_y - 0.5f) / zoom_level + 0.5f;
    
    // Remove pan offset
    normalized_x -= pan_offset.x;
    normalized_y -= pan_offset.y;
    
    return juce::Point<double>(normalized_x, normalized_y);
}

int EmbeddingSpaceView::find_nearest_point(const juce::Point<double>& pos, float max_distance) const
{
    int nearest = -1;
    double min_distance = max_distance;
    
    for (size_t i = 0; i < points.size(); ++i)
    {
        double distance = pos.getDistanceFrom(points[i].position);
        if (distance < min_distance)
        {
            min_distance = distance;
            nearest = static_cast<int>(i);
        }
    }
    
    return nearest;
}

juce::Rectangle<double> EmbeddingSpaceView::get_points_bounds() const
{
    if (points.empty())
        return juce::Rectangle<double>();
    
    double min_x = points[0].position.x;
    double max_x = points[0].position.x;
    double min_y = points[0].position.y;
    double max_y = points[0].position.y;
    
    for (const auto& point : points)
    {
        min_x = juce::jmin(min_x, point.position.x);
        max_x = juce::jmax(max_x, point.position.x);
        min_y = juce::jmin(min_y, point.position.y);
        max_y = juce::jmax(max_y, point.position.y);
    }
    
    return juce::Rectangle<double>(min_x, min_y, max_x - min_x, max_y - min_y);
}

void EmbeddingSpaceView::timerCallback()
{
    // Update hover animation
    bool needs_repaint = false;
    if (hovered_point >= 0)
    {
        hover_animation_time += 0.033f; // ~30 FPS
        if (hover_animation_time < hover_animation_duration)
        {
            needs_repaint = true;
        }
    }
    
    if (needs_repaint)
    {
        repaint();
    }
}

void EmbeddingSpaceView::recompute_clusters(double eps, int min_pts)
{
    if (points.empty())
        return;
    
    // Extract coordinates
    std::vector<juce::Point<double>> coordinates;
    coordinates.reserve(points.size());
    for (const auto& point : points)
    {
        coordinates.push_back(point.position);
    }
    
    // Compute new clusters
    std::vector<int> cluster_assignments = PaletteVisualization::compute_clusters(coordinates, eps, min_pts);
    
    // Update point cluster assignments
    if (cluster_assignments.size() == points.size())
    {
        for (size_t i = 0; i < points.size(); ++i)
        {
            points[i].cluster_id = cluster_assignments[i];
        }
        repaint();
    }
}

juce::File EmbeddingSpaceView::get_audio_file(int chunk_index) const
{
    if (chunk_index >= 0 && chunk_index < static_cast<int>(points.size()))
    {
        return points[chunk_index].audio_file;
    }
    return juce::File();
}

} // namespace EmbeddingSpaceSampler

