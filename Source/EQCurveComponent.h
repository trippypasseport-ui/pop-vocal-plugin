#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/*
    EQCurveComponent
    ============================================================
    Affichage temps réel (Phase 1 — pas encore éditable à la
    souris pour bouger les fréquences, ça viendra en Phase 2 si
    besoin) de la réponse en fréquence de la section EQ.

    Amplitude verticale (dbRange) réglable à la molette de la
    souris directement sur la courbe — zoom in/out sur l'échelle
    en dB, utile pour voir les coupe-bas/coupe-haut qui peuvent
    dépasser largement +/-15dB avec leur pente à 24dB/oct.

    Se repaint automatiquement via un Timer pour suivre les
    changements de paramètres, y compris ceux venant de
    l'automation de l'hôte.
    ============================================================
*/

class EQCurveComponent : public juce::Component, private juce::Timer
{
public:
    explicit EQCurveComponent (juce::AudioProcessorValueTreeState& state)
        : apvts (state)
    {
        startTimerHz (30);
    }

    ~EQCurveComponent() override { stopTimer(); }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        // Molette vers le haut = zoom in (plage plus petite, courbe plus ample visuellement)
        const float step = 3.0f;
        dbRange = juce::jlimit (6.0f, 48.0f, dbRange - wheel.deltaY * step * 4.0f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (juce::Colour (0xff1a1a1e));
        g.fillRoundedRectangle (bounds, 6.0f);

        const float zeroY = bounds.getY() + bounds.getHeight() * 0.5f;
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (bounds.getX(), zeroY, bounds.getRight(), zeroY, 1.0f);

        auto* lowCutParam   = apvts.getRawParameterValue ("eqLowCutFreq");
        auto* lowParam      = apvts.getRawParameterValue ("eqLow");
        auto* lowFreqParam  = apvts.getRawParameterValue ("eqLowFreq");
        auto* midParam      = apvts.getRawParameterValue ("eqMid");
        auto* midFreqParam  = apvts.getRawParameterValue ("eqMidFreq");
        auto* highParam     = apvts.getRawParameterValue ("eqHigh");
        auto* highFreqParam = apvts.getRawParameterValue ("eqHighFreq");
        auto* highCutParam  = apvts.getRawParameterValue ("eqHighCutFreq");
        auto* airParam      = apvts.getRawParameterValue ("eqAirAmount");
        if (lowParam == nullptr || midParam == nullptr || highParam == nullptr
            || lowFreqParam == nullptr || midFreqParam == nullptr || highFreqParam == nullptr
            || lowCutParam == nullptr || highCutParam == nullptr || airParam == nullptr)
            return;

        const float lowCutFreq  = lowCutParam->load();
        const float lowGain  = lowParam->load();
        const float midGain  = midParam->load();
        const float highGain = highParam->load();
        const float lowFreq  = lowFreqParam->load();
        const float midFreq  = midFreqParam->load();
        const float highFreq = highFreqParam->load();
        const float highCutFreq = highCutParam->load();
        const float airAmount = airParam->load();

        // Sample rate de référence pour le tracé — la réponse d'un shelf/peak
        // en dB ne dépend quasiment pas du sample rate choisi ici tant qu'il
        // est raisonnable ; ça évite de dépendre du sample rate réel du host
        // pour un simple affichage.
        const double displaySampleRate = 48000.0;

        auto lowCutCoeffs  = juce::dsp::IIR::Coefficients<float>::makeHighPass (displaySampleRate, lowCutFreq, 0.707f);
        auto lowCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            displaySampleRate, lowFreq, 0.707f, juce::Decibels::decibelsToGain (lowGain));
        auto midCoeffs  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            displaySampleRate, midFreq, 0.9f, juce::Decibels::decibelsToGain (midGain));
        auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            displaySampleRate, highFreq, 0.707f, juce::Decibels::decibelsToGain (highGain));
        auto highCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (displaySampleRate, highCutFreq, 0.707f);
        auto airDipCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (displaySampleRate, 7500.0f, 1.2f,
                                                                                  juce::Decibels::decibelsToGain (-(airAmount * 0.4f)));
        auto airCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (displaySampleRate, 15000.0f, 0.707f,
                                                                               juce::Decibels::decibelsToGain (airAmount));

        juce::Path curve;
        constexpr int numPoints = 128;
        constexpr float minFreq = 20.0f, maxFreq = 20000.0f;

        for (int i = 0; i < numPoints; ++i)
        {
            const float t = (float) i / (float) (numPoints - 1);
            const float freq = minFreq * std::pow (maxFreq / minFreq, t);

            const double magLowCut  = lowCutCoeffs->getMagnitudeForFrequency  ((double) freq, displaySampleRate);
            const double magLow  = lowCoeffs->getMagnitudeForFrequency  ((double) freq, displaySampleRate);
            const double magMid  = midCoeffs->getMagnitudeForFrequency  ((double) freq, displaySampleRate);
            const double magHigh = highCoeffs->getMagnitudeForFrequency ((double) freq, displaySampleRate);
            const double magHighCut = highCutCoeffs->getMagnitudeForFrequency ((double) freq, displaySampleRate);
            const double magAir = airCoeffs->getMagnitudeForFrequency ((double) freq, displaySampleRate);
            const double magAirDip = airDipCoeffs->getMagnitudeForFrequency ((double) freq, displaySampleRate);

            const double totalDb = 2.0 * juce::Decibels::gainToDecibels (magLowCut)   // 2 étages en cascade
                                  + juce::Decibels::gainToDecibels (magLow)
                                  + juce::Decibels::gainToDecibels (magMid)
                                  + juce::Decibels::gainToDecibels (magHigh)
                                  + juce::Decibels::gainToDecibels (magAir)
                                  + juce::Decibels::gainToDecibels (magAirDip)
                                  + 2.0 * juce::Decibels::gainToDecibels (magHighCut); // 2 étages en cascade

            const float x = bounds.getX() + t * bounds.getWidth();
            const float normalised = juce::jlimit (-1.0f, 1.0f, (float) totalDb / dbRange);
            const float y = zeroY - normalised * (bounds.getHeight() * 0.45f);

            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }

        g.setColour (juce::Colour (0xffe8862b));
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        // Indication de l'échelle actuelle (coin, petit texte discret)
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (juce::Font (9.0f));
        g.drawText ("+/-" + juce::String ((int) dbRange) + "dB  (molette = zoom)",
                    bounds.reduced (4.0f), juce::Justification::topRight);
    }

private:
    void timerCallback() override { repaint(); }

    juce::AudioProcessorValueTreeState& apvts;
    float dbRange = 15.0f; // amplitude verticale affichée, ajustable à la molette
};
