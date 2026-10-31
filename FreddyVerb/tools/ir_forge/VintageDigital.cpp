// Vintage digital reverb.
//
// Late-'80s units ran their algorithms at low sample rates (~20-34 kHz) with 16-20 bit fixed-point
// arithmetic, chorused their delay lines, and used gain-ranging 12-bit converters behind steep elliptic
// anti-alias / reconstruction filters. We reproduce that chain:
//   1. the reverb algorithm (FDN, or the classic figure-of-eight allpass tank popularised by Dattorro's
//      1997 paper on Griesinger-style designs) runs at `internalFs` with deeper, slow delay modulation;
//   2. FDN state is fixed point with magnitude truncation (dead-band, so the tail still dies to silence);
//   3. a gain-ranging DAC: per 1 ms block the signal is range-switched in 6 dB steps and quantised to a
//      12-bit mantissa with first-order error-feedback noise shaping -> grit that rides on the signal;
//   4. a steep linear-phase reconstruction filter (x3 upsampling to 96 kHz) at ~10-15 kHz.
#include "VintageDigital.h"

#include <memory>

namespace irf
{
namespace
{
    class TapAllpass
    {
    public:
        void init (double len, double gain, double excursion = 0)
        {
            base = len; g = gain; exc = excursion;
            line.init ((size_t) (len + excursion + 8));
        }
        double process (double x, double mod = 0)
        {
            const double d = exc > 0 ? line.readFrac (base + exc * mod) : line.read ((size_t) std::lround (base));
            const double v = x + g * d;
            line.write (v);
            return d - g * v;
        }
        double tap (double d) const { return line.read ((size_t) std::max (1L, std::lround (d))); }

    private:
        DelayLine line;
        double base = 1, g = 0.5, exc = 0;
    };

    struct Tank
    {
        // Dattorro figure-of-eight tank
        TapAllpass apMod1, ap2, apMod3, ap4;
        DelayLine d1, d2, d3, d4;
        double l1, l2, l3, l4, sc;
        double damp1 = 0, damp2 = 0, fbL = 0, fbR = 0;
    };

    void renderDattorro (const DattorroParams& p, double fs, size_t length, const std::vector<double>& in, bool rightInput,
                         std::vector<double>& outL, std::vector<double>& outR, uint64_t seed)
    {
        const double sc = fs / 29761.0 * p.size;
        auto S = [&] (double v) { return v * sc; };

        TapAllpass inAp[4];
        const double inLens[4] = { 142, 107, 379, 277 };
        // slightly different diffusers per input to decorrelate true-stereo paths
        Rng r (seed);
        for (int i = 0; i < 4; ++i)
            inAp[i].init (S (inLens[i]) * (rightInput ? r.uniform (0.93, 1.07) : 1.0), i < 2 ? p.inputDiff1 : p.inputDiff2);

        Tank t;
        t.apMod1.init (S (672), -p.decayDiff1, S (p.excursion));
        t.apMod3.init (S (908), -p.decayDiff1, S (p.excursion));
        t.ap2.init (S (1800), p.decayDiff2);
        t.ap4.init (S (2656), p.decayDiff2);
        t.l1 = S (4453); t.l2 = S (3720); t.l3 = S (4217); t.l4 = S (3163);
        t.d1.init ((size_t) t.l1 + 4); t.d2.init ((size_t) t.l2 + 4); t.d3.init ((size_t) t.l3 + 4); t.d4.init ((size_t) t.l4 + 4);

        const double loop = S (672 + 4453 + 1800 + 3720 + 908 + 4217 + 2656 + 3163) / fs;
        const double decay = std::pow (10.0, -3.0 * loop / (4.0 * p.rt60));

        double bw = 0, ph = r.uniform (0, kTwoPi);
        const double inc = kTwoPi * p.modRateHz / fs;
        auto tapD = [] (const DelayLine& d, double v) { return d.read ((size_t) std::max (1L, std::lround (v))); };

        for (size_t n = 0; n < length; ++n)
        {
            bw = p.bandwidth * in[n] + (1 - p.bandwidth) * bw;
            double x = bw;
            for (auto& ap : inAp) x = ap.process (x);

            const double m1 = std::sin (ph), m2 = std::cos (ph * 0.93 + 1.3);
            ph += inc;

            // left half
            double a = t.apMod1.process ((rightInput ? 0.0 : x) + t.fbL, m1);
            const double d1o = t.d1.read ((size_t) std::lround (t.l1));
            t.d1.write (a);
            t.damp1 = (1 - p.damping) * d1o + p.damping * t.damp1;
            double b = t.ap2.process (t.damp1 * decay);
            const double d2o = t.d2.read ((size_t) std::lround (t.l2));
            t.d2.write (b);

            // right half
            double c = t.apMod3.process ((rightInput ? x : 0.0) + t.fbR, m2);
            const double d3o = t.d3.read ((size_t) std::lround (t.l3));
            t.d3.write (c);
            t.damp2 = (1 - p.damping) * d3o + p.damping * t.damp2;
            double e = t.ap4.process (t.damp2 * decay);
            const double d4o = t.d4.read ((size_t) std::lround (t.l4));
            t.d4.write (e);

            t.fbR = d2o * decay;  // left half feeds right
            t.fbL = d4o * decay;  // right half feeds left

            // Dattorro's output taps (paper table, scaled)
            const double yL = tapD (t.d3, S (266)) + tapD (t.d3, S (2974)) - t.ap4.tap (S (1913)) + tapD (t.d4, S (1996))
                            - tapD (t.d1, S (1990)) - t.ap2.tap (S (187)) - tapD (t.d2, S (1066));
            const double yR = tapD (t.d1, S (353)) + tapD (t.d1, S (3627)) - t.ap2.tap (S (1228)) + tapD (t.d2, S (2673))
                            - tapD (t.d3, S (2111)) - t.ap4.tap (S (335)) - tapD (t.d4, S (121));
            outL[n] = 0.6 * yL;
            outR[n] = 0.6 * yR;
        }
    }

