#include "PanningUtils.h"
#include <cmath>
#include <random>
#include <algorithm>

namespace PanningUtils
{
    //==============================================================================
    CosinePanningLaw::CosinePanningLaw()
    {
        // Initialize cosine table: maps angle (0 to π/2) to cos(angle)
        m_cosine_table.initialise(
            [] (float angle) { return std::cos(angle); },
            0.0f,
            juce::MathConstants<float>::halfPi,
            m_num_points
        );
        
        // Initialize sine table: maps angle (0 to π/2) to sin(angle)
        m_sine_table.initialise(
            [] (float angle) { return std::sin(angle); },
            0.0f,
            juce::MathConstants<float>::halfPi,
            m_num_points
        );
    }

    float CosinePanningLaw::get_cosine(float angle) const
    {
        angle = juce::jlimit(0.0f, juce::MathConstants<float>::halfPi, angle);
        return m_cosine_table.processSampleUnchecked(angle);
    }

    float CosinePanningLaw::get_sine(float angle) const
    {
        angle = juce::jlimit(0.0f, juce::MathConstants<float>::halfPi, angle);
        return m_sine_table.processSampleUnchecked(angle);
    }

    //==============================================================================
    // Singleton instance
    static CosinePanningLaw g_cosine_panning_law;

    const CosinePanningLaw& get_cosine_panning_law()
    {
        return g_cosine_panning_law;
    }

    //==============================================================================
    std::pair<float, float> compute_stereo_gains(float pan)
    {
        pan = juce::jlimit(0.0f, 1.0f, pan);
        
        // Map pan (0-1) to angle (0 to π/2)
        float angle = pan * juce::MathConstants<float>::halfPi;
        
        const auto& law = get_cosine_panning_law();
        float left = law.get_cosine(angle);
        float right = law.get_sine(angle);
        
        return {left, right};
    }

    //==============================================================================
    std::array<float, 4> compute_quad_gains(float x, float y)
    {
        x = juce::jlimit(0.0f, 1.0f, x);
        y = juce::jlimit(0.0f, 1.0f, y);
        
        // Speaker positions in normalized 0-1 space:
        // FL: (0, 1) - Front Left
        // FR: (1, 1) - Front Right
        // BL: (0, 0) - Back Left
        // BR: (1, 0) - Back Right
        
        // Calculate distances from pan position to each speaker
        float dx_fl = x - 0.0f;  // distance to left edge
        float dx_fr = x - 1.0f;  // distance to right edge
        float dy_f = y - 1.0f;   // distance to front edge
        float dy_b = y - 0.0f;   // distance to back edge
        
        // Normalize distances to 0-1 range (max distance is diagonal = √2)
        float max_dist = std::sqrt(2.0f);
        float dist_fl = std::sqrt(dx_fl * dx_fl + dy_f * dy_f) / max_dist;
        float dist_fr = std::sqrt(dx_fr * dx_fr + dy_f * dy_f) / max_dist;
        float dist_bl = std::sqrt(dx_fl * dx_fl + dy_b * dy_b) / max_dist;
        float dist_br = std::sqrt(dx_fr * dx_fr + dy_b * dy_b) / max_dist;
        
        // Convert distances to angles (closer = higher gain)
        // Use cosine law: gain = cos(angle), where angle is proportional to distance
        const auto& law = get_cosine_panning_law();
        float angle_fl = dist_fl * juce::MathConstants<float>::halfPi;
        float angle_fr = dist_fr * juce::MathConstants<float>::halfPi;
        float angle_bl = dist_bl * juce::MathConstants<float>::halfPi;
        float angle_br = dist_br * juce::MathConstants<float>::halfPi;
        
        float gain_fl = law.get_cosine(angle_fl);
        float gain_fr = law.get_cosine(angle_fr);
        float gain_bl = law.get_cosine(angle_bl);
        float gain_br = law.get_cosine(angle_br);
        
        // Normalize gains to preserve energy
        float sum = gain_fl + gain_fr + gain_bl + gain_br;
        if (sum > 0.0f)
        {
            float norm = 1.0f / sum;
            gain_fl *= norm;
            gain_fr *= norm;
            gain_bl *= norm;
            gain_br *= norm;
        }
        
        return {gain_fl, gain_fr, gain_bl, gain_br};
    }

