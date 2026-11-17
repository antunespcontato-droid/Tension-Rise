#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    const juce::Colour kOrange (0xffff6a2a);
    const juce::Colour kGrey   (0xff9aa0ad);
    const juce::Colour kBg     (0xff0c0e13);

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

    void drawFreqGrid (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
            g.drawVerticalLine (juce::roundToInt (freqToX (f, r)), r.getY(), r.getBottom());

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (fontOf (10.0f));
        const std::pair<float, const char*> marks[] = { { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } };
        for (const auto& m : marks)
            g.drawText (m.second, juce::roundToInt (freqToX (m.first, r)) - 16, (int) r.getBottom() - 14, 32, 12,
                        juce::Justification::centred);
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
    setColour (juce::TextButton::buttonOnColourId, kOrange);
    setColour (juce::TextButton::textColourOffId,  white.withAlpha (0.8f));
    setColour (juce::TextButton::textColourOnId,   white);

    setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff141720));
    setColour (juce::PopupMenu::textColourId,                  white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffff5a1f));
    setColour (juce::PopupMenu::highlightedTextColourId,       white);
}

void TellusLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle,
                                          juce::Slider& s)
{
    const float al = s.isEnabled() ? 1.0f : 0.35f;
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const float radius    = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto  centre    = bounds.getCentre();
    const float angle     = startAngle + sliderPos * (endAngle - startAngle);
    const float thickness = juce::jmax (3.0f, radius * 0.12f);
    const float arcRadius = radius - thickness * 0.5f;

    const juce::PathStrokeType stroke (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a2e3d).withMultipliedAlpha (al));
    g.strokePath (track, stroke);

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour (kOrange.withMultipliedAlpha (al));
    g.strokePath (value, stroke);

    const float body = radius * 0.72f;
    g.setColour (juce::Colour (0xff1b1e29));
    g.fillEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-2.0f, -body * 0.92f, 4.0f, body * 0.45f, 2.0f);
    g.setColour (juce::Colours::white.withMultipliedAlpha (al));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

//==============================================================================
TensionEditor::TensionEditor (TensionProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    // Cabeçalho
    addCombo (modeC, "mode", "", -1);

    // MASTER
    addKnob (buildK,   "build",     "",             0);
    addKnob (filterK,  "filter",    "FILTER SWEEP", 0);
    addKnob (noiseK,   "noise",     "NOISE RISER",  0);
    addKnob (reverbK,  "reverb",    "REVERB WASH",  0);
    addKnob (gateK,    "stutter",   "GATE DEPTH",   0);
    addKnob (driveK,   "drive",     "DRIVE",        0);
    addKnob (mixK,     "mix",       "DRY / WET",    0);
    addKnob (outputK,  "output",    "OUTPUT",       0);
    addKnob (eqLowK,   "eqlow",     "LOW",          0);
    addKnob (eqMidK,   "eqmid",     "MID",          0);
    addKnob (eqFreqK,  "eqmidfreq", "MID FREQ",     0);
    addKnob (eqHighK,  "eqhigh",    "HIGH",         0);

    // BUILD ENGINE
    addCombo  (lengthC,    "length",    "LENGTH",        1);
    addCombo  (curveC,     "curve",     "TENSION CURVE", 1);
    addKnob   (startK,     "startbar",  "START BAR",     1);
    addKnob   (intensityK, "intensity", "INTENSITY",     1);
    addCombo  (gateRateC,  "gaterate",  "RATE",          1);
    addToggle (gateSyncT,  "gatesync",  "SYNC",          1);
    addToggle (gateAccelT, "gateaccel", "ACCELERATE",    1);
    addKnob   (gateDepthK, "stutter",   "DEPTH",         1);

    // FILTROS & ESTÉREO
    addToggle (panOnT,    "panon",    "AUTO PAN",  2);
    addCombo  (panShapeC, "panshape", "SHAPE",     2);
    addCombo  (panRateC,  "panrate",  "RATE",      2);
    addToggle (panSyncT,  "pansync",  "SYNC",      2);
    addToggle (panBassT,  "panbass",  "BASS MONO", 2);
    addKnob   (panDepthK, "pandepth", "DEPTH",     2);
    addKnob   (panPhaseK, "panphase", "PHASE",     2);
    addKnob   (panWidthK, "panwidth", "WIDTH",     2);
    addKnob   (panHzK,    "panhz",    "RATE (Hz)", 2);
    addToggle (lcOnT,     "lcon",     "LOW CUT",   2);
    addCombo  (lcSlopeC,  "lcslope",  "SLOPE",     2);
    addKnob   (lcFreqK,   "lcfreq",   "FREQ",      2);
    addToggle (hcOnT,     "hcon",     "HIGH CUT",  2);
    addCombo  (hcSlopeC,  "hcslope",  "SLOPE",     2);
    addKnob   (hcFreqK,   "hcfreq",   "FREQ",      2);

    // Abas
    const char* tabNames[3] = { "MASTER", "BUILD ENGINE", "FILTERS & STEREO" };
    for (int i = 0; i < 3; ++i)
    {
        tabBtn[i].setButtonText (tabNames[i]);
        tabBtn[i].setClickingTogglesState (true);
        tabBtn[i].setRadioGroupId (1001);
        tabBtn[i].onClick = [this, i] { setPage (i); };
        addAndMakeVisible (tabBtn[i]);
    }
    tabBtn[0].setToggleState (true, juce::dontSendNotification);

    // Presets
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);

    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    setSize (900, 676);
    setPage (0);
    startTimerHz (30);
}

