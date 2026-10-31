#include "EnvelopePreview.h"
#include "DspCommon.h"
#include "../state/Parameters.h"
#include <random>

namespace fv::dsp
{
// Milestone 1: an analytic approximation of the duck and gate curves.
// Replaced by the real Detector/Ducker/Gate in milestone 3.
void renderEnvelopePreview (const EngineParams& p, double sr, double hitAtMs, double totalMs,
                            double rt60, std::vector<VizFrame>& out)
{
    const double fr = sr / kVizDecimation;
    const int n = (int) (totalMs * 0.001 * fr);
    out.assign ((size_t) std::max (0, n), {});
    const double hitDurMs = 120.0;
    const double depth = p.duckDepthDb <= kDuckFullDb ? 0.0 : dbToGain ((double) p.duckDepthDb);
    const double floorG = p.gateFloorDb <= kGateFloorInf ? 0.0 : dbToGain ((double) p.gateFloorDb);
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> u (0.6f, 1.0f);

    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / fr * 1000.0 - hitAtMs;
        auto& f = out[(size_t) i];
        const double dryEnv = t < 0 ? 0.0 : std::exp (-t / 35.0);
        f.dryPeak = (float) (dryEnv * u (rng));
        f.keyDb = gainToDb ((float) dryEnv, -120.0f);

        double duck = 1.0;
        if (p.duckOn && t >= 0)
        {
            if (t < hitDurMs) duck = depth;
            else
            {
                const double x = std::min (1.0, (t - hitDurMs) / std::max (1.0, (double) p.duckReleaseMs));
                duck = depth + (1.0 - depth) * (1.0 - std::exp (-4.0 * x)) / (1.0 - std::exp (-4.0));
            }
        }
        double gate = 1.0;
        if (p.gateOn)
        {
            if (t < 0) gate = floorG;
            else
            {
                const double closeAt = hitDurMs + p.gateHoldMs;
                if (t > closeAt)
                {
                    const double x = std::min (1.0, (t - closeAt) / std::max (1.0, (double) p.gateReleaseMs));
                    gate = floorG + (1.0 - floorG) * (1.0 - x);
                }
            }
        }
        const double verb = t < 0 ? 0.0 : (1.0 - std::exp (-t / 6.0)) * std::exp (-6.91 * t * 0.001 / std::max (0.05, rt60));
        f.duckGain = (float) duck;
        f.gateGain = (float) gate;
        f.wetPeak = (float) (0.7 * verb * duck * gate * u (rng));
    }
}
} // namespace fv::dsp
