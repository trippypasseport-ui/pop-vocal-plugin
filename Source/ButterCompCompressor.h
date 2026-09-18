#pragma once
#include "ICompressorAlgorithm.h"
#include <cmath>
#include <cstdint>

/*
    ButterCompCompressor — mode "Doux"
    ============================================================
    Portage moderne de l'algorithme ButterComp d'Airwindows
    (licence MIT, Copyright (c) 2018 Chris Johnson).
    Source originale : plugins/LinuxVST/src/ButterComp/ButterCompProc.cpp

    Compresseur à deux paramètres seulement dans l'original
    (Compress, Dry/Wet) — caractère propre et transparent, dans
    l'esprit d'un bus compressor analogique doux (c'est le moteur
    utilisé, en cascade, dans plusieurs compresseurs de bus dérivés
    d'Airwindows). Détection séparée des demi-cycles positif/négatif
    du signal (pas un suiveur d'enveloppe classique type peak/RMS),
    ce qui donne son grain particulier.

    Le knob "OUTPUT" est un AJOUT de ce portage — un simple gain de
    sortie post-traitement, absent de l'algorithme original à 2
    paramètres — pour garder 3 knobs génériques cohérents avec les
    autres modes de la section Compressor.

    À VALIDER À L'OREILLE avant usage — voir la même remarque que
    dans PopCompressor.h : ce code n'a pas été compilé ni testé par
    son auteur.
    ============================================================
*/

class ButterCompCompressor : public ICompressorAlgorithm
{
public:
    void prepare (double sampleRateIn) override
    {
        sampleRate = sampleRateIn;
        reset();
    }

    void reset() override
    {
        left = ChannelState {};
        right = ChannelState {};
        fpdL = 1;
        fpdR = 1;
    }

    // p1 = Compress (0..1) ; p2 = Output trim, ajout de ce portage (0..1 -> -12..+12 dB) ; p3 = Mix (0..1)
    void setParameters (float p1, float p2, float p3) override
    {
        compressAmount = clamp01 (p1);
        outputGainDb   = (clamp01 (p2) - 0.5f) * 24.0f;
        mix            = clamp01 (p3);
    }

    void processStereo (float* leftChannel, float* rightChannel, int numSamples) override
    {
        const double overallScale = sampleRate / 44100.0;
        const double A = (double) compressAmount;
        const double wet = (double) mix;

        const double inputGain = std::pow (10.0, (A * 14.0) / 20.0);
        double outputGain = inputGain;
        outputGain -= 1.0; outputGain /= 1.5; outputGain += 1.0;

        double divisor = 0.012 * (A / 135.0);
        divisor /= overallScale;
        const double remainder = divisor;
        divisor = 1.0 - divisor;

        const float postGain = dbToGain (outputGainDb);

        for (int i = 0; i < numSamples; ++i)
        {
            float l = leftChannel[i];
            float r = rightChannel[i];

            processSample (l, left,  inputGain, outputGain, divisor, remainder, wet, fpdL);
            processSample (r, right, inputGain, outputGain, divisor, remainder, wet, fpdR);

            leftChannel[i]  = l * postGain;
            rightChannel[i] = r * postGain;
        }
    }

    const char* getName() const override { return "Doux (ButterComp)"; }
    std::array<const char*, 3> getKnobLabels() const override { return { "COMPRESS", "OUTPUT", "MIX" }; }

private:
    struct ChannelState
    {
        double targetPos = 1.0, targetNeg = 1.0;
        double controlAPos = 1.0, controlANeg = 1.0;
    };

    static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    static float dbToGain (float db) { return (float) std::pow (10.0, (double) db / 20.0); }

    static void processSample (float& sample, ChannelState& s, double inputGain, double outputGain,
                                double divisor, double remainder, double wet, uint32_t& fpd)
    {
        double inputSample = (double) sample;

        if (std::fabs (inputSample) < 1.18e-23)
            inputSample = (double) fpd * 1.18e-17;

        const double drySample = inputSample;
        inputSample *= inputGain;

        double inputPos = inputSample + 1.0;
        if (inputPos < 0.0) inputPos = 0.0;
        double outputPos = inputPos / 2.0;
        if (outputPos > 1.0) outputPos = 1.0;
        inputPos *= inputPos;
        s.targetPos *= divisor;
        s.targetPos += (inputPos * remainder);
        const double calcPos = std::pow (1.0 / s.targetPos, 2.0);

        double inputNeg = (-inputSample) + 1.0;
        if (inputNeg < 0.0) inputNeg = 0.0;
        double outputNeg = inputNeg / 2.0;
        if (outputNeg > 1.0) outputNeg = 1.0;
        inputNeg *= inputNeg;
        s.targetNeg *= divisor;
        s.targetNeg += (inputNeg * remainder);
        const double calcNeg = std::pow (1.0 / s.targetNeg, 2.0);

        if (inputSample > 0.0)
        {
            s.controlAPos *= divisor;
            s.controlAPos += (calcPos * remainder);
        }
        else
        {
            s.controlANeg *= divisor;
            s.controlANeg += (calcNeg * remainder);
        }

        const double totalMultiplier = (s.controlAPos * outputPos) + (s.controlANeg * outputNeg);

        inputSample *= totalMultiplier;
        inputSample /= outputGain;

        if (wet != 1.0)
            inputSample = (inputSample * wet) + (drySample * (1.0 - wet));

        fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;

        sample = (float) inputSample;
    }

    double sampleRate = 44100.0;
    float compressAmount = 0.3f;
    float outputGainDb = 0.0f;
    float mix = 1.0f;

    ChannelState left, right;
    uint32_t fpdL = 1, fpdR = 1;
};
