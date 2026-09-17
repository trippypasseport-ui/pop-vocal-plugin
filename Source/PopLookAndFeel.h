#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

/*
    PopLookAndFeel
    ============================================================
    Palette sombre + accent ambre, knobs dessinés à la main
    (arc de piste, arc de valeur en dégradé, corps du knob en
    dégradé radial, pointeur) plutôt que les sliders JUCE par
    défaut. Header-only, s'utilise via setLookAndFeel(&laf) dans
    l'éditeur.
*/

class PopLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PopLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, backgroundDark);
        setColour (juce::Slider::textBoxTextColourId, textLight);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.3f));
        setColour (juce::Label::textColourId, textLight);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                            juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centre = bounds.getCentre();
        auto angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        const float trackThickness = radius * 0.18f;
        const float knobRadius     = radius - trackThickness * 1.6f;
        const float arcRadius      = radius - trackThickness / 2.0f;

        // Piste de fond (arc complet, discret)
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                              0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (trackDark);
        g.strokePath (track, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

        // Arc de valeur (accent)
        if (sliderPos > 0.001f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                                     0.0f, rotaryStartAngle, angle, true);
            g.setColour (accent);
            g.strokePath (valueArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
        }

        // Corps du knob : dégradé radial pour un effet légèrement métallique
        juce::ColourGradient knobGradient (knobLight, centre.x - knobRadius * 0.4f, centre.y - knobRadius * 0.5f,
                                            knobDark,  centre.x + knobRadius * 0.6f, centre.y + knobRadius * 0.7f, false);
        g.setGradientFill (knobGradient);
        g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.2f);

        // Pointeur
        juce::Path pointer;
        const float pointerLength    = knobRadius * 0.72f;
        const float pointerThickness = 2.6f;
        pointer.addRoundedRectangle (-pointerThickness * 0.5f, -knobRadius + 4.0f,
                                      pointerThickness, pointerLength, pointerThickness * 0.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
        g.setColour (accent.brighter (0.6f));
        g.fillPath (pointer);
    }

private:
    juce::Colour backgroundDark { 0xff17171c };
    juce::Colour trackDark      { 0xff2c2c34 };
    juce::Colour accent         { 0xffe8862b };
    juce::Colour knobLight      { 0xff3d3d47 };
    juce::Colour knobDark       { 0xff232329 };
    juce::Colour textLight      { 0xffe4e2df };
};
