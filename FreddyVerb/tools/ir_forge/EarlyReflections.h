// ir_forge - early reflections: randomised shoebox image-source model with scattering.
#pragma once

#include "Ir.h"

namespace irf
{
struct Vec3
{
    double x = 0, y = 0, z = 0;
};

struct ErParams
{
    Vec3 room { 6.0, 5.0, 3.0 };          // metres
    Vec3 srcL { 2.6, 2.0, 1.0 };           // "left" input source position
    Vec3 srcR { 3.4, 2.0, 1.0 };           // "right" input source position
    Vec3 micL { 2.5, 3.8, 1.6 };           // left output microphone
    Vec3 micR { 3.5, 3.8, 1.6 };           // right output microphone
    double splayDeg = 35.0;                // each mic rotated outward from the source axis
    double cardioid = 0.5;                 // 0 = omni, 0.5 = cardioid, 0.3 = wide cardioid
    double betaWall = 0.85;                // pressure reflection coefficients
    double betaFloor = 0.75;
    double betaCeiling = 0.8;
    double wallLpHz = 9000.0;              // absorption low-pass applied once per reflection order
    int maxOrder = 4;
    double maxTimeMs = 80.0;               // reflections beyond this are dropped (late FDN takes over)
    double fadeMs = 25.0;                  // raised-cosine fade at the end of the ER window
    double jitterMs = 0.12;                // randomised-image time jitter per order (breaks flutter)
    double gainJitterDb = 1.5;             // surface irregularity
    double scatter = 0.35;                 // fraction of reflected energy diffused into clusters
    int scatterTaps = 8;
    double scatterSpreadMs = 1.2;          // cluster spread per sqrt(order)
    uint64_t seed = 1;
};

struct ErResult
{
    Ir4 ir;                                // 4-channel early reflections (no direct sound)
    std::array<double, 4> directTime {};   // direct-path arrival times (s) per channel
};

ErResult renderEarlyReflections (const ErParams& p, double fs, size_t length);

} // namespace irf
