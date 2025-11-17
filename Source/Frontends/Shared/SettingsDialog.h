#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Shared
{

// Dialog for application settings
class SettingsDialog : public juce::DialogWindow
{
public:
    SettingsDialog(double currentSmoothingTime,
                   std::function<void(double)> onSmoothingTimeChanged)
        : juce::DialogWindow("Settings",
                           juce::Colours::darkgrey,
                           true),
          onSmoothingTimeChangedCallback(onSmoothingTimeChanged)
    {
        auto* content = new ContentComponent(currentSmoothingTime,
            [this](double smoothingTime) {
                if (onSmoothingTimeChangedCallback)
                    onSmoothingTimeChangedCallback(smoothingTime);
            });
        
        setContentOwned(content, true);
        centreWithSize(400, 300);
        setResizable(true, true);
        setUsingNativeTitleBar(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }
    
    void updateSmoothingTime(double smoothingTime)
    {
        if (auto* content = dynamic_cast<ContentComponent*>(getContentComponent()))
            content->updateSmoothingTime(smoothingTime);
    }

private:
    std::function<void(double)> onSmoothingTimeChangedCallback;

    class ContentComponent : public juce::Component
    {
    public:
        ContentComponent(double currentSmoothingTime,
                        std::function<void(double)> onSmoothingTimeChanged)
            : onSmoothingTimeChangedCallback(onSmoothingTimeChanged),
              smoothingTimeSlider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight)
        {
            // Panner section label
            pannerLabel.setText("Panner", juce::dontSendNotification);
            auto font = juce::Font(juce::FontOptions()
                                  .withName(juce::Font::getDefaultMonospacedFontName())
                                  .withHeight(16.0f));
            pannerLabel.setFont(font.boldened());
            addAndMakeVisible(pannerLabel);
            
            // Smoothing time label
            smoothingLabel.setText("Trajectory Smoothing (seconds):", juce::dontSendNotification);
            smoothingLabel.setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(smoothingLabel);
            
            // Smoothing time slider (0.0 to 1.0 seconds)
            smoothingTimeSlider.setRange(0.0, 1.0, 0.01);
            smoothingTimeSlider.setValue(currentSmoothingTime);
            smoothingTimeSlider.setTextValueSuffix(" s");
            smoothingTimeSlider.onValueChange = [this] {
                if (onSmoothingTimeChangedCallback)
                    onSmoothingTimeChangedCallback(smoothingTimeSlider.getValue());
            };
            addAndMakeVisible(smoothingTimeSlider);
            
            // Close button
            closeButton.setButtonText("Close");
            closeButton.onClick = [this] { 
                if (auto* dialog = findParentComponentOfClass<SettingsDialog>())
                    dialog->setVisible(false);
            };
            addAndMakeVisible(closeButton);
            
            setSize(400, 300);
        }
        
        void updateSmoothingTime(double smoothingTime)
        {
            smoothingTimeSlider.setValue(smoothingTime, juce::dontSendNotification);
        }
        
        void resized() override
        {
            auto bounds = getLocalBounds().reduced(20);
            
            // Panner section
            pannerLabel.setBounds(bounds.removeFromTop(25));
            bounds.removeFromTop(10);
            
            smoothingLabel.setBounds(bounds.removeFromTop(20));
            bounds.removeFromTop(5);
            smoothingTimeSlider.setBounds(bounds.removeFromTop(30));
            bounds.removeFromTop(20);
            
            // Close button at bottom
            closeButton.setBounds(bounds.removeFromBottom(30).removeFromRight(80));
        }
        
    private:
        std::function<void(double)> onSmoothingTimeChangedCallback;
        juce::Label pannerLabel;
        juce::Label smoothingLabel;
        juce::Slider smoothingTimeSlider;
        juce::TextButton closeButton;
    };
};

} // namespace Shared

