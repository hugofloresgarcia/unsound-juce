#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Shared
{

// Dialog for editing model parameters as JSON
class ModelParameterDialog : public juce::DialogWindow
{
public:
    ModelParameterDialog(const juce::String& modelName,
                        const juce::var& currentParams,
                        std::function<void(const juce::var&)> onAccept)
        : juce::DialogWindow("Configure " + modelName + " Parameters",
                           juce::Colours::darkgrey,
                           true),
          onAcceptCallback(onAccept)
    {
        auto* content = new ContentComponent(currentParams, [this](const juce::var& params) {
            if (onAcceptCallback)
                onAcceptCallback(params);
            setVisible(false);
        });
        
        setContentOwned(content, true);
        centreWithSize(getWidth(), getHeight());
        setResizable(true, true);
        setUsingNativeTitleBar(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }
    
    void updateParams(const juce::var& newParams)
    {
        if (auto* content = dynamic_cast<ContentComponent*>(getContentComponent()))
            content->updateParams(newParams);
    }

private:
    std::function<void(const juce::var&)> onAcceptCallback;

    class ContentComponent : public juce::Component
    {
    public:
        ContentComponent(const juce::var& currentParams,
                        std::function<void(const juce::var&)> onAccept)
            : onAcceptCallback(onAccept)
        {
            // Create text editor for JSON
            jsonEditor.setMultiLine(true);
            jsonEditor.setReturnKeyStartsNewLine(true);
            jsonEditor.setScrollbarsShown(true);
            jsonEditor.setCaretVisible(true);
            jsonEditor.setPopupMenuEnabled(true);
            jsonEditor.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
            
            updateParams(currentParams);
            addAndMakeVisible(jsonEditor);
            
            // Help text
            helpLabel.setText("Edit the JSON parameters below. Invalid JSON will be rejected.",
                            juce::dontSendNotification);
            helpLabel.setJustificationType(juce::Justification::centred);
            helpLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
            addAndMakeVisible(helpLabel);
            
            // Accept button
            acceptButton.setButtonText("Accept");
            acceptButton.onClick = [this] { acceptClicked(); };
            addAndMakeVisible(acceptButton);
            
            // Cancel button
            cancelButton.setButtonText("Cancel");
            cancelButton.onClick = [this] { 
                if (auto* dialog = findParentComponentOfClass<ModelParameterDialog>())
                    dialog->setVisible(false);
            };
            addAndMakeVisible(cancelButton);
            
            setSize(500, 400);
        }
        
        void updateParams(const juce::var& newParams)
        {
            juce::String jsonText = juce::JSON::toString(newParams, true);
            jsonEditor.setText(jsonText);
        }
        
        void resized() override
        {
            auto bounds = getLocalBounds().reduced(10);
            
            helpLabel.setBounds(bounds.removeFromTop(30));
            bounds.removeFromTop(5);
            
            auto buttonArea = bounds.removeFromBottom(30);
            bounds.removeFromBottom(5);
            
            cancelButton.setBounds(buttonArea.removeFromLeft(100));
            buttonArea.removeFromLeft(5);
            acceptButton.setBounds(buttonArea.removeFromLeft(100));
            
            jsonEditor.setBounds(bounds);
        }
        
    private:
        juce::TextEditor jsonEditor;
        juce::Label helpLabel;
        juce::TextButton acceptButton;
        juce::TextButton cancelButton;
        std::function<void(const juce::var&)> onAcceptCallback;
        
        void acceptClicked()
        {
            juce::String jsonText = jsonEditor.getText();
            juce::var parsedJson;
            
            auto parseResult = juce::JSON::parse(jsonText, parsedJson);
            if (parseResult.failed())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Invalid JSON",
                    "Failed to parse JSON: " + parseResult.getErrorMessage()
                );
                return;
            }
            
            if (onAcceptCallback)
                onAcceptCallback(parsedJson);
        }
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ContentComponent)
    };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModelParameterDialog)
};

} // namespace Shared

