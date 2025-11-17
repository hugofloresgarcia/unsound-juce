#include "TokenVisualizer.h"
#include "LooperTrack.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <random>
#include <algorithm>

// Include the generated BinaryData header (created by juce_add_binary_data in CMakeLists.txt)
#include "BinaryData.h"

namespace WhAM
{

// ============================================================================
// Stateless utility functions
// ============================================================================

namespace
{
    constexpr int NUM_TOKEN_ROWS = 13;
    constexpr int SAMPLES_PER_BLOCK = 512;
    constexpr int NUM_VISIBLE_COLUMNS = 100; // Display 100 tokens in time axis

    // Per-coefficient statistics for normalization
    struct MFCCStats
    {
        std::array<float, NUM_TOKEN_ROWS> runningMin;
        std::array<float, NUM_TOKEN_ROWS> runningMax;
        bool initialized = false;
        
        MFCCStats()
        {
            runningMin.fill(0.0f);
            runningMax.fill(1.0f);
        }
        
        void update(const std::array<float, NUM_TOKEN_ROWS>& mfccs)
        {
            if (!initialized)
            {
                runningMin = mfccs;
                runningMax = mfccs;
                initialized = true;
            }
            else
            {
                constexpr float alpha = 0.95f; // Smoothing factor
                for (int i = 0; i < NUM_TOKEN_ROWS; ++i)
                {
                    runningMin[i] = std::min(runningMin[i] * alpha + mfccs[i] * (1.0f - alpha), mfccs[i]);
                    runningMax[i] = std::max(runningMax[i] * alpha + mfccs[i] * (1.0f - alpha), mfccs[i]);
                }
            }
        }
        
        void normalize(std::array<float, NUM_TOKEN_ROWS>& mfccs)
        {
            for (int i = 0; i < NUM_TOKEN_ROWS; ++i)
            {
                float range = runningMax[i] - runningMin[i];
                if (range > 1e-6f)
                {
                    mfccs[i] = (mfccs[i] - runningMin[i]) / range;
                    mfccs[i] = std::clamp(mfccs[i], 0.0f, 1.0f);
                }
                else
                {
                    mfccs[i] = 0.5f; // Middle value if no range
                }
            }
        }
    };
    
    // Global stats for input and output (separate normalization)
    static MFCCStats inputStats;
    static MFCCStats outputStats;
    
    // RMS statistics for normalization
    struct RMSStats
    {
        float runningMin = 0.0f;
        float runningMax = 1.0f;
        bool initialized = false;
        
        void update(float rms)
        {
            if (!initialized)
            {
                runningMin = rms;
                runningMax = rms;
                initialized = true;
            }
            else
            {
                constexpr float alpha = 0.98f; // Slower adaptation for RMS
                runningMin = std::min(runningMin * alpha + rms * (1.0f - alpha), rms);
                runningMax = std::max(runningMax * alpha + rms * (1.0f - alpha), rms);
            }
        }
        
        float normalize(float rms) const
        {
            float range = runningMax - runningMin;
            if (range > 1e-6f)
            {
                return std::clamp((rms - runningMin) / range, 0.0f, 1.0f);
            }
            return 0.5f;
        }
    };
    
    static RMSStats inputRMSStats;
    static RMSStats outputRMSStats;

    // Generate fake tokens for a block
    std::array<int, NUM_TOKEN_ROWS> generateFakeTokens()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(0, 255);
        
        std::array<int, NUM_TOKEN_ROWS> tokens;
        for (int i = 0; i < NUM_TOKEN_ROWS; ++i)
        {
            tokens[i] = dis(gen);
        }
        return tokens;
    }

    // Convert frequency to mel scale
    float hzToMel(float hz)
    {
        return 2595.0f * std::log10f(1.0f + hz / 700.0f);
    }
    
    // Convert mel to frequency
    float melToHz(float mel)
    {
        return 700.0f * (std::powf(10.0f, mel / 2595.0f) - 1.0f);
    }
    
