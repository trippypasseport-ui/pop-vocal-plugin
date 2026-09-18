#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PopLookAndFeel.h"
#include "EQCurveComponent.h"
#include "LevelMeter.h"
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
        int comboBoxKnobIndex = -1;                 // -1 = pleine largeur ; sinon rattaché à ce knob précis
        juce::ToggleButton* toggleButton = nullptr; // ping-pong (Delay uniquement)
        juce::ToggleButton* activeToggle = nullptr; // bypass actif/inactif, sur toutes les sections
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
    void applyMasterPreset (int selectedId);

    PopVocalAudioProcessor& processorRef;
    PopLookAndFeel lookAndFeel;

    juce::Label titleLabel, subtitleLabel;

    // Ordre visuel = ordre de la chaîne de traitement
    Section resBroadSection, compressorSection, resPreciseSection, eqSection, delaySection, reverbSection;

    juce::ComboBox compModeBox;
    juce::ComboBox resBroadPresetBox, resPrecisePresetBox, eqPresetBox, reverbPresetBox;
    juce::ComboBox delayRateBox;
    juce::ComboBox styleBox; // preset maitre, regle toute la chaine en un clic
    juce::ToggleButton delayPingPongToggle { "Ping-Pong" };

    // Bypass actif/inactif par section
    juce::ToggleButton resBroadActiveToggle { "" }, compressorActiveToggle { "" }, resPreciseActiveToggle { "" };
    juce::ToggleButton eqActiveToggle { "" }, delayActiveToggle { "" }, reverbActiveToggle { "" };

    EQCurveComponent eqCurve;

    // Entrée / Sortie — mètres + gain de tranche, en bordure gauche/droite
    juce::Label inputTitleLabel, outputTitleLabel;
    juce::Slider inputGainSlider, outputGainSlider;
    juce::Label inputGainLabel, outputGainLabel;
    LevelMeter inputMeter, outputMeter;

    using SliderAttachment      = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment    = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment      = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::unique_ptr<ComboBoxAttachment> compModeAttachment;
    std::unique_ptr<ComboBoxAttachment> delayRateAttachment;
    std::unique_ptr<ButtonAttachment> delayPingPongAttachment;
    std::unique_ptr<ButtonAttachment> resBroadActiveAttachment, compressorActiveAttachment, resPreciseActiveAttachment;
    std::unique_ptr<ButtonAttachment> eqActiveAttachment, delayActiveAttachment, reverbActiveAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PopVocalAudioProcessorEditor)
};
