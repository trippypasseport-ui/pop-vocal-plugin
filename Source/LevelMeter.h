#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

/*
    LevelMeter
    ============================================================
    Mètre de niveau vertical (peak, avec retombée douce) — lit un
    std::atomic<float> maintenu à jour par le processeur audio
    (donc thread-safe pour la lecture ; pas de verrou nécessaire).

    Se repaint automatiquement via un Timer pour suivre le niveau
    en continu pendant la lecture.
    ============================================================
*/

class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (const std::atomic<float>& levelDbRef)
        : levelDb (levelDbRef)
    {
        startTimerHz (30);
    }

    ~LevelMeter() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (juce::Colour (0xff1a1a1e));
        g.fillRoundedRectangle (bounds, 4.0f);

        const float db = juce::jlimit (-60.0f, 0.0f, levelDb.load());
        const float normalised = (db + 60.0f) / 60.0f; // 0..1
        auto inner = bounds.reduced (2.0f);
        const float barHeight = inner.getHeight() * normalised;

        auto barArea = inner.removeFromBottom (barHeight);
        juce::ColourGradient grad (juce::Colour (0xff4caf50), 0.0f, inner.getBottom(),
                                    juce::Colour (0xffe8862b), 0.0f, inner.getY(), false);
        grad.addColour (0.85, juce::Colour (0xffe83b2b)); // rouge proche du sommet (clip)
        g.setGradientFill (grad);
        g.fillRoundedRectangle (barArea, 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }

private:
    void timerCallback() override { repaint(); }

    const std::atomic<float>& levelDb;
};
