#include <juce_core/juce_core.h>
#include "flowerjuce/DSP/LfoUGen.h"
#include "TestUtils.h"

class LfoTests : public juce::UnitTest
{
public:
    LfoTests() : juce::UnitTest("LfoTests") {}

    void runTest() override
    {
        beginTest("Basic Waveforms");
        testBasicWaveforms();

        beginTest("Width Parameters");
        testWidthParameters();

        beginTest("Level and Polarity");
        testLevelAndPolarity();

        beginTest("Clocked Mode");
        testClockedMode();

        beginTest("Euclidean Rhythms");
        testEuclideanRhythms();

        beginTest("Random Skip");
        testRandomSkip();

        beginTest("Slop/Humanization");
        testSlopHumanization();

        beginTest("Delay Parameter");
        testDelayParameter();

        beginTest("Reproducibility");
        testReproducibility();

        beginTest("Scale Quantization");
        testScaleQuantization();

        beginTest("Loop Functionality");
        testLoopFunctionality();

        beginTest("Combined Features: Pentatonic, Loop 16, Skip 50%, Div 4 (16th notes)");
        testCombinedFeatures();
    }

private:
    void testCombinedFeatures()
    {
        flower::LayerCakeLfoUGen lfo;
        lfo.set_mode(flower::LfoWaveform::Random); 
        lfo.set_clock_division(4.0f); // 4 steps per beat (16th notes) - Faster clock
        lfo.set_pattern_length(16); // Loop length 16 steps
        lfo.set_random_skip(0.5f); // 50% skip chance
        lfo.set_scale(flower::LfoScale::PentatonicMajor);
        lfo.set_quantize_range(24.0f); // 2 octaves range
        lfo.set_bipolar(true);
        lfo.set_level(1.0f);
        
        uint64_t seed = 12345;
        lfo.set_random_seed(seed);

        std::string testName = "combined_pentatonic_loop16_skip50_div4";
        const double duration = 20.0; // 20 seconds
        const double bpm = 120.0;
        const double beatsPerSec = bpm / 60.0;
        // High resolution for plot to capture the 16th note steps clearly
        const double dt = 0.002; 

        // Verification: Check that skips and values repeat every 16 steps
        // We need to run it step by step. Since div=4, one step is 0.25 beats.
        std::vector<float> values;
        double beat = 0.0;
        double stepSizeBeats = 1.0 / 4.0; 
        
        for (int i = 0; i < 32; ++i) // 2 full loops (32 steps)
        {
            // We sample slightly into the step to avoid edge cases at exactly 0.0
            values.push_back(lfo.advance_clocked(beat + 0.01));
            beat += stepSizeBeats;
        }

        // Verify loop structure (values match after 16 steps, accounting for sample-and-hold drift)
        for (int i = 0; i < 16; ++i)
        {
            float val1 = values[i];
            float val2 = values[i+16];
            
            if (std::abs(val1 - val2) > 0.0001f)
            {
                // Mismatch detected. This is only allowed if BOTH are skips holding different history.
                // val[i] should hold val[i-1] (or 0 if i=0)
                float prev1 = (i == 0) ? 0.0f : values[i-1]; 
                // val[i+16] should hold val[i+15]
                float prev2 = values[i+15];
                
                bool isHold1 = (std::abs(val1 - prev1) < 0.0001f);
                bool isHold2 = (std::abs(val2 - prev2) < 0.0001f);
                
                if (!isHold1 || !isHold2)
                {
                    // If either is NOT a hold, then we have a real divergence (one hit vs one skip, or different hits)
                     expectEquals(val1, val2, "Step " + juce::String(i) + " mismatch: val1=" + juce::String(val1) + " val2=" + juce::String(val2) + 
                                  " (isHold1=" + juce::String(isHold1 ? 1 : 0) + ", isHold2=" + juce::String(isHold2 ? 1 : 0) + ")");
                }
            }
        }
        
        // Reset for visualization/audio
        lfo.set_random_seed(seed);
        lfo.reset_phase(0.0);
        lfo.sync_time(0.0);

        TestUtils::CsvWriter writer(testName, {"Time", "Value"});
        
        for (double t = 0.0; t <= duration; t += dt)
        {
            double currentBeat = t * beatsPerSec;
            float val = lfo.advance_clocked(currentBeat);
            writer.writeRow(t, val);
        }

        // Generate audio 
        // Map to frequency range 220-880Hz (2 octaves) to align with quantize range
        lfo.set_random_seed(seed);
        generateTestAudio(testName, lfo, duration, 220.0f, 880.0f);
    }

