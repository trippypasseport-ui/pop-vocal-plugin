#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PopLookAndFeel.h"
#include "EQCurveComponent.h"
#include <array>

class PopVocalAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PopVocalAudioProcessorEditor (PopVocalAudioProcessor&);
    ~PopVocalAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
    };

    struct Section
    {
        juce::Label titleLabel;
        std::array<Knob, 3> knobs;
        juce::ComboBox* comboBox = nullptr;        // optionnel (Compressor)
        juce::Component* extraDisplay = nullptr;   // optionnel (EQ : courbe)
        int extraDisplayHeight = 0;
    };

    void setupSection (Section& section, const juce::String& title,
                        std::array<juce::String, 3> knobNames);
    void layoutSection (juce::Rectangle<int> area, Section& section);
    void drawPanel (juce::Graphics& g, juce::Rectangle<int> area);
    void updateCompressorLabels();

    PopVocalAudioProcessor& processorRef;
    PopLookAndFeel lookAndFeel;

    juce::Label titleLabel, subtitleLabel;

    // Ordre visuel = ordre de la chaîne de traitement
    Section resBroadSection, compressorSection, resPreciseSection, eqSection, delaySection, reverbSection;

    juce::ComboBox compModeBox;
    EQCurveComponent eqCurve;

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::unique_ptr<ComboBoxAttachment> compModeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PopVocalAudioProcessorEditor)
};
