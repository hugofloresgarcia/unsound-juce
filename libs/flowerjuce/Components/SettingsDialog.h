#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// Forward declaration
namespace Shared { class MidiLearnManager; }

namespace Shared
{

// Dialog for application settings
class SettingsDialog : public juce::DialogWindow
{
public:
    SettingsDialog(double currentSmoothingTime,
                   std::function<void(double)> onSmoothingTimeChanged,
                   const juce::String& currentGradioUrl = juce::String(),
                   std::function<void(const juce::String&)> onGradioUrlChanged = nullptr,
                   Shared::MidiLearnManager* midiLearnManager = nullptr,
                   const juce::String& currentTrajectoryDir = juce::String(),
                   std::function<void(const juce::String&)> onTrajectoryDirChanged = nullptr)
        : juce::DialogWindow("Settings",
                           juce::Colours::darkgrey,
                           true),
          onSmoothingTimeChangedCallback(onSmoothingTimeChanged),
          onGradioUrlChangedCallback(onGradioUrlChanged),
          onTrajectoryDirChangedCallback(onTrajectoryDirChanged),
          midiLearnManagerPtr(midiLearnManager)
    {
        auto* content = new ContentComponent(currentSmoothingTime,
            [this](double smoothingTime) {
                if (onSmoothingTimeChangedCallback)
                    onSmoothingTimeChangedCallback(smoothingTime);
            },
            currentGradioUrl,
            [this](const juce::String& url) {
                if (onGradioUrlChangedCallback)
                    onGradioUrlChangedCallback(url);
            },
            midiLearnManager,
            currentTrajectoryDir,
            [this](const juce::String& dir) {
                if (onTrajectoryDirChangedCallback)
                    onTrajectoryDirChangedCallback(dir);
            });
        
        setContentOwned(content, true);
        centreWithSize(500, 500);
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
    
    void updateGradioUrl(const juce::String& url)
    {
        if (auto* content = dynamic_cast<ContentComponent*>(getContentComponent()))
            content->updateGradioUrl(url);
    }
    
    void updateTrajectoryDir(const juce::String& dir)
    {
        if (auto* content = dynamic_cast<ContentComponent*>(getContentComponent()))
            content->updateTrajectoryDir(dir);
    }
    
    void refreshMidiInfo()
    {
        if (auto* content = dynamic_cast<ContentComponent*>(getContentComponent()))
            content->refreshMidiInfo();
    }

private:
    std::function<void(double)> onSmoothingTimeChangedCallback;
    std::function<void(const juce::String&)> onGradioUrlChangedCallback;
    std::function<void(const juce::String&)> onTrajectoryDirChangedCallback;
    Shared::MidiLearnManager* midiLearnManagerPtr;

    class ContentComponent : public juce::Component
    {
    public:
        ContentComponent(double currentSmoothingTime,
                        std::function<void(double)> onSmoothingTimeChanged,
                        const juce::String& currentGradioUrl,
                        std::function<void(const juce::String&)> onGradioUrlChanged,
                        Shared::MidiLearnManager* midiLearnManager,
                        const juce::String& currentTrajectoryDir,
                        std::function<void(const juce::String&)> onTrajectoryDirChanged)
            : onSmoothingTimeChangedCallback(onSmoothingTimeChanged),
              onGradioUrlChangedCallback(onGradioUrlChanged),
              onTrajectoryDirChangedCallback(onTrajectoryDirChanged),
              midiLearnManagerPtr(midiLearnManager),
              smoothingTimeSlider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight)
        {
            auto font = juce::Font(juce::FontOptions()
                                  .withName(juce::Font::getDefaultMonospacedFontName())
                                  .withHeight(16.0f));
            
            // Panner section label
            pannerLabel.setText("Panner", juce::dontSendNotification);
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
            
            // Gradio section (only if callback provided)
            if (onGradioUrlChangedCallback)
            {
                gradioLabel.setText("Gradio", juce::dontSendNotification);
                gradioLabel.setFont(font.boldened());
                addAndMakeVisible(gradioLabel);
                
                gradioUrlLabel.setText("Gradio URL:", juce::dontSendNotification);
                gradioUrlLabel.setJustificationType(juce::Justification::centredLeft);
                addAndMakeVisible(gradioUrlLabel);
                
                gradioUrlEditor.setText(currentGradioUrl);
                gradioUrlEditor.onTextChange = [this] {
                    if (onGradioUrlChangedCallback)
                    {
                        juce::String newUrl = gradioUrlEditor.getText().trim();
                        if (!newUrl.isEmpty())
                        {
                            if (!newUrl.endsWithChar('/'))
                                newUrl += "/";
                            onGradioUrlChangedCallback(newUrl);
                        }
                    }
                };
                addAndMakeVisible(gradioUrlEditor);
            }
            
            // Trajectory section (only if callback provided)
            if (onTrajectoryDirChangedCallback)
            {
                trajectoryLabel.setText("Trajectory", juce::dontSendNotification);
                trajectoryLabel.setFont(font.boldened());
                addAndMakeVisible(trajectoryLabel);
                
                trajectoryDirLabel.setText("Trajectory Directory:", juce::dontSendNotification);
                trajectoryDirLabel.setJustificationType(juce::Justification::centredLeft);
                addAndMakeVisible(trajectoryDirLabel);
                
                trajectoryDirEditor.setText(currentTrajectoryDir);
                trajectoryDirEditor.onTextChange = [this] {
                    if (onTrajectoryDirChangedCallback)
                    {
                        juce::String newDir = trajectoryDirEditor.getText().trim();
                        if (!newDir.isEmpty())
                        {
                            onTrajectoryDirChangedCallback(newDir);
                        }
                    }
                };
                addAndMakeVisible(trajectoryDirEditor);
            }
            
            // MIDI section (only if manager provided)
            if (midiLearnManagerPtr != nullptr)
            {
                midiLabel.setText("MIDI Learn", juce::dontSendNotification);
                midiLabel.setFont(font.boldened());
                addAndMakeVisible(midiLabel);
                
                midiInfoEditor.setReadOnly(true);
                midiInfoEditor.setMultiLine(true);
                midiInfoEditor.setCaretVisible(false);
                midiInfoEditor.setScrollbarsShown(true);
                refreshMidiInfo();
                addAndMakeVisible(midiInfoEditor);
            }
            
            // Close button
            closeButton.setButtonText("Close");
            closeButton.onClick = [this] { 
                if (auto* dialog = findParentComponentOfClass<SettingsDialog>())
                    dialog->setVisible(false);
            };
            addAndMakeVisible(closeButton);
            
            setSize(500, 500);
        }
        
        void updateSmoothingTime(double smoothingTime)
        {
            smoothingTimeSlider.setValue(smoothingTime, juce::dontSendNotification);
        }
        
        void updateGradioUrl(const juce::String& url)
        {
            if (gradioUrlEditor.isVisible())
                gradioUrlEditor.setText(url, juce::dontSendNotification);
        }
        
        void updateTrajectoryDir(const juce::String& dir)
        {
            if (trajectoryDirEditor.isVisible())
                trajectoryDirEditor.setText(dir, juce::dontSendNotification);
        }
        
        void refreshMidiInfo()
        {
            if (midiLearnManagerPtr == nullptr || !midiInfoEditor.isVisible())
                return;
                
            auto devices = midiLearnManagerPtr->getAvailableMidiDevices();
            auto mappings = midiLearnManagerPtr->getAllMappings();
            
            juce::String info = "MIDI Learn is enabled!\n\n";
            info += "How to use:\n";
            info += "1. Right-click any control\n";
            info += "2. Select 'MIDI Learn...' from the menu\n";
            info += "3. Move a MIDI controller to assign it\n";
            info += "   (or click/press ESC to cancel)\n\n";
            info += "Available MIDI devices:\n";
            if (devices.isEmpty())
                info += "  (none)\n";
            else
            {
                for (const auto& device : devices)
                    info += "  " + device + "\n";
            }
            info += "\nCurrent mappings: " + juce::String(mappings.size());
            
            midiInfoEditor.setText(info, juce::dontSendNotification);
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
            
            // Gradio section (if visible)
            if (gradioLabel.isVisible())
            {
                gradioLabel.setBounds(bounds.removeFromTop(25));
                bounds.removeFromTop(10);
                
                gradioUrlLabel.setBounds(bounds.removeFromTop(20));
                bounds.removeFromTop(5);
                gradioUrlEditor.setBounds(bounds.removeFromTop(25));
                bounds.removeFromTop(20);
            }
            
            // Trajectory section (if visible)
            if (trajectoryLabel.isVisible())
            {
                trajectoryLabel.setBounds(bounds.removeFromTop(25));
                bounds.removeFromTop(10);
                
                trajectoryDirLabel.setBounds(bounds.removeFromTop(20));
                bounds.removeFromTop(5);
                trajectoryDirEditor.setBounds(bounds.removeFromTop(25));
                bounds.removeFromTop(20);
            }
            
            // MIDI section (if visible)
            if (midiLabel.isVisible())
            {
                midiLabel.setBounds(bounds.removeFromTop(25));
                bounds.removeFromTop(10);
                
                midiInfoEditor.setBounds(bounds.removeFromTop(150));
                bounds.removeFromTop(20);
            }
            
            // Close button at bottom
            closeButton.setBounds(bounds.removeFromBottom(30).removeFromRight(80));
        }
        
    private:
        std::function<void(double)> onSmoothingTimeChangedCallback;
        std::function<void(const juce::String&)> onGradioUrlChangedCallback;
        std::function<void(const juce::String&)> onTrajectoryDirChangedCallback;
        Shared::MidiLearnManager* midiLearnManagerPtr;
        
        juce::Label pannerLabel;
        juce::Label smoothingLabel;
        juce::Slider smoothingTimeSlider;
        
        juce::Label gradioLabel;
        juce::Label gradioUrlLabel;
        juce::TextEditor gradioUrlEditor;
        
        juce::Label trajectoryLabel;
        juce::Label trajectoryDirLabel;
        juce::TextEditor trajectoryDirEditor;
        
        juce::Label midiLabel;
        juce::TextEditor midiInfoEditor;
        
        juce::TextButton closeButton;
    };
};

} // namespace Shared

