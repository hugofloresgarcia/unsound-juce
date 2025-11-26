#include <juce_core/juce_core.h>
#include <flowerjuce/Panners/StereoPanner.h>
#include <flowerjuce/Panners/QuadPanner.h>
#include <flowerjuce/Panners/CLEATPanner.h>
#include "TestUtils.h"
#include <random>
#include <cmath>

// Simple Sine Wave Generator at -3dBFS
class SineWave
{
public:
    SineWave() 
    {
        reset();
    }

    void reset()
    {
        phase = 0.0;
        // -3dBFS amplitude: 10^(-3/20) ~= 0.707945784
        amplitude = std::pow(10.0, -3.0 / 20.0);
        // Frequency 1kHz at 44.1kHz
        increment = 1000.0 / 44100.0;
    }

    // Returns sample
    float next()
    {
        float sample = (float)(std::sin(phase * juce::MathConstants<double>::twoPi) * amplitude);
        phase += increment;
        if (phase >= 1.0) phase -= 1.0;
        return sample;
    }

private:
    double phase = 0.0;
    double increment = 0.0;
    double amplitude = 0.0;
};

class PannerTests : public juce::UnitTest
{
public:
    PannerTests() : juce::UnitTest("PannerTests") {}

    void runTest() override
    {
        beginTest("Stereo Panner Sweep");
        testStereoPannerSweep();

        beginTest("Quad Panner Sweep");
        testQuadPannerSweep();

        beginTest("CLEAT Panner Sweep");
        testCLEATPannerSweep();

        beginTest("Stereo Panner Random Checks");
        testStereoPannerRandom();

        beginTest("Quad Panner Random Checks");
        testQuadPannerRandom();

        beginTest("CLEAT Panner Random Checks");
        testCLEATPannerRandom();
    }

private:
    // Helper to run audio through panner and measure RMS of outputs
    // Returns vector of RMS values for each channel
    std::vector<float> measurePannerOutput(Panner& panner, int numChannels, int numSamples, SineWave& source)
    {
        // Prepare buffers
        // Input: 1 channel (mono source)
        juce::AudioBuffer<float> inputBuffer(1, numSamples);
        auto* inWrite = inputBuffer.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
            inWrite[i] = source.next();

        // Output: N channels
        juce::AudioBuffer<float> outputBuffer(numChannels, numSamples);
        outputBuffer.clear();

        // Pointers for process_block
        const float* inputPtrs[] = { inputBuffer.getReadPointer(0) };
        float* outputPtrs[16]; // Max 16 for CLEAT
        for (int i = 0; i < numChannels; ++i)
            outputPtrs[i] = outputBuffer.getWritePointer(i);

        // Process
        panner.process_block(inputPtrs, 1, outputPtrs, numChannels, numSamples);

        // Calculate RMS for each channel
        std::vector<float> rmsValues(numChannels);
        for (int i = 0; i < numChannels; ++i)
        {
            rmsValues[i] = outputBuffer.getRMSLevel(i, 0, numSamples);
        }
        return rmsValues;
    }

    void testStereoPannerSweep()
    {
        StereoPanner panner;
        SineWave source;
        int blockSize = 256;
        
        TestUtils::CsvWriter writer("stereo_panner_sweep", {"Pan", "Left_RMS", "Right_RMS", "Total_Power"});

        // Sweep pan from 0 to 1
        for (float pan = 0.0f; pan <= 1.0f; pan += 0.01f)
        {
            panner.set_pan(pan);
            auto rms = measurePannerOutput(panner, 2, blockSize, source);
            
            float left = rms[0];
            float right = rms[1];
            float power = left * left + right * right;

            writer.writeRow(pan, left, right, power);
        }
    }

    void testQuadPannerSweep()
    {
        QuadPanner panner;
        SineWave source;
        int blockSize = 256;
        
        TestUtils::CsvWriter writer("quad_panner_sweep", {"Time", "PanX", "PanY", "FL", "FR", "BL", "BR"});

        // Circular sweep
        int steps = 100;
        for (int i = 0; i < steps; ++i)
        {
            float angle = (float)i / steps * juce::MathConstants<float>::twoPi;
            float radius = 0.5f;
            float panX = 0.5f + std::cos(angle) * radius; // 0 to 1
            float panY = 0.5f + std::sin(angle) * radius; // 0 to 1
            
            // Clamp to 0-1
            panX = juce::jlimit(0.0f, 1.0f, panX);
            panY = juce::jlimit(0.0f, 1.0f, panY);

            panner.set_pan(panX, panY);
            auto rms = measurePannerOutput(panner, 4, blockSize, source);

            writer.writeRow(i, panX, panY, rms[0], rms[1], rms[2], rms[3]);
        }
    }

