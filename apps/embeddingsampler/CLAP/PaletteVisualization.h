#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace EmbeddingSpaceSampler
{
    class DBScan;
}

namespace EmbeddingSpaceSampler
{

// Helper class to compute and store t-SNE coordinates and cluster assignments
class PaletteVisualization
{
public:
    // Update palette metadata with t-SNE coordinates and cluster assignments
    // Coordinates: 2D points for each chunk [[x1,y1], [x2,y2], ...]
    // Cluster assignments: cluster ID for each chunk [cluster0, cluster1, ...]
    static bool update_palette_visualization(
        const juce::File& palette_dir,
        const std::vector<juce::Point<double>>& tsne_coordinates,
        const std::vector<int>& cluster_assignments
    );
    
    // Load t-SNE coordinates and cluster assignments from palette metadata
    static bool load_palette_visualization(
        const juce::File& palette_dir,
        std::vector<juce::Point<double>>& tsne_coordinates,
        std::vector<int>& cluster_assignments
    );
    
    // Compute clusters using DBScan from t-SNE coordinates
    // Returns cluster assignments (cluster ID for each point, -2 for noise)
    static std::vector<int> compute_clusters(
        const std::vector<juce::Point<double>>& coordinates,
        double eps = 0.1,
        int min_pts = 5
    );
    
    // Compute t-SNE coordinates from embeddings stored in palette
    // Loads embeddings from embeddings.bin, runs t-SNE, computes clusters, and saves results
    // Returns true on success, false on failure
    static bool compute_tsne_from_embeddings(
        const juce::File& palette_dir,
        std::function<void(const juce::String&)> progressCallback = nullptr
    );
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PaletteVisualization)
};

} // namespace EmbeddingSpaceSampler

