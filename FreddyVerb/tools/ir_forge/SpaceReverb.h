// ir_forge - acoustic spaces (rooms, chambers, halls): image-source early reflections + FDN late field.
#pragma once

#include "EarlyReflections.h"
#include "FdnReverb.h"

namespace irf
{
struct SpaceParams
{
    ErParams er;
    FdnParams fdn;
    double erToLateDb = 0.0;      // energy ratio of early reflections to late field (LL/RR average)
    double lateDelayMs = 0.0;     // extra delay of the late-field excitation after the direct-path arrival
    double lateFadeInMs = 0.0;    // optional raised-cosine fade-in of the late field from its onset
    double preDelayMs = 0.0;      // removes (negative) or adds leading time; applied after mixing
    double lateLpHz = 0.0;        // late-field input colouring low-pass (0 = derive from er.wallLpHz)
    double lengthS = 2.0;         // render length (trimmed later)
    std::vector<EqBand> eq;       // tone shaping applied to the sum
};

Ir4 renderSpace (const SpaceParams& p, double fs = kOutputRate);

} // namespace irf
