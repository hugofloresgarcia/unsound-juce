#include "PannerTests.h"
#include "PanningUtils.h"
#include "StereoPanner.h"
#include "QuadPanner.h"
#include "CLEATPanner.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <algorithm>

//==============================================================================
struct PanningUtilsTests final : public juce::UnitTest
{
    PanningUtilsTests() : juce::UnitTest("PanningUtils", "Panners") {}

    void runTest() override
    {
        beginTest("Stereo gains at extremes");
        {
            // Pan = 0.0 (all left)
            auto [left, right] = PanningUtils::computeStereoGains(0.0f);
            expectWithinAbsoluteError(left, 1.0f, 0.01f, "Left gain should be 1.0 at pan=0");
            expectWithinAbsoluteError(right, 0.0f, 0.01f, "Right gain should be 0.0 at pan=0");
            
            // Pan = 1.0 (all right)
            std::tie(left, right) = PanningUtils::computeStereoGains(1.0f);
            expectWithinAbsoluteError(left, 0.0f, 0.01f, "Left gain should be 0.0 at pan=1");
            expectWithinAbsoluteError(right, 1.0f, 0.01f, "Right gain should be 1.0 at pan=1");
            
            // Pan = 0.5 (center)
            std::tie(left, right) = PanningUtils::computeStereoGains(0.5f);
            float expectedCenter = std::cos(juce::MathConstants<float>::pi / 4.0f); // cos(π/4) = √2/2
            expectWithinAbsoluteError(left, expectedCenter, 0.01f, "Left gain should be √2/2 at pan=0.5");
            expectWithinAbsoluteError(right, expectedCenter, 0.01f, "Right gain should be √2/2 at pan=0.5");
        }

        beginTest("Stereo gains follow cosine law (equal power)");
        {
            // Equal power panning (cosine law) does NOT sum to 1.0
            // At center (pan=0.5), sum should be √2 ≈ 1.414
            // At extremes (pan=0 or 1), sum should be 1.0
            for (float pan = 0.0f; pan <= 1.0f; pan += 0.1f)
            {
                auto [left, right] = PanningUtils::computeStereoGains(pan);
                float sum = left + right;
                
                if (std::abs(pan - 0.0f) < 0.01f || std::abs(pan - 1.0f) < 0.01f)
                {
                    // At extremes, sum should be 1.0
                    expectWithinAbsoluteError(sum, 1.0f, 0.01f,
                        "Gain sum should be 1.0 at extreme pan=" + juce::String(pan));
                }
                else
                {
                    // At other positions, sum should be > 1.0 (equal power)
                    expect(sum > 1.0f, "Gain sum should be > 1.0 for equal power panning at pan=" + juce::String(pan));
                    // At center, sum should be √2
                    if (std::abs(pan - 0.5f) < 0.01f)
                    {
                        expectWithinAbsoluteError(sum, std::sqrt(2.0f), 0.01f,
                            "Gain sum should be √2 at center pan");
                    }
                }
            }
        }

        beginTest("Quad gains at corners");
        {
            // Bottom-left corner (0, 0)
            auto gains = PanningUtils::computeQuadGains(0.0f, 0.0f);
            expect(gains[2] > gains[0] && gains[2] > gains[1] && gains[2] > gains[3],
                   "BL should have highest gain at (0,0)");
            
            // Bottom-right corner (1, 0)
            gains = PanningUtils::computeQuadGains(1.0f, 0.0f);
            expect(gains[3] > gains[0] && gains[3] > gains[1] && gains[3] > gains[2],
                   "BR should have highest gain at (1,0)");
            
            // Top-left corner (0, 1)
            gains = PanningUtils::computeQuadGains(0.0f, 1.0f);
            expect(gains[0] > gains[1] && gains[0] > gains[2] && gains[0] > gains[3],
                   "FL should have highest gain at (0,1)");
            
            // Top-right corner (1, 1)
            gains = PanningUtils::computeQuadGains(1.0f, 1.0f);
            expect(gains[1] > gains[0] && gains[1] > gains[2] && gains[1] > gains[3],
                   "FR should have highest gain at (1,1)");
        }

        beginTest("Quad gains sum to 1.0");
        {
            for (float x = 0.0f; x <= 1.0f; x += 0.2f)
            {
                for (float y = 0.0f; y <= 1.0f; y += 0.2f)
                {
                    auto gains = PanningUtils::computeQuadGains(x, y);
                    float sum = gains[0] + gains[1] + gains[2] + gains[3];
                    expectWithinAbsoluteError(sum, 1.0f, 0.01f,
                        "Gain sum should be 1.0 at (" + juce::String(x) + "," + juce::String(y) + ")");
                }
            }
        }

        beginTest("CLEAT gains at corners");
        {
            // Bottom-left corner (0, 0) - should favor channel 0
            auto gains = PanningUtils::computeCLEATGains(0.0f, 0.0f);
            expect(gains[0] > gains[15], "Channel 0 should have higher gain than channel 15 at (0,0)");
            
            // Bottom-right corner (1, 0) - should favor channel 3
            gains = PanningUtils::computeCLEATGains(1.0f, 0.0f);
            expect(gains[3] > gains[12], "Channel 3 should have higher gain than channel 12 at (1,0)");
            
            // Top-left corner (0, 1) - should favor channel 12
            gains = PanningUtils::computeCLEATGains(0.0f, 1.0f);
            expect(gains[12] > gains[0], "Channel 12 should have higher gain than channel 0 at (0,1)");
            
            // Top-right corner (1, 1) - should favor channel 15
            gains = PanningUtils::computeCLEATGains(1.0f, 1.0f);
            expect(gains[15] > gains[0], "Channel 15 should have higher gain than channel 0 at (1,1)");
        }

        beginTest("CLEAT gains sum to 1.0");
        {
            for (float x = 0.0f; x <= 1.0f; x += 0.25f)
            {
                for (float y = 0.0f; y <= 1.0f; y += 0.25f)
                {
                    auto gains = PanningUtils::computeCLEATGains(x, y);
                    float sum = 0.0f;
                    for (float gain : gains)
                        sum += gain;
                    expectWithinAbsoluteError(sum, 1.0f, 0.01f,
                        "Gain sum should be 1.0 at (" + juce::String(x) + "," + juce::String(y) + ")");
                }
            }
        }
    }
};

