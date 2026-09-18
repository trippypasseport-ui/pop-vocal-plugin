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

    auto titleRow = area.removeFromTop (18);
    if (section.activeToggle != nullptr)
        section.activeToggle->setBounds (titleRow.removeFromRight (46));
    section.titleLabel.setBounds (titleRow);

    juce::Rectangle<int> comboRow;
    const bool hasCombo = (section.comboBox != nullptr);
    if (hasCombo)
        comboRow = area.removeFromTop (22);

    if (section.toggleButton != nullptr)
        section.toggleButton->setBounds (area.removeFromTop (20).reduced (2, 0));

    if (section.extraDisplay != nullptr && section.extraDisplayHeight > 0)
        section.extraDisplay->setBounds (area.removeFromTop (section.extraDisplayHeight).reduced (2, 2));

    const int columns = juce::jmax (1, section.knobColumns);
    const int totalKnobs = (int) section.knobs.size();
    if (totalKnobs == 0)
    {
        if (hasCombo)
            section.comboBox->setBounds (comboRow.reduced (2, 0));
        return;
    }

    const int rows = (totalKnobs + columns - 1) / columns;
    const int rowHeight = area.getHeight() / rows;
    const int firstRowKnobs = juce::jmin (columns, totalKnobs);
    const int firstRowKnobWidth = area.getWidth() / juce::jmax (1, firstRowKnobs);

    if (hasCombo)
    {
        if (section.comboBoxKnobIndex >= 0 && section.comboBoxKnobIndex < firstRowKnobs)
        {
            auto cell = comboRow.withX (comboRow.getX() + section.comboBoxKnobIndex * firstRowKnobWidth)
                                 .withWidth (firstRowKnobWidth);
            section.comboBox->setBounds (cell.reduced (2, 0));
        }
        else
        {
            section.comboBox->setBounds (comboRow.reduced (2, 0));
        }
    }

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
    : AudioProcessorEditor (&p), processorRef (p), eqCurve (p.apvts),
      inputMeter (p.getInputLevelDb()), outputMeter (p.getOutputLevelDb())
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
    addAndMakeVisible (resBroadActiveToggle);
    resBroadSection.activeToggle = &resBroadActiveToggle;
    resBroadActiveAttachment = std::make_unique<ButtonAttachment> (apvts, "resBroadActive", resBroadActiveToggle);
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
    addAndMakeVisible (compressorActiveToggle);
    compressorSection.activeToggle = &compressorActiveToggle;
    compressorActiveAttachment = std::make_unique<ButtonAttachment> (apvts, "compressorActive", compressorActiveToggle);
    attach (addKnob (compressorSection, "INTENSITY").slider, "compP1");
    attach (addKnob (compressorSection, "OUTPUT").slider,    "compP2");
    attach (addKnob (compressorSection, "MIX").slider,       "compP3");

    compModeBox.addItem ("Agressif (Pop)",    1);
    compModeBox.addItem ("Doux (ButterComp)", 2);
    compModeBox.addItem ("Naturel (VariMu)",  3);
    addAndMakeVisible (compModeBox);
    compressorSection.comboBox = &compModeBox;

    // L'attachment DOIT être créé avant qu'on personnalise onChange : son
    // constructeur assigne son propre onChange en interne, donc le créer
    // après (comme précédemment) écrasait silencieusement notre callback —
    // c'était le vrai bug : ni le relabel ni l'application des valeurs par
    // défaut ne se déclenchaient jamais.
    compModeAttachment = std::make_unique<ComboBoxAttachment> (apvts, "compMode", compModeBox);
    {
        auto attachmentOnChange = compModeBox.onChange; // celui que l'attachment vient de poser

        const std::vector<std::pair<juce::String, float>> agressif { {"compP1",0.3f}, {"compP2",1.0f}, {"compP3",1.0f} };
        const std::vector<std::pair<juce::String, float>> doux     { {"compP1",0.3f}, {"compP2",0.5f}, {"compP3",1.0f} };
        const std::vector<std::pair<juce::String, float>> naturel  { {"compP1",0.3f}, {"compP2",0.5f}, {"compP3",1.0f} };

        compModeBox.onChange = [this, attachmentOnChange, agressif, doux, naturel]
        {
            if (attachmentOnChange)
                attachmentOnChange(); // préserve la synchro du paramètre compMode

            updateCompressorLabels();
            switch (compModeBox.getSelectedId())
            {
                case 1: applyPreset (agressif); break;
                case 2: applyPreset (doux);     break;
                case 3: applyPreset (naturel);  break;
                default: break;
            }
        };
    }

    // ================= DE-RES PRECISE =================
    finishSectionSetup (resPreciseSection, "DE-RES (PRECISE)");
    addAndMakeVisible (resPreciseActiveToggle);
    resPreciseSection.activeToggle = &resPreciseActiveToggle;
    resPreciseActiveAttachment = std::make_unique<ButtonAttachment> (apvts, "resPreciseActive", resPreciseActiveToggle);
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
    addAndMakeVisible (eqActiveToggle);
    eqSection.activeToggle = &eqActiveToggle;
    eqActiveAttachment = std::make_unique<ButtonAttachment> (apvts, "eqActive", eqActiveToggle);
    eqSection.knobColumns = 3; // 8 knobs -> 3 rangees (3+3+2)
    attach (addKnob (eqSection, "LOW").slider,       "eqLow");
    attach (addKnob (eqSection, "MID").slider,       "eqMid");
    attach (addKnob (eqSection, "HIGH").slider,      "eqHigh");
    attach (addKnob (eqSection, "LOW FREQ").slider,  "eqLowFreq");
    attach (addKnob (eqSection, "MID FREQ").slider,  "eqMidFreq");
    attach (addKnob (eqSection, "HIGH FREQ").slider, "eqHighFreq");
    attach (addKnob (eqSection, "LOW CUT").slider,   "eqLowCutFreq");
    attach (addKnob (eqSection, "HIGH CUT").slider,  "eqHighCutFreq");

    addAndMakeVisible (eqCurve);
    eqSection.extraDisplay = &eqCurve;
    eqSection.extraDisplayHeight = 40;

    eqPresetBox.addItem ("Neutre", 1);
    eqPresetBox.addItem ("Chaleureux", 2);
    eqPresetBox.addItem ("Brillant", 3);
    eqPresetBox.addItem ("Presence Radio", 4);
    eqPresetBox.addItem ("Air", 5);
    addAndMakeVisible (eqPresetBox);
    eqSection.comboBox = &eqPresetBox;
    {
        const std::vector<std::pair<juce::String, float>> neutre     { {"eqLowCutFreq",80.0f}, {"eqLow",0.0f},{"eqLowFreq",120.0f},{"eqMid",0.0f},{"eqMidFreq",1000.0f},{"eqHigh",0.0f},{"eqHighFreq",8000.0f}, {"eqHighCutFreq",18000.0f} };
        const std::vector<std::pair<juce::String, float>> chaleureux { {"eqLowCutFreq",80.0f}, {"eqLow",3.0f},{"eqLowFreq",120.0f},{"eqMid",-1.0f},{"eqMidFreq",1000.0f},{"eqHigh",-2.0f},{"eqHighFreq",8000.0f}, {"eqHighCutFreq",16000.0f} };
        const std::vector<std::pair<juce::String, float>> brillant   { {"eqLowCutFreq",100.0f},{"eqLow",-1.0f},{"eqLowFreq",120.0f},{"eqMid",0.0f},{"eqMidFreq",1000.0f},{"eqHigh",3.0f},{"eqHighFreq",8000.0f}, {"eqHighCutFreq",18000.0f} };
        const std::vector<std::pair<juce::String, float>> presence   { {"eqLowCutFreq",150.0f},{"eqLow",-2.0f},{"eqLowFreq",100.0f},{"eqMid",3.0f},{"eqMidFreq",2500.0f},{"eqHigh",1.0f},{"eqHighFreq",9000.0f}, {"eqHighCutFreq",10000.0f} };
        // Air : léger creux dans le bas-médium pour désencombrer, shelf haut très étiré
        // et généreux (~13kHz) pour donner de l'ouverture sans devenir sifflant
        const std::vector<std::pair<juce::String, float>> air        { {"eqLowCutFreq",90.0f}, {"eqLow",0.0f},{"eqLowFreq",120.0f},{"eqMid",-1.5f},{"eqMidFreq",400.0f},{"eqHigh",4.5f},{"eqHighFreq",13000.0f}, {"eqHighCutFreq",19000.0f} };
        eqPresetBox.onChange = [this, neutre, chaleureux, brillant, presence, air]
        {
            switch (eqPresetBox.getSelectedId())
            {
                case 1: applyPreset (neutre);     break;
                case 2: applyPreset (chaleureux); break;
                case 3: applyPreset (brillant);   break;
                case 4: applyPreset (presence);   break;
                case 5: applyPreset (air);        break;
                default: break;
            }
        };
    }

    // ================= DELAY =================
    finishSectionSetup (delaySection, "DELAY");
    addAndMakeVisible (delayActiveToggle);
    delaySection.activeToggle = &delayActiveToggle;
    delayActiveAttachment = std::make_unique<ButtonAttachment> (apvts, "delayActive", delayActiveToggle);
    attach (addKnob (delaySection, "TIME").slider, "delayTime");
    attach (addKnob (delaySection, "FEEDBACK").slider,    "delayFeedback");
    attach (addKnob (delaySection, "MIX").slider,         "delayMix");

    delayRateBox.addItem ("Free", 1);
    delayRateBox.addItem ("1/2", 2);
    delayRateBox.addItem ("1/4", 3);
    delayRateBox.addItem ("1/8", 4);
    delayRateBox.addItem ("1/16", 5);
    addAndMakeVisible (delayRateBox);
    delaySection.comboBox = &delayRateBox;
    delaySection.comboBoxKnobIndex = 0; // rattaché visuellement au knob TIME (même colonne)
    delayRateAttachment = std::make_unique<ComboBoxAttachment> (apvts, "delayRateMode", delayRateBox);

    addAndMakeVisible (delayPingPongToggle);
    delaySection.toggleButton = &delayPingPongToggle;
    delayPingPongAttachment = std::make_unique<ButtonAttachment> (apvts, "delayPingPong", delayPingPongToggle);

    // ================= REVERB =================
    finishSectionSetup (reverbSection, "REVERB");
    addAndMakeVisible (reverbActiveToggle);
    reverbSection.activeToggle = &reverbActiveToggle;
    reverbActiveAttachment = std::make_unique<ButtonAttachment> (apvts, "reverbActive", reverbActiveToggle);
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

    updateCompressorLabels(); // synchronise les libellés avec le mode actuel au chargement

    // ================= INPUT / OUTPUT =================
    inputTitleLabel.setText ("INPUT", juce::dontSendNotification);
    inputTitleLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    inputTitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8862b));
    inputTitleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (inputTitleLabel);

    outputTitleLabel.setText ("OUTPUT", juce::dontSendNotification);
    outputTitleLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    outputTitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8862b));
    outputTitleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (outputTitleLabel);

    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    inputGainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    inputGainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 56, 16);
    addAndMakeVisible (inputGainSlider);
    inputGainLabel.setText ("GAIN", juce::dontSendNotification);
    inputGainLabel.setFont (juce::Font (10.0f, juce::Font::bold));
    inputGainLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a8a92));
    inputGainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (inputGainLabel);
    attach (inputGainSlider, "inputGain");

    outputGainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 56, 16);
    addAndMakeVisible (outputGainSlider);
    outputGainLabel.setText ("GAIN", juce::dontSendNotification);
    outputGainLabel.setFont (juce::Font (10.0f, juce::Font::bold));
    outputGainLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a8a92));
    outputGainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (outputGainLabel);
    attach (outputGainSlider, "outputGain");

    setSize (1300, 760);
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

    auto full = getLocalBounds();
    auto inputStrip  = full.removeFromLeft (130);
    auto outputStrip = full.removeFromRight (130);

    drawPanel (g, inputStrip.withTop (86).withBottom (getHeight() - 24).reduced (8, 0));
    drawPanel (g, outputStrip.withTop (86).withBottom (getHeight() - 24).reduced (8, 0));

    auto content = full.reduced (24, 0).withTop (86).withBottom (getHeight() - 24);
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

    auto full = getLocalBounds();
    auto inputStrip  = full.removeFromLeft (130);
    auto outputStrip = full.removeFromRight (130);

    // --- Tranche INPUT ---
    {
        auto area = inputStrip.withTop (86).withBottom (getHeight() - 24).reduced (8, 0);
        area.reduce (8, 6);
        inputTitleLabel.setBounds (area.removeFromTop (18));
        auto gainArea = area.removeFromBottom (78);
        inputMeter.setBounds (area.reduced (30, 4));
        inputGainLabel.setBounds (gainArea.removeFromTop (14));
        inputGainSlider.setBounds (gainArea.reduced (4, 0));
    }

    // --- Tranche OUTPUT ---
    {
        auto area = outputStrip.withTop (86).withBottom (getHeight() - 24).reduced (8, 0);
        area.reduce (8, 6);
        outputTitleLabel.setBounds (area.removeFromTop (18));
        auto gainArea = area.removeFromBottom (78);
        outputMeter.setBounds (area.reduced (30, 4));
        outputGainLabel.setBounds (gainArea.removeFromTop (14));
        outputGainSlider.setBounds (gainArea.reduced (4, 0));
    }

    // --- Grille 3x2 de la chaîne, au centre ---
    auto content = full.reduced (24, 0).withTop (86).withBottom (getHeight() - 24);
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
