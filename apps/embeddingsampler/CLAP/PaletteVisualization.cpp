#include "PaletteVisualization.h"
#include "../DBScan.h"
#include <juce_data_structures/juce_data_structures.h>
#include <algorithm>
#include <qdtsne/qdtsne.hpp>
#include <knncolle/knncolle.hpp>
#include <functional>
#include <cmath>

namespace EmbeddingSpaceSampler
{

bool PaletteVisualization::update_palette_visualization(
    const juce::File& palette_dir,
    const std::vector<juce::Point<double>>& tsne_coordinates,
    const std::vector<int>& cluster_assignments)
{
    juce::File metadata_file = palette_dir.getChildFile("metadata.json");
    if (!metadata_file.existsAsFile())
    {
        DBG("PaletteVisualization: Metadata file not found: " + metadata_file.getFullPathName());
        return false;
    }
    
    juce::var metadata = juce::JSON::parse(metadata_file);
    if (!metadata.isObject())
    {
        DBG("PaletteVisualization: Failed to parse metadata JSON");
        return false;
    }
    
    // Validate sizes
    int num_chunks = metadata.getProperty("numChunks", 0);
    if (static_cast<int>(tsne_coordinates.size()) != num_chunks ||
        static_cast<int>(cluster_assignments.size()) != num_chunks)
    {
        DBG("PaletteVisualization: Size mismatch. Expected " + juce::String(num_chunks) + 
            ", got coordinates: " + juce::String(tsne_coordinates.size()) + 
            ", clusters: " + juce::String(cluster_assignments.size()));
        return false;
    }
    
    // Store t-SNE coordinates
    juce::Array<juce::var> coordinates_array;
    for (const auto& coord : tsne_coordinates)
    {
        juce::Array<juce::var> point_array;
        point_array.add(coord.x);
        point_array.add(coord.y);
        coordinates_array.add(juce::var(point_array));
    }
    metadata.getDynamicObject()->setProperty("tsneCoordinates", juce::var(coordinates_array));
    
    // Store cluster assignments
    juce::Array<juce::var> clusters_array;
    for (int cluster_id : cluster_assignments)
    {
        clusters_array.add(cluster_id);
    }
    metadata.getDynamicObject()->setProperty("clusterAssignments", juce::var(clusters_array));
    
    // Write updated metadata
    metadata_file.replaceWithText(juce::JSON::toString(metadata));
    
    DBG("PaletteVisualization: Updated palette visualization data");
    return true;
}

bool PaletteVisualization::load_palette_visualization(
    const juce::File& palette_dir,
    std::vector<juce::Point<double>>& tsne_coordinates,
    std::vector<int>& cluster_assignments)
{
    juce::File metadata_file = palette_dir.getChildFile("metadata.json");
    if (!metadata_file.existsAsFile())
    {
        return false;
    }
    
    juce::var metadata = juce::JSON::parse(metadata_file);
    if (!metadata.isObject())
    {
        return false;
    }
    
    // Load t-SNE coordinates
    if (metadata.hasProperty("tsneCoordinates"))
    {
        auto coords_var = metadata["tsneCoordinates"];
        if (coords_var.isArray())
        {
            tsne_coordinates.clear();
            for (int i = 0; i < coords_var.size(); ++i)
            {
                auto point_var = coords_var[i];
                if (point_var.isArray() && point_var.size() >= 2)
                {
                    double x = point_var[0];
                    double y = point_var[1];
                    tsne_coordinates.push_back(juce::Point<double>(x, y));
                }
            }
        }
    }
    
    // Load cluster assignments
    if (metadata.hasProperty("clusterAssignments"))
    {
        auto clusters_var = metadata["clusterAssignments"];
        if (clusters_var.isArray())
        {
            cluster_assignments.clear();
            for (int i = 0; i < clusters_var.size(); ++i)
            {
                cluster_assignments.push_back(static_cast<int>(clusters_var[i]));
            }
        }
    }
    
    return !tsne_coordinates.empty() && !cluster_assignments.empty();
}

std::vector<int> PaletteVisualization::compute_clusters(
    const std::vector<juce::Point<double>>& coordinates,
    double eps,
    int min_pts)
{
    if (coordinates.empty())
    {
        return {};
    }
    
    // Run DBScan
    DBScan dbscan(eps, min_pts, coordinates);
    dbscan.run();
    
    // Get cluster assignments
    std::vector<int> cluster_assignments(coordinates.size());
    for (size_t i = 0; i < coordinates.size(); ++i)
    {
        cluster_assignments[i] = dbscan.get_cluster_id(static_cast<int>(i));
    }
    
    return cluster_assignments;
}

bool PaletteVisualization::compute_tsne_from_embeddings(
    const juce::File& palette_dir,
    std::function<void(const juce::String&)> progressCallback)
{
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Starting");
    
    if (progressCallback)
        progressCallback("Loading embeddings...");
    
    // Load embeddings from embeddings.bin
    juce::File embeddings_file = palette_dir.getChildFile("embeddings.bin");
    if (!embeddings_file.existsAsFile())
    {
        DBG("PaletteVisualization::compute_tsne_from_embeddings: Embeddings file not found: " + embeddings_file.getFullPathName());
        return false;
    }
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Opening embeddings file: " + embeddings_file.getFullPathName());
    
    juce::FileInputStream input_stream(embeddings_file);
    if (!input_stream.openedOk())
    {
        DBG("PaletteVisualization::compute_tsne_from_embeddings: Failed to open embeddings file");
        return false;
    }
    
    // Read header
    int32_t num_embeddings = 0;
    int32_t embedding_size = 0;
    
    input_stream.read(&num_embeddings, sizeof(int32_t));
    input_stream.read(&embedding_size, sizeof(int32_t));
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Read header - num_embeddings=" + juce::String(num_embeddings) + 
        ", embedding_size=" + juce::String(embedding_size));
    
