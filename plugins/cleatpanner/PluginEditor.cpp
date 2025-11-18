#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CLEATPannerAudioProcessorEditor::CLEATPannerAudioProcessorEditor(CLEATPannerAudioProcessor& p)
    : AudioProcessorEditor(&p), m_processor_ref(p)
{
    setLookAndFeel(&m_look_and_feel);
    
    // Set up panner component with proper automation gesture handling
    m_panner_component.m_on_pan_change = [this](float x, float y) {
        auto* pan_x_param = m_processor_ref.get_apvts().getParameter("panX");
        auto* pan_y_param = m_processor_ref.get_apvts().getParameter("panY");
        
        if (pan_x_param != nullptr && pan_y_param != nullptr)
        {
            // Values are already normalized (0-1), so we can pass them directly
            // setValueNotifyingHost expects normalized values (0-1)
            pan_x_param->setValueNotifyingHost(x);
            pan_y_param->setValueNotifyingHost(y);
        }
    };
    
    addAndMakeVisible(m_panner_component);
    
    // Set up gain power slider
    m_gain_power_slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    m_gain_power_slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    m_gain_power_slider.setRange(0.1, 10.0, 0.01);
    m_gain_power_slider.setValue(1.0);
    addAndMakeVisible(m_gain_power_slider);
    
    m_gain_power_label.setText("Gain Power", juce::dontSendNotification);
    m_gain_power_label.attachToComponent(&m_gain_power_slider, false);
    m_gain_power_label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_gain_power_label);
    
    // Create parameter attachment for gain power slider
    m_gain_power_attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        m_processor_ref.get_apvts(), "gainPower", m_gain_power_slider);
    
    // Set initial pan position from parameters
    float pan_x = m_processor_ref.get_apvts().getRawParameterValue("panX")->load();
    float pan_y = m_processor_ref.get_apvts().getRawParameterValue("panY")->load();
    m_panner_component.set_pan_position(pan_x, pan_y, juce::dontSendNotification);
    
    // Start timer to sync UI with parameters
    startTimer(30); // ~30Hz update rate
    
    // Set editor size
    const int editor_width = 500;
    const int editor_height = 600;
    setSize(editor_width, editor_height);
}

CLEATPannerAudioProcessorEditor::~CLEATPannerAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void CLEATPannerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void CLEATPannerAudioProcessorEditor::resized()
{
    const int margin = 20;
    const int label_height = 20;
    const int slider_height = 100;
    const int panner_size = 400;
    
    auto bounds = getLocalBounds();
    
    // Panner component takes most of the space
    auto panner_bounds = bounds.removeFromTop(panner_size);
    panner_bounds.reduce(margin, margin);
    m_panner_component.setBounds(panner_bounds);
    
    // Gain power slider at the bottom
    auto slider_bounds = bounds.removeFromBottom(slider_height);
    slider_bounds.reduce(margin, margin);
    m_gain_power_slider.setBounds(slider_bounds);
}

void CLEATPannerAudioProcessorEditor::timerCallback()
{
    // Sync panner component with parameter values (in case changed externally)
    float pan_x = m_processor_ref.get_apvts().getRawParameterValue("panX")->load();
    float pan_y = m_processor_ref.get_apvts().getRawParameterValue("panY")->load();
    
    // Only update if different to avoid feedback loop
    if (std::abs(m_panner_component.get_pan_x() - pan_x) > 0.001f ||
        std::abs(m_panner_component.get_pan_y() - pan_y) > 0.001f)
    {
        m_panner_component.set_pan_position(pan_x, pan_y, juce::dontSendNotification);
    }
}

void CLEATPannerAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    // Check if mouse down is on the panner component
    auto panner_bounds = m_panner_component.getBounds();
    auto local_pos = e.getEventRelativeTo(this).getPosition();
    
    if (panner_bounds.contains(local_pos))
    {
        m_is_dragging_panner = true;
        
        // Begin automation gesture for both parameters
        // This tells the DAW that the user is starting to change these parameters
        auto* pan_x_param = m_processor_ref.get_apvts().getParameter("panX");
        auto* pan_y_param = m_processor_ref.get_apvts().getParameter("panY");
        
        if (pan_x_param != nullptr)
            pan_x_param->beginChangeGesture();
        if (pan_y_param != nullptr)
            pan_y_param->beginChangeGesture();
    }
    
    // Forward to parent for default handling (which will forward to child components)
    Component::mouseDown(e);
}

void CLEATPannerAudioProcessorEditor::mouseUp(const juce::MouseEvent& e)
{
    if (m_is_dragging_panner)
    {
        // End automation gesture for both parameters
        // This tells the DAW that the user has finished changing these parameters
        auto* pan_x_param = m_processor_ref.get_apvts().getParameter("panX");
        auto* pan_y_param = m_processor_ref.get_apvts().getParameter("panY");
        
        if (pan_x_param != nullptr)
            pan_x_param->endChangeGesture();
        if (pan_y_param != nullptr)
            pan_y_param->endChangeGesture();
        
        m_is_dragging_panner = false;
    }
    
    // Forward to parent for default handling
    Component::mouseUp(e);
}