TensionEditor::~TensionEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TensionEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& text, int pg)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (fontOf (11.5f, true));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);

    if (pg >= 0) { pageComps[pg].push_back (&k.slider); pageComps[pg].push_back (&k.label); }
}

void TensionEditor::addCombo (Combo& c, const juce::String& paramId, const juce::String& text, int pg)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        c.box.addItemList (choice->choices, 1);
    addAndMakeVisible (c.box);

    c.label.setText (text, juce::dontSendNotification);
    c.label.setJustificationType (juce::Justification::centredLeft);
    c.label.setFont (fontOf (10.5f, true));
    if (text.isNotEmpty())
        addAndMakeVisible (c.label);

    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, c.box);

    if (pg >= 0)
    {
        pageComps[pg].push_back (&c.box);
        if (text.isNotEmpty()) pageComps[pg].push_back (&c.label);
    }
}

void TensionEditor::addToggle (Toggle& t, const juce::String& paramId, const juce::String& text, int pg)
{
    t.button.setButtonText (text);
    t.button.setClickingTogglesState (true);
    addAndMakeVisible (t.button);
    t.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramId, t.button);
    if (pg >= 0) pageComps[pg].push_back (&t.button);
}

void TensionEditor::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (16));
    k.slider.setBounds (area);
}

void TensionEditor::placeCombo (Combo& c, juce::Rectangle<int> area)
{
    c.label.setBounds (area.removeFromTop (14));
    c.box.setBounds (area.withHeight (26));
}

void TensionEditor::setPage (int newPage)
{
    page = newPage;
    for (int p = 0; p < 3; ++p)
        for (auto* c : pageComps[p])
            c->setVisible (p == page);
    repaint();
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

    // No modo Auto o knob Build é controlado pelo motor
    const bool autoMode = (int) param ("mode") == 1;
    if (buildK.slider.isEnabled() == autoMode)
        buildK.slider.setEnabled (! autoMode);

    repaint();
}

