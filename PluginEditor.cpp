#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
TellusLookAndFeel::TellusLookAndFeel()
{
    const auto white = juce::Colours::white;

    setColour (juce::Slider::textBoxTextColourId,    white.withAlpha (0.85f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,            white.withAlpha (0.7f));

    setColour (juce::ComboBox::backgroundColourId,   juce::Colour (0xff1a1f33));
    setColour (juce::ComboBox::textColourId,         white);
    setColour (juce::ComboBox::outlineColourId,      juce::Colour (0xff3a4160));
    setColour (juce::ComboBox::arrowColourId,        white);

    setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff141828));
    setColour (juce::PopupMenu::textColourId,                  white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffff5a1f));
    setColour (juce::PopupMenu::highlightedTextColourId,       white);
}

void TellusLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle,
                                          juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const float radius    = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto  centre    = bounds.getCentre();
    const float angle     = startAngle + sliderPos * (endAngle - startAngle);
    const float thickness = juce::jmax (3.0f, radius * 0.12f);
    const float arcRadius = radius - thickness * 0.5f;

    const juce::PathStrokeType stroke (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a3050));
    g.strokePath (track, stroke);

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour (juce::Colour (0xffff6a2a));
    g.strokePath (value, stroke);

    const float body = radius * 0.72f;
    g.setColour (juce::Colour (0xff161a30));
    g.fillEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-2.0f, -body * 0.92f, 4.0f, body * 0.45f, 2.0f);
    g.setColour (juce::Colours::white);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

//==============================================================================
TensionEditor::TensionEditor (TensionProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    addKnob (buildK,   "build",     "BUILD");
    addKnob (filterK,  "filter",    "FILTER SWEEP");
    addKnob (noiseK,   "noise",     "NOISE RISER");
    addKnob (reverbK,  "reverb",    "REVERB WASH");
    addKnob (stutterK, "stutter",   "STUTTER");
    addKnob (driveK,   "drive",     "DRIVE");
    addKnob (eqLowK,   "eqlow",     "LOW");
    addKnob (eqMidK,   "eqmid",     "MID");
    addKnob (eqFreqK,  "eqmidfreq", "MID FREQ");
    addKnob (eqHighK,  "eqhigh",    "HIGH");

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);

    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    setSize (720, 640);
    startTimerHz (30);
}

TensionEditor::~TensionEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TensionEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void TensionEditor::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (20));
    k.slider.setBounds (area);
}

void TensionEditor::timerCallback()
{
    const float b = proc.apvts.getRawParameterValue ("build")->load();
    if (std::abs (b - lastBuild) > 0.001f)
    {
        lastBuild = b;
        repaint();
    }
}

void TensionEditor::resized()
{
    logoArea = { 24, 14, 130, 122 };
    presetBox.setBounds (getWidth() - 24 - 230, 56, 230, 30);

    placeKnob (buildK, { (getWidth() - 200) / 2, 155, 200, 190 });

    Knob* fx[] = { &filterK, &noiseK, &reverbK, &stutterK, &driveK };
    const int fw = getWidth() / 5;
    for (int i = 0; i < 5; ++i)
        placeKnob (*fx[i], { i * fw + 10, 350, fw - 20, 130 });

    Knob* eq[] = { &eqLowK, &eqMidK, &eqFreqK, &eqHighK };
    const int ew = getWidth() / 4;
    for (int i = 0; i < 4; ++i)
        placeKnob (*eq[i], { i * ew + 20, 512, ew - 40, 118 });
}

void TensionEditor::paint (juce::Graphics& g)
{
    const float b = proc.apvts.getRawParameterValue ("build")->load();
    const auto accent = juce::Colour (0xff4aa3ff).interpolatedWith (juce::Colour (0xffff5a1f), b);

    // Fundo: azul-noite que esquenta para vermelho/laranja conforme a tensão sobe
    const auto top    = juce::Colour (0xff0b1020).interpolatedWith (juce::Colour (0xff3a0d12), b);
    const auto bottom = juce::Colour (0xff05060a).interpolatedWith (juce::Colour (0xff8a2a10), b * b);
    g.setGradientFill (juce::ColourGradient (top, 0.0f, 0.0f, bottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // Logo Tellus: branca em repouso, tingida com a cor de acento quando o build sobe
    if (logo.isValid())
    {
        g.setColour (juce::Colours::white.interpolatedWith (accent, b));
        g.drawImage (logo, logoArea.toFloat(), juce::RectanglePlacement::centred, true);
    }

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.drawText ("TENSION RISE", 172, 44, 320, 36, juce::Justification::centredLeft);

    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawText ("BUILD-UP & TENSION", 174, 80, 320, 20, juce::Justification::centredLeft);
    g.drawText ("PRESET", presetBox.getX(), presetBox.getY() - 20, 100, 18, juce::Justification::centredLeft);

    // Barra de tensão
    const auto bar = juce::Rectangle<float> (24.0f, 146.0f, (float) getWidth() - 48.0f, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * b), 2.0f);

    // Título da seção de EQ
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawText ("EQ", 24, 488, 100, 18, juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRect (24, 507, getWidth() - 48, 1);
}
