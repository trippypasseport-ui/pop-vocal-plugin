#pragma once
#include <array>

/*
    ICompressorAlgorithm
    ============================================================
    Interface commune aux algorithmes de compression sélectionnables
    dans la section Compressor (Doux / Agressif / Naturel). Chaque
    algorithme reçoit 3 paramètres génériques (0..1) dont le sens
    dépend de l'algorithme actif — getKnobLabels() donne le libellé
    à afficher pour chaque knob dans l'éditeur, pour que l'interface
    se relabel automatiquement au changement de mode.

    Volontairement sans dépendance JUCE (comme PopCompressor.h) :
    ces classes sont du DSP pur, testable et lisible indépendamment
    du framework de plugin.
    ============================================================
*/

class ICompressorAlgorithm
{
public:
    virtual ~ICompressorAlgorithm() = default;

    virtual void prepare (double sampleRate) = 0;
    virtual void reset() = 0;
    virtual void setParameters (float p1, float p2, float p3) = 0;
    virtual void processStereo (float* left, float* right, int numSamples) = 0;

    virtual const char* getName() const = 0;
    virtual std::array<const char*, 3> getKnobLabels() const = 0;
};
