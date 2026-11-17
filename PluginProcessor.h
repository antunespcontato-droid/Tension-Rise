#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

class TensionProcessor : public juce::AudioProcessor
{
public:
    TensionProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tension Rise"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    // Presets de fábrica (aparecem como "programas" no host e no menu do plugin)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float value);
    void updateEq (float lowDb, float midDb, float midFreq, float highDb);

    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    juce::dsp::StateVariableTPTFilter<float> hpf, noiseFilter;
    juce::dsp::IIR::Filter<float> eqLow[2], eqMid[2], eqHigh[2];
    juce::Reverb reverb;
    juce::Random rng;
    juce::SmoothedValue<float> build;

    float lfoPhase = 0.0f;
    double sampleRate = 44100.0;
    int currentPreset = 0;
    float lastEq[4] = { -999.f, -999.f, -999.f, -999.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TensionProcessor)
};