//==============================================================================
struct StereoPannerTests final : public juce::UnitTest
{
    StereoPannerTests() : juce::UnitTest("StereoPanner", "Panners") {}

    void runTest() override
    {
        beginTest("Channel counts");
        {
            StereoPanner panner;
            expectEquals(panner.getNumInputChannels(), 1, "Should have 1 input channel");
            expectEquals(panner.getNumOutputChannels(), 2, "Should have 2 output channels");
        }

        beginTest("Pan position get/set");
        {
            StereoPanner panner;
            panner.setPan(0.25f);
            expectWithinAbsoluteError(panner.getPan(), 0.25f, 0.001f, "Pan should be 0.25");
            
            panner.setPan(0.75f);
            expectWithinAbsoluteError(panner.getPan(), 0.75f, 0.001f, "Pan should be 0.75");
            
            // Test clamping
            panner.setPan(-0.5f);
            expectWithinAbsoluteError(panner.getPan(), 0.0f, 0.001f, "Pan should clamp to 0.0");
            
            panner.setPan(1.5f);
            expectWithinAbsoluteError(panner.getPan(), 1.0f, 0.001f, "Pan should clamp to 1.0");
        }

        beginTest("ProcessBlock mono to stereo");
        {
            StereoPanner panner;
            panner.setPan(0.0f); // All left
            
            constexpr int numSamples = 64;
            float inputBuffer[numSamples];
            float* inputChannels[1] = {inputBuffer};
            
            float leftBuffer[numSamples] = {0};
            float rightBuffer[numSamples] = {0};
            float* outputChannels[2] = {leftBuffer, rightBuffer};
            
            // Fill input with test signal
            for (int i = 0; i < numSamples; ++i)
                inputBuffer[i] = 1.0f;
            
            panner.processBlock(inputChannels, 1, outputChannels, 2, numSamples);
            
            // At pan=0, all signal should go to left
            for (int i = 0; i < numSamples; ++i)
            {
                expectWithinAbsoluteError(leftBuffer[i], 1.0f, 0.01f, 
                    "Left channel should have full signal at pan=0");
                expectWithinAbsoluteError(rightBuffer[i], 0.0f, 0.01f,
                    "Right channel should be silent at pan=0");
            }
        }

        beginTest("ProcessBlock center pan");
        {
            StereoPanner panner;
            panner.setPan(0.5f); // Center
            
            constexpr int numSamples = 64;
            float inputBuffer[numSamples];
            float* inputChannels[1] = {inputBuffer};
            
            float leftBuffer[numSamples] = {0};
            float rightBuffer[numSamples] = {0};
            float* outputChannels[2] = {leftBuffer, rightBuffer};
            
            // Fill input with test signal
            for (int i = 0; i < numSamples; ++i)
                inputBuffer[i] = 1.0f;
            
            panner.processBlock(inputChannels, 1, outputChannels, 2, numSamples);
            
            // At pan=0.5, signal should be split equally
            float expectedCenter = std::cos(juce::MathConstants<float>::pi / 4.0f);
            for (int i = 0; i < numSamples; ++i)
            {
                expectWithinAbsoluteError(leftBuffer[i], expectedCenter, 0.01f,
                    "Left channel should have center gain");
                expectWithinAbsoluteError(rightBuffer[i], expectedCenter, 0.01f,
                    "Right channel should have center gain");
            }
        }
    }
};

