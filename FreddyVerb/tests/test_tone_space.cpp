#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/ToneSpace.h"
#include "TestHelpers.h"
#include <complex>

using namespace fv::dsp;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double sr = 48000.0;

    EngineParams allOff()
    {
        EngineParams p;
        p.hpfOn = p.lpfOn = p.lsOn = p.hsOn = p.b1On = p.b2On = false;
        return p;
    }

    /** Measured steady-state gain (dB) of the EQ for a sine at f. */
    double measure (const EngineParams& p, double f)
    {
        WetEQ eq;
        eq.prepare (sr, 256);
        const int n = (int) (sr * 0.5);
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = (float) (0.25 * std::sin (2 * kPi * f * i / sr));
        for (int pos = 0; pos < n; pos += 256) eq.process (l.data() + pos, r.data() + pos, std::min (256, n - pos), p, true);
        // exact amplitude by correlation (a sample-peak reading is phase-dependent near Nyquist)
        double re = 0, im = 0;
        for (int i = n / 2; i < n; ++i)
        {
            re += l[(size_t) i] * std::cos (2 * kPi * f * i / sr);
            im += l[(size_t) i] * std::sin (2 * kPi * f * i / sr);
        }
        const double amp = 2.0 * std::sqrt (re * re + im * im) / (n - n / 2);
        return 20 * std::log10 (amp / 0.25);
    }

    using C = std::complex<double>;
    double analogBell (double f, double f0, double g, double q)
    {
        C s (0, f / f0); const double A = std::pow (10.0, g / 40.0);
        return 20 * std::log10 (std::abs ((s * s + s * (A / q) + 1.0) / (s * s + s / (A * q) + 1.0)));
    }
    double analogHighShelf (double f, double f0, double g, double q)
    {
        C s (0, f / f0); const double A = std::pow (10.0, g / 40.0), sa = std::sqrt (A);
        return 20 * std::log10 (std::abs (A * (A * s * s + sa / q * s + 1.0) / (s * s + sa / q * s + A)));
    }
}

TEST_CASE ("Wet EQ matches the analogue response, including near Nyquist", "[tone][eq]")
{
    auto p = allOff();
    p.b1On = true; p.b1Hz = 1000.0f; p.b1GainDb = 9.0f; p.b1Q = 1.0f;
    for (double f : { 250.0, 1000.0, 3000.0 })
        CHECK_THAT (measure (p, f), WithinAbs (analogBell (f, 1000, 9, 1), 0.25));

    // a bell at 15 kHz on a 48 kHz session: un-oversampled bilinear designs cramp badly here
    p.b1Hz = 15000.0f; p.b1GainDb = 12.0f;
    for (double f : { 8000.0, 15000.0, 19000.0 })
    {
        INFO ("f " << f);
        CHECK_THAT (measure (p, f), WithinAbs (analogBell (f, 15000, 12, 1), 0.5));
    }

    p = allOff();
    p.hsOn = true; p.hsHz = 8000.0f; p.hsGainDb = -9.0f; p.hsQ = 0.707f;
    for (double f : { 2000.0, 8000.0, 16000.0 })
        CHECK_THAT (measure (p, f), WithinAbs (analogHighShelf (f, 8000, -9, 0.707), 0.3));
}

TEST_CASE ("Wet EQ filters: Butterworth slopes", "[tone][eq]")
{
    auto p = allOff();
    p.hpfOn = true; p.hpfHz = 200.0f;
    for (int slope : { 0, 1, 2 })
    {
        p.hpfSlope = slope;
        CHECK_THAT (measure (p, 200.0), WithinAbs (-3.01, 0.3));                  // -3 dB at cutoff
        const double octaveBelow = measure (p, 100.0);
        CHECK_THAT (octaveBelow, WithinAbs (-10.0 * std::log10 (1.0 + std::pow (2.0, 2.0 * 2 * (1 << slope))), 0.6));
    }
    p = allOff();
    p.lpfOn = true; p.lpfHz = 5000.0f; p.lpfSlope = 1;
    CHECK_THAT (measure (p, 5000.0), WithinAbs (-3.01, 0.3));
    CHECK (measure (p, 300.0) > -0.05);
}