void TensionEditor::resized()
{
    logoArea  = { 16, 8, 52, 48 };
    meterArea = { 696, 14, 188, 36 };
    prevBtn.setBounds   (294, 18, 30, 28);
    presetBox.setBounds (330, 18, 240, 28);
    nextBtn.setBounds   (576, 18, 30, 28);

    for (int i = 0; i < 3; ++i)
        tabBtn[i].setBounds (16 + i * 128, 70, 124, 24);
    modeC.box.setBounds (452, 70, 120, 24);
    syncArea = { 600, 68, 284, 28 };

    // MASTER
    panelBuild   = {  16, 100, 300, 240 };
    panelDisplay = { 328, 100, 556, 240 };
    panelFx      = {  16, 348, 584, 156 };
    panelOut     = { 612, 348, 272, 156 };
    panelEq      = {  16, 512, 868, 148 };
    displayArea   = { 340, 128, 532, 200 };
    eqDisplayArea = {  28, 540, 392, 108 };

    placeKnob (buildK, { 46, 124, 240, 212 });
    Knob* fx[] = { &filterK, &noiseK, &reverbK, &gateK, &driveK };
    for (int i = 0; i < 5; ++i)
        placeKnob (*fx[i], { 28 + i * 112, 376, 108, 120 });
    placeKnob (mixK,    { 628, 376, 120, 120 });
    placeKnob (outputK, { 752, 376, 120, 120 });
    Knob* eq[] = { &eqLowK, &eqMidK, &eqFreqK, &eqHighK };
    for (int i = 0; i < 4; ++i)
        placeKnob (*eq[i], { 436 + i * 110, 540, 106, 112 });

    // BUILD ENGINE
    panelEngine = { 16, 100, 868, 340 };
    panelGate   = { 16, 448, 868, 212 };
    engineDisplayArea = { 340, 128, 532, 300 };
    gateDisplayArea   = { 340, 476, 532, 164 };

    placeCombo (lengthC, { 28, 134, 170, 44 });
    placeCombo (curveC,  { 28, 190, 170, 44 });
    placeKnob  (startK,     { 28, 248, 140, 180 });
    placeKnob  (intensityK, { 180, 248, 140, 180 });

    placeCombo (gateRateC, { 28, 476, 170, 44 });
    gateSyncT.button.setBounds  (28, 532, 80, 26);
    gateAccelT.button.setBounds (116, 532, 102, 26);
    placeKnob (gateDepthK, { 214, 472, 112, 120 });
    gateHintArea = { 28, 574, 298, 80 };

    // FILTROS & ESTÉREO
    panelPan = { 16, 100, 868, 250 };
    panelCut = { 16, 358, 868, 302 };
    panDisplayArea = { 560, 134, 312, 206 };
    cutDisplayArea = { 28, 386, 520, 260 };

    panOnT.button.setBounds (28, 148, 90, 26);
    placeCombo (panShapeC, { 130, 134, 110, 44 });
    placeCombo (panRateC,  { 254, 134, 100, 44 });
    panSyncT.button.setBounds (368, 148, 64, 26);
    panBassT.button.setBounds (442, 148, 100, 26);
    Knob* pk[] = { &panDepthK, &panPhaseK, &panWidthK, &panHzK };
    for (int i = 0; i < 4; ++i)
        placeKnob (*pk[i], { 28 + i * 132, 190, 122, 152 });

    lcOnT.button.setBounds (572, 400, 90, 26);
    placeCombo (lcSlopeC, { 572, 440, 130, 44 });
    placeKnob  (lcFreqK,  { 716, 392, 150, 118 });
    hcOnT.button.setBounds (572, 532, 90, 26);
    placeCombo (hcSlopeC, { 572, 572, 130, 44 });
    placeKnob  (hcFreqK,  { 716, 524, 150, 118 });
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

void TensionEditor::drawSyncStatus (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const int state = proc.uiSync.load();
    const juce::Colour led = state == 2 ? juce::Colour (0xff39d98a) : (state == 1 ? kOrange : juce::Colour (0xff5a5f6b));

    // LED: pisca no tempo quando está tocando
    float glow = 1.0f;
    if (state == 2)
        glow = 0.55f + 0.45f * (1.0f - std::fmod (proc.uiBarFrac.load() * (float) proc.uiNum.load() * 4.0f / (float) proc.uiDen.load(), 1.0f));

    const float cy = (float) area.getCentreY();
    g.setColour (led.withAlpha (0.25f * glow));
    g.fillEllipse ((float) area.getX() - 3.0f, cy - 8.0f, 16.0f, 16.0f);
    g.setColour (led.withAlpha (glow));
    g.fillEllipse ((float) area.getX(), cy - 5.0f, 10.0f, 10.0f);

    g.setFont (fontOf (11.5f, true));
    g.setColour (kGrey);
    g.drawText ("SYNC", area.getX() + 18, area.getY(), 40, area.getHeight(), juce::Justification::centredLeft);

    juce::String s;
    if (state == 0)
        s = "NO HOST";
    else
        s = juce::String (proc.uiBpm.load(), 1) + " BPM   BAR " + juce::String ((int) std::floor (proc.uiBarPos.load()) + 1)
            + "   " + juce::String (proc.uiNum.load()) + "/" + juce::String (proc.uiDen.load())
            + (state == 2 ? "   PLAYING" : "   STOPPED");

    g.setColour (state == 0 ? kGrey : juce::Colours::white.withAlpha (0.9f));
    g.drawText (s, area.getX() + 58, area.getY(), area.getWidth() - 58, area.getHeight(), juce::Justification::centredLeft);
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
    g.setColour (kBg);
    g.fillRoundedRectangle (r, 4.0f);

    const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    const float dbTop = 12.0f, dbBottom = -48.0f;

    auto dbToY = [&] (float db)
    {
        return r.getY() + r.getHeight() * (dbTop - juce::jlimit (dbBottom, dbTop, db)) / (dbTop - dbBottom);
    };
    auto fToX = [&] (float f) { return freqToX (f, r); };

    drawFreqGrid (g, r);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f })
        g.drawHorizontalLine (juce::roundToInt (dbToY (db)), r.getX(), r.getRight());

    // Faixa do noise riser
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

    // Curva do filtro passa-alta do build
    const float amtFilter = param ("filter");
    const float cutoff    = 20.0f * std::pow (400.0f, b * amtFilter);
    const double q        = 0.7 + 2.0 * b * amtFilter;
    const auto hp = tr::makeHighPass (sr, cutoff, q);

    juce::Path curve;
    const int steps = juce::jmax (2, (int) (r.getWidth() / 2.0f));
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        const float x = r.getX() + r.getWidth() * t;
        const double mag = tr::magnitudeAt (hp, 20.0 * std::pow (1000.0, (double) t), sr);
        const float y = dbToY (juce::Decibels::gainToDecibels ((float) mag, -100.0f));
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

    g.setFont (fontOf (11.0f, true));
    g.setColour (kGrey);
    g.drawText ("HIGH-PASS", (int) r.getX() + 10, (int) r.getY() + 8, 70, 14, juce::Justification::centredLeft);
    g.setColour (accent);
    g.drawText (formatHz (cutoff), (int) r.getX() + 82, (int) r.getY() + 8, 90, 14, juce::Justification::centredLeft);

    // Ritmo do gate no momento
    const bool sync = param ("gatesync") > 0.5f;
    g.setColour (kGrey);
    g.drawText ("GATE", (int) r.getRight() - 160, (int) r.getY() + 8, 40, 14, juce::Justification::centredRight);
    g.setColour (accent);
    g.drawText (sync ? juce::String (tr::divisionName ((int) param ("gaterate"))) + (param ("gateaccel") > 0.5f ? "  ACCEL" : "")
                     : juce::String (1.0f + 15.0f * b * b, 1) + " Hz (free)",
                (int) r.getRight() - 114, (int) r.getY() + 8, 104, 14, juce::Justification::centredRight);
}

