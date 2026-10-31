// ir_forge - special effects: reverse, bloom, tuned metallic resonators.
#pragma once

#include "FdnReverb.h"

namespace irf
{
struct ReverseParams
{
    double swellS = 0.9;       // portion of the base IR (from its start) that is reversed
    double endFadeMs = 6.0;    // short release after the swell peak
    double shapePower = 1.0;   // extra (t / swellS)^power emphasis on the swell
};

/** Reverse the first `swellS` of a base IR: a swell that peaks at the end and then cuts. */
Ir4 makeReverse (const Ir4& base, const ReverseParams& p);

struct BloomParams
{
    double attackS = 0.45;     // time constant of the swell
    double power = 2.0;        // envelope (1 - exp(-t / attack))^power
    double startS = 0.0;       // envelope start (s)
};

/** Slow-attack swelling envelope over a long, diffuse base reverb. */
Ir4 makeBloom (const Ir4& base, const BloomParams& p);

struct MetalParams
{
    std::vector<double> pitchesHz { 220.0, 277.18, 329.63, 415.30, 554.37, 659.26, 880.0, 1108.7 };
    double detuneCents = 7.0;      // second, detuned resonator layer for beating shimmer
    double rt60 = 1.6;             // resonator decay at the fundamental
    double hfDampHz = 7000;        // in-loop low-pass (sets harmonic roll-off)
    double modDepthCents = 3.0;    // slow pitch wobble
    double modRateHz = 0.35;
    double diffuseLevel = 0.35;    // energy of the diffuse FDN layer relative to the resonators
    FdnParams diffuse;             // short, bright diffuse wash underneath
    double lengthS = 2.4;
    std::vector<EqBand> eq;
    uint64_t seed = 1;
};

Ir4 renderMetallic (const MetalParams& p, double fs = kOutputRate);

} // namespace irf
