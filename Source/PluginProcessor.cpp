#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

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
    compGainReductionLimitParam = apvts.getRawParameterValue ("compGainReductionLimit");

    resPreciseSensitivityParam = apvts.getRawParameterValue ("resPreciseSensitivity");
    resPreciseDepthParam       = apvts.getRawParameterValue ("resPreciseDepth");
    resPreciseMixParam         = apvts.getRawParameterValue ("resPreciseMix");

    eqLowCutParam      = apvts.getRawParameterValue ("eqLowCutFreq");
    eqLowParam         = apvts.getRawParameterValue ("eqLow");
    eqLowFreqParam     = apvts.getRawParameterValue ("eqLowFreq");
    eqLowMidParam      = apvts.getRawParameterValue ("eqLowMid");
    eqLowMidFreqParam  = apvts.getRawParameterValue ("eqLowMidFreq");
    eqHighMidParam     = apvts.getRawParameterValue ("eqHighMid");
    eqHighMidFreqParam = apvts.getRawParameterValue ("eqHighMidFreq");
    eqHighParam        = apvts.getRawParameterValue ("eqHigh");
    eqHighFreqParam    = apvts.getRawParameterValue ("eqHighFreq");
    eqHighCutParam     = apvts.getRawParameterValue ("eqHighCutFreq");
    eqAirAmountParam   = apvts.getRawParameterValue ("eqAirAmount");

    eqModeParam = apvts.getRawParameterValue ("eqMode");
    pultecLowFreqParam       = apvts.getRawParameterValue ("pultecLowFreq");
    pultecLowBoostParam      = apvts.getRawParameterValue ("pultecLowBoost");
    pultecLowAttenParam      = apvts.getRawParameterValue ("pultecLowAtten");
    pultecHighBoostFreqParam = apvts.getRawParameterValue ("pultecHighBoostFreq");
    pultecHighBoostParam     = apvts.getRawParameterValue ("pultecHighBoost");
    pultecHighBandwidthParam = apvts.getRawParameterValue ("pultecHighBandwidth");
    pultecHighAttenFreqParam = apvts.getRawParameterValue ("pultecHighAttenFreq");
    pultecHighAttenParam     = apvts.getRawParameterValue ("pultecHighAtten");

    delayTimeParam     = apvts.getRawParameterValue ("delayTime");
    delayRateModeParam = apvts.getRawParameterValue ("delayRateMode");
    delayFeedbackParam = apvts.getRawParameterValue ("delayFeedback");
    delayMixParam      = apvts.getRawParameterValue ("delayMix");
    delayPingPongParam = apvts.getRawParameterValue ("delayPingPong");
    delayDuckAmountParam = apvts.getRawParameterValue ("delayDuckAmount");

    reverbMixParam     = apvts.getRawParameterValue ("reverbMix");
    reverbSizeParam    = apvts.getRawParameterValue ("reverbSize");
    reverbDampingParam = apvts.getRawParameterValue ("reverbDamping");
    reverbDuckAmountParam = apvts.getRawParameterValue ("reverbDuckAmount");

    inputGainParam  = apvts.getRawParameterValue ("inputGain");
    outputGainParam = apvts.getRawParameterValue ("outputGain");
    outputCeilingParam = apvts.getRawParameterValue ("outputCeiling");

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
    // Gain Reduction Limit — plafond de réduction max, indépendant de l'algorithme.
    // 24dB = pas de plafond en pratique (les 3 algos ne descendent quasiment jamais
    // en dessous) ; on baisse la valeur pour vraiment brider la compression.
    addFloat ("compGainReductionLimit", "GR Limit", 3.0f, 24.0f, 24.0f);

    // De-Resonance précise (2e passage, après le compresseur)
    addFloat ("resPreciseSensitivity", "Precise Sensitivity", 0.0f, 1.0f, 0.5f);
    addFloat ("resPreciseDepth",       "Precise Depth",       0.0f, 1.0f, 0.3f);
    addFloat ("resPreciseMix",         "Precise Mix",         0.0f, 1.0f, 1.0f);

    // EQ — coupe-bas, gain ET fréquence sur chaque bande, coupe-haut
    addFreq  ("eqLowCutFreq", "Low Cut", 20.0f, 500.0f, 80.0f);
    addFloat ("eqLow",  "Low Gain",  -12.0f, 12.0f, 0.0f);
    addFreq  ("eqLowFreq",  "Low Freq",  40.0f,  400.0f,  120.0f);
    addFloat ("eqLowMid", "Low-Mid Gain", -12.0f, 12.0f, 0.0f);
    addFreq  ("eqLowMidFreq", "Low-Mid Freq", 150.0f, 800.0f, 300.0f);
    addFloat ("eqHighMid", "High-Mid Gain", -12.0f, 12.0f, 0.0f);
    addFreq  ("eqHighMidFreq", "High-Mid Freq", 1500.0f, 6000.0f, 3000.0f);
    addFloat ("eqHigh", "High Gain", -12.0f, 12.0f, 0.0f);
    addFreq  ("eqHighFreq", "High Freq", 2000.0f, 16000.0f, 8000.0f);
    addFreq  ("eqHighCutFreq", "High Cut", 2000.0f, 20000.0f, 18000.0f);
    addFloat ("eqAirAmount", "Air", 0.0f, 6.0f, 0.0f);

    // Mode EQ : Normal (bandes paramétriques ci-dessus) ou Pultec
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "eqMode", "EQ Mode", juce::StringArray { "Normal", "Pultec" }, 0));

    // Pultec — fréquences par crans (index), comme l'original
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "pultecLowFreq", "Pultec Low Freq", juce::StringArray { "20Hz", "30Hz", "60Hz", "100Hz" }, 2));
    addFloat ("pultecLowBoost", "Pultec Low Boost", 0.0f, 10.0f, 0.0f);
    addFloat ("pultecLowAtten", "Pultec Low Atten", 0.0f, 10.0f, 0.0f);
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "pultecHighBoostFreq", "Pultec High Boost Freq",
        juce::StringArray { "3kHz", "4kHz", "5kHz", "8kHz", "10kHz", "12kHz", "16kHz" }, 3));
    addFloat ("pultecHighBoost", "Pultec High Boost", 0.0f, 10.0f, 0.0f);
    addFloat ("pultecHighBandwidth", "Pultec High Bandwidth", 0.0f, 1.0f, 0.5f);
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "pultecHighAttenFreq", "Pultec High Atten Freq", juce::StringArray { "5kHz", "10kHz", "20kHz" }, 1));
    addFloat ("pultecHighAtten", "Pultec High Atten", 0.0f, 10.0f, 0.0f);

    // Delay — sync tempo + ping-pong
    addFloat ("delayTime", "Time (Free)", 0.0f, 1.0f, 0.3f);
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "delayRateMode", "Delay Rate", juce::StringArray { "Free", "1/2", "1/4", "1/8", "1/16" }, 0));
    addFloat ("delayFeedback", "Feedback",  0.0f, 1.0f, 0.3f);
    addFloat ("delayMix",      "Delay Mix", 0.0f, 1.0f, 0.0f);
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "delayPingPong", "Ping-Pong", false));
    addFloat ("delayDuckAmount", "Delay Duck", 0.0f, 1.0f, 0.5f);

    // Reverb
    addFloat ("reverbMix",     "Reverb Mix", 0.0f, 1.0f, 0.0f);
    addFloat ("reverbSize",    "Size",       0.0f, 1.0f, 0.5f);
    addFloat ("reverbDamping", "Damping",    0.0f, 1.0f, 0.5f);
    addFloat ("reverbDuckAmount", "Reverb Duck", 0.0f, 1.0f, 0.5f);

    // Entrée / Sortie — gain de tranche, mesuré par les mètres de niveau
    addFloat ("inputGain",  "Input Gain",  -24.0f, 24.0f, 0.0f);
    addFloat ("outputGain", "Output Gain", -24.0f, 24.0f, 0.0f);
    addFloat ("outputCeiling", "Ceiling", -12.0f, 0.0f, -0.3f);

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
    pultecEq.prepare (sampleRate);
    delay.prepare (sampleRate, samplesPerBlock);
    reverb.prepare (sampleRate, samplesPerBlock);
    outputLimiter.prepare (sampleRate);
}

