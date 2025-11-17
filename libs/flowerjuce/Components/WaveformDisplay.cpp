#include "WaveformDisplay.h"
#include "../Engine/MultiTrackLooperEngine.h"
#include "../Engine/TapeLoop.h"
#include "../Engine/LooperWriteHead.h"
#include "../Engine/LooperReadHead.h"

using namespace Shared;

WaveformDisplay::WaveformDisplay(MultiTrackLooperEngine& engine, int index)
    : engineType(Basic), trackIndex(index)
{
    looperEngine.basicEngine = &engine;
}

WaveformDisplay::WaveformDisplay(VampNetMultiTrackLooperEngine& engine, int index)
    : engineType(VampNet), trackIndex(index)
{
    looperEngine.vampNetEngine = &engine;
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
    
    if (engineType == Basic)
    {
        auto& track = looperEngine.basicEngine->get_track(trackIndex);
        tapeLoop = &track.m_tape_loop;
        writeHead = &track.m_write_head;
        isPlaying = &track.m_is_playing;
        readHead = &track.m_read_head;
    }
    else // VampNet
    {
        auto& track = looperEngine.vampNetEngine->get_track(trackIndex);
        tapeLoop = &track.m_record_buffer; // Show record buffer in waveform
        writeHead = &track.m_write_head;
        isPlaying = &track.m_is_playing;
        readHead = &track.m_record_read_head; // Use record read head for playhead position
    }
    
    const juce::ScopedLock sl(tapeLoop->m_lock);
    
    // Determine display length - use WrapPos if set (for duration control), otherwise use recordedLength
    size_t wrapPos = writeHead->get_wrap_pos();
    size_t displayLength = (wrapPos > 0) ? wrapPos : tapeLoop->m_recorded_length.load();
    
    if (writeHead->get_record_enable())
    {
        // Show current recording position
        displayLength = juce::jmax(displayLength, static_cast<size_t>(writeHead->get_pos()));
    }
    
    if (displayLength == 0 && !writeHead->get_record_enable())
    {
        // Draw empty waveform placeholder
        g.setColour(juce::Colour(0xff333333));
        g.drawRect(area);
        g.setColour(juce::Colour(0xfff3d430).withAlpha(0.5f)); // Yellow text
        g.drawText("no audio recorded", area, juce::Justification::centred);
        return;
    }
    
    auto& buffer = tapeLoop->get_buffer();
    if (buffer.empty())
        return;
    
    // Use buffer size if no recorded length yet
    if (displayLength == 0)
        displayLength = buffer.size();
    
    // Clamp displayLength to buffer size
    displayLength = juce::jmin(displayLength, buffer.size());
    
    // Draw waveform - use red-orange when recording, teal when playing
    g.setColour(writeHead->get_record_enable() ? juce::Colour(0xfff04e36) : juce::Colour(0xff1eb19d));
    
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
    TapeLoop* tapeLoop = nullptr;
    LooperWriteHead* writeHead = nullptr;
    std::atomic<bool>* isPlaying = nullptr;
    LooperReadHead* readHead = nullptr;
    
    if (engineType == Basic)
    {
        auto& track = looperEngine.basicEngine->get_track(trackIndex);
        tapeLoop = &track.m_tape_loop;
        writeHead = &track.m_write_head;
        isPlaying = &track.m_is_playing;
        readHead = &track.m_read_head;
    }
    else // VampNet
    {
        auto& track = looperEngine.vampNetEngine->get_track(trackIndex);
        tapeLoop = &track.m_record_buffer;
        writeHead = &track.m_write_head;
        isPlaying = &track.m_is_playing;
        readHead = &track.m_record_read_head;
    }
    
    // Show playhead if playing (even during recording before audio is recorded)
    if (!isPlaying->load())
        return;
    
    // Use WrapPos if set (for duration control), otherwise use recordedLength
    size_t wrapPos = writeHead->get_wrap_pos();
    size_t playbackLength = (wrapPos > 0) ? wrapPos : tapeLoop->m_recorded_length.load();
    
    // During new recording, use recordHead or playhead to show position
    if (playbackLength == 0)
    {
        if (writeHead->get_record_enable())
        {
            // Show playhead based on current recording position
            float playheadPosition = readHead->get_pos();
            float maxLength = static_cast<float>(tapeLoop->get_buffer_size());
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
    
    if (tapeLoop->get_buffer_size() == 0 || playbackLength == 0)
        return;
    
    float playheadPosition = readHead->get_pos();
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

