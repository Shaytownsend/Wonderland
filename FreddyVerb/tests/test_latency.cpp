#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "TestHelpers.h"
#include "PluginProcessor.h"
#include "state/Parameters.h"

using namespace fvtest;

TEST_CASE ("Reported latency = convolution latency + lookahead, in every mode", "[latency]")
{
    const double sr = GENERATE (44100.0, 48000.0, 96000.0, 192000.0);
    fv::dsp::FreddyEngine e;
    e.prepare (sr, 512, 2);
    for (int q : { 0, 1 })
        for (float la : { 0.0f, 1.0f, 3.3f, 10.0f })
        {
            auto p = neutralParams();
            p.quality = q;
            p.lookaheadMs = la;
            const int expected = e.getConvolutionLatency (q) + (int) std::lround (la * 0.001 * sr);
            CHECK (e.computeLatency (p) == expected);
            if (q == 0) CHECK (e.getConvolutionLatency (q) == 0);
            else        CHECK (e.getConvolutionLatency (q) > 0);
        }
}

TEST_CASE ("Processor reports engine latency to the host", "[latency]")
{
    fv::FreddyVerbProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    CHECK (proc.getLatencySamples() == 0);
    proc.getAPVTS().getParameter (fv::pid::quality)->setValueNotifyingHost (1.0f);
    proc.getAPVTS().getParameter (fv::pid::lookahead)->setValueNotifyingHost (
        proc.getAPVTS().getParameter (fv::pid::lookahead)->convertTo0to1 (5.0f));
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    CHECK (proc.getLatencySamples() == proc.getEngine().getConvolutionLatency (1) + 240);
}

TEST_CASE ("Dry/wet alignment null test at 0% wet: output == input delayed by latency", "[latency][null]")
{
    const int q = GENERATE (0, 1);
    const float la = GENERATE (0.0f, 2.5f, 10.0f);
    const bool randomBlocks = GENERATE (false, true);
    const double sr = 48000.0;
    fv::dsp::FreddyEngine e;
    e.prepare (sr, 1024, 2);
    auto p = neutralParams();
    p.quality = q;
    p.lookaheadMs = la;
    p.mixPct = 0.0f;
    // settle the delay lines to the target (no crossfade from 0)
    e.reset();
    const int L = e.computeLatency (p);
    auto inL = noise (20000, 3), inR = noise (20000, 4);
    std::vector<float> oL, oR;
    // first call primes the latency; process a block of silence then reset to apply immediately
    runEngine (e, p, std::vector<float> (4096, 0.0f), std::vector<float> (4096, 0.0f), oL, oR, 512);
    e.reset();
    runEngine (e, p, inL, inR, oL, oR, 1024, randomBlocks);
    std::vector<float> refL (inL.size(), 0.0f), refR (inR.size(), 0.0f);
    for (size_t i = (size_t) L; i < inL.size(); ++i) { refL[i] = inL[i - (size_t) L]; refR[i] = inR[i - (size_t) L]; }
    // skip the parameter smoothing ramp (20 ms) at the very beginning
    const int settle = (int) (0.03 * sr) + L;
    INFO ("quality " << q << " lookahead " << la << " latency " << L);
    CHECK (maxAbsDiff (oL, refL, settle) < 1.0e-7);
    CHECK (maxAbsDiff (oR, refR, settle) < 1.0e-7);
}

TEST_CASE ("Bypass outputs the latency-compensated dry signal", "[latency][bypass]")
{
    const double sr = 48000.0;
    fv::dsp::FreddyEngine e;
    e.prepare (sr, 512, 2);
    auto p = neutralParams();
    p.quality = 1;
    p.lookaheadMs = 4.0f;
    p.mixPct = 80.0f;
    p.bypass = true;
    std::vector<float> oL, oR;
    runEngine (e, p, std::vector<float> (2048, 0.0f), std::vector<float> (2048, 0.0f), oL, oR, 512);
    e.reset();
    auto in = noise (16000, 5);
    runEngine (e, p, in, in, oL, oR, 333);
    const int L = e.computeLatency (p);
    std::vector<float> ref (in.size(), 0.0f);
    for (size_t i = (size_t) L; i < in.size(); ++i) ref[i] = in[i - (size_t) L];
    CHECK (maxAbsDiff (oL, ref, 2000) < 1.0e-6);
}
