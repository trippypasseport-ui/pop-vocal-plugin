#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

/*
    DelayModule
    ============================================================
    Delay stéréo avec feedback, interpolation linéaire, et un
    mode ping-pong optionnel.

    Mode normal : L et R traités indépendamment, même temps de
    delay des deux côtés.

    Mode ping-pong — refonte v2 : le RÉSEAU de feedback interne
    (génération des répétitions) reste TOUJOURS fixe et stable —
    aucune discontinuité possible à cet endroit. Ce qui alterne,
    c'est uniquement le ROUTAGE de sortie (quel canal physique
    reçoit la répétition "génératrice" la plus forte), et ce
    routage est CROSSFADÉ en douceur (~25ms) plutôt que basculé
    d'un coup — c'est ce qui causait le clic à chaque bascule
    dans la v1. Le côté privilégié alterne toutes les
    delayTimeSamples, pour une vraie alternance perceptible sans
    artefact.
    ============================================================
*/

class DelayModule
{
public:
    void prepare (double sampleRateIn, int /*maxBlockSize*/)
    {
        sampleRate = sampleRateIn;
        const int maxDelaySamples = (int) (2.5 * sampleRate);
        bufferGen.assign ((size_t) maxDelaySamples, 0.0f);
        bufferFollow.assign ((size_t) maxDelaySamples, 0.0f);
        crossfadeSamples = std::max (1.0, 0.025 * sampleRate); // ~25ms de rampe
        reset();
    }

    void reset()
    {
        std::fill (bufferGen.begin(), bufferGen.end(), 0.0f);
        std::fill (bufferFollow.begin(), bufferFollow.end(), 0.0f);
        writePos = 0;
        sampleCounterSinceFlip = 0.0;
        genOnLeftTarget = 1.0f;
        genOnLeftSmoothed = 1.0f;
    }

    /** delayTimeMs déjà résolu (Free ms, ou calculé depuis le tempo côté processeur). */
    void setParameters (double delayTimeMs, float feedback01, float mix01, bool pingPongOn)
    {
        delayTimeSamples = (delayTimeMs * 0.001) * sampleRate;
        feedback = std::min (0.9f, std::max (0.0f, feedback01 * 0.9f));
        mix = mix01;
        pingPong = pingPongOn;
    }

    void processStereo (float* left, float* right, int numSamples)
    {
        const int bufSize = (int) bufferGen.size();
        if (bufSize == 0)
            return;

        const float smoothCoeff = (float) (1.0 / crossfadeSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            double readPos = (double) writePos - delayTimeSamples;
            while (readPos < 0.0)
                readPos += bufSize;

            const int readIndex0 = (int) readPos % bufSize;
            const int readIndex1 = (readIndex0 + 1) % bufSize;
            const float frac = (float) (readPos - std::floor (readPos));

            const float delayedGen    = bufferGen[(size_t) readIndex0]    * (1.0f - frac) + bufferGen[(size_t) readIndex1]    * frac;
            const float delayedFollow = bufferFollow[(size_t) readIndex0] * (1.0f - frac) + bufferFollow[(size_t) readIndex1] * frac;

            const float inL = left[i];
            const float inR = right[i];

            float wetL, wetR;

            if (pingPong)
            {
                // Réseau de feedback FIXE — toujours la même topologie, jamais de rupture ici
                const float inputMono = 0.5f * (inL + inR);
                bufferGen[(size_t) writePos]    = inputMono + delayedFollow * feedback;
                bufferFollow[(size_t) writePos] = delayedGen * feedback;

                // Bascule périodique de la CIBLE de routage (pas du réseau lui-même)
                sampleCounterSinceFlip += 1.0;
                if (sampleCounterSinceFlip >= delayTimeSamples)
                {
                    sampleCounterSinceFlip -= delayTimeSamples;
                    genOnLeftTarget = 1.0f - genOnLeftTarget;
                }

                // Suivi en douceur de la cible (~25ms) -> zéro discontinuité audible
                genOnLeftSmoothed += (genOnLeftTarget - genOnLeftSmoothed) * smoothCoeff;

                wetL = genOnLeftSmoothed * delayedGen + (1.0f - genOnLeftSmoothed) * delayedFollow;
                wetR = (1.0f - genOnLeftSmoothed) * delayedGen + genOnLeftSmoothed * delayedFollow;
            }
            else
            {
                // Mode normal : L et R indépendants, même temps de delay
                bufferGen[(size_t) writePos]    = inL + delayedGen * feedback;
                bufferFollow[(size_t) writePos] = inR + delayedFollow * feedback;
                wetL = delayedGen;
                wetR = delayedFollow;
            }

            left[i]  = inL * (1.0f - mix) + wetL * mix;
            right[i] = inR * (1.0f - mix) + wetR * mix;

            writePos = (writePos + 1) % bufSize;
        }
    }

private:
    double sampleRate = 44100.0;
    double delayTimeSamples = 20000.0;
    double crossfadeSamples = 1000.0;
    float feedback = 0.3f;
    float mix = 0.0f;
    bool pingPong = false;

    double sampleCounterSinceFlip = 0.0;
    float genOnLeftTarget = 1.0f;
    float genOnLeftSmoothed = 1.0f;

    std::vector<float> bufferGen, bufferFollow;
    int writePos = 0;
};
