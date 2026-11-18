#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Presets de fábrica (psytrance progressivo). O knob Build e o Mode não são
// alterados por eles: Build é o macro que você automatiza no Ableton.
namespace
{
    struct Preset
    {
        const char* name;
        float filter, noise, reverb, gate, drive;
        float eqLow, eqMid, eqMidFreq, eqHigh;
        int curve, length, gateRate;
        bool gateAccel;
    };

    const Preset presets[] =
    {
        //  nome                    filt   noise  rev    gate   drive   low   mid   freq    high  curve len rate accel
        { "Init",                  0.80f, 0.50f, 0.50f, 0.30f, 0.30f,  0.f,  0.f, 1000.f,  0.f,    0,  1,  8, false },
        { "Prog Psy Rise",         0.90f, 0.50f, 0.50f, 0.15f, 0.25f, -2.f,  0.f, 1000.f,  2.f,    1,  2,  8, false },
        { "Forest Build",          0.75f, 0.40f, 0.90f, 0.10f, 0.15f,  0.f, -1.f,  800.f,  3.f,    2,  2,  5, false },
        { "Full-On Lift",          1.00f, 0.80f, 0.60f, 0.40f, 0.40f, -3.f,  2.f, 2500.f,  4.f,    1,  1,  8, true  },
        { "Snare Roll Tension",    0.60f, 0.90f, 0.40f, 1.00f, 0.50f, -4.f,  3.f, 3000.f,  3.f,    1,  0,  8, true  },
        { "Dark Psy Tension",      0.70f, 0.50f, 0.60f, 0.50f, 0.85f,  2.f, -2.f,  400.f, -2.f,    4,  1, 10, false },
        { "Hypnotic Rise",         0.85f, 0.30f, 0.80f, 0.35f, 0.30f, -1.f,  1.f, 1200.f,  1.f,    2,  3,  6, false },
        { "Pre-Drop Wash",         1.00f, 1.00f, 1.00f, 0.60f, 0.60f, -6.f,  0.f, 1000.f,  5.f,    3,  0, 11, true  },
    };

    constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));

    // Destinos do LFO
    enum LfoDest { dOff = 0, dFilter, dResonance, dPan, dWidth, dVolume, dDelayMix, dDelayFb, dDrive };
}

//==============================================================================
TensionProcessor::TensionProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();
    for (int i = 0; i < 16; ++i)
        lfoStepParam[i] = apvts.getRawParameterValue ("lfostep" + juce::String (i));
}

