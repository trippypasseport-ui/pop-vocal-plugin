#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

/*
    ResonanceSuppressor
    ============================================================
    Suppresseur dynamique de résonances — implémentation
    INDÉPENDANTE, écrite à partir de zéro. Ce n'est PAS un
    portage, une rétro-ingénierie, ni une tentative de reproduire
    l'algorithme de Soothe2 (oeksound) ou de tout autre plugin
    commercial fermé — leur code n'est pas public et je ne l'ai
    pas consulté. Le principe ci-dessous (EQ dynamique
    multi-bandes) est une technique DSP générale, documentée
    dans la littérature et utilisée sous des formes variées par
    de nombreux outils de ce type.

    Principe :
      - un banc de bandes fixes couvre le spectre vocal utile
      - pour chaque bande, un filtre de détection (passe-bande,
        non modifié) mesure l'énergie de cette bande
      - cette énergie est comparée à l'énergie large-bande du
        signal (référence)
      - si une bande dépasse significativement cette référence,
        un filtre en cloche (peak filter) à cette fréquence
        réduit dynamiquement son niveau, proportionnellement au
        dépassement
      - la réduction est lissée (attack/release) et recalculée
        par petits blocs de contrôle (32 échantillons) plutôt
        qu'échantillon par échantillon, pour rester raisonnable
        en CPU et éviter le bruit de commutation des coefficients

    Trois contrôles, dans l'esprit d'un outil simple à utiliser :
      - sensitivity (0..1) : à quel point une bande doit dépasser
        la référence avant d'être atténuée
      - depth (0..1)       : profondeur maximale de réduction
      - mix (0..1)         : dry/wet de l'étage entier

    À VALIDER À L'OREILLE avant usage — voir la même remarque que
    pour PopCompressor.h : ce code n'a pas été compilé ni testé
    par son auteur.
    ============================================================
*/

class ResonanceSuppressor
{
public:
    static constexpr int numBands = 8;

    /**
        bandQIn : largeur des bandes de détection/correction.
          - Q bas (~1.5-2)  -> bandes larges, comportement "lissant"
            (chevauchement important entre bandes voisines)
          - Q haut (~5-7)   -> bandes étroites, comportement "précis"
            (chirurgical, cible une fréquence sans trop toucher
            ses voisines)
    */
    explicit ResonanceSuppressor (float bandQIn = 3.5f) : bandQ (bandQIn) {}

    void prepare (double sampleRateIn, int maxBlockSize)
    {
        sampleRate = sampleRateIn;
        controlBlockSize = 32;

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int b = 0; b < numBands; ++b)
            {
                detectorFilters[ch][b].coefficients =
                    juce::dsp::IIR::Coefficients<float>::makeBandPass (
                        sampleRate, bandFrequencies[(size_t) b], bandQ);
                detectorFilters[ch][b].reset();

                updateProcessingFilter (ch, b, 0.0);
                processingFilters[ch][b].reset();

                bandEnvelope[ch][b] = 0.0f;
                smoothedReductionDb[ch][b] = 0.0f;
            }
            broadbandEnvelope[ch] = 0.0f;
        }

        juce::ignoreUnused (maxBlockSize);
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int b = 0; b < numBands; ++b)
            {
                detectorFilters[ch][b].reset();
                processingFilters[ch][b].reset();
                bandEnvelope[ch][b] = 0.0f;
                smoothedReductionDb[ch][b] = 0.0f;
            }
            broadbandEnvelope[ch] = 0.0f;
        }
    }

    void setParameters (float sensitivity01, float depth01, float mix01)
    {
        // Sensibilité haute -> seuil de déclenchement bas (réagit plus tôt)
        thresholdRatio = 1.05 + (1.0 - (double) sensitivity01) * 2.5;
        maxDepthDb     = (double) depth01 * 18.0;
        mix            = (double) mix01;
    }

    void processStereo (float* left, float* right, int numSamples)
    {
        int sampleIndex = 0;
        while (sampleIndex < numSamples)
        {
            const int blockLen = juce::jmin (controlBlockSize, numSamples - sampleIndex);

            updateControlBlock (left + sampleIndex, right + sampleIndex, blockLen);
            processBlockWithCurrentFilters (left + sampleIndex, right + sampleIndex, blockLen);

            sampleIndex += blockLen;
        }
    }

