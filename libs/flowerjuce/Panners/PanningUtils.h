#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <utility>

namespace PanningUtils
{
    // Cosine panning law wavetable
    // Maps pan values (0-1) to cosine values for panning calculations
    class CosinePanningLaw
    {
    public:
        CosinePanningLaw();
        
        // Get cosine value for a given angle (0 to π/2)
        float get_cosine(float angle) const;
        
        // Get sine value for a given angle (0 to π/2)
        float get_sine(float angle) const;
        
    private:
        static constexpr size_t m_num_points = 1024;
        juce::dsp::LookupTableTransform<float> m_cosine_table;
        juce::dsp::LookupTableTransform<float> m_sine_table;
    };

    // Get the shared cosine panning law instance
    const CosinePanningLaw& get_cosine_panning_law();

    // Compute stereo panning gains for a mono signal
    // pan: 0.0 = all left, 0.5 = center, 1.0 = all right
    // Returns: pair of gains (left, right)
    std::pair<float, float> compute_stereo_gains(float pan);

    // Compute quad panning gains for a mono signal
    // x: 0.0 = left, 1.0 = right
    // y: 0.0 = back, 1.0 = front
    // Returns: array of 4 gains [FL, FR, BL, BR]
    std::array<float, 4> compute_quad_gains(float x, float y);

    // Compute CLEAT panning gains for a mono signal
    // x: 0.0 = left, 1.0 = right
    // y: 0.0 = bottom, 1.0 = top
    // Returns: array of 16 gains (row-major: channels 0-3 = bottom row left-to-right)
    std::array<float, 16> compute_cleat_gains(float x, float y);
    
    // Path generation functions for panner trajectories
    // All functions generate points in normalized 0-1 space (x, y)
    // Returns vector of (x, y) pairs where x and y are in 0-1 range
    
    // Generate circular path
    std::vector<std::pair<float, float>> generate_circle_path(int num_points = 0);
    
    // Generate random path
    std::vector<std::pair<float, float>> generate_random_path(int num_points = 0);
    
    // Generate wander path (Brownian motion)
    std::vector<std::pair<float, float>> generate_wander_path(int num_points = 0);
    
    // Generate swirls path (multiple overlapping circular motions)
    std::vector<std::pair<float, float>> generate_swirls_path(int num_points = 0);
    
    // Generate bounce path (two points in different quadrants)
    std::vector<std::pair<float, float>> generate_bounce_path();
    
    // Generate spiral path (from center outward)
    std::vector<std::pair<float, float>> generate_spiral_path(int num_points = 0);
}

