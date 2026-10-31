#include "SpaceReverb.h"

namespace irf
{
Ir4 renderSpace (const SpaceParams& p, double fs)
{
    const size_t length = (size_t) (p.lengthS * fs);

    auto er = renderEarlyReflections (p.er, fs, length);

    // the late field is excited when the direct sound reaches the room: use the per-source mean arrival
    const double tL = 0.5 * (er.directTime[LL] + er.directTime[LR]) + p.lateDelayMs * 0.001;
    const double tR = 0.5 * (er.directTime[RL] + er.directTime[RR]) + p.lateDelayMs * 0.001;
    auto late = renderFdnImpulse (p.fdn, fs, length, tL, tR);

    // The late field is built from many wall reflections, so its spectrum starts already darker than the
    // excitation: colour it like ~2 wall bounces (2nd-order low-pass at the wall absorption corner).
    {
        const double lp = p.lateLpHz > 0 ? p.lateLpHz : p.er.wallLpHz * 1.2;
        applyEq (late, { { EqBand::LP, lp, 0.6 } });
    }

    if (p.lateFadeInMs > 0)
    {
        const double t0 = std::min (tL, tR), tf = p.lateFadeInMs * 0.001;
        applyEnvelope (late, [&] (double t) {
            if (t <= t0) return 0.0;
            if (t >= t0 + tf) return 1.0;
            return 0.5 - 0.5 * std::cos (kPi * (t - t0) / tf);
        });
    }

    const double eEr = er.ir.mainEnergy(), eLate = late.mainEnergy();
    Ir4 out (fs, length);
    const double ratio = dbToGain (p.erToLateDb * 0.5);   // amplitude ratio for an energy ratio in dB
    if (eEr > 0) out.addScaled (er.ir, ratio / std::sqrt (eEr));
    if (eLate > 0) out.addScaled (late, 1.0 / std::sqrt (eLate));

    applyEq (out, p.eq);

    // pre-delay handling: shift so the first arrival lands at preDelayMs (keeps IRs tight)
    if (p.preDelayMs >= 0)
    {
        const double tFirst = std::min ({ er.directTime[0], er.directTime[1], er.directTime[2], er.directTime[3] });
        const long shift = (long) std::lround ((tFirst - p.preDelayMs * 0.001) * fs) - 64; // keep sinc/EQ pre-ringing
        if (shift > 0)
            for (auto& c : out.ch)
            {
                c.erase (c.begin(), c.begin() + std::min ((long) c.size(), shift));
                c.resize (length, 0.0);
            }
    }
    return out;
}
} // namespace irf
