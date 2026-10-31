// ir_forge - shared DSP primitives (header-only, double precision, offline use).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace irf
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kSpeedOfSound = 343.0;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-30)); }

/** Per-pass gain for a delay of `samples` so that the loop decays by 60 dB in rt60 seconds. */
inline double rt60ToGain (double samples, double fs, double rt60)
{
    return std::pow (10.0, -3.0 * samples / (fs * std::max (rt60, 1.0e-4)));
}

//==============================================================================
/** xoshiro256** seeded through splitmix64. Fully deterministic across platforms. */
class Rng
{
public:
    explicit Rng (uint64_t seed)
    {
        uint64_t z = seed + 0x9E3779B97F4A7C15ull;
        for (auto& s : state)
        {
            z += 0x9E3779B97F4A7C15ull;
            uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            s = x ^ (x >> 31);
        }
    }

    uint64_t next()
    {
        const uint64_t result = rotl (state[1] * 5, 7) * 9;
        const uint64_t t = state[1] << 17;
        state[2] ^= state[0];
        state[3] ^= state[1];
        state[1] ^= state[2];
        state[0] ^= state[3];
        state[2] ^= t;
        state[3] = rotl (state[3], 45);
        return result;
    }

    double uniform() { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }
    double uniform (double lo, double hi) { return lo + (hi - lo) * uniform(); }
    double bipolar() { return uniform (-1.0, 1.0); }
    bool coin() { return (next() >> 63) != 0; }
    double sign() { return coin() ? 1.0 : -1.0; }
    int range (int lo, int hiInclusive) { return lo + (int) (next() % (uint64_t) (hiInclusive - lo + 1)); }

    double gauss()
    {
        if (hasSpare) { hasSpare = false; return spare; }
        double u, v, s;
        do { u = bipolar(); v = bipolar(); s = u * u + v * v; } while (s >= 1.0 || s == 0.0);
        const double m = std::sqrt (-2.0 * std::log (s) / s);
        spare = v * m;
        hasSpare = true;
        return u * m;
    }

