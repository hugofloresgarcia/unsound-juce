#include "LibraryBrowserWindow.h"
#include "LayerCakeLookAndFeel.h"
#include <utility>

namespace
{
constexpr int kRowHeight = 24; // Revert to compact height for consistent theming
} // namespace

class LibraryRowButtonLookAndFeel : public LayerCakeLookAndFeel // Inherit from main LNF
{
public:
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override
    {
        juce::ignoreUnused(button, buttonHeight);
        // We need to access makeMonoFont, but it's private in LayerCakeLookAndFeel.
        // However, we can just use the public method from base class if we made it protected/public,
        // or replicate it. Let's use the public methods that are available or recreate FontOptions.
        
        // Since makeMonoFont is private, we recreate the font logic here to match the style
        juce::FontOptions options(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::bold);
        return juce::Font(options);
    }
};

class LibraryBrowserComponent::LibraryRowComponent : public juce::Component,
                                                    private juce::Button::Listener
{
public:
    LibraryRowComponent()
        : m_save_button("sv"),
          m_load_button("ld"),
          m_delete_button("x")
    {
        addAndMakeVisible(m_name_label);
        m_name_label.setJustificationType(juce::Justification::centredLeft);
        
        juce::FontOptions options(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain);
        m_name_label.setFont(juce::Font(options));

        for (auto* button : { &m_save_button, &m_load_button, &m_delete_button })
        {
            button->setColour(juce::TextButton::buttonColourId,
                              juce::Colours::transparentWhite);
            button->setColour(juce::TextButton::textColourOffId,
                              juce::Colours::white.withAlpha(0.8f));
            button->addListener(this);
            button->setWantsKeyboardFocus(false);
            button->setLookAndFeel(&m_button_lnf);
            addAndMakeVisible(button);
        }
    }
    
    ~LibraryRowComponent() override
    {
        for (auto* button : { &m_save_button, &m_load_button, &m_delete_button })
            button->setLookAndFeel(nullptr);
    }

    void setRowName(const juce::String& name)
    {
        m_name_label.setText(name, juce::dontSendNotification);
    }

    void setCallbacks(std::function<void()> on_save,
                      std::function<void()> on_load,
                      std::function<void()> on_delete)
    {
        m_on_save = std::move(on_save);
        m_on_load = std::move(on_load);
        m_on_delete = std::move(on_delete);
    }

    void resized() override
    {
        const int margin = 2;
        const int buttonWidth = 24; // Compact button width
        const int buttonSpacing = 2;

        auto bounds = getLocalBounds().reduced(margin);
        auto buttonArea = bounds.removeFromRight(3 * buttonWidth + 2 * buttonSpacing);
        m_save_button.setBounds(buttonArea.removeFromLeft(buttonWidth));
        buttonArea.removeFromLeft(buttonSpacing);
        m_load_button.setBounds(buttonArea.removeFromLeft(buttonWidth));
        buttonArea.removeFromLeft(buttonSpacing);
        m_delete_button.setBounds(buttonArea.removeFromLeft(buttonWidth));
        m_name_label.setBounds(bounds);
    }

private:
    void buttonClicked(juce::Button* button) override
    {
        if (button == &m_save_button)
        {
            if (m_on_save)
                m_on_save();
            else
                DBG("LibraryBrowserComponent::LibraryRowComponent missing save callback");
        }
        else if (button == &m_load_button)
        {
            if (m_on_load)
                m_on_load();
            else
                DBG("LibraryBrowserComponent::LibraryRowComponent missing load callback");
        }
        else if (button == &m_delete_button)
        {
            if (m_on_delete)
                m_on_delete();
            else
                DBG("LibraryBrowserComponent::LibraryRowComponent missing delete callback");
        }
    }

    juce::Label m_name_label;
    juce::TextButton m_save_button;
    juce::TextButton m_load_button;
    juce::TextButton m_delete_button;
    std::function<void()> m_on_save;
    std::function<void()> m_on_load;
    std::function<void()> m_on_delete;
    LibraryRowButtonLookAndFeel m_button_lnf;
};

LibraryBrowserComponent::ColumnModel::ColumnModel(LibraryBrowserComponent& owner, ColumnType type)
    : m_owner(owner),
      m_type(type)
{
}

int LibraryBrowserComponent::ColumnModel::getNumRows()
{
    return m_owner.names_for(m_type).size();
}

