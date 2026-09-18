#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ICompressorAlgorithm.h"
#include "PopCompressor.h"
#include "ButterCompCompressor.h"
#include "VariMuCompressor.h"
#include "ResonanceSuppressor.h"
#include "ParametricEQ.h"
#include "DelayModule.h"
#include "ReverbModule.h"
#include <array>

class PopVocalAudioProcessor : public juce::AudioProcessor
{
public:
    PopVocalAudioProcessor();
    ~PopVocalAudioProcessor() override = default;

    // --- Cycle de vie ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // --- Traitement audio ---
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // --- Paramètres ---
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    // --- API AudioProcessor standard ---
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    // Chaîne, dans l'ordre :
    // de-res large (lisse) -> compresseur (sélectionnable) ->
    // de-res précise (chirurgicale) -> EQ -> delay -> reverb
    ResonanceSuppressor resonanceBroad   { 1.8f };  // Q bas = bandes larges
    ResonanceSuppressor resonancePrecise { 6.0f };  // Q haut = bandes étroites

    PopCompressor        compAgressif;
    ButterCompCompressor compDoux;
    VariMuCompressor     compNaturel;
    std::array<ICompressorAlgorithm*, 3> compressorAlgorithms { &compAgressif, &compDoux, &compNaturel };

    ParametricEQ eq;
    DelayModule delay;
    ReverbModule reverb;

    std::atomic<float>* resBroadSensitivityParam = nullptr;
    std::atomic<float>* resBroadDepthParam       = nullptr;
    std::atomic<float>* resBroadMixParam         = nullptr;

    std::atomic<float>* compModeParam = nullptr;
    std::atomic<float>* compP1Param   = nullptr;
    std::atomic<float>* compP2Param   = nullptr;
    std::atomic<float>* compP3Param   = nullptr;

    std::atomic<float>* resPreciseSensitivityParam = nullptr;
    std::atomic<float>* resPreciseDepthParam       = nullptr;
    std::atomic<float>* resPreciseMixParam         = nullptr;

    std::atomic<float>* eqLowParam  = nullptr;
    std::atomic<float>* eqMidParam  = nullptr;
    std::atomic<float>* eqHighParam = nullptr;

    std::atomic<float>* delayTimeParam     = nullptr;
    std::atomic<float>* delayFeedbackParam = nullptr;
    std::atomic<float>* delayMixParam      = nullptr;

    std::atomic<float>* reverbMixParam     = nullptr;
    std::atomic<float>* reverbSizeParam    = nullptr;
    std::atomic<float>* reverbDampingParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PopVocalAudioProcessor)
};
