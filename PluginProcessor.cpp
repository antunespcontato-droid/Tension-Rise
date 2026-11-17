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
    using F = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;
    using ID = juce::ParameterID;
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

    const Attr deg = Attr()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 360.0f)) + juce::String::fromUTF8 ("\xc2\xb0"); })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 360.0f; });

    const Attr hzSlow = Attr()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " Hz"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });

    const juce::NormalisableRange<float> unit (0.0f, 1.0f, 0.001f);
    const juce::NormalisableRange<float> wide (0.0f, 2.0f, 0.001f);
    const juce::NormalisableRange<float> gain (-12.0f, 12.0f, 0.1f);
    juce::NormalisableRange<float> freq (200.0f, 5000.0f, 1.0f);       freq.setSkewForCentre (1000.0f);
    juce::NormalisableRange<float> lcRange (20.0f, 500.0f, 1.0f);      lcRange.setSkewForCentre (100.0f);
    juce::NormalisableRange<float> hcRange (2000.0f, 20000.0f, 1.0f);  hcRange.setSkewForCentre (8000.0f);
    juce::NormalisableRange<float> panHz (0.05f, 10.0f, 0.01f);        panHz.setSkewForCentre (1.0f);

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

    reverb.setSampleRate (sr);
    reverb.reset();

    dryBuffer.setSize (2, samplesPerBlock);
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

    buildSm = apvts.getRawParameterValue ("build")->load();
    gateGain = 1.0f; gatePhase = 0.0f; panSm = 0.0f; panPrevPhase = 0.0f; panPhaseFree = 0.0f;
    lcAmt = hcAmt = 0.0f;
    panPrevActive = false;
    freePpq = 0.0;
}

void TensionProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nCh = juce::jmin (buf.getNumChannels(), 2);
    const int n   = buf.getNumSamples();
    if (nCh < 1 || n < 1)
        return;

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
    const double phasePpq0      = (hasPpq && playing) ? ppqHost : freePpq; // gate e pan seguem a grade quando toca

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

    // Guarda o sinal original para o Mix (dry/wet)
    dryBuffer.setSize (2, n, false, false, true);
    for (int ch = 0; ch < nCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buf, ch, 0, n);

    float* d[2] = { buf.getWritePointer (0), nCh > 1 ? buf.getWritePointer (1) : nullptr };
    double gateCycleNow = tr::divisionBeats (gateRate);

    // --- Build Engine + gate + filtro + drive + ruído
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

        if ((i & 31) == 0)
        {
            hpf.setCutoffFrequency (20.0f * std::pow (400.0f, b * amtFilter)); // 20 Hz -> 8 kHz
            hpf.setResonance (0.7f + 2.0f * b * amtFilter);
            noiseFilter.setCutoffFrequency (300.0f + 8000.0f * b * b);
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

        const float driveMix  = amtDrive * b;
        const float driveGain = 1.0f + 4.0f * b;
        const float noiseGain = amtNoise * b * b * 0.3f;

        for (int ch = 0; ch < nCh; ++ch)
        {
            float x = hpf.processSample (ch, d[ch][i]);
            x = (1.0f - driveMix) * x + driveMix * std::tanh (x * driveGain);
            x *= gateGain;
            const float nz = noiseFilter.processSample (ch, rng.nextFloat() * 2.0f - 1.0f) * noiseGain;
            d[ch][i] = x + nz;
        }
    }

    // --- Reverb
    if (nCh >= 2)
    {
        juce::Reverb::Parameters p;
        p.roomSize = 0.5f + 0.45f * buildSm;
        p.damping  = 0.4f;
        p.wetLevel = amtReverb * buildSm * 0.35f;
        p.dryLevel = 0.5f;   // o JUCE dobra esse valor internamente -> ganho seco unitário
        p.width    = 1.0f;
        reverb.setParameters (p);
        reverb.processStereo (d[0], d[1], n);
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

    const bool panActive = nCh > 1 && (panOn || panDepthSm.isSmoothing() || widthSm.isSmoothing()
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

            const float target = panDepthSm.getNextValue() * tr::lfoShape (panShape, ph, panRnd);
            panSm += panCoef * (target - panSm);
            const float w = widthSm.getNextValue();

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

    // --- Mix (dry/wet) + ganho de saída
    mixSm.setTargetValue (P ("mix"));
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (P ("output")));

    for (int i = 0; i < n; ++i)
    {
        const float m  = mixSm.getNextValue();
        const float gn = gainSm.getNextValue();

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
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TensionProcessor();
}
