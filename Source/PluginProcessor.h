#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PopCompressor.h"

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
    double getTailLengthSeconds() const override { return 0.0; }
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
    PopCompressor pop;
    std::atomic<float>* intensityParam = nullptr;
    std::atomic<float>* outputParam    = nullptr;
    std::atomic<float>* mixParam       = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PopVocalAudioProcessor)
};