    void testLoopFunctionality()
    {
        std::vector<int> loopLengths = {4, 8, 16};
        const double duration = 20.0; // seconds requested by user

        for (int length : loopLengths)
        {
            flower::LayerCakeLfoUGen lfo;
            lfo.set_mode(flower::LfoWaveform::Random);
            lfo.set_clock_division(1.0f); // 1 step per beat
            lfo.set_pattern_length(length);
            lfo.set_random_seed(12345);

            std::string testName = "loop_functionality_" + std::to_string(length);

            // Verification Part (Logic check)
            // We need enough steps to verify at least one full loop and a bit more
            // For length 16, we need at least 17 steps to see repetition at index 0 and 16.
            // Let's grab 2 * length + 4 steps to be safe and thorough.
            std::vector<float> stepValues;
            double beat = 0.0;
            int stepsToRecord = 2 * length + 4; 
            
            for (int i = 0; i < stepsToRecord; ++i)
            {
                float val = lfo.advance_clocked(beat);
                stepValues.push_back(val);
                beat += 1.0;
            }
            
            // Verify loop structure: value at i should match value at i + length
            for (int i = 0; i < length; ++i)
            {
                expectEquals(stepValues[i], stepValues[i + length], 
                             "Step " + juce::String(i) + " and " + juce::String(i + length) + " should match for loop length " + juce::String(length));
            }

            // Verify randomness (check a few adjacent steps don't match)
            // In a random sequence, it's possible but unlikely adjacent are same.
            // We'll check a few.
            int adjacentDiffs = 0;
            for (int i = 0; i < length - 1; ++i)
            {
                if (std::abs(stepValues[i] - stepValues[i+1]) > 0.0001f)
                    adjacentDiffs++;
            }
            expectGreaterThan(adjacentDiffs, 0, "Some adjacent steps should differ in random mode");


            // Visualization Part (CSV for Plot matching Audio)
            // Reset LFO to ensure plot matches the sequence we just verified
            lfo.set_random_seed(12345);
            lfo.reset_phase(0.0);
            
            TestUtils::CsvWriter writer(testName, {"Time", "Value"});
            
            const double bpm = 120.0;
            const double beatsPerSec = bpm / 60.0;
            const double dt = 0.005; // 200Hz resolution
            
            for (double t = 0.0; t <= duration; t += dt)
            {
                double currentBeat = t * beatsPerSec;
                float val = lfo.advance_clocked(currentBeat);
                writer.writeRow(t, val);
            }

            // Generate audio 
            lfo.set_random_seed(12345);
            generateTestAudio(testName, lfo, duration);
        }
    }