    void testCLEATPannerSweep()
    {
        CLEATPanner panner;
        panner.prepare(44100.0);
        SineWave source;
        int blockSize = 256;
        
        // Manual CSV writing for variable channels
        juce::File outputDir = juce::File::getCurrentWorkingDirectory().getChildFile("tests/output");
        if (!outputDir.exists()) outputDir.createDirectory();
        juce::File csvFile = outputDir.getChildFile("cleat_panner_sweep.csv");
        std::ofstream ofs(csvFile.getFullPathName().toStdString());
        
        // Header
        ofs << "Time,PanX,PanY";
        for (int i=0; i<16; ++i) ofs << ",Ch" << i;
        ofs << "\n";

        // Diagonal sweep from (0,0) to (1,1)
        for (float t = 0.0f; t <= 1.0f; t += 0.01f)
        {
            panner.set_pan(t, t);
            auto rms = measurePannerOutput(panner, 16, blockSize, source);
            
            ofs << t << "," << t << "," << t;
            for (float val : rms) ofs << "," << val;
            ofs << "\n";
        }
    }

    void testStereoPannerRandom()
    {
        StereoPanner panner;
        SineWave source;
        int blockSize = 4096; // Larger block for stable RMS
        std::mt19937 rng(1234);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (int i = 0; i < 20; ++i)
        {
            float pan = dist(rng);
            panner.set_pan(pan);
            auto rms = measurePannerOutput(panner, 2, blockSize, source);
            
            float l = rms[0];
            float r = rms[1];
            
            float distL = pan; // 0..1 distance from 0
            float distR = 1.0f - pan; // distance from 1
            
            if (distL < distR) {
                expectGreaterThan(l, r, "Left should be louder when closer to Left (pan=" + juce::String(pan) + ")");
            } else if (distR < distL) {
                expectGreaterThan(r, l, "Right should be louder when closer to Right (pan=" + juce::String(pan) + ")");
            }
        }
    }

    void testQuadPannerRandom()
    {
        QuadPanner panner;
        SineWave source;
        int blockSize = 4096;
        std::mt19937 rng(5678);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        // Speaker positions: FL(0,1), FR(1,1), BL(0,0), BR(1,0)
        struct Point { float x, y; };
        Point speakers[4] = { {0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f} };

        for (int i = 0; i < 20; ++i)
        {
            float x = dist(rng);
            float y = dist(rng);
            panner.set_pan(x, y);
            auto rms = measurePannerOutput(panner, 4, blockSize, source);
            
            // Find closest speaker
            int closestIdx = -1;
            float minDistance = 100.0f;
            
            for (int ch = 0; ch < 4; ++ch)
            {
                float dx = x - speakers[ch].x;
                float dy = y - speakers[ch].y;
                float d = std::sqrt(dx*dx + dy*dy);
                if (d < minDistance) {
                    minDistance = d;
                    closestIdx = ch;
                }
            }
            
            // Check if closest speaker has the highest gain
            float maxRMS = 0.0f;
            for (float val : rms) if (val > maxRMS) maxRMS = val;
            
            expectWithinAbsoluteError(rms[closestIdx], maxRMS, 0.05f * maxRMS, 
                "Closest speaker " + juce::String(closestIdx) + " should have max RMS (Pan: " + juce::String(x) + "," + juce::String(y) + ")");
        }
    }

    void testCLEATPannerRandom()
    {
        CLEATPanner panner;
        panner.prepare(44100.0);
        SineWave source;
        int blockSize = 4096;
        std::mt19937 rng(999);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        // CLEAT 4x4 grid positions
        // 16 speakers evenly spaced 0..1
        struct Point { float x, y; };
        std::vector<Point> speakers(16);
        for (int i = 0; i < 16; ++i)
        {
            int row = i / 4;
            int col = i % 4;
            speakers[i].x = col / 3.0f; // 0, 0.33, 0.66, 1
            speakers[i].y = row / 3.0f; // 0, 0.33, 0.66, 1
        }

        for (int i = 0; i < 20; ++i)
        {
            float x = dist(rng);
            float y = dist(rng);
            panner.set_pan(x, y);
            
            // Warm up smoothing
            {
                 juce::AudioBuffer<float> dummyIn(1, 44100);
                 juce::AudioBuffer<float> dummyOut(16, 44100);
                 const float* in[] = {dummyIn.getReadPointer(0)};
                 float* out[16]; for(int k=0; k<16; ++k) out[k] = dummyOut.getWritePointer(k);
                 panner.process_block(in, 1, out, 16, 44100); 
            }

            auto rms = measurePannerOutput(panner, 16, blockSize, source);
            
            // Find closest speaker
            int closestIdx = -1;
            float minDistance = 100.0f;
            
            for (int ch = 0; ch < 16; ++ch)
            {
                float dx = x - speakers[ch].x;
                float dy = y - speakers[ch].y;
                float d = std::sqrt(dx*dx + dy*dy);
                if (d < minDistance) {
                    minDistance = d;
                    closestIdx = ch;
                }
            }
            
            float maxRMS = 0.0f;
            for (float val : rms) if (val > maxRMS) maxRMS = val;

            expectWithinAbsoluteError(rms[closestIdx], maxRMS, 0.05f * maxRMS, 
                "Closest speaker " + juce::String(closestIdx) + " should have max RMS (Pan: " + juce::String(x) + "," + juce::String(y) + ")");
        }
    }
};

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;
    PannerTests tests;
    juce::UnitTestRunner runner;
    runner.runTests({&tests});
    return 0;
}