    //==============================================================================
    std::array<float, 16> compute_cleat_gains(float x, float y, float gain_power)
    {
        x = juce::jlimit(0.0f, 1.0f, x);
        y = juce::jlimit(0.0f, 1.0f, y);
        
        // CLEAT speaker grid: 4x4, row-major ordering
        // Channels 0-3: bottom row (left to right)
        // Channels 4-7: second row (left to right)
        // Channels 8-11: third row (left to right)
        // Channels 12-15: top row (left to right)

        // map x and y to the range (0.3) -> 1.0
        float scaled_x = juce::jmap(x, 0.0f, 1.0f, 0.275f, 1.0f);
        float scaled_y = juce::jmap(y, 0.0f, 1.0f, 0.275f, 1.0f);
        
        
        // Column offsets (left to right): -0.75, -0.5, -0.25, 0.0
        constexpr float column_offsets[4] = {-0.75f, -0.5f, -0.25f, 0.0f};
        
        // Row offsets (bottom to top): -0.75, -0.5, -0.25, 0.0
        constexpr float row_offsets[4] = {-0.75f, -0.5f, -0.25f, 0.0f};
        
        std::array<float, 16> gains = {0.0f};
        
        // Compute gains using oscillator-based algorithm (matching Max/MSP patcher)
        for (int row = 0; row < 4; ++row)
        {
            // Compute y phase: scaled_y + row_offset, clipped to [-0.5, 0.5]
            float y_phase = juce::jlimit(-0.5f, 0.5f, scaled_y + row_offsets[row]);
            
            // Generate y oscillator: sin(2π * phase)
            float y_osc = std::sin(2.0f * juce::MathConstants<float>::pi * y_phase);
            
            // Apply gain formula: oscillator * 0.5 + 1.0
            float y_gain = (y_osc + 1.0f) * 0.5f;
            
            for (int col = 0; col < 4; ++col)
            {
                int channel = row * 4 + col;
                
                // Compute x phase: scaled_x + column_offset, clipped to [-0.5, 0.5]
                float x_phase = juce::jlimit(-0.5f, 0.5f, scaled_x + column_offsets[col]);
                
                // Generate x oscillator: sin(2π * phase)
                float x_osc = std::sin(2.0f * juce::MathConstants<float>::pi * x_phase);
                
                // Apply gain formula: oscillator * 0.5 + 1.0
                float x_gain = (x_osc + 1.0f) * 0.5f;
                
                // Multiply x and y gains (matching Max/MSP patcher behavior)
                gains[channel] = x_gain * y_gain;
            }
        }
        
        // Apply power function to increase gain differences while preserving relative proportions
        // Power factor > 1 increases contrast: higher gains become relatively higher
        // Only apply if gain_power != 1.0 (to avoid unnecessary computation when default)
        if (gain_power != 1.0f)
        {
            float sum = 0.0f;
            for (int i = 0; i < 16; ++i)
            {
                gains[i] = std::pow(gains[i], gain_power);
                sum += gains[i];
            }
            
            // // Normalize to preserve energy and relative proportions
            // if (sum > 0.0f)
            // {
            //     float norm = 1.0f / sum;
            //     for (int i = 0; i < 16; ++i)
            //     {
            //         gains[i] *= norm;
            //     }
            // }
        }
        
        return gains;
    }
    
    //==============================================================================
    // Path generation functions
    
    // Helper function to sample a random origin point within r=0.25 from center (0.5, 0.5)
    static std::pair<float, float> sample_random_origin()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        
        // Sample uniformly within a circle of radius 0.25 centered at (0.5, 0.5)
        // Use rejection sampling: sample in unit square, accept if within circle
        float x, y;
        do
        {
            x = dist(gen);
            y = dist(gen);
            // Check if point is within circle of radius 0.25 centered at (0.5, 0.5)
            float dx = x - 0.5f;
            float dy = y - 0.5f;
            float dist_sq = dx * dx + dy * dy;
            if (dist_sq <= 0.0625f) // r^2 = 0.25^2 = 0.0625
                break;
        } while (true);
        