private:
    void updateProcessingFilter (int channel, int band, double reductionDb)
    {
        const float gainLinear = juce::Decibels::decibelsToGain ((float) -reductionDb);
        processingFilters[channel][band].coefficients =
            juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sampleRate, bandFrequencies[(size_t) band], bandQ, gainLinear);
    }

    void updateControlBlock (const float* left, const float* right, int blockLen)
    {
        // Enveloppe large-bande (mono, moyenne des deux canaux) — la référence
        double broadbandSum = 0.0;
        for (int i = 0; i < blockLen; ++i)
            broadbandSum += std::fabs (0.5 * ((double) left[i] + (double) right[i]));
        const double broadbandBlockLevel = broadbandSum / (double) juce::jmax (1, blockLen);

        const float attackCoeff  = 0.35f;   // réagit vite à une résonance qui apparaît
        const float releaseCoeff = 0.05f;   // relâche plus progressivement

        for (int ch = 0; ch < 2; ++ch)
        {
            broadbandEnvelope[ch] += ((float) broadbandBlockLevel - broadbandEnvelope[ch]) * attackCoeff;

            const float* channelData = (ch == 0) ? left : right;

            for (int b = 0; b < numBands; ++b)
            {
                // Détection : filtre passe-bande fixe, jamais modifié, juste pour mesurer l'énergie
                float bandSum = 0.0f;
                for (int i = 0; i < blockLen; ++i)
                    bandSum += std::fabs (detectorFilters[ch][b].processSample (channelData[i]));
                const float bandBlockLevel = bandSum / (float) juce::jmax (1, blockLen);

                const float envCoeff = bandBlockLevel > bandEnvelope[ch][b] ? attackCoeff : releaseCoeff;
                bandEnvelope[ch][b] += (bandBlockLevel - bandEnvelope[ch][b]) * envCoeff;

                const double ratio = (double) bandEnvelope[ch][b] / ((double) broadbandEnvelope[ch] + 1.0e-6);

                double targetReductionDb = 0.0;
                if (ratio > thresholdRatio)
                {
                    const double excessDb = 20.0 * std::log10 (ratio / thresholdRatio);
                    targetReductionDb = juce::jmin (excessDb * 1.5, maxDepthDb);
                }

                // Lissage de la réduction elle-même (évite les à-coups de filtre)
                const float smoothCoeff = (float) targetReductionDb > smoothedReductionDb[ch][b] ? 0.5f : 0.15f;
                smoothedReductionDb[ch][b] += ((float) targetReductionDb - smoothedReductionDb[ch][b]) * smoothCoeff;

                updateProcessingFilter (ch, b, (double) smoothedReductionDb[ch][b]);
            }
        }
    }

    void processBlockWithCurrentFilters (float* left, float* right, int blockLen)
    {
        for (int i = 0; i < blockLen; ++i)
        {
            const float dryL = left[i];
            const float dryR = right[i];

            float wetL = dryL;
            float wetR = dryR;

            for (int b = 0; b < numBands; ++b)
            {
                wetL = processingFilters[0][b].processSample (wetL);
                wetR = processingFilters[1][b].processSample (wetR);
            }

            left[i]  = (float) ((1.0 - mix) * (double) dryL + mix * (double) wetL);
            right[i] = (float) ((1.0 - mix) * (double) dryR + mix * (double) wetR);
        }
    }

    static constexpr std::array<float, numBands> bandFrequencies {
        250.0f, 500.0f, 1000.0f, 2000.0f, 3500.0f, 5500.0f, 8000.0f, 12000.0f
    };
    const float bandQ;

    double sampleRate = 44100.0;
    int controlBlockSize = 32;

    double thresholdRatio = 2.0;
    double maxDepthDb = 9.0;
    double mix = 1.0;

    juce::dsp::IIR::Filter<float> detectorFilters[2][numBands];
    juce::dsp::IIR::Filter<float> processingFilters[2][numBands];
    float bandEnvelope[2][numBands] {};
    float smoothedReductionDb[2][numBands] {};
    float broadbandEnvelope[2] {};
};
