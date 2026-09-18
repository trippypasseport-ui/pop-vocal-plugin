#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PopLookAndFeel.h"
#include "EQCurveComponent.h"
#include <array>
#include <vector>

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
        std::vector<std::unique_ptr<Knob>> knobs;
        juce::ComboBox* comboBox = nullptr;         // algo mode / rate mode / preset
        juce::ToggleButton* toggleButton = nullptr; // ping-pong
        juce::Component* extraDisplay = nullptr;    // EQ : courbe
        int extraDisplayHeight = 0;
        int knobColumns = 3;                        // EQ : 3 colonnes x 2 rangées (6 knobs)
    };

    Knob& addKnob (Section& section, const juce::String& name);
    void finishSectionSetup (Section& section, const juce::String& title);
    void layoutSection (juce::Rectangle<int> area, Section& section);
    void drawPanel (juce::Graphics& g, juce::Rectangle<int> area);
    void updateCompressorLabels();
    void applyPreset (const std::vector<std::pair<juce::String, float>>& values);

    PopVocalAudioProcessor& processorRef;
    PopLookAndFeel lookAndFeel;

    juce::Label titleLabel, subtitleLabel;

    // Ordre visuel = ordre de la chaîne de traitement
    Section resBroadSection, compressorSection, resPreciseSection, eqSection, delaySection, reverbSection;

    juce::ComboBox compModeBox;
    juce::ComboBox resBroadPresetBox, resPrecisePresetBox, eqPresetBox, reverbPresetBox;
    juce::ComboBox delayRateBox;
    juce::ToggleButton delayPingPongToggle { "Ping-Pong" };

    EQCurveComponent eqCurve;

    using SliderAttachment      = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment    = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment      = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::unique_ptr<ComboBoxAttachment> compModeAttachment;
    std::unique_ptr<ComboBoxAttachment> delayRateAttachment;
    std::unique_ptr<ButtonAttachment> delayPingPongAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PopVocalAudioProcessorEditor)
};
