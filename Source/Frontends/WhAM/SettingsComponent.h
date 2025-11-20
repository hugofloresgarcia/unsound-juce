#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Shared/MidiLearnManager.h"

namespace WhAM
{

class SettingsComponent : public juce::Component
{
public:
    SettingsComponent(const juce::String& initialUrl,
                      const std::vector<Shared::MidiMapping>& initialMappings,
                      const juce::String& initialIoSummary)
    {
        urlEditor.setText(initialUrl, juce::dontSendNotification);
        cachedMappings = initialMappings;
        cachedIoSummary = initialIoSummary;

        setupTabButtons();
        setupGradioTab();
        setupMidiTab();
        setupIoTab();

        updateVisibility();
        refreshMidiRows();
        ioInfoBox.setText(cachedIoSummary, juce::dontSendNotification);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);

        auto tabsArea = bounds.removeFromTop(35);
        layoutTabButtons(tabsArea);

        bounds.removeFromTop(10);

        auto footerArea = bounds.removeFromBottom(40);
        cancelButton.setBounds(footerArea.removeFromLeft(100));
        if (activeTab == Tab::Gradio)
        {
            footerArea.removeFromLeft(10);
            saveButton.setBounds(footerArea.removeFromLeft(100));
            saveButton.setVisible(true);
        }
        else
        {
            saveButton.setVisible(false);
        }

        const int labelWidth = 120;
        auto contentArea = bounds;

        if (activeTab == Tab::Gradio)
        {
            auto urlArea = contentArea.removeFromTop(30);
            urlLabel.setBounds(urlArea.removeFromLeft(labelWidth));
            urlEditor.setBounds(urlArea);
        }
        else if (activeTab == Tab::Midi)
        {
            auto headerArea = contentArea.removeFromTop(30);
            midiLabel.setBounds(headerArea.removeFromLeft(labelWidth));
            unbindAllButton.setBounds(headerArea.removeFromRight(150));

            midiViewport.setBounds(contentArea);
            midiEmptyLabel.setBounds(midiListContent.getLocalBounds());
            relayoutMidiRows();
        }
        else if (activeTab == Tab::Io)
        {
            auto headerArea = contentArea.removeFromTop(30);
            ioLabel.setBounds(headerArea.removeFromLeft(labelWidth));
            refreshIoButton.setBounds(headerArea.removeFromRight(150));

            ioInfoBox.setBounds(contentArea);
        }
    }

    void setOnCancel(std::function<void()> cb) { onCancel = std::move(cb); }
    void setOnApply(std::function<void(const juce::String&)> cb) { onApply = std::move(cb); }
    void setOnUnbindMapping(std::function<void(const juce::String&)> cb) { onUnbindMapping = std::move(cb); }
    void setOnUnbindAll(std::function<void()> cb) { onUnbindAll = std::move(cb); }

    void setMappingsProvider(std::function<std::vector<Shared::MidiMapping>()> provider)
    {
        mappingsProvider = std::move(provider);
    }

    void setIoInfoProvider(std::function<juce::String()> provider)
    {
        ioInfoProvider = std::move(provider);
    }

    void refreshMidiMappings()
    {
        if (mappingsProvider)
            cachedMappings = mappingsProvider();

        refreshMidiRows();
        resized();
    }

    void refreshIoSummary()
    {
        if (ioInfoProvider)
            cachedIoSummary = ioInfoProvider();

        ioInfoBox.setText(cachedIoSummary, juce::dontSendNotification);
    }

    juce::String getUrlText() const { return urlEditor.getText().trim(); }

private:
    enum class Tab
    {
        Gradio,
        Midi,
        Io
    };

    class MidiMappingRow : public juce::Component
    {
    public:
        MidiMappingRow(const Shared::MidiMapping& mapping,
                       std::function<void(const juce::String&)> onUnbindCallback)
            : parameterId(mapping.parameterId),
              onUnbind(std::move(onUnbindCallback))
        {
            addAndMakeVisible(descriptionLabel);
            descriptionLabel.setJustificationType(juce::Justification::centredLeft);
            descriptionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            descriptionLabel.setText(buildDescription(mapping), juce::dontSendNotification);

            addAndMakeVisible(unbindButton);
            unbindButton.setButtonText("unbind");
            unbindButton.onClick = [this]() {
                if (onUnbind)
                    onUnbind(parameterId);
            };
        }

        void resized() override
        {
            auto rowBounds = getLocalBounds();
            auto buttonArea = rowBounds.removeFromRight(110);
            buttonArea = buttonArea.reduced(5, 0);
            unbindButton.setBounds(buttonArea);
            descriptionLabel.setBounds(rowBounds.reduced(5, 0));
        }

    private:
        static juce::String buildDescription(const Shared::MidiMapping& mapping)
        {
            juce::String typeStr = mapping.type == Shared::MidiMapping::MessageType::CC ? "CC" : "Note";
            juce::String modeStr = mapping.mode == Shared::MidiMapping::Mode::Toggle ? " (toggle)" : "";
            return mapping.parameterId + " -> " + typeStr + " " + juce::String(mapping.number) + modeStr;
        }

        juce::Label descriptionLabel;
        juce::TextButton unbindButton;
        juce::String parameterId;
        std::function<void(const juce::String&)> onUnbind;
    };

    Tab activeTab { Tab::Gradio };

    // Tabs
    juce::TextButton gradioTabButton { "Gradio" };
    juce::TextButton midiTabButton { "MIDI Binds" };
    juce::TextButton ioTabButton { "I/O" };

    // Gradio controls
    juce::Label urlLabel { {}, "gradio url:" };
    juce::TextEditor urlEditor;

