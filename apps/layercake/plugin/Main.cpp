#include <juce_audio_processors/juce_audio_processors.h>
#include "../core/LayerCakeProcessor.h"

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LayerCakeApp::LayerCakeProcessor();
}

