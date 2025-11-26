#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <flowerjuce/Panners/DSP/StereoPanner.h>
#include <flowerjuce/Panners/DSP/StereoPanner2D.h>
#include <flowerjuce/Panners/DSP/QuadPanner.h>
#include <flowerjuce/Panners/DSP/CLEATPanner.h>
#include "TestUtils.h"
#include <random>
#include <cmath>

// Robust Sine Wave Generator using JUCE DSP
// Uses direct calculation (no lookup table) for maximum precision to avoid artifacts
class SineWave
{
public:
    SineWave() 
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 4096;
        spec.numChannels = 1;
        
        osc.prepare(spec);
        // 0 = no lookup table (direct calculation) for pure sine
        osc.initialise([](float x) { return std::sin(x); }, 0); 
        osc.setFrequency(440.0f); // A4 - standard pitch, usually aligns reasonably well
        
        // -3dBFS amplitude: 10^(-3/20)
        gain.prepare(spec);
        gain.setGainDecibels(-3.0f);
    }

    // Returns sample
    float next()
    {
        return osc.processSample(0.0f); // processSample takes input FM, 0 for none
    }
    
    // Handle gain application if needed, but osc output is full scale -1..1
    // We apply gain post-process manually or via gain processor
    // Since we need single sample, let's just multiply.
    // But to be consistent with previous block-based approach, let's use a small helper or just do it here.
    // Actually oscillator output is 1.0. We need -3dB.
    
    float nextWithGain()
    {
        float s = osc.processSample(0.0f);
        return s * 0.707945784f; // -3dB
    }

private:
    juce::dsp::Oscillator<float> osc;
    juce::dsp::Gain<float> gain; // Unused in nextWithGain optimization
};

class PannerTests : public juce::UnitTest
{
public:
    PannerTests() : juce::UnitTest("PannerTests") {}

    void runTest() override
    {
        beginTest("Stereo Panner Sweep");
        testStereoPannerSweep();

        beginTest("Stereo Panner 2D Sweep");
        testStereoPanner2DSweep();

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
            inWrite[i] = source.nextWithGain();

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
        int blockSize = 1024; // Increased from 256 to reduce RMS fluctuation (spikes)
        
        TestUtils::CsvWriter writer("stereo_panner_sweep", {"Pan", "Left_RMS", "Right_RMS", "Total_Power"});
        TestUtils::AudioWriter audioWriter("stereo_panner_sweep", 2, 44100.0);
        std::vector<float> audioBuffer;

        // Sweep pan from 0 to 1
        for (float pan = 0.0f; pan <= 1.0f; pan += 0.01f)
        {
            panner.set_pan(pan);
            
            // Run a few blocks to capture audio
            // With 1024 samples, 3 blocks is plenty (~3000 samples = 70ms)
            for(int i=0; i<3; ++i)
            {
                juce::AudioBuffer<float> inputBuffer(1, blockSize);
                auto* inWrite = inputBuffer.getWritePointer(0);
                for (int s = 0; s < blockSize; ++s) inWrite[s] = source.nextWithGain();

                juce::AudioBuffer<float> outputBuffer(2, blockSize);
                outputBuffer.clear();

                const float* inputPtrs[] = { inputBuffer.getReadPointer(0) };
                float* outputPtrs[] = { outputBuffer.getWritePointer(0), outputBuffer.getWritePointer(1) };

                panner.process_block(inputPtrs, 1, outputPtrs, 2, blockSize);

                for(int s=0; s<blockSize; ++s)
                {
                    audioBuffer.push_back(outputBuffer.getSample(0, s));
                    audioBuffer.push_back(outputBuffer.getSample(1, s));
                }
            }

            // Measurement
            auto rms = measurePannerOutput(panner, 2, blockSize, source);
            
            float left = rms[0];
            float right = rms[1];
            float power = left * left + right * right;

            writer.writeRow(pan, left, right, power);
        }
        
        audioWriter.write(audioBuffer);
    }

    void testStereoPanner2DSweep()
    {
        StereoPanner2D panner;
        panner.prepare(44100.0, 1024);
        SineWave source;
        int blockSize = 1024;
        
        TestUtils::CsvWriter writer("stereo_panner_2d_sweep", {"Y", "Left_RMS", "Right_RMS"});
        TestUtils::AudioWriter audioWriter("stereo_panner_2d_sweep", 2, 44100.0);
        std::vector<float> audioBuffer;

        panner.set_point(0.0f, 0.0f); // Center X
        
        // Warm up
        measurePannerOutput(panner, 2, blockSize, source);

        for (float y = -1.0f; y <= 1.0f; y += 0.05f)
        {
            panner.set_point(0.0f, y);
            
            // Run blocks for audio and smoothing settling
            // 1024 samples * 3 = 3072 samples > 2205 (50ms smoothing)
            for(int i=0; i<3; ++i) 
            {
                juce::AudioBuffer<float> inputBuffer(1, blockSize);
                auto* inWrite = inputBuffer.getWritePointer(0);
                for (int s = 0; s < blockSize; ++s) inWrite[s] = source.nextWithGain();

                juce::AudioBuffer<float> outputBuffer(2, blockSize);
                outputBuffer.clear();

                const float* inputPtrs[] = { inputBuffer.getReadPointer(0) };
                float* outputPtrs[] = { outputBuffer.getWritePointer(0), outputBuffer.getWritePointer(1) };

                panner.process_block(inputPtrs, 1, outputPtrs, 2, blockSize);

                for(int s=0; s<blockSize; ++s)
                {
                    audioBuffer.push_back(outputBuffer.getSample(0, s));
                    audioBuffer.push_back(outputBuffer.getSample(1, s));
                }
            }
            
            auto rms = measurePannerOutput(panner, 2, blockSize, source);
            writer.writeRow(y, rms[0], rms[1]);
        }
        
        audioWriter.write(audioBuffer);
    }