juce::Component* LibraryBrowserComponent::ColumnModel::refreshComponentForRow(int rowNumber,
                                                                              bool isRowSelected,
                                                                              juce::Component* existingComponent)
{
    juce::ignoreUnused(isRowSelected);
    auto* row = dynamic_cast<LibraryRowComponent*>(existingComponent);
    if (row == nullptr)
        row = new LibraryRowComponent();

    const auto& names = m_owner.names_for(m_type);
    if (!juce::isPositiveAndBelow(rowNumber, names.size()))
    {
        row->setRowName({});
        row->setCallbacks(nullptr, nullptr, nullptr);
        row->setVisible(false);
        if (!m_reported_invalid_row)
        {
            DBG("LibraryBrowserComponent::ColumnModel early return invalid row="
                + juce::String(rowNumber)
                + " column="
                + LibraryBrowserComponent::column_title(m_type));
            m_reported_invalid_row = true;
        }
        return row;
    }

    m_reported_invalid_row = false;
    row->setVisible(true);
    const auto name = names[rowNumber];
    row->setRowName(name);

    row->setCallbacks(
        [this, name]() { m_owner.handle_row_action(m_type, name, RowAction::Save); },
        [this, name]() { m_owner.handle_row_action(m_type, name, RowAction::Load); },
        [this, name]() { m_owner.handle_row_action(m_type, name, RowAction::Delete); });

    return row;
}

void LibraryBrowserComponent::ColumnModel::paintListBoxItem(int rowNumber,
                                                            juce::Graphics& g,
                                                            int width,
                                                            int height,
                                                            bool rowIsSelected)
{
    juce::ignoreUnused(rowNumber);
    const auto base = m_owner.getLookAndFeel().findColour(juce::Slider::backgroundColourId);
    const auto accent = m_owner.getLookAndFeel().findColour(juce::Slider::trackColourId);
    g.setColour(rowIsSelected ? accent.withAlpha(0.25f) : base.withAlpha(0.1f));
    g.fillRect(0, 0, width, height);
}

LibraryBrowserComponent::LibraryBrowserComponent(LayerCakeLibraryManager& manager,
                                                 std::function<LayerCakePresetData()> capture_pattern_fn,
                                                 std::function<LayerBufferArray()> capture_layers_fn,
                                                 std::function<void(const LayerCakePresetData&)> apply_pattern_fn,
                                                 std::function<void(const LayerBufferArray&)> apply_layers_fn,
                                                 std::function<LayerCakePresetData()> capture_knobset_fn,
                                                 std::function<void(const LayerCakePresetData&)> apply_knobset_fn)
    : m_manager(manager),
      m_capture_pattern_fn(std::move(capture_pattern_fn)),
      m_capture_layers_fn(std::move(capture_layers_fn)),
      m_apply_pattern_fn(std::move(apply_pattern_fn)),
      m_apply_layers_fn(std::move(apply_layers_fn)),
      m_capture_knobset_fn(std::move(capture_knobset_fn)),
      m_apply_knobset_fn(std::move(apply_knobset_fn))
{
    const juce::Colour paletteBorder(0xfffd5e53);
    const juce::Colour knobsetBorder(0xfff2b950);
    const juce::Colour sceneBorder(0xff35c0ff);

    auto configureColumn = [this](ColumnWidgets& widgets,
                                  ColumnType type,
                                  const juce::String& placeholder,
                                  juce::Colour borderColour)
    {
        widgets.title.setText(column_title(type).toLowerCase(), juce::dontSendNotification);
        widgets.title.setJustificationType(juce::Justification::centred);
        widgets.title.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)));
        addAndMakeVisible(widgets.title);

        format_name_editor(widgets.name_editor);
        widgets.name_editor.setTextToShowWhenEmpty(placeholder, juce::Colours::darkgrey);
        addAndMakeVisible(widgets.name_editor);

        widgets.save_button.setButtonText("save");
        widgets.save_button.addListener(this);
        widgets.save_button.setWantsKeyboardFocus(false);
        widgets.save_button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        widgets.save_button.setColour(juce::TextButton::buttonOnColourId, borderColour.withAlpha(0.4f));
        widgets.save_button.setColour(juce::ComboBox::outlineColourId, borderColour);
        widgets.save_button.setColour(juce::TextButton::textColourOffId, borderColour);
        widgets.save_button.setColour(juce::TextButton::textColourOnId, borderColour);
        addAndMakeVisible(widgets.save_button);

        widgets.list_box.setRowHeight(kRowHeight);
        widgets.list_box.setOutlineThickness(2);
        widgets.list_box.setColour(juce::ListBox::outlineColourId, borderColour);
        widgets.model = std::make_unique<ColumnModel>(*this, type);
        widgets.list_box.setModel(widgets.model.get());
        addAndMakeVisible(widgets.list_box);
    };

    configureColumn(m_palette_widgets, ColumnType::Palette, "new palette name", paletteBorder);
    // Pattern column removed
    configureColumn(m_knobset_widgets, ColumnType::Knobset, "new knobset name", knobsetBorder);
    configureColumn(m_scene_widgets, ColumnType::Scene, "new scene name", sceneBorder);

    refresh_lists();
}

void LibraryBrowserComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    auto& laf = getLookAndFeel();
    const auto background = laf.findColour(juce::ResizableWindow::backgroundColourId).darker(0.35f);
    const auto frame = laf.findColour(juce::Slider::rotarySliderOutlineColourId);
    const auto glow = laf.findColour(juce::Slider::trackColourId).withAlpha(0.08f);

    g.setColour(background);
    g.fillRoundedRectangle(bounds, 12.0f);

    g.setColour(glow);
    g.fillEllipse(bounds.withSizeKeepingCentre(bounds.getWidth() * 0.7f, bounds.getHeight() * 0.7f));

    g.setColour(frame);
    g.drawRoundedRectangle(bounds, 12.0f, 1.6f);
}

void LibraryBrowserComponent::resized()
{
    const int marginOuter = 8;
    const int rowSpacing = 8;
    const int titleHeight = 24; // Increased from 18
    const int titleVerticalPadding = 4;
    const int editorHeight = 32; // Increased from 22
    const int editorSpacing = 8; // Increased from 4
    const int listSpacing = 4;
    const int minListHeight = 100; // Increased from 60

    auto bounds = getLocalBounds().reduced(marginOuter);
    if (bounds.isEmpty())
        return;

    // Vertical layout - stack columns on top of each other
    const int rowCount = 3;
    const int totalSpacing = rowSpacing * (rowCount - 1);
    const int availableHeight = bounds.getHeight() - totalSpacing;
    const int baseRowHeight = availableHeight / rowCount;

    auto layoutRow = [&](ColumnWidgets& widgets, juce::Rectangle<int> rowBounds)
    {
        auto row_area = rowBounds;
        auto title_area = row_area.removeFromTop(titleHeight);
        widgets.title.setBounds(title_area.reduced(0, titleVerticalPadding));
        row_area.removeFromTop(editorSpacing);
        auto editorRow = row_area.removeFromTop(editorHeight);
        const int buttonWidth = 60; // Increased from 36
        auto buttonArea = editorRow.removeFromRight(buttonWidth);
        widgets.save_button.setBounds(buttonArea);
        widgets.name_editor.setBounds(editorRow.reduced(0, 1));
        row_area.removeFromTop(listSpacing);
        // Ensure minimum list height
        if (row_area.getHeight() >= minListHeight)
            widgets.list_box.setBounds(row_area);
        else
            widgets.list_box.setBounds(row_area.withHeight(minListHeight));
    };

    auto rowArea = bounds;
    auto layoutNext = [&](ColumnWidgets& widgets, bool isLast)
    {
        auto rowBounds = rowArea.removeFromTop(baseRowHeight);
        layoutRow(widgets, rowBounds);
        if (!isLast)
            rowArea.removeFromTop(rowSpacing);
    };

    layoutNext(m_palette_widgets, false);
    layoutNext(m_knobset_widgets, false);
    layoutNext(m_scene_widgets, true);
}

void LibraryBrowserComponent::buttonClicked(juce::Button* button)
{
    if (button == &m_palette_widgets.save_button)
        handle_new_save(ColumnType::Palette);
    else if (button == &m_scene_widgets.save_button)
        handle_new_save(ColumnType::Scene);
    else if (button == &m_knobset_widgets.save_button)
        handle_new_save(ColumnType::Knobset);
}

void LibraryBrowserComponent::refresh_lists()
{
    m_manager.refresh();
    m_palette_widgets.list_box.updateContent();
    m_knobset_widgets.list_box.updateContent();
    m_scene_widgets.list_box.updateContent();
    repaint();
}

