#pragma once

namespace fv::dsp
{
/** Plain-value snapshot of every real-time parameter, filled by the processor
    once per block from the APVTS atomics. The engine never touches the APVTS,
    which keeps it unit-testable and host-independent. */
struct EngineParams
{
    // Reverb (real-time part; IR-shaping values live in IRShapeParams)
    int   quality       = 0;      // 0 zero latency, 1 efficient
    float predelayMs    = 8.0f;

    // Detector
    float keyHpfHz      = 20.0f;
    float keyLpfHz      = 20000.0f;
    int   detMode       = 0;      // 0 peak, 1 rms
    float rmsMs         = 10.0f;
    float lookaheadMs   = 0.0f;
    bool  keyListen     = false;

    // Duck
    bool  duckOn        = true;
    int   duckMode      = 0;      // 0 follow, 1 curve
    float duckDepthDb   = -18.0f; // <= kDuckFullDb means full duck
    float duckThreshDb  = -30.0f;
    float duckAttackMs  = 2.0f;
    float duckReleaseMs = 250.0f;
    float duckAmount    = 1.0f;
    int   duckRelCurve  = 1;
    int   curveShape    = 0;
    float curveDurMs    = 300.0f;
    float curveTension  = 0.0f;
    int   curveRetrig   = 0;
    float curveGuardMs  = 30.0f;
    float curveHystDb   = 3.0f;

    // Gate
    bool  gateOn        = true;
    float gateThreshDb  = -40.0f;
    float gateHystDb    = 4.0f;
    float gateAttackMs  = 1.0f;
    float gateHoldMs    = 150.0f;
    float gateReleaseMs = 300.0f;
    float gateFloorDb   = -81.0f; // <= kGateFloorInf means -inf
    int   gateRelCurve  = 1;

    // EQ
    bool  eqOn = true;
    int   eqPosition = 1;         // 0 pre, 1 post
    bool  hpfOn = true;  float hpfHz = 120.0f;  int hpfSlope = 1;
    bool  lpfOn = true;  float lpfHz = 14000.0f; int lpfSlope = 0;
    bool  lsOn = false;  float lsHz = 200.0f;  float lsGainDb = 0.0f; float lsQ = 0.707f;
    bool  hsOn = false;  float hsHz = 6000.0f; float hsGainDb = 0.0f; float hsQ = 0.707f;
    bool  b1On = false;  float b1Hz = 500.0f;  float b1GainDb = 0.0f; float b1Q = 1.0f;
    bool  b2On = false;  float b2Hz = 3000.0f; float b2GainDb = 0.0f; float b2Q = 1.0f;

    // Space
    float widthPct      = 100.0f;
    bool  monoBelowOn   = false;
    float monoBelowHz   = 120.0f;
    float balance       = 0.0f;   // -100..100

    // Mix
    float mixPct        = 35.0f;
    float dryDb         = 0.0f;
    float wetDb         = 0.0f;
    bool  wetSolo       = false;
    float outDb         = 0.0f;
    bool  bypass        = false;
};
} // namespace fv::dsp
