// Non-linear reverb envelopes.
//
// Classic gated ('80s AMS / SSL-gated room) and "reverse non-lin" programs are, in IR terms, a dense
// reverb whose envelope is flat or rising and then cut abruptly. We render a long, very diffuse base reverb
// (so the spectral evolution and density are natural), divide out its own smoothed broadband envelope
// (flattening the decay), then impose the target envelope and a very short raised-cosine release.
#include "NonLinear.h"

namespace irf
{
double firstArrival (const Ir4& ir, double relDb)
{
    const double thr = ir.peak() * dbToGain (relDb);
    for (size_t i = 0; i < ir.size(); ++i)
        for (auto& c : ir.ch)
            if (std::abs (c[i]) > thr) return (double) i / ir.fs;
    return 0.0;
}

Ir4 applyNonLinear (const Ir4& base, const NonLinParams& p)
{
    Ir4 ir = base;
    const double fs = ir.fs;
    const double t0 = firstArrival (base, -30.0);
    const auto env = smoothedEnvelope (base, p.flattenMs);

    const double tKeep = t0 + std::max (p.keepNaturalMs, p.attackMs) * 0.001;
    const size_t iKeep = std::min (env.size() - 1, (size_t) (tKeep * fs));
    // reference level: envelope just after the onset region
    const double ref = std::max (env[iKeep], 1e-12);
    const double tGate = t0 + p.gateMs * 0.001, tFade = p.fadeMs * 0.001;

    for (size_t i = 0; i < ir.size(); ++i)
    {
        const double t = (double) i / fs;
        double g;
        if (t >= tGate + tFade) g = 0.0;
        else
        {
            // flattening factor (held constant through the natural-burst / attack region)
            const double flat = i <= iKeep ? 1.0 : ref / std::max (env[i], ref * 1e-4);

            // target envelope in dB
            const double span = std::max (1e-6, tGate - tKeep);
            double x = std::clamp ((t - tKeep) / span, 0.0, 1.0);
            if (p.curve != 1.0) x = std::pow (x, p.curve);
            double db = p.startDb + (p.endDb - p.startDb) * x;
            double shape = dbToGain (db);

            if (t < t0 + p.attackMs * 0.001 && p.keepNaturalMs <= 0)
            {
                const double a = std::clamp ((t - t0) / (p.attackMs * 0.001), 0.0, 1.0);
                shape *= 0.5 - 0.5 * std::cos (kPi * a);
            }
            if (i <= iKeep && p.keepNaturalMs > 0) shape = dbToGain (p.startDb);

            double rel = 1.0;
            if (t > tGate) rel = 0.5 + 0.5 * std::cos (kPi * (t - tGate) / tFade);
            g = flat * shape * rel;
        }
        for (auto& c : ir.ch) c[i] *= g;
    }
    return ir;
}

} // namespace irf
