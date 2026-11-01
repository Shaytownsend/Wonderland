#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "dsp/Convolver.h"
#include "TestHelpers.h"
#include <random>
#include <thread>

using namespace fv::dsp;

namespace
{
    std::vector<double> randomIR (int len, unsigned seed, double decaySec = 0.5, double sr = 48000.0)
    {
        std::mt19937 rng (seed);
        std::normal_distribution<double> g (0.0, 1.0);
        std::vector<double> h ((size_t) len);
        for (int i = 0; i < len; ++i) h[(size_t) i] = g (rng) * std::exp (-(double) i / (decaySec * sr));
        return h;
    }

    /** Brute-force true-stereo convolution in long double. */
    void bruteForce (const IRData& ir, const std::vector<float>& xL, const std::vector<float>& xR,
                     std::vector<double>& yL, std::vector<double>& yR)
    {
        const size_t n = xL.size();
        yL.assign (n, 0.0); yR.assign (n, 0.0);
        auto conv = [&] (const std::vector<double>& h, const std::vector<float>& x, std::vector<double>& y)
        {
            for (size_t i = 0; i < n; ++i)
            {
                if (x[i] == 0.0f) continue;
                const long double xi = x[i];
                const size_t m = std::min (h.size(), n - i);
                for (size_t k = 0; k < m; ++k) y[i + k] += (double) (xi * (long double) h[k]);
            }
        };
        if (ir.active[IRData::LL]) conv (ir.path[IRData::LL], xL, yL);
        if (ir.active[IRData::RL]) conv (ir.path[IRData::RL], xR, yL);
        if (ir.active[IRData::LR]) conv (ir.path[IRData::LR], xL, yR);
        if (ir.active[IRData::RR]) conv (ir.path[IRData::RR], xR, yR);
    }

    double errorDb (const std::vector<float>& got, const std::vector<double>& ref, int latency)
    {
        double errMax = 0.0, refMax = 1.0e-30;
        for (size_t i = 0; i < ref.size(); ++i)
        {
            const size_t j = i + (size_t) latency;
            if (j >= got.size()) break;
            errMax = std::max (errMax, std::abs ((double) got[j] - ref[i]));
            refMax = std::max (refMax, std::abs (ref[i]));
        }
        return fvtest::toDb (errMax / refMax);
    }

    void runConvolver (Convolver& c, const std::vector<float>& xL, const std::vector<float>& xR,
                       std::vector<float>& yL, std::vector<float>& yR, int maxBlock, bool random, bool realtimePacing)
    {
        const size_t n = xL.size();
        yL.assign (n, 0.0f); yR.assign (n, 0.0f);
        std::mt19937 rng (5);
        std::uniform_int_distribution<int> bs (1, maxBlock);
        for (size_t pos = 0; pos < n;)
        {
            const int b = (int) std::min<size_t> (n - pos, (size_t) (random ? bs (rng) : maxBlock));
            c.process (xL.data() + pos, xR.data() + pos, yL.data() + pos, yR.data() + pos, b);
            pos += (size_t) b;
            if (realtimePacing && (pos / (size_t) maxBlock) % 7 == 0)
                std::this_thread::sleep_for (std::chrono::microseconds (200)); // give the worker some wall time
        }
    }
}

TEST_CASE ("FFT backend round-trip is exact to double precision", "[fft]")
{
    for (int n : { 32, 128, 1024, 8192, 65536 })
    {
        RealFFT fft (n);
        AlignedBuffer x ((size_t) n), X ((size_t) n), w ((size_t) n), y ((size_t) n), H ((size_t) n), acc ((size_t) n), d ((size_t) n);
        std::mt19937 rng (n);
        std::uniform_real_distribution<double> u (-1, 1);
        for (int i = 0; i < n; ++i) x[(size_t) i] = u (rng);
        d[0] = 1.0;  // delta: x (*) delta == x
        fft.forward (x.data(), X.data(), w.data());
        fft.forward (d.data(), H.data(), w.data());
        fft.multiplyAccumulate (X.data(), H.data(), acc.data());
        fft.inverse (acc.data(), y.data(), w.data());
        double err = 0;
        for (int i = 0; i < n; ++i) err = std::max (err, std::abs (y[(size_t) i] * fft.getConvolutionScale() - x[(size_t) i]));
        INFO ("N = " << n << " backend " << RealFFT::backendName());
        CHECK (err < 1.0e-13);
    }
}

