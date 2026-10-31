#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Dynamics.h"

namespace fv::dsp
{
/** Simper/Zavalishin SVF with mixing coefficients for shelves and bells
    (topology-preserving, smooth under modulation). */
struct SvfEq
{
    enum Type { lowpass, highpass, lowShelf, highShelf, bell };
    void set (Type t, double hz, double q, double gainDb, double sr) noexcept;
    double process (double x) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenormal (2.0 * v1 - ic1);
        ic2 = flushDenormal (2.0 * v2 - ic2);
        return m0 * x + m1 * v1 + m2 * v2;
    }
    void reset() noexcept { ic1 = ic2 = 0.0; }
    double a1 = 0, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0, ic1 = 0, ic2 = 0;
};

/** Wet EQ: HPF / LPF (12/24/48 dB/oct Butterworth), low & high shelf, two bells.
    Runs at 2x (polyphase IIR oversampling) so bells and shelves keep their
    analogue shape near Nyquist. Parameters are smoothed and coefficients are
    recomputed every few samples; the TPT structure makes that click-free. */
class WetEQ
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    /** In place. `active` crossfades the EQ in/out (EQ on/off, pre/post moves). */
    void process (float* l, float* r, int n, const EngineParams& p, bool active) noexcept;

    /** Magnitude (dB) of the digital filter chain at f, for tests. */
    double responseDb (double hz, const EngineParams& p) const;

private:
    void updateCoefficients (const EngineParams& p) noexcept;
    double sr = 48000.0, osr = 96000.0;
    std::unique_ptr<juce::dsp::Oversampling<double>> os;
    juce::AudioBuffer<double> buf;
    SvfEq hp[2][4], lp[2][4], ls[2], hs[2], b1[2], b2[2];
    int hpN = 2, lpN = 1;
    bool hpOn = false, lpOn = false, lsOn = false, hsOn = false, b1On = false, b2On = false;
    LinearSmoother mix;
    // smoothed parameter state (log-frequency, dB)
    double sm[16] {};
    bool first = true;
};

/** Wet stereo stage: M/S width 0..200 %, Linkwitz-Riley mono-below, balance. */
class WidthStage
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float* l, float* r, int n, const EngineParams& p) noexcept;

private:
    double sr = 48000.0;
    SvfTPT lr[2];               // two cascaded Butterworth HP = LR4 on the side signal
    LinearSmoother width, balL, balR, monoMix;
    float lastHz = -1.0f;
};
} // namespace fv::dsp