    /** Gain-ranging DAC with first-order noise shaping. */
    void gainRangingDac (std::vector<double>& x, int bits, double fs)
    {
        if (bits <= 0) return;
        const size_t block = (size_t) std::max (8.0, fs * 0.001);
        double err = 0;
        for (size_t s = 0; s < x.size(); s += block)
        {
            const size_t e = std::min (x.size(), s + block);
            double pk = 0;
            for (size_t i = s; i < e; ++i) pk = std::max (pk, std::abs (x[i]));
            if (pk < 1.0e-12) { std::fill (x.begin() + (long) s, x.begin() + (long) e, 0.0); err = 0; continue; }
            const double range = std::pow (2.0, std::ceil (std::log2 (pk)));       // 6 dB range steps
            const double step = range * std::pow (2.0, -(bits - 1));
            for (size_t i = s; i < e; ++i)
            {
                const double v = x[i] - err;
                const double q = step * std::round (v / step);
                err = q - v;
                x[i] = q;
            }
        }
    }
} // namespace

Ir4 renderVintage (const VintageParams& p)
{
    const double fsI = p.internalFs;
    const int factor = (int) std::lround (kOutputRate / fsI);
    const size_t lenI = (size_t) (p.lengthS * fsI);
    const size_t pre = (size_t) std::lround (p.preDelayMs * 0.001 * fsI) + 4;

    std::vector<double> exc (lenI, 0.0);
    exc[pre] = p.engine == VintageParams::Fdn ? p.inputLevel : 1.0;

    Ir4 wet (fsI, lenI);
    if (p.engine == VintageParams::Fdn)
        wet = renderFdn (p.fdn, fsI, lenI, exc, exc);
    else
    {
        renderDattorro (p.dattorro, fsI, lenI, exc, false, wet.ch[LL], wet.ch[LR], p.seed);
        renderDattorro (p.dattorro, fsI, lenI, exc, true, wet.ch[RL], wet.ch[RR], p.seed);
    }

    if (p.taps.numTaps > 0)
    {
        Ir4 taps (fsI, lenI);
        Rng rng (p.seed * 31 + 7);
        for (int c = 0; c < 4; ++c)
            for (int k = 0; k < p.taps.numTaps; ++k)
            {
                const double x = (k + rng.uniform (0.0, 1.0)) / p.taps.numTaps;
                const double tMs = p.taps.minMs + (p.taps.maxMs - p.taps.minMs) * x;
                const double g = dbToGain (p.taps.decayDb * x) * rng.sign() * rng.uniform (0.6, 1.0);
                const size_t idx = pre + (size_t) std::lround (tMs * 0.001 * fsI);
                if (idx < lenI) taps.ch[(size_t) c][idx] += g;
            }
        const double eT = taps.mainEnergy(), eW = wet.mainEnergy();
        if (eT > 0 && eW > 0) wet.addScaled (taps, std::sqrt (p.taps.level * eW / eT));
    }

    for (auto& c : wet.ch) gainRangingDac (c, p.dacBits, fsI);

    Ir4 out (kOutputRate, lenI * (size_t) factor);
    for (int c = 0; c < 4; ++c)
        out.ch[(size_t) c] = upsampleInteger (wet.ch[(size_t) c], factor, p.bandwidthHz, kOutputRate);
    applyEq (out, p.eq);
    return out;
}

} // namespace irf
