#include "TokenVisualizer.h"
#include <cmath>
#include <random>

namespace WhAM
{

// ============================================================================
// Stateless utility functions
// ============================================================================

namespace
{
    constexpr int NUM_TOKEN_ROWS = 13;
    constexpr int SAMPLES_PER_BLOCK = 512;
    constexpr int NUM_VISIBLE_COLUMNS = 30; // Reduced from 100 for better performance

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

    // Calculate RMS from audio samples
    float calculateRMS(const float* samples, int numSamples)
    {
        if (numSamples == 0)
            return 0.0f;
        
        double sum = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            double sample = static_cast<double>(samples[i]);
            sum += sample * sample;
        }
        
        return static_cast<float>(std::sqrt(sum / numSamples));
    }

    // Generate vibrant color for a token
    juce::Colour generateTokenColor(int tokenIndex, int tokenValue, float rms, bool isInput)
    {
        float hue;
        
        if (isInput)
        {
            // Input: Map RMS to hue: quieter (low RMS) = blue, louder (high RMS) = red/orange
            // Blue is around 240 degrees, red is around 0 degrees
            // Map RMS (0.0-1.0) to hue: 240 (blue) -> 0 (red), wrapping through purple
            hue = 240.0f - (rms * 240.0f); // 240 (blue) to 0 (red)
            if (hue < 0.0f) hue += 360.0f;
        }
        else
        {
            // Output: Map RMS to hue: quieter (low RMS) = green, louder (high RMS) = yellow/orange
            // Green is around 120 degrees, yellow/orange is around 30-60 degrees
            // Map RMS (0.0-1.0) to hue: 120 (green) -> 30 (yellow/orange)
            hue = 120.0f - (rms * 90.0f); // 120 (green) to 30 (yellow/orange)
            if (hue < 0.0f) hue += 360.0f;
        }
        
        // Add randomness based on token value and index for more variation
        float randomOffset = static_cast<float>(tokenValue % 60) - 30.0f; // -30 to +30 degrees
        hue += randomOffset;
        if (hue < 0.0f) hue += 360.0f;
        if (hue >= 360.0f) hue -= 360.0f;
        
        // Add slight variation based on token index
        hue += (tokenIndex % 5) * 2.0f; // Small variation per row
        if (hue >= 360.0f) hue -= 360.0f;
        
        // Map RMS to saturation: quieter = less saturated (more grayish), louder = more saturated
        // More drastic range: 0.1 (very gray) to 1.0 (very vibrant)
        float saturation = 0.1f + (rms * 0.9f); // 0.1 to 1.0
        
        // Map RMS to brightness: quieter = darker, louder = brighter
        // More drastic range: 0.1 (very dark) to 1.0 (very bright)
        float brightness = 0.1f + (rms * 0.9f); // 0.1 to 1.0
        
        return juce::Colour::fromHSV(hue / 360.0f, saturation, brightness, 1.0f);
    }

    // Process audio block and generate token data
    void processAudioBlock(const float* samples, int numSamples, float& rmsOut, std::array<int, NUM_TOKEN_ROWS>& tokensOut)
    {
        rmsOut = calculateRMS(samples, numSamples);
        tokensOut = generateFakeTokens();
    }
}

// ============================================================================
// State structures
// ============================================================================

struct TokenBlock
{
    std::array<int, NUM_TOKEN_ROWS> tokens;
    float rms;
    