    // Helper to generate audio file for a test case
    // Generates a sine wave modulated by the LFO
    void generateTestAudio(const std::string& name, flower::LayerCakeLfoUGen& lfo, double durationSec = 2.0, float minFreq = 220.0f, float maxFreq = 440.0f)
    {
        // Save LFO state to restore it
        flower::LayerCakeLfoUGen lfoCopy = lfo;
        
        const double sampleRate = 44100.0;
        const double dt = 1.0 / sampleRate;
        const int numSamples = static_cast<int>(durationSec * sampleRate);
        
        std::vector<float> audioBuffer;
        audioBuffer.reserve(static_cast<size_t>(numSamples));
        
        double phase = 0.0;
        
        // If in clocked mode (rate_hz <= 0 or relying on advance_clocked), we need to simulate beats
        // For simplicity, assume 120 BPM -> 2 beats per second
        const double bpm = 120.0;
        const double beatsPerSec = bpm / 60.0;
        double currentBeat = 0.0;

        // We need to know if we should use advance() or advance_clocked()
        // Check rate_hz or just check if we were using clocked mode in the test
        // A simple heuristic: if rate_hz is 0.5 (default) but we're in a clocked test context, we might prefer clocked.
        // However, LfoUGen is stateless regarding "mode" - it just responds to calls.
        // We'll assume clocked mode if the name implies it, otherwise time-based.
        bool useClocked = (name.find("clocked") != std::string::npos || 
                           name.find("euclidean") != std::string::npos ||
                           name.find("slop") != std::string::npos ||
                           name.find("delay") != std::string::npos ||
                           name.find("skip") != std::string::npos ||
                           name.find("loop") != std::string::npos);

        lfoCopy.reset_phase(0.0);
        lfoCopy.sync_time(0.0); // Reset time base

        for (int i = 0; i < numSamples; ++i)
        {
            float lfoValue = 0.0f;
            
            if (useClocked)
            {
                lfoValue = lfoCopy.advance_clocked(currentBeat);
                currentBeat += beatsPerSec * dt;
            }
            else
            {
                double timeMs = static_cast<double>(i) * dt * 1000.0;
                lfoValue = lfoCopy.advance(timeMs);
            }
            
            // Map LFO [-1, 1] (or [0, 1]) to Frequency [minFreq, maxFreq]
            float freq = 0.0f;
            if (lfoCopy.get_bipolar())
                freq = juce::jmap(lfoValue, -1.0f, 1.0f, minFreq, maxFreq);
            else
                freq = juce::jmap(lfoValue, 0.0f, 1.0f, minFreq, maxFreq);
                
            // Advance sine phase
            phase += (freq * dt);
            if (phase > 1.0) phase -= 1.0;
            
            float sample = std::sin(phase * juce::MathConstants<double>::twoPi);
            audioBuffer.push_back(sample * 0.5f); // -6dB output
        }
        
        TestUtils::AudioWriter writer(name, sampleRate);
        writer.write(audioBuffer);
    }

    void testBasicWaveforms()
    {
        flower::LayerCakeLfoUGen lfo;
        lfo.set_bipolar(true);
        lfo.set_level(1.0f);
        lfo.set_width(0.5f);

        const double sampleRate = 100.0; 
        const double delta = 1.0 / sampleRate;
        
        struct WaveformConfig {
            flower::LfoWaveform type;
            std::string name;
        };

        std::vector<WaveformConfig> waveforms = {
            {flower::LfoWaveform::Sine, "Sine"},
            {flower::LfoWaveform::Triangle, "Triangle"},
            {flower::LfoWaveform::Square, "Square"},
            {flower::LfoWaveform::Gate, "Gate"},
            {flower::LfoWaveform::Envelope, "Envelope"},
            {flower::LfoWaveform::Random, "Random"},
            {flower::LfoWaveform::SmoothRandom, "SmoothRandom"}
        };
        
        // Request: plot different frequencies in different subplots
        // We will generate data for 1Hz and 5Hz
        std::vector<float> rates = {1.0f, 5.0f};

        for (const auto& config : waveforms)
        {
            lfo.set_mode(config.type);
            
            // CSV for plotting (contains data for both rates)
            TestUtils::CsvWriter writer("basic_waveform_" + config.name, {"Time", "Value_1Hz", "Value_5Hz"});
            
            // Run two parallel simulations for CSV
            flower::LayerCakeLfoUGen lfo1 = lfo; lfo1.set_rate_hz(rates[0]); lfo1.reset_phase(0.0); lfo1.sync_time(0.0);
            flower::LayerCakeLfoUGen lfo2 = lfo; lfo2.set_rate_hz(rates[1]); lfo2.reset_phase(0.0); lfo2.sync_time(0.0);

            // Only plot 1 second to see detail
            double time = 0.0;
            for (int i = 0; i <= 100; ++i) 
            {
                // Using advance() with time
                float val1 = lfo1.advance(time * 1000.0);
                float val2 = lfo2.advance(time * 1000.0);
                
                writer.writeRow(time, val1, val2);
                time += delta;
            }
            
            // Generate audio (using 5Hz for audible modulation)
            lfo.set_rate_hz(5.0f);
            // Reset random seed for audio generation so it aligns with expectations (though time base differs from plot)
            lfo.set_random_seed(12345);
            generateTestAudio("basic_waveform_" + config.name, lfo, 1.0);
        }
    }

