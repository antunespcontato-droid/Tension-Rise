#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    const juce::Colour kOrange (0xffff6a2a);
    const juce::Colour kGrey   (0xff9aa0ad);

    juce::Font fontOf (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    float freqToX (float f, juce::Rectangle<float> r)
    {
        return r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); // 20 Hz .. 20 kHz
    }

    juce::String formatHz (float f)
    {
        return f >= 1000.0f ? juce::String (f / 1000.0f, 2) + " kHz"
                            : juce::String (juce::roundToInt (f)) + " Hz";
    }
}

//==============================================================================
TellusLookAndFeel::TellusLookAndFeel()
{
    const auto white = juce::Colours::white;

    setColour (juce::Slider::textBoxTextColourId,       white.withAlpha (0.85f));
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               kGrey);

    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1c1f27));
    setColour (juce::ComboBox::textColourId,       white);
    setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff343845));
    setColour (juce::ComboBox::arrowColourId,      white);

    setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1c1f27));
    setColour (juce::TextButton::textColourOffId,  white);

    setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff141720));
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
    g.setColour (juce::Colour (0xff2a2e3d));
    g.strokePath (track, stroke);

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour (kOrange);
    g.strokePath (value, stroke);

    const float body = radius * 0.72f;
    g.setColour (juce::Colour (0xff1b1e29));
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

    addKnob (buildK,   "build",     "");
    addKnob (filterK,  "filter",    "FILTER SWEEP");
    addKnob (noiseK,   "noise",     "NOISE RISER");
    addKnob (reverbK,  "reverb",    "REVERB WASH");
    addKnob (stutterK, "stutter",   "STUTTER");
    addKnob (driveK,   "drive",     "DRIVE");
    addKnob (mixK,     "mix",       "DRY / WET");
    addKnob (outputK,  "output",    "OUTPUT");
    addKnob (eqLowK,   "eqlow",     "LOW");
    addKnob (eqMidK,   "eqmid",     "MID");
    addKnob (eqFreqK,  "eqmidfreq", "MID FREQ");
    addKnob (eqHighK,  "eqhigh",    "HIGH");

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);

    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    setSize (900, 648);
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
    k.label.setFont (fontOf (11.5f, true));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void TensionEditor::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (16));
    k.slider.setBounds (area);
}

void TensionEditor::stepPreset (int delta)
{
    const int n = presetBox.getNumItems();
    if (n <= 0)
        return;

    int idx = presetBox.getSelectedItemIndex() + delta;
    if (idx < 0)  idx = n - 1;
    if (idx >= n) idx = 0;
    presetBox.setSelectedItemIndex (idx, juce::sendNotificationSync);
}

float TensionEditor::param (const char* id) const
{
    return proc.apvts.getRawParameterValue (id)->load();
}

void TensionEditor::timerCallback()
{
    meterL = juce::jmax (proc.outLevelL.load(), meterL * 0.88f);
    meterR = juce::jmax (proc.outLevelR.load(), meterR * 0.88f);
    repaint();
}

void TensionEditor::resized()
{
    logoArea  = { 16, 8, 52, 48 };
    meterArea = { 696, 14, 188, 36 };

    prevBtn.setBounds   (294, 18, 30, 28);
    presetBox.setBounds (330, 18, 240, 28);
    nextBtn.setBounds   (576, 18, 30, 28);

    panelBuild   = {  16,  72, 300, 240 };
    panelDisplay = { 328,  72, 556, 240 };
    panelFx      = {  16, 320, 584, 156 };
    panelOut     = { 612, 320, 272, 156 };
    panelEq      = {  16, 484, 868, 148 };

    displayArea   = { 340, 100, 532, 200 };
    eqDisplayArea = {  28, 512, 392, 108 };

    placeKnob (buildK, { 46, 96, 240, 212 });

    Knob* fx[] = { &filterK, &noiseK, &reverbK, &stutterK, &driveK };
    for (int i = 0; i < 5; ++i)
        placeKnob (*fx[i], { 28 + i * 112, 348, 108, 120 });

    placeKnob (mixK,    { 628, 348, 120, 120 });
    placeKnob (outputK, { 752, 348, 120, 120 });

    Knob* eq[] = { &eqLowK, &eqMidK, &eqFreqK, &eqHighK };
    for (int i = 0; i < 4; ++i)
        placeKnob (*eq[i], { 436 + i * 110, 512, 106, 112 });
}

