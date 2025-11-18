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
    // Determine how much audio we actually recorded.
    const size_t recordedLen = tapeLoop.recordedLength.load();

    // If nothing was ever written, just turn off recording and leave loop bounds unchanged.
    if (recordedLen == 0 || finalPosition <= 0.0f)
    {
        recordEnable.store(false); // Turn off record enable so UI reflects the change
        juce::Logger::writeToLog("~~~ FinalizeRecording called with no recorded audio; ignoring loop bounds");
        return;
    }

    // Clamp the final position to the range [1, recordedLen]
    size_t finalPosSamples = static_cast<size_t>(finalPosition);
    if (finalPosSamples == 0 || finalPosSamples > recordedLen)
        finalPosSamples = recordedLen;

    tapeLoop.hasRecorded.store(true);
    recordEnable.store(false); // Turn off record enable so UI reflects the change

    setWrapPos(finalPosSamples);
    juce::Logger::writeToLog("~~~ Finalized recording");
}

void LooperWriteHead::reset()
{
    pos.store(0);
    juce::Logger::writeToLog("~~~ Reset write head");
    // set wrapPos to the length of the tape loop
    setWrapPos(tapeLoop.getBufferSize());
}