    TokenBlock() : tokens{}, rms(0.0f) {}
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
// TokenVisualizerComponent - holds state and calls stateless functions
// ============================================================================

class TokenVisualizerWindow::TokenVisualizerComponent : public juce::Component,
                                                        public juce::Timer
{
public:
    TokenVisualizerComponent(VampNetMultiTrackLooperEngine& engine, int numTracks)
        : looperEngine(engine),
          numTracks(numTracks)
    {
        // Initialize grid data for each track (input and output)
        for (int i = 0; i < numTracks; ++i)
        {
            inputGrids.push_back(TokenGridData(i));
            outputGrids.push_back(TokenGridData(i));
            lastInputReadPos.push_back(0.0f);
            lastOutputReadPos.push_back(0.0f);
        }
        
        startTimer(50); // Update every 50ms
    }
    
    ~TokenVisualizerComponent() override
    {
        stopTimer();
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
        
        auto bounds = getLocalBounds();
        const int trackHeight = bounds.getHeight() / (numTracks * 2); // Each track has input + output
        
        for (int trackIdx = 0; trackIdx < numTracks; ++trackIdx)
        {
            auto trackBounds = bounds.removeFromTop(trackHeight * 2);
            
            // Draw input tokens (top half)
            auto inputBounds = trackBounds.removeFromTop(trackHeight);
            drawTokenGrid(g, inputGrids[trackIdx], inputBounds, true);
            
            // Draw separator line
            g.setColour(juce::Colour(0xff333333));
            g.drawLine(inputBounds.getX(), inputBounds.getBottom(),
                      inputBounds.getRight(), inputBounds.getBottom(), 1.0f);
            
            // Draw output tokens (bottom half)
            auto outputBounds = trackBounds;
            drawTokenGrid(g, outputGrids[trackIdx], outputBounds, false);
            
            // Draw track label
            g.setColour(juce::Colour(0xfff3d430));
            g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
            g.drawText("Track " + juce::String(trackIdx + 1),
                      inputBounds.reduced(5),
                      juce::Justification::topLeft);
        }
    }
    
    void resized() override
    {
        // Nothing to do - we just paint into our bounds
    }
    
    void timerCallback() override
    {
        updateTokenData();
        repaint();
    }
    
private:
    VampNetMultiTrackLooperEngine& looperEngine;
    int numTracks;
    
    std::vector<TokenGridData> inputGrids;
    std::vector<TokenGridData> outputGrids;
    
    // Track last processed positions for each track (to avoid duplicate blocks)
    std::vector<float> lastInputReadPos;
    std::vector<float> lastOutputReadPos;
    
    // Stateless rendering function
    void drawTokenGrid(juce::Graphics& g, const TokenGridData& gridData, juce::Rectangle<int> bounds, bool isInput)
    {
        if (gridData.blocks.empty())
            return;
        
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
                
                juce::Colour color = generateTokenColor(row, block.tokens[row], block.rms, isInput);
                g.setColour(color);
                g.fillRect(rect);
            }
        }
    }
    
    // Update token data from audio buffers
    void updateTokenData()
    {
        for (int trackIdx = 0; trackIdx < numTracks; ++trackIdx)
        {
            auto& track = looperEngine.getTrack(trackIdx);
            
            // Process input buffer - sample from current read head position (handles circular buffer)
            {
                const juce::ScopedLock sl(track.recordBuffer.lock);
                const auto& buffer = track.recordBuffer.getBuffer();
                
                if (!buffer.empty() && track.recordBuffer.recordedLength.load() > 0)
                {
                    float readHeadPos = track.recordReadHead.getPos();
                    size_t recordedLength = track.recordBuffer.recordedLength.load();
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
                            processAudioBlock(
                                samples.data(),
                                SAMPLES_PER_BLOCK,
                                block.rms,
                                block.tokens
                            );
                            inputGrids[trackIdx].addBlock(block);
                            lastPos = readHeadPos;
                        }
                    }
                }
            }
            
            // Process output buffer - sample from current read head position
            {
                const juce::ScopedLock sl(track.outputBuffer.lock);
                const auto& buffer = track.outputBuffer.getBuffer();
                
                if (!buffer.empty() && track.outputBuffer.recordedLength.load() > 0)
                {
                    float readHeadPos = track.outputReadHead.getPos();
                    size_t recordedLength = track.outputBuffer.recordedLength.load();
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
                            processAudioBlock(
                                samples.data(),
                                SAMPLES_PER_BLOCK,
                                block.rms,
                                block.tokens
                            );
                            outputGrids[trackIdx].addBlock(block);
                            lastPos = readHeadPos;
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

TokenVisualizerWindow::TokenVisualizerWindow(VampNetMultiTrackLooperEngine& engine, int numTracks)
    : juce::DialogWindow("Token Visualizer",
                        juce::Colours::darkgrey,
                        true),
      contentComponent(new TokenVisualizerComponent(engine, numTracks))
{
    setContentOwned(contentComponent, true);
    setResizable(false, false);
    setUsingNativeTitleBar(true);
    
    // Calculate window size: ~30 columns × 13 rows per track × 2 (input+output) × numTracks
    const int columnWidth = 8; // pixels per column
    const int rowHeight = 8;   // pixels per row
    const int trackSpacing = 20; // spacing between tracks
    const int windowWidth = NUM_VISIBLE_COLUMNS * columnWidth + 40; // 40 for margins
    const int windowHeight = (numTracks * 2 * NUM_TOKEN_ROWS * rowHeight) + (numTracks * trackSpacing) + 60; // 60 for title bar and margins
    
    centreWithSize(windowWidth, windowHeight);
}

TokenVisualizerWindow::~TokenVisualizerWindow()
{
}

void TokenVisualizerWindow::closeButtonPressed()
{
    setVisible(false);
}

} // namespace WhAM

