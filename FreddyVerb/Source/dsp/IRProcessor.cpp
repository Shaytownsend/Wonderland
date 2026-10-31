#include "IRProcessor.h"
#include "DspCommon.h"
#include <CDSPResampler.h>
#include <complex>
#include <numeric>

namespace fv::dsp
{
namespace
{
    using cd = std::complex<double>;

    /** In-place iterative radix-2 complex FFT (offline use only). */
    void fftInPlace (std::vector<cd>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[i], a[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = 2.0 * kPi / (double) len * (inverse ? 1.0 : -1.0);
            const cd wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                cd w (1.0, 0.0);
                for (size_t k = 0; k < len / 2; ++k)
                {
                    const cd u = a[i + k], v = a[i + k + len / 2] * w;
                    a[i + k] = u + v;
                    a[i + k + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
        if (inverse)
            for (auto& x : a) x /= (double) n;
    }

    int nextPow2 (int v) { int p = 1; while (p < v) p <<= 1; return p; }

    /** RBJ biquad (double), used for K-weighting. */
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        static Biquad highShelf (double fs, double f0, double gainDb, double q)
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f0 / fs, cw = std::cos (w), sw = std::sin (w);
            const double alpha = sw / (2 * q), sa = 2 * std::sqrt (A) * alpha;
            const double a0 = (A + 1) - (A - 1) * cw + sa;
            Biquad b;
            b.b0 = A * ((A + 1) + (A - 1) * cw + sa) / a0;
            b.b1 = -2 * A * ((A - 1) + (A + 1) * cw) / a0;
            b.b2 = A * ((A + 1) + (A - 1) * cw - sa) / a0;
            b.a1 = 2 * ((A - 1) - (A + 1) * cw) / a0;
            b.a2 = ((A + 1) - (A - 1) * cw - sa) / a0;
            return b;
        }
        static Biquad highPass (double fs, double f0, double q)
        {
            const double w = 2 * kPi * f0 / fs, cw = std::cos (w), alpha = std::sin (w) / (2 * q);
            const double a0 = 1 + alpha;
            Biquad b;
            b.b0 = (1 + cw) / 2 / a0; b.b1 = -(1 + cw) / a0; b.b2 = (1 + cw) / 2 / a0;
            b.a1 = -2 * cw / a0; b.a2 = (1 - alpha) / a0;
            return b;
        }
    };

    double kWeightedEnergy (const std::vector<double>& x, double fs)
    {
        auto shelf = Biquad::highShelf (fs, 1681.97, 4.0, 0.7071);
        auto hp = Biquad::highPass (fs, 38.13, 0.5003);
        double e = 0;
        for (double v : x) { const double y = hp.process (shelf.process (v)); e += y * y; }
        // let the filters ring out
        for (int i = 0; i < (int) (fs * 0.05); ++i) { const double y = hp.process (shelf.process (0.0)); e += y * y; }
        return e;
    }
}

// ---------------------------------------------------------------------------
std::vector<double> IRProcessor::resample (const std::vector<double>& x, double fromRate, double toRate)
{
    if (x.empty()) return {};
    if (std::abs (fromRate - toRate) < 1.0e-9) return x;
    const int inLen = (int) x.size();
    const int outLen = std::max (1, (int) std::ceil ((double) inLen * toRate / fromRate));
    r8b::CDSPResampler24 rs (fromRate, toRate, inLen);
    std::vector<double> out ((size_t) outLen, 0.0);
    rs.oneshot (x.data(), inLen, out.data(), outLen);
    return out;
}

double IRProcessor::estimateRt60 (const std::vector<double>& x, double fs)
{
    if (x.size() < 16) return 0.0;
    std::vector<double> sch (x.size());
    double acc = 0;
    for (size_t i = x.size(); i-- > 0;) { acc += x[i] * x[i]; sch[i] = acc; }
    if (acc <= 0) return 0.0;
    auto timeAt = [&] (double db) -> double
    {
        const double thr = acc * std::pow (10.0, db / 10.0);
        for (size_t i = 0; i < sch.size(); ++i) if (sch[i] <= thr) return (double) i / fs;
        return -1.0;
    };
    const double t5 = timeAt (-5.0), t35 = timeAt (-35.0), t25 = timeAt (-25.0), t10 = timeAt (-10.0);
    if (t5 >= 0 && t35 > t5) return 2.0 * (t35 - t5);
    if (t5 >= 0 && t25 > t5) return 3.0 * (t25 - t5);
    if (t10 > 0) return 6.0 * t10;
    return (double) x.size() / fs;
}

IRSource IRProcessor::upmixToFourPaths (const IRSource& src)
{
    IRSource out;
    out.sampleRate = src.sampleRate;
    const int n = src.length();
    out.channels.assign (4, std::vector<double> ((size_t) n, 0.0));
    if (src.channels.size() >= 4)
        for (int c = 0; c < 4; ++c) out.channels[(size_t) c] = src.channels[(size_t) c];
    else if (src.channels.size() >= 2)
    {
        out.channels[IRData::LL] = src.channels[0];
        out.channels[IRData::RR] = src.channels[1];
    }
    else if (src.channels.size() == 1)
    {
        out.channels[IRData::LL] = src.channels[0];
        out.channels[IRData::RR] = src.channels[0];
    }
    return out;
}

double IRProcessor::loudnessNormalisationGain (const IRData& ir)
{
    double eL = 0, eR = 0;
    if (ir.active[IRData::LL]) eL += kWeightedEnergy (ir.path[IRData::LL], ir.sampleRate);
    if (ir.active[IRData::RL]) eL += kWeightedEnergy (ir.path[IRData::RL], ir.sampleRate);
    if (ir.active[IRData::LR]) eR += kWeightedEnergy (ir.path[IRData::LR], ir.sampleRate);
    if (ir.active[IRData::RR]) eR += kWeightedEnergy (ir.path[IRData::RR], ir.sampleRate);
    const double e = 0.5 * (eL + eR);
    return e > 1.0e-30 ? std::sqrt (kTargetEnergy / e) : 1.0;
}

void IRProcessor::applyDamping (std::vector<double>& x, double fs, double amount, double corner)
{
    if (amount <= 0.0 || x.empty()) return;
    const int N = nextPow2 ((int) (0.021 * fs));     // ~21 ms frames
    const int hop = N / 4;
    const int len = (int) x.size();
    std::vector<double> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = std::sqrt (0.5 - 0.5 * std::cos (2.0 * kPi * i / N)); // sqrt-Hann (periodic)
    std::vector<double> padded ((size_t) (len + 2 * N), 0.0), outp ((size_t) (len + 2 * N), 0.0);
    std::copy (x.begin(), x.end(), padded.begin() + N);

    // 100% damping: frequencies far above the corner lose an extra 60 dB every 0.35 s
    const double alpha = amount * std::log (1000.0) / 0.35;
    std::vector<double> shape ((size_t) (N / 2 + 1));
    for (int k = 0; k <= N / 2; ++k)
    {
        const double f = (double) k * fs / N;
        shape[(size_t) k] = (f * f) / (f * f + corner * corner);
    }
    std::vector<cd> buf ((size_t) N);
    for (int start = 0; start + N <= (int) padded.size(); start += hop)
    {
        const double tCentre = std::max (0.0, (double) (start + N / 2 - N) / fs);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = cd (padded[(size_t) (start + i)] * win[(size_t) i], 0.0);
        fftInPlace (buf, false);
        for (int k = 0; k <= N / 2; ++k)
        {
            const double g = std::exp (-alpha * tCentre * shape[(size_t) k]);
            buf[(size_t) k] *= g;
            if (k > 0 && k < N / 2) buf[(size_t) (N - k)] *= g;
        }
        fftInPlace (buf, true);
        for (int i = 0; i < N; ++i) outp[(size_t) (start + i)] += buf[(size_t) i].real() * win[(size_t) i];
    }
    // sqrt-Hann^2 at 75% overlap sums to 2.0
    for (int i = 0; i < len; ++i) x[(size_t) i] = outp[(size_t) (i + N)] / 2.0;
}

// ---------------------------------------------------------------------------
IRData IRProcessor::render (const IRSource& srcIn, const IRShapeParams& sp, double sessionRate, IRDisplayInfo* info)
{
    IRData ir;
    ir.sampleRate = sessionRate;
    if (srcIn.length() == 0)
    {
        ir.path[IRData::LL] = { 1.0 }; ir.path[IRData::RR] = { 1.0 };
        ir.active[IRData::LL] = ir.active[IRData::RR] = true;
        return ir;
    }
    auto src = upmixToFourPaths (srcIn);
    for (int p = 0; p < 4; ++p)
    {
        const auto& ch = src.channels[(size_t) p];
        ir.active[(size_t) p] = std::any_of (ch.begin(), ch.end(), [] (double v) { return v != 0.0; });
    }

    // 1) resample to the session rate; size stretches by resampling to a scaled rate
    const double size = std::clamp ((double) sp.sizePct, 50.0, 200.0) / 100.0;
    const double targetRate = sessionRate * size;
    for (int p = 0; p < 4; ++p)
        if (ir.active[(size_t) p])
            ir.path[(size_t) p] = resample (src.channels[(size_t) p], src.sampleRate, targetRate);
    int len = ir.length();
    for (int p = 0; p < 4; ++p)
        if (ir.active[(size_t) p]) ir.path[(size_t) p].resize ((size_t) len, 0.0);

    // reference mono sum for analysis
    auto monoSum = [&]
    {
        std::vector<double> m ((size_t) ir.length(), 0.0);
        for (int p = 0; p < 4; ++p)
            if (ir.active[(size_t) p])
                for (size_t i = 0; i < m.size() && i < ir.path[(size_t) p].size(); ++i) m[i] += ir.path[(size_t) p][i];
        return m;
    };
    const double rt0 = std::max (0.02, estimateRt60 (monoSum(), sessionRate));
    if (info) info->sourceRt60 = rt0;

    // 2) loudness normalisation of the (resampled, unshaped) IR
    if (sp.normalize)
    {
        const double g = loudnessNormalisationGain (ir);
        for (auto& v : ir.path) for (auto& s : v) s *= g;
    }

    // 3) decay reshaping: multiply by exp(-6.91 t (1/RT1 - 1/RT0)), capped at +24 dB
    const double d = std::clamp ((double) sp.decayPct, 25.0, 200.0) / 100.0;
    if (std::abs (d - 1.0) > 1.0e-6)
    {
        const double k = 6.907755 * (1.0 / (rt0 * d) - 1.0 / rt0);
        const double cap = std::pow (10.0, 24.0 / 20.0);
        for (int p = 0; p < 4; ++p)
            if (ir.active[(size_t) p])
            {
                auto& v = ir.path[(size_t) p];
                for (size_t i = 0; i < v.size(); ++i)
                    v[i] *= std::min (cap, std::exp (-k * (double) i / sessionRate));
            }
    }

    // 4) damping: time-varying spectral decay (highs die faster)
    if (sp.dampingPct > 0.0f)
        for (int p = 0; p < 4; ++p)
            if (ir.active[(size_t) p])
                applyDamping (ir.path[(size_t) p], sessionRate, sp.dampingPct / 100.0, sp.dampFreqHz);

    // 5) early / late balance with a 5 ms crossfade around the split
    if (sp.earlyDb != 0.0f || sp.lateDb != 0.0f)
    {
        const double ge = dbToGain ((double) sp.earlyDb), gl = dbToGain ((double) sp.lateDb);
        const double x0 = (sp.xoverMs - 2.5) * 0.001 * sessionRate, x1 = (sp.xoverMs + 2.5) * 0.001 * sessionRate;
        for (int p = 0; p < 4; ++p)
            if (ir.active[(size_t) p])
            {
                auto& v = ir.path[(size_t) p];
                for (size_t i = 0; i < v.size(); ++i)
                {
                    const double t = (double) i;
                    const double w = t <= x0 ? 0.0 : (t >= x1 ? 1.0 : (t - x0) / (x1 - x0));
                    v[i] *= ge * (1.0 - w) + gl * w;
                }
            }
    }

    // 6) reverse
    if (sp.reverse)
        for (auto& v : ir.path) std::reverse (v.begin(), v.end());

    // 7) start trim with a 1 ms raised-cosine fade-in
    const int trim = std::min (len - 1, (int) std::lround (std::clamp ((double) sp.startMs, 0.0, 100.0) * 0.001 * sessionRate));
    if (trim > 0)
    {
        const int fadeIn = std::max (1, (int) (0.001 * sessionRate));
        for (int p = 0; p < 4; ++p)
            if (ir.active[(size_t) p])
            {
                auto& v = ir.path[(size_t) p];
                v.erase (v.begin(), v.begin() + trim);
                for (int i = 0; i < std::min (fadeIn, (int) v.size()); ++i)
                    v[(size_t) i] *= 0.5 - 0.5 * std::cos (kPi * (double) i / fadeIn);
            }
        len = ir.length();
    }

    // 8) length: keep lengthPct of the IR, raised-cosine fade over the last 10% of what's kept
    const double lp = std::clamp ((double) sp.lengthPct, 10.0, 100.0) / 100.0;
    const int keep = std::max (1, (int) std::lround (len * lp));
    if (keep < len || lp < 1.0)
    {
        const int fade = std::max (1, std::min (keep, (int) (keep * 0.1)));
        for (int p = 0; p < 4; ++p)
            if (ir.active[(size_t) p])
            {
                auto& v = ir.path[(size_t) p];
                v.resize ((size_t) keep);
                for (int i = 0; i < fade; ++i)
                    v[(size_t) (keep - fade + i)] *= 0.5 + 0.5 * std::cos (kPi * (double) (i + 1) / fade);
            }
    }

    // inactive paths stay empty
    for (int p = 0; p < 4; ++p) if (! ir.active[(size_t) p]) ir.path[(size_t) p].clear();

    if (info != nullptr)
    {
        const auto m = monoSum();
        info->rt60 = estimateRt60 (m, sessionRate);
        info->lengthSeconds = (double) m.size() / sessionRate;
        const int points = 400;
        info->peaks.assign ((size_t) points, 0.0f);
        double peak = 1.0e-12;
        for (int p = 0; p < 4; ++p)
            for (double v : ir.path[(size_t) p]) peak = std::max (peak, std::abs (v));
        for (int p = 0; p < 4; ++p)
        {
            const auto& v = ir.path[(size_t) p];
            for (size_t i = 0; i < v.size(); ++i)
            {
                auto& pk = info->peaks[std::min ((size_t) points - 1, i * (size_t) points / std::max<size_t> (1, v.size()))];
                pk = std::max (pk, (float) (std::abs (v[i]) / peak));
            }
        }
        // decay spectrogram: 8 bands x 96 columns, log magnitude mapped to 0..1 over 60 dB
        const int cols = 96, bands = 8;
        info->spectrogram.assign ((size_t) bands, std::vector<float> ((size_t) cols, 0.0f));
        const int N = 1024;
        std::vector<cd> buf ((size_t) N);
        double maxE = 1.0e-30;
        std::vector<std::vector<double>> e ((size_t) bands, std::vector<double> ((size_t) cols, 0.0));
        for (int c = 0; c < cols; ++c)
        {
            const int start = (int) ((double) c / cols * (double) m.size());
            for (int i = 0; i < N; ++i)
            {
                const int idx = start + i;
                const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * i / N);
                buf[(size_t) i] = cd (idx < (int) m.size() ? m[(size_t) idx] * w : 0.0, 0.0);
            }
            fftInPlace (buf, false);
            for (int k = 1; k < N / 2; ++k)
            {
                const double f = (double) k * sessionRate / N;
                const int b = std::clamp ((int) std::floor (std::log2 (std::max (1.0, f / 80.0))), 0, bands - 1);
                e[(size_t) b][(size_t) c] += std::norm (buf[(size_t) k]);
            }
            for (int b = 0; b < bands; ++b) maxE = std::max (maxE, e[(size_t) b][(size_t) c]);
        }
        for (int b = 0; b < bands; ++b)
            for (int c = 0; c < cols; ++c)
            {
                const double db = 10.0 * std::log10 (std::max (1.0e-30, e[(size_t) b][(size_t) c] / maxE));
                info->spectrogram[(size_t) b][(size_t) c] = (float) std::clamp ((db + 60.0) / 60.0, 0.0, 1.0);
            }
    }
    return ir;
}
} // namespace fv::dsp
