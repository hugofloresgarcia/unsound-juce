#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Shared/MidiLearnManager.h"

namespace WhAM
{
class SettingsComponent : public juce::Component
{
public:
    SettingsComponent(const juce::String& initialUrl,
                      std::function<void(const juce::String&)> onApplyCallback,
                      const std::vector<Shared::MidiMapping>& mappings)
        : onApply(std::move(onApplyCallback))
    {
        addAndMakeVisible(urlLabel);
        urlLabel.setText("gradio url:", juce::dontSendNotification);

        addAndMakeVisible(urlEditor);
        urlEditor.setText(initialUrl, juce::dontSendNotification);

        addAndMakeVisible(midiLabel);
        midiLabel.setText("MIDI bindings:", juce::dontSendNotification);

        midiText.setMultiLine(true);
        midiText.setReadOnly(true);
        midiText.setScrollbarsShown(true);

        juce::String midiInfo;
        if (mappings.empty())
        {
            midiInfo = "No MIDI bindings configured.";
        }
        else
        {
            midiInfo = "Current MIDI bindings:\n\n";
            for (const auto& mapping : mappings)
            {
                if (!mapping.isValid())
                    continue;

                juce::String typeStr = mapping.type == Shared::MidiMapping::MessageType::CC ? "CC" : "Note";
                juce::String modeStr = mapping.mode == Shared::MidiMapping::Mode::Toggle ? " (toggle)" : "";
                midiInfo += mapping.parameterId + " -> " + typeStr + " " + juce::String(mapping.number) + modeStr + "\n";
            }
        }

        midiText.setText(midiInfo, juce::dontSendNotification);

        // Attach the text editor to the viewport and give it an initial size
        midiViewport.setViewedComponent(&midiText, false);
        midiText.setSize(400, 200);
        addAndMakeVisible(midiViewport);

        addAndMakeVisible(cancelButton);
        cancelButton.setButtonText("cancel");
        cancelButton.onClick = [this]() {
            if (onCancel)
                onCancel();
        };

        addAndMakeVisible(saveButton);
        saveButton.setButtonText("save");
        saveButton.onClick = [this]() {
            auto text = urlEditor.getText().trim();
            if (onApply)
                onApply(text);
        };
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);

        auto urlArea = bounds.removeFromTop(30);
        urlLabel.setBounds(urlArea.removeFromLeft(100));
        urlEditor.setBounds(urlArea);

        bounds.removeFromTop(10);
        auto midiLabelArea = bounds.removeFromTop(20);
        midiLabel.setBounds(midiLabelArea);

        auto buttonArea = bounds.removeFromBottom(30);
        cancelButton.setBounds(buttonArea.removeFromLeft(80));
        buttonArea.removeFromLeft(10);
        saveButton.setBounds(buttonArea.removeFromLeft(80));

        midiViewport.setBounds(bounds);
        // Ensure the viewed component has a visible size inside the viewport
        midiText.setSize(bounds.getWidth(), bounds.getHeight());
    }

    void setOnCancel(std::function<void()> cb) { onCancel = std::move(cb); }
    void setOnApply(std::function<void(const juce::String&)> cb) { onApply = std::move(cb); }

    juce::String getUrlText() const { return urlEditor.getText().trim(); }

private:
    juce::Label urlLabel;
    juce::TextEditor urlEditor;

    juce::Label midiLabel;
    juce::TextEditor midiText;
    juce::Viewport midiViewport;

    juce::TextButton cancelButton;
    juce::TextButton saveButton;

    std::function<void(const juce::String&)> onApply;
    std::function<void()> onCancel;
};
} // namespace WhAM

