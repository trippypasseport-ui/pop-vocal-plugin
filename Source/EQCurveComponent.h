#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/*
    EQCurveComponent
    ============================================================
    Affichage temps réel (Phase 1) de la réponse en fréquence du
    mode EQ "Normal" uniquement — le mode Pultec n'a pas encore
    de courbe dédiée (ses fréquences par crans + le comportement
    boost/atten simultané rendent le calcul différent ; à faire
    si besoin plus tard).

    Amplitude verticale réglable à la molette de la souris.
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

        struct P { const char* id; float defaultVal; };
        auto get = [this] (const char* id, float def) -> float
        {
            auto* p = apvts.getRawParameterValue (id);
            return p != nullptr ? p->load() : def;
        };

        const float lowCutFreq  = get ("eqLowCutFreq", 80.0f);
        const float lowGain     = get ("eqLow", 0.0f);
        const float lowFreq     = get ("eqLowFreq", 120.0f);
        const float lowMidGain  = get ("eqLowMid", 0.0f);
        const float lowMidFreq  = get ("eqLowMidFreq", 300.0f);
        const float midGain     = get ("eqMid", 0.0f);
        const float midFreq     = get ("eqMidFreq", 1000.0f);
        const float highMidGain = get ("eqHighMid", 0.0f);
        const float highMidFreq = get ("eqHighMidFreq", 3000.0f);
        const float highGain    = get ("eqHigh", 0.0f);
        const float highFreq    = get ("eqHighFreq", 8000.0f);
        const float highCutFreq = get ("eqHighCutFreq", 18000.0f);
        const float airAmount   = get ("eqAirAmount", 0.0f);

        const double sr = 48000.0;

        auto lowCutC   = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, lowCutFreq, 0.707f);
        auto lowC      = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, lowFreq, 0.707f, juce::Decibels::decibelsToGain (lowGain));
        auto lowMidC   = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, lowMidFreq, 1.0f, juce::Decibels::decibelsToGain (lowMidGain));
        auto midC      = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, midFreq, 0.9f, juce::Decibels::decibelsToGain (midGain));
        auto highMidC  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, highMidFreq, 1.0f, juce::Decibels::decibelsToGain (highMidGain));
        auto highC     = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, highFreq, 0.707f, juce::Decibels::decibelsToGain (highGain));
        auto airDipC   = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 7500.0f, 1.2f, juce::Decibels::decibelsToGain (-(airAmount * 0.4f)));
        auto airShelfC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 15000.0f, 0.707f, juce::Decibels::decibelsToGain (airAmount));
        auto highCutC  = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, highCutFreq, 0.707f);

        juce::Path curve;
        constexpr int numPoints = 128;
        constexpr float minFreq = 20.0f, maxFreq = 20000.0f;

        for (int i = 0; i < numPoints; ++i)
        {
            const float t = (float) i / (float) (numPoints - 1);
            const float freq = minFreq * std::pow (maxFreq / minFreq, t);

            double totalDb = 2.0 * juce::Decibels::gainToDecibels (lowCutC->getMagnitudeForFrequency ((double) freq, sr));
            totalDb += juce::Decibels::gainToDecibels (lowC->getMagnitudeForFrequency ((double) freq, sr));
            totalDb += juce::Decibels::gainToDecibels (lowMidC->getMagnitudeForFrequency ((double) freq, sr));
            totalDb += juce::Decibels::gainToDecibels (midC->getMagnitudeForFrequency ((double) freq, sr));
            totalDb += juce::Decibels::gainToDecibels (highMidC->getMagnitudeForFrequency ((double) freq, sr));
            totalDb += juce::Decibels::gainToDecibels (highC->getMagnitudeForFrequency ((double) freq, sr));
            totalDb += juce::Decibels::gainToDecibels (airDipC->getMagnitudeForFrequency ((double) freq, sr));
            totalDb += juce::Decibels::gainToDecibels (airShelfC->getMagnitudeForFrequency ((double) freq, sr));
            totalDb += 2.0 * juce::Decibels::gainToDecibels (highCutC->getMagnitudeForFrequency ((double) freq, sr));

            const float x = bounds.getX() + t * bounds.getWidth();
            const float normalised = juce::jlimit (-1.0f, 1.0f, (float) totalDb / dbRange);
            const float y = zeroY - normalised * (bounds.getHeight() * 0.45f);

            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }

        g.setColour (juce::Colour (0xffe8862b));
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (juce::Font (9.0f));
        g.drawText ("+/-" + juce::String ((int) dbRange) + "dB  (molette = zoom)",
                    bounds.reduced (4.0f), juce::Justification::topRight);
    }

private:
    void timerCallback() override { repaint(); }

    juce::AudioProcessorValueTreeState& apvts;
    float dbRange = 15.0f;
};
