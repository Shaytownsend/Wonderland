// CPU benchmark: run with  FreddyVerbTests "[bench]"
// Target from the spec: < 3% of one core at 48 kHz / 128 samples with a 3 s true-stereo IR.
#include <catch2/catch_test_macros.hpp>
#include "dsp/FreddyEngine.h"
#include "dsp/IRProcessor.h"
#include "TestHelpers.h"
#include <chrono>
#include <iostream>
#include <random>

using namespace fv::dsp;

static double measure (int quality, bool async, int block, double sr, double seconds)
{
    FreddyEngine e;
    e.prepare (sr, block, 2);
    IRSource src;
    src.sampleRate = sr;
    std::mt19937 rng (1);
    std::normal_distribution<double> g;
    src.channels.assign (4, std::vector<double> ((size_t) (3.0 * sr)));
    for (auto& c : src.channels)
        for (size_t i = 0; i < c.size(); ++i) c[i] = g (rng) * std::exp (-6.9 * (double) i / (2.5 * sr));
    auto ir = IRProcessor::render (src, IRShapeParams(), sr);
    e.installConvolverNow (std::make_unique<Convolver> (ir, Convolver::Config { quality, block, async }, &e.getWorker()));

    EngineParams p;       // defaults: duck + gate + EQ on, like a real session
    p.quality = quality;
    auto x = fvtest::noise (block, 3, 0.3f);
    std::vector<float> oL ((size_t) block), oR ((size_t) block);
    const float* in[2] { x.data(), x.data() };
    float* out[2] { oL.data(), oR.data() };
    const int blocks = (int) (seconds * sr / block);
    for (int i = 0; i < 200; ++i) e.process (in, 2, out, block, p);  // warm-up
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < blocks; ++i) e.process (in, 2, out, block, p);
    const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    return 100.0 * wall / (blocks * block / sr);
}

TEST_CASE ("CPU benchmark (audio-thread time, % of one core)", "[.bench][bench]")
{
    const double sr = 48000.0;
    const double zl = measure (0, true, 128, sr, 20.0);
    const double zlSync = measure (0, false, 128, sr, 20.0);
    const double eff = measure (1, false, 128, sr, 20.0);
    std::cout << "\nCPU @48k/128, 3 s true-stereo IR (audio thread):\n"
              << "  Zero Latency (worker for tail): " << zl << " %\n"
              << "  Zero Latency (all inline)     : " << zlSync << " %\n"
              << "  Efficient (1024, inline)       : " << eff << " %\n";
    CHECK (zlSync < 100.0);
}