void TensionEditor::drawEngineDisplay (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour accent) const
{
    const auto r = area.toFloat();
    g.setColour (kBg);
    g.fillRoundedRectangle (r, 4.0f);

    const int curve    = (int) param ("curve");
    const int len      = tr::lengthBars ((int) param ("length"));
    const int startBar = juce::roundToInt (param ("startbar"));
    const float inten  = param ("intensity");
    const int mode     = (int) param ("mode");

    auto plot = r.reduced (14.0f, 30.0f);
    plot.removeFromBottom (6.0f);

    // Grade: um traço por grupo de compassos
    const int stepBars = len <= 16 ? 1 : (len == 32 ? 2 : 4);
    for (int bar = 0; bar <= len; bar += stepBars)
    {
        const float x = plot.getX() + plot.getWidth() * (float) bar / (float) len;
        g.setColour (juce::Colours::white.withAlpha (bar % (stepBars * 4) == 0 ? 0.10f : 0.04f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
    }
    for (float v : { 0.25f, 0.5f, 0.75f })
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine (juce::roundToInt (plot.getBottom() - plot.getHeight() * v), plot.getX(), plot.getRight());
    }

    juce::Path path;
    const int steps = 240;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        const float x = plot.getX() + plot.getWidth() * t;
        const float y = plot.getBottom() - plot.getHeight() * tr::curveValue (curve, t) * inten;
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }
    juce::Path fill (path);
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();

    const float alpha = mode == 0 ? 0.35f : 1.0f;
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.45f * alpha), 0.0f, plot.getY(),
                                             accent.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
    g.fillPath (fill);
    g.setColour (accent.withAlpha (alpha));
    g.strokePath (path, juce::PathStrokeType (2.0f));

    // Marcador de posição (playhead)
    const double t = (proc.uiBarPos.load() - (double) (startBar - 1)) / (double) len;
    const bool inside = proc.uiSync.load() > 0 && t >= 0.0 && t < 1.0;
    if (inside)
    {
        const float x = plot.getX() + plot.getWidth() * (float) t;
        const float y = plot.getBottom() - plot.getHeight() * tr::curveValue (curve, (float) t) * inten;
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
    }

    // Textos
    g.setFont (fontOf (11.0f, true));
    g.setColour (kGrey);
    g.drawText ("BAR " + juce::String (startBar), (int) plot.getX(), (int) r.getBottom() - 22, 80, 14, juce::Justification::centredLeft);
    g.drawText ("BAR " + juce::String (startBar + len) + "  DROP", (int) plot.getRight() - 120, (int) r.getBottom() - 22, 120, 14,
                juce::Justification::centredRight);

    static const char* curveNames[] = { "LINEAR RISE", "EXPONENTIAL RISE", "SLOW RISE", "FAST RISE", "STEPPED RISE" };
    g.setColour (accent);
    g.drawText (juce::String (curveNames[juce::jlimit (0, 4, curve)]) + "  /  " + juce::String (len) + " BARS",
                (int) r.getX() + 12, (int) r.getY() + 8, 260, 14, juce::Justification::centredLeft);

    juce::String status;
    if (mode == 0)               status = "MODE: MANUAL (engine off)";
    else if (proc.uiSync.load() == 0) status = "NO HOST POSITION";
    else if (t < 0.0)            status = "WAITING  " + juce::String ((int) std::ceil (-t * len)) + " BARS";
    else if (t >= 1.0)           status = "DROP PASSED";
    else                         status = "BUILD " + juce::String (juce::roundToInt ((float) t * 100.0f)) + "%";
    g.setColour (mode == 0 ? kGrey : juce::Colours::white.withAlpha (0.9f));
    g.drawText (status, (int) r.getRight() - 232, (int) r.getY() + 8, 220, 14, juce::Justification::centredRight);
}

