// ir_forge - non-linear / gated envelopes ('80s gated reverb, reverse non-lin, flat non-lin).
#pragma once

#include "Ir.h"

namespace irf
{
struct NonLinParams
{
    enum Shape { Gated, Flat, Reverse };
    Shape shape = Gated;
    double gateMs = 380.0;       // time (from first arrival) at which the gate closes
    double fadeMs = 12.0;        // raised-cosine release of the gate (very short)
    double attackMs = 4.0;       // envelope rise at the onset
    double startDb = 0.0;        // envelope level at onset (after attack)
    double endDb = -4.0;         // envelope level just before the gate closes (dB-linear ramp)
    double curve = 1.0;          // >1 bends the ramp (Reverse: slow start, fast swell)
    double keepNaturalMs = 0.0;  // first N ms keep the natural (unflattened) early reflections burst
    double flattenMs = 30.0;     // envelope-follower window used to flatten the natural decay
};

/** Reshape a dense base reverb (rendered with a long RT60) into a non-linear envelope. */
Ir4 applyNonLinear (const Ir4& base, const NonLinParams& p);

/** Time (s) of the first sample whose magnitude exceeds `relDb` re the peak. */
double firstArrival (const Ir4& ir, double relDb = -40.0);

} // namespace irf
