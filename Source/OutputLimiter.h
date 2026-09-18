#pragma once
#include <cmath>
#include <algorithm>

/*
    OutputLimiter
    ============================================================
    Limiteur de sécurité simple, sans lookahead, placé tout en fin
    de chaîne (après le gain de sortie, avant le mètre). Attaque
    rapide (~2ms), release modéré (~100ms) — suffisant pour éviter
    qu'un réglage de chaîne trop chaud ne sorte du plugin plus fort
    que prévu, sans prétendre à la transparence d'un vrai limiteur
    de mastering (pas de lookahead = léger dépassement possible sur
    des transitoires très rapides, acceptable pour un usage "garde-fou"
    plutôt que mastering final).
    ============================================================
*/

class OutputLimiter
{
public:
    void prepare (double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        envelope = 1.0f;
    }

    void reset() { envelope = 1.0f; }

    void setCeilingDb (float ceilingDb)
    {
        ceilingLinear = (float) std::pow (10.0, (double) ceilingDb / 20.0);
    }

    void processStereo (float* left, float* right, int numSamples)
    {
        const float attackCoeff  = 1.0f - std::exp (-1.0f / (0.002f * (float) sampleRate)); // ~2ms
        const float releaseCoeff = 1.0f - std::exp (-1.0f / (0.100f * (float) sampleRate)); // ~100ms

        for (int i = 0; i < numSamples; ++i)
        {
            const float peak = std::max (std::fabs (left[i]), std::fabs (right[i]));
            const float targetGain = (peak > ceilingLinear) ? (ceilingLinear / peak) : 1.0f;

            const float coeff = (targetGain < envelope) ? attackCoeff : releaseCoeff;
            envelope += (targetGain - envelope) * coeff;

            left[i]  *= envelope;
            right[i] *= envelope;
        }
    }

private:
    double sampleRate = 44100.0;
    float ceilingLinear = 1.0f;
    float envelope = 1.0f;
};
