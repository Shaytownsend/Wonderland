#include "EnvelopePreview.h"
#include "Dynamics.h"
#include "DspCommon.h"
#include <random>

namespace fv::dsp
{
// Runs one synthetic drum hit through the real Detector / Ducker / Gate so the
// static preview always matches the engine. The "reverb" is a noise tail with the
// IR's RT60 (convolution itself is not needed to show the gain shapes).
void renderEnvelopePreview (const EngineParams& pIn, double sr, double hitAtMs, double totalMs,
                            double rt60, std::vector<VizFrame>& out)
{
    // render at a reduced rate for speed: the shapes are rate-independent
    const double rate = std::min (sr, 24000.0);
    const int n = (int) (totalMs * 0.001 * rate);
    const int dec = std::max (1, (int) std::lround (rate / (sr / kVizDecimation)));
    EngineParams p = pIn;
    Detector det; Ducker duck; Gate gate;
    det.prepare (rate); duck.prepare (rate); gate.prepare (rate);
    {
        // silent pre-roll so the gate starts closed, as it would be between hits
        std::vector<float> z ((size_t) (rate * 0.25 + p.gateReleaseMs * 0.001 * rate + p.gateHoldMs * 0.001 * rate), 0.0f), t1 (z.size()), t2 (z.size());
        det.process (z.data(), z.data(), t1.data(), nullptr, nullptr, (int) z.size(), p);
        duck.process (t1.data(), t2.data(), (int) z.size(), p);
        gate.process (t1.data(), t2.data(), (int) z.size(), p);
    }

    std::vector<float> key ((size_t) n, 0.0f), lvl ((size_t) n), dg ((size_t) n), gg ((size_t) n);
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    const int hit = (int) (hitAtMs * 0.001 * rate);
    for (int i = hit; i < n; ++i)
    {
        const double t = (i - hit) / rate;
        key[(size_t) i] = (float) (0.9 * std::exp (-t * 30.0) * (0.5 * std::sin (2 * kPi * 180.0 * t) + 0.5 * u (rng)));
    }
    det.process (key.data(), key.data(), lvl.data(), nullptr, nullptr, n, p);
    duck.process (lvl.data(), dg.data(), n, p);
    gate.process (lvl.data(), gg.data(), n, p);

    out.clear();
    VizFrame acc;
    int cnt = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = (i - hit) / rate;
        const double verb = t < 0 ? 0.0 : 0.5 * (1.0 - std::exp (-t / 0.006)) * std::exp (-6.907755 * t / std::max (0.05, rt60));
        acc.dryPeak = std::max (acc.dryPeak, std::abs (key[(size_t) i]));
        acc.wetPeak = std::max (acc.wetPeak, (float) std::abs (verb * u (rng)) * dg[(size_t) i] * gg[(size_t) i]);
        acc.keyDb = std::max (acc.keyDb, lvl[(size_t) i]);
        acc.duckGain = dg[(size_t) i];
        acc.gateGain = gg[(size_t) i];
        if (++cnt >= dec) { out.push_back (acc); acc = {}; cnt = 0; }
    }
}
} // namespace fv::dsp
