#pragma once
#include "ICompressorAlgorithm.h"
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

/*
    PopCompressor — mode "Agressif"
    ============================================================
    Portage moderne (sans API VST2, sans dépendance JUCE) de
    l'algorithme DSP "Pop" d'Airwindows (Chris Johnson).

    Source originale : github.com/airwindows/airwindows
                        plugins/LinuxVST/src/Pop/PopProc.cpp
    Licence           : MIT (voir LICENSE-airwindows-MIT.txt)
                        Copyright (c) 2018 Chris Johnson

    Ce fichier réimplémente fidèlement la logique sample-par-
    sample de l'original : compresseur "vari-mu" program-
    dependent (deux jeux de coefficients en ping-pong, vitesses
    d'attaque/release qui varient avec le niveau du signal) +
    saturation sinusoïdale intégrée ("Spiral") + un flou/
    épaississement contrôlé via une ligne à retard courte.

    ATTENTION — À VALIDER À L'OREILLE avant usage :
    ce portage a été réécrit à la main à partir du code source
    original pour en extraire une classe autonome, stateful,
    sans API de plugin. La logique mathématique suit fidèlement
    l'original ligne à ligne, mais n'a pas été compilée ni
    testée en conditions réelles (l'auteur de ce fichier n'a
    pas d'accès à un environnement audio). Avant de l'intégrer
    à un outil de production, compare-le à l'oreille avec le
    plugin AU/VST original d'Airwindows sur le même signal.

    Utilisation typique dans un juce::AudioProcessor :

        // membre de la classe processeur
        PopCompressor popL, popR; // ou une seule instance stéréo

        // dans prepareToPlay()
        pop.prepare (sampleRate);

        // dans processBlock()
        pop.setParameters (intensityParam, outputParam, mixParam);
        pop.processStereo (buffer.getWritePointer (0),
                            buffer.getWritePointer (1),
                            buffer.getNumSamples());
    ============================================================
*/

class PopCompressor : public ICompressorAlgorithm
{
public:
    PopCompressor() = default;

    const char* getName() const override { return "Agressif (Pop)"; }
    std::array<const char*, 3> getKnobLabels() const override { return { "INTENSITY", "OUTPUT", "MIX" }; }

    /** À appeler depuis prepareToPlay(). */
    void prepare (double sampleRate) override
    {
        overallScale = sampleRate / 44100.0;

        maxDelay = static_cast<int> (1450.0 * overallScale);
        maxDelay = std::clamp (maxDelay, 1, 9999);

        delayBufferL.assign (static_cast<size_t> (maxDelay) + 1, 0.0);
        delayBufferR.assign (static_cast<size_t> (maxDelay) + 1, 0.0);

        reset();
    }

    /** Réinitialise tout l'état interne (silence, changement de contexte). */
    void reset() override
    {
        std::fill (delayBufferL.begin(), delayBufferL.end(), 0.0);
        std::fill (delayBufferR.begin(), delayBufferR.end(), 0.0);

        delayIndex = 0;
        flip = false;

        left  = ChannelState {};
        right = ChannelState {};

        fpdL = 1;
        fpdR = 1;
    }

    /**
        intensityParam : 0..1 — seul vrai contrôle du caractère de
                          compression (threshold + makeup gain +
                          vitesse de release en dépendent tous).
        outputGainParam : 0..1 — atténuation de sortie (pas de boost,
                          comme l'original : n'agit qu'en dessous de 1.0).
        mixParam        : 0..1 — dry/wet.
    */
    void setParameters (float intensityParam, float outputGainParam, float mixParam) override
    {
        intensity  = static_cast<double> (std::clamp (intensityParam, 0.0f, 1.0f));
        outputGain = static_cast<double> (std::clamp (outputGainParam, 0.0f, 1.0f));
        mix        = static_cast<double> (std::clamp (mixParam, 0.0f, 1.0f));
    }

