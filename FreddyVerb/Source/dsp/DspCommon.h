#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <cstring>

namespace fv::dsp
{
inline constexpr double kPi = 3.14159265358979323846;

inline float  dbToGain (float db)  noexcept { return std::pow (10.0f, db * 0.05f); }
inline double dbToGain (double db) noexcept { return std::pow (10.0, db * 0.05); }
inline float  gainToDb (float g, float floorDb = -200.0f) noexcept
{
    return g > 0.0f ? std::max (floorDb, 20.0f * std::log10 (g)) : floorDb;
}

/** Kills denormals/subnormals in feedback paths without relying only on FTZ flags. */
inline float  flushDenormal (float v)  noexcept { return std::abs (v) < 1.0e-20f ? 0.0f : v; }
inline double flushDenormal (double v) noexcept { return std::abs (v) < 1.0e-30 ? 0.0 : v; }

/** One-pole coefficient so that the step response reaches ~63% after timeMs. */
inline double onePoleCoef (double timeMs, double sampleRate) noexcept
{
    const double n = std::max (1.0e-6, timeMs * 0.001 * sampleRate);
    return 1.0 - std::exp (-1.0 / n);
}

inline int msToSamples (double ms, double sr) noexcept { return (int) std::lround (ms * 0.001 * sr); }

/** Scoped FTZ/DAZ for any thread that does DSP (audio thread and workers). */
struct ScopedNoDenormalsFV
{
    ScopedNoDenormalsFV() noexcept;
    ~ScopedNoDenormalsFV() noexcept;
    intptr_t saved = 0;
};

/** Small linear parameter smoother (per sample), used where juce::SmoothedValue
    would be awkward (inside the engine's sub-block loop). */
struct LinearSmoother
{
    void reset (double sr, double rampMs) { steps = std::max (1, (int) (sr * rampMs * 0.001)); }
    void setCurrentAndTarget (float v) { cur = target = v; remaining = 0; }
    void setTarget (float v)
    {
        if (v == target) return;
        target = v; remaining = steps; inc = (target - cur) / (float) steps;
    }
    float next() noexcept
    {
        if (remaining > 0) { cur += inc; if (--remaining == 0) cur = target; }
        return cur;
    }
    bool isSmoothing() const noexcept { return remaining > 0; }
    float current() const noexcept { return cur; }
    float cur = 0, target = 0, inc = 0;
    int remaining = 0, steps = 64;
};
} // namespace fv::dsp
