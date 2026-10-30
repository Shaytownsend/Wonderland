#include "helpers/render.h"
#include <catch2/catch_test_macros.hpp>
#include <dsp/Arpeggiator.h>
#include <dsp/JunoFilter.h>
#include <dsp/Lfo.h>
#include <vector>

static double sineRmsThroughFilter (queen::JunoFilter& f, double freq, double sr = 48000.0)
{
    std::vector<float> buf (4800);
    double sum = 0.0;
    for (int block = 0; block < 20; ++block)
    {
        for (size_t i = 0; i < buf.size(); ++i)
            buf[i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) (block * 4800 + (int) i) / sr);
        f.process (buf.data(), (int) buf.size());
        if (block >= 10)
            for (auto v : buf)
                sum += (double) v * v;
    }
    return std::sqrt (sum / (10.0 * 4800.0));
}

TEST_CASE ("VCF: ladder LPF attenuates above cutoff", "[juno][filter]")
{
    queen::JunoFilter f;
    f.prepare (48000.0, 4800);
    f.setHpf (0);
    f.setResonance (0.0f);
    f.setCutoff (500.0f);
    const auto pass = sineRmsThroughFilter (f, 100.0);
    f.reset();
    const auto stop = sineRmsThroughFilter (f, 4000.0);
    CHECK (pass > 0.4);
    CHECK (stop < pass * 0.02); // 3 octaves above cutoff at 24 dB/oct
    CHECK (std::abs (queen::JunoFilter::baseCutoff (0.0f) - 20.0f) < 0.01f);
    CHECK (std::abs (queen::JunoFilter::baseCutoff (1.0f) - 20000.0f) < 1.0f);
}

TEST_CASE ("HPF positions", "[juno][filter]")
{
    queen::JunoFilter f;
    f.prepare (48000.0, 4800);
    f.setResonance (0.0f);
    f.setCutoff (20000.0f);
    double prev = 1.0e9;
    for (int h = 0; h < 4; ++h)
    {
        f.reset();
        f.setHpf (h);
        const auto r = sineRmsThroughFilter (f, 100.0);
        CHECK (r < prev); // each HPF step removes more of a 100 Hz tone
        prev = r;
    }
}

TEST_CASE ("ADSR envelope shapes the VCA; gate mode is flat", "[juno][env]")
{
    auto rmsAt = [] (const juce::AudioBuffer<float>& b, double t0, double t1) {
        double s = 0.0;
        const int a = (int) (t0 * 48000), z = (int) (t1 * 48000);
        for (int i = a; i < z; ++i)
            s += (double) b.getSample (0, i) * b.getSample (0, i);
        return std::sqrt (s / (z - a));
    };
    queen::SynthEngine e;
    e.setParam ("chorus_1", 0.0f);
    e.setParam ("vcf_env", 0.0f);
    e.setParam ("vcf_freq", 1.0f);
    e.setParam ("attack", 0.3f);
    e.setParam ("decay", 0.002f);
    e.setParam ("sustain", 1.0f);
    e.setParam ("release", 0.05f);
    e.prepare (48000.0, 128);
    auto out = renderNotes (e, { 60 }, 1.0, 0.6);
    CHECK (rmsAt (out, 0.0, 0.05) < rmsAt (out, 0.4, 0.5) * 0.5); // attack ramps up
    CHECK (rmsAt (out, 0.8, 1.0) < 1.0e-4);                        // released

    queen::SynthEngine g;
    g.setParam ("chorus_1", 0.0f);
    g.setParam ("vca_mode", 1.0f);
    g.setParam ("attack", 2.0f);
    g.setParam ("vcf_env", 0.0f);
    g.setParam ("vcf_freq", 1.0f);
    g.prepare (48000.0, 128);
    auto gated = renderNotes (g, { 60 }, 0.5);
    CHECK (rmsAt (gated, 0.01, 0.05) > rmsAt (gated, 0.4, 0.45) * 0.7); // no attack ramp in gate mode
}

TEST_CASE ("LFO: triangle, delay fade-in, manual trigger", "[juno][lfo]")
{
    queen::Lfo l;
    l.prepare (1000.0);
    float mx = -2, mn = 2;
    for (int i = 0; i < 1000; ++i)
    {
        const float v = l.advance (1, 1.0f, 0.0f, false, false);
        mx = std::max (mx, v);
        mn = std::min (mn, v);
    }
    CHECK (mx > 0.99f);
    CHECK (mn < -0.99f);

    queen::Lfo d;
    d.prepare (1000.0);
    d.retrigger();
    d.advance (500, 1.0f, 1.0f, false, false);
    CHECK (std::abs (d.currentFade() - 0.5f) < 0.01f);
    d.advance (600, 1.0f, 1.0f, false, false);
    CHECK (d.currentFade() == 1.0f);

    queen::Lfo m;
    m.prepare (1000.0);
    CHECK (m.advance (100, 5.0f, 0.0f, true, false) == 0.0f); // manual: silent until trigger
    CHECK (m.advance (40, 5.0f, 0.0f, true, true) != 0.0f);
}

