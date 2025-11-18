#include "SinksWindow.h"
#include <flowerjuce/CustomLookAndFeel.h>
#include <cmath>

namespace flowerjuce
{

SinksWindow::SinksWindow(CLEATPanner* panner, const std::array<std::atomic<float>, 16>& levels)
    : cleatPanner(panner), channelLevels(levels)
{
    setSize(500, 500);
    startTimer(50); // Update every 50ms
    
    // Setup toggle button
    showPinkBoxesToggle.setButtonText("Show Pink Boxes");
    showPinkBoxesToggle.setToggleState(showPinkBoxes, juce::dontSendNotification);
    showPinkBoxesToggle.onClick = [this] {
        showPinkBoxes = showPinkBoxesToggle.getToggleState();
        repaint();
    };
    addAndMakeVisible(showPinkBoxesToggle);
}

SinksWindow::SinksWindow(const std::array<std::atomic<float>, 16>& levels)
    : cleatPanner(nullptr), channelLevels(levels)
{
    setSize(500, 500);
    startTimer(50); // Update every 50ms
    
    // Setup toggle button (disabled since no CLEAT panner available)
    showPinkBoxesToggle.setButtonText("Show Pink Boxes");
    showPinkBoxesToggle.setToggleState(false, juce::dontSendNotification);
    showPinkBoxesToggle.setEnabled(false);
    addAndMakeVisible(showPinkBoxesToggle);
}

SinksWindow::~SinksWindow()
{
    stopTimer();
}

void SinksWindow::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    // Draw "Channel Meters" label above the meters
    if (metersArea.getHeight() > 0)
    {
        auto labelArea = metersArea;
        labelArea.setHeight(15);
        labelArea.translate(0, -15);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("bold")));
        g.drawText("Channel Meters", labelArea, juce::Justification::centred);
    }
    
    // Draw channel level meters in a 4x4 grid
    if (metersArea.getHeight() > 50 && metersArea.getWidth() > 50)
    {
        const int numChannels = 16;
        const int cols = 4;
        const int rows = 4;
        const int meterSpacing = 5;
        const int meterWidth = (metersArea.getWidth() - (cols + 1) * meterSpacing) / cols;
        const int meterHeight = (metersArea.getHeight() - (rows + 1) * meterSpacing) / rows;
        
        // Get pan position and gains if using CLEAT panner
        float panX = 0.5f;
        float panY = 0.5f;
        float gainPower = 1.0f;
        std::array<float, 16> gains{};
        
        if (cleatPanner != nullptr)
        {
            panX = cleatPanner->get_smoothed_pan_x();
            panY = cleatPanner->get_smoothed_pan_y();
            gainPower = cleatPanner->get_gain_power();
            gains = PanningUtils::compute_cleat_gains(panX, panY, gainPower);
        }
        else
        {
            // Default gains (all equal) if no panner
            for (int i = 0; i < 16; ++i)
            {
                gains[i] = 1.0f / 16.0f; // Equal distribution
            }
        }
        
        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                int channel = row * cols + col;
                int x = metersArea.getX() + col * (meterWidth + meterSpacing) + meterSpacing;
                int y = metersArea.getY() + row * (meterHeight + meterSpacing) + meterSpacing;
                
                juce::Rectangle<int> meterRect(x, y, meterWidth, meterHeight);
                if (channel >= 0 && channel < numChannels)
                {
                    float level = channelLevels[channel].load();
                    float gain = gains[channel];
                    drawChannelMeter(g, meterRect, channel, level, gain, panX, panY);
                }
            }
        }
    }
}

