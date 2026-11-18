#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "DspUtil.h"
#include "RhythmEngines.h"

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
    double getTailLengthSeconds() const override { return 4.0; }

    // Presets de fábrica
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Sample do snare roll (thread da interface). Sem sample carregado, usa o snare interno.
    void loadSample (const juce::File& file);
    void clearSample();
    juce::String getSampleName() const { return sampleName; }

    juce::AudioProcessorValueTreeState apvts;

    // Estado publicado para a interface (somente leitura na UI)
    std::atomic<float>  outLevelL { 0.0f }, outLevelR { 0.0f };
    std::atomic<float>  uiBuild { 0.0f };        // build efetivo (já considera Manual/Auto/Hybrid)
    std::atomic<float>  uiBarFrac { 0.0f };      // posição dentro do compasso (0..1)
    std::atomic<float>  uiGateCycle { 0.25f };   // ciclo atual do gate, em tempos
    std::atomic<float>  uiPan { 0.0f };          // pan atual (-1..1)
    std::atomic<float>  uiLfoVal { 0.0f }, uiLfoPhase { 0.0f };
    std::atomic<double> uiBpm { 120.0 };
    std::atomic<double> uiBarPos { 0.0 };        // compasso atual, começando em 0
    std::atomic<int>    uiSync { 0 };            // 0 sem host, 1 parado, 2 tocando
    std::atomic<int>    uiNum { 4 }, uiDen { 4 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float value);
    void updateEq (float lowDb, float midDb, float midFreq, float highDb);
    void updateCuts (float lcFreq, int lcSlope, float hcFreq, int hcSlope);

    juce::dsp::StateVariableTPTFilter<float> hpf, noiseFilter, bassLP;
    juce::Reverb reverb;
    juce::Random rng;
    juce::SmoothedValue<float> mixSm, gainSm, panDepthSm, widthSm;
    juce::AudioBuffer<float> dryBuffer;

    // Motores da Fase 2
    tr::BeatRepeat beatRepeat;
    tr::SnareRoll snareRoll;
    tr::TempoDelay delayLine;
    std::vector<float> synthSnare;

    // Sample externo: 3 posições em rodízio, trocadas por um índice atômico
    juce::AudioBuffer<float> sampleSlots[3];
    double sampleSlotRate[3] = { 44100.0, 44100.0, 44100.0 };
    std::atomic<int> activeSlot { -1 };
    int slotCounter = 0, lastSlotSeen = -2;
    juce::AudioFormatManager formatManager;
    juce::String sampleName;

    // Áreas de trabalho (alocadas em prepareToPlay)
    std::atomic<float>* lfoStepParam[16] = {};   // ponteiros guardados uma vez (sem criar textos no áudio)
    std::vector<float> lfoBuf, buildBuf, revL, revR, preL, preR;
    int preSize = 1, preW = 0;

    // EQ e cortes (biquads próprios, sem alocação no áudio)
    tr::BiquadCoef eqLowC, eqMidC, eqHighC, lcC[4], hcC[4], dlLpC;
    tr::BiquadState eqLowS[2], eqMidS[2], eqHighS[2], lcS[2][4], hcS[2][4];
    int lcStages = 2, hcStages = 2;
    float lastEq[4] = { -999.f, -999.f, -999.f, -999.f };
    float lastCut[4] = { -999.f, -999.f, -999.f, -999.f };
    float lastDlFilter = -1.0f;

    float buildSm = 0.0f, gateGain = 1.0f, gatePhase = 0.0f;
    float panSm = 0.0f, panRnd = 0.0f, panPrevPhase = 0.0f, panPhaseFree = 0.0f;
    float lcAmt = 0.0f, hcAmt = 0.0f, dlOnSm = 0.0f;
    float lfoSm = 0.0f, lfoRnd = 0.0f, lfoPrevPhase = 0.0f, lfoPhaseFree = 0.0f;
    float buildCoef = 0.01f, gateCoef = 0.05f, panCoef = 0.01f, amtCoef = 0.01f, lfoCoef = 0.05f;
    bool panPrevActive = false, prevPlaying = false;
    double brFree = 0.0, srFree = 0.0;
    int64_t srLastIdx = INT64_MIN;

    double freePpq = 0.0;
    double sampleRate = 44100.0;
    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TensionProcessor)
};