//==============================================================================
struct QuadPannerTests final : public juce::UnitTest
{
    QuadPannerTests() : juce::UnitTest("QuadPanner", "Panners") {}

    void runTest() override
    {
        beginTest("Channel counts");
        {
            QuadPanner panner;
            expectEquals(panner.getNumInputChannels(), 1, "Should have 1 input channel");
            expectEquals(panner.getNumOutputChannels(), 4, "Should have 4 output channels");
        }

        beginTest("Pan position get/set");
        {
            QuadPanner panner;
            panner.setPan(0.25f, 0.75f);
            expectWithinAbsoluteError(panner.getPanX(), 0.25f, 0.001f, "PanX should be 0.25");
            expectWithinAbsoluteError(panner.getPanY(), 0.75f, 0.001f, "PanY should be 0.75");
            
            // Test clamping
            panner.setPan(-0.5f, 1.5f);
            expectWithinAbsoluteError(panner.getPanX(), 0.0f, 0.001f, "PanX should clamp to 0.0");
            expectWithinAbsoluteError(panner.getPanY(), 1.0f, 0.001f, "PanY should clamp to 1.0");
        }

        beginTest("ProcessBlock mono to quad");
        {
            QuadPanner panner;
            panner.setPan(0.0f, 0.0f); // Bottom-left
            
            constexpr int numSamples = 64;
            float inputBuffer[numSamples];
            float* inputChannels[1] = {inputBuffer};
            
            float flBuffer[numSamples] = {0};
            float frBuffer[numSamples] = {0};
            float blBuffer[numSamples] = {0};
            float brBuffer[numSamples] = {0};
            float* outputChannels[4] = {flBuffer, frBuffer, blBuffer, brBuffer};
            
            // Fill input with test signal
            for (int i = 0; i < numSamples; ++i)
                inputBuffer[i] = 1.0f;
            
            panner.processBlock(inputChannels, 1, outputChannels, 4, numSamples);
            
            // At (0,0), BL should have highest gain
            expect(blBuffer[0] > flBuffer[0] && blBuffer[0] > frBuffer[0] && blBuffer[0] > brBuffer[0],
                   "BL should have highest gain at (0,0)");
        }
    }
};

//==============================================================================
struct CLEATPannerTests final : public juce::UnitTest
{
    CLEATPannerTests() : juce::UnitTest("CLEATPanner", "Panners") {}

    void runTest() override
    {
        beginTest("Channel counts");
        {
            CLEATPanner panner;
            expectEquals(panner.getNumInputChannels(), 1, "Should have 1 input channel");
            expectEquals(panner.getNumOutputChannels(), 16, "Should have 16 output channels");
        }

        beginTest("Pan position get/set");
        {
            CLEATPanner panner;
            panner.setPan(0.3f, 0.7f);
            expectWithinAbsoluteError(panner.getPanX(), 0.3f, 0.001f, "PanX should be 0.3");
            expectWithinAbsoluteError(panner.getPanY(), 0.7f, 0.001f, "PanY should be 0.7");
            
            // Test clamping
            panner.setPan(-0.5f, 1.5f);
            expectWithinAbsoluteError(panner.getPanX(), 0.0f, 0.001f, "PanX should clamp to 0.0");
            expectWithinAbsoluteError(panner.getPanY(), 1.0f, 0.001f, "PanY should clamp to 1.0");
        }

        beginTest("ProcessBlock mono to CLEAT");
        {
            CLEATPanner panner;
            panner.setPan(0.0f, 0.0f); // Bottom-left
            
            constexpr int numSamples = 64;
            float inputBuffer[numSamples];
            float* inputChannels[1] = {inputBuffer};
            
            float outputBuffers[16][numSamples] = {{0}};
            float* outputChannels[16];
            for (int i = 0; i < 16; ++i)
                outputChannels[i] = outputBuffers[i];
            
            // Fill input with test signal
            for (int i = 0; i < numSamples; ++i)
                inputBuffer[i] = 1.0f;
            
            panner.processBlock(inputChannels, 1, outputChannels, 16, numSamples);
            
            // At (0,0), channel 0 should have highest gain
            expect(outputBuffers[0][0] > outputBuffers[15][0],
                   "Channel 0 should have higher gain than channel 15 at (0,0)");
        }
    }
};

//==============================================================================
// Static test instances (automatically registered)
static PanningUtilsTests panningUtilsTests;
static StereoPannerTests stereoPannerTests;
static QuadPannerTests quadPannerTests;
static CLEATPannerTests cleatPannerTests;

