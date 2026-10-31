// ir_forge - finishing (DC, tail fade to silence, trim, normalisation) and RT60 measurement.
#pragma once

#include "Ir.h"

namespace irf
{
struct FinishParams
{
    double endDb = -90.0;        // IR is cut where the energy decay curve reaches this level (dB re total)
    double fadeFromDb = -65.0;   // raised-cosine fade starts where the EDC reaches this level
    double maxLengthS = 0.0;     // optional hard cap (0 = none); the fade is then fitted inside the cap
    double peakDbfs = -1.0;
    bool keepEnd = false;        // generator already shaped its own ending (gates, reverse): no extra fade
};

/** Remove DC (20 Hz 2nd-order high-pass), fade/trim the tail to true silence, peak-normalise. */
void finishIr (Ir4& ir, const FinishParams& p);

struct DecayMeasure
{
    double t30 = 0, t20 = 0, edt = 0;
    bool valid = false;
};

/** Schroeder backward integration on the energy sum of all 4 channels, after an octave-band filter
    centred on `bandHz` (0 = broadband). T30: linear fit of -5..-35 dB, extrapolated to 60 dB. */
DecayMeasure measureDecay (const Ir4& ir, double bandHz);

/** Value reported as "rt60" in library.json: T30 in the 1 kHz octave (falls back to T20, then EDT). */
double reportedRt60 (const Ir4& ir);

} // namespace irf
