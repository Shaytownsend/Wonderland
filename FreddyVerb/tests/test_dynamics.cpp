#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/Dynamics.h"
#include "state/Parameters.h"
#include <fstream>
#include <sstream>
#include <random>

using namespace fv::dsp;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double sr = 48000.0;

    /** Key level in dB for a signal (runs the real detector). */
    std::vector<float> levels (const std::vector<float>& x, const EngineParams& p)
    {
        Detector d; d.prepare (sr);
        std::vector<float> lv (x.size());
        d.process (x.data(), x.data(), lv.data(), nullptr, nullptr, (int) x.size(), p);
        return lv;
    }

    /** Drum-ish test signal: hits at the given times (s) with given lengths. */
    std::vector<float> hits (double seconds, const std::vector<std::pair<double, double>>& hs, float amp = 0.8f)
    {
        std::vector<float> x ((size_t) (seconds * sr), 0.0f);
        std::mt19937 rng (3);
        std::uniform_real_distribution<float> u (-1, 1);
        for (auto [t0, len] : hs)
            for (int i = (int) (t0 * sr); i < (int) ((t0 + len) * sr) && i < (int) x.size(); ++i)
                x[(size_t) i] += amp * u (rng);
        return x;
    }

    double maxStep (const std::vector<float>& g)
    {
        double m = 0;
        for (size_t i = 1; i < g.size(); ++i) m = std::max (m, (double) std::abs (g[i] - g[i - 1]));
        return m;
    }

    std::vector<float> runDuck (const std::vector<float>& lv, const EngineParams& p)
    {
        Ducker d; d.prepare (sr);
        std::vector<float> g (lv.size());
        for (size_t pos = 0; pos < lv.size(); pos += 100)
        {
            const int n = (int) std::min<size_t> (100, lv.size() - pos);
            d.process (lv.data() + pos, g.data() + pos, n, p);
        }
        return g;
    }

    std::vector<float> runGate (const std::vector<float>& lv, const EngineParams& p)
    {
        Gate d; d.prepare (sr);
        std::vector<float> g (lv.size());
        d.process (lv.data(), g.data(), (int) lv.size(), p);
        return g;
    }
}

TEST_CASE ("Curve shapes match the independent reference table", "[dynamics][curves]")
{
    std::ifstream f (std::string (FV_TEST_DATA_DIR) + "/curve_reference.csv");
    REQUIRE (f.good());
    std::string line;
    std::getline (f, line);
    int rows = 0;
    double worst = 0;
    while (std::getline (f, line))
    {
        std::stringstream ss (line);
        int s; double T, p, v; char c;
        ss >> s >> c >> T >> c >> p >> c >> v;
        worst = std::max (worst, std::abs (CurveShapes::evaluate (s, p, T) - v));
        ++rows;
    }
    CHECK (rows == 7 * 5 * 201);
    CHECK (worst < 1.0e-9);
}

TEST_CASE ("Curve shapes start ducked, end recovered; Swell overshoots +1.5 dB", "[dynamics][curves]")
{
    for (int s = 0; s < fv::choices::numCurveShapes; ++s)
        for (double T : { -1.0, 0.0, 1.0 })
        {
            CHECK (CurveShapes::evaluate (s, 0.0, T) == 0.0);
            CHECK (CurveShapes::evaluate (s, 1.0, T) == 1.0);
            double peak = 0;
            for (int i = 0; i <= 1000; ++i) peak = std::max (peak, CurveShapes::evaluate (s, i / 1000.0, T));
            if (s == fv::choices::swell) CHECK_THAT (20 * std::log10 (peak), WithinAbs (1.5, 0.01));
            else CHECK (peak <= 1.0 + 1e-12);
        }
}