    if (num_embeddings <= 0 || embedding_size <= 0)
    {
        DBG("PaletteVisualization::compute_tsne_from_embeddings: Invalid embeddings file (num_embeddings=" + juce::String(num_embeddings) + 
            ", embedding_size=" + juce::String(embedding_size) + ")");
        return false;
    }
    
    // Read all embeddings (row-major: each embedding is a row)
    std::vector<std::vector<float>> embeddings;
    embeddings.reserve(num_embeddings);
    
    for (int32_t i = 0; i < num_embeddings; ++i)
    {
        std::vector<float> embedding(embedding_size);
        input_stream.read(embedding.data(), sizeof(float) * embedding_size);
        embeddings.push_back(embedding);
        
        if (progressCallback && i % 100 == 0)
        {
            progressCallback("Loading embeddings... " + juce::String(i) + "/" + juce::String(num_embeddings));
        }
    }
    
    if (static_cast<int32_t>(embeddings.size()) != num_embeddings)
    {
        DBG("PaletteVisualization::compute_tsne_from_embeddings: Failed to read all embeddings (read " + 
            juce::String(embeddings.size()) + ", expected " + juce::String(num_embeddings) + ")");
        return false;
    }
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Successfully loaded " + juce::String(embeddings.size()) + " embeddings");
    
    if (progressCallback)
        progressCallback("Converting embeddings to column-major format...");
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Converting to column-major format...");
    
    // Convert from row-major to column-major format for qdtsne
    // qdtsne expects: data_dim rows (embedding dimensions), num_obs columns (observations)
    // So we transpose: embedding_size rows, num_embeddings columns
    std::vector<double> column_major_data;
    column_major_data.reserve(embedding_size * num_embeddings);
    
    for (int32_t dim = 0; dim < embedding_size; ++dim)
    {
        for (int32_t obs = 0; obs < num_embeddings; ++obs)
        {
            column_major_data.push_back(static_cast<double>(embeddings[obs][dim]));
        }
        
        if (dim % 100 == 0 && progressCallback)
        {
            progressCallback("Converting embeddings... " + juce::String(dim) + "/" + juce::String(embedding_size));
        }
    }
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Converted to column-major format. Data size: " + 
        juce::String(column_major_data.size()));
    
    if (progressCallback)
        progressCallback("Building neighbor search index...");
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Building neighbor search index...");
    
    // Configure neighbor search algorithm (VP tree with Euclidean distance)
    knncolle::VptreeBuilder<int, double, double> nnalg(
        std::make_shared<knncolle::EuclideanDistance<double, double>>()
    );
    
    // Configure t-SNE options
    qdtsne::Options opt;
    opt.perplexity = 30.0; // Default perplexity
    opt.max_iterations = 1000; // Default iterations
    opt.max_depth = 7; // Good balance between speed and accuracy
    opt.leaf_approximation = true; // Enable leaf approximation for speed
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: t-SNE options - perplexity=" + juce::String(opt.perplexity) + 
        ", max_iterations=" + juce::String(opt.max_iterations) + ", max_depth=" + juce::String(opt.max_depth));
    
    if (progressCallback)
        progressCallback("Running t-SNE algorithm...");
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Initializing t-SNE (embedding_size=" + juce::String(embedding_size) + 
        ", num_embeddings=" + juce::String(num_embeddings) + ")");
    
    // Initialize t-SNE (data_dim=embedding_size, num_obs=num_embeddings)
    auto status = qdtsne::initialize<2>(
        static_cast<std::size_t>(embedding_size),
        static_cast<int>(num_embeddings),
        column_major_data.data(),
        nnalg,
        opt
    );
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: t-SNE initialized, initializing random coordinates...");
    
    // Initialize random 2D coordinates
    auto Y = qdtsne::initialize_random<2>(static_cast<int>(num_embeddings));
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Starting t-SNE iterations (this may take a while)...");
    
    // Run t-SNE iterations
    status.run(Y.data());
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: t-SNE iterations completed");
    
    if (progressCallback)
        progressCallback("Converting t-SNE results...");
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Converting t-SNE results to coordinates...");
    
    // Convert results to juce::Point<double>
    std::vector<juce::Point<double>> tsne_coordinates;
    tsne_coordinates.reserve(num_embeddings);
    
    for (int32_t i = 0; i < num_embeddings; ++i)
    {
        // Y is column-major: [x0, y0, x1, y1, ...] for 2D
        double x = Y[i * 2];
        double y = Y[i * 2 + 1];
        tsne_coordinates.push_back(juce::Point<double>(x, y));
    }
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Converted " + juce::String(tsne_coordinates.size()) + " coordinates");
    
    if (progressCallback)
        progressCallback("Computing clusters...");
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Computing clusters...");
    
    // Compute clusters using DBScan
    std::vector<int> cluster_assignments = compute_clusters(tsne_coordinates);
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Computed " + juce::String(cluster_assignments.size()) + " cluster assignments");
    
    if (progressCallback)
        progressCallback("Saving visualization data...");
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Saving visualization data...");
    
    // Save results
    bool success = update_palette_visualization(palette_dir, tsne_coordinates, cluster_assignments);
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Save result: " + juce::String(success ? "success" : "failed"));
    
    if (success && progressCallback)
        progressCallback("t-SNE computation complete!");
    
    DBG("PaletteVisualization::compute_tsne_from_embeddings: Completed successfully");
    
    return success;
}

} // namespace EmbeddingSpaceSampler