juce::AudioProcessorValueTreeState::ParameterLayout TensionProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;
    using ID = juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const Attr pct = Attr()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; });

    const Attr pctSigned = Attr()
        .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
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

    const Attr deg = Attr()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 360.0f)) + juce::String::fromUTF8 ("\xc2\xb0"); })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 360.0f; });

    const Attr hzSlow = Attr()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " Hz"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });

    const Attr ms = Attr()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });

    const Attr hidden = Attr().withAutomatable (false);

    const juce::NormalisableRange<float> unit (0.0f, 1.0f, 0.001f);
    const juce::NormalisableRange<float> wide (0.0f, 2.0f, 0.001f);
    const juce::NormalisableRange<float> bipolar (-1.0f, 1.0f, 0.001f);
    const juce::NormalisableRange<float> gain (-12.0f, 12.0f, 0.1f);
    const juce::NormalisableRange<float> fbRange (0.0f, 0.92f, 0.001f);
    const juce::NormalisableRange<float> preRange (0.0f, 100.0f, 1.0f);
    juce::NormalisableRange<float> freq (200.0f, 5000.0f, 1.0f);       freq.setSkewForCentre (1000.0f);
    juce::NormalisableRange<float> lcRange (20.0f, 500.0f, 1.0f);      lcRange.setSkewForCentre (100.0f);
    juce::NormalisableRange<float> hcRange (2000.0f, 20000.0f, 1.0f);  hcRange.setSkewForCentre (8000.0f);
    juce::NormalisableRange<float> panHz (0.05f, 10.0f, 0.01f);        panHz.setSkewForCentre (1.0f);
    juce::NormalisableRange<float> lfoHz (0.05f, 20.0f, 0.01f);        lfoHz.setSkewForCentre (2.0f);
    juce::NormalisableRange<float> msBr (20.0f, 1000.0f, 1.0f);        msBr.setSkewForCentre (200.0f);
    juce::NormalisableRange<float> msSr (20.0f, 500.0f, 1.0f);         msSr.setSkewForCentre (120.0f);
    juce::NormalisableRange<float> msDl (20.0f, 1500.0f, 1.0f);        msDl.setSkewForCentre (350.0f);
    juce::NormalisableRange<float> dlFilt (500.0f, 16000.0f, 1.0f);    dlFilt.setSkewForCentre (3500.0f);

    juce::StringArray divisions;
    for (int i = 0; i < tr::kNumDivisions; ++i)
        divisions.add (tr::divisionName (i));

    // Macro e efeitos principais
    layout.add (std::make_unique<F> (ID { "build",     1 }, "Build",        unit, 0.0f, pct));
    layout.add (std::make_unique<F> (ID { "filter",    1 }, "Filter Sweep", unit, 0.8f, pct));
    layout.add (std::make_unique<F> (ID { "noise",     1 }, "Noise Riser",  unit, 0.5f, pct));
    layout.add (std::make_unique<F> (ID { "reverb",    1 }, "Reverb Wash",  unit, 0.5f, pct));
    layout.add (std::make_unique<F> (ID { "stutter",   1 }, "Gate Depth",   unit, 0.3f, pct));
    layout.add (std::make_unique<F> (ID { "drive",     1 }, "Drive",        unit, 0.3f, pct));

    // EQ
    layout.add (std::make_unique<F> (ID { "eqlow",     1 }, "EQ Low",       gain, 0.0f,    db));
    layout.add (std::make_unique<F> (ID { "eqmid",     1 }, "EQ Mid",       gain, 0.0f,    db));
    layout.add (std::make_unique<F> (ID { "eqmidfreq", 1 }, "EQ Mid Freq",  freq, 1000.0f, hz));
    layout.add (std::make_unique<F> (ID { "eqhigh",    1 }, "EQ High",      gain, 0.0f,    db));

    // Saída
    layout.add (std::make_unique<F> (ID { "mix",       1 }, "Mix",          unit, 1.0f,    pct));
    layout.add (std::make_unique<F> (ID { "output",    1 }, "Output",       gain, 0.0f,    db));

    // Build Engine
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "mode",   1 }, "Mode",
                juce::StringArray { "Manual", "Auto", "Hybrid" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "length", 1 }, "Build Length",
                juce::StringArray { "8 bars", "16 bars", "32 bars", "64 bars" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "curve",  1 }, "Tension Curve",
                juce::StringArray { "Linear Rise", "Exponential Rise", "Slow Rise", "Fast Rise", "Stepped Rise" }, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (ID { "startbar", 1 }, "Start Bar", 1, 512, 1,
                juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return "Bar " + juce::String (v); })
                                                   .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("0123456789").getIntValue(); })));
    layout.add (std::make_unique<F> (ID { "intensity", 1 }, "Build Intensity", unit, 1.0f, pct));

    // Gate rítmico
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "gaterate", 1 }, "Gate Rate", divisions, 8));
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "gatesync",  1 }, "Gate Sync", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "gateaccel", 1 }, "Gate Accelerate", false));

    // Auto Pan e largura estéreo
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "panon",    1 }, "Auto Pan", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "panshape", 1 }, "Pan Shape",
                juce::StringArray { "Sine", "Triangle", "Square", "Random" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "panrate",  1 }, "Pan Rate", divisions, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "pansync",  1 }, "Pan Sync", true));
    layout.add (std::make_unique<F> (ID { "panhz",    1 }, "Pan Rate (Hz)", panHz, 0.5f, hzSlow));
    layout.add (std::make_unique<F> (ID { "pandepth", 1 }, "Pan Depth",     unit, 0.5f, pct));
    layout.add (std::make_unique<F> (ID { "panphase", 1 }, "Pan Phase",     unit, 0.0f, deg));
    layout.add (std::make_unique<F> (ID { "panwidth", 1 }, "Stereo Width",  wide, 1.0f, pct));
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "panbass",  1 }, "Bass Mono", true));

    // Low Cut / High Cut
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "lcon", 1 }, "Low Cut", false));
    layout.add (std::make_unique<F> (ID { "lcfreq", 1 }, "Low Cut Freq", lcRange, 30.0f, hz));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "lcslope", 1 }, "Low Cut Slope",
                juce::StringArray { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "hcon", 1 }, "High Cut", false));
    layout.add (std::make_unique<F> (ID { "hcfreq", 1 }, "High Cut Freq", hcRange, 16000.0f, hz));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "hcslope", 1 }, "High Cut Slope",
                juce::StringArray { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 1));

    // --- Fase 2: Beat Repeat
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "bron",     1 }, "Beat Repeat", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "brrate", 1 }, "Beat Repeat Rate", divisions, 5));
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "brsync",   1 }, "Beat Repeat Sync", true));
    layout.add (std::make_unique<F> (ID { "brtime",      1 }, "Beat Repeat Time",      msBr, 125.0f, ms));
    layout.add (std::make_unique<F> (ID { "braccel",     1 }, "Beat Repeat Accel",     unit, 0.5f, pct));
    layout.add (std::make_unique<F> (ID { "brintensity", 1 }, "Beat Repeat Intensity", unit, 1.0f, pct));
    layout.add (std::make_unique<F> (ID { "brfeedback",  1 }, "Beat Repeat Feedback",  unit, 0.8f, pct));
    layout.add (std::make_unique<F> (ID { "brmix",       1 }, "Beat Repeat Mix",       unit, 1.0f, pct));
    layout.add (std::make_unique<F> (ID { "brvar",       1 }, "Beat Repeat Variation", unit, 0.2f, pct));
    layout.add (std::make_unique<F> (ID { "brstart",     1 }, "Beat Repeat Start",     unit, 0.5f, pct));

    // --- Fase 2: Snare Roll
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "sron",     1 }, "Snare Roll", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "srrate", 1 }, "Snare Roll Rate", divisions, 8));
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "srsync",   1 }, "Snare Roll Sync", true));
    layout.add (std::make_unique<F> (ID { "srtime",      1 }, "Snare Roll Time",      msSr, 125.0f, ms));
    layout.add (std::make_unique<F> (ID { "srvel",       1 }, "Snare Roll Velocity",  unit, 0.4f, pct));
    layout.add (std::make_unique<F> (ID { "srintensity", 1 }, "Snare Roll Intensity", unit, 0.7f, pct));
    layout.add (std::make_unique<F> (ID { "srvol",       1 }, "Snare Roll Volume",    unit, 0.8f, pct));
    layout.add (std::make_unique<F> (ID { "sraccel",     1 }, "Snare Roll Accel",     unit, 0.5f, pct));
    layout.add (std::make_unique<F> (ID { "srvar",       1 }, "Snare Roll Variation", unit, 0.2f, pct));
    layout.add (std::make_unique<F> (ID { "srmix",       1 }, "Snare Roll Mix",       unit, 0.8f, pct));
    layout.add (std::make_unique<F> (ID { "srstart",     1 }, "Snare Roll Start",     unit, 0.5f, pct));

    // --- Fase 2: Delay
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "dlon",     1 }, "Delay", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "dlsync",   1 }, "Delay Sync", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "dlrate", 1 }, "Delay Division", divisions, 6));
    layout.add (std::make_unique<F> (ID { "dlms",     1 }, "Delay Time",        msDl,   375.0f, ms));
    layout.add (std::make_unique<F> (ID { "dlfb",     1 }, "Delay Feedback",    fbRange, 0.45f, pct));
    layout.add (std::make_unique<F> (ID { "dlrise",   1 }, "Delay Feedback Rise", unit, 0.3f,  pct));
    layout.add (std::make_unique<F> (ID { "dlfilter", 1 }, "Delay Filter",      dlFilt, 6000.0f, hz));
    layout.add (std::make_unique<F> (ID { "dlstereo", 1 }, "Delay Stereo",      unit,   0.6f,  pct));
    layout.add (std::make_unique<F> (ID { "dlmix",    1 }, "Delay Mix",         unit,   0.35f, pct));
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "dlfollow", 1 }, "Delay Follow Build", true));

    // --- Fase 2: Reverb
    layout.add (std::make_unique<F> (ID { "revsize",  1 }, "Reverb Size",       unit,   0.5f, pct));
    layout.add (std::make_unique<F> (ID { "revdamp",  1 }, "Reverb Damping",    unit,   0.4f, pct));
    layout.add (std::make_unique<F> (ID { "revwidth", 1 }, "Reverb Width",      unit,   1.0f, pct));
    layout.add (std::make_unique<F> (ID { "revpre",   1 }, "Reverb Pre-delay",  preRange, 0.0f, ms));

    // --- Fase 2: LFO
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "lfodest", 1 }, "LFO Destination",
                juce::StringArray { "Off", "Filter Cutoff", "Resonance", "Pan", "Stereo Width", "Volume", "Delay Mix", "Delay Feedback", "Drive" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "lfoshape", 1 }, "LFO Shape",
                juce::StringArray { "Sine", "Triangle", "Square", "Saw Up", "Saw Down", "Random", "Custom" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ID { "lforate", 1 }, "LFO Rate", divisions, 2));
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "lfosync",   1 }, "LFO Sync", true));
    layout.add (std::make_unique<F> (ID { "lfohz",     1 }, "LFO Rate (Hz)", lfoHz,  2.0f, hzSlow));
    layout.add (std::make_unique<F> (ID { "lfoamount", 1 }, "LFO Amount",    bipolar, 0.5f, pctSigned));
    layout.add (std::make_unique<F> (ID { "lfophase",  1 }, "LFO Phase",     unit,   0.0f, deg));
    layout.add (std::make_unique<juce::AudioParameterBool> (ID { "lforetrig", 1 }, "LFO Retrigger", true));
    for (int i = 0; i < 16; ++i)
        layout.add (std::make_unique<F> (ID { "lfostep" + juce::String (i), 1 }, "LFO Step " + juce::String (i + 1),
                                         bipolar, 1.0f - 2.0f * (float) i / 15.0f, hidden));
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
    setParam ("stutter",   p.gate);
    setParam ("drive",     p.drive);
    setParam ("eqlow",     p.eqLow);
    setParam ("eqmid",     p.eqMid);
    setParam ("eqmidfreq", p.eqMidFreq);
    setParam ("eqhigh",    p.eqHigh);
    setParam ("curve",     (float) p.curve);
    setParam ("length",    (float) p.length);
    setParam ("gaterate",  (float) p.gateRate);
    setParam ("gateaccel", p.gateAccel ? 1.0f : 0.0f);
}