    void testWidthParameters()
    {
        flower::LayerCakeLfoUGen lfo;
        lfo.set_rate_hz(1.0f);
        lfo.set_bipolar(true);
        
        const double sampleRate = 100.0;
        const double delta = 1.0 / sampleRate;

        // Request: 10, 25, 75, 90
        std::vector<float> widths = {0.10f, 0.25f, 0.75f, 0.90f};
        std::vector<flower::LfoWaveform> types = {
            flower::LfoWaveform::Sine,
            flower::LfoWaveform::Triangle,
            flower::LfoWaveform::Square
        };

        for (auto type : types)
        {
            std::string typeName;
            if (type == flower::LfoWaveform::Sine) typeName = "Sine";
            else if (type == flower::LfoWaveform::Triangle) typeName = "Triangle";
            else typeName = "Square";

            lfo.set_mode(type);

            for (float width : widths)
            {
                lfo.set_width(width);
                lfo.reset_phase(0.0);
                lfo.sync_time(0.0);
                
                int widthInt = static_cast<int>(width * 100);
                std::string filename = "width_" + typeName + "_" + std::to_string(widthInt);
                TestUtils::CsvWriter writer(filename, {"Time", "Value"});
                
                double time = 0.0;
                for (int i = 0; i <= 100; ++i)
                {
                    float val = lfo.advance(time * 1000.0);
                    writer.writeRow(time, val);
                    time += delta;
                }
                
                // Audio
                generateTestAudio(filename, lfo, 1.0);
            }
        }
    }

    void testLevelAndPolarity()
    {
        flower::LayerCakeLfoUGen lfo;
        lfo.set_mode(flower::LfoWaveform::Sine);
        lfo.set_rate_hz(1.0f);
        lfo.set_width(0.5f);

        const double delta = 0.01;

        // Generate data for plot
        TestUtils::CsvWriter writer("level_polarity", {"Time", "Bipolar_1.0", "Bipolar_0.5", "Unipolar_1.0"});
        
        flower::LayerCakeLfoUGen lfoB1 = lfo; lfoB1.set_bipolar(true); lfoB1.set_level(1.0f); lfoB1.reset_phase(0.0); lfoB1.sync_time(0.0);
        flower::LayerCakeLfoUGen lfoB05 = lfo; lfoB05.set_bipolar(true); lfoB05.set_level(0.5f); lfoB05.reset_phase(0.0); lfoB05.sync_time(0.0);
        flower::LayerCakeLfoUGen lfoU1 = lfo; lfoU1.set_bipolar(false); lfoU1.set_level(1.0f); lfoU1.reset_phase(0.0); lfoU1.sync_time(0.0);
        
        double time = 0.0;
        for (int i=0; i<=100; ++i)
        {
            float b1 = lfoB1.advance(time * 1000.0);
            float b05 = lfoB05.advance(time * 1000.0);
            float u1 = lfoU1.advance(time * 1000.0);
            
            writer.writeRow(time, b1, b05, u1);
            time += delta;
        }
        
        // Audio for Bipolar 1.0 (Reference)
        generateTestAudio("level_polarity", lfoB1, 1.0);
    }

