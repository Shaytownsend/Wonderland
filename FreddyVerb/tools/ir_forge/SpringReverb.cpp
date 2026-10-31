// Spring reverberation (after Välimäki, Parker & Abel, "Parametric spring reverberation effect", 2010).
//
// Each spring is a feedback loop:
//     u   = x + g * HP(LP(delay(c)))           round-trip delay with slow, partly random modulation
//     c   = C(u)                               cascade of M stretched allpasses (a + z^-K)/(1 + a z^-K)
//     out = c
// The stretched allpass has group delay  K (1 - a^2) / (1 + 2 a cos(K w) + a^2), which (for a > 0) rises
// from DC up to the transition frequency fC = fs / 2K. Cascading ~100 of them gives tens of milliseconds of
// frequency-dependent delay: every echo is a chirp in which lows arrive first and the band near fC last,
// the "boing"/"drip" of a real tank. Because the chirp sits inside the loop the dispersion accumulates with
// each round trip, so later echoes smear more, exactly like a physical spring. The modulated loop delay
// adds flutter. Two or three springs of different length are mixed into the four true-stereo paths.
#include "SpringReverb.h"

#include <memory>

namespace irf
{
namespace
{
    class StretchedAllpass
    {
    public:
        void init (int K, double a)
        {
            k = K; c = a;
            xs.assign ((size_t) K, 0.0);
            ys.assign ((size_t) K, 0.0);
            pos = 0;
        }
        double process (double x)
        {
            const double xd = xs[pos], yd = ys[pos];
            const double y = c * x + xd - c * yd;
            xs[pos] = x; ys[pos] = y;
            if (++pos == (size_t) k) pos = 0;
            return y;
        }

    private:
        int k = 1;
        double c = 0;
        std::vector<double> xs, ys;
        size_t pos = 0;
    };

    std::vector<double> renderUnit (const SpringUnit& s, double fs, size_t length, uint64_t seed)
    {
        const int K = std::max (1, (int) std::lround (fs / (2.0 * s.transitionHz)));
        std::vector<StretchedAllpass> chain ((size_t) s.stages);
        Rng rng (seed);
        for (auto& ap : chain) ap.init (K, s.apCoef * rng.uniform (0.97, 1.03));

        // loop delay at 1 kHz = pure delay + chirp group delay at 1 kHz (for the RT60 gain)
        const double wk = K * kTwoPi * 1000.0 / fs;
        const double a = s.apCoef;
        const double gdStage = K * (1 - a * a) / (1 + 2 * a * std::cos (wk) + a * a);
        const double loopSamples = s.loopMs * 0.001 * fs + s.stages * gdStage;
        const double g = rt60ToGain (loopSamples, fs, s.rt60);

        DelayLine dl;
        const double baseDelay = s.loopMs * 0.001 * fs;
        dl.init ((size_t) (baseDelay + s.modDepthSamples + 16));
        auto lp = Biquad::lowpass (fs, s.hfDampHz, 0.6);
        auto hp = FirstOrder::highpass (fs, s.lfDampHz);
        auto modRng = std::make_unique<Rng> (seed ^ 0xABCDEFull);
        SmoothRandom sr;
        sr.init (*modRng, s.modRateHz * 2.3, fs);
        double phase = rng.uniform (0, kTwoPi);
        const double inc = kTwoPi * s.modRateHz / fs;

        std::vector<double> out (length, 0.0);
        for (size_t n = 0; n < length; ++n)
        {
            const double mod = s.modDepthSamples * ((1 - s.noiseAmt) * std::sin (phase) + s.noiseAmt * sr.next());
            phase += inc;
            const double fb = hp.process (lp.process (dl.readFrac (baseDelay + mod)));
            double u = (n == 0 ? 1.0 : 0.0) + g * fb;
            for (auto& ap : chain) u = ap.process (u);
            dl.write (u);
            out[n] = u;
        }
        return out;
    }
} // namespace

Ir4 renderSpring (const SpringParams& p, double fs)
{
    const size_t length = (size_t) (p.lengthS * fs);
    Ir4 ir (fs, length);
    Rng rng (p.seed);
    for (const auto& s : p.springs)
    {
        const auto h = renderUnit (s, fs, length, rng.next());
        for (int in = 0; in < 2; ++in)
            for (int out = 0; out < 2; ++out)
            {
                const double g = s.inputGain[in] * s.outputGain[out];
                auto& c = ir.ch[(size_t) (in * 2 + out)];
                for (size_t i = 0; i < length; ++i) c[i] += g * h[i];
            }
    }
    applyEq (ir, p.eq);
    return ir;
}

} // namespace irf
