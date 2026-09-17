#include "PluginEditor.h"

namespace
{
    void setupRotarySlider (juce::Slider& slider, juce::Label& label, const juce::String& text,
                             juce::Component& parent)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        parent.addAndMakeVisible (slider);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.attachToComponent (&slider, false);
        parent.addAndMakeVisible (label);
    }
}

PopVocalAudioProcessorEditor::PopVocalAudioProcessorEditor (PopVocalAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setupRotarySlider (intensitySlider, intensityLabel, "Intensity", *this);
    setupRotarySlider (outputSlider,    outputLabel,    "Output",    *this);
    setupRotarySlider (mixSlider,       mixLabel,       "Mix",       *this);

    auto& apvts = processorRef.apvts;
    intensityAttachment = std::make_unique<SliderAttachment> (apvts, "intensity", intensitySlider);
    outputAttachment    = std::make_unique<SliderAttachment> (apvts, "output",    outputSlider);
    mixAttachment        = std::make_unique<SliderAttachment> (apvts, "mix",       mixSlider);

    setSize (320, 160);
}

void PopVocalAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PopVocalAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    const int sliderWidth = area.getWidth() / 3;

    intensitySlider.setBounds (area.removeFromLeft (sliderWidth).reduced (10, 30));
    outputSlider.setBounds    (area.removeFromLeft (sliderWidth).reduced (10, 30));
    mixSlider.setBounds       (area.reduced (10, 30));
}
