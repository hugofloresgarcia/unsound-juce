/*
  ==============================================================================

    TapTempoController.h
    Created: 25 Nov 2025
    Author:  LayerCake

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

namespace layercake {

class TapTempoController
{
public:
    TapTempoController() = default;
    
    void setTempoCallback(std::function<void(float)> callback)
    {
        onTempoSet = callback;
    }

    void tap()
    {
        auto now = juce::Time::getMillisecondCounterHiRes();
        
        // Reset if too long between taps (e.g., 2 seconds)
        if (now - lastTapTime > 2000.0)
        {
            tapTimes.clear();
        }

        lastTapTime = now;
        tapTimes.add(now);

        // Keep only last 4 taps for average
        while (tapTimes.size() > 4)
            tapTimes.remove(0);

        if (tapTimes.size() >= 2)
        {
            double totalInterval = 0;
            for (int i = 1; i < tapTimes.size(); ++i)
            {
                totalInterval += (tapTimes[i] - tapTimes[i-1]);
            }
            
            double avgIntervalMs = totalInterval / (double)(tapTimes.size() - 1);
            if (avgIntervalMs > 0)
            {
                float bpm = (float)(60000.0 / avgIntervalMs);
                
                // constrain to reasonable limits
                bpm = juce::jlimit(30.0f, 300.0f, bpm);
                
                if (onTempoSet)
                    onTempoSet(bpm);
            }
        }
    }

private:
    std::function<void(float)> onTempoSet;
    juce::Array<double> tapTimes;
    double lastTapTime = 0;
};

} // namespace layercake

