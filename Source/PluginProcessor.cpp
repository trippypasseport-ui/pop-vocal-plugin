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

    eqLowCutParam   = apvts.getRawParameterValue ("eqLowCutFreq");
    eqLowParam      = apvts.getRawParameterValue ("eqLow");
    eqLowFreqParam  = apvts.getRawParameterValue ("eqLowFreq");
    eqMidParam      = apvts.getRawParameterValue ("eqMid");
    eqMidFreqParam  = apvts.getRawParameterValue ("eqMidFreq");
    eqHighParam     = apvts.getRawParameterValue ("eqHigh");
    eqHighFreqParam = apvts.getRawParameterValue ("eqHighFreq");
    eqHighCutParam  = apvts.getRawParameterValue ("eqHighCutFreq");
    eqAirAmountParam = apvts.getRawParameterValue ("eqAirAmount");

    delayTimeParam     = apvts.getRawParameterValue ("delayTime");
    delayRateModeParam = apvts.getRawParameterValue ("delayRateMode");
    delayFeedbackParam = apvts.getRawParameterValue ("delayFeedback");
    delayMixParam      = apvts.getRawParameterValue ("delayMix");
    delayPingPongParam = apvts.getRawParameterValue ("delayPingPong");

    reverbMixParam     = apvts.getRawParameterValue ("reverbMix");
    reverbSizeParam    = apvts.getRawParameterValue ("reverbSize");
    reverbDampingParam = apvts.getRawParameterValue ("reverbDamping");

    inputGainParam  = apvts.getRawParameterValue ("inputGain");
    outputGainParam = apvts.getRawParameterValue ("outputGain");

    resBroadActiveParam   = apvts.getRawParameterValue ("resBroadActive");
    compressorActiveParam = apvts.getRawParameterValue ("compressorActive");
    resPreciseActiveParam = apvts.getRawParameterValue ("resPreciseActive");
    eqActiveParam         = apvts.getRawParameterValue ("eqActive");
    delayActiveParam      = apvts.getRawParameterValue ("delayActive");
    reverbActiveParam     = apvts.getRawParameterValue ("reverbActive");
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

    // Freq avec skew pour un ressenti "log" au toucher du knob (plus de résolution dans les graves)
    auto addFreq = [&params] (const juce::String& id, const juce::String& name,
                               float minHz, float maxHz, float defaultHz)
    {
        juce::NormalisableRange<float> range (minHz, maxHz);
        range.setSkewForCentre (std::sqrt (minHz * maxHz)); // centre géométrique
        params.push_back (std::make_unique<juce::AudioParameterFloat> (id, name, range, defaultHz));
    };

    // De-Resonance large (1er passage, lisse, avant le compresseur)
    addFloat ("resBroadSensitivity", "Broad Sensitivity", 0.0f, 1.0f, 0.35f);
    addFloat ("resBroadDepth",       "Broad Depth",       0.0f, 1.0f, 0.4f);
    addFloat ("resBroadMix",         "Broad Mix",         0.0f, 1.0f, 1.0f);

    // Compresseur sélectionnable — 3 algorithmes réels
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

    // EQ — coupe-bas, gain ET fréquence par bande, coupe-haut
    addFreq  ("eqLowCutFreq", "Low Cut", 20.0f, 500.0f, 80.0f);
    addFloat ("eqLow",  "Low Gain",  -12.0f, 12.0f, 0.0f);
    addFreq  ("eqLowFreq",  "Low Freq",  40.0f,  400.0f,  120.0f);
    addFloat ("eqMid",  "Mid Gain",  -12.0f, 12.0f, 0.0f);
    addFreq  ("eqMidFreq",  "Mid Freq",  200.0f, 5000.0f, 1000.0f);
    addFloat ("eqHigh", "High Gain", -12.0f, 12.0f, 0.0f);
    addFreq  ("eqHighFreq", "High Freq", 2000.0f, 16000.0f, 8000.0f);
    addFreq  ("eqHighCutFreq", "High Cut", 2000.0f, 20000.0f, 18000.0f);
    addFloat ("eqAirAmount", "Air", 0.0f, 6.0f, 0.0f);

    // Delay — sync tempo + ping-pong
    addFloat ("delayTime", "Time (Free)", 0.0f, 1.0f, 0.3f);
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "delayRateMode", "Delay Rate", juce::StringArray { "Free", "1/2", "1/4", "1/8", "1/16" }, 0));
    addFloat ("delayFeedback", "Feedback",  0.0f, 1.0f, 0.3f);
    addFloat ("delayMix",      "Delay Mix", 0.0f, 1.0f, 0.0f);
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "delayPingPong", "Ping-Pong", false));

    // Reverb
    addFloat ("reverbMix",     "Reverb Mix", 0.0f, 1.0f, 0.0f);
    addFloat ("reverbSize",    "Size",       0.0f, 1.0f, 0.5f);
    addFloat ("reverbDamping", "Damping",    0.0f, 1.0f, 0.5f);

    // Entrée / Sortie — gain de tranche, mesuré par les mètres de niveau
    addFloat ("inputGain",  "Input Gain",  -24.0f, 24.0f, 0.0f);
    addFloat ("outputGain", "Output Gain", -24.0f, 24.0f, 0.0f);

    // Bypass par section (tous actifs par défaut)
    auto addBool = [&params] (const juce::String& id, const juce::String& name)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (id, name, true));
    };
    addBool ("resBroadActive",   "De-Res Broad Active");
    addBool ("compressorActive", "Compressor Active");
    addBool ("resPreciseActive", "De-Res Precise Active");
    addBool ("eqActive",         "EQ Active");
    addBool ("delayActive",      "Delay Active");
    addBool ("reverbActive",     "Reverb Active");

    return { params.begin(), params.end() };
}

void PopVocalAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

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

    // 0. Gain d'entrée + mètre (avant tout traitement)
    buffer.applyGain (juce::Decibels::decibelsToGain (inputGainParam->load()));
    {
        const float peak = buffer.getMagnitude (0, numSamples);
        const float peakDb = juce::Decibels::gainToDecibels (peak, -60.0f);
        const float current = inputLevelDb.load();
        // Retombée en dB/seconde (pas dB/bloc) pour que la vitesse du mètre
        // ne dépende pas de la taille de buffer réglée côté host.
        const double blockSeconds = (double) numSamples / currentSampleRate;
        const float decay = 20.0f * (float) blockSeconds; // ~20 dB/s
        inputLevelDb.store (peakDb > current ? peakDb : current - decay);
    }

    // 1. De-Resonance large
    if (resBroadActiveParam->load() > 0.5f)
    {
        resonanceBroad.setParameters (resBroadSensitivityParam->load(), resBroadDepthParam->load(), resBroadMixParam->load());
        resonanceBroad.processStereo (left, right, numSamples);
    }

    // 2. Compresseur — algorithme sélectionné
    if (compressorActiveParam->load() > 0.5f)
    {
        const int modeIndex = juce::jlimit (0, (int) compressorAlgorithms.size() - 1, (int) compModeParam->load());
        auto* activeCompressor = compressorAlgorithms[(size_t) modeIndex];
        activeCompressor->setParameters (compP1Param->load(), compP2Param->load(), compP3Param->load());
        activeCompressor->processStereo (left, right, numSamples);
    }

    // 3. De-Resonance précise
    if (resPreciseActiveParam->load() > 0.5f)
    {
        resonancePrecise.setParameters (resPreciseSensitivityParam->load(), resPreciseDepthParam->load(), resPreciseMixParam->load());
        resonancePrecise.processStereo (left, right, numSamples);
    }

    // 4. EQ
    if (eqActiveParam->load() > 0.5f)
    {
        eq.setParameters (eqLowCutParam->load(),
                           eqLowParam->load(), eqLowFreqParam->load(),
                           eqMidParam->load(), eqMidFreqParam->load(),
                           eqHighParam->load(), eqHighFreqParam->load(),
                           eqHighCutParam->load(),
                           eqAirAmountParam->load());
        eq.processStereo (left, right, numSamples);
    }

    // 5. Delay — temps résolu depuis le tempo hôte si un mode synchronisé est choisi
    if (delayActiveParam->load() > 0.5f)
    {
        double bpm = 120.0;
        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                if (auto tempo = position->getBpm())
                    bpm = *tempo;
            }
        }

        const int rateMode = (int) delayRateModeParam->load(); // 0=Free,1=1/2,2=1/4,3=1/8,4=1/16
        double delayMs;
        if (rateMode == 0)
        {
            delayMs = 20.0 + (double) delayTimeParam->load() * 980.0;
        }
        else
        {
            const double quarterNoteMs = 60000.0 / bpm;
            double multiplier;
            switch (rateMode)
            {
                case 1: multiplier = 2.0;  break; // 1/2
                case 2: multiplier = 1.0;  break; // 1/4
                case 3: multiplier = 0.5;  break; // 1/8
                case 4: multiplier = 0.25; break; // 1/16
                default: multiplier = 1.0; break;
            }
            delayMs = quarterNoteMs * multiplier;
        }

        const bool pingPong = delayPingPongParam->load() > 0.5f;
        delay.setParameters (delayMs, delayFeedbackParam->load(), delayMixParam->load(), pingPong);
        delay.processStereo (left, right, numSamples);
    }

    // 6. Reverb
    if (reverbActiveParam->load() > 0.5f)
    {
        reverb.setParameters (reverbMixParam->load(), reverbSizeParam->load(), reverbDampingParam->load());
        reverb.processStereo (left, right, numSamples);
    }

    // 7. Gain de sortie + mètre (après tout traitement)
    buffer.applyGain (juce::Decibels::decibelsToGain (outputGainParam->load()));
    {
        const float peak = buffer.getMagnitude (0, numSamples);
        const float peakDb = juce::Decibels::gainToDecibels (peak, -60.0f);
        const float current = outputLevelDb.load();
        const double blockSeconds = (double) numSamples / currentSampleRate;
        const float decay = 20.0f * (float) blockSeconds; // ~20 dB/s
        outputLevelDb.store (peakDb > current ? peakDb : current - decay);
    }
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
