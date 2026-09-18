#pragma once
#include "ICompressorAlgorithm.h"
#include <cmath>
#include <cstdint>

/*
    VariMuCompressor — mode "Naturel"
    ============================================================
    Portage moderne de l'algorithme VariMu d'Airwindows (licence
    MIT, Copyright (c) 2018 Chris Johnson). Décrit par son auteur
    comme "plus simple que Compressor et mieux adapté au bus
    2-mix... effets naturels — un excellent compresseur de
    mastering" — d'où son rôle ici en mode "Naturel".
    Source originale : plugins/LinuxVST/src/VariMu/VariMuProc.cpp

    Même famille vari-mu ping-pong que Pop (deux jeux de
    coefficients alternés dont les vitesses varient avec le niveau
    du signal), mais détection par échantillon au carré (RMS-like)
    plutôt que par le détecteur biaisé-vers-le-creux de Pop, et pas
    d'étage de saturation — d'où un caractère nettement plus doux
    et transparent.

    Simplification assumée par rapport à l'original : le bruit de
    dither "analogique" (le bloc noisesourceL/R du fichier source,
    qui ajoute un souffle très faible en permanence) a été omis —
    effet inaudible en pratique, non porté pour garder le code
    lisible. Le 4e paramètre original (Output) est fixé
    automatiquement (voir outGain) plutôt qu'exposé en knob, pour
    garder 3 contrôles génériques cohérents avec les autres modes.

    À VALIDER À L'OREILLE avant usage — voir la même remarque que
    dans PopCompressor.h.
    ============================================================
*/

class VariMuCompressor : public ICompressorAlgorithm
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
        flip = false;
    }

    // p1 = Intensity (0..1) ; p2 = Speed/release (0..1) ; p3 = Mix (0..1)
    void setParameters (float p1, float p2, float p3) override
    {
        intensity = clamp01 (p1);
        speed     = clamp01 (p2);
        mix       = clamp01 (p3);
    }

    void processStereo (float* leftChannel, float* rightChannel, int numSamples) override
    {
        const double overallScale = (2.0 / 44100.0) * sampleRate;

        const double A = (double) intensity;
        const double B = (double) speed;
        const double D = (double) mix;

        const double threshold = 1.001 - (1.0 - std::pow (1.0 - A, 3.0));
        double muMakeupGain = std::sqrt (1.0 / threshold);
        muMakeupGain = (muMakeupGain + std::sqrt (muMakeupGain)) / 2.0;
        muMakeupGain = std::sqrt (muMakeupGain);
        const double outGain = std::sqrt (muMakeupGain);
        const double output = outGain; // C original fixé à 1.0 (pas de knob Output dédié ici)

        double release = std::pow (1.15 - B, 5.0) * 32768.0;
        release /= overallScale;
        const double fastest = std::sqrt (release);

        for (int i = 0; i < numSamples; ++i)
        {
            float l = leftChannel[i];
            float r = rightChannel[i];

            processSample (l, left,  threshold, muMakeupGain, release, fastest, output, D, flip);
            processSample (r, right, threshold, muMakeupGain, release, fastest, output, D, flip);

            leftChannel[i]  = l;
            rightChannel[i] = r;

            flip = !flip;
        }
    }

    const char* getName() const override { return "Naturel (VariMu)"; }
    std::array<const char*, 3> getKnobLabels() const override { return { "INTENSITY", "SPEED", "MIX" }; }

private:
    struct ChannelState
    {
        double muVaryA = 1.0, muVaryB = 1.0;
        double muAttackA = 1.0, muAttackB = 1.0;
        double muSpeedA = 10000.0, muSpeedB = 10000.0;
        double muCoefficientA = 1.0, muCoefficientB = 1.0;
        double previous = 0.0;
    };

    static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    static void processSample (float& sample, ChannelState& s, double threshold, double muMakeupGain,
                                double release, double fastest, double output, double wet, bool flipState)
    {
        double inputSample = (double) sample;
        const double drySample = inputSample;

        double squaredSample;
        if (std::fabs (inputSample) > std::fabs (s.previous))
            squaredSample = s.previous * s.previous;
        else
            squaredSample = inputSample * inputSample;
        s.previous = inputSample;
        inputSample *= muMakeupGain;

        double& muVary        = flipState ? s.muVaryA        : s.muVaryB;
        double& muAttack      = flipState ? s.muAttackA       : s.muAttackB;
        double& muSpeed       = flipState ? s.muSpeedA        : s.muSpeedB;
        double& muCoefficient = flipState ? s.muCoefficientA  : s.muCoefficientB;

        if (std::fabs (squaredSample) > threshold)
        {
            muVary = threshold / std::fabs (squaredSample);
            muAttack = std::sqrt (std::fabs (muSpeed));
            muCoefficient *= (muAttack - 1.0);
            muCoefficient += (muVary < threshold) ? threshold : muVary;
            muCoefficient /= muAttack;
        }
        else
        {
            muCoefficient *= ((muSpeed * muSpeed) - 1.0);
            muCoefficient += 1.0;
            muCoefficient /= (muSpeed * muSpeed);
        }

        const double newSpeed = (muSpeed * (muSpeed - 1.0)) + std::fabs (squaredSample * release) + fastest;
        muSpeed = newSpeed / muSpeed;

        const double coefficient = (muCoefficient + muCoefficient * muCoefficient) / 2.0;
        inputSample *= coefficient;

        if (output < 1.0)
            inputSample *= output;

        if (wet < 1.0)
            inputSample = (drySample * (1.0 - wet)) + (inputSample * wet);

        sample = (float) inputSample;
    }

    double sampleRate = 44100.0;
    float intensity = 0.3f, speed = 0.5f, mix = 1.0f;

    ChannelState left, right;
    bool flip = false;
};
