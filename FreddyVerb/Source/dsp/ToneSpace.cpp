#include "ToneSpace.h"
#include "../state/Parameters.h"
#include <complex>

namespace fv::dsp
{
namespace
{
    const double kButterQ[3][4] = { { 0.70710678, 0, 0, 0 },
                                    { 0.54119610, 1.30656296, 0, 0 },
                                    { 0.50979558, 0.60134489, 0.89997622, 2.56291545 } };
    const int kStages[3] = { 1, 2, 4 };
}

void SvfEq::set (Type t, double hz, double q, double gainDb, double fs) noexcept
{
    hz = std::clamp (hz, 5.0, fs * 0.49);
    const double A = std::pow (10.0, gainDb / 40.0);
    double g = std::tan (kPi * hz / fs);
    double k = 1.0 / q;
    switch (t)
    {
        case lowpass:   m0 = 0; m1 = 0; m2 = 1; break;
        case highpass:  m0 = 1; m1 = -k; m2 = -1; break;
        case bell:
        {
            // decramp: keep the centre exact and choose the digital Q so the warped lower
            // band edge lands on the analogue prototype's lower edge (in real Hz).
            const double half = 1.0 / (2.0 * q);
            const double f1 = hz * (std::sqrt (1.0 + half * half) - half);
            const double om1 = std::tan (kPi * f1 / fs) / g;            // warped lower edge, normalised
            const double qd = (om1 > 0.0 && om1 < 1.0) ? om1 / (1.0 - om1 * om1) : q;
            k = 1.0 / (qd * A); m0 = 1; m1 = k * (A * A - 1.0); m2 = 0;
            break;
        }
        case lowShelf:  g /= std::sqrt (A); m0 = 1; m1 = k * (A - 1.0); m2 = A * A - 1.0; break;
        case highShelf: g *= std::sqrt (A); m0 = A * A; m1 = k * (1.0 - A) * A; m2 = 1.0 - A * A; break;
    }
    a1 = 1.0 / (1.0 + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

// ===========================================================================
// WetEQ
// ===========================================================================
void WetEQ::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    osr = sr * 2.0;
    os = std::make_unique<juce::dsp::Oversampling<double>> (2, 1, juce::dsp::Oversampling<double>::filterHalfBandPolyphaseIIR, true, false);
    os->initProcessing ((size_t) maxBlock);
    buf.setSize (2, maxBlock);
    mix.reset (sr, 30.0);
    reset();
}

void WetEQ::reset()
{
    if (os) os->reset();
    for (auto& ch : hp) for (auto& f : ch) f.reset();
    for (auto& ch : lp) for (auto& f : ch) f.reset();
    for (int c = 0; c < 2; ++c) { ls[c].reset(); hs[c].reset(); b1[c].reset(); b2[c].reset(); }
    first = true;
}

void WetEQ::updateCoefficients (const EngineParams& p) noexcept
{
    // targets: frequencies in log2 for perceptually even smoothing
    const double tgt[16] = { std::log2 (p.hpfHz), std::log2 (p.lpfHz), std::log2 (p.lsHz), p.lsGainDb, p.lsQ,
                             std::log2 (p.hsHz), p.hsGainDb, p.hsQ, std::log2 (p.b1Hz), p.b1GainDb, p.b1Q,
                             std::log2 (p.b2Hz), p.b2GainDb, p.b2Q, 0, 0 };
    const double a = first ? 1.0 : 0.25;   // per update (every 8 samples): ~1 ms smoothing
    bool moving = first;
    for (int i = 0; i < 14; ++i)
    {
        const double d = tgt[i] - sm[i];
        if (std::abs (d) > 1.0e-6) { sm[i] += d * a; moving = true; }
        else sm[i] = tgt[i];
    }
    const int slopeKey = p.hpfSlope * 3 + p.lpfSlope;
    first = false;
    // coefficients only change while a parameter is still gliding (or a slope changed)
    if (! moving && slopeKey == lastSlopeKey)
    {
        hpOn = p.hpfOn; lpOn = p.lpfOn; lsOn = p.lsOn; hsOn = p.hsOn; b1On = p.b1On; b2On = p.b2On;
        return;
    }
    lastSlopeKey = slopeKey;

    hpOn = p.hpfOn; lpOn = p.lpfOn; lsOn = p.lsOn; hsOn = p.hsOn; b1On = p.b1On; b2On = p.b2On;
    hpN = kStages[std::clamp (p.hpfSlope, 0, 2)];
    lpN = kStages[std::clamp (p.lpfSlope, 0, 2)];
    for (int c = 0; c < 2; ++c)
    {
        for (int s = 0; s < hpN; ++s) hp[c][s].set (SvfEq::highpass, std::exp2 (sm[0]), kButterQ[std::clamp (p.hpfSlope, 0, 2)][s], 0, osr);
        for (int s = 0; s < lpN; ++s) lp[c][s].set (SvfEq::lowpass, std::exp2 (sm[1]), kButterQ[std::clamp (p.lpfSlope, 0, 2)][s], 0, osr);
        ls[c].set (SvfEq::lowShelf, std::exp2 (sm[2]), sm[4], sm[3], osr);
        hs[c].set (SvfEq::highShelf, std::exp2 (sm[5]), sm[7], sm[6], osr);
        b1[c].set (SvfEq::bell, std::exp2 (sm[8]), sm[10], sm[9], osr);
        b2[c].set (SvfEq::bell, std::exp2 (sm[11]), sm[13], sm[12], osr);
    }
}

void WetEQ::process (float* l, float* r, int n, const EngineParams& p, bool active) noexcept
{
    mix.setTarget (active ? 1.0f : 0.0f);
    if (! active && ! mix.isSmoothing() && mix.current() <= 0.0f)
    {
        // fully bypassed: keep filter state fresh-ish by resetting once
        return;
    }
    for (int i = 0; i < n; ++i) { buf.setSample (0, i, l[i]); buf.setSample (1, i, r[i]); }
    juce::dsp::AudioBlock<double> block (buf.getArrayOfWritePointers(), 2, (size_t) n);
    auto up = os->processSamplesUp (block);
    const int un = (int) up.getNumSamples();
    for (int i = 0; i < un; ++i)
    {
        if ((i & 7) == 0) updateCoefficients (p);
        for (int c = 0; c < 2; ++c)
        {
            double x = up.getSample (c, i);
            if (hpOn) for (int s = 0; s < hpN; ++s) x = hp[c][s].process (x);
            if (lsOn) x = ls[c].process (x);
            if (b1On) x = b1[c].process (x);
            if (b2On) x = b2[c].process (x);
            if (hsOn) x = hs[c].process (x);
            if (lpOn) for (int s = 0; s < lpN; ++s) x = lp[c][s].process (x);
            up.setSample (c, i, x);
        }
    }
    os->processSamplesDown (block);
    for (int i = 0; i < n; ++i)
    {
        const float m = mix.next();
        l[i] = l[i] * (1.0f - m) + (float) buf.getSample (0, i) * m;
        r[i] = r[i] * (1.0f - m) + (float) buf.getSample (1, i) * m;
    }
}

double WetEQ::responseDb (double hz, const EngineParams& p) const
{
    // response of the digital SVF chain at the oversampled rate (oversampling filters excluded)
    using C = std::complex<double>;
    const C z = std::polar (1.0, 2.0 * kPi * hz / osr);
    auto svf = [&] (SvfEq::Type t, double f, double q, double gdb)
    {
        SvfEq e; e.set (t, f, q, gdb, osr);
        // closed-form TPT SVF transfer: derived from the bilinear transform of the analog prototype
        const double g = e.a2 / e.a1, k = (1.0 / e.a1 - 1.0 - g * g) / g;
        const C s = (z - 1.0) / (z + 1.0) / g;     // normalised so that the cutoff maps to s = j
        const C den = s * s + k * s + 1.0;
        const C hpR = s * s / den, bpR = s / den, lpR = 1.0 / den;
        return e.m0 * (hpR + k * bpR + lpR) + e.m1 * bpR + e.m2 * lpR;
    };
    C h (1.0, 0.0);
    const int hs_ = std::clamp (p.hpfSlope, 0, 2), ls_ = std::clamp (p.lpfSlope, 0, 2);
    if (p.hpfOn) for (int s = 0; s < kStages[hs_]; ++s) h *= svf (SvfEq::highpass, p.hpfHz, kButterQ[hs_][s], 0);
    if (p.lpfOn) for (int s = 0; s < kStages[ls_]; ++s) h *= svf (SvfEq::lowpass, p.lpfHz, kButterQ[ls_][s], 0);
    if (p.lsOn) h *= svf (SvfEq::lowShelf, p.lsHz, p.lsQ, p.lsGainDb);
    if (p.hsOn) h *= svf (SvfEq::highShelf, p.hsHz, p.hsQ, p.hsGainDb);
    if (p.b1On) h *= svf (SvfEq::bell, p.b1Hz, p.b1Q, p.b1GainDb);
    if (p.b2On) h *= svf (SvfEq::bell, p.b2Hz, p.b2Q, p.b2GainDb);
    return 20.0 * std::log10 (std::max (1.0e-12, std::abs (h)));
}

// ===========================================================================
// WidthStage
// ===========================================================================
void WidthStage::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto* s : { &width, &balL, &balR, &monoMix }) s->reset (sr, 20.0);
    reset();
}

