#include "LayerCakeProcessor.h"
#include "LayerCakeComponent.h"

namespace LayerCakeApp
{

LayerCakeProcessor::LayerCakeProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      m_apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Setup logging
    auto logDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library/Logs/LayerCake");
        
    if (!logDir.exists())
        logDir.createDirectory();
        
    auto logFile = logDir.getChildFile("LayerCake.log");
    
    m_logger = std::make_unique<juce::FileLogger>(logFile, "LayerCake Log");
    juce::Logger::setCurrentLogger(m_logger.get());
    
    DBG("LayerCakeProcessor initialized");
}

LayerCakeProcessor::~LayerCakeProcessor()
{
    DBG("LayerCakeProcessor destroyed");
    if (juce::Logger::getCurrentLogger() == m_logger.get())
        juce::Logger::setCurrentLogger(nullptr);
    m_logger = nullptr;
}

juce::AudioProcessorValueTreeState::ParameterLayout LayerCakeProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto makeFloat = [&](const juce::String& id, const juce::String& name, float min, float max, float def) {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id, name, min, max, def));
    };
    
    auto makeInt = [&](const juce::String& id, const juce::String& name, int min, int max, int def) {
         params.push_back(std::make_unique<juce::AudioParameterInt>(id, name, min, max, def));
    };

    auto makeBool = [&](const juce::String& id, const juce::String& name, bool def) {
        params.push_back(std::make_unique<juce::AudioParameterBool>(id, name, def));
    };

    // Main Controls
    makeFloat("layercake_master_gain", "Master Gain", -24.0f, 6.0f, 0.0f);
    makeFloat("layercake_position", "Position", 0.0f, 1.0f, 0.5f);
    makeFloat("layercake_duration", "Duration", 10.0f, 5000.0f, 300.0f);
    makeFloat("layercake_rate", "Rate", -24.0f, 24.0f, 0.0f);
    makeFloat("layercake_env", "Envelope", 0.0f, 1.0f, 0.5f);
    makeFloat("layercake_direction", "Direction", 0.0f, 1.0f, 0.5f);
    makeFloat("layercake_pan", "Pan", 0.0f, 1.0f, 0.5f);
    makeInt("layercake_layer_select", "Layer Select", 1, 6, 1);
    makeFloat("layercake_tempo", "Tempo", 10.0f, 600.0f, 140.0f);

    // LFOs (8 slots)
    for (int i = 0; i < 8; ++i)
    {
        juce::String prefix = "lfo" + juce::String(i + 1) + "_";
        juce::String namePrefix = "LFO " + juce::String(i + 1) + " ";
        
        makeBool(prefix + "enabled", namePrefix + "Enabled", true);
        makeInt(prefix + "mode", namePrefix + "Mode", 0, 10, 0); // Adjust max mode as needed
        makeFloat(prefix + "rate_hz", namePrefix + "Rate Hz", 0.01f, 20.0f, 1.0f);
        makeFloat(prefix + "clock_division", namePrefix + "Clock Div", 0.125f, 32.0f, 1.0f); 
        makeInt(prefix + "pattern_length", namePrefix + "Pattern Len", 0, 16, 0);
        
        makeFloat(prefix + "level", namePrefix + "Level", 0.0f, 1.0f, 1.0f);
        makeFloat(prefix + "width", namePrefix + "Width", 0.0f, 1.0f, 0.5f);
        makeFloat(prefix + "phase", namePrefix + "Phase", 0.0f, 1.0f, 0.0f);
        makeFloat(prefix + "delay", namePrefix + "Delay", 0.0f, 1.0f, 0.0f);
        makeInt(prefix + "delay_div", namePrefix + "Delay Div", 1, 16, 1);
        
        makeFloat(prefix + "slop", namePrefix + "Slop", 0.0f, 1.0f, 0.0f);
        
        makeInt(prefix + "euc_steps", namePrefix + "Euc Steps", 0, 32, 0);
        makeInt(prefix + "euc_trigs", namePrefix + "Euc Trigs", 0, 32, 0);
        makeInt(prefix + "euc_rot", namePrefix + "Euc Rot", 0, 32, 0);
        
        makeFloat(prefix + "rnd_skip", namePrefix + "Rnd Skip", 0.0f, 1.0f, 0.0f);
        makeInt(prefix + "loop_beats", namePrefix + "Loop Beats", 0, 64, 0);
        makeBool(prefix + "bipolar", namePrefix + "Bipolar", true);
    }

    return { params.begin(), params.end() };
}

void LayerCakeProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    m_engine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

void LayerCakeProcessor::releaseResources()
{
}

