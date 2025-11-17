#include "LevelControl.h"

using namespace Shared;

LevelControl::LevelControl(MultiTrackLooperEngine& engine, int index)
    : LevelControl(engine, index, nullptr, "")
{
}

LevelControl::LevelControl(MultiTrackLooperEngine& engine, int index, MidiLearnManager* midiManager, const juce::String& trackPrefix)
    : engineType(Basic),
      trackIndex(index),
      looperEngine{},
      levelSlider(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow),
      levelLabel("Level", "level"),
      midiLearnManager(midiManager),
      trackIdPrefix(trackPrefix)
{
    looperEngine.basicEngine = &engine;
    
    // Setup level slider (dB)
    levelSlider.setRange(-60.0, 12.0, 0.1);
    levelSlider.setValue(0.0);
    levelSlider.setTextValueSuffix(" dB");
    levelSlider.onValueChange = [this]()
    {
        if (onLevelChange)
            onLevelChange(levelSlider.getValue());
    };
    
    addAndMakeVisible(levelSlider);
    addAndMakeVisible(levelLabel);
    
    // Setup MIDI learn
    if (midiLearnManager)
    {
        levelLearnable = std::make_unique<MidiLearnable>(*midiLearnManager, trackIdPrefix + "_level");
        
        // Create mouse listener for right-click handling
        levelMouseListener = std::make_unique<MidiLearnMouseListener>(*levelLearnable, this);
        levelSlider.addMouseListener(levelMouseListener.get(), false);
        
        // Register parameter - convert normalized 0-1 to dB range
        midiLearnManager->registerParameter({
            trackIdPrefix + "_level",
            [this](float normalizedValue) {
                // Map 0.0-1.0 to -60.0 to +12.0 dB
                double dbValue = -60.0 + normalizedValue * 72.0;
                levelSlider.setValue(dbValue, juce::dontSendNotification);
                if (onLevelChange) onLevelChange(dbValue);
            },
            [this]() {
                // Map -60.0 to +12.0 dB back to 0.0-1.0
                double dbValue = levelSlider.getValue();
                return static_cast<float>((dbValue + 60.0) / 72.0);
            },
            trackIdPrefix + " Level",
            false  // Continuous control
        });
    }
}

LevelControl::~LevelControl()
{
    // Remove mouse listener first
    if (levelMouseListener)
        levelSlider.removeMouseListener(levelMouseListener.get());
    
    if (midiLearnManager)
    {
        midiLearnManager->unregisterParameter(trackIdPrefix + "_level");
    }
}

void LevelControl::paint(juce::Graphics& g)
{
    // Calculate VU meter area
    auto bounds = getLocalBounds();
    
    const int levelLabelHeight = 15;
    const int spacingTiny = 2;
    const int levelAreaWidth = 80;
    const int vuMeterWidth = 30;
    const int vuMeterSpacing = 5;
    
    // Skip past level slider
    auto vuMeterArea = bounds;
    vuMeterArea.removeFromLeft(levelAreaWidth);
    vuMeterArea.removeFromLeft(vuMeterSpacing);
    vuMeterArea = vuMeterArea.removeFromLeft(vuMeterWidth);
    vuMeterArea.removeFromTop(levelLabelHeight + spacingTiny);
    
    drawVUMeter(g, vuMeterArea);
    
    // Draw MIDI indicator on slider if mapped
    if (levelLearnable && levelLearnable->hasMidiMapping())
    {
        auto sliderBounds = levelSlider.getBounds();
        g.setColour(juce::Colour(0xffed1683));  // Pink
        g.fillEllipse(sliderBounds.getRight() - 8.0f, sliderBounds.getY() + 2.0f, 6.0f, 6.0f);
    }
}

void LevelControl::resized()
{
    auto bounds = getLocalBounds();
    
    const int levelLabelHeight = 15;
    const int spacingTiny = 2;
    const int levelAreaWidth = 80;
    
    // Level slider on left (vertical)
    auto levelArea = bounds.removeFromLeft(levelAreaWidth);
    levelLabel.setBounds(levelArea.removeFromTop(levelLabelHeight));
    levelArea.removeFromTop(spacingTiny);
    levelSlider.setBounds(levelArea);
}

double LevelControl::getLevelValue() const
{
    return levelSlider.getValue();
}

void LevelControl::setLevelValue(double value, juce::NotificationType notification)
{
    levelSlider.setValue(value, notification);
}

LevelControl::LevelControl(VampNetMultiTrackLooperEngine& engine, int index)
    : LevelControl(engine, index, nullptr, "")
{
}

