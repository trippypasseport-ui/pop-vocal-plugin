#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>
#include <array>

/*
    AutoBalancer — v2
    ============================================================
    Balanceur automatique — équilibre en continu l'énergie relative
    de plusieurs bandes les unes par rapport aux autres, plutôt que
    de traquer des pics ponctuels (ce n'est pas un suppresseur de
    résonances).

    v2, deux améliorations concrètes après comparaison avec Gullfoss
    (Soundtheory), la référence du marché pour ce type d'outil :

    1. 5 bandes au lieu de 3 (Low / Low-Mid / Mid / High-Mid / High),
       même principe de séparation par soustraction généralisé (4
       passe-bas en cascade, chaque bande = différence entre deux
       passe-bas successifs -> somme des bandes = signal d'origine
       exactement, à gains égaux, sans artefact de phase).

    2. Pondération perceptive (inspirée des courbes Fletcher-Munson,
       que Gullfoss cite explicitement comme une de ses inspirations) :
       l'oreille est moins sensible dans le grave profond et l'aigu
       extrême qu'au médium — donc "même énergie brute dans toutes
       les bandes" ne sonne PAS équilibré à l'oreille. La v1
       comparait l'énergie brute des bandes ; la v2 pondère chaque
       bande avant comparaison pour viser un équilibre perçu plutôt
       que mesuré.

    Ce que ce n'est PAS : un vrai modèle de masquage psychoacoustique
    entre fréquences comme celui de Gullfoss (relations dynamiques
    "quel son masque quel autre son", des années de R&D propriétaire
    dessus) — hors de portée ici, assumé comme limite connue.
    ============================================================
*/

class AutoBalancer
{
public:
    static constexpr int numBands = 5;

    void prepare (double sampleRateIn, int /*maxBlockSize*/)
    {
        sampleRate = sampleRateIn;
        controlBlockSize = 32;

        const double controlBlockSeconds = (double) controlBlockSize / sampleRate;
        smoothCoeff = 1.0f - (float) std::exp (-controlBlockSeconds / smoothTimeConstantSeconds);

        for (int ch = 0; ch < 2; ++ch)
            for (int c = 0; c < numCrossovers; ++c)
                crossoverFilters[ch][c].coefficients =
                    juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, crossoverFreqs[(size_t) c], 0.707f);

        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int c = 0; c < numCrossovers; ++c)
                crossoverFilters[ch][c].reset();

        for (int b = 0; b < numBands; ++b)
            gainSmoothedDb[(size_t) b] = 0.0f;
    }

    void setParameters (float sensitivity01, float depth01, float mix01)
    {
        sensitivity = juce::jlimit (0.0f, 1.0f, sensitivity01);
        maxCorrectionDb = depth01 * 12.0f;
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
    // Découpe le signal en 5 bandes par soustraction de passe-bas en cascade :
    // band[0]     = LP(f0)(x)
    // band[i]     = LP(f_i)(x) - LP(f_{i-1})(x)   pour i = 1..numCrossovers-1
    // band[last]  = x - LP(f_last)(x)
    // Somme des bandes = x exactement, quel que soit l'état des filtres.
    void splitBands (int ch, const float* in, int blockLen, float bands[numBands][64])
    {
        float lp[numCrossovers][64];
        for (int c = 0; c < numCrossovers; ++c)
            for (int i = 0; i < blockLen; ++i)
                lp[c][i] = crossoverFilters[ch][c].processSample (in[i]);

        for (int i = 0; i < blockLen; ++i)
        {
            bands[0][i] = lp[0][i];
            for (int c = 1; c < numCrossovers; ++c)
                bands[c][i] = lp[c][i] - lp[c - 1][i];
            bands[numCrossovers][i] = in[i] - lp[numCrossovers - 1][i];
        }
    }

    void processControlBlock (float* left, float* right, int blockLen)
    {
        float bandsL[numBands][64], bandsR[numBands][64];
        splitBands (0, left,  blockLen, bandsL);
        splitBands (1, right, blockLen, bandsR);

        double bandLevel[numBands];
        for (int b = 0; b < numBands; ++b)
        {
            double sum = 0.0;
            for (int i = 0; i < blockLen; ++i)
                sum += std::fabs (bandsL[b][i]) + std::fabs (bandsR[b][i]);
            bandLevel[b] = (sum / (2.0 * (double) juce::jmax (1, blockLen))) + 1.0e-9;
        }

        // Niveau perçu = niveau brut * poids perceptif (Fletcher-Munson approché) —
        // la cible d'équilibre se calcule sur le PERÇU, pas sur le brut.
        double perceivedLevel[numBands];
        double logSum = 0.0;
        for (int b = 0; b < numBands; ++b)
        {
            perceivedLevel[b] = bandLevel[b] * (double) perceptualWeight[(size_t) b];
            logSum += std::log (perceivedLevel[b]);
        }
        const double avgPerceived = std::exp (logSum / (double) numBands); // moyenne geometrique

        for (int b = 0; b < numBands; ++b)
        {
            const double ratioDb = 20.0 * std::log10 (avgPerceived / perceivedLevel[b]);
            const double scaled = ratioDb * (double) sensitivity;
            const double targetDb = juce::jlimit ((double) -maxCorrectionDb, (double) maxCorrectionDb, scaled);

            gainSmoothedDb[(size_t) b] += ((float) targetDb - gainSmoothedDb[(size_t) b]) * smoothCoeff;
        }

        float bandGain[numBands];
        for (int b = 0; b < numBands; ++b)
            bandGain[b] = juce::Decibels::decibelsToGain (gainSmoothedDb[(size_t) b]);

        for (int i = 0; i < blockLen; ++i)
        {
            float wetL = 0.0f, wetR = 0.0f;
            for (int b = 0; b < numBands; ++b)
            {
                wetL += bandsL[b][i] * bandGain[b];
                wetR += bandsR[b][i] * bandGain[b];
            }
            left[i]  = left[i]  * (1.0f - mix) + wetL * mix;
            right[i] = right[i] * (1.0f - mix) + wetR * mix;
        }
    }

    static constexpr int numCrossovers = numBands - 1;
    static constexpr std::array<float, numCrossovers> crossoverFreqs { 150.0f, 500.0f, 2000.0f, 6000.0f };

    // Poids perceptifs approximatifs (Fletcher-Munson) : l'oreille est moins
    // sensible dans le grave profond et l'aigu extrême qu'au médium autour de
    // 2-5kHz -> ces bandes ont besoin de MOINS d'énergie brute pour paraître
    // aussi présentes, donc un poids plus faible ici les rend moins "punies"
    // par la correction pour un même niveau brut.
    static constexpr std::array<float, numBands> perceptualWeight { 0.6f, 0.85f, 1.15f, 1.1f, 0.75f };

    static constexpr float smoothTimeConstantSeconds = 0.08f;

    double sampleRate = 44100.0;
    int controlBlockSize = 32;
    float smoothCoeff = 0.15f;

    float sensitivity = 0.35f;
    float maxCorrectionDb = 4.8f;
    float mix = 1.0f;

    std::array<float, numBands> gainSmoothedDb {};

    juce::dsp::IIR::Filter<float> crossoverFilters[2][numCrossovers];
};