bool LayerCakeProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void LayerCakeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    // Sync with Host
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (pos->getBpm().hasValue())
            {
                const float newBpm = static_cast<float>(*pos->getBpm());
                if (std::abs(newBpm - m_engine.get_bpm()) > 0.001f)
                    m_engine.set_bpm(newBpm);
            }
            
            const bool isPlaying = pos->getIsPlaying();
            if (isPlaying != m_engine.is_transport_playing())
                m_engine.set_transport_playing(isPlaying);
        }
    }

    // Update Parameters from APVTS
    updateEngineParams();

    // Process Audio
    m_engine.process_block(buffer.getArrayOfReadPointers(), 
                           buffer.getNumChannels(),
                           buffer.getArrayOfWritePointers(),
                           buffer.getNumChannels(),
                           buffer.getNumSamples());
}

void LayerCakeProcessor::updateEngineParams()
{
    // Main params
    m_engine.set_master_gain_db(m_apvts.getRawParameterValue("layercake_master_gain")->load());
    m_engine.set_record_layer((int)m_apvts.getRawParameterValue("layercake_layer_select")->load() - 1);
    // ... manual grain state is complex, it might be better to let the UI push it or only update if changed
    // For now, basic params. Manual Grain State is typically UI driven or automated via params if we map them all. 
    // Ideally, m_engine exposes methods to update individual grain parameters or we reconstruct state here.
    
    // We need to update LFOs
    for (int i = 0; i < 8; ++i)
    {
        updateLfoParams(i);
    }
}

void LayerCakeProcessor::updateLfoParams(int i)
{
    juce::String prefix = "lfo" + juce::String(i + 1) + "_";
    
    bool enabled = (bool)m_apvts.getRawParameterValue(prefix + "enabled")->load();
    
    flower::LayerCakeLfoUGen generator;
    generator.set_mode(static_cast<flower::LfoWaveform>((int)m_apvts.getRawParameterValue(prefix + "mode")->load()));
    generator.set_rate_hz(m_apvts.getRawParameterValue(prefix + "rate_hz")->load());
    generator.set_clock_division(m_apvts.getRawParameterValue(prefix + "clock_division")->load());
    generator.set_pattern_length((int)m_apvts.getRawParameterValue(prefix + "pattern_length")->load());
    
    generator.set_level(m_apvts.getRawParameterValue(prefix + "level")->load());
    generator.set_width(m_apvts.getRawParameterValue(prefix + "width")->load());
    generator.set_phase_offset(m_apvts.getRawParameterValue(prefix + "phase")->load());
    generator.set_delay(m_apvts.getRawParameterValue(prefix + "delay")->load());
    generator.set_delay_div((int)m_apvts.getRawParameterValue(prefix + "delay_div")->load());
    
    generator.set_slop(m_apvts.getRawParameterValue(prefix + "slop")->load());
    
    generator.set_euclidean_steps((int)m_apvts.getRawParameterValue(prefix + "euc_steps")->load());
    generator.set_euclidean_triggers((int)m_apvts.getRawParameterValue(prefix + "euc_trigs")->load());
    generator.set_euclidean_rotation((int)m_apvts.getRawParameterValue(prefix + "euc_rot")->load());
    
    generator.set_random_skip(m_apvts.getRawParameterValue(prefix + "rnd_skip")->load());
    generator.set_loop_beats((int)m_apvts.getRawParameterValue(prefix + "loop_beats")->load());
    generator.set_bipolar((bool)m_apvts.getRawParameterValue(prefix + "bipolar")->load());
    
    m_engine.update_lfo_slot(i, generator, enabled);
}


juce::AudioProcessorEditor* LayerCakeProcessor::createEditor()
{
    return new LayerCakeComponent(*this);
}

bool LayerCakeProcessor::hasEditor() const
{
    return true;
}

const juce::String LayerCakeProcessor::getName() const
{
    return "LayerCake";
}

bool LayerCakeProcessor::acceptsMidi() const
{
    return false;
}

bool LayerCakeProcessor::producesMidi() const
{
    return false;
}

bool LayerCakeProcessor::isMidiEffect() const
{
    return false;
}

double LayerCakeProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LayerCakeProcessor::getNumPrograms()
{
    return 1;
}

int LayerCakeProcessor::getCurrentProgram()
{
    return 0;
}

void LayerCakeProcessor::setCurrentProgram(int index)
{
}

const juce::String LayerCakeProcessor::getProgramName(int index)
{
    return {};
}

void LayerCakeProcessor::changeProgramName(int index, const juce::String& newName)
{
}

void LayerCakeProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = m_apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LayerCakeProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(m_apvts.state.getType()))
            m_apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

} // namespace LayerCakeApp

