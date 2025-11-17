/*
  ==============================================================================

    Utils.h
    Created: [Date]
    Author: [Author]

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

namespace flowerjuce
{

/** Linear mapping function similar to SuperCollider's LinLin.
    
    Maps a value from one linear range to another linear range.
    
    @param value        The input value to map
    @param inMin        Minimum of the input range
    @param inMax        Maximum of the input range
    @param outMin       Minimum of the output range
    @param outMax       Maximum of the output range
    
    @returns The mapped value in the output range
    
    @code
    linlin(0.5f, 0.0f, 1.0f, 100.0f, 200.0f) == 150.0f
    linlin(25.0f, 0.0f, 100.0f, -1.0f, 1.0f) == -0.5f
    @endcode
*/
template <typename Type>
constexpr Type linlin (Type value, Type inMin, Type inMax, Type outMin, Type outMax)
{
    return juce::jmap (value, inMin, inMax, outMin, outMax);
}

} // namespace flowerjuce