        return {x, y};
    }
    
    std::vector<std::pair<float, float>> generate_circle_path(int num_points)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> point_dist(50, 100);
        std::uniform_int_distribution<int> dir_dist(0, 1);
        
        // Generate random number of points if not specified
        if (num_points <= 0)
            num_points = point_dist(gen);
        
        // Sample random origin within r=0.5 from center
        auto origin = sample_random_origin();
        float origin_x = origin.first;
        float origin_y = origin.second;
        
        // Random starting angle
        float angle = dist(gen) * 2.0f * juce::MathConstants<float>::pi;
        
        // Random direction (clockwise or counterclockwise)
        int direction = dir_dist(gen) == 0 ? 1 : -1;
        
        // Random radius (0.15 to 0.3, relative to origin)
        float radius = 0.15f + dist(gen) * 0.15f;
        
        float angle_step = (2.0f * juce::MathConstants<float>::pi / num_points) * direction;
        
        for (int i = 0; i < num_points; ++i)
        {
            float x = origin_x + radius * std::cos(angle);
            float y = origin_y + radius * std::sin(angle);
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
            angle += angle_step;
        }
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generate_random_path(int num_points)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> point_dist(200, 300);
        
        // Generate random number of points if not specified
        if (num_points <= 0)
            num_points = point_dist(gen);
        
        // Ensure minimum points
        if (num_points < 4)
            num_points = 4;
        
        // Sample random origin within r=0.5 from center
        auto origin = sample_random_origin();
        float origin_x = origin.first;
        float origin_y = origin.second;
        
        // Generate random points around the origin, ensuring they stay within bounds
        for (int i = 0; i < num_points; ++i)
        {
            // Generate offset from origin (smaller range to ensure bounds)
            float offset_x = (dist(gen) - 0.5f) * 0.8f; // Scale to keep within bounds
            float offset_y = (dist(gen) - 0.5f) * 0.8f;
            
            float x = origin_x + offset_x;
            float y = origin_y + offset_y;
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
        }
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generate_wander_path(int num_points)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_real_distribution<float> wander_dist(-0.1f, 0.1f);
        std::uniform_int_distribution<int> point_dist(50, 100);
        
        // Generate random number of points if not specified
        if (num_points <= 0)
            num_points = point_dist(gen);
        
        // Sample random origin within r=0.5 from center
        auto origin = sample_random_origin();
        float x = origin.first;
        float y = origin.second;
        
        for (int i = 0; i < num_points; ++i)
        {
            // Add random wander
            x += wander_dist(gen);
            y += wander_dist(gen);
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
        }
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generate_swirls_path(int num_points)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> point_dist(50, 100);
        
        // Generate random number of points if not specified
        if (num_points <= 0)
            num_points = point_dist(gen);
        
        // Sample random origin within r=0.5 from center
        auto origin = sample_random_origin();
        float origin_x = origin.first;
        float origin_y = origin.second;
        
        // Create multiple overlapping circular motions
        const int num_swirls = 2 + (gen() % 3); // 2-4 swirls
        std::vector<float> angles(num_swirls);
        std::vector<float> radii(num_swirls);
        std::vector<float> centers_x(num_swirls);
        std::vector<float> centers_y(num_swirls);
        std::vector<float> speeds(num_swirls);
        
        for (int s = 0; s < num_swirls; ++s)
        {
            angles[s] = dist(gen) * 2.0f * juce::MathConstants<float>::pi;
            radii[s] = 0.08f + dist(gen) * 0.15f; // Smaller radii to keep within bounds
            // Place swirl centers relative to origin
            centers_x[s] = origin_x + (dist(gen) - 0.5f) * 0.3f;
            centers_y[s] = origin_y + (dist(gen) - 0.5f) * 0.3f;
            speeds[s] = (dist(gen) < 0.5f ? 1.0f : -1.0f) * (0.5f + dist(gen) * 0.5f);
        }
        
        float angle_step = 2.0f * juce::MathConstants<float>::pi / num_points;
        
        for (int i = 0; i < num_points; ++i)
        {
            float x = origin_x;
            float y = origin_y;
            
            // Sum contributions from all swirls
            for (int s = 0; s < num_swirls; ++s)
            {
                float swirl_x = centers_x[s] + radii[s] * std::cos(angles[s]);
                float swirl_y = centers_y[s] + radii[s] * std::sin(angles[s]);
                
                // Blend with center
                x = x * 0.5f + swirl_x * 0.5f;
                y = y * 0.5f + swirl_y * 0.5f;
                
                angles[s] += angle_step * speeds[s];
            }
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
        }
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generate_bounce_path(int num_points)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> point_dist(200, 300);
        
        // Generate random number of points if not specified
        if (num_points <= 0)
            num_points = point_dist(gen);
        
        // Ensure minimum points
        if (num_points < 2)
            num_points = 2;
        
        // Define quadrants
        struct Quadrant {
            float x_sign, y_sign;
        };
        
        Quadrant quads[4] = {
            {1.0f, 1.0f},   // Quadrant 1: top-right
            {-1.0f, 1.0f},  // Quadrant 2: top-left
            {-1.0f, -1.0f}, // Quadrant 3: bottom-left
            {1.0f, -1.0f}   // Quadrant 4: bottom-right
        };
        
        // Pick two random quadrants
        std::vector<int> quad_indices = {0, 1, 2, 3};
        std::shuffle(quad_indices.begin(), quad_indices.end(), gen);
        
        int quad_idx1 = quad_indices[0];
        int quad_idx2 = quad_indices[1];
        
        Quadrant quad1 = quads[quad_idx1];
        Quadrant quad2 = quads[quad_idx2];
        
        // Sample random origin within r=0.5 from center
        auto origin = sample_random_origin();
        float origin_x = origin.first;
        float origin_y = origin.second;
        
        // Pick point 1: offset from origin in quadrant direction
        // Use smaller offset to ensure bounds
        float offset1 = 0.2f + dist(gen) * 0.2f; // 0.2 to 0.4
        float x1 = origin_x + offset1 * quad1.x_sign;
        float y1 = origin_y + offset1 * quad1.y_sign;
        
        // Pick point 2: offset from origin in quadrant direction
        float offset2 = 0.2f + dist(gen) * 0.2f; // 0.2 to 0.4
        float x2 = origin_x + offset2 * quad2.x_sign;
        float y2 = origin_y + offset2 * quad2.y_sign;
        
        // Clamp to valid range
        x1 = juce::jlimit(0.0f, 1.0f, x1);
        y1 = juce::jlimit(0.0f, 1.0f, y1);
        x2 = juce::jlimit(0.0f, 1.0f, x2);
        y2 = juce::jlimit(0.0f, 1.0f, y2);
        
        // Generate intermediate points between the two bounce points
        for (int i = 0; i < num_points; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(num_points - 1);
            // Use smooth interpolation (ease-in-out) for more natural bounce motion
            float smooth_t = t * t * (3.0f - 2.0f * t); // Smoothstep
            float x = x1 + (x2 - x1) * smooth_t;
            float y = y1 + (y2 - y1) * smooth_t;
            
            coords.push_back({x, y});
        }
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generate_spiral_path(int num_points)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> point_dist(50, 100);
        std::uniform_int_distribution<int> dir_dist(0, 1);
        
        // Generate random number of points if not specified
        if (num_points <= 0)
            num_points = point_dist(gen);
        
        // Random direction
        int direction = dir_dist(gen) == 0 ? 1 : -1;
        
        // Sample random origin within r=0.5 from center
        auto origin = sample_random_origin();
        float origin_x = origin.first;
        float origin_y = origin.second;
        
        // Random number of turns
        float num_turns = 1.0f + dist(gen) * 2.0f; // 1-3 turns
        
        // Start from origin and spiral outward
        // Use smaller max radius to ensure we stay within bounds
        float max_radius = 0.3f;
        
        for (int i = 0; i < num_points; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(num_points - 1);
            float angle = t * num_turns * 2.0f * juce::MathConstants<float>::pi * direction;
            float radius = t * max_radius;
            
            float x = origin_x + radius * std::cos(angle);
            float y = origin_y + radius * std::sin(angle);
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
        }
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generate_horizontal_line_path(int num_points)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> point_dist(200, 300);
        std::uniform_int_distribution<int> dir_dist(0, 1);
        
        // Generate random number of points if not specified
        if (num_points <= 0)
            num_points = point_dist(gen);
        
        // Random Y position (vertical position of the line)
        float y = 0.2f + dist(gen) * 0.6f; // Keep within bounds (0.2 to 0.8)
        
        // Random direction (left to right or right to left)
        int direction = dir_dist(gen) == 0 ? 1 : -1;
        
        // Start position
        float start_x = direction == 1 ? 0.1f : 0.9f;
        
        // Generate points along horizontal line
        for (int i = 0; i < num_points; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(num_points - 1);
            float x = start_x + direction * t * 0.8f; // Move across 80% of width
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
        }
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generate_vertical_line_path(int num_points)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> point_dist(200, 300);
        std::uniform_int_distribution<int> dir_dist(0, 1);
        
        // Generate random number of points if not specified
        if (num_points <= 0)
            num_points = point_dist(gen);
        
        // Random X position (horizontal position of the line)
        float x = 0.2f + dist(gen) * 0.6f; // Keep within bounds (0.2 to 0.8)
        
        // Random direction (bottom to top or top to bottom)
        int direction = dir_dist(gen) == 0 ? 1 : -1;
        
        // Start position
        float start_y = direction == 1 ? 0.1f : 0.9f;
        
        // Generate points along vertical line
        for (int i = 0; i < num_points; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(num_points - 1);
            float y = start_y + direction * t * 0.8f; // Move across 80% of height
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
        }
        
        return coords;
    }
}

