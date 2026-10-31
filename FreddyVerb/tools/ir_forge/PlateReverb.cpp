// Modal plate reverberation.
//
// A thin rectangular plate (Kirchhoff bending stiffness D = E h^3 / 12(1 - nu^2), mass/area rho*h) with
// optional membrane tension T, simply supported edges. Mode (m, n):
//     k^2   = (m pi / Lx)^2 + (n pi / Ly)^2
//     w_mn  = sqrt( (D / rho h) k^4 + (T / rho h) k^2 )
//     phi   = sin(m pi x / Lx) sin(n pi y / Ly)
// The velocity impulse response from a point driver at x_in to a pickup at x_out is
//     h(t) = sum_mn phi(x_in) phi(x_out) cos(w_mn t) exp(-t / tau(w_mn)),
// with tau from the target RT60(f) curve (6.91 / RT60). Because bending waves are dispersive
// (group velocity ~ sqrt(f)) this reproduces the plate's characteristic "splashy" onset in which highs
// arrive before lows, its constant modal density (dense at all frequencies, no flutter), and the bright,
// shimmering tail. Mode frequencies get a tiny random detune (real plates are not ideal), which also
// lifts the degeneracies of a rational aspect ratio. Four driver/pickup combinations give true stereo.
#include "PlateReverb.h"

#include <thread>