    // Calculate MFCCs from audio samples
    void calculateMFCCs(const float* samples, int numSamples, double sampleRate, std::array<float, NUM_TOKEN_ROWS>& mfccsOut, MFCCStats& stats)
    {
        if (numSamples == 0)
        {
            mfccsOut.fill(0.0f);
            return;
        }
        
        constexpr int numMelFilters = 26;
        constexpr int fftOrder = 11; // 2^11 = 2048 points
        constexpr int fftSize = 1 << fftOrder;
        constexpr float preEmphasisCoeff = 0.97f;
        
        // Apply pre-emphasis filter to amplify high frequencies
        std::vector<float> emphasizedSamples(numSamples);
        emphasizedSamples[0] = samples[0];
        for (int i = 1; i < numSamples; ++i)
        {
            emphasizedSamples[i] = samples[i] - preEmphasisCoeff * samples[i - 1];
        }
        
        // Apply Hamming window and prepare FFT input
        juce::dsp::FFT fft(fftOrder);
        std::vector<float> windowedSamples(fftSize, 0.0f);
        
        // Copy pre-emphasized samples and apply Hamming window
        for (int i = 0; i < numSamples && i < fftSize; ++i)
        {
            float window = 0.54f - 0.46f * std::cosf(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1));
            windowedSamples[i] = emphasizedSamples[i] * window;
        }
        
        // Perform FFT (needs separate input and output buffers)
        std::vector<juce::dsp::Complex<float>> fftInput(fftSize);
        std::vector<juce::dsp::Complex<float>> fftOutput(fftSize);
        for (int i = 0; i < fftSize; ++i)
        {
            fftInput[i] = juce::dsp::Complex<float>(windowedSamples[i], 0.0f);
        }
        
        fft.perform(fftInput.data(), fftOutput.data(), false);
        
        // Calculate power spectrum
        std::vector<float> powerSpectrum(fftSize / 2);
        for (int i = 0; i < fftSize / 2; ++i)
        {
            float real = fftOutput[i].real();
            float imag = fftOutput[i].imag();
            powerSpectrum[i] = real * real + imag * imag;
        }
        
        // Create mel filterbank (skip DC component by starting at 300 Hz)
        constexpr float minFreqHz = 300.0f; // Skip DC and very low frequencies
        float nyquist = static_cast<float>(sampleRate / 2.0);
        float melMax = hzToMel(nyquist);
        float melMin = hzToMel(minFreqHz);
        float melStep = (melMax - melMin) / (numMelFilters + 1);
        
        std::vector<std::vector<float>> melFilters(numMelFilters);
        for (int i = 0; i < numMelFilters; ++i)
        {
            float melCenter = melMin + (i + 1) * melStep;
            float hzCenter = melToHz(melCenter);
            float hzLeft = melToHz(melCenter - melStep);
            float hzRight = melToHz(melCenter + melStep);
            
            melFilters[i].resize(fftSize / 2, 0.0f);
            
            for (int j = 0; j < fftSize / 2; ++j)
            {
                float freq = static_cast<float>(j * sampleRate / fftSize);
                
                if (freq >= hzLeft && freq <= hzRight)
                {
                    if (freq < hzCenter)
                        melFilters[i][j] = (freq - hzLeft) / (hzCenter - hzLeft);
                    else
                        melFilters[i][j] = (hzRight - freq) / (hzRight - hzCenter);
                }
            }
        }
        
        // Apply mel filterbank and take log
        std::vector<float> melEnergies(numMelFilters);
        for (int i = 0; i < numMelFilters; ++i)
        {
            float energy = 0.0f;
            for (int j = 0; j < fftSize / 2; ++j)
            {
                energy += powerSpectrum[j] * melFilters[i][j];
            }
            // Add small epsilon to avoid log(0)
            melEnergies[i] = std::logf(energy + 1e-6f);
        }
        
        // DCT to get MFCCs (simplified - just use first 13 coefficients)
        mfccsOut.fill(0.0f);
        for (int i = 0; i < NUM_TOKEN_ROWS; ++i)
        {
            float sum = 0.0f;
            for (int j = 0; j < numMelFilters; ++j)
            {
                sum += melEnergies[j] * std::cosf(juce::MathConstants<float>::pi * i * (j + 0.5f) / numMelFilters);
            }
            mfccsOut[i] = sum * std::sqrtf(2.0f / numMelFilters);
        }
        