    void testQuadPannerSweep()
    {
        QuadPanner panner;
        SineWave source;
        int blockSize = 1024;
        
        TestUtils::CsvWriter writer("quad_panner_sweep", {"Time", "PanX", "PanY", "FL", "FR", "BL", "BR"});

        int steps = 100;
        for (int i = 0; i < steps; ++i)
        {
            float angle = (float)i / steps * juce::MathConstants<float>::twoPi;
            float radius = 0.5f;
            float panX = 0.5f + std::cos(angle) * radius; 
            float panY = 0.5f + std::sin(angle) * radius;
            
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
        int blockSize = 1024;
        
        // Manual CSV writing for variable channels
        juce::File outputDir = juce::File::getCurrentWorkingDirectory().getChildFile("tests/output");
        if (!outputDir.exists()) outputDir.createDirectory();
        juce::File csvFile = outputDir.getChildFile("cleat_panner_sweep.csv");
        std::ofstream ofs(csvFile.getFullPathName().toStdString());
        
        ofs << "Time,PanX,PanY";
        for (int i=0; i<16; ++i) ofs << ",Ch" << i;
        ofs << "\n";

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
        int blockSize = 4096; 
        std::mt19937 rng(1234);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (int i = 0; i < 20; ++i)
        {
            float pan = dist(rng);
            panner.set_pan(pan);
            auto rms = measurePannerOutput(panner, 2, blockSize, source);
            
            float l = rms[0];
            float r = rms[1];
            float distL = pan;
            float distR = 1.0f - pan;
            
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

        struct Point { float x, y; };
        Point speakers[4] = { {0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f} };

        for (int i = 0; i < 20; ++i)
        {
            float x = dist(rng);
            float y = dist(rng);
            panner.set_pan(x, y);
            auto rms = measurePannerOutput(panner, 4, blockSize, source);
            
            int closestIdx = -1;
            float minDistance = 100.0f;
            for (int ch = 0; ch < 4; ++ch) {
                float dx = x - speakers[ch].x;
                float dy = y - speakers[ch].y;
                float d = std::sqrt(dx*dx + dy*dy);
                if (d < minDistance) { minDistance = d; closestIdx = ch; }
            }
            
            float maxRMS = 0.0f;
            for (float val : rms) if (val > maxRMS) maxRMS = val;
            expectWithinAbsoluteError(rms[closestIdx], maxRMS, 0.05f * maxRMS, 
                "Closest speaker " + juce::String(closestIdx) + " should have max RMS");
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

        struct Point { float x, y; };
        std::vector<Point> speakers(16);
        for (int i = 0; i < 16; ++i) {
            int row = i / 4; int col = i % 4;
            speakers[i].x = col / 3.0f; speakers[i].y = row / 3.0f;
        }

        for (int i = 0; i < 20; ++i)
        {
            float x = dist(rng);
            float y = dist(rng);
            panner.set_pan(x, y);
            
            {
                 juce::AudioBuffer<float> dummyIn(1, 44100);
                 juce::AudioBuffer<float> dummyOut(16, 44100);
                 const float* in[] = {dummyIn.getReadPointer(0)};
                 float* out[16]; for(int k=0; k<16; ++k) out[k] = dummyOut.getWritePointer(k);
                 panner.process_block(in, 1, out, 16, 44100); 
            }

            auto rms = measurePannerOutput(panner, 16, blockSize, source);
            
            int closestIdx = -1;
            float minDistance = 100.0f;
            for (int ch = 0; ch < 16; ++ch) {
                float dx = x - speakers[ch].x;
                float dy = y - speakers[ch].y;
                float d = std::sqrt(dx*dx + dy*dy);
                if (d < minDistance) { minDistance = d; closestIdx = ch; }
            }
            
            float maxRMS = 0.0f;
            for (float val : rms) if (val > maxRMS) maxRMS = val;
            expectWithinAbsoluteError(rms[closestIdx], maxRMS, 0.05f * maxRMS, 
                "Closest speaker " + juce::String(closestIdx) + " should have max RMS");
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