void TensionEditor::drawGateDisplay (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour accent) const
{
    const auto r = area.toFloat();
    g.setColour (kBg);
    g.fillRoundedRectangle (r, 4.0f);

    const double beatsPerBar = (double) proc.uiNum.load() * 4.0 / (double) proc.uiDen.load();
    const bool sync = param ("gatesync") > 0.5f;
    const float depth = param ("stutter");
    const double cycle = (double) proc.uiGateCycle.load();

    auto plot = r.reduced (14.0f, 30.0f);
    plot.removeFromBottom (4.0f);

    if (! sync)
    {
        g.setColour (kGrey);
        g.setFont (fontOf (12.0f, true));
        g.drawText ("SYNC OFF  -  free rate", plot.toNearestInt(), juce::Justification::centred);
    }
    else
    {
        const int cycles = (int) std::ceil (beatsPerBar / cycle - 1e-6);
        for (int c = 0; c < cycles; ++c)
        {
            const float x0 = plot.getX() + plot.getWidth() * (float) (c * cycle / beatsPerBar);
            const float w  = plot.getWidth() * (float) (cycle / beatsPerBar);
            g.setColour (accent.withAlpha (0.75f));
            g.fillRect (x0 + 0.5f, plot.getY(), juce::jmax (1.0f, w * 0.5f - 1.0f), plot.getHeight());
            g.setColour (accent.withAlpha (0.08f + 0.55f * (1.0f - depth)));
            g.fillRect (x0 + w * 0.5f + 0.5f, plot.getY(), juce::jmax (1.0f, w * 0.5f - 1.0f), plot.getHeight());
        }
        for (int beat = 0; beat <= (int) beatsPerBar; ++beat)
        {
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawVerticalLine (juce::roundToInt (plot.getX() + plot.getWidth() * (float) beat / (float) beatsPerBar),
                                plot.getY() - 4.0f, plot.getBottom() + 4.0f);
        }
        if (proc.uiSync.load() > 0)
        {
            g.setColour (juce::Colours::white);
            g.fillRect (plot.getX() + plot.getWidth() * proc.uiBarFrac.load() - 1.0f, plot.getY() - 6.0f, 2.0f, plot.getHeight() + 12.0f);
        }
    }

    g.setFont (fontOf (11.0f, true));
    g.setColour (kGrey);
    g.drawText ("1 BAR", (int) r.getX() + 12, (int) r.getY() + 8, 60, 14, juce::Justification::centredLeft);
    g.setColour (accent);
    g.drawText (sync ? juce::String (tr::divisionName ((int) param ("gaterate"))) + (param ("gateaccel") > 0.5f ? "  ACCELERATING" : "")
                     : "FREE",
                (int) r.getRight() - 232, (int) r.getY() + 8, 220, 14, juce::Justification::centredRight);
}