        // Update running statistics and normalize per-coefficient
        stats.update(mfccsOut);
        stats.normalize(mfccsOut);
    }

    // Generate vibrant color for a token using MFCC and RMS values
    // MFCC → Hue & Saturation (spectral content)
    // RMS → Brightness (energy/amplitude)
    juce::Colour generateTokenColor(int tokenIndex, int tokenValue, float mfccValue, float rmsValue, bool isInput)
    {
        // Flip the MFCC value to invert the color mapping
        float invertedMfcc = 1.0f - mfccValue;
        
        float hue;
        
        if (isInput)
        {
            // Input: Warm colors - red to yellow range (0° to 60°)
            // MFCC controls hue: red (0°) to yellow (60°)
            hue = invertedMfcc * 60.0f; // 0° (red) to 60° (yellow)
        }
        else
        {
            // Output: Cool colors - cyan to magenta range (180° to 300°)
            // MFCC controls hue: cyan (180°) to magenta (300°)
            hue = 180.0f + (invertedMfcc * 120.0f); // 180° (cyan) to 300° (magenta)
        }
        
        // Add small randomness based on token value for texture
        float randomOffset = static_cast<float>(tokenValue % 20) - 10.0f; // ±10 degrees
        hue += randomOffset;
        
        // Wrap hue to [0, 360)
        while (hue < 0.0f) hue += 360.0f;
        while (hue >= 360.0f) hue -= 360.0f;
        
        // Add slight variation based on token index
        hue += (tokenIndex % 5) * 2.0f; // Small variation per row
        if (hue >= 360.0f) hue -= 360.0f;
        
        // MFCC controls saturation: high MFCC = more saturated colors
        // Use full dynamic range for visual impact
        float saturation = 0.5f + (invertedMfcc * 0.5f); // 0.5 to 1.0 based on MFCC
        saturation = std::clamp(saturation, 0.4f, 1.0f);
        
        // RMS exclusively controls brightness: high RMS = brighter
        // More subtle brightness variation for better readability
        float brightness = 0.5f + (rmsValue * 0.4f); // 0.5 to 0.9 based on RMS
        brightness = std::clamp(brightness, 0.4f, 0.95f);
        
        return juce::Colour::fromHSV(hue / 360.0f, saturation, brightness, 1.0f);
    }

    // Calculate RMS from audio samples
    float calculateRMS(const float* samples, int numSamples)
    {
        if (numSamples == 0)
            return 0.0f;
        
        float sumSquares = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            sumSquares += samples[i] * samples[i];
        }
        return std::sqrt(sumSquares / numSamples);
    }

    // Process audio block and generate token data with MFCCs and RMS
    float processAudioBlock(const float* samples, int numSamples, double sampleRate, std::array<int, NUM_TOKEN_ROWS>& tokensOut, std::array<float, NUM_TOKEN_ROWS>& mfccsOut, bool isInput)
    {
        tokensOut = generateFakeTokens();
        
        MFCCStats& stats = isInput ? inputStats : outputStats;
        calculateMFCCs(samples, numSamples, sampleRate, mfccsOut, stats);
        
        // Calculate and normalize RMS
        float rms = calculateRMS(samples, numSamples);
        RMSStats& rmsStats = isInput ? inputRMSStats : outputRMSStats;
        rmsStats.update(rms);
        float normalizedRMS = rmsStats.normalize(rms);
        
        return normalizedRMS;
    }
}

// ============================================================================
// State structures
// ============================================================================

struct TokenBlock
{
    std::array<int, NUM_TOKEN_ROWS> tokens;
    std::array<float, NUM_TOKEN_ROWS> mfccs; // One MFCC per row
    float rms; // RMS energy of the block
    
    TokenBlock() : tokens{}, mfccs{}, rms(0.0f) {}
};

struct TokenGridData
{
    std::vector<TokenBlock> blocks;
    int trackIndex;
    
    TokenGridData(int trackIdx) : trackIndex(trackIdx) {}
    
    void addBlock(const TokenBlock& block)
    {
        blocks.push_back(block);
        // Keep only the most recent NUM_VISIBLE_COLUMNS blocks
        if (static_cast<int>(blocks.size()) > NUM_VISIBLE_COLUMNS)
        {
            blocks.erase(blocks.begin());
        }
    }
};

// ============================================================================
// TokenVisualizerComponent - Main visualizer component
// ============================================================================

