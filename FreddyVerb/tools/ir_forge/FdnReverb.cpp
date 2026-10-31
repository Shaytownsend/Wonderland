// Dense FDN late reverberator.
//
// Topology (per sample):
//   r_i  = line_i.read(m_i + mod_i(t))                    6-point Lagrange fractional read
//   y_i  = A_i(r_i)                                       per-line absorption (gain + 3 first-order shelves)
//   y_i  = AP_i(y_i)                                      optional in-loop allpasses / dispersion
//   outL = sum cL_i y_i,  outR = sum cR_i y_i              orthogonal output vectors (decorrelated L/R)
//   v    = H * (S y)                                      Hadamard (orthogonal, lossless), random sign diag S
//   line_i.write(v_i + b_i * diffuse(x))                  input via allpass diffuser, per-input b vector
//
// Absorption design: for a loop of length m samples, the gain that gives RT60 = T is
// g = 10^(-3 m / (fs T)). Per line we compute g at low/mid/high/air from the target RT60 curve and realise
// it as broadband gain g_mid times a low shelf (g_low/g_mid), high shelf (g_high/g_mid) and an air shelf
// (g_air/g_high). Because the per-pass attenuation is small (a few dB at most), first-order shelves
// cascade almost exactly in dB, so every line decays with the same RT60(f): the network has a single,
// frequency-dependent exponential decay and no line "rings" longer than the others.
#include "FdnReverb.h"

#include <memory>

namespace irf
{
namespace
{
    struct LineConfig
    {
        int delay = 1000;
        double modDepth = 0;   // samples
        double modRate = 0.2;  // Hz
        double modPhase = 0;
        uint64_t modSeed = 1;
        double gain = 1;
        FirstOrder lowShelf, highShelf, airShelf;
        std::vector<std::pair<int, double>> loopAps;
        double dispersion = 0;
        double sign = 1;
        double bL = 0, bR = 0, cL = 0, cR = 0;
    };

    struct Network
    {
        std::vector<LineConfig> lines;
        std::vector<std::pair<int, double>> diffL, diffR;
    };

    Network design (const FdnParams& p, double fs)
    {
        Network net;
        Rng rng (p.seed);
        const int N = p.numLines;
        const double modDepthS = p.modDepthMs * 0.001 * fs;

        auto delays = pickDelayLengths (rng, N, std::max (p.minDelayMs * 0.001 * fs, modDepthS + 8.0),
                                        std::max (p.maxDelayMs * 0.001 * fs, modDepthS + 16.0));

        // orthogonal output vectors: cR = cL .* balanced(+/-1) pattern
        std::vector<double> pattern ((size_t) N);
        for (int i = 0; i < N; ++i) pattern[(size_t) i] = i < N / 2 ? 1.0 : -1.0;
        for (int i = N - 1; i > 0; --i) std::swap (pattern[(size_t) i], pattern[(size_t) rng.range (0, i)]);

        // input vectors: random signs with slight magnitude variation
        std::vector<double> bL ((size_t) N), bR ((size_t) N);
        double nL = 0, nR = 0;
        for (int i = 0; i < N; ++i)
        {
            bL[(size_t) i] = rng.sign() * rng.uniform (0.75, 1.0);
            bR[(size_t) i] = rng.sign() * rng.uniform (0.75, 1.0);
            nL += bL[(size_t) i] * bL[(size_t) i];
            nR += bR[(size_t) i] * bR[(size_t) i];
        }

        net.lines.resize ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            auto& L = net.lines[(size_t) i];
            L.delay = delays[(size_t) i];
            L.modDepth = modDepthS * rng.uniform (0.55, 1.0);
            L.modRate = rng.uniform (p.modRateLo, p.modRateHi);
            L.modPhase = rng.uniform (0, kTwoPi);
            L.modSeed = rng.next();
            L.sign = rng.sign();
            L.bL = bL[(size_t) i] / std::sqrt (nL);
            L.bR = bR[(size_t) i] / std::sqrt (nR);
            L.cL = rng.sign() / std::sqrt ((double) N);
            L.cR = L.cL * pattern[(size_t) i];

            double effective = L.delay;
            for (int a = 0; a < p.loopAllpasses; ++a)
            {
                const int len = (int) std::lround (rng.uniform (p.loopApMinMs, p.loopApMaxMs) * 0.001 * fs);
                L.loopAps.push_back ({ std::max (2, len), p.loopApG * rng.sign() });
                effective += len;
            }
            if (p.loopDispersion > 0)
            {
                L.dispersion = -p.loopDispersion * rng.uniform (0.6, 1.0);
                effective += (1 - L.dispersion) / (1 + L.dispersion) * 0.5;
            }

            const double gLow = rt60ToGain (effective, fs, p.rtLow);
            const double gMid = rt60ToGain (effective, fs, p.rtMid);
            const double gHigh = rt60ToGain (effective, fs, p.rtHigh);
            const double gAir = rt60ToGain (effective, fs, p.rtAir);
            L.gain = gMid;
            L.lowShelf = FirstOrder::lowShelf (fs, p.fLow, gLow / gMid);
            L.highShelf = FirstOrder::highShelf (fs, p.fHigh, gHigh / gMid);
            L.airShelf = FirstOrder::highShelf (fs, p.fAir, gAir / gHigh);
        }

