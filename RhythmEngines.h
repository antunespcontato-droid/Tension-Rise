#pragma once
// Motores rítmicos do Tension Rise (Fase 2). Sem dependência do JUCE:
// testados isoladamente em tests/test_rhythm.cpp. Sem alocação no áudio
// (todos os buffers são criados em prepare()).
#include "DspUtil.h"
#include <cstdint>
#include <vector>

namespace tr
{
//==============================================================================
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    float next01() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.0f / 16777216.0f); }
    float nextBi() { return next01() * 2.0f - 1.0f; }
};

//==============================================================================
// Beat Repeat: congela a última fatia (1 ciclo da divisão escolhida) e repete.
//   engaged    - quando verdadeiro, o loop é capturado na próxima divisa de grade e tocado
//   intensity  - quanto do som original é substituído pelo loop
//   mix        - nível do loop
//   feedback   - decaimento a cada repetição (1 = sem decaimento)
//   variation  - chance de uma repetição tocar só a primeira metade da fatia (duas vezes)
class BeatRepeat
{
public:
    void prepare (double sr)
    {
        size = (int) (sr * 2.0) + 8;
        for (int c = 0; c < 2; ++c) { ring[c].assign ((size_t) size, 0.0f); slice[c].assign ((size_t) size, 0.0f); }
        coef = (float) (1.0 - std::exp (-1.0 / (0.003 * sr)));
        maxSlice = size - 2;
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c) { std::fill (ring[c].begin(), ring[c].end(), 0.0f); std::fill (slice[c].begin(), slice[c].end(), 0.0f); }
        w = 0; captured = false; repeatIndex = 0; slicePos = 0; playLimit = 1; capturedLen = 0;
        a = 0.0f; repGain = 1.0f; lastIdx = INT64_MIN;
    }

    int maxSliceLength() const { return maxSlice; }
    bool isActive() const { return captured && a > 0.01f; }

    // cyc: posição em ciclos (ppq / duração do ciclo). Cada vez que sua parte inteira muda começa uma nova fatia.
    void process (float& l, float& r, bool engaged, double cyc, int sliceLen,
                  float feedback, float intensity, float mix, float variation)
    {
        sliceLen = std::clamp (sliceLen, 16, maxSlice);
        const int64_t idx = (int64_t) std::floor (cyc);

        if (idx != lastIdx)
        {
            lastIdx = idx;
            if (engaged)
            {
                if (! captured || capturedLen != sliceLen)
                {
                    for (int k = 0; k < sliceLen; ++k)
                    {
                        const int p = (w - sliceLen + k + size) % size;
                        slice[0][(size_t) k] = ring[0][(size_t) p];
                        slice[1][(size_t) k] = ring[1][(size_t) p];
                    }
                    capturedLen = sliceLen;
                    captured = true;
                    repeatIndex = 0;
                }
                else
                    ++repeatIndex;

                repGain = std::pow (feedback, (float) repeatIndex);
                slicePos = 0;
                playLimit = (rng.next01() < variation && sliceLen >= 64) ? sliceLen / 2 : sliceLen;
            }
            else
            {
                captured = false;
                repeatIndex = 0;
            }
        }

        float rl = 0.0f, rr = 0.0f;
        if (captured)
        {
            rl = slice[0][(size_t) slicePos] * repGain;
            rr = slice[1][(size_t) slicePos] * repGain;
            if (++slicePos >= playLimit)
                slicePos = 0;
        }

        a += coef * (((engaged && captured) ? 1.0f : 0.0f) - a);

        const float outL = l * (1.0f - intensity * a) + rl * mix * a;
        const float outR = r * (1.0f - intensity * a) + rr * mix * a;

        ring[0][(size_t) w] = l;
        ring[1][(size_t) w] = r;
        if (++w >= size) w = 0;

        l = outL;
        r = outR;
    }

private:
    std::vector<float> ring[2], slice[2];
    int size = 0, maxSlice = 0, w = 0, slicePos = 0, playLimit = 1, capturedLen = 0, repeatIndex = 0;
    bool captured = false;
    float a = 0.0f, coef = 0.01f, repGain = 1.0f;
    int64_t lastIdx = INT64_MIN;
    Rng rng;
};

//==============================================================================
// Snare roll: até 8 vozes sobrepostas tocando um sample (ou o snare sintético interno).
struct SampleView
{
    const float* ch[2] = { nullptr, nullptr };   // ch[1] nulo = mono
    int length = 0;
    double ratio = 1.0;                          // taxa do sample / taxa do host
};

class SnareRoll
{
public:
    static constexpr int kVoices = 8;

    void reset() { for (auto& v : voices) v.active = false; nextSteal = 0; }

