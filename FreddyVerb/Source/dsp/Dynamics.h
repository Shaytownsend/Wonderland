#pragma once

#include <vector>
#include "DspCommon.h"
#include "EngineParams.h"

namespace fv::dsp
{
/** Closed-form duck curves. p in [0,1] is the normalised time since the curve
    started; the result is the recovered fraction g (0 = fully ducked, 1 = back
    to unity; Swell overshoots to +1.5 dB = 1.1885). tension in [-1, 1] bends the
    shape. Values for p >= 1 are exactly 1, for p <= 0 exactly the start value. */
namespace CurveShapes
{
    double evaluate (int shape, double p, double tension) noexcept;
    /** Follow-mode release curves: 0 linear, 1 exponential, 2 logarithmic, 3 S-curve. */
    double releaseCurve (int curve, double p) noexcept;
    /** Gate release curves: 0 linear, 1 exponential, 2 logarithmic (fraction already released). */
    double gateCurve (int curve, double p) noexcept;
    inline constexpr double kSwellPeak = 1.18850222744; // +1.5 dB
}

/** Topology-preserving (Zavalishin/Simper) state-variable filter, 2nd order. */
struct SvfTPT
{
    enum Mode { lowpass, highpass, bandpass };
    void setCutoff (double hz, double q, double sr) noexcept
    {
        g = std::tan (kPi * std::clamp (hz, 1.0, sr * 0.49) / sr);
        k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    double process (double x, Mode m) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenormal (2.0 * v1 - ic1);
        ic2 = flushDenormal (2.0 * v2 - ic2);
        switch (m)
        {
            case highpass: return x - k * v1 - v2;
            case bandpass: return v1;
            case lowpass:
            default: return v2;
        }
    }
    void reset() noexcept { ic1 = ic2 = 0.0; }
    double g = 0, k = 1.41421356, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
};

/** Shared key detector: filtered dry key -> level in dB (stereo key = max(L, R)). */
class Detector
{
public:
    void prepare (double sampleRate);
    void reset();
    /** keyL/keyR: aligned dry key. levelDb: per-sample detector level.
        filteredL/R (optional): the filtered key audio for Key Listen. */
    void process (const float* keyL, const float* keyR, float* levelDb, float* filteredL, float* filteredR,
                  int n, const EngineParams& p) noexcept;

    static constexpr double kPeakReleaseMs = 10.0;   // peak detector hold/release
private:
    double sr = 48000.0;
    SvfTPT hp[2], lp[2];
    float lastHp = -1, lastLp = -1;
    double peakEnv = 0.0, peakRel = 0.999;
    std::vector<double> rmsBuf;   // squared max(L,R) history for the moving RMS window
    int rmsPos = 0, rmsLen = 1;
    double rmsSum = 0.0;
    int resum = 0;
};

/** Duck & bloom stage (FOLLOW and CURVE modes). */
class Ducker
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (const float* levelDb, float* gain, int n, const EngineParams& p) noexcept;
    double getGain() const noexcept { return g; }

    /** Visualizer/static-preview helper: true while releasing / blooming. */
    bool isReleasing() const noexcept { return releasing; }

private:
    double sr = 48000.0;
    double g = 1.0;
    // follow mode
    double relStart = 1.0, relPhase = 1.0;
    bool releasing = false;
    // curve mode
    enum class CurveState { idle, attack, running };
    CurveState cs = CurveState::idle;
    double attackFrom = 1.0, attackPhase = 0.0, curveStart = 0.0, curvePhase = 1.0;
    bool armed = true;
    int64_t sinceTrigger = 1 << 30;
    int lastMode = -1;
};

/** Gate keyed from the dry key: decides when the tail ends and how fast it fades. */
class Gate
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (const float* levelDb, float* gain, int n, const EngineParams& p) noexcept;
    double getGain() const noexcept { return g; }
    enum class State { closed, attack, open, hold, release };
    State getState() const noexcept { return state; }

private:
    double sr = 48000.0;
    double g = 0.0, from = 0.0, phase = 0.0;
    int64_t holdLeft = 0;
    State state = State::closed;
};
} // namespace fv::dsp
