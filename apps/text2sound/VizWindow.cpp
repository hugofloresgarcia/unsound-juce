#include "VizWindow.h"
#include <flowerjuce/CustomLookAndFeel.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

using namespace Text2Sound;

VizWindow::VizWindow(MultiTrackLooperEngine& engine, std::vector<std::weak_ptr<LooperTrack>> tracks)
    : looperEngine(engine), tracks(std::move(tracks))
{
    // Initialize track colors (matching UI theme)
    trackColors[0] = juce::Colour(0xff1eb19d); // Teal
    trackColors[1] = juce::Colour(0xffed1683); // Pink
    trackColors[2] = juce::Colour(0xfff3d430); // Yellow
    trackColors[3] = juce::Colour(0xfff36e27); // Orange
    trackColors[4] = juce::Colour(0xff00ff00); // Green
    trackColors[5] = juce::Colour(0xff00ffff); // Cyan
    trackColors[6] = juce::Colour(0xffff00ff); // Magenta
    trackColors[7] = juce::Colour(0xffff8000); // Orange-red
    
    // Initialize track levels to zero
    for (auto& level : trackLevels)
    {
        level.store(0.0f);
    }
    
    setSize(800, 800);
    startTimer(50); // Update every 50ms
}

VizWindow::~VizWindow()
{
    stopTimer();
}

void VizWindow::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    // Draw "Multi-Track Panner" label above the panner view
    if (pannerViewArea.getHeight() > 0)
    {
        auto labelArea = pannerViewArea;
        labelArea.setHeight(20);
        labelArea.translate(0, -30);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("bold")));
        g.drawText("Multi-Track Panner", labelArea, juce::Justification::centred);
    }
    
    // Draw multi-track panner view
    if (pannerViewArea.getHeight() > 50 && pannerViewArea.getWidth() > 50)
    {
        drawMultiTrackPanner(g, pannerViewArea);
    }
}