//==============================================================================
void TensionProcessor::loadSample (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return;

    const int len = (int) juce::jmin ((juce::int64) (reader->sampleRate * 4.0), reader->lengthInSamples);  // até 4 s
    const int next = (slotCounter + 1) % 3;

    auto& buf = sampleSlots[next];
    buf.setSize (2, len);
    buf.clear();
    reader->read (&buf, 0, len, 0, true, true);

    const float peak = juce::jmax (buf.getMagnitude (0, 0, len), buf.getMagnitude (1, 0, len));
    if (peak > 1.0e-4f)
        buf.applyGain (0.9f / peak);   // normaliza o pico

    sampleSlotRate[next] = reader->sampleRate;
    slotCounter = next;
    sampleName = file.getFileName();
    apvts.state.setProperty ("samplePath", file.getFullPathName(), nullptr);
    activeSlot.store (next);
}

void TensionProcessor::clearSample()
{
    activeSlot.store (-1);
    sampleName = {};
    apvts.state.setProperty ("samplePath", "", nullptr);
}

//==============================================================================
bool TensionProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void TensionProcessor::updateEq (float lowDb, float midDb, float midFreq, float highDb)
{
    eqLowC  = tr::makeLowShelf  (sampleRate, 120.0,  0.707, juce::Decibels::decibelsToGain ((double) lowDb));
    eqMidC  = tr::makePeak      (sampleRate, midFreq, 0.9,   juce::Decibels::decibelsToGain ((double) midDb));
    eqHighC = tr::makeHighShelf (sampleRate, 6000.0, 0.707, juce::Decibels::decibelsToGain ((double) highDb));
}

