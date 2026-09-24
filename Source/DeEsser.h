#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>
#include <vector>

/*
    DeEsser
    ============================================================
    De-esser "split-band" avec lookahead : détecte l'énergie dans
    la zone des sifflantes (~6.5kHz fixe) et réduit dynamiquement
    SEULEMENT cette bande via un filtre en cloche — pas tout
    l'aigu, pas tout le signal.

    Lookahead (~2ms) : la détection tourne sur le signal NON
    retardé, mais le traitement (la réduction de gain) s'applique
    à une version légèrement retardée du signal — la réduction est
    donc déjà en place au moment exact où la sifflante arrive,
    plutôt que de réagir après coup. C'est la pratique standard des
    de-essers pro (confirmé par plusieurs sources spécialisées :
    au moins 1ms de lookahead recommandé pour un rendu transparent).

    Deux contrôles : Threshold et Amount (profondeur max). Fréquence
    fixe à 6.5kHz.
    ============================================================
*/

class DeEsser
{
public:
    void prepare (double sampleRateIn, int /*maxBlockSize*/)
    {
        sampleRate = sampleRateIn;
        controlBlockSize = 32;

        const double controlBlockSeconds = (double) controlBlockSize / sampleRate;
        envelopeAttackCoeff  = 1.0f - (float) std::exp (-controlBlockSeconds / envelopeAttackTimeSeconds);
        envelopeReleaseCoeff = 1.0f - (float) std::exp (-controlBlockSeconds / envelopeReleaseTimeSeconds);
        reductionRiseCoeff    = 1.0f - (float) std::exp (-controlBlockSeconds / reductionRiseTimeSeconds);
        reductionFallCoeff    = 1.0f - (float) std::exp (-controlBlockSeconds / reductionFallTimeSeconds);

        lookaheadSamples = juce::jmax (1, (int) (lookaheadSeconds * sampleRate));
        // Le buffer doit contenir AU MOINS lookaheadSamples + controlBlockSize
        // de marge : sinon, la position de lecture retombe dans la zone que le
        // bloc en cours vient tout juste d'écrire (chevauchement), et on relit
        // des échantillons quasi neufs au lieu du signal vraiment retardé —
        // c'est exactement ce qui causait le grésillement trouvé en test réel.
        const int bufSizeNeeded = lookaheadSamples + controlBlockSize + 16;
        delayBufferL.assign ((size_t) bufSizeNeeded, 0.0f);
        delayBufferR.assign ((size_t) bufSizeNeeded, 0.0f);

        for (int ch = 0; ch < 2; ++ch)
        {
            detectorFilter[ch].coefficients =
                juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, sibilanceFreq, 2.0f);
        }

        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            detectorFilter[ch].reset();
            processingFilter[ch].reset();
        }
        std::fill (delayBufferL.begin(), delayBufferL.end(), 0.0f);
        std::fill (delayBufferR.begin(), delayBufferR.end(), 0.0f);
        delayWritePos = 0;
        envelope = 0.0f;
        smoothedReductionDb = 0.0f;
        updateProcessingFilter (0.0);
    }

    void setParameters (float thresholdDb, float amount01)
    {
        threshold = (double) juce::Decibels::decibelsToGain (thresholdDb);
        maxReductionDb = (double) amount01 * 15.0; // amount 0..1 -> 0..15dB de reduction max
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
    void updateProcessingFilter (double reductionDb)
    {
        const float gainLinear = juce::Decibels::decibelsToGain ((float) -reductionDb);
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, sibilanceFreq, 2.0f, gainLinear);
        processingFilter[0].coefficients = coeffs;
        processingFilter[1].coefficients = coeffs;
    }

    void processControlBlock (float* left, float* right, int blockLen)
    {
        const int bufSize = (int) delayBufferL.size();

        // --- Passe 1 : détection sur le signal NON retardé, + on remplit le
        // buffer de lookahead avec le signal brut pendant qu'on y est ---
        double sum = 0.0;
        for (int i = 0; i < blockLen; ++i)
        {
            const float detL = detectorFilter[0].processSample (left[i]);
            const float detR = detectorFilter[1].processSample (right[i]);
            sum += std::fabs (detL) + std::fabs (detR);

            delayBufferL[(size_t) delayWritePos] = left[i];
            delayBufferR[(size_t) delayWritePos] = right[i];
            delayWritePos = (delayWritePos + 1) % bufSize;
        }
        const double blockLevel = sum / (2.0 * (double) juce::jmax (1, blockLen));

        const float envCoeff = (float) blockLevel > envelope ? envelopeAttackCoeff : envelopeReleaseCoeff;
        envelope += ((float) blockLevel - envelope) * envCoeff;

        double targetReductionDb = 0.0;
        if ((double) envelope > threshold)
        {
            const double excessDb = 20.0 * std::log10 ((double) envelope / threshold);
            targetReductionDb = juce::jmin (excessDb * 2.0, maxReductionDb);
        }

        const float riseFallCoeff = (float) targetReductionDb > smoothedReductionDb ? reductionRiseCoeff : reductionFallCoeff;
        smoothedReductionDb += ((float) targetReductionDb - smoothedReductionDb) * riseFallCoeff;

        updateProcessingFilter ((double) smoothedReductionDb);

        // --- Passe 2 : on relit le signal RETARDÉ (lookahead) et on applique
        // le filtre de réduction dessus -> la réduction est déjà calculée avec
        // la connaissance de ce qui arrive, pas juste une réaction après coup ---
        int readPos = delayWritePos - blockLen - lookaheadSamples;
        while (readPos < 0) readPos += bufSize;

        for (int i = 0; i < blockLen; ++i)
        {
            const float delayedL = delayBufferL[(size_t) readPos];
            const float delayedR = delayBufferR[(size_t) readPos];
            readPos = (readPos + 1) % bufSize;

            left[i]  = processingFilter[0].processSample (delayedL);
            right[i] = processingFilter[1].processSample (delayedR);
        }
    }

    static constexpr float sibilanceFreq = 6500.0f;
    static constexpr double lookaheadSeconds = 0.002; // ~2ms

    static constexpr double envelopeAttackTimeSeconds  = 0.003; // tres rapide, les sifflantes sont breves
    static constexpr double envelopeReleaseTimeSeconds = 0.060;
    static constexpr double reductionRiseTimeSeconds   = 0.005;
    static constexpr double reductionFallTimeSeconds   = 0.100;

    double sampleRate = 44100.0;
    int controlBlockSize = 32;
    int lookaheadSamples = 88; // recalcule dans prepare()
    float envelopeAttackCoeff = 0.5f, envelopeReleaseCoeff = 0.1f;
    float reductionRiseCoeff = 0.5f, reductionFallCoeff = 0.15f;

    double threshold = 0.1;
    double maxReductionDb = 8.0;

    float envelope = 0.0f;
    float smoothedReductionDb = 0.0f;

    juce::dsp::IIR::Filter<float> detectorFilter[2];
    juce::dsp::IIR::Filter<float> processingFilter[2];

    std::vector<float> delayBufferL, delayBufferR;
    int delayWritePos = 0;
};