namespace irf
{
int gInnerThreads = 4;

double interpRt (double f, double fLow, double fHigh, double fAir, double rtLow, double rtMid, double rtHigh, double rtAir)
{
    // anchors in log-frequency: (fLow/2, low) (sqrt(fLow fHigh), mid) (fHigh*1.4, high) (fAir*1.4, air)
    const double xs[4] = { std::log (fLow * 0.5), std::log (std::sqrt (fLow * fHigh)), std::log (fHigh * 1.4), std::log (fAir * 1.4) };
    const double ys[4] = { std::log (rtLow), std::log (rtMid), std::log (rtHigh), std::log (rtAir) };
    const double x = std::log (std::max (f, 1.0));
    if (x <= xs[0]) return rtLow;
    if (x >= xs[3]) return rtAir * std::pow (std::exp (x - xs[3]), -0.5); // keep falling above the air anchor
    for (int i = 0; i < 3; ++i)
        if (x <= xs[i + 1])
        {
            double t = (x - xs[i]) / (xs[i + 1] - xs[i]);
            t = 0.5 - 0.5 * std::cos (kPi * t);   // smooth joins
            return std::exp (ys[i] + (ys[i + 1] - ys[i]) * t);
        }
    return rtAir;
}

namespace
{
    struct Mode
    {
        double w;       // rad/sample
        double r;       // per-sample decay
        double a[4];    // amplitudes LL, LR, RL, RR
        size_t len;
    };
}

Ir4 renderPlate (const PlateParams& p, double fs)
{
    const size_t length = (size_t) (p.lengthS * fs);
    Rng rng (p.seed * 104729 + 3);

    const double rhoH = p.density * p.thickness;
    const double D = p.youngs * p.thickness * p.thickness * p.thickness / (12.0 * (1.0 - p.poisson * p.poisson));
    const double kappa2 = D / rhoH;     // m^4/s^2
    const double c2 = p.tension / rhoH; // m^2/s^2

    const double kMax = std::sqrt (kTwoPi * p.fMax / std::sqrt (kappa2));
    const int mMax = (int) (kMax * p.sizeX / kPi) + 1, nMax = (int) (kMax * p.sizeY / kPi) + 1;

    auto phi = [] (const double* pos, int m, int n) { return std::sin (m * kPi * pos[0]) * std::sin (n * kPi * pos[1]); };

    std::vector<Mode> modes;
    modes.reserve ((size_t) (mMax * nMax));
    for (int m = 1; m <= mMax; ++m)
        for (int n = 1; n <= nMax; ++n)
        {
            const double kx = m * kPi / p.sizeX, ky = n * kPi / p.sizeY, k2 = kx * kx + ky * ky;
            double f = std::sqrt (kappa2 * k2 * k2 + c2 * k2) / kTwoPi;
            const double jitter = rng.gauss();
            if (f > p.fMax || f < p.fMin * 0.5) continue;
            f *= 1.0 + p.detune * jitter;

            double taper = 1.0;
            if (f > p.taperHz) taper = 0.5 + 0.5 * std::cos (kPi * std::min (1.0, (f - p.taperHz) / (p.fMax - p.taperHz)));
            if (f < p.fMin) taper *= 0.5 - 0.5 * std::cos (kPi * (f - p.fMin * 0.5) / (p.fMin * 0.5));

            const double rt = interpRt (f, p.fLow, p.fHigh, p.fAir, p.rtLow, p.rtMid, p.rtHigh, p.rtAir);
            Mode md;
            md.w = kTwoPi * f / fs;
            md.r = std::exp (-6.907755 / (rt * fs));
            md.a[LL] = taper * phi (p.inL, m, n) * phi (p.outL, m, n);
            md.a[LR] = taper * phi (p.inL, m, n) * phi (p.outR, m, n);
            md.a[RL] = taper * phi (p.inR, m, n) * phi (p.outL, m, n);
            md.a[RR] = taper * phi (p.inR, m, n) * phi (p.outR, m, n);
            md.len = std::min (length, (size_t) (rt * 1.8 * fs));
            modes.push_back (md);
        }

    std::sort (modes.begin(), modes.end(), [] (const Mode& a, const Mode& b) { return a.len > b.len; });

    constexpr int B = 8;
    const size_t numBlocks = (modes.size() + B - 1) / B;
    // Fixed bucket partition (independent of thread count) so output is bit-identical for any -j.
    constexpr int kBuckets = 8;
    const int T = std::clamp (gInnerThreads, 1, kBuckets);
    std::vector<Ir4> partial ((size_t) kBuckets, Ir4 (fs, length));

    auto renderBucket = [&] (int bucket) {
        auto& out = partial[(size_t) bucket];
        double* o[4] = { out.ch[0].data(), out.ch[1].data(), out.ch[2].data(), out.ch[3].data() };
        for (size_t blk = (size_t) bucket; blk < numBlocks; blk += (size_t) kBuckets)
        {
            double zr[B], zi[B], pr[B], pi[B], a0[B], a1[B], a2[B], a3[B];
            size_t len = 0;
            for (int j = 0; j < B; ++j)
            {
                const size_t idx = blk * B + (size_t) j;
                if (idx < modes.size())
                {
                    const auto& md = modes[idx];
                    zr[j] = 1; zi[j] = 0;
                    pr[j] = md.r * std::cos (md.w); pi[j] = md.r * std::sin (md.w);
                    a0[j] = md.a[0]; a1[j] = md.a[1]; a2[j] = md.a[2]; a3[j] = md.a[3];
                    len = std::max (len, md.len);
                }
                else
                {
                    zr[j] = zi[j] = pr[j] = pi[j] = 0;
                    a0[j] = a1[j] = a2[j] = a3[j] = 0;
                }
            }
            for (size_t t = 0; t < len; ++t)
            {
                double s0 = 0, s1 = 0, s2 = 0, s3 = 0;
                for (int j = 0; j < B; ++j)
                {
                    const double r = zr[j];
                    s0 += a0[j] * r; s1 += a1[j] * r; s2 += a2[j] * r; s3 += a3[j] * r;
                    const double nr = r * pr[j] - zi[j] * pi[j];
                    zi[j] = r * pi[j] + zi[j] * pr[j];
                    zr[j] = nr;
                }
                o[0][t] += s0; o[1][t] += s1; o[2][t] += s2; o[3][t] += s3;
            }
        }
    };
    auto work = [&] (int tid) { for (int b = tid; b < kBuckets; b += T) renderBucket (b); };

    std::vector<std::thread> threads;
    for (int t = 1; t < T; ++t) threads.emplace_back (work, t);
    work (0);
    for (auto& th : threads) th.join();

    Ir4 ir (fs, length);
    for (int b = 0; b < kBuckets; ++b) ir.addScaled (partial[(size_t) b], 1.0);
    applyEq (ir, p.eq);
    return ir;
}

} // namespace irf
