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

// Editor de 16 passos para a forma de onda "Custom" do LFO (clicar e arrastar)
class StepEditor : public juce::Component
{
public:
    StepEditor (juce::AudioProcessorValueTreeState& state, const std::atomic<float>& phaseRef);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void setFromMouse (const juce::MouseEvent&);
    void setStep (int index, float value);

    juce::AudioProcessorValueTreeState& apvts;
    const std::atomic<float>& phase;
    int lastIdx = -1;
    float lastVal = 0.0f;
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

    // page: 0 MASTER, 1 ENGINE, 2 RHYTHM, 3 SPACE, 4 FILTER/PAN, -1 sempre visível
    void addKnob   (Knob&,   const juce::String& paramId, const juce::String& text, int page);
    void addCombo  (Combo&,  const juce::String& paramId, const juce::String& text, int page);
    void addToggle (Toggle&, const juce::String& paramId, const juce::String& text, int page);
    static void placeKnob  (Knob&,  juce::Rectangle<int> area);
    static void placeCombo (Combo&, juce::Rectangle<int> area);

    void setPage (int newPage);
    void timerCallback() override;
    void stepPreset (int delta);

    float param (const juce::String& id) const;
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;
    void drawSyncStatus (juce::Graphics&, juce::Rectangle<int>) const;
    void drawMeters (juce::Graphics&, juce::Rectangle<int>) const;
    void drawTensionDisplay (juce::Graphics&, juce::Rectangle<int>, float build, juce::Colour accent) const;
    void drawEngineDisplay (juce::Graphics&, juce::Rectangle<int>, juce::Colour accent) const;
    void drawGateDisplay (juce::Graphics&, juce::Rectangle<int>, juce::Colour accent) const;
    void drawPanDisplay (juce::Graphics&, juce::Rectangle<int>) const;
    void drawEqDisplay (juce::Graphics&, juce::Rectangle<int>) const;
    void drawLfoDisplay (juce::Graphics&, juce::Rectangle<int>, juce::Colour accent) const;

    TensionProcessor& proc;
    TellusLookAndFeel laf;
    juce::Image logo;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton tabBtn[5];
    std::vector<juce::Component*> pageComps[5];
    int page = 0;

    juce::Rectangle<int> logoArea, meterArea, syncArea;
    juce::Rectangle<int> panelBuild, panelDisplay, panelFx, panelOut, panelEq;
    juce::Rectangle<int> panelEngine, panelGate, panelPan, panelCut;
    juce::Rectangle<int> panelBR, panelSR, panelDelay, panelRev, panelLfo;
    juce::Rectangle<int> displayArea, eqDisplayArea, engineDisplayArea, gateDisplayArea, panDisplayArea, cutDisplayArea;
    juce::Rectangle<int> gateHintArea, lfoDisplayArea, sampleNameArea;

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
    // RHYTHM
    Knob brTimeK, brAccelK, brIntensityK, brFeedbackK, brMixK, brVarK, brStartK;
    Knob srTimeK, srVelK, srIntensityK, srVolK, srAccelK, srVarK, srMixK, srStartK;
    Combo brRateC, srRateC;
    Toggle brOnT, brSyncT, srOnT, srSyncT;
    juce::TextButton loadBtn { "LOAD SAMPLE" }, clearBtn { "CLEAR" };
    std::unique_ptr<juce::FileChooser> chooser;
    // SPACE
    Knob dlTimeK, dlFbK, dlRiseK, dlFilterK, dlStereoK, dlMixK;
    Knob revAmountK, revSizeK, revDampK, revWidthK, revPreK;
    Knob lfoHzK, lfoAmountK, lfoPhaseK;
    Combo dlRateC, lfoDestC, lfoShapeC, lfoRateC;
    Toggle dlOnT, dlSyncT, dlFollowT, lfoSyncT, lfoRetrigT;
    StepEditor stepEditor;
    // Cabeçalho
    Combo modeC;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TensionEditor)
};
