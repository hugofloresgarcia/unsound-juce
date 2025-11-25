#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LayerCakeLibraryManager.h"
#include <functional>

class LibraryBrowserComponent : public juce::Component,
                                private juce::Button::Listener
{
public:
    LibraryBrowserComponent(LayerCakeLibraryManager& manager,
                            std::function<LayerCakePresetData()> capture_pattern_fn,
                            std::function<LayerBufferArray()> capture_layers_fn,
                            std::function<void(const LayerCakePresetData&)> apply_pattern_fn,
                            std::function<void(const LayerBufferArray&)> apply_layers_fn,
                            std::function<LayerCakePresetData()> capture_knobset_fn,
                            std::function<void(const LayerCakePresetData&)> apply_knobset_fn);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    enum class ColumnType
    {
        Palette,
        Knobset,
        Scene
    };

    enum class RowAction
    {
        Save,
        Load,
        Delete
    };

    class LibraryRowComponent;

    class ColumnModel : public juce::ListBoxModel
    {
    public:
        ColumnModel(LibraryBrowserComponent& owner, ColumnType type);

        int getNumRows() override;
        juce::Component* refreshComponentForRow(int rowNumber,
                                                bool isRowSelected,
                                                juce::Component* existingComponent) override;
        void paintListBoxItem(int rowNumber,
                              juce::Graphics& g,
                              int width,
                              int height,
                              bool rowIsSelected) override;

        int getColumnId() const { return static_cast<int>(m_type); }

    private:
        LibraryBrowserComponent& m_owner;
        ColumnType m_type;
        bool m_reported_invalid_row{false};
    };

    struct ColumnWidgets
    {
        juce::Label title;
        juce::TextEditor name_editor;
        juce::TextButton save_button;
        juce::ListBox list_box;
        std::unique_ptr<ColumnModel> model;
    };

    void buttonClicked(juce::Button* button) override;

    void refresh_lists();
    void handle_new_save(ColumnType type);
    void handle_row_action(ColumnType type, const juce::String& name, RowAction action);
    const juce::StringArray& names_for(ColumnType type) const;
    ColumnWidgets& widgets_for(ColumnType type);
    void format_name_editor(juce::TextEditor& editor) const;
    static juce::String column_title(ColumnType type);

    LayerCakeLibraryManager& m_manager;
    ColumnWidgets m_palette_widgets;
    ColumnWidgets m_knobset_widgets;
    ColumnWidgets m_scene_widgets;

    std::function<LayerCakePresetData()> m_capture_pattern_fn;
    std::function<LayerBufferArray()> m_capture_layers_fn;
    std::function<void(const LayerCakePresetData&)> m_apply_pattern_fn;
    std::function<void(const LayerBufferArray&)> m_apply_layers_fn;
    std::function<LayerCakePresetData()> m_capture_knobset_fn;
    std::function<void(const LayerCakePresetData&)> m_apply_knobset_fn;
};

class LibraryBrowserWindow : public juce::DocumentWindow
{
public:
    LibraryBrowserWindow(LayerCakeLibraryManager& manager,
                         std::function<LayerCakePresetData()> capture_pattern_fn,
                         std::function<LayerBufferArray()> capture_layers_fn,
                         std::function<void(const LayerCakePresetData&)> apply_pattern_fn,
                         std::function<void(const LayerBufferArray&)> apply_layers_fn,
                         std::function<LayerCakePresetData()> capture_knobset_fn,
                         std::function<void(const LayerCakePresetData&)> apply_knobset_fn,
                         std::function<void()> on_close);

    void closeButtonPressed() override;

private:
    std::function<void()> m_on_close;
};
