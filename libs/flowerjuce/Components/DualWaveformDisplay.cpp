#include "DualWaveformDisplay.h"
#include "../Engine/MultiTrackLooperEngine.h"
#include "../Engine/TapeLoop.h"
#include "../Engine/LooperWriteHead.h"
#include "../Engine/LooperReadHead.h"

using namespace Shared;

DualWaveformDisplay::DualWaveformDisplay(VampNetMultiTrackLooperEngine& engine, int index)
    : looperEngine(engine), trackIndex(index)
{
}

void DualWaveformDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Split the area in half - top for record buffer, bottom for output buffer
    auto recordArea = bounds.removeFromTop(bounds.getHeight() / 2);
    auto separatorY = recordArea.getBottom();
    auto outputArea = bounds;
    
    auto& track = looperEngine.getTrack(trackIndex);
    
    // Draw record buffer waveform (top)
    drawWaveform(g, recordArea, track.recordBuffer, track.recordReadHead, track.writeHead, true);
    drawPlayhead(g, recordArea, track.recordBuffer, track.recordReadHead, track.isPlaying.load());
    
    // Draw separator line between the two waveforms
    g.setColour(juce::Colour(0xff333333));
    g.drawLine(bounds.getX(), separatorY, bounds.getRight(), separatorY, 2.0f);
    
    // Draw output buffer waveform (bottom)
    drawWaveform(g, outputArea, track.outputBuffer, track.outputReadHead, track.writeHead, false);
    drawPlayhead(g, outputArea, track.outputBuffer, track.outputReadHead, track.isPlaying.load());
    
    // Draw labels
    g.setColour(juce::Colour(0xfff3d430));
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    g.drawText("record", recordArea.reduced(5), juce::Justification::topLeft);
    g.drawText("output", outputArea.reduced(5), juce::Justification::topLeft);
}

void DualWaveformDisplay::resized()
{
    // Nothing to do - we just paint into our bounds
}

void DualWaveformDisplay::drawWaveform(juce::Graphics& g, juce::Rectangle<int> area, TapeLoop& tapeLoop, LooperReadHead& readHead, LooperWriteHead& writeHead, bool isRecordBuffer)
{
    const juce::ScopedLock sl(tapeLoop.m_lock);
    
    // Show recording progress even if not fully recorded yet
    size_t displayLength = tapeLoop.m_recorded_length.load();
    
    if (writeHead.get_record_enable() && isRecordBuffer)
    {
        // Show current recording position (only for record buffer)
        displayLength = juce::jmax(displayLength, static_cast<size_t>(writeHead.get_pos()));
    }
    
    if (displayLength == 0 && !(writeHead.get_record_enable() && isRecordBuffer))
    {
        // Draw empty waveform placeholder
        g.setColour(juce::Colour(0xff333333));
        g.drawRect(area);
        g.setColour(juce::Colour(0xfff3d430).withAlpha(0.5f)); // Yellow text
        juce::String label = isRecordBuffer ? "no input recorded" : "no output generated";
        g.drawText(label, area, juce::Justification::centred);
        return;
    }
    
    auto& buffer = tapeLoop.get_buffer();
    if (buffer.empty())
        return;
    
    // Use buffer size if no recorded length yet
    if (displayLength == 0)
        displayLength = buffer.size();
    
    // Draw waveform - use red-orange for record buffer when recording, teal for output buffer
    juce::Colour waveformColor;
    if (isRecordBuffer)
    {
        waveformColor = writeHead.get_record_enable() ? juce::Colour(0xfff04e36) : juce::Colour(0xff1eb19d);
    }
    else
    {
        waveformColor = juce::Colour(0xff1eb19d); // Always teal for output buffer
    }
    g.setColour(waveformColor);
    
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

void DualWaveformDisplay::drawPlayhead(juce::Graphics& g, juce::Rectangle<int> waveformArea, TapeLoop& tapeLoop, LooperReadHead& readHead, bool isPlaying)
{
    // Show playhead if playing (even during recording before audio is recorded)
    if (!isPlaying)
        return;
    
    size_t recordedLength = tapeLoop.m_recorded_length.load();
    
    if (tapeLoop.get_buffer_size() == 0 || recordedLength == 0)
        return;
    
    float playheadPosition = readHead.get_pos();
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