void LibraryBrowserComponent::handle_new_save(ColumnType type)
{
    auto& widgets = widgets_for(type);
    const juce::String name = widgets.name_editor.getText().trim();
    if (name.isEmpty())
    {
        DBG("LibraryBrowserComponent::handle_new_save missing name");
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               column_title(type),
                                               "Enter a name before saving.");
        return;
    }

    handle_row_action(type, name, RowAction::Save);
    widgets.name_editor.clear();
}

void LibraryBrowserComponent::handle_row_action(ColumnType type,
                                                const juce::String& name,
                                                RowAction action)
{
    if (name.isEmpty())
    {
        DBG("LibraryBrowserComponent::handle_row_action empty name");
        return;
    }

    auto showError = [&](const juce::String& message)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               column_title(type),
                                               message);
    };

    switch (type)
    {
        case ColumnType::Palette:
        {
            if (action == RowAction::Save)
            {
                if (!m_capture_layers_fn)
                {
                    DBG("LibraryBrowserComponent missing capture_layers_fn");
                    return;
                }
                const auto layers = m_capture_layers_fn();
                if (!m_manager.save_palette(name, layers))
                {
                    showError("Unable to store palette '" + name + "'.");
                    DBG("LibraryBrowserComponent failed saving palette " + name);
                    return;
                }
                refresh_lists();
            }
            else if (action == RowAction::Load)
            {
                LayerBufferArray layers{};
                if (!m_manager.load_palette(name, layers))
                {
                    showError("Unable to load palette '" + name + "'.");
                    DBG("LibraryBrowserComponent failed loading palette " + name);
                    return;
                }
                if (m_apply_layers_fn)
                    m_apply_layers_fn(layers);
                else
                    DBG("LibraryBrowserComponent missing apply_layers_fn");
            }
            else
            {
                if (!juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                        column_title(type),
                                                        "Delete palette '" + name + "'?",
                                                        "Delete",
                                                        "Cancel"))
                {
                    DBG("LibraryBrowserComponent palette delete cancelled for " + name);
                    return;
                }
                if (!m_manager.delete_palette(name))
                {
                    showError("Unable to delete palette '" + name + "'.");
                    DBG("LibraryBrowserComponent failed deleting palette " + name);
                    return;
                }
                refresh_lists();
            }
            break;
        }
        case ColumnType::Knobset:
        {
            if (action == RowAction::Save)
            {
                if (!m_capture_knobset_fn)
                {
                    DBG("LibraryBrowserComponent missing capture_knobset_fn");
                    return;
                }
                const auto data = m_capture_knobset_fn();
                if (!m_manager.save_knobset(name, data))
                {
                    showError("Unable to store knobset '" + name + "'.");
                    DBG("LibraryBrowserComponent failed saving knobset " + name);
                    return;
                }
                refresh_lists();
            }
            else if (action == RowAction::Load)
            {
                LayerCakePresetData data;
                if (!m_manager.load_knobset(name, data))
                {
                    showError("Unable to load knobset '" + name + "'.");
                    DBG("LibraryBrowserComponent failed loading knobset " + name);
                    return;
                }
                if (m_apply_knobset_fn)
                    m_apply_knobset_fn(data);
                else
                    DBG("LibraryBrowserComponent missing apply_knobset_fn");
            }
            else
            {
                if (!juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                        column_title(type),
                                                        "Delete knobset '" + name + "'?",
                                                        "Delete",
                                                        "Cancel"))
                {
                    DBG("LibraryBrowserComponent knobset delete cancelled for " + name);
                    return;
                }
                if (!m_manager.delete_knobset(name))
                {
                    showError("Unable to delete knobset '" + name + "'.");
                    DBG("LibraryBrowserComponent failed deleting knobset " + name);
                    return;
                }
                refresh_lists();
            }
            break;
        }
        case ColumnType::Scene:
        {
            if (action == RowAction::Save)
            {
                // For Scene, we capture both knobset (pattern) and layers
                // NOTE: m_capture_pattern_fn was passed the knobset capture function in MainComponent/LayerCakeComponent
                if (!m_capture_pattern_fn || !m_capture_layers_fn)
                {
                    DBG("LibraryBrowserComponent missing capture callbacks for scene");
                    return;
                }
                const auto data = m_capture_pattern_fn();
                const auto layers = m_capture_layers_fn();
                if (!m_manager.save_scene(name, data, layers))
                {
                    showError("Unable to store scene '" + name + "'.");
                    DBG("LibraryBrowserComponent failed saving scene " + name);
                    return;
                }
                refresh_lists();
            }
            else if (action == RowAction::Load)
            {
                LayerCakePresetData data;
                LayerBufferArray layers{};
                if (!m_manager.load_scene(name, data, layers))
                {
                    showError("Unable to load scene '" + name + "'.");
                    DBG("LibraryBrowserComponent failed loading scene " + name);
                    return;
                }
                if (m_apply_pattern_fn) // Maps to apply_knobset in Component
                    m_apply_pattern_fn(data);
                else
                    DBG("LibraryBrowserComponent missing apply_pattern_fn");
                
                if (m_apply_layers_fn)
                    m_apply_layers_fn(layers);
                else
                    DBG("LibraryBrowserComponent missing apply_layers_fn");
            }
            else
            {
                if (!juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                        column_title(type),
                                                        "Delete scene '" + name + "'?",
                                                        "Delete",
                                                        "Cancel"))
                {
                    DBG("LibraryBrowserComponent scene delete cancelled for " + name);
                    return;
                }
                if (!m_manager.delete_scene(name))
                {
                    showError("Unable to delete scene '" + name + "'.");
                    DBG("LibraryBrowserComponent failed deleting scene " + name);
                    return;
                }
                refresh_lists();
            }
            break;
        }
        default:
            break;
    }
}

