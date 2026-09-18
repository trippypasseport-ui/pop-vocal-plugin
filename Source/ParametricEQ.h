#pragma once
#include <juce_dsp/juce_dsp.h>

/*
    ParametricEQ
    ============================================================
    EQ 3 bandes pensé comme les mouvements de base d'un channel
    strip vocal, fréquence ET gain réglables par bande :
      - Low shelf  (40-400 Hz par défaut 120 Hz)  : corps / proximité
      - Mid bell   (200-5000 Hz par défaut 1 kHz) : présence / nasillard
      - High shelf (2000-16000 Hz par défaut 8kHz): air / brillance
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
            lowShelf[ch].reset();
            midBell[ch].reset();
            highShelf[ch].reset();
        }

        updateCoefficients();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            lowShelf[ch].reset();
            midBell[ch].reset();
            highShelf[ch].reset();
        }
    }

    void setParameters (float lowGainDb, float lowFreqHz, float midGainDb, float midFreqHz,
                         float highGainDb, float highFreqHz)
    {
        lowGain  = lowGainDb;  lowFreq  = lowFreqHz;
        midGain  = midGainDb;  midFreq  = midFreqHz;
        highGain = highGainDb; highFreq = highFreqHz;
        updateCoefficients();
    }

    void processStereo (float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            left[i]  = highShelf[0].processSample (midBell[0].processSample (lowShelf[0].processSample (left[i])));
            right[i] = highShelf[1].processSample (midBell[1].processSample (lowShelf[1].processSample (right[i])));
        }
    }

private:
    void updateCoefficients()
    {
        auto lowCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, lowFreq, 0.707f, juce::Decibels::decibelsToGain (lowGain));
        auto midCoeffs  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, midFreq, 0.9f, juce::Decibels::decibelsToGain (midGain));
        auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, highFreq, 0.707f, juce::Decibels::decibelsToGain (highGain));

        for (int ch = 0; ch < 2; ++ch)
        {
            lowShelf[ch].coefficients  = lowCoeffs;
            midBell[ch].coefficients   = midCoeffs;
            highShelf[ch].coefficients = highCoeffs;
        }
    }

    double sampleRate = 44100.0;
    float lowGain = 0.0f, midGain = 0.0f, highGain = 0.0f;
    float lowFreq = 120.0f, midFreq = 1000.0f, highFreq = 8000.0f;

    juce::dsp::IIR::Filter<float> lowShelf[2], midBell[2], highShelf[2];
};
