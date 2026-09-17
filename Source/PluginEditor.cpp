#include "PluginEditor.h"

namespace
{
    void setupRotarySlider (juce::Slider& slider, juce::Label& label, const juce::String& text,
                             juce::Component& parent)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 70, 18);
        parent.addAndMakeVisible (slider);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (13.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, juce::Colour (0xff8a8a92));
        parent.addAndMakeVisible (label);
    }
}

PopVocalAudioProcessorEditor::PopVocalAudioProcessorEditor (PopVocalAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("POP VOCAL", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe4e2df));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("VOCAL COMPRESSOR", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (10.5f, juce::Font::plain));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8862b));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    setupRotarySlider (intensitySlider, intensityLabel, "INTENSITY", *this);
    setupRotarySlider (outputSlider,    outputLabel,    "OUTPUT",    *this);
    setupRotarySlider (mixSlider,       mixLabel,       "MIX",       *this);

    auto& apvts = processorRef.apvts;
    intensityAttachment = std::make_unique<SliderAttachment> (apvts, "intensity", intensitySlider);
    outputAttachment    = std::make_unique<SliderAttachment> (apvts, "output",    outputSlider);
    mixAttachment        = std::make_unique<SliderAttachment> (apvts, "mix",       mixSlider);

    setSize (420, 260);
}

PopVocalAudioProcessorEditor::~PopVocalAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void PopVocalAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Fond en dégradé vertical (charbon -> presque noir)
    juce::ColourGradient bg (juce::Colour (0xff1c1c22), bounds.getX(), bounds.getY(),
                              juce::Colour (0xff121216), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Ligne d'accent sous le titre
    g.setColour (juce::Colour (0xffe8862b).withAlpha (0.6f));
    g.fillRect (24.0f, 66.0f, (float) getWidth() - 48.0f, 1.5f);

    // Panneau des knobs
    auto panel = bounds.reduced (24.0f, 0.0f).withTop (90.0f).withBottom ((float) getHeight() - 24.0f);
    g.setColour (juce::Colour (0xff232328));
    g.fillRoundedRectangle (panel, 10.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (panel, 10.0f, 1.0f);
}

void PopVocalAudioProcessorEditor::resized()
{
    titleLabel.setBounds (24, 16, 250, 30);
    subtitleLabel.setBounds (24, 44, 250, 16);

    auto area = getLocalBounds().reduced (24).withTop (110);
    const int sliderWidth = area.getWidth() / 3;

    auto layoutKnob = [] (juce::Rectangle<int> cell, juce::Slider& slider, juce::Label& label)
    {
        label.setBounds (cell.removeFromTop (18));
        slider.setBounds (cell.reduced (14, 4));
    };

    layoutKnob (area.removeFromLeft (sliderWidth), intensitySlider, intensityLabel);
    layoutKnob (area.removeFromLeft (sliderWidth), outputSlider,    outputLabel);
    layoutKnob (area,                              mixSlider,       mixLabel);
}
