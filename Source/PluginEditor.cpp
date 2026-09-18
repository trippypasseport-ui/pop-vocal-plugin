#include "PluginEditor.h"

namespace
{
    // Libellés des 3 knobs génériques du compresseur, par mode (même ordre
    // que les choix de compMode dans PluginProcessor::createParameterLayout)
    constexpr std::array<std::array<const char*, 3>, 3> compressorKnobLabels {{
        { "INTENSITY", "OUTPUT", "MIX" },   // Agressif (Pop)
        { "COMPRESS",  "OUTPUT", "MIX" },   // Doux (ButterComp)
        { "INTENSITY", "SPEED",  "MIX" }    // Naturel (VariMu)
    }};
}

void PopVocalAudioProcessorEditor::setupSection (Section& section, const juce::String& title,
                                                  std::array<juce::String, 3> knobNames)
{
    section.titleLabel.setText (title, juce::dontSendNotification);
    section.titleLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    section.titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8862b));
    section.titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (section.titleLabel);

    for (int i = 0; i < 3; ++i)
    {
        auto& knob = section.knobs[(size_t) i];

        knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 56, 16);
        addAndMakeVisible (knob.slider);

        knob.label.setText (knobNames[(size_t) i], juce::dontSendNotification);
        knob.label.setFont (juce::Font (10.5f, juce::Font::bold));
        knob.label.setColour (juce::Label::textColourId, juce::Colour (0xff8a8a92));
        knob.label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (knob.label);
    }
}

void PopVocalAudioProcessorEditor::layoutSection (juce::Rectangle<int> area, Section& section)
{
    area.reduce (10, 6);
    section.titleLabel.setBounds (area.removeFromTop (18));

    if (section.comboBox != nullptr)
        section.comboBox->setBounds (area.removeFromTop (22).reduced (2, 0));

    if (section.extraDisplay != nullptr && section.extraDisplayHeight > 0)
        section.extraDisplay->setBounds (area.removeFromTop (section.extraDisplayHeight).reduced (2, 2));

    const int knobWidth = area.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto cell = area.removeFromLeft (knobWidth);
        auto& knob = section.knobs[(size_t) i];
        knob.label.setBounds (cell.removeFromTop (14));
        knob.slider.setBounds (cell.reduced (4, 2));
    }
}

void PopVocalAudioProcessorEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto r = area.toFloat();
    g.setColour (juce::Colour (0xff232328));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
}

void PopVocalAudioProcessorEditor::updateCompressorLabels()
{
    const int index = juce::jlimit (0, 2, compModeBox.getSelectedId() - 1);
    const auto& labels = compressorKnobLabels[(size_t) index];

    for (int i = 0; i < 3; ++i)
        compressorSection.knobs[(size_t) i].label.setText (labels[(size_t) i], juce::dontSendNotification);
}

