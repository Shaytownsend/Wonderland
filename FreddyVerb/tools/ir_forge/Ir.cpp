#include "Ir.h"

namespace irf
{
void applyEq (Ir4& ir, const std::vector<EqBand>& eq)
{
    for (const auto& b : eq)
    {
        for (auto& c : ir.ch)
        {
            if (b.type == EqBand::HP1 || b.type == EqBand::LP1)
            {
                auto f = b.type == EqBand::HP1 ? FirstOrder::highpass (ir.fs, b.freq) : FirstOrder::lowpass (ir.fs, b.freq);
                for (auto& v : c) v = f.process (v);
                continue;
            }
            Biquad q;
            switch (b.type)
            {
                case EqBand::HP:        q = Biquad::highpass (ir.fs, b.freq, b.q); break;
                case EqBand::LP:        q = Biquad::lowpass (ir.fs, b.freq, b.q); break;
                case EqBand::Peak:      q = Biquad::peaking (ir.fs, b.freq, b.q, b.db); break;
                case EqBand::LowShelf:  q = Biquad::lowShelf (ir.fs, b.freq, b.db, b.q); break;
                case EqBand::HighShelf: q = Biquad::highShelf (ir.fs, b.freq, b.db, b.q); break;
                default: break;
            }
            for (auto& v : c) v = q.process (v);
        }
    }
}

std::vector<double> smoothedEnvelope (const Ir4& ir, double windowMs)
{
    const size_t n = ir.size();
    std::vector<double> p (n, 0.0);
    for (size_t i = 0; i < n; ++i)
    {
        double s = 0;
        for (auto& c : ir.ch) s += c[i] * c[i];
        p[i] = s * 0.25;
    }
    // centred moving average (prefix sums), then sqrt
    std::vector<double> pre (n + 1, 0.0);
    for (size_t i = 0; i < n; ++i) pre[i + 1] = pre[i] + p[i];
    const long half = std::max (1L, (long) (windowMs * 0.001 * ir.fs * 0.5));
    std::vector<double> env (n);
    for (size_t i = 0; i < n; ++i)
    {
        const long a = std::max (0L, (long) i - half), b = std::min ((long) n, (long) i + half + 1);
        env[i] = std::sqrt ((pre[(size_t) b] - pre[(size_t) a]) / (double) (b - a));
    }
    return env;
}
} // namespace irf
