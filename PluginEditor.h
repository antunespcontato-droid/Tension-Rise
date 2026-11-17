#pragma once
#include "PluginProcessor.h"

// Visual dos knobs: trilho escuro + arco laranja + ponteiro branco
class TellusLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TellusLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

class TensionEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit TensionEditor (TensionProcessor&);
    ~TensionEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Combo
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    struct Toggle
    {
        juce::TextButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    // page: 0 MASTER, 1 BUILD ENGINE, 2 FILTROS & ESTÉREO, -1 sempre visível
    void addKnob   (Knob&,   const juce::String& paramId, const juce::String& text, int page);
    void addCombo  (Combo&,  const juce::String& paramId, const juce::String& text, int page);
    void addToggle (Toggle&, const juce::String& paramId, const juce::String& text, int page);
    static void placeKnob  (Knob&,  juce::Rectangle<int> area);
    static void placeCombo (Combo&, juce::Rectangle<int> area);

    void setPage (int newPage);
    void timerCallback() override;
    void stepPreset (int delta);

    float param (const char* id) const;
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;
    void drawSyncStatus (juce::Graphics&, juce::Rectangle<int>) const;
    void drawMeters (juce::Graphics&, juce::Rectangle<int>) const;
    void drawTensionDisplay (juce::Graphics&, juce::Rectangle<int>, float build, juce::Colour accent) const;
    void drawEngineDisplay (juce::Graphics&, juce::Rectangle<int>, juce::Colour accent) const;
    void drawGateDisplay (juce::Graphics&, juce::Rectangle<int>, juce::Colour accent) const;
    void drawPanDisplay (juce::Graphics&, juce::Rectangle<int>) const;
    void drawEqDisplay (juce::Graphics&, juce::Rectangle<int>) const;

    TensionProcessor& proc;
    TellusLookAndFeel laf;
    juce::Image logo;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton tabBtn[3];
    std::vector<juce::Component*> pageComps[3];
    int page = 0;

    juce::Rectangle<int> logoArea, meterArea, syncArea;
    juce::Rectangle<int> panelBuild, panelDisplay, panelFx, panelOut, panelEq;
    juce::Rectangle<int> panelEngine, panelGate, panelPan, panelCut;
    juce::Rectangle<int> displayArea, eqDisplayArea, engineDisplayArea, gateDisplayArea, panDisplayArea, cutDisplayArea;
    juce::Rectangle<int> gateHintArea;

    float meterL = 0.0f, meterR = 0.0f;

    // MASTER
    Knob buildK, filterK, noiseK, reverbK, gateK, driveK, mixK, outputK;
    Knob eqLowK, eqMidK, eqFreqK, eqHighK;
    // BUILD ENGINE
    Knob startK, intensityK, gateDepthK;
    Combo lengthC, curveC, gateRateC;
    Toggle gateSyncT, gateAccelT;
    // FILTROS & ESTÉREO
    Knob panDepthK, panPhaseK, panWidthK, panHzK, lcFreqK, hcFreqK;
    Combo panShapeC, panRateC, lcSlopeC, hcSlopeC;
    Toggle panOnT, panSyncT, panBassT, lcOnT, hcOnT;
    // Cabeçalho
    Combo modeC;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TensionEditor)
};