TEST_CASE ("Detector: peak vs RMS and key filtering", "[dynamics][detector]")
{
    std::vector<float> sine ((size_t) sr);
    for (size_t i = 0; i < sine.size(); ++i) sine[i] = 0.5f * (float) std::sin (2 * kPi * 1000.0 * (double) i / sr);
    EngineParams p;
    p.detMode = fv::choices::peak;
    auto pk = levels (sine, p);
    p.detMode = fv::choices::rms;
    auto rm = levels (sine, p);
    CHECK_THAT (pk.back(), WithinAbs (20 * std::log10 (0.5), 0.2));
    CHECK_THAT (rm.back(), WithinAbs (20 * std::log10 (0.5 / std::sqrt (2.0)), 0.2));

    // a 100 Hz kick bleed is rejected by a 1 kHz key HPF
    std::vector<float> low ((size_t) sr);
    for (size_t i = 0; i < low.size(); ++i) low[i] = 0.5f * (float) std::sin (2 * kPi * 100.0 * (double) i / sr);
    p.detMode = fv::choices::peak;
    p.keyHpfHz = 1000.0f;
    CHECK (levels (low, p).back() < -40.0f);
}

TEST_CASE ("Ducker FOLLOW: program dependent bloom", "[dynamics][duck]")
{
    EngineParams p;
    p.duckMode = fv::choices::follow;
    p.duckDepthDb = -24.0f; p.duckThreshDb = -30.0f; p.duckReleaseMs = 200.0f;
    // short hit vs long hit: the long one keeps the reverb ducked longer
    auto shortHit = runDuck (levels (hits (1.5, { { 0.1, 0.03 } }), p), p);
    auto longHit  = runDuck (levels (hits (1.5, { { 0.1, 0.40 } }), p), p);
    auto recovered = [] (const std::vector<float>& g)
    {
        for (size_t i = (size_t) (0.15 * sr); i < g.size(); ++i) if (g[i] > 0.9f) return (double) i / sr;
        return 99.0;
    };
    CHECK (recovered (longHit) > recovered (shortHit) + 0.3);
    CHECK (*std::min_element (shortHit.begin(), shortHit.end()) < fv::dsp::dbToGain (-20.0f));
    // depth is respected
    CHECK (*std::min_element (longHit.begin(), longHit.end()) >= fv::dsp::dbToGain (-24.0f) - 1e-4f);
}

TEST_CASE ("Ducker CURVE: retrigger guard and hysteresis", "[dynamics][duck]")
{
    EngineParams p;
    p.duckMode = fv::choices::curve;
    p.curveShape = fv::choices::linearRamp;
    p.curveDurMs = 300.0f; p.duckThreshDb = -30.0f; p.duckDepthDb = -30.0f; p.curveGuardMs = 100.0f;
    // two hits 50 ms apart: the second is inside the guard and must not restart the curve
    auto g = runDuck (levels (hits (1.0, { { 0.1, 0.01 }, { 0.15, 0.01 } }), p), p);
    // the curve started at ~0.1 s + attack; with duration 300 ms it should be ~recovered at 0.42 s
    CHECK (g[(size_t) (0.43 * sr)] > 0.95f);
    p.curveGuardMs = 20.0f;
    g = runDuck (levels (hits (1.0, { { 0.1, 0.01 }, { 0.15, 0.01 } }), p), p);
    CHECK (g[(size_t) (0.43 * sr)] < 0.95f);  // restarted at 0.15 s
}

TEST_CASE ("Gate: hold and release timing", "[dynamics][gate]")
{
    EngineParams p;
    p.gateThreshDb = -40.0f; p.gateHystDb = 4.0f; p.gateHoldMs = 150.0f; p.gateReleaseMs = 300.0f;
    p.gateRelCurve = 0; p.gateFloorDb = -81.0f;
    p.detMode = fv::choices::peak;
    auto lv = levels (hits (2.0, { { 0.2, 0.05 } }), p);
    auto g = runGate (lv, p);
    // find where the key falls below the close threshold
    size_t below = 0;
    for (size_t i = (size_t) (0.2 * sr); i < lv.size(); ++i) if (lv[i] < p.gateThreshDb - p.gateHystDb) { below = i; break; }
    REQUIRE (below > 0);
    const size_t holdEnd = below + (size_t) (0.150 * sr);
    CHECK (g[holdEnd - 10] == 1.0f);
    CHECK (g[holdEnd + (size_t) (0.150 * sr)] < 0.6f);     // halfway through a linear release
    CHECK (g[holdEnd + (size_t) (0.301 * sr)] == 0.0f);    // fully closed to -inf
}