//==============================================================================
void TensionEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title) const
{
    g.setColour (juce::Colour (0xff15171d));
    g.fillRoundedRectangle (r.toFloat(), 5.0f);
    g.setColour (juce::Colour (0xff2a2d38));
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, 1.0f);

    g.setColour (kGrey);
    g.setFont (fontOf (11.5f, true));
    g.drawText (title, r.getX() + 12, r.getY() + 7, r.getWidth() - 24, 16, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillRect (r.getX() + 12, r.getY() + 25, r.getWidth() - 24, 1);
}

void TensionEditor::drawMeters (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (kGrey);
    g.setFont (fontOf (10.5f, true));
    g.drawText ("OUT", area.getX(), area.getY(), 30, area.getHeight(), juce::Justification::centredLeft);

    const float levels[2] = { meterL, meterR };
    for (int i = 0; i < 2; ++i)
    {
        auto bar = juce::Rectangle<float> ((float) area.getX() + 34.0f, (float) area.getY() + 5.0f + 15.0f * (float) i,
                                           (float) area.getWidth() - 34.0f, 9.0f);
        g.setColour (juce::Colour (0xff22252e));
        g.fillRoundedRectangle (bar, 2.0f);

        const float db   = juce::Decibels::gainToDecibels (levels[i], -60.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f));
        const float hot  = 0.95f; // ~ -3 dB

        g.setColour (kOrange);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jmin (norm, hot)), 2.0f);
        if (norm > hot)
        {
            g.setColour (juce::Colour (0xffff2a2a));
            g.fillRect (bar.getX() + bar.getWidth() * hot, bar.getY(), bar.getWidth() * (norm - hot), bar.getHeight());
        }
    }
}