void TensionEditor::drawPanDisplay (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const auto r = area.toFloat();
    g.setColour (kBg);
    g.fillRoundedRectangle (r, 4.0f);

    const int shape = (int) param ("panshape");
    const float depth = param ("pandepth") * (param ("panon") > 0.5f ? 1.0f : 0.3f);

    auto wave = r.reduced (14.0f, 14.0f);
    wave.removeFromBottom (56.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine (juce::roundToInt (wave.getCentreY()), wave.getX(), wave.getRight());

    juce::Path p;
    const float rndPattern[4] = { 0.6f, -0.8f, 0.3f, -0.5f };    // S&H: exemplo ilustrativo do formato
    for (int i = 0; i <= 160; ++i)
    {
        const float t = (float) i / 160.0f;
        const float ph = std::fmod (t + param ("panphase"), 1.0f);
        const float rnd = rndPattern[juce::jmin (3, (int) (t * 4.0f))];
        const float v = tr::lfoShape (shape, ph, rnd) * depth;
        const float x = wave.getX() + wave.getWidth() * t;
        const float y = wave.getCentreY() - v * (wave.getHeight() * 0.5f);
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (kOrange.withAlpha (param ("panon") > 0.5f ? 1.0f : 0.4f));
    g.strokePath (p, juce::PathStrokeType (2.0f));

    // Barra L - R com o ponto da posição atual
    auto bar = juce::Rectangle<float> (r.getX() + 36.0f, r.getBottom() - 38.0f, r.getWidth() - 72.0f, 10.0f);
    g.setColour (juce::Colour (0xff22252e));
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillRect (bar.getCentreX() - 0.5f, bar.getY() - 3.0f, 1.0f, bar.getHeight() + 6.0f);

    const float pan = proc.uiPan.load();
    g.setColour (kOrange);
    g.fillEllipse (bar.getCentreX() + pan * bar.getWidth() * 0.5f - 7.0f, bar.getCentreY() - 7.0f, 14.0f, 14.0f);

    g.setFont (fontOf (11.0f, true));
    g.setColour (kGrey);
    g.drawText ("L", (int) r.getX() + 12, (int) bar.getY() - 3, 20, 16, juce::Justification::centred);
    g.drawText ("R", (int) r.getRight() - 32, (int) bar.getY() - 3, 20, 16, juce::Justification::centred);
}

void TensionEditor::drawEqDisplay (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const auto r = area.toFloat();
    g.setColour (kBg);
    g.fillRoundedRectangle (r, 4.0f);

    const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    const float range = 15.0f;

    auto dbToY = [&] (float db) { return r.getCentreY() - juce::jlimit (-range, range, db) / range * (r.getHeight() * 0.5f - 4.0f); };
    auto fToX  = [&] (float f)  { return freqToX (f, r); };

    drawFreqGrid (g, r);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (float db : { -12.0f, -6.0f, 6.0f, 12.0f })
        g.drawHorizontalLine (juce::roundToInt (dbToY (db)), r.getX(), r.getRight());
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawHorizontalLine (juce::roundToInt (dbToY (0.0f)), r.getX(), r.getRight());

    const float lowDb = param ("eqlow"), midDb = param ("eqmid"), midF = param ("eqmidfreq"), highDb = param ("eqhigh");
    const auto lc = tr::makeLowShelf   (sr, 120.0, 0.707, juce::Decibels::decibelsToGain ((double) lowDb));
    const auto mc = tr::makePeak       (sr, midF,  0.9,   juce::Decibels::decibelsToGain ((double) midDb));
    const auto hc = tr::makeHighShelf  (sr, 6000.0, 0.707, juce::Decibels::decibelsToGain ((double) highDb));

    const bool lcOn = param ("lcon") > 0.5f, hcOn = param ("hcon") > 0.5f;
    const float lcF = param ("lcfreq"), hcF = param ("hcfreq");
    const int lcSlope = (int) param ("lcslope"), hcSlope = (int) param ("hcslope");

    auto totalDb = [&] (float f)
    {
        double m = tr::magnitudeAt (lc, f, sr) * tr::magnitudeAt (mc, f, sr) * tr::magnitudeAt (hc, f, sr);
        if (lcOn) m *= tr::cutMagnitude (true,  lcF, lcSlope, f, sr);
        if (hcOn) m *= tr::cutMagnitude (false, hcF, hcSlope, f, sr);
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
        g.setColour (kBg);
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colours::white);
        g.drawEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f, 1.5f);
    }

    // Marcas dos cortes ativos
    g.setFont (fontOf (10.0f, true));
    if (lcOn)
    {
        g.setColour (juce::Colour (0xff4aa3ff).withAlpha (0.7f));
        g.drawVerticalLine (juce::roundToInt (fToX (lcF)), r.getY(), r.getBottom());
        g.drawText ("LC", juce::roundToInt (fToX (lcF)) + 3, (int) r.getY() + 3, 20, 12, juce::Justification::centredLeft);
    }
    if (hcOn)
    {
        g.setColour (juce::Colour (0xff4aa3ff).withAlpha (0.7f));
        g.drawVerticalLine (juce::roundToInt (fToX (hcF)), r.getY(), r.getBottom());
        g.drawText ("HC", juce::roundToInt (fToX (hcF)) - 23, (int) r.getY() + 3, 20, 12, juce::Justification::centredRight);
    }

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawText ("+12", (int) r.getX() + 4, juce::roundToInt (dbToY (12.0f)) - 12, 30, 12, juce::Justification::centredLeft);
    g.drawText ("-12", (int) r.getX() + 4, juce::roundToInt (dbToY (-12.0f)), 30, 12, juce::Justification::centredLeft);
}

