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

    TensionProcessor& proc;
    TellusLookAndFeel laf;
    juce::Image logo;
    juce::Rectangle<int> logoArea;
    juce::ComboBox presetBox;
    float lastBuild = -1.0f;

    Knob buildK, filterK, noiseK, reverbK, stutterK, driveK;
    Knob eqLowK, eqMidK, eqFreqK, eqHighK;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TensionEditor)
};