const juce::StringArray& LibraryBrowserComponent::names_for(ColumnType type) const
{
    switch (type)
    {
        case ColumnType::Palette: return m_manager.get_palettes();
        case ColumnType::Knobset: return m_manager.get_knobsets();
        case ColumnType::Scene: return m_manager.get_scenes();
        default: break;
    }
    jassertfalse;
    static juce::StringArray empty;
    return empty;
}

LibraryBrowserComponent::ColumnWidgets& LibraryBrowserComponent::widgets_for(ColumnType type)
{
    switch (type)
    {
        case ColumnType::Palette: return m_palette_widgets;
        case ColumnType::Knobset: return m_knobset_widgets;
        case ColumnType::Scene: return m_scene_widgets;
        default: break;
    }
    jassertfalse;
    return m_knobset_widgets;
}

void LibraryBrowserComponent::format_name_editor(juce::TextEditor& editor) const
{
    editor.setSelectAllWhenFocused(true);
    editor.setColour(juce::TextEditor::backgroundColourId,
                     juce::Colours::black.withAlpha(0.2f));
    editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    editor.setColour(juce::TextEditor::highlightColourId,
                     juce::Colours::white.withAlpha(0.2f));
}

juce::String LibraryBrowserComponent::column_title(ColumnType type)
{
    switch (type)
    {
        case ColumnType::Palette: return "Palettes";
        case ColumnType::Knobset: return "Knobsets";
        case ColumnType::Scene: return "Scenes";
        default: break;
    }
    jassertfalse;
    return "Library";
}

LibraryBrowserWindow::LibraryBrowserWindow(LayerCakeLibraryManager& manager,
                                           std::function<LayerCakePresetData()> capture_pattern_fn,
                                           std::function<LayerBufferArray()> capture_layers_fn,
                                           std::function<void(const LayerCakePresetData&)> apply_pattern_fn,
                                           std::function<void(const LayerBufferArray&)> apply_layers_fn,
                                           std::function<LayerCakePresetData()> capture_knobset_fn,
                                           std::function<void(const LayerCakePresetData&)> apply_knobset_fn,
                                           std::function<void()> on_close)
    : juce::DocumentWindow("LayerCake Library",
                           juce::Colours::black,
                           DocumentWindow::closeButton),
      m_on_close(std::move(on_close))
{
    setUsingNativeTitleBar(true);
    auto* content = new LibraryBrowserComponent(manager,
                                                std::move(capture_pattern_fn),
                                                std::move(capture_layers_fn),
                                                std::move(apply_pattern_fn),
                                                std::move(apply_layers_fn),
                                                std::move(capture_knobset_fn),
                                                std::move(apply_knobset_fn));
    setContentOwned(content, true);
    centreWithSize(720, 420); // Slightly narrower without Pattern column
    setResizable(true, true);
    setVisible(true);
}

void LibraryBrowserWindow::closeButtonPressed()
{
    setVisible(false);
    if (m_on_close)
        m_on_close();
}
