#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace Unsound4All
{
    class PaletteCreationProgressWindow : public juce::Component,
                                          public juce::Timer
    {
    public:
        PaletteCreationProgressWindow();
        ~PaletteCreationProgressWindow() override;
        
        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;
        
        // Update progress information
        void setStatus(const juce::String& status);
        void setCurrentFile(int current, int total);
        void setCurrentFileName(const juce::String& fileName);
        void setProgress(double progress); // 0.0 to 1.0
        
        // Show as modal dialog
        static void showModal(juce::Component* parent, std::function<void()> onCancel);
        
        // Get the window instance (for updating from background thread)
        static PaletteCreationProgressWindow* getInstance() { return s_instance; }
        
        // Close the progress window
        void closeWindow();
        
    private:
        juce::String m_status;
        int m_currentFile{0};
        int m_totalFiles{0};
        juce::String m_currentFileName;
        double m_progress{0.0};
        
        juce::Label m_statusLabel;
        juce::Label m_fileLabel;
        juce::Label m_progressLabel;
        juce::ProgressBar m_progressBar;
        juce::TextButton m_cancelButton;
        
        std::function<void()> m_onCancel;
        bool m_cancelled{false};
        
        static PaletteCreationProgressWindow* s_instance;
        std::unique_ptr<juce::DialogWindow> m_dialogWindow;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PaletteCreationProgressWindow)
    };
}

