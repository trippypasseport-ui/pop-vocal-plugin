#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>

/*
    PultecEQ — mode "Pultec"
    ============================================================
    Inspiré du Pultec EQP-1A (pas une modélisation de circuit
    précise — un moteur numérique qui reproduit le comportement
    caractéristique, pas les imperfections analogiques exactes).

    La particularité qui définit le Pultec : sur la bande grave,
    on peut BOOSTER et ATTÉNUER la même fréquence en même temps.
    Ça paraît contre-intuitif, mais ça crée une bosse large et
    douce juste au-dessus du point choisi, sans jamais devenir
    boueux — la raison pour laquelle cet EQ est utilisé depuis 60
    ans pour donner du corps sans épaissir.

    Fréquences par crans (pas continues) — comme l'original :
      - Grave   : 20 / 30 / 60 / 100 Hz
      - Aigu (boost) : 3 / 4 / 5 / 8 / 10 / 12 / 16 kHz
      - Aigu (atten) : 5 / 10 / 20 kHz

    Signal : Low Boost -> Low Atten -> High Boost -> High Atten
    ============================================================
*/

class PultecEQ
{
public:
    static constexpr std::array<float, 4> lowFreqChoices  { 20.0f, 30.0f, 60.0f, 100.0f };
    static constexpr std::array<float, 7> highBoostFreqChoices { 3000.0f, 4000.0f, 5000.0f, 8000.0f, 10000.0f, 12000.0f, 16000.0f };
    static constexpr std::array<float, 3> highAttenFreqChoices { 5000.0f, 10000.0f, 20000.0f };

    void prepare (double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        for (int ch = 0; ch < 2; ++ch)
        {
            lowBoost[ch].reset(); lowAtten[ch].reset();
            highBoost[ch].reset(); highAtten[ch].reset();
        }
        updateCoefficients();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            lowBoost[ch].reset(); lowAtten[ch].reset();
            highBoost[ch].reset(); highAtten[ch].reset();
        }
    }

    /**
        lowFreqIndex/highBoostFreqIndex/highAttenFreqIndex : index dans les
        tableaux de choix ci-dessus (crans, comme l'original).
        lowBoostDb/lowAttenDb/highBoostDb/highAttenDb : 0..10 (positifs, le
        sens est déjà fixé par le nom du contrôle, comme sur le vrai Pultec).
        highBandwidth : 0..1, large -> étroit (bande passante du boost aigu).
    */
    void setParameters (int lowFreqIndex, float lowBoostDb, float lowAttenDb,
                         int highBoostFreqIndex, float highBoostDb, float highBandwidth,
                         int highAttenFreqIndex, float highAttenDb)
    {
        lowFreq = lowFreqChoices[(size_t) juce::jlimit (0, 3, lowFreqIndex)];
        this->lowBoostDb = lowBoostDb;
        this->lowAttenDb = lowAttenDb;

        highBoostFreq = highBoostFreqChoices[(size_t) juce::jlimit (0, 6, highBoostFreqIndex)];
        this->highBoostDb = highBoostDb;
        // bandwidth 0 (large) -> Q bas ; 1 (étroit) -> Q haut, comme le knob "Bandwidth" original
        highQ = 0.4f + highBandwidth * 2.6f;

        highAttenFreq = highAttenFreqChoices[(size_t) juce::jlimit (0, 2, highAttenFreqIndex)];
        this->highAttenDb = highAttenDb;

        updateCoefficients();
    }

    void processStereo (float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float l = lowBoost[0].processSample (left[i]);
            l = lowAtten[0].processSample (l);
            l = highBoost[0].processSample (l);
            l = highAtten[0].processSample (l);
            left[i] = l;

            float r = lowBoost[1].processSample (right[i]);
            r = lowAtten[1].processSample (r);
            r = highBoost[1].processSample (r);
            r = highAtten[1].processSample (r);
            right[i] = r;
        }
    }

private:
    void updateCoefficients()
    {
        // Boost grave : shelf large (Q bas) -> la bosse douce caractéristique
        auto lowBoostCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, lowFreq, 0.5f, juce::Decibels::decibelsToGain (lowBoostDb));
        // Atten grave : cloche plus étroite, MÊME fréquence -> le "trou" du truc Pultec
        auto lowAttenCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, lowFreq, 1.5f, juce::Decibels::decibelsToGain (-lowAttenDb));

        auto highBoostCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, highBoostFreq, highQ, juce::Decibels::decibelsToGain (highBoostDb));
        auto highAttenCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, highAttenFreq, 0.707f, juce::Decibels::decibelsToGain (-highAttenDb));

        for (int ch = 0; ch < 2; ++ch)
        {
            lowBoost[ch].coefficients  = lowBoostCoeffs;
            lowAtten[ch].coefficients  = lowAttenCoeffs;
            highBoost[ch].coefficients = highBoostCoeffs;
            highAtten[ch].coefficients = highAttenCoeffs;
        }
    }

    double sampleRate = 44100.0;
    float lowFreq = 60.0f, lowBoostDb = 0.0f, lowAttenDb = 0.0f;
    float highBoostFreq = 10000.0f, highBoostDb = 0.0f, highQ = 1.0f;
    float highAttenFreq = 10000.0f, highAttenDb = 0.0f;

    juce::dsp::IIR::Filter<float> lowBoost[2], lowAtten[2];
    juce::dsp::IIR::Filter<float> highBoost[2], highAtten[2];
};
