#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/IRProcessor.h"
#include "dsp/FreddyEngine.h"
#include "state/IRLibrary.h"
#include "TestHelpers.h"
#include <random>

using namespace fv::dsp;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

namespace
{
    IRSource exponentialNoiseIR (double rt60, double sr, int channels = 2, double len = 0.0)
    {
        IRSource s;
        s.sampleRate = sr;
        const int n = (int) ((len > 0 ? len : rt60 * 1.3) * sr);
        std::mt19937 rng (77);
        std::normal_distribution<double> g (0.0, 1.0);
        s.channels.assign ((size_t) channels, std::vector<double> ((size_t) n));
        for (auto& ch : s.channels)
            for (int i = 0; i < n; ++i) ch[(size_t) i] = g (rng) * std::exp (-6.907755 * i / (rt60 * sr));
        return s;
    }

    IRShapeParams flat()
    {
        IRShapeParams p;
        p.normalize = false;
        return p;
    }

    double bandEnergy (const std::vector<double>& x, double sr, double f0, double f1, int from, int to)
    {
        // brute-force DFT energy in a band over a time window (fine for tests)
        double e = 0;
        const int n = to - from;
        for (double f = f0; f < f1; f += sr / n)
        {
            double re = 0, im = 0;
            for (int i = from; i < to; ++i)
            {
                const double ph = 2 * kPi * f * (i - from) / sr;
                re += x[(size_t) i] * std::cos (ph); im -= x[(size_t) i] * std::sin (ph);
            }
            e += re * re + im * im;
        }
        return e;
    }
}

TEST_CASE ("RT60 estimate is accurate for exponential decays", "[ir]")
{
    for (double rt : { 0.3, 1.0, 2.5 })
    {
        auto s = exponentialNoiseIR (rt, 48000.0, 1);
        CHECK_THAT (IRProcessor::estimateRt60 (s.channels[0], 48000.0), WithinRel (rt, 0.08));
    }
}

TEST_CASE ("Resampling preserves frequency and duration (r8brain)", "[ir][resample]")
{
    const double from = 44100.0, to = 96000.0, f = 1000.0;
    std::vector<double> x (44100);
    for (size_t i = 0; i < x.size(); ++i) x[i] = std::sin (2 * kPi * f * (double) i / from);
    auto y = IRProcessor::resample (x, from, to);
    CHECK ((int) y.size() == (int) std::ceil (44100.0 * to / from));
    // compare to the ideal sine in the steady-state middle region
    double err = 0;
    for (size_t i = 5000; i < y.size() - 5000; ++i)
        err = std::max (err, std::abs (y[i] - std::sin (2 * kPi * f * (double) i / to)));
    CHECK (fvtest::toDb (err) < -90.0);
}

TEST_CASE ("Render upmixes mono/stereo/4-channel IRs sensibly", "[ir]")
{
    auto mono = exponentialNoiseIR (0.5, 48000.0, 1);
    auto ir = IRProcessor::render (mono, flat(), 48000.0);
    CHECK (ir.active[IRData::LL]); CHECK (ir.active[IRData::RR]);
    CHECK_FALSE (ir.active[IRData::LR]); CHECK_FALSE (ir.active[IRData::RL]);
    CHECK (ir.path[IRData::LL] == ir.path[IRData::RR]);

    auto quad = exponentialNoiseIR (0.5, 48000.0, 4);
    ir = IRProcessor::render (quad, flat(), 48000.0);
    for (int p = 0; p < 4; ++p) CHECK (ir.active[(size_t) p]);
}

TEST_CASE ("Normalize makes different IRs equally loud", "[ir][normalize]")
{
    IRShapeParams p;  // normalize on
    auto a = IRProcessor::render (exponentialNoiseIR (0.4, 48000.0), p, 48000.0);
    auto b = IRProcessor::render (exponentialNoiseIR (2.5, 44100.0), p, 48000.0);
    for (auto* ir : { &a, &b })
    {
        // gain needed after normalisation should be ~1
        CHECK_THAT (IRProcessor::loudnessNormalisationGain (*ir), WithinRel (1.0, 0.01));
    }
}

TEST_CASE ("Decay shaping changes the apparent RT60 proportionally", "[ir][decay]")
{
    auto src = exponentialNoiseIR (1.2, 48000.0, 2, 2.5);
    for (float pct : { 50.0f, 100.0f, 150.0f })
    {
        auto sp = flat();
        sp.decayPct = pct;
        IRDisplayInfo info;
        IRProcessor::render (src, sp, 48000.0, &info);
        INFO ("decay " << pct);
        CHECK_THAT (info.rt60, WithinRel (1.2 * pct / 100.0, 0.12));
    }
}

TEST_CASE ("Size stretches the IR length by resampling", "[ir][size]")
{
    auto src = exponentialNoiseIR (0.5, 48000.0, 2, 1.0);
    auto sp = flat();
    sp.sizePct = 200.0f;
    auto ir = IRProcessor::render (src, sp, 48000.0);
    CHECK (std::abs (ir.length() - 96000) < 64);
    sp.sizePct = 50.0f;
    ir = IRProcessor::render (src, sp, 48000.0);
    CHECK (std::abs (ir.length() - 24000) < 64);
}

