#include "LooperReadHead.h"
#include <cmath>

// TODO: Remove this debug macro after fixing segmentation fault
#define DEBUG_SEGFAULT 1
#if DEBUG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#else
#define DBG_SEGFAULT(msg)
#endif

LooperReadHead::LooperReadHead(TapeLoop& loop)
    : tapeLoop(loop)
{
    // Initialize mute ramp to 10ms at default sample rate (will be reset when device starts)
    double defaultSampleRate = sampleRate.load();
    muteGain.reset(defaultSampleRate, 0.01); // 10ms ramp
    muteGain.setCurrentAndTargetValue(1.0f); // Start unmuted
}

float LooperReadHead::processSample()
{   
    static int callCount = 0;
    callCount++;
    bool isFirstCall = (callCount == 1);
    
    if (isFirstCall)
        DBG_SEGFAULT("ENTRY: LooperReadHead::processSample");

    // Interpolate sample at current position
    if (isFirstCall)
        DBG_SEGFAULT("Calling interpolateSample, pos=" + juce::String(pos.load()));
    float sampleValue = interpolateSample(pos.load());
    if (isFirstCall)
        DBG_SEGFAULT("interpolateSample returned, value=" + juce::String(sampleValue));
    
    // Apply level gain (convert dB to linear)
    float gain = juce::Decibels::decibelsToGain(levelDb.load());
    sampleValue *= gain;
    
    // Apply mute ramp (smooth transition to avoid clicks)
    sampleValue *= muteGain.getNextValue();
    
    // Track level for VU meter (peak detection with decay)
    float absValue = std::abs(sampleValue);
    float currentLevel = levelMeter.load();
    if (absValue > currentLevel)
        levelMeter.store(absValue);
    else
        levelMeter.store(currentLevel * 0.999f); // Decay
    
    if (isFirstCall)
        DBG_SEGFAULT("EXIT: LooperReadHead::processSample");
    return sampleValue;
}

bool LooperReadHead::advance(float wrapPos)
{   
    static int callCount = 0;
    callCount++;
    bool isFirstCall = (callCount == 1);
    
    if (isFirstCall)
        DBG_SEGFAULT("ENTRY: LooperReadHead::advance, wrapPos=" + juce::String(wrapPos));
    
    // Safety check: if wrapPos is 0 or invalid, don't advance
    if (wrapPos < 0.0f)
    {
        juce::Logger::writeToLog("WARNING: Wrap position is 0 or invalid in advance");
        if (isFirstCall)
            DBG_SEGFAULT("wrapPos is invalid, returning false");
        return false;
    }
    
    if (isFirstCall)
        DBG_SEGFAULT("Loading position and speed");
    float currentPos = pos.load();
    float speed = playbackSpeed.load();
    currentPos += speed;

    if (isFirstCall)
        DBG_SEGFAULT("Checking wrap and calculating fmod");
    // check if we'll wrap  around the tape loop
    bool wrapped = currentPos >= wrapPos;
    currentPos = std::fmod(currentPos, wrapPos);
    
    if (isFirstCall)
        DBG_SEGFAULT("Storing new position");
    pos.store(currentPos);
    
    if (isFirstCall)
        DBG_SEGFAULT("EXIT: LooperReadHead::advance, wrapped=" + juce::String(wrapped ? "YES" : "NO"));
    return wrapped;
}

void LooperReadHead::setMuted(bool muted)
{
    isMuted.store(muted);
    // Set target value for smooth mute ramp (0.0 = muted, 1.0 = unmuted)
    muteGain.setTargetValue(muted ? 0.0f : 1.0f);
}

void LooperReadHead::setSampleRate(double rate)
{
    sampleRate.store(rate);
    resetMuteRamp(rate);
}

void LooperReadHead::resetMuteRamp(double sampleRate)
{
    // Reset mute ramp for new sample rate (10ms ramp)
    muteGain.reset(sampleRate, 0.01); // 10ms ramp
    // Set current and target to match current mute state
    bool currentlyMuted = isMuted.load();
    muteGain.setCurrentAndTargetValue(currentlyMuted ? 0.0f : 1.0f);
}

void LooperReadHead::reset()
{
    pos.store(0.0f);
}

void LooperReadHead::syncTo(float position)
{
    pos.store(position);
}

float LooperReadHead::interpolateSample(float position) const
{
    static int callCount = 0;
    callCount++;
    bool isFirstCall = (callCount == 1);
    
    if (isFirstCall)
        DBG_SEGFAULT("ENTRY: LooperReadHead::interpolateSample, position=" + juce::String(position));
    
    if (isFirstCall)
        DBG_SEGFAULT("Getting buffer reference");
    const auto& buffer = tapeLoop.getBuffer();
    
    if (isFirstCall)
        DBG_SEGFAULT("Buffer size=" + juce::String(buffer.size()));
    
    // Safety check: if buffer is empty, return silence
    if (buffer.empty()){
        juce::Logger::writeToLog("WARNING: Buffer is empty in interpolateSample");
        if (isFirstCall)
            DBG_SEGFAULT("Buffer is empty, returning 0.0f");
        return 0.0f;
    }
    
    if (isFirstCall)
        DBG_SEGFAULT("Calculating indices");
    size_t index0 = static_cast<size_t>(position) % buffer.size();
    size_t index1 = (index0 + 1) % buffer.size();
    float fraction = position - std::floor(position);
    
    if (isFirstCall)
        DBG_SEGFAULT("Accessing buffer[" + juce::String(index0) + "] and buffer[" + juce::String(index1) + "]");
    float result = buffer[index0] * (1.0f - fraction) + buffer[index1] * fraction;
    
    if (isFirstCall)
        DBG_SEGFAULT("EXIT: LooperReadHead::interpolateSample, result=" + juce::String(result));
    return result;
}