    void testClockedMode()
    {
        flower::LayerCakeLfoUGen lfo;
        lfo.set_mode(flower::LfoWaveform::Square);
        lfo.set_bipolar(false);
        lfo.set_width(0.5f);
        
        TestUtils::CsvWriter writer("clocked_mode", {"Beat", "Div_1.0", "Div_0.25", "Div_4.0"});

        double beat = 0.0;
        double deltaBeat = 0.05;
        
        flower::LayerCakeLfoUGen lfo1 = lfo; lfo1.set_clock_division(1.0f);
        flower::LayerCakeLfoUGen lfo025 = lfo; lfo025.set_clock_division(0.25f);
        flower::LayerCakeLfoUGen lfo4 = lfo; lfo4.set_clock_division(4.0f);

        for (int i = 0; i < 16 * 20; ++i)
        {
            float v1 = lfo1.advance_clocked(beat);
            float v025 = lfo025.advance_clocked(beat);
            float v4 = lfo4.advance_clocked(beat);

            writer.writeRow(beat, v1, v025, v4);
            beat += deltaBeat;
        }
        
        // Audio for Div 4.0 (fastest)
        generateTestAudio("clocked_mode", lfo4, 8.0);
    }

    void testEuclideanRhythms()
    {
        flower::LayerCakeLfoUGen lfo;
        lfo.set_mode(flower::LfoWaveform::Gate);
        lfo.set_bipolar(false); 
        lfo.set_clock_division(4.0f); // 16th notes
        
        // E(3,8) Tresillo
        lfo.set_euclidean_steps(8);
        lfo.set_euclidean_triggers(3);
        
        TestUtils::CsvWriter writer("euclidean_3_8", {"Beat", "Value", "Step"});
        
        double beat = 0.0;
        double deltaBeat = 0.05;
        
        for (int i = 0; i < 8 * 20; ++i)
        {
            float val = lfo.advance_clocked(beat);
            int step = static_cast<int>(beat * 4.0f); 
            writer.writeRow(beat, val, step % 8);
            beat += deltaBeat;
        }
        
        generateTestAudio("euclidean_3_8", lfo, 4.0);
    }

    void testRandomSkip()
    {
        flower::LayerCakeLfoUGen lfo;
        // Use Sine for better audio/visual verification of skipping (silence/zero on skip)
        lfo.set_mode(flower::LfoWaveform::Sine);
        lfo.set_bipolar(true);
        lfo.set_clock_division(1.0f);
        lfo.set_random_seed(999);
        lfo.set_pattern_length(16); 
        lfo.set_random_skip(0.5f);
        // Offset phase so we hold a non-zero value when skipping (end of cycle is usually 0)
        lfo.set_phase_offset(0.25f); 

        TestUtils::CsvWriter writer("random_skip", {"Beat", "Value"});
        
        double beat = 0.0;
        double delta = 0.05; // High res to see sine wave
        
        // 16 beats to see full pattern
        for (int i = 0; i < 16 * 20; ++i)
        {
            float val = lfo.advance_clocked(beat);
            writer.writeRow(beat, val);
            beat += delta;
        }
        
        // Reset seed so audio matches plot
        lfo.set_random_seed(999);
        generateTestAudio("random_skip", lfo, 8.0);
    }

    void testSlopHumanization()
    {
        flower::LayerCakeLfoUGen lfo;
        lfo.set_mode(flower::LfoWaveform::Gate);
        lfo.set_bipolar(false);
        lfo.set_clock_division(1.0f);
        lfo.set_slop(0.5f); 
        lfo.set_random_seed(123);

        TestUtils::CsvWriter writer("slop_humanization", {"Beat", "Value", "NoSlop"});
        
        flower::LayerCakeLfoUGen cleanLfo = lfo;
        cleanLfo.set_slop(0.0f);

        double beat = 0.0;
        double delta = 0.01; // High res

        for (int i = 0; i < 400; ++i)
        {
            float val = lfo.advance_clocked(beat);
            float cleanVal = cleanLfo.advance_clocked(beat);
            writer.writeRow(beat, val, cleanVal);
            beat += delta;
        }
        
        // Reset seed so audio matches plot
        lfo.set_random_seed(123);
        generateTestAudio("slop_humanization", lfo, 2.0);
    }