class TokenVisualizerWindow::TokenVisualizerComponent : public juce::Component,
                                                        public juce::Timer
{
public:
    TokenVisualizerComponent(VampNetMultiTrackLooperEngine& engine, int numTracks, const std::vector<LooperTrack*>& tracks)
        : looperEngine(engine),
          numTracks(numTracks),
          animationFrame(0),
          looperTracks(tracks)  // Copy the vector (pointers remain valid)
    {
        // Initialize grid data for each track (input and output)
        for (int i = 0; i < numTracks; ++i)
        {
            inputGrids.push_back(TokenGridData(i));
            outputGrids.push_back(TokenGridData(i));
            lastInputReadPos.push_back(0.0f);
            lastOutputReadPos.push_back(0.0f);
            lastInputRecordedLength.push_back(0);
            lastOutputRecordedLength.push_back(0);
        }
        
        // Load logo from embedded BinaryData (if available)
        loadLogo();
        
        startTimer(50); // Update every 50ms
    }
    
    ~TokenVisualizerComponent() override
    {
        stopTimer();
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
        
        auto bounds = getLocalBounds().reduced(20);
        
        // Reserve space and draw logo centered at top
        const int logoWidth = 500;
        const int logoHeight = 200;
        const int logoMargin = 10;
        
        auto logoBounds = bounds.removeFromTop(logoHeight + logoMargin);
        auto logoArea = logoBounds.withSizeKeepingCentre(logoWidth, logoHeight);
        
        drawLogoPlaceholder(g, logoArea);
        
        // Add some spacing after the logo area
        bounds.removeFromTop(logoMargin);
        
        const int trackHeight = bounds.getHeight() / numTracks;
        
        for (int trackIdx = 0; trackIdx < numTracks; ++trackIdx)
        {
            auto trackBounds = bounds.removeFromTop(trackHeight).reduced(0, 5);
            
            // Split into input (left) and output (right) with more arrow space
            auto inputSection = trackBounds.removeFromLeft((trackBounds.getWidth() - 120) / 2).reduced(10, 0);
            auto arrowSection = trackBounds.removeFromLeft(120);  // More space for arrows
            auto outputSection = trackBounds.reduced(10, 0);
            
            // Draw input section (warm neon - orange)
            drawSection(g, inputSection, inputGrids[trackIdx], true, trackIdx);
            
            // Draw arrow animation in the middle if generating
            drawArrow(g, arrowSection, trackIdx);
            
            // Draw output section (cool neon - cyan)
            drawSection(g, outputSection, outputGrids[trackIdx], false, trackIdx);
        }
    }
    
    void resized() override
    {
        // Nothing to do - we just paint into our bounds
    }
    
    void timerCallback() override
    {
        updateTokenData();
        animationFrame = (animationFrame + 1) % 60;  // 60-frame animation loop
        repaint();
    }
    
private:
    VampNetMultiTrackLooperEngine& looperEngine;
    int numTracks;
    int animationFrame;
    
    std::vector<TokenGridData> inputGrids;
    std::vector<TokenGridData> outputGrids;
    
    // Track last processed positions for each track (to avoid duplicate blocks)
    std::vector<float> lastInputReadPos;
    std::vector<float> lastOutputReadPos;
    
    // Track previous recorded lengths to detect when buffers are cleared
    std::vector<size_t> lastInputRecordedLength;
    std::vector<size_t> lastOutputRecordedLength;
    
    // Copy of LooperTrack pointers for checking generation state (must be copy, not reference, to avoid dangling reference)
    std::vector<LooperTrack*> looperTracks;
    
    // Logo image (load from file when available)
    juce::Image logoImage;
    
    // Load logo from embedded BinaryData
    void loadLogo()
    {
        // Load the wham.png logo from embedded BinaryData
        // BinaryData is generated by CMake from assets/wham.png
        logoImage = juce::ImageFileFormat::loadFrom(BinaryData::wham_png, 
                                                     static_cast<size_t>(BinaryData::wham_pngSize));
        
        if (!logoImage.isValid())
        {
            // Fallback: Try with ImageCache
            logoImage = juce::ImageCache::getFromMemory(BinaryData::wham_png, 
                                                         BinaryData::wham_pngSize);
        }
    }
    
    // Draw logo or placeholder
    void drawLogoPlaceholder(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        if (logoImage.isValid())
        {
            // Draw actual logo image, scaled to fit within bounds while maintaining aspect ratio
            float imageAspect = static_cast<float>(logoImage.getWidth()) / logoImage.getHeight();
            float boundsAspect = static_cast<float>(bounds.getWidth()) / bounds.getHeight();
            
            juce::Rectangle<float> imageBounds = bounds.toFloat();
            
            if (imageAspect > boundsAspect)
            {
                // Image is wider - fit to width
                float scaledHeight = bounds.getWidth() / imageAspect;
                imageBounds.setHeight(scaledHeight);
                imageBounds.setCentre(bounds.toFloat().getCentre());
            }
            else
            {
                // Image is taller - fit to height
                float scaledWidth = bounds.getHeight() * imageAspect;
                imageBounds.setWidth(scaledWidth);
                imageBounds.setCentre(bounds.toFloat().getCentre());
            }
            
            g.drawImage(logoImage, imageBounds);
        }
        else
        {
            // Draw placeholder with neon border
            g.setColour(juce::Colour(0xff888888).withAlpha(0.3f));
            g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
            
            g.setColour(juce::Colour(0xffaaaaaa));
            g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.5f);
            
            // Draw "LOGO" text in the center
            g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
            g.setColour(juce::Colour(0xffaaaaaa).withAlpha(0.6f));
            g.drawText("LOGO", bounds, juce::Justification::centred);
        }
    }
    
    // Draw a section with neon box, waveform, and tokens
    void drawSection(juce::Graphics& g, juce::Rectangle<int> bounds, const TokenGridData& gridData, bool isInput, int trackIdx)
    {
        // Draw neon rounded box (outline only, no background)
        juce::Colour neonColor = isInput ? juce::Colour(0xffff6600) : juce::Colour(0xff00ccff);  // Orange or cyan
        g.setColour(neonColor);
        g.drawRoundedRectangle(bounds.toFloat().reduced(2.0f), 8.0f, 2.0f);
        
        auto contentBounds = bounds.reduced(10);
        
        // Label
        g.setColour(neonColor);
        g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        auto labelBounds = contentBounds.removeFromTop(20);
        g.drawText(isInput ? "INPUT" : "OUTPUT", labelBounds, juce::Justification::centredLeft);
        g.drawText("track " + juce::String(trackIdx + 1), labelBounds, juce::Justification::centredRight);
        
        contentBounds.removeFromTop(5);
        
        // Waveform (top 40%)
        auto waveformBounds = contentBounds.removeFromTop(contentBounds.getHeight() * 0.4f);
        drawWaveform(g, waveformBounds, trackIdx, isInput);
        
        contentBounds.removeFromTop(5);
        
        // Tokens (bottom 60%)
        drawTokenGrid(g, contentBounds, gridData, isInput);
    }
    
    // Draw waveform for a track (streaming, time-aligned with tokens)
    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds, int trackIdx, bool isInput)
    {
        auto& track = looperEngine.getTrack(trackIdx);
        auto& tapeLoop = isInput ? track.recordBuffer : track.outputBuffer;
        
        const juce::ScopedLock sl(tapeLoop.m_lock);
        
        size_t totalRecorded = tapeLoop.m_recorded_length.load();
        
        if (totalRecorded == 0 || tapeLoop.get_buffer().empty())
        {
            // Draw empty placeholder
            g.setColour(juce::Colour(0xff333333));
            g.drawRect(bounds);
            return;
        }
        
        auto& buffer = tapeLoop.get_buffer();
        
        // Calculate time window to match NUM_VISIBLE_COLUMNS token blocks
        // Each token block represents SAMPLES_PER_BLOCK (512) consecutive samples
        // The waveform should show samples corresponding to NUM_VISIBLE_COLUMNS token blocks
        size_t samplesToShow = NUM_VISIBLE_COLUMNS * SAMPLES_PER_BLOCK;
        
        if (totalRecorded == 0)
        {
            g.setColour(juce::Colour(0xff333333));
            g.drawRect(bounds);
            return;
        }
        
        // Get the read head position - this is where tokens are being extracted from
        auto& readHead = isInput ? track.recordReadHead : track.outputReadHead;
        float currentReadPos = readHead.get_pos();
        
        // Show the waveform window ending at the current read head position
        // This matches exactly where the tokens are being extracted from
        size_t displayEndSample = static_cast<size_t>(currentReadPos);
        size_t displayStartSample;
        
        if (displayEndSample >= samplesToShow)
        {
            // We have enough data to show a full window
            displayStartSample = displayEndSample - samplesToShow;
        }
        else
        {
            // Show from beginning up to current position
            displayStartSample = 0;
        }
        
        size_t displayLength = displayEndSample - displayStartSample;
        
        // Debug output (only occasionally to avoid spam)
        static int debugCounter = 0;
        if (debugCounter++ % 100 == 0)
        {
            DBG("Track " + juce::String(trackIdx) + " " + juce::String(isInput ? "INPUT" : "OUTPUT") +
                " - samplesToShow: " + juce::String(samplesToShow) +
                ", totalRecorded: " + juce::String(totalRecorded) +
                ", displayLength: " + juce::String(displayLength) +
                ", bufferSize: " + juce::String(buffer.size()) +
                ", displayStart: " + juce::String(displayStartSample) +
                ", displayEnd: " + juce::String(displayEndSample));
        }
        
        if (displayLength == 0)
        {
            g.setColour(juce::Colour(0xff333333));
            g.drawRect(bounds);
            return;
        }
        
        // Draw waveform - streaming from left to right
        juce::Colour waveformColor = isInput ? juce::Colour(0xffff8844) : juce::Colour(0xff44ddff);
        g.setColour(waveformColor.withAlpha(0.6f));
        
        const int numPoints = bounds.getWidth();
        const float samplesPerPixel = static_cast<float>(displayLength) / numPoints;
        
        juce::Path waveformPath;
        waveformPath.startNewSubPath(bounds.getX(), bounds.getCentreY());
        
        // Draw top half of waveform
        for (int x = 0; x < numPoints; ++x)
        {
            size_t startSample = displayStartSample + static_cast<size_t>(x * samplesPerPixel);
            size_t endSample = displayStartSample + static_cast<size_t>((x + 1) * samplesPerPixel);
            endSample = std::min(endSample, displayEndSample);
            
            float minVal = 0.0f;
            float maxVal = 0.0f;
            
            for (size_t i = startSample; i < endSample; ++i)
            {
                // Handle circular buffer wrap
                size_t bufferIndex = i % buffer.size();
                float sample = buffer[bufferIndex];
                minVal = std::min(minVal, sample);
                maxVal = std::max(maxVal, sample);
            }
            
            float maxY = bounds.getCentreY() - (maxVal * bounds.getHeight() * 0.5f);
            waveformPath.lineTo(bounds.getX() + x, maxY);
        }
        
        // Draw bottom half of waveform
        for (int x = numPoints - 1; x >= 0; --x)
        {
            size_t startSample = displayStartSample + static_cast<size_t>(x * samplesPerPixel);
            size_t endSample = displayStartSample + static_cast<size_t>((x + 1) * samplesPerPixel);
            endSample = std::min(endSample, displayEndSample);
            
            float minVal = 0.0f;
            
            for (size_t i = startSample; i < endSample; ++i)
            {
                // Handle circular buffer wrap
                size_t bufferIndex = i % buffer.size();
                float sample = buffer[bufferIndex];
                minVal = std::min(minVal, sample);
            }
            
            float minY = bounds.getCentreY() - (minVal * bounds.getHeight() * 0.5f);
            waveformPath.lineTo(bounds.getX() + x, minY);
        }
        
        waveformPath.closeSubPath();
        g.fillPath(waveformPath);
        
        // Draw playhead at the right edge (since we're showing up to last processed position)
        if (track.isPlaying.load() && displayLength > 0)
        {
            // Playhead is always at the right edge in streaming mode
            float playheadX = bounds.getRight();
            
            g.setColour(waveformColor);
            g.drawLine(playheadX, bounds.getY(), playheadX, bounds.getBottom(), 2.0f);
        }
    }
    
    // Draw token grid
    void drawTokenGrid(juce::Graphics& g, juce::Rectangle<int> bounds, const TokenGridData& gridData, bool isInput)
    {
        if (gridData.blocks.empty())
        {
            g.setColour(juce::Colour(0xff333333));
            g.drawRect(bounds);
            return;
        }
        
        const int numColumns = static_cast<int>(gridData.blocks.size());
        const float columnWidth = static_cast<float>(bounds.getWidth()) / numColumns;
        const float rowHeight = static_cast<float>(bounds.getHeight()) / NUM_TOKEN_ROWS;
        
        for (int col = 0; col < numColumns; ++col)
        {
            const auto& block = gridData.blocks[col];
            const float x = bounds.getX() + (col * columnWidth);
            
            for (int row = 0; row < NUM_TOKEN_ROWS; ++row)
            {
                const float y = bounds.getY() + (row * rowHeight);
                const juce::Rectangle<float> rect(x, y, columnWidth, rowHeight);
                
                // Use MFCC and RMS values to determine hue, saturation, and brightness
                juce::Colour color = generateTokenColor(row, block.tokens[row], block.mfccs[row], block.rms, isInput);
                g.setColour(color);
                g.fillRect(rect);
            }
        }
    }
    
    // Draw animated arrow
    void drawArrow(juce::Graphics& g, juce::Rectangle<int> bounds, int trackIdx)
    {
        // Check if generation is in progress using LooperTrack's isGenerating() method
        bool isGenerating = false;
        if (trackIdx >= 0 && trackIdx < static_cast<int>(looperTracks.size()) && looperTracks[trackIdx] != nullptr)
        {
            isGenerating = looperTracks[trackIdx]->isGenerating();
        }
        
        // Also check if we have input but haven't generated output yet (for initial state)
        auto& track = looperEngine.getTrack(trackIdx);
        bool hasInput = track.recordBuffer.m_recorded_length.load() > 0;
        bool hasOutput = track.outputBuffer.m_recorded_length.load() > 0;
        
        // Show arrow if generation is in progress OR if we have input but no output yet
        bool shouldShowArrow = isGenerating || (hasInput && !hasOutput);
        
        if (!shouldShowArrow)
            return;
        
        // Animated ASCII arrow
        g.setColour(juce::Colour(0xfff3d430)); // Yellow
        g.setFont(juce::Font(juce::FontOptions()
                            .withName(juce::Font::getDefaultMonospacedFontName())
                            .withHeight(14.0f)));
        
        // Create animation with different arrow patterns
        int phase = (animationFrame / 10) % 4;  // Change every 10 frames, 4 phases
        juce::String arrow;
        
        switch (phase)
        {
            case 0: arrow = "~>"; break;
            case 1: arrow = "~~>"; break;
            case 2: arrow = "~~~>"; break;
            case 3: arrow = "~~~~>"; break;
        }
        
        g.drawText(arrow, bounds, juce::Justification::centred);
    }
    
    // Update token data from audio buffers
    void updateTokenData()
    {
        for (int trackIdx = 0; trackIdx < numTracks; ++trackIdx)
        {
            auto& track = looperEngine.getTrack(trackIdx);
            
            // Process input buffer - sample from current read head position (handles circular buffer)
            {
                const juce::ScopedLock sl(track.recordBuffer.m_lock);
                const auto& buffer = track.recordBuffer.get_buffer();
                
                size_t currentRecordedLength = track.recordBuffer.m_recorded_length.load();
                size_t& lastRecordedLength = lastInputRecordedLength[trackIdx];
                
                // Detect if buffer was cleared (recordedLength went from non-zero to zero)
                if (lastRecordedLength > 0 && currentRecordedLength == 0)
                {
                    // Buffer was cleared - reset token data for this track
                    inputGrids[trackIdx].blocks.clear();
                    lastInputReadPos[trackIdx] = 0.0f;
                    DBG("Track " + juce::String(trackIdx) + " INPUT buffer cleared - resetting token data");
                }
                lastRecordedLength = currentRecordedLength;
                
                if (!buffer.empty() && currentRecordedLength > 0)
                {
                    float readHeadPos = track.recordReadHead.get_pos();
                    size_t recordedLength = currentRecordedLength;
                    float& lastPos = lastInputReadPos[trackIdx];
                    
                    // Only process if read head has advanced by at least one block
                    float posDelta = readHeadPos - lastPos;
                    // Handle wrap-around
                    if (posDelta < 0) posDelta += static_cast<float>(recordedLength);
                    
                    if (posDelta >= static_cast<float>(SAMPLES_PER_BLOCK))
                    {
                        // Collect samples from current read head position (wraps around circular buffer)
                        std::vector<float> samples;
                        samples.reserve(SAMPLES_PER_BLOCK);
                        
                        for (int i = 0; i < SAMPLES_PER_BLOCK && recordedLength > 0; ++i)
                        {
                            size_t sampleIndex = static_cast<size_t>(readHeadPos + i) % recordedLength;
                            samples.push_back(buffer[sampleIndex]);
                        }
                        
                        // Process the block
                        if (static_cast<int>(samples.size()) >= SAMPLES_PER_BLOCK)
                        {
                            TokenBlock block;
                            // Get sample rate from audio device
                            double sampleRate = looperEngine.getAudioDeviceManager().getCurrentAudioDevice() != nullptr
                                ? looperEngine.getAudioDeviceManager().getCurrentAudioDevice()->getCurrentSampleRate()
                                : 44100.0;
                            block.rms = processAudioBlock(
                                samples.data(),
                                SAMPLES_PER_BLOCK,
                                sampleRate,
                                block.tokens,
                                block.mfccs,
                                true  // isInput = true
                            );
                            inputGrids[trackIdx].addBlock(block);
                            lastPos = readHeadPos;
                            
                            // Debug: Log when we add a token block
                            static int inputBlockCounter = 0;
                            if (inputBlockCounter++ % 10 == 0)
                            {
                                // DBG("Added INPUT token block - Track: " + juce::String(trackIdx) +
                                //     ", readHeadPos: " + juce::String(readHeadPos) +
                                //     ", recordedLength: " + juce::String(recordedLength) +
                                //     ", numBlocks: " + juce::String(inputGrids[trackIdx].blocks.size()));
                            }
                        }
                    }
                }
            }
            
            // Process output buffer - sample from current read head position
            {
                const juce::ScopedLock sl(track.outputBuffer.m_lock);
                const auto& buffer = track.outputBuffer.get_buffer();
                
                size_t currentRecordedLength = track.outputBuffer.m_recorded_length.load();
                size_t& lastRecordedLength = lastOutputRecordedLength[trackIdx];
                
                // Detect if buffer was cleared (recordedLength went from non-zero to zero)
                if (lastRecordedLength > 0 && currentRecordedLength == 0)
                {
                    // Buffer was cleared - reset token data for this track
                    outputGrids[trackIdx].blocks.clear();
                    lastOutputReadPos[trackIdx] = 0.0f;
                    DBG("Track " + juce::String(trackIdx) + " OUTPUT buffer cleared - resetting token data");
                }
                lastRecordedLength = currentRecordedLength;
                
                if (!buffer.empty() && currentRecordedLength > 0)
                {
                    float readHeadPos = track.outputReadHead.get_pos();
                    size_t recordedLength = currentRecordedLength;
                    float& lastPos = lastOutputReadPos[trackIdx];
                    
                    // Only process if read head has advanced by at least one block
                    float posDelta = readHeadPos - lastPos;
                    // Handle wrap-around
                    if (posDelta < 0) posDelta += static_cast<float>(recordedLength);
                    
                    if (posDelta >= static_cast<float>(SAMPLES_PER_BLOCK))
                    {
                        // Collect samples from current read head position (wraps around circular buffer)
                        std::vector<float> samples;
                        samples.reserve(SAMPLES_PER_BLOCK);
                        
                        for (int i = 0; i < SAMPLES_PER_BLOCK && recordedLength > 0; ++i)
                        {
                            size_t sampleIndex = static_cast<size_t>(readHeadPos + i) % recordedLength;
                            samples.push_back(buffer[sampleIndex]);
                        }
                        
                        // Process the block
                        if (static_cast<int>(samples.size()) >= SAMPLES_PER_BLOCK)
                        {
                            TokenBlock block;
                            // Get sample rate from audio device
                            double sampleRate = looperEngine.getAudioDeviceManager().getCurrentAudioDevice() != nullptr
                                ? looperEngine.getAudioDeviceManager().getCurrentAudioDevice()->getCurrentSampleRate()
                                : 44100.0;
                            block.rms = processAudioBlock(
                                samples.data(),
                                SAMPLES_PER_BLOCK,
                                sampleRate,
                                block.tokens,
                                block.mfccs,
                                false  // isInput = false
                            );
                            outputGrids[trackIdx].addBlock(block);
                            lastPos = readHeadPos;
                            
                            // Debug: Log when we add a token block
                            static int outputBlockCounter = 0;
                            if (outputBlockCounter++ % 10 == 0)
                            {
                                // DBG("Added OUTPUT token block - Track: " + juce::String(trackIdx) +
                                //     ", readHeadPos: " + juce::String(readHeadPos) +
                                //     ", recordedLength: " + juce::String(recordedLength) +
                                //     ", numBlocks: " + juce::String(outputGrids[trackIdx].blocks.size()));
                            }
                        }
                    }
                }
            }
        }
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TokenVisualizerComponent)
};

