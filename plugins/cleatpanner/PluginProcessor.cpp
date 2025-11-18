#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CLEATPannerAudioProcessor::CLEATPannerAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::mono(), true)
        .withOutput("Output", juce::AudioChannelSet::discreteChannels(16), true)),
      m_apvts(*this, nullptr, "PARAMETERS",
              juce::AudioProcessorValueTreeState::ParameterLayout{
                  std::make_unique<juce::AudioParameterFloat>(
                      PARAM_PAN_X,
                      "Pan X",
                      juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                      0.5f),
                  std::make_unique<juce::AudioParameterFloat>(
                      PARAM_PAN_Y,
                      "Pan Y",
                      juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                      0.5f),
                  std::make_unique<juce::AudioParameterFloat>(
                      PARAM_GAIN_POWER,
                      "Gain Power",
                      juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f),
                      1.0f)
              })
{
}

CLEATPannerAudioProcessor::~CLEATPannerAudioProcessor()
{
}

//==============================================================================
const juce::String CLEATPannerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CLEATPannerAudioProcessor::acceptsMidi() const
{
    return false;
}

bool CLEATPannerAudioProcessor::producesMidi() const
{
    return false;
}

bool CLEATPannerAudioProcessor::isMidiEffect() const
{
    return false;
}

double CLEATPannerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int CLEATPannerAudioProcessor::getNumPrograms()
{
    return 1;
}

int CLEATPannerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void CLEATPannerAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String CLEATPannerAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void CLEATPannerAudioProcessor::changeProgramName(int index, const juce::String& new_name)
{
    juce::ignoreUnused(index, new_name);
}

//==============================================================================
void CLEATPannerAudioProcessor::prepareToPlay(double sample_rate, int samples_per_block)
{
    DBG("CLEATPannerAudioProcessor: prepareToPlay - sample_rate=" + juce::String(sample_rate) + ", samples_per_block=" + juce::String(samples_per_block));
    m_panner.prepare(sample_rate);
    update_panner_parameters();
}

void CLEATPannerAudioProcessor::releaseResources()
{
    DBG("CLEATPannerAudioProcessor: releaseResources");
}

bool CLEATPannerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Require mono input
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono())
        return false;
    
    // Require 16-channel output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::discreteChannels(16))
        return false;
    
    return true;
}

void CLEATPannerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midi_messages)
{
    juce::ignoreUnused(midi_messages);
    
    juce::ScopedNoDenormals no_denormals;
    
    // Get parameter values
    update_panner_parameters();
    
    // Clear output channels beyond input
    auto total_num_input_channels = getTotalNumInputChannels();
    auto total_num_output_channels = getTotalNumOutputChannels();
    
    for (auto i = total_num_input_channels; i < total_num_output_channels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    
    // Prepare input/output arrays for panner
    const float* input_channel_data[1] = { buffer.getReadPointer(0) };
    float* output_channel_data[16];
    
    for (int i = 0; i < 16; ++i)
    {
        if (i < total_num_output_channels)
            output_channel_data[i] = buffer.getWritePointer(i);
        else
            output_channel_data[i] = nullptr;
    }
    
    // Process with CLEAT panner
    m_panner.process_block(input_channel_data, 1, output_channel_data, 16, buffer.getNumSamples());
}

void CLEATPannerAudioProcessor::update_panner_parameters()
{
    float pan_x = m_apvts.getRawParameterValue(PARAM_PAN_X)->load();
    float pan_y = m_apvts.getRawParameterValue(PARAM_PAN_Y)->load();
    float gain_power = m_apvts.getRawParameterValue(PARAM_GAIN_POWER)->load();
    
    m_panner.set_pan(pan_x, pan_y);
    m_panner.set_gain_power(gain_power);
}

//==============================================================================
bool CLEATPannerAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* CLEATPannerAudioProcessor::createEditor()
{
    return new CLEATPannerAudioProcessorEditor(*this);
}

//==============================================================================
void CLEATPannerAudioProcessor::getStateInformation(juce::MemoryBlock& dest_data)
{
    auto state = m_apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest_data);
}

void CLEATPannerAudioProcessor::setStateInformation(const void* data, int size_in_bytes)
{
    std::unique_ptr<juce::XmlElement> xml_state(getXmlFromBinary(data, size_in_bytes));
    
    if (xml_state != nullptr)
    {
        if (xml_state->hasTagName(m_apvts.state.getType()))
        {
            m_apvts.replaceState(juce::ValueTree::fromXml(*xml_state));
            update_panner_parameters();
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CLEATPannerAudioProcessor();
}