TEST_CASE ("Wet EQ modulation is smooth (no zipper)", "[tone][eq]")
{
    WetEQ eq;
    eq.prepare (sr, 256);
    auto p = allOff();
    p.b1On = true; p.b1GainDb = 15.0f; p.b1Q = 4.0f;
    const int n = (int) sr;
    std::vector<float> l ((size_t) n), r ((size_t) n);
    for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = (float) (0.2 * std::sin (2 * kPi * 440.0 * i / sr));
    double maxStep = 0, prev = 0;
    for (int pos = 0; pos < n; pos += 64)
    {
        // jump the bell between 200 Hz and 2 kHz every block: worst-case automation
        p.b1Hz = (pos / 64) % 2 ? 200.0f : 2000.0f;
        eq.process (l.data() + pos, r.data() + pos, 64, p, true);
    }
    for (int i = (int) (0.1 * sr); i < n; ++i) { maxStep = std::max (maxStep, std::abs (l[(size_t) i] - prev)); prev = l[(size_t) i]; }
    // a 440 Hz sine at <= +15 dB moves at most ~0.065 per sample; a click would be far larger
    CHECK (maxStep < 0.2);
}

TEST_CASE ("Width: 0% is mono, 100% unity, 200% doubles the side", "[space]")
{
    auto run = [] (float width, bool monoBelow, float monoHz, float balance, std::vector<float>& l, std::vector<float>& r)
    {
        WidthStage w; w.prepare (sr);
        EngineParams p; p.widthPct = width; p.monoBelowOn = monoBelow; p.monoBelowHz = monoHz; p.balance = balance;
        w.process (l.data(), r.data(), 100, p);   // let smoothers settle
        w.reset();
        EngineParams q = p;
        WidthStage w2; w2.prepare (sr);
        // settle smoothers on silence, then process the signal
        std::vector<float> zl (4096, 0.0f), zr (4096, 0.0f);
        w2.process (zl.data(), zr.data(), 4096, q);
        w2.process (l.data(), r.data(), (int) l.size(), q);
    };
    const int n = 48000;
    auto L = fvtest::noise (n, 1), R = fvtest::noise (n, 2);
    auto l = L, r = R;
    run (0.0f, false, 120, 0, l, r);
    CHECK (fvtest::maxAbsDiff (l, r) < 1e-6);
    l = L; r = R;
    run (100.0f, false, 120, 0, l, r);
    CHECK (fvtest::maxAbsDiff (l, L) < 1e-6);
    l = L; r = R;
    run (200.0f, false, 120, 0, l, r);
    CHECK_THAT (l[1000] - r[1000], WithinAbs (2.0 * (L[1000] - R[1000]), 1e-5));
    l = L; r = R;
    run (100.0f, false, 120, -100.0f, l, r);
    CHECK (*std::max_element (r.begin(), r.end()) < 1e-6f);
}

TEST_CASE ("Mono-below removes low-frequency side content only", "[space]")
{
    const int n = 48000;
    std::vector<float> l ((size_t) n), r ((size_t) n);
    // side-only signal: 50 Hz + 2 kHz in anti-phase
    for (int i = 0; i < n; ++i)
    {
        const float s = (float) (0.3 * std::sin (2 * kPi * 50.0 * i / sr) + 0.3 * std::sin (2 * kPi * 2000.0 * i / sr));
        l[(size_t) i] = s; r[(size_t) i] = -s;
    }
    WidthStage w; w.prepare (sr);
    EngineParams p; p.monoBelowOn = true; p.monoBelowHz = 200.0f;
    std::vector<float> zl (4096, 0.0f), zr (4096, 0.0f);
    w.process (zl.data(), zr.data(), 4096, p);
    w.process (l.data(), r.data(), n, p);
    // measure 50 Hz and 2 kHz side energy in the second half via correlation
    auto amp = [&] (double f)
    {
        double re = 0, im = 0;
        for (int i = n / 2; i < n; ++i)
        {
            const double s = 0.5 * (l[(size_t) i] - r[(size_t) i]);
            re += s * std::cos (2 * kPi * f * i / sr); im += s * std::sin (2 * kPi * f * i / sr);
        }
        return 2.0 * std::sqrt (re * re + im * im) / (n / 2);
    };
    CHECK (amp (50.0) < 0.3 * 0.03);    // < -30 dB
    CHECK (amp (2000.0) > 0.3 * 0.97);  // untouched
}