void TensionEditor::drawTensionDisplay (juce::Graphics& g, juce::Rectangle<int> area, float b, juce::Colour accent) const
{
    const auto r = area.toFloat();
    g.setColour (juce::Colour (0xff0c0e13));
    g.fillRoundedRectangle (r, 4.0f);

    const float sr = proc.getSampleRate() > 0.0 ? (float) proc.getSampleRate() : 44100.0f;
    const float dbTop = 12.0f, dbBottom = -48.0f;

    auto dbToY = [&] (float db)
    {
        return r.getY() + r.getHeight() * (dbTop - juce::jlimit (dbBottom, dbTop, db)) / (dbTop - dbBottom);
    };
    auto fToX = [&] (float f) { return freqToX (f, r); };

    // Grade
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        g.drawVerticalLine (juce::roundToInt (fToX (f)), r.getY(), r.getBottom());
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f })
        g.drawHorizontalLine (juce::roundToInt (dbToY (db)), r.getX(), r.getRight());

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.setFont (fontOf (10.0f));
    const std::pair<float, const char*> marks[] = { { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } };
    for (const auto& m : marks)
        g.drawText (m.second, juce::roundToInt (fToX (m.first)) - 16, (int) r.getBottom() - 15, 32, 13, juce::Justification::centred);

    // Faixa do noise riser (aparece da frequência de corte do ruído até 20 kHz)
    const float noiseCut = 300.0f + 8000.0f * b * b;
    const float noiseLvl = param ("noise") * b * b;
    if (noiseLvl > 0.003f)
    {
        const float x0 = fToX (noiseCut);
        const float y0 = dbToY (-48.0f + 44.0f * noiseLvl);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRect (x0, y0, r.getRight() - x0, r.getBottom() - y0);
        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.drawHorizontalLine (juce::roundToInt (y0), x0, r.getRight());
        g.setFont (fontOf (10.0f, true));
        g.drawText ("NOISE", juce::roundToInt (x0) + 6, juce::roundToInt (y0) + 3, 50, 12, juce::Justification::centredLeft);
    }

    // Curva do filtro passa-alta
    const float amtFilter = param ("filter");
    const float cutoff    = 20.0f * std::pow (400.0f, b * amtFilter);
    const float q         = 0.7f + 2.0f * b * amtFilter;
    auto hp = Coeffs::makeHighPass (sr, cutoff, q);

    juce::Path curve;
    const int steps = juce::jmax (2, (int) (r.getWidth() / 2.0f));
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        const float x = r.getX() + r.getWidth() * t;
        const float f = 20.0f * std::pow (1000.0f, t);
        const float mag = (float) hp->getMagnitudeForFrequency ((double) f, (double) sr);
        const float y = dbToY (juce::Decibels::gainToDecibels (mag, -100.0f));
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }

    juce::Path fill (curve);
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.50f), 0.0f, r.getY(),
                                             accent.withAlpha (0.04f), 0.0f, r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    g.setColour (accent.withAlpha (0.55f));
    g.drawVerticalLine (juce::roundToInt (fToX (cutoff)), r.getY(), r.getBottom());

    // Leituras
    g.setFont (fontOf (11.0f, true));
    g.setColour (kGrey);
    g.drawText ("HIGH-PASS", (int) r.getX() + 10, (int) r.getY() + 8, 70, 14, juce::Justification::centredLeft);
    g.setColour (accent);
    g.drawText (formatHz (cutoff), (int) r.getX() + 82, (int) r.getY() + 8, 90, 14, juce::Justification::centredLeft);

    g.setColour (kGrey);
    g.drawText ("STUTTER", (int) r.getRight() - 150, (int) r.getY() + 8, 60, 14, juce::Justification::centredRight);
    g.setColour (accent);
    g.drawText (juce::String (1.0f + 15.0f * b * b, 1) + " Hz", (int) r.getRight() - 86, (int) r.getY() + 8, 76, 14, juce::Justification::centredRight);
}

