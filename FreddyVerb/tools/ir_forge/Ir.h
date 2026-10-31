// ir_forge - true-stereo impulse response container and shared helpers.
#pragma once

#include "Dsp.h"

#include <array>
#include <string>
#include <vector>

namespace irf
{
/** Channel order everywhere: 0 = L->L, 1 = L->R, 2 = R->L, 3 = R->R (input -> output). */
enum Chan { LL = 0, LR = 1, RL = 2, RR = 3 };

constexpr double kOutputRate = 96000.0;

struct Ir4
{
    double fs = kOutputRate;
    std::array<std::vector<double>, 4> ch;

    Ir4() = default;
    Ir4 (double rate, size_t n) : fs (rate) { resize (n); }

    size_t size() const { return ch[0].size(); }
    void resize (size_t n) { for (auto& c : ch) c.resize (n, 0.0); }
    void scale (double g) { for (auto& c : ch) for (auto& v : c) v *= g; }

    void addScaled (const Ir4& o, double g)
    {
        if (o.size() > size()) resize (o.size());
        for (int c = 0; c < 4; ++c)
            for (size_t i = 0; i < o.size(); ++i) ch[(size_t) c][i] += g * o.ch[(size_t) c][i];
    }

    double energy (int c) const
    {
        double e = 0;
        for (auto v : ch[(size_t) c]) e += v * v;
        return e;
    }
    /** Mean energy of the direct-path channels (LL, RR). */
    double mainEnergy() const { return 0.5 * (energy (LL) + energy (RR)); }

    double peak() const
    {
        double p = 0;
        for (auto& c : ch) for (auto v : c) p = std::max (p, std::abs (v));
        return p;
    }

    void reverse() { for (auto& c : ch) std::reverse (c.begin(), c.end()); }
};

/** Simple EQ description applied to all channels. */
struct EqBand
{
    enum Type { HP, LP, Peak, LowShelf, HighShelf, HP1, LP1 };
    Type type;
    double freq;
    double q = 0.70710678;  // or shelf slope for shelves
    double db = 0.0;
};

void applyEq (Ir4& ir, const std::vector<EqBand>& eq);

/** Multiply all channels by env(t) (t in seconds). */
template <typename Fn>
void applyEnvelope (Ir4& ir, Fn&& env)
{
    for (size_t i = 0; i < ir.size(); ++i)
    {
        const double g = env ((double) i / ir.fs);
        for (auto& c : ir.ch) c[i] *= g;
    }
}

/** Smoothed broadband amplitude envelope (RMS over all channels, window in ms). */
std::vector<double> smoothedEnvelope (const Ir4& ir, double windowMs);

} // namespace irf
