#include "ModelParamsPopup.h"

namespace WhAM
{

namespace
{
constexpr int kPanelPadding = 20;
constexpr int kHeaderHeight = 60;
constexpr int kMinWidth = 500;
constexpr int kMinHeight = 350;
} // namespace

//==============================================================================
// ContentComponent - holds the actual controls shown inside the window
class ModelParamsPopup::ContentComponent : public juce::Component
{
public:
    ContentComponent(Shared::MidiLearnManager* midiManager,
                     const juce::String& trackPrefix,
                     std::function<void()> onCloseClickedIn)
        : parameterKnobs(midiManager, trackPrefix),
          onCloseClicked(std::move(onCloseClickedIn)),
          hasMidiManager(midiManager != nullptr)
    {
        titleLabel.setText("Model parameters", juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        addAndMakeVisible(titleLabel);

        bindAllButton.setButtonText("bind all");
        bindAllButton.onClick = [this]()
        {
            if (hasMidiManager)
                parameterKnobs.bindKnobsSequentially();
        };
        bindAllButton.setEnabled(hasMidiManager);
        bindAllButton.setTooltip("Assign CC 0-15 to these knobs");
        addAndMakeVisible(bindAllButton);

        unbindAllButton.setButtonText("unbind all");
        unbindAllButton.onClick = [this]()
        {
            if (hasMidiManager)
                parameterKnobs.clearAllMidiMappings();
        };
        unbindAllButton.setEnabled(hasMidiManager);
        addAndMakeVisible(unbindAllButton);

        closeButton.setButtonText("close");
        closeButton.onClick = [this]()
        {
            if (onCloseClicked)
                onCloseClicked();
        };
        addAndMakeVisible(closeButton);

        addAndMakeVisible(parameterKnobs);
    }

    Shared::ParameterKnobs& getKnobs() noexcept               { return parameterKnobs; }
    const Shared::ParameterKnobs& getKnobs() const noexcept   { return parameterKnobs; }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(kPanelPadding);

        auto headerArea = bounds.removeFromTop(kHeaderHeight);
        closeButton.setBounds(headerArea.removeFromRight(80));
        headerArea.removeFromRight(10);

        unbindAllButton.setBounds(headerArea.removeFromRight(100));
        headerArea.removeFromRight(5);
        bindAllButton.setBounds(headerArea.removeFromRight(90));
        headerArea.removeFromRight(10);

        titleLabel.setBounds(headerArea.removeFromTop(24));
        bounds.removeFromTop(10);

        parameterKnobs.setBounds(bounds);
    }

private:
    Shared::ParameterKnobs parameterKnobs;
    juce::Label titleLabel;
    juce::TextButton bindAllButton;
    juce::TextButton unbindAllButton;
    juce::TextButton closeButton;
    std::function<void()> onCloseClicked;
    const bool hasMidiManager = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ContentComponent)
};

//==============================================================================
// ModelParamsPopup
ModelParamsPopup::ModelParamsPopup(Shared::MidiLearnManager* midiManager,
                                   const juce::String& trackPrefix)
    : juce::DialogWindow("Model parameters",
                         juce::Colours::darkgrey,
                         true)
{
    contentComponent = new ContentComponent(midiManager, trackPrefix, [this]()
    {
        dismiss();
    });

    setContentOwned(contentComponent, true);
    setUsingNativeTitleBar(true);
    setResizable(true, true);

    auto displays = juce::Desktop::getInstance().getDisplays();
    if (auto* mainDisplay = displays.getPrimaryDisplay())
    {
        auto screenArea = mainDisplay->userArea;
        const int windowWidth  = juce::jlimit(kMinWidth,  screenArea.getWidth(),  screenArea.getWidth()  * 3 / 5);
        const int windowHeight = juce::jlimit(kMinHeight, screenArea.getHeight(), screenArea.getHeight() * 3 / 5);

        centreWithSize(windowWidth, windowHeight);
        setResizeLimits(kMinWidth, kMinHeight, screenArea.getWidth(), screenArea.getHeight());
    }
    else
    {
        centreWithSize(700, 500);
        setResizeLimits(kMinWidth, kMinHeight, 3840, 2160);
    }

    setVisible(false);
}

ModelParamsPopup::~ModelParamsPopup() = default;

Shared::ParameterKnobs& ModelParamsPopup::getKnobs()
{
    jassert(contentComponent != nullptr);
    return contentComponent->getKnobs();
}

const Shared::ParameterKnobs& ModelParamsPopup::getKnobs() const
{
    jassert(contentComponent != nullptr);
    return contentComponent->getKnobs();
}

void ModelParamsPopup::show(const juce::Rectangle<int>& anchorArea)
{
    positionRelativeTo(anchorArea);
    setVisible(true);
    toFront(true);
}

void ModelParamsPopup::dismiss()
{
    if (!isVisible())
        return;

    setVisible(false);

    if (onDismissed)
        onDismissed();
}

void ModelParamsPopup::closeButtonPressed()
{
    dismiss();
}

void ModelParamsPopup::positionRelativeTo(const juce::Rectangle<int>& anchorArea)
{
    auto displays = juce::Desktop::getInstance().getDisplays();
    if (auto* mainDisplay = displays.getPrimaryDisplay())
    {
        auto screenArea = mainDisplay->userArea;

        auto bounds = getBounds();
        if (bounds.isEmpty())
            bounds.setSize(kMinWidth, kMinHeight);

        if (!anchorArea.isEmpty())
            bounds.setCentre(anchorArea.getCentre());
        else
            bounds.setCentre(screenArea.getCentre());

        bounds = bounds.constrainedWithin(screenArea);
        setBounds(bounds);
    }
    else
    {
        if (getWidth() == 0 || getHeight() == 0)
            centreWithSize(kMinWidth, kMinHeight);
    }
}

} // namespace WhAM


