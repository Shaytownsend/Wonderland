// ir_forge - late-'80s digital reverb coloration (low internal rate, fixed point, gain-ranging converters).
#pragma once

#include "FdnReverb.h"

namespace irf
{
struct TapPattern
{
    int numTaps = 0;                 // early multi-tap pattern (digital "room" programs); 0 = none
    double minMs = 4.0, maxMs = 60.0;
    double decayDb = -9.0;           // level of the last tap relative to the first
    double level = 0.5;              // tap sum level relative to the reverb (energy)
};

struct DattorroParams
{
    double rt60 = 2.0;
    double inputDiff1 = 0.75, inputDiff2 = 0.625;
    double decayDiff1 = 0.70, decayDiff2 = 0.50;
    double bandwidth = 0.9995;       // input one-pole coefficient (paper notation)
    double damping = 0.0005;         // tank one-pole coefficient (paper notation)
    double excursion = 16.0;         // modulation depth, samples at 29.761 kHz
    double modRateHz = 1.0;
    double size = 1.0;               // scales all tank/diffuser delays
};

struct VintageParams
{
    enum Engine { Fdn, Dattorro };
    Engine engine = Fdn;
    double internalFs = 32000.0;     // must divide 96 kHz (32k -> x3)
    FdnParams fdn;
    DattorroParams dattorro;
    TapPattern taps;
    double bandwidthHz = 12000.0;    // reconstruction filter cutoff (converter bandwidth)
    int dacBits = 12;                // gain-ranging DAC mantissa bits, first-order noise shaped (0 = off)
    double inputLevel = 16.0;        // impulse amplitude: puts the FDN's line signals near full scale so that
                                     // fdn.quantBits is meaningful (20 bits -> dead band ~ -110 dB re peak)
    double preDelayMs = 0.0;
    double lengthS = 2.0;
    std::vector<EqBand> eq;          // applied at 96 kHz after reconstruction
    uint64_t seed = 1;
};

Ir4 renderVintage (const VintageParams& p);

} // namespace irf