void TensionProcessor::updateCuts (float lcFreq, int lcSlope, float hcFreq, int hcSlope)
{
    const int lcOrder = tr::slopeOrder (lcSlope), hcOrder = tr::slopeOrder (hcSlope);
    lcStages = lcOrder / 2;
    hcStages = hcOrder / 2;

    for (int s = 0; s < lcStages; ++s) lcC[s] = tr::makeHighPass (sampleRate, lcFreq, tr::butterworthQ (lcOrder, s));
    for (int s = 0; s < hcStages; ++s) hcC[s] = tr::makeLowPass  (sampleRate, hcFreq, tr::butterworthQ (hcOrder, s));
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

    bassLP.prepare (spec);
    bassLP.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    bassLP.setCutoffFrequency (150.0f);
    bassLP.reset();

    for (int ch = 0; ch < 2; ++ch)
    {
        eqLowS[ch].reset(); eqMidS[ch].reset(); eqHighS[ch].reset();
        for (int s = 0; s < 4; ++s) { lcS[ch][s].reset(); hcS[ch][s].reset(); }
    }
    for (float& v : lastEq)  v = -999.0f;
    for (float& v : lastCut) v = -999.0f;
    lastDlFilter = -1.0f;

    reverb.setSampleRate (sr);
    reverb.reset();

    beatRepeat.prepare (sr);
    snareRoll.reset();
    delayLine.prepare (sr);
    synthSnare = tr::makeSnare (sr);
    lastSlotSeen = -2;

    const int maxBlock = juce::jmax (samplesPerBlock, 64);
    lfoBuf.assign ((size_t) maxBlock, 0.0f);
    buildBuf.assign ((size_t) maxBlock, 0.0f);
    revL.assign ((size_t) maxBlock, 0.0f);
    revR.assign ((size_t) maxBlock, 0.0f);
    preSize = (int) (sr * 0.1) + 4;
    preL.assign ((size_t) preSize, 0.0f);
    preR.assign ((size_t) preSize, 0.0f);
    preW = 0;

    dryBuffer.setSize (2, maxBlock);
    mixSm.reset (sr, 0.02);
    mixSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("mix")->load());
    gainSm.reset (sr, 0.02);
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));
    panDepthSm.reset (sr, 0.02);
    panDepthSm.setCurrentAndTargetValue (0.0f);
    widthSm.reset (sr, 0.02);
    widthSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("panwidth")->load());

    auto coefFor = [sr] (double seconds) { return (float) (1.0 - std::exp (-1.0 / (seconds * sr))); };
    buildCoef = coefFor (0.010);
    gateCoef  = coefFor (0.002);
    panCoef   = coefFor (0.005);
    amtCoef   = coefFor (0.010);
    lfoCoef   = coefFor (0.002);

    buildSm = apvts.getRawParameterValue ("build")->load();
    gateGain = 1.0f; gatePhase = 0.0f; panSm = 0.0f; panPrevPhase = 0.0f; panPhaseFree = 0.0f;
    lcAmt = hcAmt = dlOnSm = 0.0f;
    lfoSm = 0.0f; lfoRnd = 0.0f; lfoPrevPhase = 0.0f; lfoPhaseFree = 0.0f;
    panPrevActive = false; prevPlaying = false;
    brFree = srFree = 0.0; srLastIdx = INT64_MIN;
    freePpq = 0.0;
}

void TensionProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nCh = juce::jmin (buf.getNumChannels(), 2);
    const int n   = buf.getNumSamples();
    if (nCh < 1 || n < 1)
        return;

    // Blocos maiores do que o previsto em prepareToPlay (raro): aumenta as áreas de trabalho uma vez
    if ((int) lfoBuf.size() < n)
    {
        lfoBuf.resize ((size_t) n); buildBuf.resize ((size_t) n);
        revL.resize ((size_t) n);   revR.resize ((size_t) n);
    }

    // --- Sincronia com o host (BPM, transporte, posição musical, fórmula de compasso)
    double bpm = 120.0, ppqHost = 0.0;
    bool hasHost = false, hasPpq = false, playing = false;
    int num = 4, den = 4;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hasHost = true;
            if (auto v = pos->getBpm())           bpm = *v;
            if (auto v = pos->getPpqPosition())   { ppqHost = *v; hasPpq = true; }
            if (auto v = pos->getTimeSignature()) { num = v->numerator; den = v->denominator; }
            playing = pos->getIsPlaying();
        }

    bpm = juce::jlimit (20.0, 400.0, bpm);
    if (num <= 0) num = 4;
    if (den <= 0) den = 4;

    const double beatsPerBar    = (double) num * 4.0 / (double) den;
    const double beatsPerSample = bpm / 60.0 / sampleRate;
    const bool   engineAdvances = hasPpq ? playing : true;               // parado: o build "congela" na posição do cursor
    const double enginePpq0     = hasPpq ? ppqHost : freePpq;
    const double phasePpq0      = (hasPpq && playing) ? ppqHost : freePpq; // gate, pan, repeat e LFO seguem a grade quando toca

    // --- Parâmetros
    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float manual    = P ("build");
    const int   mode      = (int) P ("mode");
    const int   lengthIdx = (int) P ("length");
    const int   curveIdx  = (int) P ("curve");
    const int   startBar  = (int) P ("startbar");
    const float intensity = P ("intensity");

    const float amtFilter = P ("filter"), amtNoise = P ("noise"), amtReverb = P ("reverb");
    const float amtGate   = P ("stutter"), amtDrive = P ("drive");
    const int   gateRate  = (int) P ("gaterate");
    const bool  gateSync  = P ("gatesync") > 0.5f, gateAccel = P ("gateaccel") > 0.5f;

    // Beat Repeat
    const bool  brOn = P ("bron") > 0.5f, brSync = P ("brsync") > 0.5f;
    const int   brRate = (int) P ("brrate");
    const float brTimeMs = P ("brtime"), brAccel = P ("braccel"), brIntensity = P ("brintensity");
    const float brFeedback = P ("brfeedback"), brMix = P ("brmix"), brVar = P ("brvar"), brStart = P ("brstart");

    // Snare Roll
    const bool  srOn = P ("sron") > 0.5f, srSync = P ("srsync") > 0.5f;
    const int   srRate = (int) P ("srrate");
    const float srTimeMs = P ("srtime"), srVel = P ("srvel"), srIntensity = P ("srintensity"), srVol = P ("srvol");
    const float srAccel = P ("sraccel"), srVar = P ("srvar"), srMix = P ("srmix"), srStart = P ("srstart");

    // LFO
    const int   lfoDest = (int) P ("lfodest"), lfoShape = (int) P ("lfoshape"), lfoRate = (int) P ("lforate");
    const bool  lfoSync = P ("lfosync") > 0.5f, lfoRetrig = P ("lforetrig") > 0.5f;
    const float lfoHz = P ("lfohz"), lfoAmount = P ("lfoamount"), lfoPhaseOff = P ("lfophase");
    float lfoSteps[16];
    if (lfoShape == 6)
        for (int i = 0; i < 16; ++i)
            lfoSteps[i] = lfoStepParam[i]->load();
    else
        for (float& s : lfoSteps) s = 0.0f;

    if (lfoRetrig && playing && ! prevPlaying)
        lfoPhaseFree = 0.0f;
    prevPlaying = playing;

    // Sample do snare roll (externo ou sintético)
    const int slot = activeSlot.load();
    if (slot != lastSlotSeen)
    {
        snareRoll.reset();
        lastSlotSeen = slot;
    }
    tr::SampleView sample;
    if (slot >= 0)
    {
        sample.ch[0]  = sampleSlots[slot].getReadPointer (0);
        sample.ch[1]  = sampleSlots[slot].getReadPointer (1);
        sample.length = sampleSlots[slot].getNumSamples();
        sample.ratio  = sampleSlotRate[slot] / sampleRate;
    }
    else
    {
        sample.ch[0]  = synthSnare.data();
        sample.length = (int) synthSnare.size();
        sample.ratio  = 1.0;
    }

    // Guarda o sinal original para o Mix (dry/wet)
    dryBuffer.setSize (2, n, false, false, true);
    for (int ch = 0; ch < nCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buf, ch, 0, n);

    float* d[2] = { buf.getWritePointer (0), nCh > 1 ? buf.getWritePointer (1) : nullptr };
    double gateCycleNow = tr::divisionBeats (gateRate);

    // --- Build Engine + LFO + beat repeat + gate + filtro + drive + ruído + snare roll
    for (int i = 0; i < n; ++i)
    {
        const double enginePpq = enginePpq0 + (engineAdvances ? (double) i * beatsPerSample : 0.0);
        const double phasePpq  = phasePpq0 + (double) i * beatsPerSample;

        float eff = manual;
        if (mode != 0)
        {
            const float eng = tr::buildAt (enginePpq / beatsPerBar, (double) (startBar - 1),
                                           tr::lengthBars (lengthIdx), curveIdx) * intensity;
            eff = (mode == 1) ? eng : juce::jlimit (0.0f, 1.0f, eng + manual);
        }
        buildSm += buildCoef * (eff - buildSm);
        const float b = buildSm;

        // LFO (uma onda, vários destinos)
        float lfoVal = 0.0f;
        if (lfoDest != dOff)
        {
            float ph;
            if (lfoSync)
            {
                double x = phasePpq / tr::divisionBeats (lfoRate) + (double) lfoPhaseOff;
                x -= std::floor (x);
                ph = (float) x;
            }
            else
            {
                lfoPhaseFree += lfoHz / (float) sampleRate;
                lfoPhaseFree -= std::floor (lfoPhaseFree);
                const float x = lfoPhaseFree + lfoPhaseOff;
                ph = x - std::floor (x);
            }
            if (ph < lfoPrevPhase)
                lfoRnd = rng.nextFloat() * 2.0f - 1.0f;
            lfoPrevPhase = ph;

            lfoSm += lfoCoef * (tr::lfoWave (lfoShape, ph, lfoRnd, lfoSteps) - lfoSm);
            lfoVal = lfoSm * lfoAmount;
        }
        lfoBuf[(size_t) i]   = lfoVal;
        buildBuf[(size_t) i] = b;

        if ((i & 31) == 0)
        {
            const float sweep = juce::jlimit (0.0f, 1.0f, b * amtFilter + (lfoDest == dFilter ? 0.5f * lfoVal : 0.0f));
            hpf.setCutoffFrequency (20.0f * std::pow (400.0f, sweep));                 // 20 Hz -> 8 kHz
            hpf.setResonance (juce::jlimit (0.5f, 4.0f, 0.7f + 2.0f * b * amtFilter + (lfoDest == dResonance ? 2.0f * lfoVal : 0.0f)));
            noiseFilter.setCutoffFrequency (300.0f + 8000.0f * b * b);
        }

        // Beat Repeat: engata quando o build chega ao ponto "Start"; a taxa acelera com o build
        float in[2] = { d[0][i], d[1] ? d[1][i] : d[0][i] };
        {
            const float prog = brStart < 0.999f ? juce::jlimit (0.0f, 1.0f, (b - brStart) / (1.0f - brStart)) : 0.0f;
            const int k = juce::jlimit (0, 3, (int) std::floor (brAccel * 4.0f * prog));
            const bool engaged = brOn && b >= brStart - 1.0e-6f;

            double cyc;
            int sliceLen;
            if (brSync)
            {
                const double cycleBeats = tr::divisionBeats (brRate) / (double) (1 << k);
                cyc = phasePpq / cycleBeats;
                sliceLen = (int) std::lround (cycleBeats * 60.0 / bpm * sampleRate);
            }
            else
            {
                const double lenSamples = juce::jmax (16.0, (double) brTimeMs * 0.001 * sampleRate / (double) (1 << k));
                cyc = brFree / lenSamples;
                brFree += 1.0;
                sliceLen = (int) lenSamples;
            }
            beatRepeat.process (in[0], in[1], engaged, cyc, sliceLen, brFeedback, brIntensity, brMix, brVar);
        }

        // Gate rítmico: preso à grade do host (ou livre, se Sync estiver desligado)
        bool open;
        if (gateSync)
        {
            double cycle = tr::divisionBeats (gateRate);
            if (gateAccel)
                cycle /= (double) (1 << juce::jmin (2, (int) (b * 3.0f)));   // 1x, 2x, 4x conforme o build sobe
            gateCycleNow = cycle;
            double ph = phasePpq / cycle;
            ph -= std::floor (ph);
            open = ph < 0.5;
        }
        else
        {
            gatePhase += (1.0f + 15.0f * b * b) / (float) sampleRate;
            if (gatePhase >= 1.0f) gatePhase -= 1.0f;
            open = gatePhase < 0.5f;
        }
        const float depthEff = amtGate * b;
        gateGain += gateCoef * ((open ? 1.0f : 1.0f - depthEff) - gateGain);

        const float driveMix  = juce::jlimit (0.0f, 1.0f, amtDrive * b + (lfoDest == dDrive ? 0.5f * lfoVal : 0.0f));
        const float driveGain = 1.0f + 4.0f * b;
        const float noiseGain = amtNoise * b * b * 0.3f;

        // Snare roll: dispara a cada divisão da grade enquanto o build estiver acima do ponto "Start"
        float roll[2] = { 0.0f, 0.0f };
        {
            const float prog = srStart < 0.999f ? juce::jlimit (0.0f, 1.0f, (b - srStart) / (1.0f - srStart)) : 0.0f;
            const int k = juce::jlimit (0, 3, (int) std::floor (srAccel * 4.0f * prog));
            const bool engaged = srOn && b >= srStart - 1.0e-6f;

            double cyc;
            if (srSync)
                cyc = phasePpq / (tr::divisionBeats (srRate) / (double) (1 << k));
            else
            {
                cyc = srFree / juce::jmax (16.0, (double) srTimeMs * 0.001 * sampleRate / (double) (1 << k));
                srFree += 1.0;
            }

            const int64_t idx = (int64_t) std::floor (cyc);
            if (idx != srLastIdx)
            {
                srLastIdx = idx;
                if (engaged)
                {
                    const float vel = juce::jlimit (0.0f, 1.5f, juce::jmap (srIntensity * prog, srVel, 1.0f)
                                                                 * (1.0f + srVar * 0.4f * (rng.nextFloat() * 2.0f - 1.0f))) * srVol;
                    snareRoll.trigger (vel, srVar * 0.5f * (rng.nextFloat() * 2.0f - 1.0f));
                }
            }
            snareRoll.render (roll[0], roll[1], sample);
        }

        for (int ch = 0; ch < nCh; ++ch)
        {
            float x = hpf.processSample (ch, in[ch]);
            x = (1.0f - driveMix) * x + driveMix * std::tanh (x * driveGain);
            x *= gateGain;
            const float nz = noiseFilter.processSample (ch, rng.nextFloat() * 2.0f - 1.0f) * noiseGain;
            d[ch][i] = x + nz + roll[ch] * srMix;
        }
    }

    // --- Delay (sincronizado ao BPM, com filtro e ping-pong no feedback)
    {
        const bool  dlOn = P ("dlon") > 0.5f, dlSync = P ("dlsync") > 0.5f, dlFollow = P ("dlfollow") > 0.5f;
        const int   dlRate = (int) P ("dlrate");
        const float dlMs = P ("dlms"), dlFb = P ("dlfb"), dlRise = P ("dlrise"), dlFilter = P ("dlfilter");
        const float dlStereo = P ("dlstereo"), dlMix = P ("dlmix");

        const double dlSamples = dlSync ? tr::divisionBeats (dlRate) * 60.0 / bpm * sampleRate
                                        : (double) dlMs * 0.001 * sampleRate;
        if (dlFilter != lastDlFilter)
        {
            delayLine.setLowPass (tr::makeLowPass (sampleRate, dlFilter, 0.707));
            lastDlFilter = dlFilter;
        }

        for (int i = 0; i < n; ++i)
        {
            dlOnSm += amtCoef * ((dlOn ? 1.0f : 0.0f) - dlOnSm);
            const float b = buildBuf[(size_t) i], lv = lfoBuf[(size_t) i];

            float mixEff = dlMix * (dlFollow ? b : 1.0f);                                  // "Follow Build"
            if (lfoDest == dDelayMix) mixEff = juce::jlimit (0.0f, 1.0f, mixEff + 0.5f * lv);

            float fbEff = dlFb + dlRise * b * (0.92f - dlFb);                              // feedback sobe com o build
            if (lfoDest == dDelayFb) fbEff += 0.4f * lv;
            fbEff = juce::jlimit (0.0f, 0.92f, fbEff);

            const float inL = d[0][i], inR = d[1] ? d[1][i] : d[0][i];
            float wl, wr;
            delayLine.process (inL * dlOnSm, inR * dlOnSm, dlSamples, fbEff, dlStereo, wl, wr);   // desligar só para de alimentar: a cauda termina sozinha
            d[0][i] += wl * mixEff;
            if (d[1]) d[1][i] += wr * mixEff;
        }
    }

    // --- Reverb (pré-delay, tamanho, damping, largura; só a parte molhada é somada)
    if (nCh >= 2)
    {
        const float revSize = P ("revsize"), revDamp = P ("revdamp"), revWidth = P ("revwidth");
        const int   preSamples = juce::jlimit (0, preSize - 2, (int) (P ("revpre") * 0.001f * (float) sampleRate));

        for (int i = 0; i < n; ++i)
        {
            preL[(size_t) preW] = d[0][i];
            preR[(size_t) preW] = d[1][i];
            int rp = preW - preSamples;
            if (rp < 0) rp += preSize;
            revL[(size_t) i] = preL[(size_t) rp];
            revR[(size_t) i] = preR[(size_t) rp];
            if (++preW >= preSize) preW = 0;
        }

        juce::Reverb::Parameters p;
        p.roomSize = juce::jlimit (0.0f, 1.0f, 0.25f + 0.55f * revSize + 0.2f * buildSm);
        p.damping  = revDamp;
        p.wetLevel = amtReverb * buildSm * 0.35f;
        p.dryLevel = 0.0f;
        p.width    = revWidth;
        reverb.setParameters (p);
        reverb.processStereo (revL.data(), revR.data(), n);

        for (int i = 0; i < n; ++i)
        {
            d[0][i] += revL[(size_t) i];
            d[1][i] += revR[(size_t) i];
        }
    }

    // --- EQ + Low Cut / High Cut
    const float eqL = P ("eqlow"), eqM = P ("eqmid"), eqF = P ("eqmidfreq"), eqH = P ("eqhigh");
    if (eqL != lastEq[0] || eqM != lastEq[1] || eqF != lastEq[2] || eqH != lastEq[3])
    {
        updateEq (eqL, eqM, eqF, eqH);
        lastEq[0] = eqL; lastEq[1] = eqM; lastEq[2] = eqF; lastEq[3] = eqH;
    }

    const float lcF = P ("lcfreq"), hcF = P ("hcfreq");
    const int   lcSlope = (int) P ("lcslope"), hcSlope = (int) P ("hcslope");
    if (lcF != lastCut[0] || (float) lcSlope != lastCut[1] || hcF != lastCut[2] || (float) hcSlope != lastCut[3])
    {
        updateCuts (lcF, lcSlope, hcF, hcSlope);
        lastCut[0] = lcF; lastCut[1] = (float) lcSlope; lastCut[2] = hcF; lastCut[3] = (float) hcSlope;
    }
    const float lcTarget = P ("lcon") > 0.5f ? 1.0f : 0.0f;
    const float hcTarget = P ("hcon") > 0.5f ? 1.0f : 0.0f;

    for (int i = 0; i < n; ++i)
    {
        lcAmt += amtCoef * (lcTarget - lcAmt);
        hcAmt += amtCoef * (hcTarget - hcAmt);

        for (int ch = 0; ch < nCh; ++ch)
        {
            float x = d[ch][i];
            x = eqLowS[ch].process (eqLowC, x);
            x = eqMidS[ch].process (eqMidC, x);
            x = eqHighS[ch].process (eqHighC, x);

            float y = x;
            for (int s = 0; s < lcStages; ++s) y = lcS[ch][s].process (lcC[s], y);
            x += lcAmt * (y - x);

            y = x;
            for (int s = 0; s < hcStages; ++s) y = hcS[ch][s].process (hcC[s], y);
            x += hcAmt * (y - x);

            d[ch][i] = x;
        }
    }

    // --- Auto Pan + largura estéreo (graves ficam em mono para proteger kick e bass)
    const bool  panOn    = P ("panon") > 0.5f;
    const int   panShape = (int) P ("panshape"), panRate = (int) P ("panrate");
    const bool  panSync  = P ("pansync") > 0.5f, bassMono = P ("panbass") > 0.5f;
    const float panHz    = P ("panhz"), panPhaseOff = P ("panphase");
    panDepthSm.setTargetValue (panOn ? P ("pandepth") : 0.0f);
    widthSm.setTargetValue (P ("panwidth"));

    const bool panActive = nCh > 1 && (panOn || lfoDest == dPan || lfoDest == dWidth
                                       || panDepthSm.isSmoothing() || widthSm.isSmoothing()
                                       || std::abs (widthSm.getTargetValue() - 1.0f) > 0.001f);
    if (panActive)
    {
        if (! panPrevActive)
            bassLP.reset();

        const double panCycle = tr::divisionBeats (panRate);

        for (int i = 0; i < n; ++i)
        {
            float ph;
            if (panSync)
            {
                double x = (phasePpq0 + (double) i * beatsPerSample) / panCycle + (double) panPhaseOff;
                x -= std::floor (x);
                ph = (float) x;
            }
            else
            {
                panPhaseFree += panHz / (float) sampleRate;
                panPhaseFree -= std::floor (panPhaseFree);
                float x = panPhaseFree + panPhaseOff;
                ph = x - std::floor (x);
            }

            if (ph < panPrevPhase)                       // começou um novo ciclo
                panRnd = rng.nextFloat() * 2.0f - 1.0f;
            panPrevPhase = ph;

            float target = panDepthSm.getNextValue() * tr::lfoShape (panShape, ph, panRnd);
            if (lfoDest == dPan) target += lfoBuf[(size_t) i];
            panSm += panCoef * (juce::jlimit (-1.0f, 1.0f, target) - panSm);

            float w = widthSm.getNextValue();
            if (lfoDest == dWidth) w += lfoBuf[(size_t) i];
            w = juce::jlimit (0.0f, 2.0f, w);

            float l = d[0][i], r = d[1][i], monoLow = 0.0f;
            if (bassMono)
            {
                const float lowL = bassLP.processSample (0, l), lowR = bassLP.processSample (1, r);
                monoLow = 0.5f * (lowL + lowR);
                l -= lowL;
                r -= lowR;
            }

            l *= panSm > 0.0f ? 1.0f - panSm : 1.0f;     // balanço: sem aumento de volume
            r *= panSm < 0.0f ? 1.0f + panSm : 1.0f;

            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * w;
            d[0][i] = m + s + monoLow;
            d[1][i] = m - s + monoLow;
        }
    }
    else
    {
        panSm += 0.2f * (0.0f - panSm);
    }
    panPrevActive = panActive;

    // --- Mix (dry/wet) + ganho de saída (+ LFO em Volume)
    mixSm.setTargetValue (P ("mix"));
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (P ("output")));

    for (int i = 0; i < n; ++i)
    {
        const float m  = mixSm.getNextValue();
        float gn = gainSm.getNextValue();
        if (lfoDest == dVolume)
            gn *= juce::jlimit (0.0f, 1.5f, 1.0f + 0.5f * lfoBuf[(size_t) i]);

        for (int ch = 0; ch < nCh; ++ch)
            d[ch][i] = (dryBuffer.getSample (ch, i) * (1.0f - m) + d[ch][i] * m) * gn;
    }

    // --- Medidores e estado para a interface
    outLevelL.store (buf.getMagnitude (0, 0, n));
    if (nCh > 1)
        outLevelR.store (buf.getMagnitude (1, 0, n));

    freePpq += (double) n * beatsPerSample;
    if (freePpq > 1.0e7) freePpq = 0.0;

    uiBuild.store (buildSm);
    uiBpm.store (bpm);
    uiBarPos.store (enginePpq0 / beatsPerBar);
    uiBarFrac.store ((float) (phasePpq0 / beatsPerBar - std::floor (phasePpq0 / beatsPerBar)));
    uiGateCycle.store ((float) gateCycleNow);
    uiPan.store (panSm);
    uiLfoVal.store (lfoBuf[(size_t) (n - 1)]);
    uiLfoPhase.store (lfoPrevPhase);
    uiNum.store (num);
    uiDen.store (den);
    uiSync.store (hasHost ? (playing ? 2 : 1) : 0);
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
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

            const juce::File f (apvts.state.getProperty ("samplePath").toString());
            if (f.existsAsFile())
                loadSample (f);
            else
                clearSample();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TensionProcessor();
}
