#pragma once

#include <juce_core/juce_core.h>
#include "../Shared/MidiLearnManager.h"
#include <vector>

namespace WhAM
{

struct SessionConfig
{
    struct TrackState
    {
        int trackIndex = 0;
        juce::var knobState;
        juce::var vampNetParams;
        bool autogenEnabled = false;
        bool useOutputAsInput = false;
        double levelDb = 0.0;
        juce::var pannerState;
    };

    juce::String gradioUrl;
    std::vector<TrackState> tracks;
    std::vector<Shared::MidiMapping> midiMappings;

    juce::var toVar() const;
    static juce::Result fromVar(const juce::var& data, SessionConfig& out);

    juce::Result saveToFile(const juce::File& file) const;
    static juce::Result loadFromFile(const juce::File& file, SessionConfig& out);
};

} // namespace WhAM


