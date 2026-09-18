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

PopVocalAudioProcessorEditor::Knob& PopVocalAudioProcessorEditor::addKnob (Section& section, const juce::String& name)
{
    section.knobs.push_back (std::make_unique<Knob>());
    auto& knob = *section.knobs.back();

    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 52, 16);
    addAndMakeVisible (knob.slider);

    knob.label.setText (name, juce::dontSendNotification);
    knob.label.setFont (juce::Font (10.0f, juce::Font::bold));
    knob.label.setColour (juce::Label::textColourId, juce::Colour (0xff8a8a92));
    knob.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (knob.label);

    return knob;
}

void PopVocalAudioProcessorEditor::finishSectionSetup (Section& section, const juce::String& title)
{
    section.titleLabel.setText (title, juce::dontSendNotification);
    section.titleLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    section.titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8862b));
    section.titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (section.titleLabel);
}

void PopVocalAudioProcessorEditor::layoutSection (juce::Rectangle<int> area, Section& section)
{
    area.reduce (10, 6);
    section.titleLabel.setBounds (area.removeFromTop (18));

    if (section.comboBox != nullptr)
        section.comboBox->setBounds (area.removeFromTop (22).reduced (2, 0));

    if (section.toggleButton != nullptr)
        section.toggleButton->setBounds (area.removeFromTop (20).reduced (2, 0));

    if (section.extraDisplay != nullptr && section.extraDisplayHeight > 0)
        section.extraDisplay->setBounds (area.removeFromTop (section.extraDisplayHeight).reduced (2, 2));

    const int columns = juce::jmax (1, section.knobColumns);
    const int totalKnobs = (int) section.knobs.size();
    if (totalKnobs == 0)
        return;
    const int rows = (totalKnobs + columns - 1) / columns;
    const int rowHeight = area.getHeight() / rows;

    for (int r = 0; r < rows; ++r)
    {
        auto rowArea = area.removeFromTop (rowHeight);
        const int knobsInRow = juce::jmin (columns, totalKnobs - r * columns);
        const int knobWidth = rowArea.getWidth() / juce::jmax (1, knobsInRow);

        for (int c = 0; c < knobsInRow; ++c)
        {
            auto cell = rowArea.removeFromLeft (knobWidth);
            auto& knob = *section.knobs[(size_t) (r * columns + c)];
            knob.label.setBounds (cell.removeFromTop (14));
            knob.slider.setBounds (cell.reduced (4, 2));
        }
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
        compressorSection.knobs[(size_t) i]->label.setText (labels[(size_t) i], juce::dontSendNotification);
}

void PopVocalAudioProcessorEditor::applyPreset (const std::vector<std::pair<juce::String, float>>& values)
{
    auto& apvts = processorRef.apvts;
    for (auto& entry : values)
    {
        if (auto* param = apvts.getParameter (entry.first))
        {
            auto range = apvts.getParameterRange (entry.first);
            param->setValueNotifyingHost (range.convertTo0to1 (entry.second));
        }
    }
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

    auto& apvts = processorRef.apvts;
    auto attach = [&] (juce::Slider& slider, const juce::String& paramId)
    {
        attachments.push_back (std::make_unique<SliderAttachment> (apvts, paramId, slider));
    };

    // ================= DE-RES BROAD =================
    finishSectionSetup (resBroadSection, "DE-RES (BROAD)");
    attach (addKnob (resBroadSection, "SENSITIVITY").slider, "resBroadSensitivity");
    attach (addKnob (resBroadSection, "DEPTH").slider,       "resBroadDepth");
    attach (addKnob (resBroadSection, "MIX").slider,         "resBroadMix");

    resBroadPresetBox.addItem ("Neutre", 1);
    resBroadPresetBox.addItem ("Leger", 2);
    resBroadPresetBox.addItem ("Modere", 3);
    resBroadPresetBox.addItem ("Agressif", 4);
    addAndMakeVisible (resBroadPresetBox);
    resBroadSection.comboBox = &resBroadPresetBox;
    {
        const std::vector<std::pair<juce::String, float>> neutre { {"resBroadSensitivity",0.35f}, {"resBroadDepth",0.4f},  {"resBroadMix",1.0f} };
        const std::vector<std::pair<juce::String, float>> leger  { {"resBroadSensitivity",0.2f},  {"resBroadDepth",0.25f}, {"resBroadMix",0.8f} };
        const std::vector<std::pair<juce::String, float>> modere { {"resBroadSensitivity",0.4f},  {"resBroadDepth",0.5f},  {"resBroadMix",1.0f} };
        const std::vector<std::pair<juce::String, float>> aggro  { {"resBroadSensitivity",0.6f},  {"resBroadDepth",0.8f},  {"resBroadMix",1.0f} };
        resBroadPresetBox.onChange = [this, neutre, leger, modere, aggro]
        {
            switch (resBroadPresetBox.getSelectedId())
            {
                case 1: applyPreset (neutre); break;
                case 2: applyPreset (leger);  break;
                case 3: applyPreset (modere); break;
                case 4: applyPreset (aggro);  break;
                default: break;
            }
        };
    }

    // ================= COMPRESSOR =================
    finishSectionSetup (compressorSection, "COMPRESSOR");
    attach (addKnob (compressorSection, "INTENSITY").slider, "compP1");
    attach (addKnob (compressorSection, "OUTPUT").slider,    "compP2");
    attach (addKnob (compressorSection, "MIX").slider,       "compP3");

    compModeBox.addItem ("Agressif (Pop)",    1);
    compModeBox.addItem ("Doux (ButterComp)", 2);
    compModeBox.addItem ("Naturel (VariMu)",  3);
    addAndMakeVisible (compModeBox);
    compressorSection.comboBox = &compModeBox;
    compModeBox.onChange = [this] { updateCompressorLabels(); };

    // ================= DE-RES PRECISE =================
    finishSectionSetup (resPreciseSection, "DE-RES (PRECISE)");
    attach (addKnob (resPreciseSection, "SENSITIVITY").slider, "resPreciseSensitivity");
    attach (addKnob (resPreciseSection, "DEPTH").slider,       "resPreciseDepth");
    attach (addKnob (resPreciseSection, "MIX").slider,         "resPreciseMix");

    resPrecisePresetBox.addItem ("Neutre", 1);
    resPrecisePresetBox.addItem ("Leger", 2);
    resPrecisePresetBox.addItem ("Chirurgical", 3);
    resPrecisePresetBox.addItem ("Max", 4);
    addAndMakeVisible (resPrecisePresetBox);
    resPreciseSection.comboBox = &resPrecisePresetBox;
    {
        const std::vector<std::pair<juce::String, float>> neutre      { {"resPreciseSensitivity",0.5f},  {"resPreciseDepth",0.3f}, {"resPreciseMix",1.0f} };
        const std::vector<std::pair<juce::String, float>> leger       { {"resPreciseSensitivity",0.3f},  {"resPreciseDepth",0.15f},{"resPreciseMix",0.7f} };
        const std::vector<std::pair<juce::String, float>> chirurgical { {"resPreciseSensitivity",0.65f}, {"resPreciseDepth",0.5f}, {"resPreciseMix",1.0f} };
        const std::vector<std::pair<juce::String, float>> maxi        { {"resPreciseSensitivity",0.8f},  {"resPreciseDepth",0.9f}, {"resPreciseMix",1.0f} };
        resPrecisePresetBox.onChange = [this, neutre, leger, chirurgical, maxi]
        {
            switch (resPrecisePresetBox.getSelectedId())
            {
                case 1: applyPreset (neutre);      break;
                case 2: applyPreset (leger);       break;
                case 3: applyPreset (chirurgical); break;
                case 4: applyPreset (maxi);        break;
                default: break;
            }
        };
    }

    // ================= EQ =================
    finishSectionSetup (eqSection, "EQ");
    eqSection.knobColumns = 3; // 6 knobs -> 2 rangees de 3
    attach (addKnob (eqSection, "LOW").slider,       "eqLow");
    attach (addKnob (eqSection, "MID").slider,       "eqMid");
    attach (addKnob (eqSection, "HIGH").slider,      "eqHigh");
    attach (addKnob (eqSection, "LOW FREQ").slider,  "eqLowFreq");
    attach (addKnob (eqSection, "MID FREQ").slider,  "eqMidFreq");
    attach (addKnob (eqSection, "HIGH FREQ").slider, "eqHighFreq");

    addAndMakeVisible (eqCurve);
    eqSection.extraDisplay = &eqCurve;
    eqSection.extraDisplayHeight = 40;

    eqPresetBox.addItem ("Neutre", 1);
    eqPresetBox.addItem ("Chaleureux", 2);
    eqPresetBox.addItem ("Brillant", 3);
    eqPresetBox.addItem ("Presence Radio", 4);
    addAndMakeVisible (eqPresetBox);
    eqSection.comboBox = &eqPresetBox;
    {
        const std::vector<std::pair<juce::String, float>> neutre     { {"eqLow",0.0f},{"eqLowFreq",120.0f},{"eqMid",0.0f},{"eqMidFreq",1000.0f},{"eqHigh",0.0f},{"eqHighFreq",8000.0f} };
        const std::vector<std::pair<juce::String, float>> chaleureux { {"eqLow",3.0f},{"eqLowFreq",120.0f},{"eqMid",-1.0f},{"eqMidFreq",1000.0f},{"eqHigh",-2.0f},{"eqHighFreq",8000.0f} };
        const std::vector<std::pair<juce::String, float>> brillant   { {"eqLow",-1.0f},{"eqLowFreq",120.0f},{"eqMid",0.0f},{"eqMidFreq",1000.0f},{"eqHigh",3.0f},{"eqHighFreq",8000.0f} };
        const std::vector<std::pair<juce::String, float>> presence   { {"eqLow",-2.0f},{"eqLowFreq",100.0f},{"eqMid",3.0f},{"eqMidFreq",2500.0f},{"eqHigh",1.0f},{"eqHighFreq",9000.0f} };
        eqPresetBox.onChange = [this, neutre, chaleureux, brillant, presence]
        {
            switch (eqPresetBox.getSelectedId())
            {
                case 1: applyPreset (neutre);     break;
                case 2: applyPreset (chaleureux); break;
                case 3: applyPreset (brillant);   break;
                case 4: applyPreset (presence);   break;
                default: break;
            }
        };
    }

    // ================= DELAY =================
    finishSectionSetup (delaySection, "DELAY");
    attach (addKnob (delaySection, "TIME (FREE)").slider, "delayTime");
    attach (addKnob (delaySection, "FEEDBACK").slider,    "delayFeedback");
    attach (addKnob (delaySection, "MIX").slider,         "delayMix");

    delayRateBox.addItem ("Free", 1);
    delayRateBox.addItem ("1/2", 2);
    delayRateBox.addItem ("1/4", 3);
    delayRateBox.addItem ("1/8", 4);
    addAndMakeVisible (delayRateBox);
    delaySection.comboBox = &delayRateBox;
    delayRateAttachment = std::make_unique<ComboBoxAttachment> (apvts, "delayRateMode", delayRateBox);

    addAndMakeVisible (delayPingPongToggle);
    delaySection.toggleButton = &delayPingPongToggle;
    delayPingPongAttachment = std::make_unique<ButtonAttachment> (apvts, "delayPingPong", delayPingPongToggle);

    // ================= REVERB =================
    finishSectionSetup (reverbSection, "REVERB");
    attach (addKnob (reverbSection, "MIX").slider,     "reverbMix");
    attach (addKnob (reverbSection, "SIZE").slider,    "reverbSize");
    attach (addKnob (reverbSection, "DAMPING").slider, "reverbDamping");

    reverbPresetBox.addItem ("Off", 1);
    reverbPresetBox.addItem ("Room", 2);
    reverbPresetBox.addItem ("Hall", 3);
    reverbPresetBox.addItem ("Plate", 4);
    addAndMakeVisible (reverbPresetBox);
    reverbSection.comboBox = &reverbPresetBox;
    {
        const std::vector<std::pair<juce::String, float>> off  { {"reverbMix",0.0f},  {"reverbSize",0.3f}, {"reverbDamping",0.5f} };
        const std::vector<std::pair<juce::String, float>> room { {"reverbMix",0.15f}, {"reverbSize",0.3f}, {"reverbDamping",0.6f} };
        const std::vector<std::pair<juce::String, float>> hall { {"reverbMix",0.25f}, {"reverbSize",0.75f},{"reverbDamping",0.4f} };
        const std::vector<std::pair<juce::String, float>> plate{ {"reverbMix",0.2f},  {"reverbSize",0.5f}, {"reverbDamping",0.2f} };
        reverbPresetBox.onChange = [this, off, room, hall, plate]
        {
            switch (reverbPresetBox.getSelectedId())
            {
                case 1: applyPreset (off);  break;
                case 2: applyPreset (room); break;
                case 3: applyPreset (hall); break;
                case 4: applyPreset (plate);break;
                default: break;
            }
        };
    }

    compModeAttachment = std::make_unique<ComboBoxAttachment> (apvts, "compMode", compModeBox);
    updateCompressorLabels();

    setSize (1040, 680);
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
