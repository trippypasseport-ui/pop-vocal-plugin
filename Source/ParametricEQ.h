#pragma once
#include <juce_dsp/juce_dsp.h>

/*
    ParametricEQ
    ============================================================
    EQ 3 bandes + coupe-bas/coupe-haut, pensé comme les mouvements
    de base d'un channel strip vocal, fréquence ET gain réglables
    par bande :
      - Low Cut  (coupe-bas, 24dB/oct)            : nettoie ronflement/basses parasites
      - Low shelf  (40-400 Hz, défaut 120 Hz)     : corps / proximité
      - Mid bell   (200-5000 Hz, défaut 1 kHz)    : présence / nasillard
      - High shelf (2000-16000 Hz, défaut 8kHz)   : air / brillance
      - High Cut (coupe-haut, 24dB/oct)           : nettoie souffle/bruit HF

    Ordre du signal : Low Cut -> Low Shelf -> Mid Bell -> High Shelf -> High Cut
    ============================================================
*/

class ParametricEQ
{
public:
    void prepare (double sampleRateIn)
    {
        sampleRate = sampleRateIn;

        for (int ch = 0; ch < 2; ++ch)
        {
            lowCut1[ch].reset(); lowCut2[ch].reset();
            lowShelf[ch].reset();
            midBell[ch].reset();
            highShelf[ch].reset();
            highCut1[ch].reset(); highCut2[ch].reset();
        }

        updateCoefficients();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            lowCut1[ch].reset(); lowCut2[ch].reset();
            lowShelf[ch].reset();
            midBell[ch].reset();
            highShelf[ch].reset();
            highCut1[ch].reset(); highCut2[ch].reset();
        }
    }

    void setParameters (float lowCutFreqHz, float lowGainDb, float lowFreqHz,
                         float midGainDb, float midFreqHz,
                         float highGainDb, float highFreqHz, float highCutFreqHz)
    {
        lowCutFreq = lowCutFreqHz;
        lowGain  = lowGainDb;  lowFreq  = lowFreqHz;
        midGain  = midGainDb;  midFreq  = midFreqHz;
        highGain = highGainDb; highFreq = highFreqHz;
        highCutFreq = highCutFreqHz;
        updateCoefficients();
    }

    void processStereo (float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float l = lowCut1[0].processSample (left[i]);
            l = lowCut2[0].processSample (l);
            l = lowShelf[0].processSample (l);
            l = midBell[0].processSample (l);
            l = highShelf[0].processSample (l);
            l = highCut1[0].processSample (l);
            l = highCut2[0].processSample (l);
            left[i] = l;

            float r = lowCut1[1].processSample (right[i]);
            r = lowCut2[1].processSample (r);
            r = lowShelf[1].processSample (r);
            r = midBell[1].processSample (r);
            r = highShelf[1].processSample (r);
            r = highCut1[1].processSample (r);
            r = highCut2[1].processSample (r);
            right[i] = r;
        }
    }

private:
    void updateCoefficients()
    {
        auto lowCutCoeffs  = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, lowCutFreq, 0.707f);
        auto lowCoeffs     = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, lowFreq, 0.707f, juce::Decibels::decibelsToGain (lowGain));
        auto midCoeffs     = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, midFreq, 0.9f, juce::Decibels::decibelsToGain (midGain));
        auto highCoeffs    = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, highFreq, 0.707f, juce::Decibels::decibelsToGain (highGain));
        auto highCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, highCutFreq, 0.707f);

        for (int ch = 0; ch < 2; ++ch)
        {
            lowCut1[ch].coefficients   = lowCutCoeffs;
            lowCut2[ch].coefficients   = lowCutCoeffs;  // 2 étages en cascade = 24dB/oct
            lowShelf[ch].coefficients  = lowCoeffs;
            midBell[ch].coefficients   = midCoeffs;
            highShelf[ch].coefficients = highCoeffs;
            highCut1[ch].coefficients  = highCutCoeffs;
            highCut2[ch].coefficients  = highCutCoeffs; // idem, 24dB/oct
        }
    }

    double sampleRate = 44100.0;
    float lowGain = 0.0f, midGain = 0.0f, highGain = 0.0f;
    float lowFreq = 120.0f, midFreq = 1000.0f, highFreq = 8000.0f;
    float lowCutFreq = 80.0f, highCutFreq = 18000.0f;

    juce::dsp::IIR::Filter<float> lowCut1[2], lowCut2[2];
    juce::dsp::IIR::Filter<float> lowShelf[2], midBell[2], highShelf[2];
    juce::dsp::IIR::Filter<float> highCut1[2], highCut2[2];
};
