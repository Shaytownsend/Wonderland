#include "helpers/render.h"
#include <catch2/catch_test_macros.hpp>
#include <dsp/Algorithms.h>

TEST_CASE ("Algorithm routing tables", "[algorithms]")
{
    using queen::kAlgorithms;
    // modulators always have higher index than their targets (op4 -> op1 render order is valid)
    for (const auto& a : kAlgorithms)
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j <= i; ++j)
                CHECK_FALSE (a.mod[(size_t) i][(size_t) j]);

    const int carriers[8] { 1, 2, 2, 1, 1, 3, 3, 4 };
    for (int a = 0; a < 8; ++a)
        CHECK (queen::numCarriers (kAlgorithms[(size_t) a]) == carriers[a]);

    // spot-check the spec's routings (indices 0..3 = op1..op4)
    CHECK (kAlgorithms[0].mod[0][1]); // 1: 2->1
    CHECK (kAlgorithms[0].mod[2][3]); //    4->3
    CHECK (kAlgorithms[1].mod[1][2]); // 2: 3->2
    CHECK (kAlgorithms[1].mod[0][2]); //    3->1
    CHECK (kAlgorithms[2].mod[1][3]); // 3: 4->2
    CHECK (kAlgorithms[2].mod[0][2]); //    3->1
    CHECK (kAlgorithms[3].mod[1][2]); // 4: 3->2
    CHECK (kAlgorithms[3].mod[1][3]); //    4->2
    CHECK (kAlgorithms[4].mod[0][1]); // 5: 2->1
    CHECK (kAlgorithms[4].mod[0][2]); //    3->1
    CHECK (kAlgorithms[4].mod[2][3]); //    4->3
    CHECK (kAlgorithms[5].mod[2][3]); // 6: 4->3,2,1
    CHECK (kAlgorithms[6].mod[2][3]); // 7: 4->3
    CHECK_FALSE (kAlgorithms[6].mod[1][3]);
}

TEST_CASE ("Voice renders 1 s of C4 for every algorithm", "[voice]")
{
    for (int a = 0; a < 8; ++a)
    {
        INFO ("algorithm " << a + 1);
        queen::SynthEngine e;
        e.setParam ("fm_algorithm", (float) a);
        e.setParam ("chorus_1", 0.0f);
        e.prepare (48000.0, 128);
        auto out = renderNotes (e, { 60 }, 1.0);
        auto s = stats (out);
        CHECK_FALSE (s.hasNaN);
        CHECK (s.rms > 1.0e-3);       // non-silent
        CHECK (s.peak < 1.0f);        // < 0 dBFS
    }
}

TEST_CASE ("Voice render snapshot is deterministic", "[voice]")
{
    auto renderOnce = [] {
        queen::SynthEngine e;
        e.setParam ("chorus_1", 0.0f);
        e.prepare (48000.0, 128);
        return renderNotes (e, { 60 }, 0.25);
    };
    auto a = renderOnce();
    auto b = renderOnce();
    float diff = 0.0f;
    for (int i = 0; i < a.getNumSamples(); ++i)
        diff = std::max (diff, std::abs (a.getSample (0, i) - b.getSample (0, i)));
    CHECK (diff == 0.0f);
    auto s = stats (a);
    // snapshot bounds of the default patch (algorithm 5)
    CHECK (s.rms > 0.01);
    CHECK (s.rms < 0.5);
}

TEST_CASE ("Polyphony: 6 voices, oldest stolen, no clipping", "[voice]")
{
    queen::SynthEngine e;
    e.prepare (48000.0, 128);
    auto out = renderNotes (e, { 48, 52, 55, 60, 64, 67, 72 }, 0.5);
    CHECK (e.getActiveVoiceCount() == 6);
    auto s = stats (out);
    CHECK_FALSE (s.hasNaN);
    CHECK (s.peak < 1.0f);
}

TEST_CASE ("Release ends the voice", "[voice]")
{
    queen::SynthEngine e;
    e.setParam ("release", 0.05f);
    e.prepare (48000.0, 128);
    renderNotes (e, { 60 }, 0.6, 0.1);
    CHECK (e.getActiveVoiceCount() == 0);
}

TEST_CASE ("2x oversampling renders and reports latency", "[voice]")
{
    queen::SynthEngine e;
    e.prepare (48000.0, 128, 2);
    auto s = stats (renderNotes (e, { 60 }, 0.3));
    CHECK (s.rms > 1.0e-3);
    CHECK_FALSE (s.hasNaN);
    CHECK (e.getLatencySamples() >= 0);
}

static double zeroCrossFreq (const juce::AudioBuffer<float>& b, int from, int to, double sr)
{
    int crossings = 0;
    for (int i = from + 1; i < to; ++i)
        if (b.getSample (0, i - 1) < 0.0f && b.getSample (0, i) >= 0.0f)
            ++crossings;
    return crossings * sr / (to - from);
}

TEST_CASE ("Pitch: C4 at 8', octave/range shifts, tune", "[voice]")
{
    auto measure = [] (std::function<void (queen::SynthEngine&)> setup) {
        queen::SynthEngine e;
        e.setParam ("fm_algorithm", 7.0f); // all carriers, all ratio 1 -> pure sine at f0
        e.setParam ("chorus_1", 0.0f);
        e.setParam ("vcf_freq", 1.0f);
        e.setParam ("vcf_res", 0.0f);
        e.setParam ("hpf", 0.0f);
        e.setParam ("wave_sub", 0.0f); // op3 as carrier would sound an octave down
        e.setParam ("op3_level", 0.0f);
        for (auto id : { "op1_feedback", "op2_feedback", "op3_feedback", "op4_feedback" })
            e.setParam (id, 0.0f);
        setup (e);
        e.prepare (48000.0, 128);
        auto out = renderNotes (e, { 60 }, 1.0);
        return zeroCrossFreq (out, 4800, 48000, 48000.0);
    };
    CHECK (std::abs (measure ([] (auto&) {}) - 261.63) < 3.0);
    CHECK (std::abs (measure ([] (auto& e) { e.setParam ("octave", 2.0f); }) - 523.25) < 5.0);
    CHECK (std::abs (measure ([] (auto& e) { e.setParam ("dco_range", 0.0f); }) - 130.81) < 2.0);
    CHECK (std::abs (measure ([] (auto& e) { e.setParam ("tune", 50.0f); }) - 269.29) < 3.0);
}
