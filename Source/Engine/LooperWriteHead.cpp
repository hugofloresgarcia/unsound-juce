#include "LooperWriteHead.h"
#include <cmath>

LooperWriteHead::LooperWriteHead(TapeLoop& loop)
    : tapeLoop(loop)
{
}

bool LooperWriteHead::processSample(float inputSample, float currentPosition)
{
    
    const juce::ScopedLock sl(tapeLoop.lock);
    auto& buffer = tapeLoop.getBuffer();
    
    if (buffer.empty())
        return false;
    
    // Wrap position to buffer size
    size_t recordPos = static_cast<size_t>(std::fmod(currentPosition, buffer.size()));
    
    // Overdub: mix new input with existing audio
    float existingSample = buffer[recordPos];
    float mix = overdubMix.load();
    buffer[recordPos] = existingSample * mix + inputSample * (1.0f - mix);
    tapeLoop.recordedLength.store(std::max(tapeLoop.recordedLength.load(), recordPos + 1));

    // Update record head to track maximum position written to
    pos.store(recordPos + 1);
    
    return true;
}

void LooperWriteHead::finalizeRecording(float finalPosition)
{
    tapeLoop.hasRecorded.store(true);
    recordEnable.store(false); // Turn off record enable so UI reflects the change
    
    setWrapPos(static_cast<size_t>(finalPosition));
    juce::Logger::writeToLog("~~~ Finalized recording");
}

void LooperWriteHead::reset()
{
    pos.store(0);
    juce::Logger::writeToLog("~~~ Reset write head");
    // set wrapPos to the length of the tape loop
    setWrapPos(tapeLoop.getBufferSize());
}

