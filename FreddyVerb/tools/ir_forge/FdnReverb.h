// ir_forge - dense feedback delay network (late reverberation engine).
#pragma once

#include "Ir.h"

namespace irf
{
struct FdnParams
{
    int numLines = 16;                     // 16 or 32 (power of two, Hadamard feedback)
    double minDelayMs = 18.0, maxDelayMs = 55.0;

    // Target RT60 (s) per band. Crossovers: low shelf at fLow, high shelf at fHigh, "air" shelf at fAir.
    double rtLow = 1.0, rtMid = 1.0, rtHigh = 0.8, rtAir = 0.5;
    double fLow = 250.0, fHigh = 3500.0, fAir = 9000.0;

    // Delay modulation (per line: sine + smooth random, randomised rate/phase).
    double modDepthMs = 0.04;
    double modRateLo = 0.07, modRateHi = 0.45;
    double modRandomAmt = 0.5;

    // Input diffusion (Schroeder allpass cascade, separate chain per input channel).
    int diffStages = 6;
    double diffMinMs = 1.0, diffMaxMs = 9.0, diffG = 0.62;

    // In-loop Schroeder allpasses per line (extra density, used for plates / clouds).
    int loopAllpasses = 0;
    double loopApMinMs = 0.5, loopApMaxMs = 3.0, loopApG = 0.5;

    // In-loop first-order allpass (dispersion), coefficient magnitude (0 = off).
    double loopDispersion = 0.0;

    // Vintage: fixed-point state with magnitude truncation (0 = off). Step = 2^-(bits-1).
    int quantBits = 0;

    uint64_t seed = 1;
};

/** Render the FDN for two excitation signals (left and right input). Returns 4-channel true stereo. */
Ir4 renderFdn (const FdnParams& p, double fs, size_t length,
               const std::vector<double>& exciteL, const std::vector<double>& exciteR);

/** Convenience: impulse excitation at time t (seconds) for both inputs. */
Ir4 renderFdnImpulse (const FdnParams& p, double fs, size_t length, double tL = 0.0, double tR = 0.0);

} // namespace irf
