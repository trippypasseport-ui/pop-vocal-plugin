#pragma once
#include <juce_dsp/juce_dsp.h>

/*
    ParametricEQ — mode "Normal"
    ============================================================
    Low Cut -> Low shelf -> Low-Mid bell -> High-Mid bell ->
    High shelf -> Air (dip+lift) -> High Cut

    Pas de bande "Mid" centrale unique : Low-Mid et High-Mid
    couvrent la zone médium avec deux points réglables plutôt
    qu'un seul, à la demande.
      - Low Cut   (coupe-bas, 24dB/oct)
      - Low shelf   (40-400 Hz, défaut 120 Hz)     : corps / proximité
      - Low-Mid bell (150-800 Hz, défaut 300 Hz)   : boxy / rondeur
      - High-Mid bell (1500-6000 Hz, défaut 3 kHz) : mordant / dureté
      - High shelf  (2000-16000 Hz, défaut 8kHz)   : air / brillance
      - Air (knob 0-6, quantité)                   : creux ~7.5kHz + lift ~15kHz
      - High Cut  (coupe-haut, 24dB/oct)
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
            lowShelf[ch].reset(); lowMidBell[ch].reset(); highMidBell[ch].reset(); highShelf[ch].reset();
            airDip[ch].reset(); airShelf[ch].reset();
            highCut1[ch].reset(); highCut2[ch].reset();
        }
        updateCoefficients();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            lowCut1[ch].reset(); lowCut2[ch].reset();
            lowShelf[ch].reset(); lowMidBell[ch].reset(); highMidBell[ch].reset(); highShelf[ch].reset();
            airDip[ch].reset(); airShelf[ch].reset();
            highCut1[ch].reset(); highCut2[ch].reset();
        }
    }

    void setParameters (float lowCutFreqHz,
                         float lowGainDb, float lowFreqHz,
                         float lowMidGainDb, float lowMidFreqHz,
                         float highMidGainDb, float highMidFreqHz,
                         float highGainDb, float highFreqHz,
                         float highCutFreqHz, float airAmount)
    {
        lowCutFreq = lowCutFreqHz;
        lowGain  = lowGainDb;  lowFreq  = lowFreqHz;
        lowMidGain = lowMidGainDb; lowMidFreq = lowMidFreqHz;
        highMidGain = highMidGainDb; highMidFreq = highMidFreqHz;
        highGain = highGainDb; highFreq = highFreqHz;
        highCutFreq = highCutFreqHz;
        airDipDb   = -(airAmount * 0.4f);
        airShelfDb =   airAmount;
        updateCoefficients();
    }

    void processStereo (float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float l = lowCut1[0].processSample (left[i]);
            l = lowCut2[0].processSample (l);
            l = lowShelf[0].processSample (l);
            l = lowMidBell[0].processSample (l);
            l = highMidBell[0].processSample (l);
            l = highShelf[0].processSample (l);
            l = airDip[0].processSample (l);
            l = airShelf[0].processSample (l);
            l = highCut1[0].processSample (l);
            l = highCut2[0].processSample (l);
            left[i] = l;

            float r = lowCut1[1].processSample (right[i]);
            r = lowCut2[1].processSample (r);
            r = lowShelf[1].processSample (r);
            r = lowMidBell[1].processSample (r);
            r = highMidBell[1].processSample (r);
            r = highShelf[1].processSample (r);
            r = airDip[1].processSample (r);
            r = airShelf[1].processSample (r);
            r = highCut1[1].processSample (r);
            r = highCut2[1].processSample (r);
            right[i] = r;
        }
    }

private:
    void updateCoefficients()
    {
        auto lowCutCoeffs   = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, lowCutFreq, 0.707f);
        auto lowCoeffs      = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, lowFreq, 0.707f, juce::Decibels::decibelsToGain (lowGain));
        auto lowMidCoeffs   = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, lowMidFreq, 1.0f, juce::Decibels::decibelsToGain (lowMidGain));
        auto highMidCoeffs  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, highMidFreq, 1.0f, juce::Decibels::decibelsToGain (highMidGain));
        auto highCoeffs     = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, highFreq, 0.707f, juce::Decibels::decibelsToGain (highGain));
        auto airDipCoeffs   = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, airDipFreq, 1.2f, juce::Decibels::decibelsToGain (airDipDb));
        auto airShelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, airShelfFreq, 0.707f, juce::Decibels::decibelsToGain (airShelfDb));
        auto highCutCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, highCutFreq, 0.707f);

        for (int ch = 0; ch < 2; ++ch)
        {
            lowCut1[ch].coefficients = lowCutCoeffs;   lowCut2[ch].coefficients = lowCutCoeffs;
            lowShelf[ch].coefficients = lowCoeffs;
            lowMidBell[ch].coefficients = lowMidCoeffs;
            highMidBell[ch].coefficients = highMidCoeffs;
            highShelf[ch].coefficients = highCoeffs;
            airDip[ch].coefficients = airDipCoeffs;
            airShelf[ch].coefficients = airShelfCoeffs;
            highCut1[ch].coefficients = highCutCoeffs; highCut2[ch].coefficients = highCutCoeffs;
        }
    }

    double sampleRate = 44100.0;
    float lowGain = 0.0f, lowMidGain = 0.0f, highMidGain = 0.0f, highGain = 0.0f;
    float lowFreq = 120.0f, lowMidFreq = 300.0f, highMidFreq = 3000.0f, highFreq = 8000.0f;
    float lowCutFreq = 80.0f, highCutFreq = 18000.0f;
    float airDipDb = 0.0f, airShelfDb = 0.0f;
    static constexpr float airDipFreq = 7500.0f;
    static constexpr float airShelfFreq = 15000.0f;

    juce::dsp::IIR::Filter<float> lowCut1[2], lowCut2[2];
    juce::dsp::IIR::Filter<float> lowShelf[2], lowMidBell[2], highMidBell[2], highShelf[2];
    juce::dsp::IIR::Filter<float> airDip[2], airShelf[2];
    juce::dsp::IIR::Filter<float> highCut1[2], highCut2[2];
};