    /** Derive an independent child generator (stable for a given tag). */
    Rng fork (uint64_t tag) { return Rng (next() ^ (tag * 0xD1B54A32D192ED03ull)); }

private:
    static uint64_t rotl (uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    uint64_t state[4] {};
    bool hasSpare = false;
    double spare = 0.0;
};

//==============================================================================
/** Transposed direct form II biquad with RBJ-cookbook designs. */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }

    static Biquad make (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        Biquad q;
        q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0; q.a1 = a1 / a0; q.a2 = a2 / a0;
        return q;
    }
    static Biquad lowpass (double fs, double f, double Q = 0.70710678)
    {
        const double w = kTwoPi * std::min (f, fs * 0.49) / fs, c = std::cos (w), al = std::sin (w) / (2 * Q);
        return make ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    static Biquad highpass (double fs, double f, double Q = 0.70710678)
    {
        const double w = kTwoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * Q);
        return make ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    static Biquad bandpass (double fs, double f, double Q)
    {
        const double w = kTwoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * Q);
        return make (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    static Biquad peaking (double fs, double f, double Q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = kTwoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * Q);
        return make (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    static Biquad lowShelf (double fs, double f, double db, double S = 1.0)
    {
        const double A = std::pow (10.0, db / 40.0), w = kTwoPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sq = 2 * std::sqrt (A) * al;
        return make (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                     (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    static Biquad highShelf (double fs, double f, double db, double S = 1.0)
    {
        const double A = std::pow (10.0, db / 40.0), w = kTwoPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sq = 2 * std::sqrt (A) * al;
        return make (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                     (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

/** First-order IIR section (bilinear designs). */
struct FirstOrder
{
    double b0 = 1, b1 = 0, a1 = 0, z = 0;

    double process (double x)
    {
        const double y = b0 * x + z;
        z = b1 * x - a1 * y;
        return y;
    }
    void reset() { z = 0; }

    static FirstOrder fromAnalog (double nb0, double nb1, double na0, double na1)
    {
        // (nb0 + nb1 z^-1) / (na0 + na1 z^-1)
        FirstOrder f;
        f.b0 = nb0 / na0; f.b1 = nb1 / na0; f.a1 = na1 / na0;
        return f;
    }
    /** Low shelf: gain V below fc, unity above. Pole/zero symmetric about fc in log-frequency. */
    static FirstOrder lowShelf (double fs, double fc, double V)
    {
        const double w = std::tan (kPi * fc / fs), r = std::sqrt (V);
        return fromAnalog (1 + w * r, w * r - 1, 1 + w / r, w / r - 1);
    }
    /** High shelf: unity below fc, gain V above. */
    static FirstOrder highShelf (double fs, double fc, double V)
    {
        const double w = std::tan (kPi * fc / fs), r = std::sqrt (V);
        return fromAnalog (r + w, w - r, 1 / r + w, w - 1 / r);
    }
    static FirstOrder lowpass (double fs, double fc)
    {
        const double w = std::tan (kPi * std::min (fc, fs * 0.49) / fs);
        return fromAnalog (w, w, 1 + w, w - 1);
    }
    static FirstOrder highpass (double fs, double fc)
    {
        const double w = std::tan (kPi * fc / fs);
        return fromAnalog (1, -1, 1 + w, w - 1);
    }
    /** Allpass (c + z^-1)/(1 + c z^-1). */
    static FirstOrder allpass (double c)
    {
        FirstOrder f;
        f.b0 = c; f.b1 = 1; f.a1 = c;
        return f;
    }
};

//==============================================================================
/** Circular delay line with high-quality fractional reads (6-point Lagrange, order 5). */
class DelayLine
{
public:
    void init (size_t maxDelay)
    {
        size_t n = 16;
        while (n < maxDelay + 8) n <<= 1;
        buf.assign (n, 0.0);
        mask = n - 1;
        w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0); w = 0; }
    void write (double x) { buf[w] = x; w = (w + 1) & mask; }
    /** Sample written `d` writes ago (d >= 1). */
    double read (size_t d) const { return buf[(w - d) & mask]; }

    double readFrac (double d) const
    {
        const double fl = std::floor (d);
        const double f = d - fl;
        const size_t i = (size_t) fl;
        // taps at delays i-2 .. i+3; interpolation point x = 2 + f in 0..5
        const double x = 2.0 + f;
        double out = 0.0;
        for (int k = 0; k < 6; ++k)
        {
            double h = 1.0;
            for (int j = 0; j < 6; ++j)
                if (j != k) h *= (x - j) / (double) (k - j);
            out += h * read (i - 2 + (size_t) k);
        }
        return out;
    }

private:
    std::vector<double> buf;
    size_t mask = 0, w = 0;
};

/** Schroeder allpass  H(z) = (-g + z^-M) / (1 - g z^-M). */
class SchroederAllpass
{
public:
    void init (int delaySamples, double gain) { M = std::max (1, delaySamples); g = gain; line.init ((size_t) M + 1); }
    void reset() { line.reset(); }
    double process (double x)
    {
        const double d = line.read ((size_t) M);
        const double v = x + g * d;
        line.write (v);
        return d - g * v;
    }
    int length() const { return M; }

private:
    DelayLine line;
    int M = 1;
    double g = 0.5;
};

/** Smooth random signal in roughly [-1, 1]: random breakpoints at ~rateHz joined by raised-cosine segments. */
class SmoothRandom
{
public:
    void init (Rng& rng, double rateHz, double fs)
    {
        r = &rng;
        period = std::max (2.0, fs / std::max (rateHz, 1.0e-3));
        a = r->bipolar(); b = r->bipolar();
        phase = r->uniform();
        curPeriod = period * r->uniform (0.7, 1.3);
    }
    double next()
    {
        const double t = 0.5 - 0.5 * std::cos (kPi * phase);
        const double y = a + (b - a) * t;
        phase += 1.0 / curPeriod;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            a = b;
            b = r->bipolar();
            curPeriod = period * r->uniform (0.7, 1.3);
        }
        return y;
    }

private:
    Rng* r = nullptr;
    double period = 1000, curPeriod = 1000, phase = 0, a = 0, b = 0;
};

//==============================================================================
/** Add a band-limited (Blackman-windowed sinc) impulse at fractional position `pos`. */
inline void addSincImpulse (std::vector<double>& buf, double pos, double amp, int halfWidth = 16)
{
    const long base = (long) std::floor (pos);
    for (long k = base - halfWidth + 1; k <= base + halfWidth; ++k)
    {
        if (k < 0 || k >= (long) buf.size()) continue;
        const double x = (double) k - pos;
        const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (kPi * x) / (kPi * x);
        const double wn = (x + halfWidth) / (2.0 * halfWidth);
        if (wn <= 0.0 || wn >= 1.0) continue;
        const double win = 0.42 - 0.5 * std::cos (kTwoPi * wn) + 0.08 * std::cos (2 * kTwoPi * wn);
        buf[(size_t) k] += amp * sinc * win;
    }
}

/** In-place normalised fast Walsh-Hadamard transform (n must be a power of two). Orthogonal. */
inline void fwht (double* x, int n)
{
    for (int len = 1; len < n; len <<= 1)
        for (int i = 0; i < n; i += len << 1)
            for (int j = i; j < i + len; ++j)
            {
                const double a = x[j], b = x[j + len];
                x[j] = a + b;
                x[j + len] = a - b;
            }
    const double s = 1.0 / std::sqrt ((double) n);
    for (int i = 0; i < n; ++i) x[i] *= s;
}

inline double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 50; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1e-17 * sum) break;
    }
    return sum;
}

/** Kaiser-windowed sinc lowpass. cutoff is normalised (cycles/sample, 0..0.5). Unity DC gain. */
inline std::vector<double> designLowpassFir (int numTaps, double cutoff, double beta)
{
    std::vector<double> h ((size_t) numTaps);
    const double mid = 0.5 * (numTaps - 1);
    double sum = 0;
    for (int i = 0; i < numTaps; ++i)
    {
        const double x = i - mid;
        const double s = std::abs (x) < 1e-12 ? 2 * cutoff : std::sin (kTwoPi * cutoff * x) / (kPi * x);
        const double r = x / (mid + 1.0);
        const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1 - r * r))) / besselI0 (beta);
        h[(size_t) i] = s * w;
        sum += h[(size_t) i];
    }
    for (auto& v : h) v /= sum;
    return h;
}

/** Integer-factor upsampler: zero-stuff + steep linear-phase FIR (group delay compensated). */
inline std::vector<double> upsampleInteger (const std::vector<double>& x, int factor, double cutoffHz, double fsOut)
{
    const int taps = 64 * factor * 2 + 1;
    auto h = designLowpassFir (taps, cutoffHz / fsOut, 10.0);
    for (auto& v : h) v *= factor;
    const int delay = (taps - 1) / 2;
    std::vector<double> y (x.size() * (size_t) factor, 0.0);
    for (size_t n = 0; n < y.size(); ++n)
    {
        // y[n] = sum_k h[k] * up[n + delay - k] where up[m] = x[m/factor] if m % factor == 0
        const long centre = (long) n + delay;
        double acc = 0;
        long kStart = centre % factor;
        for (long k = kStart; k < taps; k += factor)
        {
            const long m = centre - k;
            if (m < 0) break;
            const long idx = m / factor;
            if (idx >= (long) x.size()) continue;
            acc += h[(size_t) k] * x[(size_t) idx];
        }
        y[n] = acc;
    }
    return y;
}

inline bool isPrime (long n)
{
    if (n < 2) return false;
    for (long d = 2; d * d <= n; ++d)
        if (n % d == 0) return false;
    return true;
}

/** n distinct prime delay lengths spread geometrically between lo and hi (samples), jittered, shuffled. */
inline std::vector<int> pickDelayLengths (Rng& rng, int n, double lo, double hi)
{
    std::vector<int> out;
    const double ratio = hi / lo;
    for (int i = 0; i < n; ++i)
    {
        const double t = n > 1 ? (i + rng.uniform (-0.35, 0.35)) / (double) (n - 1) : 0.5;
        long v = (long) std::lround (lo * std::pow (ratio, std::clamp (t, 0.0, 1.0)));
        v = std::max (v, 7L);
        while (! isPrime (v) || std::find (out.begin(), out.end(), (int) v) != out.end()) ++v;
        out.push_back ((int) v);
    }
    for (int i = n - 1; i > 0; --i) std::swap (out[(size_t) i], out[(size_t) rng.range (0, i)]);
    return out;
}

} // namespace irf
