#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

/*
    DelayModule
    ============================================================
    Delay stéréo avec feedback, interpolation linéaire, et un
    mode ping-pong optionnel.

    Le calage tempo (Free / 1/2 / 1/4 / 1/8) est calculé côté
    PluginProcessor (qui a accès au BPM de l'hôte via
    AudioPlayHead) — ce module reçoit directement un temps de
    delay en millisecondes déjà résolu, il n'a pas connaissance
    du tempo lui-même.

    Mode normal : L et R traités indépendamment, même temps de
    delay des deux côtés.
    Mode ping-pong : l'entrée alimente d'abord le tap gauche ;
    chaque répétition rebondit ensuite de gauche à droite et
    vice-versa via un feedback croisé (architecture ping-pong
    classique).
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
        writePos = 0;
    }

    void reset()
    {
        std::fill (bufferL.begin(), bufferL.end(), 0.0f);
        std::fill (bufferR.begin(), bufferR.end(), 0.0f);
        writePos = 0;
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
        const int bufSize = (int) bufferL.size();
        if (bufSize == 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            double readPos = (double) writePos - delayTimeSamples;
            while (readPos < 0.0)
                readPos += bufSize;

            const int readIndex0 = (int) readPos % bufSize;
            const int readIndex1 = (readIndex0 + 1) % bufSize;
            const float frac = (float) (readPos - std::floor (readPos));

            const float delayedL = bufferL[(size_t) readIndex0] * (1.0f - frac) + bufferL[(size_t) readIndex1] * frac;
            const float delayedR = bufferR[(size_t) readIndex0] * (1.0f - frac) + bufferR[(size_t) readIndex1] * frac;

            const float inL = left[i];
            const float inR = right[i];

            if (pingPong)
            {
                // Entrée sommée mono -> tap gauche ; feedback croisé L<->R -> rebond
                const float inputMono = 0.5f * (inL + inR);
                bufferL[(size_t) writePos] = inputMono + delayedR * feedback;
                bufferR[(size_t) writePos] = delayedL * feedback;
            }
            else
            {
                // Mode normal : L et R indépendants, même temps de delay
                bufferL[(size_t) writePos] = inL + delayedL * feedback;
                bufferR[(size_t) writePos] = inR + delayedR * feedback;
            }

            left[i]  = inL * (1.0f - mix) + delayedL * mix;
            right[i] = inR * (1.0f - mix) + delayedR * mix;

            writePos = (writePos + 1) % bufSize;
        }
    }

private:
    double sampleRate = 44100.0;
    double delayTimeSamples = 20000.0;
    float feedback = 0.3f;
    float mix = 0.0f;
    bool pingPong = false;

    std::vector<float> bufferL, bufferR;
    int writePos = 0;
};