void PopVocalAudioProcessor::releaseResources()
{
    resonanceBroad.reset();
    resonancePrecise.reset();

    for (auto* algo : compressorAlgorithms)
        algo->reset();

    eq.reset();
    pultecEq.reset();
    delay.reset();
    reverb.reset();
    outputLimiter.reset();
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
        // Snapshot du sec, pour mesurer la vraie réduction de gain après coup
        // (Gain Reduction Limit — algorithme-agnostique, marche pareil sur les 3 modes)
        if ((int) dryLeftScratch.size() < numSamples)
        {
            dryLeftScratch.resize ((size_t) numSamples);
            dryRightScratch.resize ((size_t) numSamples);
        }
        std::copy (left, left + numSamples, dryLeftScratch.begin());
        std::copy (right, right + numSamples, dryRightScratch.begin());

        const int modeIndex = juce::jlimit (0, (int) compressorAlgorithms.size() - 1, (int) compModeParam->load());
        auto* activeCompressor = compressorAlgorithms[(size_t) modeIndex];
        activeCompressor->setParameters (compP1Param->load(), compP2Param->load(), compP3Param->load());
        activeCompressor->processStereo (left, right, numSamples);

        const float maxReductionDb = compGainReductionLimitParam->load();
        if (maxReductionDb < 23.9f) // ~24dB = plafond desactive en pratique
        {
            const float minGainLinear = juce::Decibels::decibelsToGain (-maxReductionDb);
            for (int i = 0; i < numSamples; ++i)
            {
                auto limitOne = [minGainLinear] (float& out, float dry)
                {
                    const float dryAbs = std::fabs (dry);
                    if (dryAbs < 1.0e-8f) return;
                    const float outAbs = std::fabs (out);
                    if (outAbs < dryAbs * minGainLinear)
                        out = (out >= 0.0f ? 1.0f : -1.0f) * dryAbs * minGainLinear;
                };
                limitOne (left[i],  dryLeftScratch[(size_t) i]);
                limitOne (right[i], dryRightScratch[(size_t) i]);
            }
        }
    }

    // 3. De-Resonance précise
    if (resPreciseActiveParam->load() > 0.5f)
    {
        resonancePrecise.setParameters (resPreciseSensitivityParam->load(), resPreciseDepthParam->load(), resPreciseMixParam->load());
        resonancePrecise.processStereo (left, right, numSamples);
    }

    // 4. EQ — mode Normal ou Pultec, selon eqMode
    if (eqActiveParam->load() > 0.5f)
    {
        const bool pultecMode = (int) eqModeParam->load() >= 1;
        if (pultecMode)
        {
            pultecEq.setParameters ((int) pultecLowFreqParam->load(),
                                     pultecLowBoostParam->load(), pultecLowAttenParam->load(),
                                     (int) pultecHighBoostFreqParam->load(), pultecHighBoostParam->load(),
                                     pultecHighBandwidthParam->load(),
                                     (int) pultecHighAttenFreqParam->load(), pultecHighAttenParam->load());
            pultecEq.processStereo (left, right, numSamples);
        }
        else
        {
            eq.setParameters (eqLowCutParam->load(),
                               eqLowParam->load(), eqLowFreqParam->load(),
                               eqLowMidParam->load(), eqLowMidFreqParam->load(),
                               eqHighMidParam->load(), eqHighMidFreqParam->load(),
                               eqHighParam->load(), eqHighFreqParam->load(),
                               eqHighCutParam->load(),
                               eqAirAmountParam->load());
            eq.processStereo (left, right, numSamples);
        }
    }

    // 5. Delay — temps résolu depuis le tempo hôte si un mode synchronisé est choisi
    // Enveloppe de ducking (mesurée une fois, réutilisée pour Delay ET Reverb) —
    // suit le signal juste avant ces deux étages, réduit leur wet quand la voix
    // est présente, laisse remonter dans les silences.
    {
        const float attackCoeff  = 1.0f - std::exp (-1.0f / (0.005f * (float) currentSampleRate));  // ~5ms
        const float releaseCoeff = 1.0f - std::exp (-1.0f / (0.250f * (float) currentSampleRate));   // ~250ms
        for (int i = 0; i < numSamples; ++i)
        {
            const float peak = std::max (std::fabs (left[i]), std::fabs (right[i]));
            const float coeff = peak > duckEnvelope ? attackCoeff : releaseCoeff;
            duckEnvelope += (peak - duckEnvelope) * coeff;
        }
    }
    const float duckAmount = juce::jlimit (0.0f, 1.0f, duckEnvelope * 3.0f); // sensibilite

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
        const float duckedDelayMix = delayMixParam->load() * (1.0f - duckAmount * delayDuckAmountParam->load());
        delay.setParameters (delayMs, delayFeedbackParam->load(), duckedDelayMix, pingPong);
        delay.processStereo (left, right, numSamples);
    }

    // 6. Reverb
    if (reverbActiveParam->load() > 0.5f)
    {
        const float duckedReverbMix = reverbMixParam->load() * (1.0f - duckAmount * reverbDuckAmountParam->load());
        reverb.setParameters (duckedReverbMix, reverbSizeParam->load(), reverbDampingParam->load());
        reverb.processStereo (left, right, numSamples);
    }

    // 7. Gain de sortie + limiteur de sécurité + mètre (après tout traitement)
    buffer.applyGain (juce::Decibels::decibelsToGain (outputGainParam->load()));
    outputLimiter.setCeilingDb (outputCeilingParam->load());
    outputLimiter.processStereo (left, right, numSamples);
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
