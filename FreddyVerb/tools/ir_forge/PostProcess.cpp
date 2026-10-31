#include "PostProcess.h"

namespace irf
{
namespace
{
    /** Normalised Schroeder energy decay curve (dB) of the 4-channel energy sum. */
    std::vector<double> edcDb (const Ir4& ir)
    {
        const size_t n = ir.size();
        std::vector<double> e (n, 0.0);
        double acc = 0;
        for (size_t i = n; i-- > 0;)
        {
            double s = 0;
            for (auto& c : ir.ch) s += c[i] * c[i];
            acc += s;
            e[i] = acc;
        }
        const double total = std::max (e.empty() ? 0.0 : e[0], 1e-300);
        for (auto& v : e) v = 10.0 * std::log10 (std::max (v / total, 1e-30));
        return e;
    }

    /** Least-squares slope (dB/s) of edc between two levels. */
    bool fitSlope (const std::vector<double>& edc, double fs, double hiDb, double loDb, double& slope)
    {
        size_t a = 0, b = 0;
        while (a < edc.size() && edc[a] > hiDb) ++a;
        b = a;
        while (b < edc.size() && edc[b] > loDb) ++b;
        if (b >= edc.size() || b <= a + 8) return false;
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        const double cnt = (double) (b - a);
        for (size_t i = a; i < b; ++i)
        {
            const double x = (double) i / fs, y = edc[i];
            sx += x; sy += y; sxx += x * x; sxy += x * y;
        }
        slope = (cnt * sxy - sx * sy) / (cnt * sxx - sx * sx);
        return slope < 0;
    }
} // namespace

void finishIr (Ir4& ir, const FinishParams& p)
{
    // DC / subsonic removal and ultrasonic band-limit (4th-order Butterworth at 20 kHz): nothing above the
    // audible band is kept (it would only cost FLAC bits and alias in the plugin's resampler)
    applyEq (ir, { { EqBand::HP, 20.0, 0.70710678 }, { EqBand::LP, 20000.0, 0.5411961 }, { EqBand::LP, 20000.0, 1.3065630 } });

    auto edc = edcDb (ir);
    size_t end = ir.size();
    if (! p.keepEnd)
    {
        size_t iEnd = 0, iFade = 0;
        while (iEnd < edc.size() && edc[iEnd] > p.endDb) ++iEnd;
        while (iFade < edc.size() && edc[iFade] > p.fadeFromDb) ++iFade;
        if (p.maxLengthS > 0)
        {
            const size_t cap = (size_t) (p.maxLengthS * ir.fs);
            if (iEnd > cap)
            {
                // keep the fade proportionally inside the cap
                const double frac = (double) (iEnd - iFade) / (double) iEnd;
                iEnd = cap;
                iFade = (size_t) ((1.0 - frac) * (double) cap);
            }
        }
        iEnd = std::min (iEnd, ir.size());
        iFade = std::min (iFade, iEnd > 0 ? iEnd - 1 : 0);
        const double fs = ir.fs;
        const double t0 = (double) iFade / fs, t1 = (double) iEnd / fs;
        applyEnvelope (ir, [&] (double t) {
            if (t <= t0) return 1.0;
            if (t >= t1) return 0.0;
            return 0.5 + 0.5 * std::cos (kPi * (t - t0) / (t1 - t0));
        });
        end = iEnd + 1;
    }

    // peak normalise (joint across the 4 channels)
    const double pk = ir.peak();
    if (pk > 0) ir.scale (dbToGain (p.peakDbfs) / pk);

    // trim to the last sample that survives 24-bit quantisation, plus one true-zero sample
    const double lsb = 1.0 / 8388608.0;
    size_t last = 0;
    for (size_t i = 0; i < std::min (end, ir.size()); ++i)
        for (auto& c : ir.ch)
            if (std::abs (c[i]) >= 0.5 * lsb) last = i;
    const size_t newLen = std::min (ir.size(), last + 2);
    ir.resize (newLen);
    for (auto& c : ir.ch)
    {
        c.back() = 0.0;
        for (auto& v : c) if (std::abs (v) < 0.5 * lsb) v = 0.0;
    }
}

DecayMeasure measureDecay (const Ir4& irIn, double bandHz)
{
    Ir4 ir = irIn;
    if (bandHz > 0)
    {
        // octave band: 4th-order Butterworth band-pass realised as HP + LP pairs at f/sqrt2, f*sqrt2
        const double lo = bandHz / std::sqrt (2.0), hi = bandHz * std::sqrt (2.0);
        applyEq (ir, { { EqBand::HP, lo, 0.5411961 }, { EqBand::HP, lo, 1.3065630 },
                       { EqBand::LP, hi, 0.5411961 }, { EqBand::LP, hi, 1.3065630 } });
    }
    auto edc = edcDb (ir);
    DecayMeasure m;
    double s;
    if (fitSlope (edc, ir.fs, -5.0, -35.0, s)) { m.t30 = -60.0 / s; m.valid = true; }
    if (fitSlope (edc, ir.fs, -5.0, -25.0, s)) m.t20 = -60.0 / s;
    if (fitSlope (edc, ir.fs, 0.0, -10.0, s)) m.edt = -60.0 / s;
    return m;
}

double reportedRt60 (const Ir4& ir)
{
    const auto m = measureDecay (ir, 1000.0);
    if (m.t30 > 0) return m.t30;
    if (m.t20 > 0) return m.t20;
    return m.edt;
}

} // namespace irf