TEST_CASE ("Convolution matches brute-force reference (< -120 dB)", "[convolver]")
{
    const int irLen = GENERATE (1, 63, 64, 65, 1000, 1500, 9000, 70000, 150000);
    const int quality = GENERATE (0, 1);
    const int channels = GENERATE (1, 2, 4);
    const bool async = GENERATE (false, true);

    IRData ir;
    ir.sampleRate = 48000.0;
    const unsigned seed = (unsigned) (irLen * 7 + channels);
    if (channels == 1) { ir.path[IRData::LL] = ir.path[IRData::RR] = randomIR (irLen, seed); ir.active[0] = ir.active[3] = true; }
    if (channels == 2) { ir.path[IRData::LL] = randomIR (irLen, seed); ir.path[IRData::RR] = randomIR (irLen, seed + 1); ir.active[0] = ir.active[3] = true; }
    if (channels == 4) for (int p = 0; p < 4; ++p) { ir.path[(size_t) p] = randomIR (irLen, seed + (unsigned) p); ir.active[(size_t) p] = true; }

    ConvolutionWorker worker;
    Convolver::Config cfg;
    cfg.quality = quality;
    cfg.maxBlockSize = 256;
    cfg.allowAsync = async;
    Convolver conv (ir, cfg, &worker);
    if (async && quality == 0 && irLen > 2000) CHECK (conv.isStageAsync (conv.getNumStages() - 1));

    const int n = std::min (irLen + 4096, 200000);
    auto xL = fvtest::noise (n, 11), xR = fvtest::noise (n, 12);
    if (irLen > 5000)
    {
        // sparse bursts keep the brute-force reference fast while still exercising every partition
        for (int i = 0; i < n; ++i)
        {
            const bool on = (i % 16384) < 1500;
            if (! on) { xL[(size_t) i] = 0.0f; xR[(size_t) i] = 0.0f; }
        }
    }
    std::vector<float> yL, yR;
    runConvolver (conv, xL, xR, yL, yR, 256, true, async);

    std::vector<double> rL, rR;
    bruteForce (ir, xL, xR, rL, rR);
    // the convolver output is float; compare relative to the output peak.
    // float output rounding is ~-150 dB, so the -120 dB bound tests the engine's double maths.
    INFO ("irLen " << irLen << " quality " << quality << " ch " << channels << " async " << async);
    CHECK (errorDb (yL, rL, conv.getLatency()) < -120.0);
    CHECK (errorDb (yR, rR, conv.getLatency()) < -120.0);
}

TEST_CASE ("Convolver latency is 0 in zero-latency mode and kEfficientPartition in efficient mode", "[convolver][latency]")
{
    IRData ir;
    ir.path[IRData::LL] = ir.path[IRData::RR] = { 1.0 };
    ir.active[0] = ir.active[3] = true;
    Convolver z (ir, { 0, 512, false }, nullptr);
    Convolver e (ir, { 1, 512, false }, nullptr);
    CHECK (z.getLatency() == 0);
    CHECK (e.getLatency() == Convolver::kEfficientPartition);

    // unit impulse IR: output == input delayed by the reported latency (bit exact up to float rounding)
    auto x = fvtest::noise (5000, 3);
    std::vector<float> yL, yR;
    runConvolver (e, x, x, yL, yR, 333, true, false);
    double err = 0;
    const size_t L = (size_t) Convolver::kEfficientPartition;
    for (size_t i = 0; i + L < x.size(); ++i) err = std::max (err, (double) std::abs (yL[i + L] - x[i]));
    CHECK (err < 1.0e-6);
    runConvolver (z, x, x, yL, yR, 333, true, false);
    err = 0;
    for (size_t i = 0; i < x.size(); ++i) err = std::max (err, (double) std::abs (yL[i] - x[i]));
    CHECK (err < 1.0e-7);
}

TEST_CASE ("Convolver handles any block size 1..8192 and reset", "[convolver]")
{
    IRData ir;
    ir.path[IRData::LL] = randomIR (20000, 1);
    ir.path[IRData::RR] = randomIR (20000, 2);
    ir.active[0] = ir.active[3] = true;
    std::vector<double> rL, rR;
    auto x = fvtest::noise (40000, 9);
    for (size_t i = 0; i < x.size(); ++i) if ((i % 8192) > 1200) x[i] = 0.0f;
    bruteForce (ir, x, x, rL, rR);
    for (int bs : { 1, 7, 64, 128, 1000, 4096, 8192 })
    {
        ConvolutionWorker worker;
        Convolver c (ir, { 0, bs, true }, &worker);
        std::vector<float> yL, yR;
        runConvolver (c, x, x, yL, yR, bs, false, true);
        INFO ("block " << bs);
        CHECK (errorDb (yL, rL, 0) < -120.0);
        c.reset();
        runConvolver (c, x, x, yL, yR, bs, true, true);
        CHECK (errorDb (yR, rR, 0) < -120.0);
    }
}
