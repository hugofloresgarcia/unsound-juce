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
        cosineTable.initialise(
            [] (float angle) { return std::cos(angle); },
            0.0f,
            juce::MathConstants<float>::halfPi,
            numPoints
        );
        
        // Initialize sine table: maps angle (0 to π/2) to sin(angle)
        sineTable.initialise(
            [] (float angle) { return std::sin(angle); },
            0.0f,
            juce::MathConstants<float>::halfPi,
            numPoints
        );
    }

    float CosinePanningLaw::getCosine(float angle) const
    {
        angle = juce::jlimit(0.0f, juce::MathConstants<float>::halfPi, angle);
        return cosineTable.processSampleUnchecked(angle);
    }

    float CosinePanningLaw::getSine(float angle) const
    {
        angle = juce::jlimit(0.0f, juce::MathConstants<float>::halfPi, angle);
        return sineTable.processSampleUnchecked(angle);
    }

    //==============================================================================
    // Singleton instance
    static CosinePanningLaw g_cosinePanningLaw;

    const CosinePanningLaw& getCosinePanningLaw()
    {
        return g_cosinePanningLaw;
    }

    //==============================================================================
    std::pair<float, float> computeStereoGains(float pan)
    {
        pan = juce::jlimit(0.0f, 1.0f, pan);
        
        // Map pan (0-1) to angle (0 to π/2)
        float angle = pan * juce::MathConstants<float>::halfPi;
        
        const auto& law = getCosinePanningLaw();
        float left = law.getCosine(angle);
        float right = law.getSine(angle);
        
        return {left, right};
    }

    //==============================================================================
    std::array<float, 4> computeQuadGains(float x, float y)
    {
        x = juce::jlimit(0.0f, 1.0f, x);
        y = juce::jlimit(0.0f, 1.0f, y);
        
        // Speaker positions in normalized 0-1 space:
        // FL: (0, 1) - Front Left
        // FR: (1, 1) - Front Right
        // BL: (0, 0) - Back Left
        // BR: (1, 0) - Back Right
        
        // Calculate distances from pan position to each speaker
        float dxFL = x - 0.0f;  // distance to left edge
        float dxFR = x - 1.0f;  // distance to right edge
        float dyF = y - 1.0f;   // distance to front edge
        float dyB = y - 0.0f;   // distance to back edge
        
        // Normalize distances to 0-1 range (max distance is diagonal = √2)
        float maxDist = std::sqrt(2.0f);
        float distFL = std::sqrt(dxFL * dxFL + dyF * dyF) / maxDist;
        float distFR = std::sqrt(dxFR * dxFR + dyF * dyF) / maxDist;
        float distBL = std::sqrt(dxFL * dxFL + dyB * dyB) / maxDist;
        float distBR = std::sqrt(dxFR * dxFR + dyB * dyB) / maxDist;
        
        // Convert distances to angles (closer = higher gain)
        // Use cosine law: gain = cos(angle), where angle is proportional to distance
        const auto& law = getCosinePanningLaw();
        float angleFL = distFL * juce::MathConstants<float>::halfPi;
        float angleFR = distFR * juce::MathConstants<float>::halfPi;
        float angleBL = distBL * juce::MathConstants<float>::halfPi;
        float angleBR = distBR * juce::MathConstants<float>::halfPi;
        
        float gainFL = law.getCosine(angleFL);
        float gainFR = law.getCosine(angleFR);
        float gainBL = law.getCosine(angleBL);
        float gainBR = law.getCosine(angleBR);
        
        // Normalize gains to preserve energy
        float sum = gainFL + gainFR + gainBL + gainBR;
        if (sum > 0.0f)
        {
            float norm = 1.0f / sum;
            gainFL *= norm;
            gainFR *= norm;
            gainBL *= norm;
            gainBR *= norm;
        }
        
        return {gainFL, gainFR, gainBL, gainBR};
    }

    //==============================================================================
    std::array<float, 16> computeCLEATGains(float x, float y)
    {
        x = juce::jlimit(0.0f, 1.0f, x);
        y = juce::jlimit(0.0f, 1.0f, y);
        
        // CLEAT speaker grid: 4x4, row-major ordering
        // Channels 0-3: bottom row (left to right)
        // Channels 4-7: second row (left to right)
        // Channels 8-11: third row (left to right)
        // Channels 12-15: top row (left to right)
        
        std::array<float, 16> gains = {0.0f};
        
        // Calculate speaker positions
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                int channel = row * 4 + col;
                
                // Speaker position in normalized 0-1 space
                float speakerX = col / 3.0f;  // 0, 1/3, 2/3, 1
                float speakerY = row / 3.0f; // 0, 1/3, 2/3, 1
                
                // Calculate distance from pan position to speaker
                float dx = x - speakerX;
                float dy = y - speakerY;
                float dist = std::sqrt(dx * dx + dy * dy);
                
                // Normalize distance (max distance is diagonal = √2)
                float maxDist = std::sqrt(2.0f);
                float normalizedDist = dist / maxDist;
                
                // Convert distance to angle and apply cosine law
                float angle = normalizedDist * juce::MathConstants<float>::halfPi;
                const auto& law = getCosinePanningLaw();
                gains[channel] = law.getCosine(angle);
            }
        }
        
        // Normalize gains to preserve energy
        float sum = 0.0f;
        for (float gain : gains)
            sum += gain;
        
        if (sum > 0.0f)
        {
            float norm = 1.0f / sum;
            for (float& gain : gains)
                gain *= norm;
        }
        
        return gains;
    }
    
    //==============================================================================
    // Path generation functions
    
    // Helper function to sample a random origin point within r=0.25 from center (0.5, 0.5)
    static std::pair<float, float> sampleRandomOrigin()
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
            float distSq = dx * dx + dy * dy;
            if (distSq <= 0.0625f) // r^2 = 0.25^2 = 0.0625
                break;
        } while (true);
        
        return {x, y};
    }
    
    std::vector<std::pair<float, float>> generateCirclePath(int numPoints)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> pointDist(50, 100);
        std::uniform_int_distribution<int> dirDist(0, 1);
        
        // Generate random number of points if not specified
        if (numPoints <= 0)
            numPoints = pointDist(gen);
        
        // Sample random origin within r=0.5 from center
        auto origin = sampleRandomOrigin();
        float originX = origin.first;
        float originY = origin.second;
        
        // Random starting angle
        float angle = dist(gen) * 2.0f * juce::MathConstants<float>::pi;
        
        // Random direction (clockwise or counterclockwise)
        int direction = dirDist(gen) == 0 ? 1 : -1;
        
        // Random radius (0.15 to 0.3, relative to origin)
        float radius = 0.15f + dist(gen) * 0.15f;
        
        float angleStep = (2.0f * juce::MathConstants<float>::pi / numPoints) * direction;
        
        for (int i = 0; i < numPoints; ++i)
        {
            float x = originX + radius * std::cos(angle);
            float y = originY + radius * std::sin(angle);
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
            angle += angleStep;
        }
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generateRandomPath(int numPoints)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> pointDist(50, 100);
        
        // Generate random number of points if not specified
        if (numPoints <= 0)
            numPoints = pointDist(gen);
        
        // Ensure minimum points
        if (numPoints < 4)
            numPoints = 4;
        
        // Sample random origin within r=0.5 from center
        auto origin = sampleRandomOrigin();
        float originX = origin.first;
        float originY = origin.second;
        
        // Generate random points around the origin, ensuring they stay within bounds
        for (int i = 0; i < numPoints; ++i)
        {
            // Generate offset from origin (smaller range to ensure bounds)
            float offsetX = (dist(gen) - 0.5f) * 0.8f; // Scale to keep within bounds
            float offsetY = (dist(gen) - 0.5f) * 0.8f;
            
            float x = originX + offsetX;
            float y = originY + offsetY;
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
        }
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generateWanderPath(int numPoints)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_real_distribution<float> wanderDist(-0.1f, 0.1f);
        std::uniform_int_distribution<int> pointDist(50, 100);
        
        // Generate random number of points if not specified
        if (numPoints <= 0)
            numPoints = pointDist(gen);
        
        // Sample random origin within r=0.5 from center
        auto origin = sampleRandomOrigin();
        float x = origin.first;
        float y = origin.second;
        
        for (int i = 0; i < numPoints; ++i)
        {
            // Add random wander
            x += wanderDist(gen);
            y += wanderDist(gen);
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
        }
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generateSwirlsPath(int numPoints)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> pointDist(50, 100);
        
        // Generate random number of points if not specified
        if (numPoints <= 0)
            numPoints = pointDist(gen);
        
        // Sample random origin within r=0.5 from center
        auto origin = sampleRandomOrigin();
        float originX = origin.first;
        float originY = origin.second;
        
        // Create multiple overlapping circular motions
        const int numSwirls = 2 + (gen() % 3); // 2-4 swirls
        std::vector<float> angles(numSwirls);
        std::vector<float> radii(numSwirls);
        std::vector<float> centersX(numSwirls);
        std::vector<float> centersY(numSwirls);
        std::vector<float> speeds(numSwirls);
        
        for (int s = 0; s < numSwirls; ++s)
        {
            angles[s] = dist(gen) * 2.0f * juce::MathConstants<float>::pi;
            radii[s] = 0.08f + dist(gen) * 0.15f; // Smaller radii to keep within bounds
            // Place swirl centers relative to origin
            centersX[s] = originX + (dist(gen) - 0.5f) * 0.3f;
            centersY[s] = originY + (dist(gen) - 0.5f) * 0.3f;
            speeds[s] = (dist(gen) < 0.5f ? 1.0f : -1.0f) * (0.5f + dist(gen) * 0.5f);
        }
        
        float angleStep = 2.0f * juce::MathConstants<float>::pi / numPoints;
        
        for (int i = 0; i < numPoints; ++i)
        {
            float x = originX;
            float y = originY;
            
            // Sum contributions from all swirls
            for (int s = 0; s < numSwirls; ++s)
            {
                float swirlX = centersX[s] + radii[s] * std::cos(angles[s]);
                float swirlY = centersY[s] + radii[s] * std::sin(angles[s]);
                
                // Blend with center
                x = x * 0.5f + swirlX * 0.5f;
                y = y * 0.5f + swirlY * 0.5f;
                
                angles[s] += angleStep * speeds[s];
            }
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
        }
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generateBouncePath()
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        
        // Define quadrants
        struct Quadrant {
            float xSign, ySign;
        };
        
        Quadrant quads[4] = {
            {1.0f, 1.0f},   // Quadrant 1: top-right
            {-1.0f, 1.0f},  // Quadrant 2: top-left
            {-1.0f, -1.0f}, // Quadrant 3: bottom-left
            {1.0f, -1.0f}   // Quadrant 4: bottom-right
        };
        
        // Pick two random quadrants
        std::vector<int> quadIndices = {0, 1, 2, 3};
        std::shuffle(quadIndices.begin(), quadIndices.end(), gen);
        
        int quadIdx1 = quadIndices[0];
        int quadIdx2 = quadIndices[1];
        
        Quadrant quad1 = quads[quadIdx1];
        Quadrant quad2 = quads[quadIdx2];
        
        // Sample random origin within r=0.5 from center
        auto origin = sampleRandomOrigin();
        float originX = origin.first;
        float originY = origin.second;
        
        // Pick point 1: offset from origin in quadrant direction
        // Use smaller offset to ensure bounds
        float offset1 = 0.2f + dist(gen) * 0.2f; // 0.2 to 0.4
        float x1 = originX + offset1 * quad1.xSign;
        float y1 = originY + offset1 * quad1.ySign;
        
        // Pick point 2: offset from origin in quadrant direction
        float offset2 = 0.2f + dist(gen) * 0.2f; // 0.2 to 0.4
        float x2 = originX + offset2 * quad2.xSign;
        float y2 = originY + offset2 * quad2.ySign;
        
        // Clamp to valid range
        x1 = juce::jlimit(0.0f, 1.0f, x1);
        y1 = juce::jlimit(0.0f, 1.0f, y1);
        x2 = juce::jlimit(0.0f, 1.0f, x2);
        y2 = juce::jlimit(0.0f, 1.0f, y2);
        
        coords.push_back({x1, y1});
        coords.push_back({x2, y2});
        
        return coords;
    }
    
    std::vector<std::pair<float, float>> generateSpiralPath(int numPoints)
    {
        std::vector<std::pair<float, float>> coords;
        
        // Random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> pointDist(50, 100);
        std::uniform_int_distribution<int> dirDist(0, 1);
        
        // Generate random number of points if not specified
        if (numPoints <= 0)
            numPoints = pointDist(gen);
        
        // Random direction
        int direction = dirDist(gen) == 0 ? 1 : -1;
        
        // Sample random origin within r=0.5 from center
        auto origin = sampleRandomOrigin();
        float originX = origin.first;
        float originY = origin.second;
        
        // Random number of turns
        float numTurns = 1.0f + dist(gen) * 2.0f; // 1-3 turns
        
        // Start from origin and spiral outward
        // Use smaller max radius to ensure we stay within bounds
        float maxRadius = 0.3f;
        
        for (int i = 0; i < numPoints; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(numPoints - 1);
            float angle = t * numTurns * 2.0f * juce::MathConstants<float>::pi * direction;
            float radius = t * maxRadius;
            
            float x = originX + radius * std::cos(angle);
            float y = originY + radius * std::sin(angle);
            
            // Clamp to 0-1 range
            x = juce::jlimit(0.0f, 1.0f, x);
            y = juce::jlimit(0.0f, 1.0f, y);
            
            coords.push_back({x, y});
        }
        
        return coords;
    }
}