    void trigger (float gain, float pan)
    {
        int slot = -1;
        for (int i = 0; i < kVoices; ++i)
            if (! voices[i].active) { slot = i; break; }
        if (slot < 0) { slot = nextSteal; nextSteal = (nextSteal + 1) % kVoices; }

        auto& v = voices[slot];
        v.pos = 0.0;
        v.gl = gain * (pan > 0.0f ? 1.0f - pan : 1.0f);
        v.gr = gain * (pan < 0.0f ? 1.0f + pan : 1.0f);
        v.active = true;
    }

    void render (float& l, float& r, const SampleView& s)
    {
        if (s.ch[0] == nullptr || s.length < 2)
            return;

        for (auto& v : voices)
        {
            if (! v.active) continue;
            const int ip = (int) v.pos;
            if (ip >= s.length - 1) { v.active = false; continue; }

            const float fr = (float) (v.pos - ip);
            const float sl = s.ch[0][ip] + (s.ch[0][ip + 1] - s.ch[0][ip]) * fr;
            const float sr = s.ch[1] ? s.ch[1][ip] + (s.ch[1][ip + 1] - s.ch[1][ip]) * fr : sl;
            l += sl * v.gl;
            r += sr * v.gr;
            v.pos += s.ratio;
        }
    }

    bool anyActive() const { for (auto& v : voices) if (v.active) return true; return false; }

private:
    struct Voice { double pos = 0.0; float gl = 0.0f, gr = 0.0f; bool active = false; };
    Voice voices[kVoices];
    int nextSteal = 0;
};

// Snare sintético interno (ruído filtrado + corpo com queda de pitch), usado quando não há sample carregado.
inline std::vector<float> makeSnare (double sr)
{
    const int len = (int) (0.16 * sr);
    std::vector<float> out ((size_t) len);
    Rng rng;
    double phase = 0.0;
    float lp = 0.0f, peak = 1e-6f;

    for (int i = 0; i < len; ++i)
    {
        const double t = (double) i / sr;
        const float noise = rng.nextBi();
        lp += 0.25f * (noise - lp);
        const float nh = noise - lp;
        phase += 2.0 * kPi * (185.0 + 110.0 * std::exp (-t / 0.018)) / sr;
        const float v = 0.75f * nh * (float) std::exp (-t / 0.045) + 0.55f * (float) (std::sin (phase) * std::exp (-t / 0.07));
        out[(size_t) i] = v;
        peak = std::max (peak, std::fabs (v));
    }

    const int fade = (int) (0.005 * sr);
    for (int i = 0; i < len; ++i)
    {
        float g = 0.9f / peak;
        if (i > len - fade) g *= (float) (len - i) / (float) fade;
        out[(size_t) i] *= g;
    }
    return out;
}

//==============================================================================
// Delay estéreo com tempo suave, filtro no feedback e ping-pong (cross feedback).
class TempoDelay
{
public:
    void prepare (double sr)
    {
        size = (int) (sr * 2.0) + 4;
        for (int c = 0; c < 2; ++c) buf[c].assign ((size_t) size, 0.0f);
        hpC = makeHighPass (sr, 120.0, 0.707);       // evita acúmulo de graves no feedback
        lpC = makeLowPass (sr, 6000.0, 0.707);
        slew = (float) (1.0 - std::exp (-1.0 / (0.05 * sr)));
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c) { std::fill (buf[c].begin(), buf[c].end(), 0.0f); lpS[c].reset(); hpS[c].reset(); }
        w = 0; cur = -1.0;
    }

    void setLowPass (const BiquadCoef& c) { lpC = c; }
    int maxDelaySamples() const { return size - 3; }

    void process (float inL, float inR, double delaySamples, float feedback, float stereo, float& wetL, float& wetR)
    {
        delaySamples = std::clamp (delaySamples, 1.0, (double) maxDelaySamples());
        if (cur < 0.0) cur = delaySamples;
        cur += (double) slew * (delaySamples - cur);

        wetL = readAt (0, cur);
        wetR = readAt (1, cur);

        const float fL = lpS[0].process (lpC, hpS[0].process (hpC, wetL));
        const float fR = lpS[1].process (lpC, hpS[1].process (hpC, wetR));
        const float xL = (1.0f - stereo) * fL + stereo * fR;
        const float xR = (1.0f - stereo) * fR + stereo * fL;

        buf[0][(size_t) w] = std::clamp (inL + feedback * xL, -4.0f, 4.0f);
        buf[1][(size_t) w] = std::clamp (inR + feedback * xR, -4.0f, 4.0f);
        if (++w >= size) w = 0;
    }

private:
    float readAt (int ch, double d) const
    {
        double pos = (double) w - d;
        while (pos < 0.0) pos += (double) size;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float fr = (float) (pos - (double) i0);
        return buf[ch][(size_t) i0] + (buf[ch][(size_t) i1] - buf[ch][(size_t) i0]) * fr;
    }

    std::vector<float> buf[2];
    BiquadCoef hpC, lpC;
    BiquadState hpS[2], lpS[2];
    int size = 0, w = 0;
    double cur = -1.0;
    float slew = 0.001f;
};
} // namespace tr
