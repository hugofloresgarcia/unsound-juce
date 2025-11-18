#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../libs/flowerjuce/Panners/CLEATPanner.h"

//==============================================================================
class CLEATPannerAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    CLEATPannerAudioProcessor();
    ~CLEATPannerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sample_rate, int samples_per_block) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& new_name) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& dest_data) override;
    void setStateInformation(const void* data, int size_in_bytes) override;

    // Parameter access
    juce::AudioProcessorValueTreeState& get_apvts() { return m_apvts; }

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState m_apvts;
    CLEATPanner m_panner;
    
    // Parameter IDs
    static constexpr const char* PARAM_PAN_X = "panX";
    static constexpr const char* PARAM_PAN_Y = "panY";
    static constexpr const char* PARAM_GAIN_POWER = "gainPower";
    
    // Update panner with current parameter values
    void update_panner_parameters();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CLEATPannerAudioProcessor)
};