LevelControl::LevelControl(VampNetMultiTrackLooperEngine& engine, int index, MidiLearnManager* midiManager, const juce::String& trackPrefix)
    : engineType(VampNet),
      trackIndex(index),
      looperEngine{},
      levelSlider(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow),
      levelLabel("Level", "level"),
      midiLearnManager(midiManager),
      trackIdPrefix(trackPrefix)
{
    looperEngine.vampNetEngine = &engine;
    
    // Setup level slider (dB)
    levelSlider.setRange(-60.0, 12.0, 0.1);
    levelSlider.setValue(0.0);
    levelSlider.setTextValueSuffix(" dB");
    levelSlider.onValueChange = [this]()
    {
        if (onLevelChange)
            onLevelChange(levelSlider.getValue());
    };
    
    addAndMakeVisible(levelSlider);
    addAndMakeVisible(levelLabel);
    
    // Setup MIDI learn
    if (midiLearnManager)
    {
        levelLearnable = std::make_unique<MidiLearnable>(*midiLearnManager, trackIdPrefix + "_level");
        
        // Create mouse listener for right-click handling
        levelMouseListener = std::make_unique<MidiLearnMouseListener>(*levelLearnable, this);
        levelSlider.addMouseListener(levelMouseListener.get(), false);
        
        // Register parameter - convert normalized 0-1 to dB range
        midiLearnManager->registerParameter({
            trackIdPrefix + "_level",
            [this](float normalizedValue) {
                // Map 0.0-1.0 to -60.0 to +12.0 dB
                double dbValue = -60.0 + normalizedValue * 72.0;
                levelSlider.setValue(dbValue, juce::dontSendNotification);
                if (onLevelChange) onLevelChange(dbValue);
            },
            [this]() {
                // Map -60.0 to +12.0 dB back to 0.0-1.0
                double dbValue = levelSlider.getValue();
                return static_cast<float>((dbValue + 60.0) / 72.0);
            },
            trackIdPrefix + " Level",
            false  // Continuous control
        });
    }
}

void LevelControl::drawVUMeter(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (area.isEmpty())
        return;
    
    float level;
    if (engineType == Basic)
    {
        auto& track = looperEngine.basicEngine->get_track(trackIndex);
        level = track.m_read_head.m_level_meter.load();
    }
    else // VampNet
    {
        auto& track = looperEngine.vampNetEngine->get_track(trackIndex);
        // Use recordReadHead level meter for VU display
        level = track.m_record_read_head.m_level_meter.load();
    }
    
    // Clamp level to 0.0-1.0
    level = juce::jlimit(0.0f, 1.0f, level);
    
    // Apply a slight skew to make the level more visible
    float skewedLevel = std::exp(std::log(level + 0.001f) / 3.0f);
    
    // Draw vertical VU meter manually
    const int totalBlocks = 7;
    const int numBlocks = juce::roundToInt(totalBlocks * skewedLevel);
    
    auto outerCornerSize = 3.0f;
    auto outerBorderWidth = 2.0f;
    auto spacingFraction = 0.03f;
    
    // Background - dark grey/black
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRoundedRectangle(area.toFloat(), outerCornerSize);
    
    // Border - teal
    g.setColour(juce::Colour(0xff1eb19d));
    g.drawRoundedRectangle(area.toFloat(), outerCornerSize, outerBorderWidth);
    
    auto doubleOuterBorderWidth = 2.0f * outerBorderWidth;
    auto blockHeight = ((float)area.getHeight() - doubleOuterBorderWidth) / static_cast<float>(totalBlocks);
    auto blockWidth = (float)area.getWidth() - doubleOuterBorderWidth;
    
    auto blockRectHeight = (1.0f - 2.0f * spacingFraction) * blockHeight;
    auto blockRectSpacing = spacingFraction * blockHeight;
    auto blockCornerSize = 0.1f * blockHeight;
    
    // Use teal for normal blocks, pink for peak
    const juce::Colour normalColor(0xff1eb19d); // Teal
    const juce::Colour peakColor(0xffed1683); // Pink
    
    // Draw blocks from bottom to top
    for (auto i = 0; i < totalBlocks; ++i)
    {
        int blockIndex = totalBlocks - 1 - i; // Reverse order (bottom to top)
        
        if (blockIndex >= numBlocks)
            g.setColour(normalColor.withAlpha(0.2f));
        else
            g.setColour(blockIndex < totalBlocks - 1 ? normalColor : peakColor);
        
        float y = outerBorderWidth + area.getY() + ((float)i * blockHeight) + blockRectSpacing;
        float x = outerBorderWidth + area.getX();
        
        g.fillRoundedRectangle(x, y, blockWidth, blockRectHeight, blockCornerSize);
    }
}

