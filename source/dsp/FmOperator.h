#pragma once

#include <array>
#include <cmath>

namespace queen
{
// 2048-point sine table (+1 guard point) with linear interpolation; phase in cycles.
struct SineTable
{
    static constexpr int size = 2048;
    std::array<float, size + 1> t {};

    SineTable()
    {
        for (int i = 0; i <= size; ++i)
            t[(size_t) i] = (float) std::sin (6.283185307179586 * i / size);
    }

    static const SineTable& get()
    {
        static const SineTable table;
        return table;
    }

    float lookup (double cycles) const noexcept
    {
        const double wrapped = cycles - std::floor (cycles);
        const double pos = wrapped * size;
        const int i = (int) pos;
        const float frac = (float) (pos - i);
        return t[(size_t) i] + frac * (t[(size_t) i + 1] - t[(size_t) i]);
    }
};

struct FmOperator
{
    double phase = 0.0; // cycles, kept in [0,1)
    double inc = 0.0;   // cycles per sample
    float level = 0.0f;
    float feedback = 0.0f;
    float prev1 = 0.0f, prev2 = 0.0f; // one-sample delay, averaged

    static constexpr float modDepth = 1.0f;      // cycles of phase offset at modulator level 1
    static constexpr float feedbackDepth = 0.35f; // cycles at feedback 1

    void reset() noexcept { phase = 0.0; prev1 = prev2 = 0.0f; }

    float tick (float phaseMod) noexcept
    {
        const float fb = 0.5f * (prev1 + prev2) * feedback * feedbackDepth;
        const float raw = SineTable::get().lookup (phase + (double) (phaseMod * modDepth + fb));
        prev2 = prev1;
        prev1 = raw;
        phase += inc;
        if (phase >= 1.0)
            phase -= 1.0;
        return raw * level;
    }
};
} // namespace queen