    /** Traitement stéréo in-place, sample par sample. */
    void processStereo (float* leftChannel, float* rightChannel, int numSamples) override
    {
        // Recalculés une fois par bloc, comme dans l'original
        // (qui les recalcule une fois par appel à processReplacing).
        const double highGainOffset = std::pow (intensity, 2.0) * 0.023;
        const double threshold      = 1.001 - (1.0 - std::pow (1.0 - intensity, 5.0));
        const double muMakeupGain   = std::sqrt (1.0 / threshold);

        double release = (intensity * 100000.0) + 300000.0;
        release /= overallScale;
        const double fastest = std::sqrt (release);

        for (int i = 0; i < numSamples; ++i)
        {
            double inL = static_cast<double> (leftChannel[i]);
            double inR = static_cast<double> (rightChannel[i]);

            // Protection anti-denormal (bruit de dither très faible)
            if (std::fabs (inL) < 1.18e-23) inL = static_cast<double> (fpdL) * 1.18e-17;
            if (std::fabs (inR) < 1.18e-23) inR = static_cast<double> (fpdR) * 1.18e-17;

            const double dryL = inL;
            const double dryR = inR;

            // Ligne à retard partagée (même index pour L et R, comme l'original)
            delayBufferL[static_cast<size_t> (delayIndex)] = inL;
            delayBufferR[static_cast<size_t> (delayIndex)] = inR;
            delayIndex--;
            if (delayIndex < 0 || delayIndex > maxDelay) delayIndex = maxDelay;

            inL = (inL * left.thicken)  + (delayBufferL[static_cast<size_t> (delayIndex)] * (1.0 - left.thicken));
            inR = (inR * right.thicken) + (delayBufferR[static_cast<size_t> (delayIndex)] * (1.0 - right.thicken));

            const double lowestL = computeLowest (inL, left);
            const double lowestR = computeLowest (inR, right);

            inL *= muMakeupGain;
            const double punchL = std::max (0.65, 0.95 - std::fabs (inL * 0.08));

            inR *= muMakeupGain;
            const double punchR = std::max (0.65, 0.95 - std::fabs (inR * 0.08));

            updateCoefficients (lowestL, left,  threshold, release, fastest);
            updateCoefficients (lowestR, right, threshold, release, fastest);

            const double coefL = highGainOffset + std::pow (flip ? left.muCoefficientA  : left.muCoefficientB,  2.0);
            inL *= coefL;
            left.thicken = (coefL / 5.0) + punchL;
            left.thicken = (1.0 - mix) + (mix * left.thicken);

            const double coefR = highGainOffset + std::pow (flip ? right.muCoefficientA : right.muCoefficientB, 2.0);
            inR *= coefR;
            right.thicken = (coefR / 5.0) + punchR;
            right.thicken = (1.0 - mix) + (mix * right.thicken);

            inL = saturate (inL, coefL);
            inR = saturate (inR, coefR);

            flip = !flip;

            // Compensation de gain — sans ça, l'Intensity pilotait le VOLUME de
            // sortie autant que la dynamique (muMakeupGain monte jusqu'à +24dB à
            // Intensity élevé). On la retire ici, après usage pour le calcul de
            // la compression elle-même : Intensity ne change plus que le
            // caractère de la compression, jamais le niveau de sortie brut.
            inL /= muMakeupGain;
            inR /= muMakeupGain;

            if (outputGain < 1.0) { inL *= outputGain; inR *= outputGain; }
            if (mix < 1.0)
            {
                inL = (dryL * (1.0 - mix)) + (inL * mix);
                inR = (dryR * (1.0 - mix)) + (inR * mix);
            }

            // Dither TPDF (xorshift), comme l'original
            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;

            leftChannel[i]  = static_cast<float> (inL);
            rightChannel[i] = static_cast<float> (inR);
        }
    }

private:
    struct ChannelState
    {
        double muVaryA = 1.0, muVaryB = 1.0;
        double muAttackA = 1.0, muAttackB = 1.0;
        double muSpeedA = 10000.0, muSpeedB = 10000.0;
        double muCoefficientA = 1.0, muCoefficientB = 1.0;
        double thicken = 1.0;
        double previous1 = 0.0, previous2 = 0.0, previous3 = 0.0, previous4 = 0.0, previous5 = 0.0;
    };

    // Détecteur d'enveloppe biaisé vers le creux du signal sur une fenêtre
    // de 5 échantillons (courant + 4 précédents), avec moyenne pondérée
    // dégressive. Met aussi à jour l'historique previous1..5.
    static double computeLowest (double inputSample, ChannelState& s)
    {
        double lowest = inputSample;
        if (std::fabs (inputSample) > std::fabs (s.previous1)) lowest = s.previous1;
        if (std::fabs (lowest) > std::fabs (s.previous2)) lowest = (lowest + s.previous2) / 1.99;
        if (std::fabs (lowest) > std::fabs (s.previous3)) lowest = (lowest + s.previous3) / 1.98;
        if (std::fabs (lowest) > std::fabs (s.previous4)) lowest = (lowest + s.previous4) / 1.97;
        if (std::fabs (lowest) > std::fabs (s.previous5)) lowest = (lowest + s.previous5) / 1.96;

        s.previous5 = s.previous4;
        s.previous4 = s.previous3;
        s.previous3 = s.previous2;
        s.previous2 = s.previous1;
        s.previous1 = inputSample;

        return lowest;
    }

    // Coeur "vari-mu" : deux jeux de coefficients (A/B) alternés à chaque
    // échantillon via `flip` (partagé entre L et R). La vitesse
    // d'attaque/release n'est pas fixe : elle se recalcule à chaque
    // échantillon en fonction du niveau du signal (muSpeedA/B),
    // ce qui reproduit le comportement program-dependent d'un
    // compresseur à lampe.
    void updateCoefficients (double lowestSample, ChannelState& s,
                              double threshold, double release, double fastest)
    {
        double& muVary        = flip ? s.muVaryA        : s.muVaryB;
        double& muAttack      = flip ? s.muAttackA       : s.muAttackB;
        double& muSpeed       = flip ? s.muSpeedA        : s.muSpeedB;
        double& muCoefficient = flip ? s.muCoefficientA  : s.muCoefficientB;

        if (std::fabs (lowestSample) > threshold)
        {
            muVary = threshold / std::fabs (lowestSample);
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

        const double newSpeed = (muSpeed * (muSpeed - 1.0)) + std::fabs (lowestSample * release) + fastest;
        muSpeed = newSpeed / muSpeed;
    }

    // Saturation "Spiral" : waveshaper sinusoïdal mélangé en proportion
    // inverse de la quantité de compression appliquée (coefficient).
    static double saturate (double inputSample, double coefficient)
    {
        double bridge = std::fabs (inputSample);
        if (bridge > 1.2533141373155) bridge = 1.2533141373155;
        bridge = (bridge == 0.0) ? 0.0 : std::sin (bridge * bridge) / bridge;

        return inputSample > 0.0
            ? (inputSample * coefficient) + (bridge * (1.0 - coefficient))
            : (inputSample * coefficient) - (bridge * (1.0 - coefficient));
    }

    // --- État partagé entre les deux canaux ---
    double overallScale = 1.0;
    int maxDelay = 1450;
    int delayIndex = 0;
    bool flip = false;

    double intensity = 0.3, outputGain = 1.0, mix = 1.0;

    std::vector<double> delayBufferL, delayBufferR;
    ChannelState left, right;
    uint32_t fpdL = 1, fpdR = 1;
};
