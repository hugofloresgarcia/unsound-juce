#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>
#include <flowerjuce/Engine/MultiTrackLooperEngine.h>

namespace WhAM
{

// Forward declarations
struct TokenBlock;
struct TokenGridData;
class LooperTrack;

// TokenVisualizerWindow - UI window for visualizing tokens
class TokenVisualizerWindow : public juce::DialogWindow
{
public:
    TokenVisualizerWindow(VampNetMultiTrackLooperEngine& engine, int numTracks, const std::vector<LooperTrack*>& tracks);
    ~TokenVisualizerWindow() override;
    
    void closeButtonPressed() override;
    
private:
    class TokenVisualizerComponent;
    TokenVisualizerComponent* contentComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TokenVisualizerWindow)
};

} // namespace WhAM

