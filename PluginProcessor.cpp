#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Presets de fábrica. O knob "Build" NÃO é alterado pelos presets: ele é o
// macro que você automatiza no Ableton.
namespace
{
    struct Preset
    {
        const char* name;
        float filter, noise, reverb, stutter, drive;
        float eqLow, eqMid, eqMidFreq, eqHigh;
    };

    const Preset presets[] =
    {
        //  nome                filt  noise rev   stut  drive  low   mid   freq    high
        { "Init",              0.80f, 0.50f, 0.50f, 0.30f, 0.30f,  0.f,  0.f, 1000.f,  0.f },
        { "Classic Riser",     1.00f, 0.80f, 0.40f, 0.00f, 0.20f, -2.f,  0.f, 1000.f,  3.f },
        { "EDM Drop Tension",  0.90f, 0.60f, 0.70f, 0.60f, 0.50f, -3.f,  2.f, 2500.f,  4.f },
        { "Dark Techno",       0.70f, 0.30f, 0.50f, 0.40f, 0.80f,  3.f, -2.f,  500.f, -3.f },
        { "Cinematic Swell",   0.60f, 0.70f, 1.00f, 0.00f, 0.10f,  1.f,  0.f,  800.f,  2.f },
        { "Stutter Panic",     0.50f, 0.20f, 0.30f, 1.00f, 0.60f,  0.f,  3.f, 1800.f,  1.f },
        { "Lo-fi Lift",        0.60f, 0.40f, 0.60f, 0.20f, 0.90f,  2.f, -3.f, 1200.f, -6.f },
        { "Noise Storm",       0.80f, 1.00f, 0.80f, 0.50f, 0.70f, -4.f,  1.f, 3500.f,  5.f },
    };

    constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));
}

//==============================================================================
TensionProcessor::TensionProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout TensionProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const Attr pct = Attr()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; });

    const Attr db = Attr()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });

    const Attr hz = Attr()
        .withStringFromValueFunction ([] (float v, int)
        {
            return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                : juce::String (juce::roundToInt (v)) + " Hz";
        })
        .withValueFromStringFunction ([] (const juce::String& t)
        {
            const float v = t.getFloatValue();
            return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
        });

    const juce::NormalisableRange<float> unit (0.0f, 1.0f, 0.001f);
    const juce::NormalisableRange<float> gain (-12.0f, 12.0f, 0.1f);
    juce::NormalisableRange<float> freq (200.0f, 5000.0f, 1.0f);
    freq.setSkewForCentre (1000.0f);

    layout.add (std::make_unique<P> (juce::ParameterID { "build",     1 }, "Build",        unit, 0.0f, pct));
    layout.add (std::make_unique<P> (juce::ParameterID { "filter",    1 }, "Filter Sweep", unit, 0.8f, pct));
    layout.add (std::make_unique<P> (juce::ParameterID { "noise",     1 }, "Noise Riser",  unit, 0.5f, pct));
    layout.add (std::make_unique<P> (juce::ParameterID { "reverb",    1 }, "Reverb Wash",  unit, 0.5f, pct));
    layout.add (std::make_unique<P> (juce::ParameterID { "stutter",   1 }, "Stutter",      unit, 0.3f, pct));
    layout.add (std::make_unique<P> (juce::ParameterID { "drive",     1 }, "Drive",        unit, 0.3f, pct));

    layout.add (std::make_unique<P> (juce::ParameterID { "eqlow",     1 }, "EQ Low",       gain, 0.0f,    db));
    layout.add (std::make_unique<P> (juce::ParameterID { "eqmid",     1 }, "EQ Mid",       gain, 0.0f,    db));
    layout.add (std::make_unique<P> (juce::ParameterID { "eqmidfreq", 1 }, "EQ Mid Freq",  freq, 1000.0f, hz));
    layout.add (std::make_unique<P> (juce::ParameterID { "eqhigh",    1 }, "EQ High",      gain, 0.0f,    db));
    return layout;
}

//==============================================================================
int TensionProcessor::getNumPrograms() { return numPresets; }

const juce::String TensionProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, numPresets) ? juce::String (presets[index].name) : juce::String();
}

void TensionProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void TensionProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, numPresets))
        return;

    currentPreset = index;
    const auto& p = presets[index];

    setParam ("filter",    p.filter);
    setParam ("noise",     p.noise);
    setParam ("reverb",    p.reverb);
    setParam ("stutter",   p.stutter);
    setParam ("drive",     p.drive);
    setParam ("eqlow",     p.eqLow);
    setParam ("eqmid",     p.eqMid);
    setParam ("eqmidfreq", p.eqMidFreq);
    setParam ("eqhigh",    p.eqHigh);
}

