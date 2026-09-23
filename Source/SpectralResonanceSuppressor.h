#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

/*
    SpectralResonanceSuppressor
    ============================================================
    De-Res "adaptatif" — remplace l'ancienne version à 8 bandes
    fixes. Ici, l'analyse spectrale (FFT) détecte la fréquence
    EXACTE qui dépasse, où qu'elle soit dans le spectre, plutôt
    que de ne réagir qu'à des points de détection prédéfinis.
    C'est l'architecture réelle des outils de résonance dynamique
    du marché (Soothe2 inclus) — analyse par blocs recouvrants
    (STFT), comparaison de chaque bin de fréquence à une référence
    "spectre normal" suivie dans le temps, réduction ciblée bin
    par bin, reconstruction par superposition-addition (OLA).

    Détails techniques (pour qui reprend ce fichier) :
      - FFT 1024 points, recouvrement 50% (hop = 512)
      - Fenêtre de Hann appliquée UNIQUEMENT à l'analyse (pas à la
        synthèse) — cette configuration précise (Hann + hop=N/2)
        donne une reconstruction PARFAITE par construction
        (Hann(n) + Hann(n+N/2) = 1 exactement, vérifié
        numériquement avant d'écrire ce fichier, voir le journal
        de session) — donc aucune normalisation OLA n'est
        nécessaire à la resynthèse.
      - Latence induite : exactement fftSize (1024 échantillons,
        ~23ms à 44.1kHz) — DOIT être déclarée à l'hôte via
        setLatencySamples() côté PluginProcessor, sinon décalage
        temporel en enregistrement live à travers le plugin.

    Point à vérifier en priorité (pas testable sans compiler) :
      la normalisation de la FFT inverse de JUCE. Ce fichier
      suppose que `juce::dsp::FFT::perform(..., inverse=true)` NE
      divise PAS automatiquement par fftSize (contrairement à
      certaines bibliothèques comme numpy) — la division
      manuelle par fftSize est donc faite explicitement plus bas.
      Si le son sort avec un volume totalement aberrant (beaucoup
      trop fort ou beaucoup trop faible), c'est le premier endroit
      à vérifier.
    ============================================================
*/

class SpectralResonanceSuppressor
{
public:
    static constexpr int fftOrder = 10;              // 2^10 = 1024
    static constexpr int fftSize  = 1 << fftOrder;   // 1024
    static constexpr int hopSize  = fftSize / 2;     // 512, recouvrement 50%
    static constexpr int numBins  = fftSize / 2 + 1; // bins utiles (0..Nyquist)

    SpectralResonanceSuppressor() : fft (fftOrder) {}

    void prepare (double sampleRateIn, int /*maxBlockSize*/)
    {
        sampleRate = sampleRateIn;

        window.resize ((size_t) fftSize);
        for (int n = 0; n < fftSize; ++n)
            window[(size_t) n] = 0.5f - 0.5f * std::cos (
                2.0f * juce::MathConstants<float>::pi * (float) n / (float) fftSize);

        for (int ch = 0; ch < 2; ++ch)
        {
            inputRingBuffer[ch].assign ((size_t) fftSize, 0.0f);
            outputAccumulator[ch].assign ((size_t) fftSize, 0.0f);
        }
        dryDelayL.assign ((size_t) fftSize, 0.0f);
        dryDelayR.assign ((size_t) fftSize, 0.0f);

        referenceMag.assign ((size_t) numBins, 1.0e-6f);
        smoothedGain.assign ((size_t) numBins, 1.0f);

        // Constantes de temps réelles (secondes), converties en coefficients
        // par frame (chaque frame = hopSize échantillons) — indépendant du
        // sample rate, même principe que les corrections déjà faites ailleurs
        // dans le projet (voir AutoBalancer / ResonanceSuppressor / DeEsser).
        const double frameSeconds = (double) hopSize / sampleRate;
        referenceCoeff = 1.0f - (float) std::exp (-frameSeconds / referenceTimeConstantSeconds);
        gainRiseCoeff  = 1.0f - (float) std::exp (-frameSeconds / gainRiseTimeConstantSeconds);
        gainFallCoeff  = 1.0f - (float) std::exp (-frameSeconds / gainFallTimeConstantSeconds);

        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill (inputRingBuffer[ch].begin(), inputRingBuffer[ch].end(), 0.0f);
            std::fill (outputAccumulator[ch].begin(), outputAccumulator[ch].end(), 0.0f);
            ringWritePos[ch] = 0;
            samplesSinceLastHop[ch] = 0;
        }
        std::fill (dryDelayL.begin(), dryDelayL.end(), 0.0f);
        std::fill (dryDelayR.begin(), dryDelayR.end(), 0.0f);
        dryDelayWritePos = 0;
        std::fill (referenceMag.begin(), referenceMag.end(), 1.0e-6f);
        std::fill (smoothedGain.begin(), smoothedGain.end(), 1.0f);
    }

    void setParameters (float sensitivity01, float depth01, float mix01)
    {
        sensitivity = juce::jlimit (0.0f, 1.0f, sensitivity01);
        maxReductionDb = depth01 * 18.0f;
        mix = juce::jlimit (0.0f, 1.0f, mix01);
    }

    static constexpr int getLatencySamples() { return fftSize; }

    void processStereo (float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            // Le chemin "wet" a fftSize échantillons de retard (latence FFT) —
            // il faut retarder le "dry" du même montant avant de les mélanger,
            // sinon le Mix combine deux versions du signal décalées de ~23ms.
            const float rawDryL = left[i];
            const float rawDryR = right[i];

            const float alignedDryL = dryDelayL[(size_t) dryDelayWritePos];
            const float alignedDryR = dryDelayR[(size_t) dryDelayWritePos];
            dryDelayL[(size_t) dryDelayWritePos] = rawDryL;
            dryDelayR[(size_t) dryDelayWritePos] = rawDryR;
            dryDelayWritePos = (dryDelayWritePos + 1) % fftSize;

            const float wetL = processSampleChannel (0, rawDryL);
            const float wetR = processSampleChannel (1, rawDryR);

            left[i]  = alignedDryL * (1.0f - mix) + wetL * mix;
            right[i] = alignedDryR * (1.0f - mix) + wetR * mix;
        }
    }