PopVocalAudioProcessorEditor::PopVocalAudioProcessorEditor (PopVocalAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), eqCurve (p.apvts)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("POP VOCAL", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe4e2df));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("RES.BROAD -> COMP -> RES.PRECISE -> EQ -> DELAY -> REVERB", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (9.5f, juce::Font::plain));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a8a92));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    setupSection (resBroadSection,   "DE-RES (BROAD)",   { "SENSITIVITY", "DEPTH",  "MIX" });
    setupSection (compressorSection, "COMPRESSOR",       { "INTENSITY",   "OUTPUT", "MIX" });
    setupSection (resPreciseSection, "DE-RES (PRECISE)", { "SENSITIVITY", "DEPTH",  "MIX" });
    setupSection (eqSection,         "EQ",               { "LOW",         "MID",    "HIGH" });
    setupSection (delaySection,      "DELAY",            { "TIME",        "FEEDBACK", "MIX" });
    setupSection (reverbSection,     "REVERB",           { "MIX",         "SIZE",   "DAMPING" });

    // Sélecteur d'algorithme du compresseur
    compModeBox.addItem ("Agressif (Pop)",    1);
    compModeBox.addItem ("Doux (ButterComp)", 2);
    compModeBox.addItem ("Naturel (VariMu)",  3);
    addAndMakeVisible (compModeBox);
    compressorSection.comboBox = &compModeBox;
    compModeBox.onChange = [this] { updateCompressorLabels(); };

    // Courbe EQ temps réel
    addAndMakeVisible (eqCurve);
    eqSection.extraDisplay = &eqCurve;
    eqSection.extraDisplayHeight = 56;

    auto& apvts = processorRef.apvts;
    auto attach = [&] (juce::Slider& slider, const juce::String& paramId)
    {
        attachments.push_back (std::make_unique<SliderAttachment> (apvts, paramId, slider));
    };

    attach (resBroadSection.knobs[0].slider, "resBroadSensitivity");
    attach (resBroadSection.knobs[1].slider, "resBroadDepth");
    attach (resBroadSection.knobs[2].slider, "resBroadMix");

    attach (compressorSection.knobs[0].slider, "compP1");
    attach (compressorSection.knobs[1].slider, "compP2");
    attach (compressorSection.knobs[2].slider, "compP3");

    attach (resPreciseSection.knobs[0].slider, "resPreciseSensitivity");
    attach (resPreciseSection.knobs[1].slider, "resPreciseDepth");
    attach (resPreciseSection.knobs[2].slider, "resPreciseMix");

    attach (eqSection.knobs[0].slider, "eqLow");
    attach (eqSection.knobs[1].slider, "eqMid");
    attach (eqSection.knobs[2].slider, "eqHigh");

    attach (delaySection.knobs[0].slider, "delayTime");
    attach (delaySection.knobs[1].slider, "delayFeedback");
    attach (delaySection.knobs[2].slider, "delayMix");

    attach (reverbSection.knobs[0].slider, "reverbMix");
    attach (reverbSection.knobs[1].slider, "reverbSize");
    attach (reverbSection.knobs[2].slider, "reverbDamping");

    compModeAttachment = std::make_unique<ComboBoxAttachment> (apvts, "compMode", compModeBox);
    updateCompressorLabels();

    setSize (960, 620);
}

PopVocalAudioProcessorEditor::~PopVocalAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void PopVocalAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient bg (juce::Colour (0xff1c1c22), bounds.getX(), bounds.getY(),
                              juce::Colour (0xff121216), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();

    g.setColour (juce::Colour (0xffe8862b).withAlpha (0.6f));
    g.fillRect (24.0f, 66.0f, (float) getWidth() - 48.0f, 1.5f);

    auto content = getLocalBounds().reduced (24).withTop (86);
    const int rowHeight = content.getHeight() / 2 - 8;
    const int colWidth  = (content.getWidth() - 2 * 16) / 3;

    for (int row = 0; row < 2; ++row)
    {
        int x = content.getX();
        for (int col = 0; col < 3; ++col)
        {
            drawPanel (g, { x, content.getY() + row * (rowHeight + 16), colWidth, rowHeight });
            x += colWidth + 16;
        }
    }
}

void PopVocalAudioProcessorEditor::resized()
{
    titleLabel.setBounds (24, 14, 500, 30);
    subtitleLabel.setBounds (24, 44, 600, 16);

    auto content = getLocalBounds().reduced (24).withTop (86);
    const int rowHeight = content.getHeight() / 2 - 8;
    const int colWidth  = (content.getWidth() - 2 * 16) / 3;

    int x = content.getX();
    layoutSection ({ x, content.getY(), colWidth, rowHeight }, resBroadSection);
    x += colWidth + 16;
    layoutSection ({ x, content.getY(), colWidth, rowHeight }, compressorSection);
    x += colWidth + 16;
    layoutSection ({ x, content.getY(), colWidth, rowHeight }, resPreciseSection);

    x = content.getX();
    layoutSection ({ x, content.getY() + rowHeight + 16, colWidth, rowHeight }, eqSection);
    x += colWidth + 16;
    layoutSection ({ x, content.getY() + rowHeight + 16, colWidth, rowHeight }, delaySection);
    x += colWidth + 16;
    layoutSection ({ x, content.getY() + rowHeight + 16, colWidth, rowHeight }, reverbSection);
}
