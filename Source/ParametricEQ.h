#pragma once
#include <juce_dsp/juce_dsp.h>

/*
    ParametricEQ
    ============================================================
    EQ 3 bandes pensé comme les mouvements de base d'un channel
    strip vocal :
      - Low shelf  ~120 Hz  : corps / proximité
      - Mid bell   ~1 kHz   : présence / nasillard à corriger
      - High shelf ~8 kHz   : air / brillance

    Fréquences et Q fixes pour l'instant, un seul knob de gain
    par bande — facile à étendre plus tard avec freq/Q par bande
    si besoin d'un contrôle plus fin.
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

    void setParameters (float lowGainDb, float midGainDb, float highGainDb)
    {
        lowGain  = lowGainDb;
        midGain  = midGainDb;
        highGain = highGainDb;
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

    static constexpr float lowFreq  = 120.0f;
    static constexpr float midFreq  = 1000.0f;
    static constexpr float highFreq = 8000.0f;

    double sampleRate = 44100.0;
    float lowGain = 0.0f, midGain = 0.0f, highGain = 0.0f;

    juce::dsp::IIR::Filter<float> lowShelf[2], midBell[2], highShelf[2];
};
