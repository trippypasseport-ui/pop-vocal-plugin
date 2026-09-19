#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

/*
    DelayModule
    ============================================================
    Delay stéréo avec feedback, interpolation linéaire, et un
    mode ping-pong optionnel.

    Mode normal : deux lignes indépendantes (bufferL/bufferR), L
    et R traités séparément, même temps de delay des deux côtés
    — le stéréo d'origine du signal est préservé.

    Mode ping-pong — v4, simplifié par rapport aux versions
    précédentes. Au lieu de deux lignes qui se nourrissent l'une
    l'autre (source d'asymétrie de volume et de complexité), on
    utilise UNE SEULE ligne mono dédiée (bufferPingPong), stable
    et classique (aucun branchement conditionnel dans son réseau
    de feedback -> aucun risque de discontinuité à cet endroit).
    Le rebond gauche/droite est obtenu en PANORAMIQUANT ce signal
    unique alternativement à gauche puis à droite, au rythme
    d'une génération de répétition par côté (répétition 1 à
    droite, répétition 2 à gauche, etc.), avec un fondu doux à la
    bascule. Comme c'est le MÊME signal des deux côtés (juste
    déplacé), les deux côtés ont mathématiquement le même niveau
    en moyenne — plus de déséquilibre possible par construction.
    ============================================================
*/

class DelayModule
{
public:
    void prepare (double sampleRateIn, int /*maxBlockSize*/)
    {
        sampleRate = sampleRateIn;
        const int maxDelaySamples = (int) (2.5 * sampleRate);
        bufferL.assign ((size_t) maxDelaySamples, 0.0f);
        bufferR.assign ((size_t) maxDelaySamples, 0.0f);
        bufferPingPong.assign ((size_t) maxDelaySamples, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (bufferL.begin(), bufferL.end(), 0.0f);
        std::fill (bufferR.begin(), bufferR.end(), 0.0f);
        std::fill (bufferPingPong.begin(), bufferPingPong.end(), 0.0f);
        writePos = 0;
        totalSamplesElapsed = 0.0;
        panLeftTarget = 0.0f; // 1ere repetition -> a droite (voir generation ci-dessous)
        panLeftSmoothed = 0.0f;
    }

    /** delayTimeMs déjà résolu (Free ms, ou calculé depuis le tempo côté processeur). */
    void setParameters (double delayTimeMs, float feedback01, float mix01, bool pingPongOn)
    {
        delayTimeSamples = (delayTimeMs * 0.001) * sampleRate;
        feedback = std::min (0.9f, std::max (0.0f, feedback01 * 0.9f));
        mix = mix01;
        pingPong = pingPongOn;

        // Fondu adaptatif : jamais plus de 15% du temps de delay, plafonné à 25ms
        crossfadeSamples = std::max (1.0, std::min (0.025 * sampleRate, delayTimeSamples * 0.15));
    }

    void processStereo (float* left, float* right, int numSamples)
    {
        const int bufSize = (int) bufferL.size();
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

            const float inL = left[i];
            const float inR = right[i];

            // Les deux réseaux (normal ET ping-pong) tournent TOUJOURS en
            // parallèle, même quand un seul est audible — comme ça, basculer
            // le mode en cours de lecture ne tombe jamais sur un buffer froid
            // (silencieux ou périmé), donc pas de trou au changement.
            const float delayedL = bufferL[(size_t) readIndex0] * (1.0f - frac) + bufferL[(size_t) readIndex1] * frac;
            const float delayedR = bufferR[(size_t) readIndex0] * (1.0f - frac) + bufferR[(size_t) readIndex1] * frac;
            const float delayedMono = bufferPingPong[(size_t) readIndex0] * (1.0f - frac)
                                     + bufferPingPong[(size_t) readIndex1] * frac;

            bufferL[(size_t) writePos] = inL + delayedL * feedback;
            bufferR[(size_t) writePos] = inR + delayedR * feedback;

            const float inputMono = 0.5f * (inL + inR);
            bufferPingPong[(size_t) writePos] = inputMono + delayedMono * feedback;

            float wetL, wetR;

            if (pingPong)
            {
                // Génération de répétition en cours -> détermine le côté cible.
                // generation impaire (1ere répétition) -> DROITE ; paire -> GAUCHE.
                totalSamplesElapsed += 1.0;
                const double generation = std::floor (totalSamplesElapsed / delayTimeSamples);
                const bool generationIsOdd = (std::fmod (generation, 2.0) >= 1.0);
                panLeftTarget = generationIsOdd ? 0.0f : 1.0f;

                panLeftSmoothed += (panLeftTarget - panLeftSmoothed) * smoothCoeff;

                // Même signal des deux côtés, juste pondéré différemment -> pas de
                // saut de niveau possible entre les deux canaux.
                wetL = panLeftSmoothed * delayedMono;
                wetR = (1.0f - panLeftSmoothed) * delayedMono;
            }
            else
            {
                wetL = delayedL;
                wetR = delayedR;
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

    double totalSamplesElapsed = 0.0;
    float panLeftTarget = 0.0f;
    float panLeftSmoothed = 0.0f;

    std::vector<float> bufferL, bufferR, bufferPingPong;
    int writePos = 0;
};