//==============================================================================
bool TensionProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void TensionProcessor::updateEq (float lowDb, float midDb, float midFreq, float highDb)
{
    auto lc = Coeffs::makeLowShelf  (sampleRate, 120.0f,  0.707f, juce::Decibels::decibelsToGain (lowDb));
    auto mc = Coeffs::makePeakFilter (sampleRate, midFreq, 0.9f,   juce::Decibels::decibelsToGain (midDb));
    auto hc = Coeffs::makeHighShelf (sampleRate, 6000.0f, 0.707f, juce::Decibels::decibelsToGain (highDb));

    for (int ch = 0; ch < 2; ++ch)
    {
        eqLow[ch].coefficients  = lc;
        eqMid[ch].coefficients  = mc;
        eqHigh[ch].coefficients = hc;
    }
}

void TensionProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };

    hpf.prepare (spec);
    hpf.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpf.reset();

    noiseFilter.prepare (spec);
    noiseFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    noiseFilter.reset();

    for (int ch = 0; ch < 2; ++ch)
    {
        eqLow[ch].reset();
        eqMid[ch].reset();
        eqHigh[ch].reset();
    }
    lastEq[0] = lastEq[1] = lastEq[2] = lastEq[3] = -999.0f; // força recalcular o EQ

    reverb.setSampleRate (sr);
    reverb.reset();

    build.reset (sr, 0.05);
    build.setCurrentAndTargetValue (apvts.getRawParameterValue ("build")->load());
    lfoPhase = 0.0f;
}

void TensionProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nCh = juce::jmin (buf.getNumChannels(), 2);
    const int n   = buf.getNumSamples();

    const float amtFilter  = apvts.getRawParameterValue ("filter")->load();
    const float amtNoise   = apvts.getRawParameterValue ("noise")->load();
    const float amtReverb  = apvts.getRawParameterValue ("reverb")->load();
    const float amtStutter = apvts.getRawParameterValue ("stutter")->load();
    const float amtDrive   = apvts.getRawParameterValue ("drive")->load();

    build.setTargetValue (apvts.getRawParameterValue ("build")->load());

    for (int i = 0; i < n; ++i)
    {
        const float b = build.getNextValue();

        // Atualiza os coeficientes dos filtros a cada 32 samples
        if ((i & 31) == 0)
        {
            hpf.setCutoffFrequency (20.0f * std::pow (400.0f, b * amtFilter)); // 20 Hz -> 8 kHz
            hpf.setResonance (0.7f + 2.0f * b * amtFilter);
            noiseFilter.setCutoffFrequency (300.0f + 8000.0f * b * b);
        }

        // Stutter / tremolo: acelera de 1 Hz até 16 Hz conforme o build sobe
        lfoPhase += (1.0f + 15.0f * b * b) / (float) sampleRate;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        const float lfo  = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * lfoPhase);
        const float trem = 1.0f - amtStutter * b * lfo;

        const float driveMix  = amtDrive * b;
        const float driveGain = 1.0f + 4.0f * b;
        const float noiseGain = amtNoise * b * b * 0.3f;

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buf.getWritePointer (ch);

            float x = hpf.processSample (ch, d[i]);
            x = (1.0f - driveMix) * x + driveMix * std::tanh (x * driveGain);
            x *= trem;

            const float nz = noiseFilter.processSample (ch, rng.nextFloat() * 2.0f - 1.0f) * noiseGain;
            d[i] = x + nz;
        }
    }

    if (nCh >= 2)
    {
        const float b = build.getCurrentValue();
        juce::Reverb::Parameters p;
        p.roomSize = 0.5f + 0.45f * b;
        p.damping  = 0.4f;
        p.wetLevel = amtReverb * b * 0.35f;
        p.dryLevel = 0.5f;   // o JUCE dobra esse valor internamente -> ganho seco unitário
        p.width    = 1.0f;
        reverb.setParameters (p);
        reverb.processStereo (buf.getWritePointer (0), buf.getWritePointer (1), n);
    }

    // EQ final (low shelf 120 Hz, bell ajustável, high shelf 6 kHz)
    const float eqL = apvts.getRawParameterValue ("eqlow")->load();
    const float eqM = apvts.getRawParameterValue ("eqmid")->load();
    const float eqF = apvts.getRawParameterValue ("eqmidfreq")->load();
    const float eqH = apvts.getRawParameterValue ("eqhigh")->load();

    if (eqL != lastEq[0] || eqM != lastEq[1] || eqF != lastEq[2] || eqH != lastEq[3])
    {
        updateEq (eqL, eqM, eqF, eqH);
        lastEq[0] = eqL; lastEq[1] = eqM; lastEq[2] = eqF; lastEq[3] = eqH;
    }

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float x = d[i];
            x = eqLow[ch].processSample (x);
            x = eqMid[ch].processSample (x);
            x = eqHigh[ch].processSample (x);
            d[i] = x;
        }
    }
}

juce::AudioProcessorEditor* TensionProcessor::createEditor()
{
    return new TensionEditor (*this);
}

void TensionProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void TensionProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TensionProcessor();
}
