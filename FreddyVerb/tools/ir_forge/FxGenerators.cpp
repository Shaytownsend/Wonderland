#include "FxGenerators.h"

#include <memory>

namespace irf
{
Ir4 makeReverse (const Ir4& base, const ReverseParams& p)
{
    const double fs = base.fs;
    const size_t n = std::min (base.size(), (size_t) (p.swellS * fs));
    Ir4 ir (fs, n);
    for (int c = 0; c < 4; ++c)
        std::copy_n (base.ch[(size_t) c].begin(), n, ir.ch[(size_t) c].begin());

    // fade the (soon to be leading) tail of the excerpt to nothing so the reversed IR starts from silence
    applyEnvelope (ir, [&] (double t) {
        const double x = t / p.swellS;
        return x < 0.6 ? 1.0 : 0.5 + 0.5 * std::cos (kPi * std::min (1.0, (x - 0.6) / 0.4));
    });
    ir.reverse();
    if (p.shapePower != 1.0)
        applyEnvelope (ir, [&] (double t) { return std::pow (std::clamp (t / p.swellS, 0.0, 1.0), p.shapePower - 1.0); });

    // short release at the end (the swell peak)
    const size_t fade = (size_t) (p.endFadeMs * 0.001 * fs);
    Ir4 out (fs, n + fade);
    for (int c = 0; c < 4; ++c)
    {
        auto& src = ir.ch[(size_t) c];
        auto& dst = out.ch[(size_t) c];
        std::copy (src.begin(), src.end(), dst.begin());
        // continue with the (pre-reversal) onset mirrored, faded over `fade`
        const double last = src.empty() ? 0.0 : src.back();
        for (size_t i = 0; i < fade; ++i)
            dst[n + i] = last * (0.5 + 0.5 * std::cos (kPi * (double) (i + 1) / (double) fade));
    }
    return out;
}

Ir4 makeBloom (const Ir4& base, const BloomParams& p)
{
    Ir4 ir = base;
    applyEnvelope (ir, [&] (double t) {
        const double x = std::max (0.0, t - p.startS);
        return std::pow (1.0 - std::exp (-x / p.attackS), p.power);
    });
    return ir;
}

// Tuned metallic resonator bank.
//
// Each pitch drives a fractional-delay feedback comb (period fs / f, 6-point Lagrange) with an in-loop
// low-pass, so it rings at f and its harmonics like a struck bar / sheet. A second, detuned layer beats
// against the first; slow random pitch wobble gives a shimmer. Every true-stereo path uses its own random
// gains and polarities for each resonator, and a short bright FDN wash sits underneath for density.
Ir4 renderMetallic (const MetalParams& p, double fs)
{
    const size_t length = (size_t) (p.lengthS * fs);
    Rng rng (p.seed);

    // excitation: short diffused noise burst (avoids a "pluck" click on every resonator)
    std::vector<double> burst (length, 0.0);
    {
        Rng br = rng.fork (5);
        const size_t nb = (size_t) (0.004 * fs);
        for (size_t i = 0; i < nb; ++i)
            burst[i + 16] = br.gauss() * std::sin (kPi * (double) i / (double) nb);
    }

    Ir4 res (fs, length);
    for (int layer = 0; layer < 2; ++layer)
        for (size_t k = 0; k < p.pitchesHz.size(); ++k)
        {
            const double cents = layer == 0 ? 0.0 : p.detuneCents * (k % 2 ? 1.0 : -1.0);
            const double f = p.pitchesHz[k] * std::pow (2.0, cents / 1200.0);
            const double period = fs / f;
            const double g = rt60ToGain (period, fs, p.rt60 * std::pow (440.0 / f, 0.25));
            DelayLine dl;
            dl.init ((size_t) (period * 1.05 + 16));
            auto lp = FirstOrder::lowpass (fs, p.hfDampHz);
            auto modRng = std::make_unique<Rng> (rng.next());
            SmoothRandom sr;
            sr.init (*modRng, p.modRateHz, fs);
            const double depth = period * (std::pow (2.0, p.modDepthCents / 1200.0) - 1.0);

            std::vector<double> y (length, 0.0);
            for (size_t n = 0; n < length; ++n)
            {
                const double d = std::max (4.0, period - 0.5 + depth * sr.next()); // -0.5: one-pole LP delay
                const double v = burst[n] + g * lp.process (dl.readFrac (d));
                dl.write (v);
                y[n] = v;
            }
            for (int c = 0; c < 4; ++c)
            {
                const double gain = rng.sign() * rng.uniform (0.5, 1.0) / std::sqrt ((double) p.pitchesHz.size());
                auto& out = res.ch[(size_t) c];
                for (size_t n = 0; n < length; ++n) out[n] += gain * y[n];
            }
        }

    auto wash = renderFdnImpulse (p.diffuse, fs, length, 0.0, 0.0);
    const double eR = res.mainEnergy(), eW = wash.mainEnergy();
    Ir4 out (fs, length);
    out.addScaled (res, 1.0 / std::sqrt (eR));
    out.addScaled (wash, std::sqrt (p.diffuseLevel / eW));
    applyEq (out, p.eq);
    return out;
}

} // namespace irf
