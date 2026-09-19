#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ICompressorAlgorithm.h"
#include "PopCompressor.h"
#include "ButterCompCompressor.h"
#include "VariMuCompressor.h"
#include "ResonanceSuppressor.h"
#include "ParametricEQ.h"
#include "PultecEQ.h"
#include "DelayModule.h"
#include "ReverbModule.h"
#include "OutputLimiter.h"
#include <array>
#include <vector>

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

    // Accès lecture seule aux niveaux entrée/sortie pour le mètre de l'éditeur
    const std::atomic<float>& getInputLevelDb() const noexcept { return inputLevelDb; }
    const std::atomic<float>& getOutputLevelDb() const noexcept { return outputLevelDb; }

private:
    // Chaîne, dans l'ordre :
    // de-res large -> compresseur (sélectionnable) -> de-res précise ->
    // EQ -> delay (sync tempo + ping-pong) -> reverb
    ResonanceSuppressor resonanceBroad   { 1.8f };
    ResonanceSuppressor resonancePrecise { 6.0f };

    PopCompressor        compAgressif;
    ButterCompCompressor compDoux;
    VariMuCompressor     compNaturel;
    std::array<ICompressorAlgorithm*, 3> compressorAlgorithms { &compAgressif, &compDoux, &compNaturel };

    ParametricEQ eq;
    PultecEQ pultecEq;
    DelayModule delay;
    ReverbModule reverb;
    OutputLimiter outputLimiter;

    std::atomic<float>* resBroadSensitivityParam = nullptr;
    std::atomic<float>* resBroadDepthParam       = nullptr;
    std::atomic<float>* resBroadMixParam         = nullptr;

    std::atomic<float>* compModeParam = nullptr;
    std::atomic<float>* compP1Param   = nullptr;
    std::atomic<float>* compP2Param   = nullptr;
    std::atomic<float>* compP3Param   = nullptr;
    std::atomic<float>* compGainReductionLimitParam = nullptr;

    std::atomic<float>* resPreciseSensitivityParam = nullptr;
    std::atomic<float>* resPreciseDepthParam       = nullptr;
    std::atomic<float>* resPreciseMixParam         = nullptr;

    std::atomic<float>* eqLowCutParam      = nullptr;
    std::atomic<float>* eqLowParam         = nullptr;
    std::atomic<float>* eqLowFreqParam     = nullptr;
    std::atomic<float>* eqLowMidParam      = nullptr;
    std::atomic<float>* eqLowMidFreqParam  = nullptr;
    std::atomic<float>* eqHighMidParam     = nullptr;
    std::atomic<float>* eqHighMidFreqParam = nullptr;
    std::atomic<float>* eqHighParam        = nullptr;
    std::atomic<float>* eqHighFreqParam    = nullptr;
    std::atomic<float>* eqHighCutParam     = nullptr;
    std::atomic<float>* eqAirAmountParam   = nullptr;

    std::atomic<float>* eqModeParam = nullptr; // 0 = Normal, 1 = Pultec
    std::atomic<float>* pultecLowFreqParam        = nullptr;
    std::atomic<float>* pultecLowBoostParam       = nullptr;
    std::atomic<float>* pultecLowAttenParam       = nullptr;
    std::atomic<float>* pultecHighBoostFreqParam  = nullptr;
    std::atomic<float>* pultecHighBoostParam      = nullptr;
    std::atomic<float>* pultecHighBandwidthParam  = nullptr;
    std::atomic<float>* pultecHighAttenFreqParam  = nullptr;
    std::atomic<float>* pultecHighAttenParam      = nullptr;

    std::atomic<float>* delayTimeParam     = nullptr; // utilisé seulement en mode "Free"
    std::atomic<float>* delayRateModeParam = nullptr; // 0=Free,1=1/2,2=1/4,3=1/8,4=1/16
    std::atomic<float>* delayFeedbackParam = nullptr;
    std::atomic<float>* delayMixParam      = nullptr;
    std::atomic<float>* delayPingPongParam = nullptr;
    std::atomic<float>* delayDuckAmountParam = nullptr;

    std::atomic<float>* reverbMixParam     = nullptr;
    std::atomic<float>* reverbSizeParam    = nullptr;
    std::atomic<float>* reverbDampingParam = nullptr;
    std::atomic<float>* reverbDuckAmountParam = nullptr;

    std::atomic<float>* inputGainParam  = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* outputCeilingParam = nullptr;
    std::atomic<float> inputLevelDb  { -60.0f };
    std::atomic<float> outputLevelDb { -60.0f };
    double currentSampleRate = 44100.0;

    // Gain Reduction Limit — plafonne la réduction max du compresseur actif,
    // indépendamment de l'algorithme (comparaison directe sortie vs entrée sèche)
    std::vector<float> dryLeftScratch, dryRightScratch;

    // Ducking Delay/Reverb — enveloppe suivant le signal juste avant ces étages
    float duckEnvelope = 0.0f;

    // Bypass par section — "actif" = traite le signal, sinon le laisse passer inchangé
    std::atomic<float>* resBroadActiveParam   = nullptr;
    std::atomic<float>* compressorActiveParam = nullptr;
    std::atomic<float>* resPreciseActiveParam = nullptr;
    std::atomic<float>* eqActiveParam         = nullptr;
    std::atomic<float>* delayActiveParam      = nullptr;
    std::atomic<float>* reverbActiveParam     = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PopVocalAudioProcessor)
};