void SinksWindow::drawChannelMeter(juce::Graphics& g, juce::Rectangle<int> area, int channel, 
                                    float level, float gain, float panX, float panY)
{
    // Convert linear level to dB
    float levelDb = linearToDb(level);
    
    // Convert gain to dB
    float gainDb = linearToDb(gain);
    
    // Define dB range: -60dB to 0dB (full range), healthy range: -40dB to -15dB
    const float minDb = -60.0f;
    const float maxDb = 0.0f;
    const float silenceThresholdDb = -50.0f; // Below this is considered silence
    const float healthyMinDb = -40.0f;
    const float healthyMaxDb = -15.0f;
    
    // Check if level is effectively silent (below threshold or very small linear value)
    bool isSilent = (level < 0.0001f) || (levelDb < silenceThresholdDb);
    
    // Map dB to normalized value (0.0 = minDb, 1.0 = maxDb)
    float normalizedLevel = juce::jlimit(0.0f, 1.0f, (levelDb - minDb) / (maxDb - minDb));
    
    // Calculate circle center and maximum radius
    // Leave room for channel label (top), dB label (below circle), and optionally pink text box (bottom)
    float centerX = area.getCentreX();
    float reservedTop = 12.0f; // Channel label
    float reservedBottom = (cleatPanner != nullptr && showPinkBoxes) ? 80.0f : 20.0f; // dB label + optional pink text box
    float availableHeight = static_cast<float>(area.getHeight()) - reservedTop - reservedBottom;
    float centerY = area.getY() + reservedTop + availableHeight * 0.3f; // Position circle in upper portion
    float maxRadius = juce::jmin(static_cast<float>(area.getWidth()), availableHeight * 0.4f) * 0.35f;
    
    // Calculate radius based on normalized level (minimum radius for visibility)
    float minRadius = maxRadius * 0.1f; // Minimum 10% of max radius
    float radius = minRadius + (maxRadius - minRadius) * normalizedLevel;
    
    // Color based on dB range:
    // Silent (below -50dB or < 0.0001 linear): dim gray
    // Below -40dB: dim gray (too quiet)
    // -40dB to -15dB: green (healthy)
    // -15dB to 0dB: yellow/red (too loud)
    juce::Colour circleColour;
    if (isSilent || levelDb < healthyMinDb)
    {
        circleColour = juce::Colours::darkgrey.withBrightness(0.3f);
    }
    else if (levelDb <= healthyMaxDb)
    {
        circleColour = juce::Colours::green;
    }
    else if (levelDb < -5.0f)
    {
        circleColour = juce::Colours::yellow;
    }
    else
    {
        circleColour = juce::Colours::red;
    }
    
    // Draw circle
    if (!isSilent && levelDb > minDb)
    {
        g.setColour(circleColour);
        g.fillEllipse(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f);
        
        // Draw border
        g.setColour(circleColour.brighter(0.3f));
        g.drawEllipse(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f, 1.5f);
    }
    else
    {
        // Draw very faint circle for silence
        g.setColour(juce::Colours::darkgrey.withAlpha(0.2f));
        g.drawEllipse(centerX - minRadius, centerY - minRadius, minRadius * 2.0f, minRadius * 2.0f, 1.0f);
        // Don't show dB value for silence - show "-inf" or just leave blank
        levelDb = minDb; // Use minDb for display when silent
    }
    
    // Channel number label (above circle)
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    auto channelLabelArea = area;
    channelLabelArea.setHeight(12);
    channelLabelArea.setY(area.getY() + 2);
    g.drawText(juce::String(channel), channelLabelArea, juce::Justification::centred);
    
    // Level value in dB (below circle) - make it clearly visible
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));
    auto dbLabelArea = area;
    dbLabelArea.setHeight(12);
    dbLabelArea.setY(static_cast<int>(centerY + maxRadius + 8));
    juce::String dbText;
    if (isSilent)
    {
        dbText = "-inf";
    }
    else
    {
        dbText = juce::String(levelDb, 1);
    }
    g.drawText(dbText, dbLabelArea, juce::Justification::centred);
    
    // Draw pink box if enabled and CLEAT panner is available
    if (cleatPanner != nullptr && showPinkBoxes)
    {
        // Compute phases for this channel
        float xPhase, yPhase;
        computeChannelPhases(channel, panX, panY, xPhase, yPhase);
        
        // Compute cos for x oscillator and sin for y oscillator
        float xOscArg = 2.0f * juce::MathConstants<float>::pi * xPhase;
        float yOscArg = 2.0f * juce::MathConstants<float>::pi * yPhase;
        float xCos = std::cos(xOscArg);
        float ySin = std::sin(yOscArg);
        
        // Draw pink text box below the dB label
        // Position it to fit within the available area (ensure it doesn't extend beyond bottom)
        auto textBoxArea = area;
        int textBoxStartY = static_cast<int>(centerY + maxRadius + 20);
        int textBoxAvailableHeight = area.getBottom() - textBoxStartY;
        
        // Only draw if there's enough space
        if (textBoxAvailableHeight > 10)
        {
            // Limit height to available space, but ensure minimum height for readability
            int textBoxHeight = juce::jmin(60, textBoxAvailableHeight - 2); // Leave 2px margin
            
            textBoxArea.removeFromTop(textBoxStartY - area.getY());
            textBoxArea.setHeight(textBoxHeight);
            
            // Ensure text box doesn't extend beyond area bounds
            if (textBoxArea.getBottom() > area.getBottom())
            {
                textBoxArea.setBottom(area.getBottom());
            }
            
            // Draw pink background
            g.setColour(juce::Colour(0xFFFF69B4)); // Pink color
            g.fillRect(textBoxArea);
            
            // Draw border
            g.setColour(juce::Colours::white);
            g.drawRect(textBoxArea, 1.0f);
            
            // Draw text: gain dB, x phase, x cos, y phase, y sin
            g.setColour(juce::Colours::black);
            g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f)));
            
            juce::String gainText = "Gain: " + juce::String(gainDb, 1) + " dB";
            juce::String xPhaseText = "X phase: " + juce::String(xPhase, 3);
            juce::String xCosText = "X cos: " + juce::String(xCos, 3);
            juce::String yPhaseText = "Y phase: " + juce::String(yPhase, 3);
            juce::String ySinText = "Y sin: " + juce::String(ySin, 3);
            
            auto textArea = textBoxArea.reduced(2, 2);
            int lineHeight = 11;
            
            g.drawText(gainText, textArea.removeFromTop(lineHeight), juce::Justification::centredLeft);
            g.drawText(xPhaseText, textArea.removeFromTop(lineHeight), juce::Justification::centredLeft);
            g.drawText(xCosText, textArea.removeFromTop(lineHeight), juce::Justification::centredLeft);
            g.drawText(yPhaseText, textArea.removeFromTop(lineHeight), juce::Justification::centredLeft);
            g.drawText(ySinText, textArea.removeFromTop(lineHeight), juce::Justification::centredLeft);
        }
    }
}