void TensionEditor::paint (juce::Graphics& g)
{
    const float b = proc.uiBuild.load();
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

    // Barra de abas: modo e sincronia
    g.setColour (kGrey);
    g.setFont (fontOf (10.5f, true));
    g.drawText ("MODE", 410, 70, 40, 24, juce::Justification::centredLeft);
    drawSyncStatus (g, syncArea);

    switch (page)
    {
        case 0:
            drawPanel (g, panelBuild,   "BUILD");
            drawPanel (g, panelDisplay, "TENSION CURVE");
            drawPanel (g, panelFx,      "TENSION FX");
            drawPanel (g, panelOut,     "OUTPUT MIX");
            drawPanel (g, panelEq,      "EQUALIZER");
            drawTensionDisplay (g, displayArea, b, accent);
            drawEqDisplay (g, eqDisplayArea);
            break;

        case 1:
        {
            drawPanel (g, panelEngine, "BUILD ENGINE");
            drawPanel (g, panelGate,   "RHYTHMIC GATE");
            drawEngineDisplay (g, engineDisplayArea, accent);
            drawGateDisplay (g, gateDisplayArea, accent);

            g.setColour (kGrey.withAlpha (0.85f));
            g.setFont (fontOf (11.0f));
            g.drawFittedText ("Accelerate: a divisao fica 2x e 4x mais rapida conforme o Build sobe. "
                              "Sync desligado usa uma taxa livre em Hz.", gateHintArea, juce::Justification::topLeft, 4);
            break;
        }

        default:
            drawPanel (g, panelPan, "AUTO PAN & STEREO");
            drawPanel (g, panelCut, "LOW CUT / HIGH CUT");
            drawPanDisplay (g, panDisplayArea);
            drawEqDisplay (g, cutDisplayArea);
            break;
    }
}
