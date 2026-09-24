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

    v2 — détection RELATIVE au signal large-bande, pas un seuil
    absolu. La v1 comparait le niveau de la bande sifflante à un
    seuil en dB absolu (Threshold, -40..0dB) — problème : cette
    bande est étroite (Q=2 autour de 6.5kHz), donc son niveau
    "normal" est naturellement bien plus bas que le niveau global
    du signal. À un réglage de seuil intuitif, le de-esser ne se
    déclenchait presque jamais ; poussé à fond pour compenser, les
    coefficients du filtre changeaient trop souvent et trop fort,
    donnant un caractère dur/granuleux plutôt qu'une vraie
    réduction propre.
    Corrigé : on compare maintenant le niveau de la bande sifflante
    au niveau large-bande du MÊME signal (même principe que le
    De-Res — bande vs référence globale). Le knob Threshold devient
    une marge relative en dB ("de combien la bande sifflante doit
    dépasser le reste du signal pour déclencher"), qui s'auto-calibre
    quel que soit le niveau d'entrée — pas besoin de retoucher le
    seuil si le gain de la voix change.

    Lookahead (~2ms) : la détection tourne sur le signal NON
    retardé, mais le traitement (la réduction de gain) s'applique
    à une version légèrement retardée du signal — la réduction est
    donc déjà en place au moment exact où la sifflante arrive,
    plutôt que de réagir après coup.

    Deux contrôles : Threshold (marge relative, dB) et Amount
    (profondeur max de réduction). Fréquence fixe à 6.5kHz.
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
        envelopeAttackCoeff   = 1.0f - (float) std::exp (-controlBlockSeconds / envelopeAttackTimeSeconds);
        envelopeReleaseCoeff  = 1.0f - (float) std::exp (-controlBlockSeconds / envelopeReleaseTimeSeconds);
        widebandAttackCoeff   = 1.0f - (float) std::exp (-controlBlockSeconds / widebandTimeConstantSeconds);
        widebandReleaseCoeff  = widebandAttackCoeff; // meme vitesse dans les 2 sens, reference stable
        reductionRiseCoeff    = 1.0f - (float) std::exp (-controlBlockSeconds / reductionRiseTimeSeconds);
        reductionFallCoeff    = 1.0f - (float) std::exp (-controlBlockSeconds / reductionFallTimeSeconds);

        lookaheadSamples = juce::jmax (1, (int) (lookaheadSeconds * sampleRate));
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
        sibilantEnvelope = 0.0f;
        widebandEnvelope = 1.0e-6f;
        smoothedReductionDb = 0.0f;
        updateProcessingFilter (0.0);
    }

    // thresholdMarginDb : de combien (en dB) la bande sifflante doit dépasser
    // le niveau large-bande du signal pour déclencher la réduction. Plus bas
    // = plus sensible. amount01 : profondeur max de réduction (0..1 -> 0..15dB).
    void setParameters (float thresholdMarginDb, float amount01)
    {
        thresholdMargin = thresholdMarginDb;
        maxReductionDb = (double) amount01 * 15.0;
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

        // --- Passe 1 : détection sur le signal NON retardé — niveau de la
        // bande sifflante ET niveau large-bande, mesurés en parallèle ---
        double sibilantSum = 0.0, widebandSum = 0.0;
        for (int i = 0; i < blockLen; ++i)
        {
            const float detL = detectorFilter[0].processSample (left[i]);
            const float detR = detectorFilter[1].processSample (right[i]);
            sibilantSum += std::fabs (detL) + std::fabs (detR);
            widebandSum += std::fabs (left[i]) + std::fabs (right[i]);

            delayBufferL[(size_t) delayWritePos] = left[i];
            delayBufferR[(size_t) delayWritePos] = right[i];
            delayWritePos = (delayWritePos + 1) % bufSize;
        }
        const double n = 2.0 * (double) juce::jmax (1, blockLen);
        const double sibilantLevel = sibilantSum / n;
        const double widebandLevel = widebandSum / n;

        const float sibCoeff = (float) sibilantLevel > sibilantEnvelope ? envelopeAttackCoeff : envelopeReleaseCoeff;
        sibilantEnvelope += ((float) sibilantLevel - sibilantEnvelope) * sibCoeff;

        const float wbCoeff = (float) widebandLevel > widebandEnvelope ? widebandAttackCoeff : widebandReleaseCoeff;
        widebandEnvelope += ((float) widebandLevel - widebandEnvelope) * wbCoeff;

        // Niveau de la bande sifflante RELATIF au signal large-bande (dB) —
        // s'auto-calibre quel que soit le gain d'entrée, contrairement à un
        // seuil absolu.
        const float sib = juce::jmax (1.0e-8f, sibilantEnvelope);
        const float wb  = juce::jmax (1.0e-8f, widebandEnvelope);
        const double relativeDb = 20.0 * std::log10 ((double) sib / (double) wb);

        double targetReductionDb = 0.0;
        if (relativeDb > (double) thresholdMargin)
            targetReductionDb = juce::jmin ((relativeDb - (double) thresholdMargin) * 2.0, maxReductionDb);

        const float riseFallCoeff = (float) targetReductionDb > smoothedReductionDb ? reductionRiseCoeff : reductionFallCoeff;
        smoothedReductionDb += ((float) targetReductionDb - smoothedReductionDb) * riseFallCoeff;

        updateProcessingFilter ((double) smoothedReductionDb);

        // --- Passe 2 : signal RETARDÉ (lookahead), filtre de réduction ---
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

    static constexpr double envelopeAttackTimeSeconds   = 0.003; // tres rapide, les sifflantes sont breves
    static constexpr double envelopeReleaseTimeSeconds  = 0.060;
    static constexpr double widebandTimeConstantSeconds = 0.15;  // reference stable, pas trop reactive
    static constexpr double reductionRiseTimeSeconds    = 0.005;
    static constexpr double reductionFallTimeSeconds    = 0.100;

    double sampleRate = 44100.0;
    int controlBlockSize = 32;
    int lookaheadSamples = 88; // recalcule dans prepare()
    float envelopeAttackCoeff = 0.5f, envelopeReleaseCoeff = 0.1f;
    float widebandAttackCoeff = 0.05f, widebandReleaseCoeff = 0.05f;
    float reductionRiseCoeff = 0.5f, reductionFallCoeff = 0.15f;

    float thresholdMargin = 6.0f; // dB au-dessus du large-bande pour declencher
    double maxReductionDb = 8.0;

    float sibilantEnvelope = 0.0f;
    float widebandEnvelope = 1.0e-6f;
    float smoothedReductionDb = 0.0f;

    juce::dsp::IIR::Filter<float> detectorFilter[2];
    juce::dsp::IIR::Filter<float> processingFilter[2];

    std::vector<float> delayBufferL, delayBufferR;
    int delayWritePos = 0;
};