TEST_CASE ("Start trim, length fade and reverse", "[ir][trim]")
{
    auto src = exponentialNoiseIR (0.5, 48000.0, 1, 1.0);
    auto sp = flat();
    sp.startMs = 50.0f;
    auto ir = IRProcessor::render (src, sp, 48000.0);
    CHECK (ir.length() == 48000 - 2400);
    CHECK (ir.path[0][0] == 0.0);                                  // faded in from silence
    CHECK_THAT (ir.path[0][100], WithinAbs (src.channels[0][2500], std::abs (src.channels[0][2500]))); // within fade, same sign-ish

    sp = flat();
    sp.lengthPct = 40.0f;
    ir = IRProcessor::render (src, sp, 48000.0);
    CHECK (ir.length() == 19200);
    CHECK (std::abs (ir.path[0].back()) < 1.0e-12);                // smooth fade to silence

    sp = flat();
    sp.reverse = true;
    ir = IRProcessor::render (src, sp, 48000.0);
    CHECK (ir.path[0].front() == src.channels[0].back());
    CHECK (ir.path[0].back() == src.channels[0].front());
}

TEST_CASE ("Early/late balance scales each region", "[ir][earlylate]")
{
    auto src = exponentialNoiseIR (1.0, 48000.0, 1, 1.0);
    auto sp = flat();
    sp.xoverMs = 80.0f; sp.earlyDb = -6.0f; sp.lateDb = 6.0f;
    auto ir = IRProcessor::render (src, sp, 48000.0);
    const int iE = 1000, iL = 20000;
    CHECK_THAT (ir.path[0][(size_t) iE] / src.channels[0][(size_t) iE], WithinRel (std::pow (10.0, -6.0 / 20.0), 1e-9));
    CHECK_THAT (ir.path[0][(size_t) iL] / src.channels[0][(size_t) iL], WithinRel (std::pow (10.0, 6.0 / 20.0), 1e-9));
}

TEST_CASE ("Damping makes highs decay faster over time (not a static filter)", "[ir][damping]")
{
    const double sr = 48000.0;
    auto src = exponentialNoiseIR (1.5, sr, 1, 1.5);
    auto sp = flat();
    sp.dampingPct = 100.0f; sp.dampFreqHz = 3000.0f;
    auto ir = IRProcessor::render (src, sp, sr);
    const auto& d = ir.path[0];
    const auto& o = src.channels[0];
    // early window: nearly untouched
    const double hiEarlyRatio = bandEnergy (d, sr, 8000, 12000, 0, 2048) / bandEnergy (o, sr, 8000, 12000, 0, 2048);
    // late window: heavily reduced
    const double hiLateRatio = bandEnergy (d, sr, 8000, 12000, 24000, 26048) / bandEnergy (o, sr, 8000, 12000, 24000, 26048);
    const double loLateRatio = bandEnergy (d, sr, 100, 300, 24000, 26048) / bandEnergy (o, sr, 100, 300, 24000, 26048);
    CHECK (hiEarlyRatio > 0.5);
    CHECK (hiLateRatio < 0.01);
    CHECK (loLateRatio > 0.5);
}

TEST_CASE ("IR swap crossfades without clicks", "[ir][swap]")
{
    const double sr = 48000.0;
    FreddyEngine e;
    e.prepare (sr, 256, 2);
    auto makeConv = [&] (double rt, unsigned seed)
    {
        auto src = exponentialNoiseIR (rt, sr, 2, rt);
        (void) seed;
        IRShapeParams sp;
        auto ir = IRProcessor::render (src, sp, sr);
        return std::make_unique<Convolver> (ir, Convolver::Config { 0, 256, true }, &e.getWorker());
    };
    e.installConvolverNow (makeConv (0.8, 1));
    auto p = fvtest::neutralParams();
    p.mixPct = 100.0f;
    // steady sine input; any discontinuity from a hard swap would show up as a large derivative
    const int n = (int) sr * 2;
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = 0.5f * (float) std::sin (2 * kPi * 220.0 * i / sr);
    std::vector<float> yL ((size_t) n), yR ((size_t) n);
    double maxStepBefore = 0, maxStepDuring = 0;
    for (int pos = 0; pos < n; pos += 256)
    {
        if (pos == n / 2) e.offerConvolver (makeConv (0.3, 2));
        const float* in[2] { x.data() + pos, x.data() + pos };
        float* out[2] { yL.data() + pos, yR.data() + pos };
        e.process (in, 2, out, 256, p);
    }
    for (int i = n / 4; i < n / 2; ++i) maxStepBefore = std::max (maxStepBefore, (double) std::abs (yL[(size_t) i] - yL[(size_t) i - 1]));
    for (int i = n / 2; i < n / 2 + (int) (0.1 * sr); ++i) maxStepDuring = std::max (maxStepDuring, (double) std::abs (yL[(size_t) i] - yL[(size_t) i - 1]));
    CHECK_FALSE (e.isCrossfading());
    CHECK (maxStepDuring < maxStepBefore * 1.5 + 1e-3);
    while (auto r = e.takeRetiredConvolver()) r.reset();
}

TEST_CASE ("Synthetic fallback IR exists for the default id", "[ir][library]")
{
    fv::IRLibrary lib;
    IRSource s;
    CHECK (lib.loadFactory (fv::IRLibrary::defaultIRId, s));
    CHECK (s.length() > 1000);
}
