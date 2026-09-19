#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

/*
    AutoBalancer
    ============================================================
    PAS un suppresseur de résonances — un balanceur automatique
    3 bandes (low/mid/high) qui équilibre en continu l'énergie
    relative des graves, médiums et aigus les uns par rapport aux
    autres, plutôt que de traquer des pics ponctuels.

    Séparation par soustraction (garantit une reconstruction
    parfaite à gains égaux, pas d'artefact de phase de crossover) :
      low  = passe-bas (250Hz) du signal
      high = passe-haut (4000Hz) du signal
      mid  = signal - low - high

    Pour chaque bloc de contrôle, on mesure le niveau de chaque
    bande, on calcule leur moyenne géométrique, et on pousse
    doucement chaque bande vers cette moyenne (bande trop faible
    -> boost, bande trop forte -> atténuation), avec la même
    décision de balance appliquée aux deux canaux stéréo (pour ne
    jamais déséquilibrer l'image stéréo).

    Sensitivity : force de la correction. Depth : plafond en dB de
    la correction possible. Mix : dry/wet de l'étage entier.
    ============================================================
*/

class AutoBalancer
{
public:
    void prepare (double sampleRateIn, int /*maxBlockSize*/)
    {
        sampleRate = sampleRateIn;
        controlBlockSize = 32;

        // Coefficient de lissage calculé depuis une vraie constante de temps
        // (pas une fraction fixe par bloc) — sans ça, le balanceur réagit deux
        // fois plus vite à 96kHz qu'à 44.1kHz pour les mêmes réglages.
        const double controlBlockSeconds = (double) controlBlockSize / sampleRate;
        smoothCoeff = 1.0f - (float) std::exp (-controlBlockSeconds / smoothTimeConstantSeconds);

        for (int ch = 0; ch < 2; ++ch)
        {
            lowFilter[ch].coefficients  = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, lowCrossoverFreq, 0.707f);
            highFilter[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, highCrossoverFreq, 0.707f);
        }

        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            lowFilter[ch].reset();
            highFilter[ch].reset();
        }
        lowGainSmoothedDb = midGainSmoothedDb = highGainSmoothedDb = 0.0f;
    }

    void setParameters (float sensitivity01, float depth01, float mix01)
    {
        sensitivity = juce::jlimit (0.0f, 1.0f, sensitivity01);
        maxCorrectionDb = depth01 * 12.0f; // depth 0..1 -> 0..12dB de correction max par bande
        mix = juce::jlimit (0.0f, 1.0f, mix01);
    }

    void processStereo (float* left, float* right, int numSamples)
    {
        int idx = 0;
        while (idx < numSamples)
        {
            const int blockLen = juce::jmin (controlBlockSize, numSamples - idx);
            processControlBlock (left + idx, right + idx, blockLen);
            idx += blockLen;
        }
    }

private:
    void processControlBlock (float* left, float* right, int blockLen)
    {
        float lowBufL[64], midBufL[64], highBufL[64];
        float lowBufR[64], midBufR[64], highBufR[64];

        double lowSum = 0.0, midSum = 0.0, highSum = 0.0;

        for (int i = 0; i < blockLen; ++i)
        {
            const float l = left[i];
            const float lowL  = lowFilter[0].processSample (l);
            const float hiL   = highFilter[0].processSample (l);
            const float midL  = l - lowL - hiL;
            lowBufL[i] = lowL; midBufL[i] = midL; highBufL[i] = hiL;

            const float r = right[i];
            const float lowR  = lowFilter[1].processSample (r);
            const float hiR   = highFilter[1].processSample (r);
            const float midR  = r - lowR - hiR;
            lowBufR[i] = lowR; midBufR[i] = midR; highBufR[i] = hiR;

            lowSum  += std::fabs (lowL) + std::fabs (lowR);
            midSum  += std::fabs (midL) + std::fabs (midR);
            highSum += std::fabs (hiL)  + std::fabs (hiR);
        }

        const double n = 2.0 * (double) blockLen;
        const double lowLevel  = lowSum  / n + 1.0e-9;
        const double midLevel  = midSum  / n + 1.0e-9;
        const double highLevel = highSum / n + 1.0e-9;

        const double avgLevel = std::cbrt (lowLevel * midLevel * highLevel); // moyenne geometrique des 3

        auto computeTargetDb = [this, avgLevel] (double bandLevel) -> double
        {
            const double ratioDb = 20.0 * std::log10 (avgLevel / bandLevel);
            const double scaled = ratioDb * (double) sensitivity;
            return juce::jlimit ((double) -maxCorrectionDb, (double) maxCorrectionDb, scaled);
        };

        const double lowTargetDb  = computeTargetDb (lowLevel);
        const double midTargetDb  = computeTargetDb (midLevel);
        const double highTargetDb = computeTargetDb (highLevel);

        const float smoothCoeffLocal = smoothCoeff; // lissage vers la cible, temps-constant (voir prepare())
        lowGainSmoothedDb  += ((float) lowTargetDb  - lowGainSmoothedDb)  * smoothCoeffLocal;
        midGainSmoothedDb  += ((float) midTargetDb  - midGainSmoothedDb)  * smoothCoeffLocal;
        highGainSmoothedDb += ((float) highTargetDb - highGainSmoothedDb) * smoothCoeffLocal;

        const float lowGain  = juce::Decibels::decibelsToGain (lowGainSmoothedDb);
        const float midGain  = juce::Decibels::decibelsToGain (midGainSmoothedDb);
        const float highGain = juce::Decibels::decibelsToGain (highGainSmoothedDb);

        for (int i = 0; i < blockLen; ++i)
        {
            const float wetL = lowBufL[i] * lowGain + midBufL[i] * midGain + highBufL[i] * highGain;
            const float wetR = lowBufR[i] * lowGain + midBufR[i] * midGain + highBufR[i] * highGain;

            left[i]  = left[i]  * (1.0f - mix) + wetL * mix;
            right[i] = right[i] * (1.0f - mix) + wetR * mix;
        }
    }

    static constexpr float lowCrossoverFreq  = 250.0f;
    static constexpr float highCrossoverFreq = 4000.0f;

    double sampleRate = 44100.0;
    int controlBlockSize = 32;
    static constexpr float smoothTimeConstantSeconds = 0.08f; // ~80ms, independant du sample rate
    float smoothCoeff = 0.15f; // recalcule dans prepare()

    float sensitivity = 0.35f;
    float maxCorrectionDb = 4.8f;
    float mix = 1.0f;

    float lowGainSmoothedDb = 0.0f, midGainSmoothedDb = 0.0f, highGainSmoothedDb = 0.0f;

    juce::dsp::IIR::Filter<float> lowFilter[2], highFilter[2];
};