void VizWindow::drawMultiTrackPanner(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto bounds = area.toFloat();
    
    // Fill background
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(bounds, 4.0f);
    
    // Draw bright border
    g.setColour(juce::Colour(0xfff3d430)); // Bright yellow
    g.drawRoundedRectangle(bounds, 4.0f, 3.0f);
    
    // Draw dense grid (16x16)
    g.setColour(juce::Colour(0xff333333));
    const int grid_divisions = 16;
    float grid_spacing_x = bounds.getWidth() / grid_divisions;
    float grid_spacing_y = bounds.getHeight() / grid_divisions;
    for (int i = 1; i < grid_divisions; ++i)
    {
        // Vertical lines
        g.drawLine(bounds.getX() + i * grid_spacing_x, bounds.getY(),
                   bounds.getX() + i * grid_spacing_x, bounds.getBottom(), 0.5f);
        // Horizontal lines
        g.drawLine(bounds.getX(), bounds.getY() + i * grid_spacing_y,
                   bounds.getRight(), bounds.getY() + i * grid_spacing_y, 0.5f);
    }
    
    // Draw center crosshair
    g.setColour(juce::Colour(0xff555555));
    float center_x = bounds.getCentreX();
    float center_y = bounds.getCentreY();
    float crosshair_size = 8.0f;
    g.drawLine(center_x - crosshair_size, center_y, center_x + crosshair_size, center_y, 1.0f);
    g.drawLine(center_x, center_y - crosshair_size, center_x, center_y + crosshair_size, 1.0f);
    
    // Draw each track's panner dot
    const float baseRadius = 8.0f;
    int numEngineTracks = looperEngine.get_num_tracks();
    int numTracks = static_cast<int>(tracks.size());
    
    for (int i = 0; i < numTracks && i < numEngineTracks; ++i)
    {
        if (i < 0 || i >= static_cast<int>(tracks.size()))
            continue;
        
        // Lock weak_ptr to get shared_ptr (safe access)
        auto track = tracks[i].lock();
        if (!track)
            continue;
        
        // Get pan position
        float panX, panY;
        if (!track->getPanPosition(panX, panY))
            continue;
        
        // Get mono output level (safe - we've checked bounds)
        if (i < 0 || i >= numEngineTracks || i >= maxTracks)
            continue;
        
        // Use decayed level (decay is handled in timerCallback)
        float monoLevel = trackLevels[i].load();
        
        // Convert pan position to component coordinates
        float x = bounds.getX() + panX * bounds.getWidth();
        float y = bounds.getY() + (1.0f - panY) * bounds.getHeight(); // Flip Y (0.0 = bottom, 1.0 = top)
        
        // Convert linear level to dB (post-fader, matching SinksWindow implementation)
        const float minDb = -60.0f;  // Minimum dB for silence
        const float maxDb = 0.0f;    // Maximum dB (full scale)
        float levelDb = linearToDb(monoLevel);
        
        // Map dB to radius (proportional to dB loudness, matching SinksWindow approach)
        // dB range: -60dB to 0dB maps to radius range: minRadius to maxRadius
        const float minRadius = baseRadius * 0.3f;   // 30% of base radius for silence
        const float maxRadius = baseRadius * 2.0f;    // 200% of base radius for full scale
        float normalizedLevel = juce::jlimit(0.0f, 1.0f, (levelDb - minDb) / (maxDb - minDb));
        float radius = minRadius + (maxRadius - minRadius) * normalizedLevel;
        
        // Get track color
        juce::Colour trackColor = trackColors[i % numTrackColors];
        
        // Draw dot shadow
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillEllipse(x - radius + 1.0f, y - radius + 1.0f, radius * 2.0f, radius * 2.0f);
        
        // Draw dot
        g.setColour(trackColor);
        g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);
        
        // Draw border in same color
        g.setColour(trackColor);
        g.drawEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f, 2.0f);
        
        // Draw text query label next to the dot
        juce::String textQuery = track->getTextPrompt();
        if (textQuery.isNotEmpty())
        {
            // Position text offset from the dot (to the right and slightly below)
            const float textOffsetX = radius + 4.0f;
            const float textOffsetY = radius + 2.0f;
            float textX = x + textOffsetX;
            float textY = y + textOffsetY;
            
            // Ensure text stays within bounds
            const float maxTextWidth = bounds.getRight() - textX - 4.0f;
            if (maxTextWidth > 0)
            {
                // Draw text background for readability
                juce::Font font(juce::FontOptions().withHeight(16.0f));  // Increased font size
                juce::Rectangle<float> textRect(textX, textY, maxTextWidth, 24.0f);  // Increased height for larger font
                
                // Truncate text if needed
                juce::String displayText = textQuery;
                float textWidth = font.getStringWidth(displayText);
                if (textWidth > maxTextWidth)
                {
                    // Binary search for the right truncation point
                    int len = displayText.length();
                    int low = 0;
                    int high = len;
                    while (low < high)
                    {
                        int mid = (low + high + 1) / 2;
                        juce::String test = textQuery.substring(0, mid) + "...";
                        if (font.getStringWidth(test) <= maxTextWidth)
                            low = mid;
                        else
                            high = mid - 1;
                    }
                    displayText = textQuery.substring(0, low) + "...";
                    textWidth = font.getStringWidth(displayText);
                }
                
                // Calculate text bounds manually (getStringBounds is deprecated/removed)
                const float textHeight = font.getHeight();
                juce::Rectangle<float> textBounds(textX, textY, textWidth, textHeight);
                textBounds = textBounds.expanded(2.0f, 1.0f);
                
                g.setColour(juce::Colours::black.withAlpha(0.7f));
                g.fillRoundedRectangle(textBounds, 2.0f);
                
                // Draw text
                g.setColour(trackColor);
                g.setFont(font);
                g.drawText(displayText, textRect, juce::Justification::centredLeft);
            }
        }
    }
}

void VizWindow::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(10);
    
    // Reserve space for "Multi-Track Panner" label (will be drawn in paint)
    bounds.removeFromTop(20);
    bounds.removeFromTop(10);
    
    // Multi-track panner view (square, centered)
    auto pannerSize = juce::jmin(bounds.getWidth() - 40, bounds.getHeight() - 40);
    pannerSize = juce::jmax(200, pannerSize); // Minimum 200px
    auto pannerArea = bounds.withSizeKeepingCentre(pannerSize, pannerSize);
    pannerViewArea = pannerArea;
}

void VizWindow::timerCallback()
{
    // Apply decay to all track levels (similar to MultiTrackLooperEngine)
    int numTracks = static_cast<int>(tracks.size());
    int numEngineTracks = looperEngine.get_num_tracks();
    
    for (int i = 0; i < numTracks && i < numEngineTracks && i < maxTracks; ++i)
    {
        float currentLevel = trackLevels[i].load();
        
        // Apply decay
        if (currentLevel > 0.001f)
        {
            trackLevels[i].store(currentLevel * levelDecayFactor);
        }
        else
        {
            trackLevels[i].store(0.0f);
        }
        
        // Update with new peak from engine if higher
        float rawLevel = looperEngine.get_track_engine(i).get_mono_output_level();
        float decayedLevel = trackLevels[i].load();
        if (rawLevel > decayedLevel)
        {
            trackLevels[i].store(rawLevel);
        }
    }
    
    // Trigger repaint to update panner view
    repaint();
}

float VizWindow::linearToDb(float linear) const
{
    if (linear > 0.0f)
        return 20.0f * std::log10(linear);
    return -60.0f; // Return minimum dB for zero/negative values (matching SinksWindow)
}