void TensionEditor::drawEqDisplay (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const auto r = area.toFloat();
    g.setColour (juce::Colour (0xff0c0e13));
    g.fillRoundedRectangle (r, 4.0f);

    const float sr = proc.getSampleRate() > 0.0 ? (float) proc.getSampleRate() : 44100.0f;
    const float range = 15.0f;

    auto dbToY = [&] (float db) { return r.getCentreY() - juce::jlimit (-range, range, db) / range * (r.getHeight() * 0.5f - 4.0f); };
    auto fToX  = [&] (float f)  { return freqToX (f, r); };

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        g.drawVerticalLine (juce::roundToInt (fToX (f)), r.getY(), r.getBottom());
    for (float db : { -12.0f, -6.0f, 6.0f, 12.0f })
        g.drawHorizontalLine (juce::roundToInt (dbToY (db)), r.getX(), r.getRight());
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawHorizontalLine (juce::roundToInt (dbToY (0.0f)), r.getX(), r.getRight());

    const float lowDb  = param ("eqlow");
    const float midDb  = param ("eqmid");
    const float midF   = param ("eqmidfreq");
    const float highDb = param ("eqhigh");

    auto lc = Coeffs::makeLowShelf   (sr, 120.0f,  0.707f, juce::Decibels::decibelsToGain (lowDb));
    auto mc = Coeffs::makePeakFilter (sr, midF,    0.9f,   juce::Decibels::decibelsToGain (midDb));
    auto hc = Coeffs::makeHighShelf  (sr, 6000.0f, 0.707f, juce::Decibels::decibelsToGain (highDb));

    auto totalDb = [&] (float f)
    {
        const double m = lc->getMagnitudeForFrequency ((double) f, (double) sr)
                       * mc->getMagnitudeForFrequency ((double) f, (double) sr)
                       * hc->getMagnitudeForFrequency ((double) f, (double) sr);
        return juce::Decibels::gainToDecibels ((float) m, -100.0f);
    };

    juce::Path curve;
    const int steps = juce::jmax (2, (int) (r.getWidth() / 2.0f));
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        const float x = r.getX() + r.getWidth() * t;
        const float y = dbToY (totalDb (20.0f * std::pow (1000.0f, t)));
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }

    juce::Path fill (curve);
    fill.lineTo (r.getRight(), dbToY (0.0f));
    fill.lineTo (r.getX(), dbToY (0.0f));
    fill.closeSubPath();
    g.setColour (kOrange.withAlpha (0.22f));
    g.fillPath (fill);
    g.setColour (kOrange);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    for (float f : { 120.0f, midF, 6000.0f })
    {
        const float x = fToX (f), y = dbToY (totalDb (f));
        g.setColour (juce::Colour (0xff0c0e13));
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colours::white);
        g.drawEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f, 1.5f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.setFont (fontOf (10.0f));
    const std::pair<float, const char*> marks[] = { { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } };
    for (const auto& m : marks)
        g.drawText (m.second, juce::roundToInt (fToX (m.first)) - 16, (int) r.getBottom() - 14, 32, 12, juce::Justification::centred);
    g.drawText ("+12", (int) r.getX() + 4, juce::roundToInt (dbToY (12.0f)) - 12, 30, 12, juce::Justification::centredLeft);
    g.drawText ("-12", (int) r.getX() + 4, juce::roundToInt (dbToY (-12.0f)), 30, 12, juce::Justification::centredLeft);
}

void TensionEditor::paint (juce::Graphics& g)
{
    const float b = param ("build");
    const auto accent = juce::Colour (0xff4aa3ff).interpolatedWith (juce::Colour (0xffff5a1f), b);

    // Fundo escuro, com um leve aquecimento conforme o build sobe
    const auto top    = juce::Colour (0xff14161c).interpolatedWith (juce::Colour (0xff2c1210), b);
    const auto bottom = juce::Colour (0xff0b0c10).interpolatedWith (juce::Colour (0xff1d0d08), b);
    g.setGradientFill (juce::ColourGradient (top, 0.0f, 0.0f, bottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // Cabeçalho
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (0, 0, getWidth(), 64);
    g.setColour (juce::Colour (0xff2a2d38));
    g.fillRect (0, 64, getWidth(), 1);
    g.setColour (accent);
    g.fillRect (0, 62, juce::roundToInt ((float) getWidth() * b), 2);   // barra de tensão

    if (logo.isValid())
    {
        g.setColour (juce::Colours::white.interpolatedWith (accent, b));
        g.drawImage (logo, logoArea.toFloat(), juce::RectanglePlacement::centred, true);
    }

    g.setColour (juce::Colours::white);
    g.setFont (fontOf (20.0f, true));
    g.drawText ("TENSION RISE", 80, 12, 200, 26, juce::Justification::centredLeft);
    g.setColour (kGrey);
    g.setFont (fontOf (10.5f));
    g.drawText ("BUILD-UP & TENSION", 81, 36, 200, 14, juce::Justification::centredLeft);

    drawMeters (g, meterArea);

    // Painéis
    drawPanel (g, panelBuild,   "BUILD");
    drawPanel (g, panelDisplay, "TENSION CURVE");
    drawPanel (g, panelFx,      "TENSION FX");
    drawPanel (g, panelOut,     "OUTPUT MIX");
    drawPanel (g, panelEq,      "EQUALIZER");

    drawTensionDisplay (g, displayArea, b, accent);
    drawEqDisplay (g, eqDisplayArea);
}
