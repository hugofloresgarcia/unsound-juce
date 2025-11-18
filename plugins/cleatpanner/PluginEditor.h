#pragma once

#include "PluginProcessor.h"
#include "../../libs/flowerjuce/Panners/Panner2DComponent.h"
#include "../../libs/flowerjuce/CustomLookAndFeel.h"

//==============================================================================
class CLEATPannerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               public juce::Timer
{
public:
    explicit CLEATPannerAudioProcessorEditor(CLEATPannerAudioProcessor&);
    ~CLEATPannerAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    
    void timerCallback() override;
    
    // Mouse event handling for automation gesture support
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    CLEATPannerAudioProcessor& m_processor_ref;
    CustomLookAndFeel m_look_and_feel;
    
    Panner2DComponent m_panner_component;
    juce::Slider m_gain_power_slider;
    juce::Label m_gain_power_label;
    
    // Parameter attachments
    // Pan X/Y are handled directly via callback (Panner2DComponent is not a Slider)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_gain_power_attachment;
    
    // Track if we're currently dragging (for automation gesture handling)
    bool m_is_dragging_panner{false};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CLEATPannerAudioProcessorEditor)
};

