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

    void addKnob (Knob&, const juce::String& paramId, const juce::String& text);
    static void placeKnob (Knob&, juce::Rectangle<int> area);
    void timerCallback() override;
    void stepPreset (int delta);

    float param (const char* id) const;
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;
    void drawTensionDisplay (juce::Graphics&, juce::Rectangle<int>, float build, juce::Colour accent) const;
    void drawEqDisplay (juce::Graphics&, juce::Rectangle<int>) const;
    void drawMeters (juce::Graphics&, juce::Rectangle<int>) const;

    TensionProcessor& proc;
    TellusLookAndFeel laf;
    juce::Image logo;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };

    juce::Rectangle<int> logoArea, meterArea;
    juce::Rectangle<int> panelBuild, panelDisplay, panelFx, panelOut, panelEq;
    juce::Rectangle<int> displayArea, eqDisplayArea;

    float meterL = 0.0f, meterR = 0.0f;

    Knob buildK, filterK, noiseK, reverbK, stutterK, driveK;
    Knob mixK, outputK;
    Knob eqLowK, eqMidK, eqFreqK, eqHighK;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TensionEditor)
};
