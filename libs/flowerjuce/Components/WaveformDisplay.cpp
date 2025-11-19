#include "WaveformDisplay.h"
#include "../LooperEngine/MultiTrackLooperEngine.h"
#include "../LooperEngine/TapeLoop.h"
#include "../LooperEngine/LooperWriteHead.h"
#include "../LooperEngine/LooperReadHead.h"

using namespace Shared;

WaveformDisplay::WaveformDisplay(MultiTrackLooperEngine& engine, int index)
    : engineType(Basic), trackIndex(index), looperEngine(&engine)
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
    TapeLoop* tapeLoop = nullptr;
    LooperWriteHead* writeHead = nullptr;
    std::atomic<bool>* isPlaying = nullptr;
    LooperReadHead* readHead = nullptr;
    
    auto& track_engine = looperEngine->get_track_engine(trackIndex);
    
    // Get buffer and related info via track_engine
    const juce::ScopedLock sl(track_engine.get_buffer_lock());
    const auto& buffer = track_engine.get_buffer();
    
    // Determine display length - use loop_end if set (for duration control), otherwise use recorded_length
    size_t wrapPos = track_engine.get_loop_end();
    size_t displayLength = (wrapPos > 0) ? wrapPos : track_engine.get_recorded_length();
    
    if (track_engine.get_record_enable())
    {
        // Show current recording position
        displayLength = juce::jmax(displayLength, static_cast<size_t>(track_engine.get_write_pos()));
    }
    
    if (displayLength == 0 && !track_engine.get_record_enable())
    {
        // Draw empty waveform placeholder
        g.setColour(juce::Colour(0xff333333));
        g.drawRect(area);
        g.setColour(juce::Colour(0xfff3d430).withAlpha(0.5f)); // Yellow text
        g.drawText("no audio recorded", area, juce::Justification::centred);
        return;
    }
    
    if (buffer.empty())
        return;
    
    // Use buffer size if no recorded length yet
    if (displayLength == 0)
        displayLength = buffer.size();
    
    // Clamp displayLength to buffer size
    displayLength = juce::jmin(displayLength, buffer.size());
    
    // Draw waveform - use red-orange when recording, teal when playing
    g.setColour(track_engine.get_record_enable() ? juce::Colour(0xfff04e36) : juce::Colour(0xff1eb19d));
    
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
    auto& track_engine = looperEngine->get_track_engine(trackIndex);
    
    // Show playhead if playing (even during recording before audio is recorded)
    if (!track_engine.get_playing())
        return;
    
    // Use loop_end if set (for duration control), otherwise use recorded_length
    size_t wrapPos = track_engine.get_loop_end();
    size_t playbackLength = (wrapPos > 0) ? wrapPos : track_engine.get_recorded_length();
    
    // During new recording, use write head position to show position
    if (playbackLength == 0)
    {
        if (track_engine.get_record_enable())
        {
            // Show playhead based on current recording position
            float playheadPosition = track_engine.get_pos();
            const auto& buffer = track_engine.get_buffer();
            float maxLength = static_cast<float>(buffer.size());
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
    
    if (track_engine.get_buffer_size() == 0 || playbackLength == 0)
        return;
    
    float playheadPosition = track_engine.get_pos();
    float normalizedPosition = playheadPosition / static_cast<float>(playbackLength);
    
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

