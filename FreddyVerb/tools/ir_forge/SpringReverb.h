// ir_forge - spring tank model (dispersive stretched-allpass chirp loops).
#pragma once

#include "Ir.h"

namespace irf
{
struct SpringUnit
{
    double loopMs = 38.0;        // pure delay in the round trip (excluding dispersion)
    int stages = 100;            // stretched first-order allpasses in the loop
    double transitionHz = 4300;  // spring transition frequency  -> stretch K = fs / (2 fC)
    double apCoef = 0.62;        // allpass coefficient (> 0: group delay rises towards fC)
    double rt60 = 2.4;           // decay at 1 kHz
    double hfDampHz = 3200;      // one-pole low-pass in the loop
    double lfDampHz = 60;        // one-pole high-pass in the loop (springs do not sustain sub-bass)
    double modDepthSamples = 6;  // loop delay wobble -> flutter / "drip" irregularity
    double modRateHz = 1.1;
    double noiseAmt = 0.35;      // portion of modulation that is smooth random
    double inputGain[2] { 1.0, 1.0 };   // from input L / R
    double outputGain[2] { 1.0, 1.0 };  // to output L / R
};

struct SpringParams
{
    std::vector<SpringUnit> springs;
    double lengthS = 3.0;
    std::vector<EqBand> eq;  // transducer / tank band-limit (HP ~100 Hz, LP ~4.5 kHz) etc.
    uint64_t seed = 1;
};

Ir4 renderSpring (const SpringParams& p, double fs = kOutputRate);

} // namespace irf
