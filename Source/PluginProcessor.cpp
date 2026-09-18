#include "PluginProcessor.h"
#include "PluginEditor.h"

PopVocalAudioProcessor::PopVocalAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    resBroadSensitivityParam = apvts.getRawParameterValue ("resBroadSensitivity");
    resBroadDepthParam       = apvts.getRawParameterValue ("resBroadDepth");
    resBroadMixParam         = apvts.getRawParameterValue ("resBroadMix");

    compModeParam = apvts.getRawParameterValue ("compMode");
    compP1Param   = apvts.getRawParameterValue ("compP1");
    compP2Param   = apvts.getRawParameterValue ("compP2");
    compP3Param   = apvts.getRawParameterValue ("compP3");

    resPreciseSensitivityParam = apvts.getRawParameterValue ("resPreciseSensitivity");
    resPreciseDepthParam       = apvts.getRawParameterValue ("resPreciseDepth");
    resPreciseMixParam         = apvts.getRawParameterValue ("resPreciseMix");

    eqLowParam  = apvts.getRawParameterValue ("eqLow");
    eqMidParam  = apvts.getRawParameterValue ("eqMid");
    eqHighParam = apvts.getRawParameterValue ("eqHigh");

    delayTimeParam     = apvts.getRawParameterValue ("delayTime");
    delayFeedbackParam = apvts.getRawParameterValue ("delayFeedback");
    delayMixParam      = apvts.getRawParameterValue ("delayMix");

    reverbMixParam     = apvts.getRawParameterValue ("reverbMix");
    reverbSizeParam    = apvts.getRawParameterValue ("reverbSize");
    reverbDampingParam = apvts.getRawParameterValue ("reverbDamping");
}

juce::AudioProcessorValueTreeState::ParameterLayout PopVocalAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addFloat = [&params] (const juce::String& id, const juce::String& name,
                                float minV, float maxV, float defaultV)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            id, name, juce::NormalisableRange<float> (minV, maxV), defaultV));
    };

    // De-Resonance large (1er passage, lisse, avant le compresseur)
    addFloat ("resBroadSensitivity", "Broad Sensitivity", 0.0f, 1.0f, 0.35f);
    addFloat ("resBroadDepth",       "Broad Depth",       0.0f, 1.0f, 0.4f);
    addFloat ("resBroadMix",         "Broad Mix",         0.0f, 1.0f, 1.0f);

    // Compresseur sélectionnable — 3 algorithmes réels, pas 3 presets d'un seul moteur
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "compMode", "Compressor Mode",
        juce::StringArray { "Agressif (Pop)", "Doux (ButterComp)", "Naturel (VariMu)" }, 0));
    addFloat ("compP1", "Comp Param 1", 0.0f, 1.0f, 0.3f);
    addFloat ("compP2", "Comp Param 2", 0.0f, 1.0f, 0.5f);
    addFloat ("compP3", "Comp Param 3", 0.0f, 1.0f, 1.0f);

    // De-Resonance précise (2e passage, après le compresseur)
    addFloat ("resPreciseSensitivity", "Precise Sensitivity", 0.0f, 1.0f, 0.5f);
    addFloat ("resPreciseDepth",       "Precise Depth",       0.0f, 1.0f, 0.3f);
    addFloat ("resPreciseMix",         "Precise Mix",         0.0f, 1.0f, 1.0f);

    // EQ
    addFloat ("eqLow",  "Low",  -12.0f, 12.0f, 0.0f);
    addFloat ("eqMid",  "Mid",  -12.0f, 12.0f, 0.0f);
    addFloat ("eqHigh", "High", -12.0f, 12.0f, 0.0f);

    // Delay
    addFloat ("delayTime",     "Time",      0.0f, 1.0f, 0.3f);
    addFloat ("delayFeedback", "Feedback",  0.0f, 1.0f, 0.3f);
    addFloat ("delayMix",      "Delay Mix", 0.0f, 1.0f, 0.0f);

    // Reverb
    addFloat ("reverbMix",     "Reverb Mix", 0.0f, 1.0f, 0.0f);
    addFloat ("reverbSize",    "Size",       0.0f, 1.0f, 0.5f);
    addFloat ("reverbDamping", "Damping",    0.0f, 1.0f, 0.5f);

    return { params.begin(), params.end() };
}

void PopVocalAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    resonanceBroad.prepare (sampleRate, samplesPerBlock);
    resonancePrecise.prepare (sampleRate, samplesPerBlock);

    for (auto* algo : compressorAlgorithms)
        algo->prepare (sampleRate);

    eq.prepare (sampleRate);
    delay.prepare (sampleRate, samplesPerBlock);
    reverb.prepare (sampleRate, samplesPerBlock);
}

void PopVocalAudioProcessor::releaseResources()
{
    resonanceBroad.reset();
    resonancePrecise.reset();

    for (auto* algo : compressorAlgorithms)
        algo->reset();

    eq.reset();
    delay.reset();
    reverb.reset();
}

bool PopVocalAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void PopVocalAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const int numSamples = buffer.getNumSamples();

    // 1. De-Resonance large — lisse le spectre avant que le compresseur agisse
    resonanceBroad.setParameters (resBroadSensitivityParam->load(), resBroadDepthParam->load(), resBroadMixParam->load());
    resonanceBroad.processStereo (left, right, numSamples);

    // 2. Compresseur — algorithme sélectionné par compMode (Agressif / Doux / Naturel)
    const int modeIndex = juce::jlimit (0, (int) compressorAlgorithms.size() - 1, (int) compModeParam->load());
    auto* activeCompressor = compressorAlgorithms[(size_t) modeIndex];
    activeCompressor->setParameters (compP1Param->load(), compP2Param->load(), compP3Param->load());
    activeCompressor->processStereo (left, right, numSamples);

    // 3. De-Resonance précise — nettoie ce que la compression a pu faire ressortir
    resonancePrecise.setParameters (resPreciseSensitivityParam->load(), resPreciseDepthParam->load(), resPreciseMixParam->load());
    resonancePrecise.processStereo (left, right, numSamples);

    // 4. EQ
    eq.setParameters (eqLowParam->load(), eqMidParam->load(), eqHighParam->load());
    eq.processStereo (left, right, numSamples);

    // 5. Delay — avant la reverb, pour que ses répétitions soient elles aussi prises dans la queue
    delay.setParameters (delayTimeParam->load(), delayFeedbackParam->load(), delayMixParam->load());
    delay.processStereo (left, right, numSamples);

    // 6. Reverb — dernier étage
    reverb.setParameters (reverbMixParam->load(), reverbSizeParam->load(), reverbDampingParam->load());
    reverb.processStereo (left, right, numSamples);
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PopVocalAudioProcessor();
}
