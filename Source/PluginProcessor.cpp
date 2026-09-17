#include "PluginProcessor.h"
#include "PluginEditor.h"

PopVocalAudioProcessor::PopVocalAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    intensityParam = apvts.getRawParameterValue ("intensity");
    outputParam    = apvts.getRawParameterValue ("output");
    mixParam       = apvts.getRawParameterValue ("mix");
}

juce::AudioProcessorValueTreeState::ParameterLayout PopVocalAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "intensity", "Intensity",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "output", "Output",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "mix", "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    return { params.begin(), params.end() };
}

void PopVocalAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    pop.prepare (sampleRate);
}

void PopVocalAudioProcessor::releaseResources()
{
    pop.reset();
}

bool PopVocalAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Vocal strip : entrée/sortie stéréo uniquement.
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void PopVocalAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    pop.setParameters (intensityParam->load(), outputParam->load(), mixParam->load());

    pop.processStereo (buffer.getWritePointer (0),
                        buffer.getWritePointer (1),
                        buffer.getNumSamples());
}

juce::AudioProcessorEditor* PopVocalAudioProcessor::createEditor()
{
    return new PopVocalAudioProcessorEditor (*this);
}

void PopVocalAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
        copyXmlToBinary (*state, destData);
}

void PopVocalAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// Point d'entrée requis par JUCE pour instancier le plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PopVocalAudioProcessor();
}
