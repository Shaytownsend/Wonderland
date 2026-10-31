#include "Dynamics.h"
#include "../state/Parameters.h"

namespace fv::dsp
{
// ===========================================================================
// Curve shapes
// ===========================================================================
namespace
{
    inline double smooth01 (double x) noexcept { return 0.5 - 0.5 * std::cos (kPi * std::clamp (x, 0.0, 1.0)); }
}

double CurveShapes::evaluate (int shape, double p, double T) noexcept
{
    if (p <= 0.0) return 0.0;
    if (p >= 1.0) return 1.0;
    T = std::clamp (T, -1.0, 1.0);
    switch (shape)
    {
        case choices::linearRamp:
            // bend with bounded slope (max 4/duration): T>0 bows up (faster), T<0 bows down
            return T >= 0.0 ? 1.0 - std::pow (1.0 - p, 1.0 + 3.0 * T) : std::pow (p, 1.0 - 3.0 * T);
        case choices::expo:
        {
            const double k = 5.0 * std::exp (T);                            // fast recovery, then easing in
            return (1.0 - std::exp (-k * p)) / (1.0 - std::exp (-k));
        }
        case choices::loga:
        {
            const double k = 5.0 * std::exp (T);                            // slow start, then fast bloom
            return (std::exp (k * p) - 1.0) / (std::exp (k) - 1.0);
        }
        case choices::sShape:
        {
            const double a = 2.0 * std::exp (1.2 * T);                      // steepness of the S
            const double pa = std::pow (p, a), qa = std::pow (1.0 - p, a);
            return pa / (pa + qa);
        }
        case choices::holdRamp:
        {
            const double h = 0.5 + 0.4 * T;                                 // fraction of the duration held fully ducked
            return p < h ? 0.0 : smooth01 ((p - h) / (1.0 - h));
        }
        case choices::pump:
        {
            // classic sidechain pump: short dip, then a concave rise that eases into unity
            const double x = std::max (0.0, (p - 0.04) / 0.96);
            const double c = 2.6 * std::exp (0.8 * T);
            return 1.0 - std::pow (1.0 - x, c);
        }
        case choices::swell:
        {
            // blooms past unity (+1.5 dB) and settles back: exaggerated bloom
            const double peakAt = 0.7 + 0.15 * T;
            if (p < peakAt)
            {
                const double s = std::sin (0.5 * kPi * p / peakAt);
                return kSwellPeak * s * s;
            }
            return 1.0 + (kSwellPeak - 1.0) * (0.5 + 0.5 * std::cos (kPi * (p - peakAt) / (1.0 - peakAt)));
        }
        default:
            return p;
    }
}

double CurveShapes::releaseCurve (int curve, double p) noexcept
{
    switch (curve)
    {
        case choices::linear:      return evaluate (choices::linearRamp, p, 0.0);
        case choices::exponential: return evaluate (choices::expo, p, 0.0);
        case choices::logarithmic: return evaluate (choices::loga, p, 0.0);
        case choices::sCurve:      return evaluate (choices::sShape, p, 0.0);
        default:                   return std::clamp (p, 0.0, 1.0);
    }
}

double CurveShapes::gateCurve (int curve, double p) noexcept
{
    // fraction of the way from the open level to the floor
    switch (curve)
    {
        case 1:  return evaluate (choices::expo, p, 0.0);   // fast at first, long tail (natural decay)
        case 2:  return evaluate (choices::loga, p, 0.0);   // holds, then drops away
        default: return std::clamp (p, 0.0, 1.0);
    }
}

// ===========================================================================
// Detector
// ===========================================================================
void Detector::prepare (double sampleRate)
{
    sr = sampleRate;
    peakRel = std::exp (-1.0 / (kPeakReleaseMs * 0.001 * sr));
    rmsBuf.assign ((size_t) std::max (1, msToSamples (50.0, sr) + 1), 0.0);
    reset();
}

void Detector::reset()
{
    for (auto& f : hp) f.reset();
    for (auto& f : lp) f.reset();
    lastHp = lastLp = -1.0f;
    peakEnv = 0.0;
    std::fill (rmsBuf.begin(), rmsBuf.end(), 0.0);
    rmsPos = 0; rmsSum = 0.0; resum = 0;
}

void Detector::process (const float* keyL, const float* keyR, float* levelDb, float* fL, float* fR,
                        int n, const EngineParams& p) noexcept
{
    if (p.keyHpfHz != lastHp) { for (auto& f : hp) f.setCutoff (p.keyHpfHz, 0.70710678, sr); lastHp = p.keyHpfHz; }
    if (p.keyLpfHz != lastLp) { for (auto& f : lp) f.setCutoff (p.keyLpfHz, 0.70710678, sr); lastLp = p.keyLpfHz; }
    const bool useHp = p.keyHpfHz > 20.5f, useLp = p.keyLpfHz < 19900.0f;
    const int wanted = std::clamp (msToSamples (p.rmsMs, sr), 1, (int) rmsBuf.size());
    if (wanted != rmsLen)
    {
        rmsLen = wanted;
        rmsSum = 0.0;
        for (int i = 0; i < rmsLen; ++i) rmsSum += rmsBuf[(size_t) ((rmsPos - 1 - i + (int) rmsBuf.size() * 2) % (int) rmsBuf.size())];
    }
    const int bufLen = (int) rmsBuf.size();

    for (int i = 0; i < n; ++i)
    {
        double l = keyL[i], r = keyR[i];
        if (useHp) { l = hp[0].process (l, SvfTPT::highpass); r = hp[1].process (r, SvfTPT::highpass); }
        if (useLp) { l = lp[0].process (l, SvfTPT::lowpass);  r = lp[1].process (r, SvfTPT::lowpass); }
        if (fL != nullptr) { fL[i] = (float) l; fR[i] = (float) r; }
        const double m = std::max (std::abs (l), std::abs (r));

        // keep the RMS history running in both modes so switching is seamless
        const double sq = m * m;
        const int outIdx = (rmsPos - rmsLen + bufLen) % bufLen;
        rmsSum += sq - rmsBuf[(size_t) outIdx];
        rmsBuf[(size_t) rmsPos] = sq;
        rmsPos = (rmsPos + 1) % bufLen;
        if (++resum >= 8192)  // guard against floating-point drift
        {
            resum = 0; rmsSum = 0.0;
            for (int k = 0; k < rmsLen; ++k) rmsSum += rmsBuf[(size_t) ((rmsPos - 1 - k + bufLen) % bufLen)];
        }

        peakEnv = std::max (m, peakEnv * peakRel);
        const double lvl = p.detMode == choices::rms ? std::sqrt (std::max (0.0, rmsSum) / rmsLen) : peakEnv;
        levelDb[i] = lvl > 1.0e-6 ? (float) (20.0 * std::log10 (lvl)) : -120.0f;
    }
    peakEnv = flushDenormal (peakEnv);
}

// ===========================================================================
// Ducker
// ===========================================================================
void Ducker::prepare (double sampleRate) { sr = sampleRate; reset(); }

void Ducker::reset()
{
    g = 1.0; relStart = 1.0; relPhase = 1.0; releasing = false;
    cs = CurveState::idle; attackPhase = 0.0; curvePhase = 1.0; curveStart = 0.0;
    armed = true; sinceTrigger = 1 << 30; lastMode = -1;
}

void Ducker::process (const float* levelDb, float* gain, int n, const EngineParams& p) noexcept
{
    const bool full = p.duckDepthDb <= kDuckFullDb;
    const double depthGain = full ? 0.0 : dbToGain ((double) std::min (0.0f, p.duckDepthDb));
    const double attSamples = std::max (1.0, p.duckAttackMs * 0.001 * sr);

    if (! p.duckOn)
    {
        const double c = onePoleCoef (10.0, sr);
        for (int i = 0; i < n; ++i) { g += (1.0 - g) * c; gain[i] = (float) g; }
        releasing = false; cs = CurveState::idle;
        return;
    }

    if (p.duckMode != lastMode)
    {
        // continue from the current level whichever way we switch
        relStart = g; relPhase = 0.0; releasing = g < 1.0;
        if (p.duckMode == choices::curve && g < 1.0) { cs = CurveState::running; curveStart = g; curvePhase = 0.0; }
        lastMode = p.duckMode;
    }

    if (p.duckMode == choices::follow)
    {
        const double attCoef = onePoleCoef (p.duckAttackMs, sr);
        const double relInc = 1.0 / std::max (1.0, p.duckReleaseMs * 0.001 * sr);
        for (int i = 0; i < n; ++i)
        {
            const double over = (double) levelDb[i] - (double) p.duckThreshDb;
            double grDb = std::max (0.0, over * (double) p.duckAmount);
            if (! full) grDb = std::min (grDb, -(double) std::min (0.0f, p.duckDepthDb));
            const double target = grDb > 0.0 ? dbToGain (-grDb) : 1.0;

            if (target < g - 1.0e-9)
            {
                // attack: push the reverb down (one-pole, bounded slope)
                g += (target - g) * attCoef;
                releasing = false;
                relStart = g; relPhase = 0.0;
            }
            else
            {
                // bloom: recover along the release curve, never above what the key allows
                releasing = true;
                relPhase = std::min (1.0, relPhase + relInc);
                const double c = relStart + (1.0 - relStart) * CurveShapes::releaseCurve (p.duckRelCurve, relPhase);
                if (c > target) { g = target; relStart = target; relPhase = 0.0; } // key still holding us down
                else g = c;
            }
            gain[i] = (float) g;
        }
        return;
    }

    // CURVE mode: transient-triggered fixed shapes
    const double durSamples = std::max (1.0, p.curveDurMs * 0.001 * sr);
    const int64_t guard = (int64_t) (p.curveGuardMs * 0.001 * sr);
    for (int i = 0; i < n; ++i)
    {
        const double lvl = levelDb[i];
        ++sinceTrigger;
        if (! armed && lvl < p.duckThreshDb - p.curveHystDb) armed = true;
        if (armed && lvl > p.duckThreshDb && sinceTrigger >= guard)
        {
            armed = false;
            sinceTrigger = 0;
            if (p.curveRetrig == choices::continueLevel && cs != CurveState::idle)
            {
                // fast rolls: carry on from wherever we are, no new dip
                curveStart = g; curvePhase = 0.0; cs = CurveState::running;
            }
            else
            {
                attackFrom = g; attackPhase = 0.0; cs = CurveState::attack;
            }
        }

        switch (cs)
        {
            case CurveState::attack:
                attackPhase += 1.0 / attSamples;
                if (attackPhase >= 1.0) { g = depthGain; cs = CurveState::running; curveStart = depthGain; curvePhase = 0.0; }
                else g = attackFrom + (depthGain - attackFrom) * attackPhase;
                break;
            case CurveState::running:
                curvePhase += 1.0 / durSamples;
                if (curvePhase >= 1.0) { g = 1.0; cs = CurveState::idle; }
                else g = curveStart + (1.0 - curveStart) * CurveShapes::evaluate (p.curveShape, curvePhase, p.curveTension);
                break;
            case CurveState::idle:
            default:
                g = 1.0;
                break;
        }
        gain[i] = (float) g;
    }
    releasing = cs == CurveState::running;
}

// ===========================================================================
// Gate
// ===========================================================================
void Gate::prepare (double sampleRate) { sr = sampleRate; reset(); }

void Gate::reset() { g = 1.0; from = 1.0; phase = 0.0; holdLeft = 0; state = State::hold; }

void Gate::process (const float* levelDb, float* gain, int n, const EngineParams& p) noexcept
{
    const double floorG = p.gateFloorDb <= kGateFloorInf ? 0.0 : dbToGain ((double) p.gateFloorDb);
    if (! p.gateOn)
    {
        const double c = onePoleCoef (10.0, sr);
        for (int i = 0; i < n; ++i) { g += (1.0 - g) * c; gain[i] = (float) g; }
        state = State::open; holdLeft = 0;
        return;
    }
    const double attInc = 1.0 / std::max (1.0, p.gateAttackMs * 0.001 * sr);
    const double relInc = 1.0 / std::max (1.0, p.gateReleaseMs * 0.001 * sr);
    const int64_t holdSamples = (int64_t) (p.gateHoldMs * 0.001 * sr);
    const double openThr = p.gateThreshDb, closeThr = p.gateThreshDb - p.gateHystDb;

    for (int i = 0; i < n; ++i)
    {
        const double lvl = levelDb[i];
        const bool above = lvl > openThr;
        switch (state)
        {
            case State::closed:
            case State::release:
                if (above) { from = g; phase = 0.0; state = State::attack; }
                break;
            case State::hold:
                if (above) state = State::open;
                break;
            case State::open:
            case State::attack:
            default:
                break;
        }
        if ((state == State::open || state == State::attack) && lvl < closeThr)
        {
            if (state == State::open) { state = State::hold; holdLeft = holdSamples; }
        }

        switch (state)
        {
            case State::attack:
                phase += attInc;
                if (phase >= 1.0) { g = 1.0; state = lvl < closeThr ? State::hold : State::open; holdLeft = holdSamples; }
                else g = from + (1.0 - from) * (0.5 - 0.5 * std::cos (kPi * phase)); // raised cosine: no corner
                break;
            case State::open:
                g = 1.0;
                break;
            case State::hold:
                g = 1.0;
                if (--holdLeft <= 0) { state = State::release; from = g; phase = 0.0; }
                break;
            case State::release:
                phase = std::min (1.0, phase + relInc);
                g = from + (floorG - from) * CurveShapes::gateCurve (p.gateRelCurve, phase);
                if (phase >= 1.0) { g = floorG; state = State::closed; }
                break;
            case State::closed:
            default:
                // track floor changes smoothly while closed
                g += (floorG - g) * 0.001;
                break;
        }
        gain[i] = (float) g;
    }
}
} // namespace fv::dsp