TEST_CASE ("Ducker and gate never click: bounded derivative under step, sweep and roll inputs", "[dynamics][clicks]")
{
    const int signal = GENERATE (0, 1, 2);   // 0 step, 1 amplitude sweep, 2 fast roll
    std::vector<float> x;
    if (signal == 0) x = hits (2.0, { { 0.2, 0.5 } });
    if (signal == 1)
    {
        x.assign ((size_t) (2.0 * sr), 0.0f);
        for (size_t i = 0; i < x.size(); ++i)
        {
            const double t = (double) i / sr;
            const double amp = std::abs (std::sin (kPi * t * 3.0));   // swells in and out
            x[i] = (float) (amp * std::sin (2 * kPi * (100.0 + 2000.0 * t) * t));
        }
    }
    if (signal == 2)
    {
        std::vector<std::pair<double, double>> roll;
        for (int k = 0; k < 24; ++k) roll.push_back ({ 0.1 + k * 0.045, 0.008 });
        x = hits (2.0, roll);
    }

    for (int mode : { 0, 1 })
        for (float att : { 0.1f, 2.0f, 50.0f })
            for (float rel : { 5.0f, 250.0f, 2000.0f })
            {
                EngineParams p;
                p.duckMode = mode; p.duckAttackMs = att; p.duckReleaseMs = rel; p.curveDurMs = rel;
                p.duckDepthDb = -61.0f; // full duck: the worst case
                p.duckThreshDb = -40.0f;
                p.gateAttackMs = std::max (0.05f, att * 0.5f); p.gateReleaseMs = rel; p.gateHoldMs = 20.0f; p.gateThreshDb = -45.0f;
                const auto lv = levels (x, p);
                const double attS = att * 0.001 * sr, relS = rel * 0.001 * sr;
                for (int c = 0; c < (mode == 0 ? 4 : fv::choices::numCurveShapes); ++c)
                    for (double T : { -1.0, 0.0, 1.0 })
                        for (int retrig : { 0, 1 })
                        {
                            if (mode == 0 && (T != 0.0 || retrig != 0)) continue;
                            p.duckRelCurve = c; p.curveShape = c; p.curveTension = (float) T; p.curveRetrig = retrig;
                            const auto g = runDuck (lv, p);
                            // the steepest any shape may be is ~16 x (1 / time constant)
                            const double bound = 16.0 / std::min (attS, relS) + 1.0e-6;
                            INFO ("signal " << signal << " mode " << mode << " att " << att << " rel " << rel
                                  << " curve " << c << " T " << T << " retrig " << retrig);
                            CHECK (maxStep (g) <= std::max (bound, 1.0 / std::max (1.0, attS) * 1.01));
                        }
                for (int gc = 0; gc < 3; ++gc)
                {
                    p.gateRelCurve = gc;
                    const auto g = runGate (lv, p);
                    const double gAtt = std::max (1.0, p.gateAttackMs * 0.001 * sr);
                    const double bound = std::max (kPi / 2.0 / gAtt, 5.1 / relS) * 1.01;
                    INFO ("gate curve " << gc << " att " << p.gateAttackMs << " rel " << rel);
                    CHECK (maxStep (g) <= bound);
                }
            }
}

TEST_CASE ("Default settings: gain changes stay below an audible-click threshold", "[dynamics][clicks]")
{
    EngineParams p; // init preset: follow duck -18 dB / 250 ms, gate 150/300, attack 2 ms / 1 ms
    const auto lv = levels (hits (2.0, { { 0.2, 0.05 }, { 0.9, 0.2 } }), p);
    CHECK (maxStep (runDuck (lv, p)) < 0.02);
    CHECK (maxStep (runGate (lv, p)) < 0.04);
}
