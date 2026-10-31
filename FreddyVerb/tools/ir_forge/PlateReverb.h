// ir_forge - modal plate model (thin, tensioned, simply-supported steel plate).
#pragma once

#include "Ir.h"

namespace irf
{
struct PlateParams
{
    double sizeX = 2.0, sizeY = 1.0;   // metres (EMT 140-like default)
    double thickness = 0.0005;         // metres
    double tension = 0.0;              // N/m membrane tension (raises the lowest modes, reduces LF dispersion)
    double youngs = 2.0e11, density = 7850.0, poisson = 0.3;

    double fMin = 40.0, fMax = 17000.0;    // modal synthesis band
    double taperHz = 10500.0;              // raised-cosine amplitude taper from here to fMax

    // RT60 curve anchors (s): below fLow, around mid, above fHigh, above fAir (log-interpolated)
    double rtLow = 1.4, rtMid = 1.6, rtHigh = 1.3, rtAir = 0.7;
    double fLow = 200.0, fHigh = 4000.0, fAir = 10000.0;

    double detune = 0.002;   // relative random mode-frequency perturbation (imperfect plate)

    // normalised (0..1) driver and pickup positions
    double inL[2] { 0.31, 0.43 }, inR[2] { 0.62, 0.58 };
    double outL[2] { 0.17, 0.71 }, outR[2] { 0.83, 0.27 };

    double lengthS = 3.0;
    std::vector<EqBand> eq;
    uint64_t seed = 1;
};

/** Reference RT60(f) curve shared by the modal plate. */
double interpRt (double f, double fLow, double fHigh, double fAir, double rtLow, double rtMid, double rtHigh, double rtAir);

Ir4 renderPlate (const PlateParams& p, double fs = kOutputRate);

/** Threads used inside one render (set by main: 1 when IRs are rendered in parallel). */
extern int gInnerThreads;

} // namespace irf
