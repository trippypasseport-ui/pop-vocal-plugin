#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

/*
    DelayModule
    ============================================================
    Delay stéréo simple : ligne à retard avec feedback,
    interpolation linéaire pour un temps de delay continu
    (pas de zipper noise en tournant le knob).

    Trois contrôles :
      - time (0..1)     -> 20 ms .. 1000 ms
      - feedback (0..1) -> jusqu'à 0.9 (limité pour éviter
                            l'emballement/le larsen numérique)
      - mix (0..1)      -> dry/wet
    ============================================================
*/

class DelayModule
{
public:
    void prepare (double sampleRateIn, int /*maxBlockSize*/)
    {
        sampleRate = sampleRateIn;
        const int maxDelaySamples = (int) (2.5 * sampleRate); // ~2.5 s de marge dans le buffer
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

    void setParameters (float time01, float feedback01, float mix01)
    {
        delayTimeSamples = (20.0 + (double) time01 * 980.0) * 0.001 * sampleRate;
        feedback = std::min (0.9f, std::max (0.0f, feedback01 * 0.9f));
        mix = mix01;
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

            bufferL[(size_t) writePos] = inL + delayedL * feedback;
            bufferR[(size_t) writePos] = inR + delayedR * feedback;

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

    std::vector<float> bufferL, bufferR;
    int writePos = 0;
};