void SinksWindow::computeChannelPhases(int channel, float panX, float panY, float& xPhase, float& yPhase) const
{
    // Map x and y to the range (0.275) -> 1.0 (matching PanningUtils::compute_cleat_gains)
    float scaled_x = juce::jmap(panX, 0.0f, 1.0f, 0.275f, 1.0f);
    float scaled_y = juce::jmap(panY, 0.0f, 1.0f, 0.275f, 1.0f);
    
    // Column offsets (left to right): -0.75, -0.5, -0.25, 0.0
    constexpr float column_offsets[4] = {-0.75f, -0.5f, -0.25f, 0.0f};
    
    // Row offsets (bottom to top): -0.75, -0.5, -0.25, 0.0
    constexpr float row_offsets[4] = {-0.75f, -0.5f, -0.25f, 0.0f};
    
    // Compute row and column from channel index (row-major ordering)
    int row = channel / 4;
    int col = channel % 4;
    
    // Compute phases (matching PanningUtils::compute_cleat_gains)
    yPhase = juce::jlimit(-0.5f, 0.5f, scaled_y + row_offsets[row]);
    xPhase = juce::jlimit(-0.5f, 0.5f, scaled_x + column_offsets[col]);
}

float SinksWindow::linearToDb(float linear) const
{
    if (linear > 0.0f)
        return 20.0f * std::log10(linear);
    return -60.0f; // Return minimum dB for zero/negative values
}

void SinksWindow::resized()
{
    auto bounds = getLocalBounds();
    
    // Reserve space for toggle button at bottom
    const int toggleHeight = 25;
    const int toggleMargin = 5;
    if (bounds.getHeight() > toggleHeight + toggleMargin)
    {
        auto toggleArea = bounds.removeFromBottom(toggleHeight + toggleMargin);
        toggleArea.removeFromTop(toggleMargin);
        showPinkBoxesToggle.setBounds(toggleArea);
    }
    
    bounds.removeFromTop(5);
    
    // Reserve space for "Channel Meters" label (will be drawn in paint)
    if (bounds.getHeight() > 15)
    {
        bounds.removeFromTop(15); // Space for "Channel Meters" label
    }
    
    // Channel meters area (all remaining space)
    metersArea = bounds;
}

void SinksWindow::timerCallback()
{
    // Trigger repaint to update meters and phase information
    repaint();
}

} // namespace flowerjuce

