#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <flowerjuce/LayerCakeEngine/LayerCakeEngine.h>

namespace LayerCakeApp
{

class LayerCakeProcessor : public juce::AudioProcessor
{
public:
    LayerCakeProcessor();
    ~LayerCakeProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    LayerCakeEngine& getEngine() { return m_engine; }
    juce::AudioProcessorValueTreeState& getAPVTS() { return m_apvts; }

private:
    std::unique_ptr<juce::FileLogger> m_logger;
    LayerCakeEngine m_engine;
    juce::AudioProcessorValueTreeState m_apvts;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateEngineParams();
    void updateLfoParams(int slotIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayerCakeProcessor)
};

} // namespace LayerCakeApp