        auto makeDiff = [&] (Rng& r) {
            std::vector<std::pair<int, double>> d;
            if (p.diffStages <= 0) return d;
            auto lens = pickDelayLengths (r, p.diffStages, p.diffMinMs * 0.001 * fs, p.diffMaxMs * 0.001 * fs);
            std::sort (lens.begin(), lens.end());
            for (int k = 0; k < p.diffStages; ++k) d.push_back ({ lens[(size_t) k], p.diffG * (k % 2 ? -1.0 : 1.0) });
            return d;
        };
        Rng dl = rng.fork (11), dr = rng.fork (23);
        net.diffL = makeDiff (dl);
        net.diffR = makeDiff (dr);
        return net;
    }

    void runOnce (const FdnParams& p, const Network& net, double fs, const std::vector<double>& excite, bool rightInput,
                  std::vector<double>& outL, std::vector<double>& outR)
    {
        const int N = (int) net.lines.size();
        const size_t length = outL.size();

        // diffuse the excitation
        std::vector<double> x (length, 0.0);
        std::copy_n (excite.begin(), std::min (length, excite.size()), x.begin());
        for (auto [len, g] : (rightInput ? net.diffR : net.diffL))
        {
            SchroederAllpass ap;
            ap.init (len, g);
            for (auto& v : x) v = ap.process (v);
        }

        struct LineState
        {
            DelayLine line;
            FirstOrder ls, hs, as, disp;
            std::vector<SchroederAllpass> aps;
            std::unique_ptr<Rng> rng;
            SmoothRandom sr;
            double phase = 0, phaseInc = 0;
        };
        std::vector<LineState> st ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            const auto& c = net.lines[(size_t) i];
            auto& s = st[(size_t) i];
            s.line.init ((size_t) (c.delay + c.modDepth + 8));
            s.ls = c.lowShelf; s.hs = c.highShelf; s.as = c.airShelf;
            s.disp = FirstOrder::allpass (c.dispersion);
            for (auto [len, g] : c.loopAps) { s.aps.emplace_back(); s.aps.back().init (len, g); }
            s.rng = std::make_unique<Rng> (c.modSeed);
            s.sr.init (*s.rng, c.modRate * 1.7, fs);
            s.phase = c.modPhase;
            s.phaseInc = kTwoPi * c.modRate / fs;
        }

        const double step = p.quantBits > 0 ? std::pow (2.0, -(p.quantBits - 1)) : 0.0;
        std::vector<double> y ((size_t) N);
        for (size_t n = 0; n < length; ++n)
        {
            double oL = 0, oR = 0;
            for (int i = 0; i < N; ++i)
            {
                const auto& c = net.lines[(size_t) i];
                auto& s = st[(size_t) i];
                double v;
                if (c.modDepth > 0)
                {
                    const double m = (1.0 - p.modRandomAmt) * std::sin (s.phase) + p.modRandomAmt * s.sr.next();
                    s.phase += s.phaseInc;
                    v = s.line.readFrac ((double) c.delay + c.modDepth * m);
                }
                else
                    v = s.line.read ((size_t) c.delay);

                v = c.gain * v;
                v = s.ls.process (v);
                v = s.hs.process (v);
                v = s.as.process (v);
                for (auto& ap : s.aps) v = ap.process (v);
                if (c.dispersion != 0) v = s.disp.process (v);
                y[(size_t) i] = v;
                oL += c.cL * v;
                oR += c.cR * v;
            }
            outL[n] = oL;
            outR[n] = oR;

            for (int i = 0; i < N; ++i) y[(size_t) i] *= net.lines[(size_t) i].sign;
            fwht (y.data(), N);

            const double in = x[n];
            for (int i = 0; i < N; ++i)
            {
                const auto& c = net.lines[(size_t) i];
                double w = y[(size_t) i] + (rightInput ? c.bR : c.bL) * in;
                if (step > 0) w = step * std::trunc (w / step);
                st[(size_t) i].line.write (w);
            }
        }
    }
} // namespace

Ir4 renderFdn (const FdnParams& p, double fs, size_t length, const std::vector<double>& exciteL, const std::vector<double>& exciteR)
{
    const auto net = design (p, fs);
    Ir4 ir (fs, length);
    runOnce (p, net, fs, exciteL, false, ir.ch[LL], ir.ch[LR]);
    runOnce (p, net, fs, exciteR, true, ir.ch[RL], ir.ch[RR]);
    return ir;
}

Ir4 renderFdnImpulse (const FdnParams& p, double fs, size_t length, double tL, double tR)
{
    std::vector<double> eL (length, 0.0), eR (length, 0.0);
    addSincImpulse (eL, tL * fs + 16.0, 1.0);
    addSincImpulse (eR, tR * fs + 16.0, 1.0);
    return renderFdn (p, fs, length, eL, eR);
}

} // namespace irf
