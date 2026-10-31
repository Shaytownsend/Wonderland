#pragma once

#include <vector>
#include <cmath>
#include <random>
#include "dsp/FreddyEngine.h"
#include "dsp/EngineParams.h"

namespace fvtest
{
/** Engine params with everything that colours the wet path switched off. */
inline fv::dsp::EngineParams neutralParams()
{
    fv::dsp::EngineParams p;
    p.duckOn = false;
    p.gateOn = false;
    p.eqOn = false;
    p.predelayMs = 0.0f;
    p.widthPct = 100.0f;
    p.balance = 0.0f;
    return p;
}

/** Runs the engine over a whole signal using a given (possibly varying) block size. */
inline void runEngine (fv::dsp::FreddyEngine& e, const fv::dsp::EngineParams& p,
                       const std::vector<float>& inL, const std::vector<float>& inR,
                       std::vector<float>& outL, std::vector<float>& outR, int blockSize, bool randomBlocks = false)
{
    const int n = (int) inL.size();
    outL.assign ((size_t) n, 0.0f);
    outR.assign ((size_t) n, 0.0f);
    std::mt19937 rng (99);
    std::uniform_int_distribution<int> bs (1, blockSize);
    for (int pos = 0; pos < n;)
    {
        const int b = std::min (n - pos, randomBlocks ? bs (rng) : blockSize);
        const float* in[2] { inL.data() + pos, inR.data() + pos };
        float* out[2] { outL.data() + pos, outR.data() + pos };
        e.process (in, 2, out, b, p);
        pos += b;
    }
}

inline std::vector<float> noise (int n, unsigned seed = 1, float amp = 0.5f)
{
    std::vector<float> v ((size_t) n);
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (-amp, amp);
    for (auto& x : v) x = u (rng);
    return v;
}

inline double maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b, int from = 0)
{
    double m = 0;
    for (size_t i = (size_t) from; i < std::min (a.size(), b.size()); ++i) m = std::max (m, (double) std::abs (a[i] - b[i]));
    return m;
}

inline double toDb (double x) { return 20.0 * std::log10 (std::max (1e-30, x)); }
} // namespace fvtest