TEST_CASE ("Chorus I / II / I+II widen the signal", "[juno][chorus]")
{
    auto sideEnergy = [] (bool c1, bool c2) {
        queen::SynthEngine e;
        e.setParam ("chorus_1", c1 ? 1.0f : 0.0f);
        e.setParam ("chorus_2", c2 ? 1.0f : 0.0f);
        e.prepare (48000.0, 128);
        auto out = renderNotes (e, { 60 }, 0.5);
        double side = 0.0;
        for (int i = 0; i < out.getNumSamples(); ++i)
        {
            const double s = out.getSample (0, i) - out.getSample (1, i);
            side += s * s;
        }
        CHECK_FALSE (stats (out).hasNaN);
        return side;
    };
    CHECK (sideEnergy (false, false) < 1.0e-9);
    CHECK (sideEnergy (true, false) > 1.0e-4);
    CHECK (sideEnergy (false, true) > 1.0e-4);
    CHECK (sideEnergy (true, true) > 1.0e-4);
}

// --------------------------------------------------------------------------- arpeggiator
static std::vector<int> sequence (int mode, int range, std::initializer_list<int> held, int steps)
{
    queen::Arpeggiator a;
    a.prepare (48000.0);
    a.setMode (mode);
    a.setRange (range);
    for (int n : held)
        a.noteOn (n, 0.8f);
    std::vector<int> seq;
    for (int i = 0; i < steps; ++i)
        seq.push_back (a.nextNote());
    return seq;
}

TEST_CASE ("Arp note order: up / updown / down at range 1-3", "[juno][arp]")
{
    using V = std::vector<int>;
    // held out of order on purpose: the arp sorts
    CHECK (sequence (0, 1, { 64, 60, 67 }, 6) == V { 60, 64, 67, 60, 64, 67 });
    CHECK (sequence (0, 2, { 60, 64, 67 }, 7) == V { 60, 64, 67, 72, 76, 79, 60 });
    CHECK (sequence (0, 3, { 60, 64 }, 7) == V { 60, 64, 72, 76, 84, 88, 60 });

    CHECK (sequence (1, 1, { 60, 64, 67 }, 6) == V { 60, 64, 67, 64, 60, 64 });
    CHECK (sequence (1, 2, { 60, 64, 67 }, 11) == V { 60, 64, 67, 72, 76, 79, 76, 72, 67, 64, 60 });
    CHECK (sequence (1, 3, { 60 }, 6) == V { 60, 72, 84, 72, 60, 72 });

    CHECK (sequence (2, 1, { 60, 64, 67 }, 4) == V { 67, 64, 60, 67 });
    CHECK (sequence (2, 2, { 60, 64, 67 }, 7) == V { 79, 76, 72, 67, 64, 60, 79 });
    CHECK (sequence (2, 3, { 60, 64 }, 7) == V { 88, 84, 76, 72, 64, 60, 88 });
}

TEST_CASE ("Arp timing is sample accurate and hold latches", "[juno][arp]")
{
    queen::Arpeggiator a;
    a.prepare (48000.0);
    a.setRate (10.0); // 4800 samples per step
    a.noteOn (60, 0.8f);
    a.noteOn (64, 0.8f);

    std::vector<std::pair<int, int>> ons; // (sample, note)
    int t = 0;
    while (t < 48000)
    {
        a.processDue ([&] (const queen::Arpeggiator::Event& e) { if (e.on) ons.emplace_back (t, e.note); });
        const int n = std::max (1, a.samplesUntilNextEvent());
        a.advance (n);
        t += n;
    }
    REQUIRE (ons.size() == 10);
    for (size_t i = 0; i < ons.size(); ++i)
    {
        CHECK (ons[i].first == (int) i * 4800);
        CHECK (ons[i].second == (i % 2 == 0 ? 60 : 64));
    }

    queen::Arpeggiator h;
    h.prepare (48000.0);
    h.setHold (true);
    h.noteOn (60, 0.8f);
    h.noteOff (60);
    CHECK (h.isRunning()); // latched
    h.setKeyTranspose (true);
    h.noteOn (65, 0.8f);   // transposes the latched pattern by +5
    CHECK (h.nextNote() == 65);
    h.setKeyTranspose (false);
    h.noteOff (65);
    h.noteOn (70, 0.8f);   // new chord after release replaces the latch
    CHECK (h.nextNote() == 70);
    h.setHold (false);
    h.noteOff (70);
    CHECK_FALSE (h.isRunning());
}

TEST_CASE ("Engine: arp drives voices from held keys", "[juno][arp]")
{
    queen::SynthEngine e;
    e.setParam ("arp_on", 1.0f);
    e.setParam ("arp_rate", 20.0f);
    e.prepare (48000.0, 128);
    auto out = renderNotes (e, { 60, 64, 67 }, 0.5);
    CHECK (stats (out).rms > 1.0e-3);
    CHECK (e.getActiveVoiceCount() >= 1);
}
