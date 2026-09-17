#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PopLookAndFeel.h"

class PopVocalAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PopVocalAudioProcessorEditor (PopVocalAudioProcessor&);
    ~PopVocalAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PopVocalAudioProcessor& processorRef;
    PopLookAndFeel lookAndFeel;

    juce::Label titleLabel, subtitleLabel;

    juce::Slider intensitySlider, outputSlider, mixSlider;
    juce::Label  intensityLabel,  outputLabel,  mixLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> intensityAttachment, outputAttachment, mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PopVocalAudioProcessorEditor)
};
