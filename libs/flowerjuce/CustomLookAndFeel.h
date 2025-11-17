#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Custom LookAndFeel with vibrant colors on black background

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        // Set background color to pitch black
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colours::black);
        
        // Define our color palette - softer, more muted tones
        const juce::Colour redOrange(0xfff04e36);
        const juce::Colour orange(0xfff36e27);
        const juce::Colour yellow(0xfff3d430);
        const juce::Colour teal(0xff1eb19d);
        const juce::Colour pink(0xffed1683);
        
        // Button colors
        setColour(juce::TextButton::buttonColourId, juce::Colours::black);
        setColour(juce::TextButton::textColourOffId, yellow);
        setColour(juce::TextButton::buttonOnColourId, yellow);
        setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        
        // Toggle button colors
        setColour(juce::ToggleButton::textColourId, yellow);
        setColour(juce::ToggleButton::tickColourId, teal);
        setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff333333));
        
        // Slider colors
        setColour(juce::Slider::thumbColourId, pink);
        setColour(juce::Slider::rotarySliderFillColourId, orange);
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff333333));
        setColour(juce::Slider::trackColourId, redOrange);
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::Slider::textBoxTextColourId, yellow);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff333333));
        
        // Label colors
        setColour(juce::Label::textColourId, yellow);
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        
        // ComboBox colors
        setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
        setColour(juce::ComboBox::textColourId, yellow);
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff333333));
        setColour(juce::ComboBox::buttonColourId, teal);
        setColour(juce::ComboBox::arrowColourId, yellow);
        
        // PopupMenu colors
        setColour(juce::PopupMenu::backgroundColourId, juce::Colours::black);
        setColour(juce::PopupMenu::textColourId, yellow);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, teal);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    }
    
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return juce::Font(juce::FontOptions()
                         .withName(juce::Font::getDefaultMonospacedFontName())
                         .withHeight(juce::jmin(16.0f, buttonHeight * 0.6f)));
    }
    
    juce::Font getLabelFont(juce::Label& label) override
    {
        return juce::Font(juce::FontOptions()
                         .withName(juce::Font::getDefaultMonospacedFontName())
                         .withHeight(14.0f));
    }
    
    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(juce::FontOptions()
                         .withName(juce::Font::getDefaultMonospacedFontName())
                         .withHeight(14.0f));
    }
    
    juce::Font getPopupMenuFont() override
    {
        return juce::Font(juce::FontOptions()
                         .withName(juce::Font::getDefaultMonospacedFontName())
                         .withHeight(14.0f));
    }
    
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                             const juce::Colour& backgroundColour,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
        auto baseColour = backgroundColour;
        
        if (shouldDrawButtonAsDown || button.getToggleState())
            baseColour = baseColour.contrasting(0.2f);
        
        if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.3f);
        
        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 6.0f); // More rounded corners for softer look
        
        g.setColour(baseColour.contrasting(0.5f));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }
    
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (slider.isBar())
        {
            g.setColour(slider.findColour(juce::Slider::trackColourId));
            g.fillRect(slider.isHorizontal() ? juce::Rectangle<float>(static_cast<float>(x), (float)y + 0.5f, sliderPos - (float)x, (float)height - 1.0f)
                                             : juce::Rectangle<float>((float)x + 0.5f, sliderPos, (float)width - 1.0f, (float)(y + height) - sliderPos));
        }
        else
        {
            auto isTwoVal = (style == juce::Slider::SliderStyle::TwoValueVertical || style == juce::Slider::SliderStyle::TwoValueHorizontal);
            auto isThreeVal = (style == juce::Slider::SliderStyle::ThreeValueVertical || style == juce::Slider::SliderStyle::ThreeValueHorizontal);
            
            auto trackWidth = juce::jmin(6.0f, slider.isHorizontal() ? (float)height * 0.25f : (float)width * 0.25f);
            
            juce::Point<float> startPoint(slider.isHorizontal() ? (float)x : (float)x + (float)width * 0.5f,
                                         slider.isHorizontal() ? (float)y + (float)height * 0.5f : (float)(height + y));
            
            juce::Point<float> endPoint(slider.isHorizontal() ? (float)(width + x) : startPoint.x,
                                       slider.isHorizontal() ? startPoint.y : (float)y);
            
            juce::Path backgroundTrack;
            backgroundTrack.startNewSubPath(startPoint);
            backgroundTrack.lineTo(endPoint);
            g.setColour(slider.findColour(juce::Slider::backgroundColourId));
            g.strokePath(backgroundTrack, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            juce::Path valueTrack;
            juce::Point<float> minPoint, maxPoint, thumbPoint;
            
            if (isThreeVal || isTwoVal)
            {
                minPoint = { slider.isHorizontal() ? minSliderPos : (float)x + (float)width * 0.5f,
                            slider.isHorizontal() ? (float)y + (float)height * 0.5f : minSliderPos };
                
                if (isThreeVal)
                    thumbPoint = { slider.isHorizontal() ? sliderPos : (float)x + (float)width * 0.5f,
                                 slider.isHorizontal() ? (float)y + (float)height * 0.5f : sliderPos };
                
                maxPoint = { slider.isHorizontal() ? maxSliderPos : (float)x + (float)width * 0.5f,
                            slider.isHorizontal() ? (float)y + (float)height * 0.5f : maxSliderPos };
            }
            else
            {
                auto kx = slider.isHorizontal() ? sliderPos : ((float)x + (float)width * 0.5f);
                auto ky = slider.isHorizontal() ? ((float)y + (float)height * 0.5f) : sliderPos;
                minPoint = startPoint;
                maxPoint = { kx, ky };
            }
            
            auto thumbWidth = getSliderThumbRadius(slider);
            
            valueTrack.startNewSubPath(minPoint);
            valueTrack.lineTo(isThreeVal ? thumbPoint : maxPoint);
            g.setColour(slider.findColour(juce::Slider::trackColourId));
            g.strokePath(valueTrack, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            if (!isTwoVal)
            {
                g.setColour(slider.findColour(juce::Slider::thumbColourId));
                g.fillEllipse(juce::Rectangle<float>(static_cast<float>(thumbWidth * 2.0f), static_cast<float>(thumbWidth * 2.0f)).withCentre(isThreeVal ? thumbPoint : maxPoint));
            }
            
            if (isTwoVal || isThreeVal)
            {
                auto sr = juce::jmin(trackWidth, (slider.isHorizontal() ? (float)height : (float)width) * 0.4f);
                auto pointerColour = slider.findColour(juce::Slider::thumbColourId);
                
                if (slider.isHorizontal())
                {
                    drawPointer(g, minSliderPos - sr,
                               juce::jmax(0.0f, (float)y + (float)height * 0.5f - trackWidth * 2.0f),
                               trackWidth * 2.0f, pointerColour, 2);
                    
                    drawPointer(g, maxSliderPos - sr,
                               juce::jmin((float)(y + height) - trackWidth * 2.0f, (float)y + (float)height * 0.5f),
                               trackWidth * 2.0f, pointerColour, 4);
                }
                else
                {
                    drawPointer(g, juce::jmax(0.0f, (float)x + (float)width * 0.5f - trackWidth * 2.0f),
                               minSliderPos - sr,
                               trackWidth * 2.0f, pointerColour, 1);
                    
                    drawPointer(g, juce::jmin((float)(x + width) - trackWidth * 2.0f, (float)x + (float)width * 0.5f), maxSliderPos - sr,
                               trackWidth * 2.0f, pointerColour, 3);
                }
            }
        }
    }
};

