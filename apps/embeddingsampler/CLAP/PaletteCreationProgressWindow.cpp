#include "PaletteCreationProgressWindow.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace Unsound4All
{
    PaletteCreationProgressWindow* PaletteCreationProgressWindow::s_instance = nullptr;
    
    PaletteCreationProgressWindow::PaletteCreationProgressWindow()
        : m_statusLabel("Status", "Initializing..."),
          m_fileLabel("File", ""),
          m_progressLabel("Progress", "0%"),
          m_progressBar(m_progress),
          m_cancelButton("Cancel")
    {
        s_instance = this;
        
        m_statusLabel.setJustificationType(juce::Justification::centredLeft);
        auto statusFont = juce::Font(juce::FontOptions().withHeight(16.0f));
        statusFont.setBold(true);
        m_statusLabel.setFont(statusFont);
        addAndMakeVisible(m_statusLabel);
        
        m_fileLabel.setJustificationType(juce::Justification::centredLeft);
        m_fileLabel.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
        addAndMakeVisible(m_fileLabel);
        
        m_progressLabel.setJustificationType(juce::Justification::centredRight);
        m_progressLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        addAndMakeVisible(m_progressLabel);
        
        m_progressBar.setPercentageDisplay(false);
        addAndMakeVisible(m_progressBar);
        
        m_cancelButton.onClick = [this]()
        {
            m_cancelled = true;
            if (m_onCancel)
                m_onCancel();
        };
        addAndMakeVisible(m_cancelButton);
        
        setSize(500, 150);
        startTimer(50); // Update UI every 50ms
    }
    
    PaletteCreationProgressWindow::~PaletteCreationProgressWindow()
    {
        stopTimer();
        if (s_instance == this)
            s_instance = nullptr;
    }
    
    void PaletteCreationProgressWindow::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colours::black);
        
        auto bounds = getLocalBounds().reduced(10);
        
        // Draw border
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawRect(bounds, 1);
    }
    
    void PaletteCreationProgressWindow::resized()
    {
        const int margin = 10;
        const int labelHeight = 24;
        const int buttonHeight = 30;
        const int buttonWidth = 80;
        const int progressBarHeight = 20;
        
        auto bounds = getLocalBounds().reduced(margin);
        
        // Status label at top
        m_statusLabel.setBounds(bounds.removeFromTop(labelHeight));
        bounds.removeFromTop(5);
        
        // File label
        m_fileLabel.setBounds(bounds.removeFromTop(labelHeight));
        bounds.removeFromTop(5);
        
        // Progress bar with label
        auto progressArea = bounds.removeFromTop(progressBarHeight);
        m_progressBar.setBounds(progressArea.removeFromLeft(progressArea.getWidth() - 60));
        m_progressLabel.setBounds(progressArea);
        bounds.removeFromTop(10);
        
        // Cancel button at bottom
        m_cancelButton.setBounds(bounds.removeFromBottom(buttonHeight).removeFromRight(buttonWidth));
    }
    
    void PaletteCreationProgressWindow::timerCallback()
    {
        // Update labels from member variables (thread-safe reads)
        m_statusLabel.setText(m_status, juce::dontSendNotification);
        
        juce::String fileText;
        if (m_totalFiles > 0)
        {
            fileText = "File " + juce::String(m_currentFile) + " of " + juce::String(m_totalFiles);
            if (m_currentFileName.isNotEmpty())
            {
                fileText += ": " + m_currentFileName;
            }
        }
        else
        {
            fileText = m_currentFileName;
        }
        m_fileLabel.setText(fileText, juce::dontSendNotification);
        
        int progressPercent = static_cast<int>(m_progress * 100.0);
        m_progressLabel.setText(juce::String(progressPercent) + "%", juce::dontSendNotification);
        
        repaint();
    }
    
    void PaletteCreationProgressWindow::setStatus(const juce::String& status)
    {
        m_status = status;
    }
    
    void PaletteCreationProgressWindow::setCurrentFile(int current, int total)
    {
        m_currentFile = current;
        m_totalFiles = total;
        if (total > 0)
        {
            m_progress = static_cast<double>(current) / static_cast<double>(total);
        }
    }
    
    void PaletteCreationProgressWindow::setCurrentFileName(const juce::String& fileName)
    {
        m_currentFileName = fileName;
    }
    
    void PaletteCreationProgressWindow::setProgress(double progress)
    {
        m_progress = juce::jlimit(0.0, 1.0, progress);
    }
    
    void PaletteCreationProgressWindow::closeWindow()
    {
        if (m_dialogWindow != nullptr)
        {
            m_dialogWindow->exitModalState(0);
            m_dialogWindow.reset();
        }
    }
    
    void PaletteCreationProgressWindow::showModal(juce::Component* parent, std::function<void()> onCancel)
    {
        auto progressWindow = std::make_unique<PaletteCreationProgressWindow>();
        progressWindow->m_onCancel = onCancel;
        
        juce::DialogWindow::LaunchOptions options;
        options.content.setNonOwned(progressWindow.get());
        options.dialogTitle = "Creating Sound Palette";
        options.dialogBackgroundColour = juce::Colours::black;
        options.escapeKeyTriggersCloseButton = false;
        options.useNativeTitleBar = false;
        options.resizable = false;
        options.componentToCentreAround = parent;
        
        auto* dialog = options.launchAsync();
        progressWindow.release(); // DialogWindow will own it
        
        // Keep reference for updates
        if (PaletteCreationProgressWindow::s_instance)
        {
            PaletteCreationProgressWindow::s_instance->m_dialogWindow.reset(dialog);
        }
    }
}