void WidthStage::reset()
{
    for (auto& f : lr) f.reset();
    lastHz = -1.0f;
    width.setCurrentAndTarget (1.0f); balL.setCurrentAndTarget (1.0f); balR.setCurrentAndTarget (1.0f);
    monoMix.setCurrentAndTarget (0.0f);
}

void WidthStage::process (float* l, float* r, int n, const EngineParams& p) noexcept
{
    width.setTarget (std::clamp (p.widthPct, 0.0f, 200.0f) / 100.0f);
    const float b = std::clamp (p.balance, -100.0f, 100.0f) / 100.0f;
    balL.setTarget (std::min (1.0f, 1.0f - b));
    balR.setTarget (std::min (1.0f, 1.0f + b));
    monoMix.setTarget (p.monoBelowOn ? 1.0f : 0.0f);
    if (p.monoBelowHz != lastHz)
    {
        for (auto& f : lr) f.setCutoff (p.monoBelowHz, 0.70710678, sr);
        lastHz = p.monoBelowHz;
    }
    for (int i = 0; i < n; ++i)
    {
        const double m = 0.5 * ((double) l[i] + r[i]);
        double s = 0.5 * ((double) l[i] - r[i]);
        // Linkwitz-Riley (LR4) high-pass on the side: everything below the crossover becomes mono
        const double sHp = lr[1].process (lr[0].process (s, SvfTPT::highpass), SvfTPT::highpass);
        const double mm = monoMix.next();
        s = s * (1.0 - mm) + sHp * mm;
        s *= width.next();
        l[i] = (float) ((m + s) * balL.next());
        r[i] = (float) ((m - s) * balR.next());
    }
}
} // namespace fv::dsp
