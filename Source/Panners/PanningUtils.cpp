#include "PanningUtils.h"
#include <cmath>

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
}