// ============================================================================
// TokenVisualizerWindow implementation
// ============================================================================

TokenVisualizerWindow::TokenVisualizerWindow(VampNetMultiTrackLooperEngine& engine, int numTracks, const std::vector<LooperTrack*>& tracks)
    : juce::DialogWindow("WhAM - Token Visualizer",
                        juce::Colours::darkgrey,
                        true),
      contentComponent(new TokenVisualizerComponent(engine, numTracks, tracks))
{
    setContentOwned(contentComponent, true);
    setResizable(true, true);
    setUsingNativeTitleBar(true);
    
    // Make window large (most of the screen but not fullscreen)
    auto displays = juce::Desktop::getInstance().getDisplays();
    auto mainDisplay = displays.getPrimaryDisplay();
    if (mainDisplay != nullptr)
    {
        auto screenArea = mainDisplay->userArea;
        // Use 90% of screen size to leave room for dock/menubar
        int windowWidth = static_cast<int>(screenArea.getWidth() * 0.9f);
        int windowHeight = static_cast<int>(screenArea.getHeight() * 0.9f);
        
        centreWithSize(windowWidth, windowHeight);
        setResizeLimits(800, 600, screenArea.getWidth(), screenArea.getHeight());
    }
    else
    {
        // Fallback to large default size
        centreWithSize(1600, 1000);
        setResizeLimits(800, 600, 3840, 2160);
    }
}

TokenVisualizerWindow::~TokenVisualizerWindow()
{
}

void TokenVisualizerWindow::closeButtonPressed()
{
    setVisible(false);
}

} // namespace WhAM
