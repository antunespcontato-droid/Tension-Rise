#pragma once
// Matemática musical e filtros do Tension Rise. Sem dependência do JUCE:
// é testado isoladamente em tests/test_dsp.cpp e não aloca memória no áudio.
#include <algorithm>
#include <cmath>
#include <complex>

namespace tr
{
constexpr double kPi = 3.14159265358979323846;

//==============================================================================
// Divisões musicais (comprimento de um ciclo, em tempos = semínimas)
constexpr int kNumDivisions = 15;

inline const char* divisionName (int i)
{
    static const char* names[kNumDivisions] = { "1/1", "1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T",
                                                "1/16", "1/16.", "1/16T", "1/32", "1/32.", "1/32T", "1/64" };
    return names[std::clamp (i, 0, kNumDivisions - 1)];
}

inline double divisionBeats (int i)
{
    static const double beats[kNumDivisions] = { 4.0, 2.0, 1.0, 1.5, 2.0 / 3.0,
                                                 0.5, 0.75, 1.0 / 3.0,
                                                 0.25, 0.375, 1.0 / 6.0,
                                                 0.125, 0.1875, 1.0 / 12.0,
                                                 0.0625 };
    return beats[std::clamp (i, 0, kNumDivisions - 1)];
}

//==============================================================================
// Build Engine
inline int lengthBars (int idx)
{
    static const int bars[4] = { 8, 16, 32, 64 };
    return bars[std::clamp (idx, 0, 3)];
}

// 0 Linear, 1 Exponential, 2 Slow, 3 Fast, 4 Stepped.  t e resultado em [0,1].
inline float curveValue (int curve, float t)
{
    t = std::clamp (t, 0.0f, 1.0f);
    switch (curve)
    {
        case 1:  return (std::exp (4.0f * t) - 1.0f) / (std::exp (4.0f) - 1.0f);
        case 2:  return t * t * (3.0f - 2.0f * t);
        case 3:  return 1.0f - (1.0f - t) * (1.0f - t);
        case 4:  return std::min (1.0f, std::floor (t * 8.0f) / 7.0f);
        default: return t;
    }
}

// Nível do build numa posição (em compassos, começando em 0). Zero antes do início
// e a partir do fim (o "drop").
inline float buildAt (double barPos, double startBar0, int lengthInBars, int curve)
{
    const double t = (barPos - startBar0) / (double) lengthInBars;
    if (t < 0.0 || t >= 1.0)
        return 0.0f;
    return curveValue (curve, (float) t);
}

//==============================================================================
// LFO de panorama: fase em [0,1), resultado em [-1,1]. rnd é o valor aleatório do ciclo atual.
inline float lfoShape (int shape, float phase, float rnd)
{
    switch (shape)
    {
        case 1:  return phase < 0.25f ? 4.0f * phase : (phase < 0.75f ? 2.0f - 4.0f * phase : 4.0f * phase - 4.0f);
        case 2:  return phase < 0.5f ? 1.0f : -1.0f;
        case 3:  return rnd;
        default: return std::sin ((float) (2.0 * kPi) * phase);
    }
}

//==============================================================================
// Biquad (formas RBJ), em double, sem alocação.
struct BiquadCoef { double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; };

inline BiquadCoef normalise (double b0, double b1, double b2, double a0, double a1, double a2)
{
    BiquadCoef c;
    c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
    return c;
}

inline double safeFreq (double f, double sr) { return std::clamp (f, 1.0, 0.499 * sr); }

inline BiquadCoef makeHighPass (double sr, double f, double q)
{
    const double w0 = 2.0 * kPi * safeFreq (f, sr) / sr, c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
    return normalise ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}

inline BiquadCoef makeLowPass (double sr, double f, double q)
{
    const double w0 = 2.0 * kPi * safeFreq (f, sr) / sr, c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
    return normalise ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}

inline BiquadCoef makePeak (double sr, double f, double q, double gain)
{
    const double A = std::sqrt (gain), w0 = 2.0 * kPi * safeFreq (f, sr) / sr, c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
    return normalise (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
}

inline BiquadCoef makeLowShelf (double sr, double f, double q, double gain)
{
    const double A = std::sqrt (gain), w0 = 2.0 * kPi * safeFreq (f, sr) / sr, c = std::cos (w0);
    const double beta = std::sin (w0) * std::sqrt (A) / q;
    return normalise (A * ((A + 1) - (A - 1) * c + beta), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - beta),
                      (A + 1) + (A - 1) * c + beta, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - beta);
}

inline BiquadCoef makeHighShelf (double sr, double f, double q, double gain)
{
    const double A = std::sqrt (gain), w0 = 2.0 * kPi * safeFreq (f, sr) / sr, c = std::cos (w0);
    const double beta = std::sin (w0) * std::sqrt (A) / q;
    return normalise (A * ((A + 1) + (A - 1) * c + beta), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - beta),
                      (A + 1) - (A - 1) * c + beta, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - beta);
}

inline double magnitudeAt (const BiquadCoef& c, double f, double sr)
{
    const std::complex<double> z = std::polar (1.0, -2.0 * kPi * f / sr);
    return std::abs ((c.b0 + c.b1 * z + c.b2 * z * z) / (1.0 + c.a1 * z + c.a2 * z * z));
}

struct BiquadState
{
    double z1 = 0.0, z2 = 0.0;

    float process (const BiquadCoef& c, float xf)
    {
        const double x = xf;
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return (float) y;
    }
    void reset() { z1 = z2 = 0.0; }
};

//==============================================================================
// Low Cut / High Cut: Butterworth em cascata. slope 0..3 => 12/24/36/48 dB por oitava.
inline int slopeOrder (int slopeIdx) { return 2 * (std::clamp (slopeIdx, 0, 3) + 1); }
inline int slopeStages (int slopeIdx) { return slopeOrder (slopeIdx) / 2; }

inline double butterworthQ (int order, int stage)
{
    const double k = stage + 1;
    return 1.0 / (2.0 * std::cos (kPi * (2.0 * k - 1.0) / (2.0 * order)));
}

// Magnitude total da cascata (para o gráfico)
inline double cutMagnitude (bool highPass, double freq, int slopeIdx, double f, double sr)
{
    const int order = slopeOrder (slopeIdx);
    double m = 1.0;
    for (int s = 0; s < order / 2; ++s)
    {
        const auto c = highPass ? makeHighPass (sr, freq, butterworthQ (order, s))
                                : makeLowPass  (sr, freq, butterworthQ (order, s));
        m *= magnitudeAt (c, f, sr);
    }
    return m;
}
//==============================================================================
// Formas de onda do motor de LFO (fase em [0,1), resultado em [-1,1])
// 0 Sine, 1 Triangle, 2 Square, 3 Saw Up, 4 Saw Down, 5 Random, 6 Custom (16 passos)
inline float lfoWave (int shape, float phase, float rnd, const float* steps16)
{
    switch (shape)
    {
        case 1:  return phase < 0.25f ? 4.0f * phase : (phase < 0.75f ? 2.0f - 4.0f * phase : 4.0f * phase - 4.0f);
        case 2:  return phase < 0.5f ? 1.0f : -1.0f;
        case 3:  return 2.0f * phase - 1.0f;
        case 4:  return 1.0f - 2.0f * phase;
        case 5:  return rnd;
        case 6:  return steps16[std::min (15, (int) (phase * 16.0f))];
        default: return std::sin ((float) (2.0 * kPi) * phase);
    }
}
} // namespace tr
