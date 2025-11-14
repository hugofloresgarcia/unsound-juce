#include "WaveformDisplay.h"

using namespace Shared;

WaveformDisplay::WaveformDisplay(MultiTrackLooperEngine& engine, int index)
    : looperEngine(engine), trackIndex(index)
{
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    drawWaveform(g, bounds);
    drawPlayhead(g, bounds);
}

void WaveformDisplay::resized()
{
    // Nothing to do - we just paint into our bounds
}

void WaveformDisplay::drawWaveform(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto& track = looperEngine.getTrack(trackIndex);
    
    const juce::ScopedLock sl(track.tapeLoop.lock);
    
    // Show recording progress even if not fully recorded yet
    size_t displayLength = track.tapeLoop.recordedLength.load();
    
    if (track.writeHead.getRecordEnable())
    {
        // Show current recording position
        displayLength = juce::jmax(displayLength, track.writeHead.getPos());
    }
    
    if (displayLength == 0 && !track.writeHead.getRecordEnable())
    {
        // Draw empty waveform placeholder
        g.setColour(juce::Colour(0xff333333));
        g.drawRect(area);
        g.setColour(juce::Colour(0xfff3d430).withAlpha(0.5f)); // Yellow text
        g.drawText("no audio recorded", area, juce::Justification::centred);
        return;
    }
    
    auto& buffer = track.tapeLoop.getBuffer();
    if (buffer.empty())
        return;
    
    // Use buffer size if no recorded length yet
    if (displayLength == 0)
        displayLength = buffer.size();
    
    // Draw waveform - use red-orange when recording, teal when playing
    g.setColour(track.writeHead.getRecordEnable() ? juce::Colour(0xfff04e36) : juce::Colour(0xff1eb19d));
    
    const int numPoints = area.getWidth();
    const float samplesPerPixel = static_cast<float>(displayLength) / numPoints;
    
    juce::Path waveformPath;
    waveformPath.startNewSubPath(area.getX(), area.getCentreY());
    
    for (int x = 0; x < numPoints; ++x)
    {
        float samplePos = x * samplesPerPixel;
        size_t sampleIndex = static_cast<size_t>(samplePos);
        
        if (sampleIndex >= displayLength)
            break;
        
        // Calculate RMS or peak for this pixel
        float maxSample = 0.0f;
        size_t endSample = static_cast<size_t>((x + 1) * samplesPerPixel);
        endSample = juce::jmin(endSample, displayLength);
        
        for (size_t i = sampleIndex; i < endSample && i < buffer.size(); ++i)
        {
            maxSample = juce::jmax(maxSample, std::abs(buffer[i]));
        }
        
        float y = area.getCentreY() - (maxSample * area.getHeight() * 0.5f);
        waveformPath.lineTo(area.getX() + x, y);
    }
    
    // Draw mirrored bottom half
    for (int x = numPoints - 1; x >= 0; --x)
    {
        float samplePos = x * samplesPerPixel;
        size_t sampleIndex = static_cast<size_t>(samplePos);
        
        if (sampleIndex >= displayLength)
            continue;
        
        float maxSample = 0.0f;
        size_t endSample = static_cast<size_t>((x + 1) * samplesPerPixel);
        endSample = juce::jmin(endSample, displayLength);
        
        for (size_t i = sampleIndex; i < endSample && i < buffer.size(); ++i)
        {
            maxSample = juce::jmax(maxSample, std::abs(buffer[i]));
        }
        
        float y = area.getCentreY() + (maxSample * area.getHeight() * 0.5f);
        waveformPath.lineTo(area.getX() + x, y);
    }
    
    waveformPath.closeSubPath();
    g.fillPath(waveformPath);
    
    // Draw center line
    g.setColour(juce::Colour(0xff333333));
    g.drawLine(area.getX(), area.getCentreY(), 
               area.getRight(), area.getCentreY(), 1.0f);
}

void WaveformDisplay::drawPlayhead(juce::Graphics& g, juce::Rectangle<int> waveformArea)
{
    auto& track = looperEngine.getTrack(trackIndex);
    
    // Show playhead if playing (even during recording before audio is recorded)
    if (!track.isPlaying.load())
        return;
    
    size_t recordedLength = track.tapeLoop.recordedLength.load();
    // During new recording, use recordHead or playhead to show position
    if (recordedLength == 0)
    {
        if (track.writeHead.getRecordEnable())
        {
            // Show playhead based on current recording position
            float playheadPosition = track.readHead.getPos();
            float maxLength = static_cast<float>(track.tapeLoop.getBufferSize());
            if (maxLength > 0)
            {
                float normalizedPosition = playheadPosition / maxLength;
                int playheadX = waveformArea.getX() + static_cast<int>(normalizedPosition * waveformArea.getWidth());
                
                // Draw playhead line - use yellow from palette
                g.setColour(juce::Colour(0xfff3d430));
                g.drawLine(playheadX, waveformArea.getY(), 
                           playheadX, waveformArea.getBottom(), 2.0f);
                
                // Draw playhead triangle
                juce::Path playheadTriangle;
                playheadTriangle.addTriangle(playheadX - 5, waveformArea.getY(),
                                            playheadX + 5, waveformArea.getY(),
                                            playheadX, waveformArea.getY() + 10);
                g.fillPath(playheadTriangle);
            }
        }
        return;
    }
    
    if (track.tapeLoop.getBufferSize() == 0 || recordedLength == 0)
        return;
    
    float playheadPosition = track.readHead.pos.load();
    float normalizedPosition = playheadPosition / static_cast<float>(recordedLength);
    
    int playheadX = waveformArea.getX() + static_cast<int>(normalizedPosition * waveformArea.getWidth());
    
    // Draw playhead line - use yellow from palette
    g.setColour(juce::Colour(0xfff3d430));
    g.drawLine(playheadX, waveformArea.getY(), 
               playheadX, waveformArea.getBottom(), 2.0f);
    
    // Draw playhead triangle
    juce::Path playheadTriangle;
    playheadTriangle.addTriangle(playheadX - 5, waveformArea.getY(),
                                  playheadX + 5, waveformArea.getY(),
                                  playheadX, waveformArea.getY() + 10);
    g.fillPath(playheadTriangle);
}