    void testDelayParameter()
    {
        flower::LayerCakeLfoUGen lfo;
        lfo.set_mode(flower::LfoWaveform::Envelope);
        lfo.set_bipolar(false);
        lfo.set_clock_division(1.0f);
        lfo.set_delay(0.5f);

        TestUtils::CsvWriter writer("delay_param", {"Beat", "Value", "NoDelay"});

        flower::LayerCakeLfoUGen cleanLfo = lfo;
        cleanLfo.set_delay(0.0f);
        
        double beat = 0.0;
        double delta = 0.01;
        
        for (int i = 0; i < 400; ++i)
        {
            float val = lfo.advance_clocked(beat);
            float cleanVal = cleanLfo.advance_clocked(beat);
            writer.writeRow(beat, val, cleanVal);
            beat += delta;
        }
        
        generateTestAudio("delay_param", lfo, 2.0);
    }

    void testReproducibility()
    {
        flower::LayerCakeLfoUGen lfo1;
        flower::LayerCakeLfoUGen lfo2;
        
        lfo1.set_mode(flower::LfoWaveform::Random);
        lfo2.set_mode(flower::LfoWaveform::Random);
        
        uint64_t seed = 42;
        lfo1.set_random_seed(seed);
        lfo2.set_random_seed(seed);
        lfo1.set_pattern_length(8);
        lfo2.set_pattern_length(8);

        TestUtils::CsvWriter writer("reproducibility", {"Step", "LFO1", "LFO2"});

        for (int i = 0; i < 16; ++i)
        {
            float v1 = lfo1.advance_clocked(static_cast<double>(i));
            float v2 = lfo2.advance_clocked(static_cast<double>(i));
            writer.writeRow(i, v1, v2);
            expectEquals(v1, v2, "LFOs with same seed should match");
        }
        
        // Reset seed for audio
        lfo1.set_random_seed(seed);
        generateTestAudio("reproducibility", lfo1, 8.0);
    }

    void testScaleQuantization()
    {
        flower::LayerCakeLfoUGen lfo;
        lfo.set_mode(flower::LfoWaveform::Triangle); // Linear ramp makes steps obvious
        lfo.set_bipolar(true); // -1 to 1
        lfo.set_level(1.0f);
        
        // Set range to match 2 octaves (+/- 12 semitones = 24 total span)
        // This aligns with the audio mapping 220Hz -> 880Hz (2 octaves)
        // So each quantized semitone step in LFO logic corresponds to a real semitone in audio.
        lfo.set_quantize_range(12.0f);
        
        // Case 1: Chromatic Scale (steps of 1 semitone)
        
        lfo.set_scale(flower::LfoScale::Chromatic);
        lfo.reset_phase(0.0); // Starts at 0 (0 semitones)
        lfo.sync_time(0.0);
        
        flower::LayerCakeLfoUGen rawLfo = lfo;
        rawLfo.set_scale(flower::LfoScale::Off);
        rawLfo.reset_phase(0.0);
        rawLfo.sync_time(0.0);
        
        TestUtils::CsvWriter writer("scale_quantization_chromatic", {"Time", "Quantized", "Raw"});
        
        // 5 second sweep for better audibility
        double delta = 0.005; 
        for (int i = 0; i < 1000; ++i)
        {
            float val = lfo.advance(i * delta * 1000.0);
            float raw = rawLfo.advance(i * delta * 1000.0);
            writer.writeRow(i * delta, val, raw);
        }
        // Map to 220-880Hz (2 octaves) to match the 24-semitone LFO range
        generateTestAudio("scale_quantization_chromatic", lfo, 5.0, 220.0f, 880.0f);
        
        // Case 2: Major Scale (0, 2, 4, 5, 7, 9, 11)
        // Should see larger steps
        lfo.set_scale(flower::LfoScale::Major);
        lfo.reset_phase(0.0);
        lfo.sync_time(0.0);
        
        TestUtils::CsvWriter writer2("scale_quantization_major", {"Time", "Quantized", "Raw"});
        for (int i = 0; i < 1000; ++i)
        {
            float val = lfo.advance(i * delta * 1000.0);
            float raw = rawLfo.advance(i * delta * 1000.0);
            writer2.writeRow(i * delta, val, raw);
        }
        generateTestAudio("scale_quantization_major", lfo, 5.0, 220.0f, 880.0f);
    }
};

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;
    LfoTests tests;
    juce::UnitTestRunner runner;
    runner.runTests({&tests});
    return 0;
}