private:
    float processSampleChannel (int ch, float inputSample)
    {
        inputRingBuffer[ch][(size_t) ringWritePos[ch]] = inputSample;

        const float outputSample = outputAccumulator[ch][(size_t) ringWritePos[ch]];
        outputAccumulator[ch][(size_t) ringWritePos[ch]] = 0.0f;

        ringWritePos[ch] = (ringWritePos[ch] + 1) % fftSize;
        samplesSinceLastHop[ch]++;

        if (samplesSinceLastHop[ch] >= hopSize)
        {
            samplesSinceLastHop[ch] = 0;
            processFrame (ch);
        }

        return outputSample;
    }

    void processFrame (int ch)
    {
        std::vector<std::complex<float>> frame ((size_t) fftSize);
        for (int n = 0; n < fftSize; ++n)
        {
            const int idx = (ringWritePos[ch] + n) % fftSize; // plus ancien -> plus recent
            frame[(size_t) n] = std::complex<float> (inputRingBuffer[ch][(size_t) idx] * window[(size_t) n], 0.0f);
        }

        std::vector<std::complex<float>> spectrum ((size_t) fftSize);
        fft.perform (frame.data(), spectrum.data(), false);

        // Référence/gain partagés entre L et R (calculés une fois par frame,
        // au passage du canal 0) pour que la correction reste identique des
        // deux côtés et ne déséquilibre jamais l'image stéréo.
        if (ch == 0)
            updateGainCurve (spectrum);

        for (int b = 0; b < numBins; ++b)
        {
            spectrum[(size_t) b] *= smoothedGain[(size_t) b];
            if (b > 0 && b < fftSize - b)
                spectrum[(size_t) (fftSize - b)] = std::conj (spectrum[(size_t) b]); // miroir conjugué
        }

        std::vector<std::complex<float>> timeDomain ((size_t) fftSize);
        fft.perform (spectrum.data(), timeDomain.data(), true);

        for (int n = 0; n < fftSize; ++n)
        {
            const int idx = (ringWritePos[ch] + n) % fftSize;
            // Division par fftSize : voir la note en tête de fichier sur la
            // normalisation de la FFT inverse — a vérifier en priorité.
            outputAccumulator[ch][(size_t) idx] += timeDomain[(size_t) n].real() / (float) fftSize;
        }
    }

    void updateGainCurve (const std::vector<std::complex<float>>& spectrum)
    {
        const float thresholdDb = 3.0f + (1.0f - sensitivity) * 12.0f; // sensitivity haute -> seuil bas

        for (int b = 0; b < numBins; ++b)
        {
            const float currentMag = std::abs (spectrum[(size_t) b]);
            referenceMag[(size_t) b] += (currentMag - referenceMag[(size_t) b]) * referenceCoeff;

            const float ref = juce::jmax (1.0e-6f, referenceMag[(size_t) b]);
            const float cur = juce::jmax (1.0e-6f, currentMag);
            const float excessDb = 20.0f * std::log10 (cur / ref);

            float targetReductionDb = 0.0f;
            if (excessDb > thresholdDb)
                targetReductionDb = juce::jmin ((excessDb - thresholdDb) * 1.5f, maxReductionDb);

            const float targetGain = juce::Decibels::decibelsToGain (-targetReductionDb);
            const float coeff = targetGain < smoothedGain[(size_t) b] ? gainRiseCoeff : gainFallCoeff;
            smoothedGain[(size_t) b] += (targetGain - smoothedGain[(size_t) b]) * coeff;
        }

        // Lissage léger sur les bins voisins (moyenne 3 points) pour éviter
        // les discontinuités en dents de scie d'un bin à l'autre — le genre
        // d'artefact qui donnerait un son "métallique"/granuleux sans ça.
        std::vector<float> smoothedAcrossFreq = smoothedGain;
        for (int b = 1; b < numBins - 1; ++b)
            smoothedAcrossFreq[(size_t) b] = 0.25f * smoothedGain[(size_t) (b - 1)]
                                            + 0.5f  * smoothedGain[(size_t) b]
                                            + 0.25f * smoothedGain[(size_t) (b + 1)];
        smoothedGain = smoothedAcrossFreq;
    }

    juce::dsp::FFT fft;
    double sampleRate = 44100.0;

    std::vector<float> window;
    std::vector<float> inputRingBuffer[2];
    std::vector<float> outputAccumulator[2];
    int ringWritePos[2] { 0, 0 };
    int samplesSinceLastHop[2] { 0, 0 };

    std::vector<float> dryDelayL, dryDelayR;
    int dryDelayWritePos = 0;

    std::vector<float> referenceMag;
    std::vector<float> smoothedGain;

    float referenceCoeff = 0.1f, gainRiseCoeff = 0.5f, gainFallCoeff = 0.1f;
    static constexpr float referenceTimeConstantSeconds = 0.4f;  // "spectre normal" moyenne sur ~400ms
    static constexpr float gainRiseTimeConstantSeconds  = 0.03f; // reaction rapide a une resonance
    static constexpr float gainFallTimeConstantSeconds  = 0.25f; // relachement plus progressif

    float sensitivity = 0.5f, maxReductionDb = 9.0f, mix = 1.0f;
};
