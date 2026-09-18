#pragma once
#include <juce_dsp/juce_dsp.h>

/*
    ReverbModule
    ============================================================
    Fine couche de reverb en fin de chaîne, via juce::dsp::Reverb
    (algorithme de type Freeverb, intégré à JUCE — pas de DSP
    maison ici, JUCE le fournit déjà et il est tout à fait
    correct pour un usage interne).
    ============================================================
*/

class ReverbModule
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = (juce::uint32) maxBlockSize;
        spec.numChannels = 2;

        reverb.prepare (spec);
        reverb.reset();
    }

    void reset()
    {
        reverb.reset();
    }

    void setParameters (float mix01, float size01, float damping01)
    {
        auto params = reverb.getParameters();
        params.roomSize   = size01;
        params.damping    = damping01;
        params.wetLevel   = mix01;
        params.dryLevel   = 1.0f - mix01;
        params.width      = 1.0f;
        params.freezeMode = 0.0f;
        reverb.setParameters (params);
    }

    void processStereo (float* left, float* right, int numSamples)
    {
        float* channels[2] = { left, right };
        juce::dsp::AudioBlock<float> block (channels, 2, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> context (block);
        reverb.process (context);
    }

private:
    juce::dsp::Reverb reverb;
};