    // Footer buttons
    juce::TextButton cancelButton { "cancel" };
    juce::TextButton saveButton { "save" };

    // MIDI tab
    juce::Label midiLabel { {}, "midi bindings:" };
    juce::Viewport midiViewport;
    juce::Component midiListContent;
    juce::Label midiEmptyLabel { {}, "No MIDI bindings configured." };
    juce::TextButton unbindAllButton { "unbind all" };
    std::vector<std::unique_ptr<MidiMappingRow>> midiRows;

    // I/O tab
    juce::Label ioLabel { {}, "audio devices:" };
    juce::TextEditor ioInfoBox;
    juce::TextButton refreshIoButton { "refresh" };

    // Data / callbacks
    std::function<void(const juce::String&)> onApply;
    std::function<void()> onCancel;
    std::function<void(const juce::String&)> onUnbindMapping;
    std::function<void()> onUnbindAll;
    std::function<std::vector<Shared::MidiMapping>()> mappingsProvider;
    std::function<juce::String()> ioInfoProvider;
    std::vector<Shared::MidiMapping> cachedMappings;
    juce::String cachedIoSummary;

    void setupTabButtons()
    {
        auto configureTab = [this](juce::TextButton& button, Tab tab) {
            button.setClickingTogglesState(true);
            button.setRadioGroupId(1001);
            button.onClick = [this, tab]() { selectTab(tab); };
            addAndMakeVisible(button);
        };

        configureTab(gradioTabButton, Tab::Gradio);
        configureTab(midiTabButton, Tab::Midi);
        configureTab(ioTabButton, Tab::Io);
        gradioTabButton.setToggleState(true, juce::dontSendNotification);
    }

    void setupGradioTab()
    {
        addAndMakeVisible(urlLabel);
        addAndMakeVisible(urlEditor);

        cancelButton.onClick = [this]() {
            if (onCancel)
                onCancel();
        };
        addAndMakeVisible(cancelButton);

        saveButton.onClick = [this]() {
            if (onApply)
                onApply(getUrlText());
        };
        addAndMakeVisible(saveButton);
    }

    void setupMidiTab()
    {
        addAndMakeVisible(midiLabel);
        midiViewport.setViewedComponent(&midiListContent, false);
        midiListContent.addAndMakeVisible(midiEmptyLabel);
        midiEmptyLabel.setJustificationType(juce::Justification::centred);
        midiEmptyLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        midiEmptyLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(midiViewport);

        unbindAllButton.onClick = [this]() {
            if (onUnbindAll)
                onUnbindAll();
            refreshMidiMappings();
        };
        addAndMakeVisible(unbindAllButton);
    }

    void setupIoTab()
    {
        addAndMakeVisible(ioLabel);
        ioInfoBox.setMultiLine(true);
        ioInfoBox.setReadOnly(true);
        ioInfoBox.setScrollbarsShown(true);
        addAndMakeVisible(ioInfoBox);

        refreshIoButton.onClick = [this]() { refreshIoSummary(); };
        addAndMakeVisible(refreshIoButton);
    }

    void layoutTabButtons(juce::Rectangle<int> bounds)
    {
        int tabWidth = bounds.getWidth() / 3;
        gradioTabButton.setBounds(bounds.removeFromLeft(tabWidth).reduced(2));
        midiTabButton.setBounds(bounds.removeFromLeft(tabWidth).reduced(2));
        ioTabButton.setBounds(bounds.reduced(2));
    }

    void selectTab(Tab tab)
    {
        if (activeTab == tab)
            return;

        activeTab = tab;
        gradioTabButton.setToggleState(tab == Tab::Gradio, juce::dontSendNotification);
        midiTabButton.setToggleState(tab == Tab::Midi, juce::dontSendNotification);
        ioTabButton.setToggleState(tab == Tab::Io, juce::dontSendNotification);

        updateVisibility();
        resized();
    }

    void updateVisibility()
    {
        bool showGradio = activeTab == Tab::Gradio;
        urlLabel.setVisible(showGradio);
        urlEditor.setVisible(showGradio);
        saveButton.setVisible(showGradio);

        bool showMidi = activeTab == Tab::Midi;
        midiLabel.setVisible(showMidi);
        midiViewport.setVisible(showMidi);
        unbindAllButton.setVisible(showMidi);

        bool showIo = activeTab == Tab::Io;
        ioLabel.setVisible(showIo);
        ioInfoBox.setVisible(showIo);
        refreshIoButton.setVisible(showIo);
    }

    void refreshMidiRows()
    {
        for (auto& row : midiRows)
            midiListContent.removeChildComponent(row.get());
        midiRows.clear();

        for (const auto& mapping : cachedMappings)
        {
            if (!mapping.isValid())
                continue;

            auto row = std::make_unique<MidiMappingRow>(mapping, [this](const juce::String& parameterId) {
                if (onUnbindMapping)
                    onUnbindMapping(parameterId);
                refreshMidiMappings();
            });
            midiListContent.addAndMakeVisible(row.get());
            midiRows.push_back(std::move(row));
        }

        midiEmptyLabel.setVisible(midiRows.empty());
        relayoutMidiRows();
    }

    void relayoutMidiRows()
    {
        const int rowHeight = 32;
        int y = 0;
        int contentWidth = midiViewport.getWidth();

        for (auto& row : midiRows)
        {
            row->setBounds(0, y, contentWidth, rowHeight);
            y += rowHeight + 4;
        }

        int viewHeight = juce::jmax(y, midiViewport.getHeight());
        midiListContent.setSize(contentWidth, viewHeight);
        midiEmptyLabel.setBounds(midiListContent.getLocalBounds());
    }
};

} // namespace WhAM
