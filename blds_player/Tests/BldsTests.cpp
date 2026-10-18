// BLDS Player verification: no GUI, no host. Drives the real plugin processor
// (linked from the plugin's shared code) and checks bit-exact playback.
//
//   usage: <TestBinary> [--seconds N]     (default 600 = 10 minutes per run)

#include "PluginProcessor.h"
#include "BldsLibrary.h"
#include "BldsWav.h"

#include "BldsEmbedded.h"
#include <juce_events/juce_events.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <new>
#include <random>
#include <string>
#include <vector>

//==============================================================================
// Allocation guard: any operator new on a thread inside processBlock is a failure.
//==============================================================================
namespace
{
    thread_local bool guardActive = false;
    std::atomic<long long> guardViolations { 0 };

    void* guardedAlloc (std::size_t n)
    {
        if (guardActive)
            guardViolations.fetch_add (1, std::memory_order_relaxed);

        return std::malloc (n != 0 ? n : 1);
    }

    void* guardedAlignedAlloc (std::size_t n, std::size_t align)
    {
        if (guardActive)
            guardViolations.fetch_add (1, std::memory_order_relaxed);

        void* p = nullptr;

        if (align < sizeof (void*))
            align = sizeof (void*);

        if (posix_memalign (&p, align, n != 0 ? n : 1) != 0)
            return nullptr;

        return p;
    }
}

void* operator new (std::size_t n)                                            { if (auto* p = guardedAlloc (n)) return p; throw std::bad_alloc(); }
void* operator new[] (std::size_t n)                                          { if (auto* p = guardedAlloc (n)) return p; throw std::bad_alloc(); }
void* operator new (std::size_t n, const std::nothrow_t&) noexcept            { return guardedAlloc (n); }
void* operator new[] (std::size_t n, const std::nothrow_t&) noexcept          { return guardedAlloc (n); }
void* operator new (std::size_t n, std::align_val_t a)                        { if (auto* p = guardedAlignedAlloc (n, (std::size_t) a)) return p; throw std::bad_alloc(); }
void* operator new[] (std::size_t n, std::align_val_t a)                      { if (auto* p = guardedAlignedAlloc (n, (std::size_t) a)) return p; throw std::bad_alloc(); }
void* operator new (std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept   { return guardedAlignedAlloc (n, (std::size_t) a); }
void* operator new[] (std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { return guardedAlignedAlloc (n, (std::size_t) a); }
void operator delete (void* p) noexcept                                       { std::free (p); }
void operator delete[] (void* p) noexcept                                     { std::free (p); }
void operator delete (void* p, std::size_t) noexcept                          { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept                        { std::free (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept                { std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept              { std::free (p); }
void operator delete (void* p, std::align_val_t) noexcept                     { std::free (p); }
void operator delete[] (void* p, std::align_val_t) noexcept                   { std::free (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept        { std::free (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept      { std::free (p); }
void operator delete (void* p, std::align_val_t, const std::nothrow_t&) noexcept   { std::free (p); }
void operator delete[] (void* p, std::align_val_t, const std::nothrow_t&) noexcept { std::free (p); }

namespace
{
using Processor = BldsPlayerAudioProcessor;
using Bits = std::vector<std::uint32_t>;

constexpr int maxBlock = 4096;
constexpr std::uint32_t sentinelBits = 0x7FA5A5A5u;   // a NaN: any unwritten sample shows up

std::uint32_t bitsOf (float f) noexcept
{
    std::uint32_t b;
    std::memcpy (&b, &f, sizeof (b));
    return b;
}

//==============================================================================
// Result table
//==============================================================================
struct Row
{
    std::string test;
    std::string rate;
    bool pass;
    std::string detail;
};

std::vector<Row> rows;

void record (std::string test, int rate, bool pass, std::string detail)
{
    rows.push_back ({ std::move (test), rate > 0 ? std::to_string (rate) : std::string ("-"), pass, std::move (detail) });
    std::printf ("  [%s] %-44s %7s  %s\n", pass ? "PASS" : "FAIL", rows.back().test.c_str(),
                 rows.back().rate.c_str(), rows.back().detail.c_str());
    std::fflush (stdout);
}

//==============================================================================
// Test fixtures (NOT BLDS: random 24-bit test data used to exercise the engine
// and parser. Never embedded in the plugin.)
//==============================================================================
struct WavSpec
{
    std::uint16_t formatTag = 1;
    std::uint16_t channels = 1;
    std::uint32_t sampleRate = 48000;
    std::uint16_t bits = 24;
    bool extensible = false;
    bool oddChunkBeforeData = false;
};

void putU16 (std::vector<unsigned char>& v, std::uint32_t x) { v.push_back ((unsigned char) x); v.push_back ((unsigned char) (x >> 8)); }
void putU32 (std::vector<unsigned char>& v, std::uint32_t x) { putU16 (v, x & 0xffff); putU16 (v, x >> 16); }
void putId  (std::vector<unsigned char>& v, const char* id)   { v.insert (v.end(), id, id + 4); }

std::vector<unsigned char> makeWav (const WavSpec& spec, const std::vector<std::int32_t>& samples)
{
    const std::uint32_t bytesPerSample = spec.bits / 8u;
    const std::uint32_t blockAlign = bytesPerSample * spec.channels;

    std::vector<unsigned char> fmt;
    putU16 (fmt, spec.extensible ? 0xFFFEu : spec.formatTag);
    putU16 (fmt, spec.channels);
    putU32 (fmt, spec.sampleRate);
    putU32 (fmt, spec.sampleRate * blockAlign);
    putU16 (fmt, blockAlign);
    putU16 (fmt, spec.bits);

    if (spec.extensible)
    {
        putU16 (fmt, 22);
        putU16 (fmt, spec.bits);
        putU32 (fmt, 4);
        const unsigned char guid[16] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                         0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 };
        fmt.insert (fmt.end(), guid, guid + 16);
    }

    std::vector<unsigned char> data;

    for (auto s : samples)
        for (std::uint32_t c = 0; c < spec.channels; ++c)
            for (std::uint32_t b = 0; b < bytesPerSample; ++b)
                data.push_back ((unsigned char) ((std::uint32_t) s >> (8 * b)));

    std::vector<unsigned char> body;
    putId (body, "WAVE");
    putId (body, "fmt ");
    putU32 (body, (std::uint32_t) fmt.size());
    body.insert (body.end(), fmt.begin(), fmt.end());

    if (spec.oddChunkBeforeData)
    {
        putId (body, "LIST");
        putU32 (body, 3);
        body.push_back ('a'); body.push_back ('b'); body.push_back ('c');
        body.push_back (0);   // pad byte
    }

    putId (body, "data");
    putU32 (body, (std::uint32_t) data.size());
    body.insert (body.end(), data.begin(), data.end());

    if (data.size() & 1u)
        body.push_back (0);

    std::vector<unsigned char> file;
    putId (file, "RIFF");
    putU32 (file, (std::uint32_t) body.size());
    file.insert (file.end(), body.begin(), body.end());
    return file;
}

std::vector<std::int32_t> randomInt24 (std::size_t n, std::uint32_t seed)
{
    std::mt19937 rng (seed);
    std::uniform_int_distribution<std::int32_t> dist (-8388608, 8388607);
    std::vector<std::int32_t> v (n);

    for (auto& s : v)
        s = dist (rng);

    // Always include the extremes and zero.
    if (n >= 3) { v[0] = -8388608; v[1] = 8388607; v[2] = 0; }
    return v;
}

//==============================================================================
// Independent reference decoder (deliberately a separate implementation from the
// plugin's): finds the 'data' chunk and converts with shift-based sign extension
// and a double-precision division.
//==============================================================================
bool referenceDecode (const unsigned char* p, std::size_t n, Bits& out)
{
    if (n < 12 || std::memcmp (p, "RIFF", 4) != 0 || std::memcmp (p + 8, "WAVE", 4) != 0)
        return false;

    std::size_t pos = 12;

    while (pos + 8 <= n)
    {
        const std::uint32_t size = (std::uint32_t) p[pos + 4] | ((std::uint32_t) p[pos + 5] << 8)
                                 | ((std::uint32_t) p[pos + 6] << 16) | ((std::uint32_t) p[pos + 7] << 24);

        if (std::memcmp (p + pos, "data", 4) == 0)
        {
            if (pos + 8 + size > n)
                return false;

            out.clear();

            for (std::size_t i = pos + 8; i + 3 <= pos + 8 + size; i += 3)
            {
                const std::int32_t s = (std::int32_t) (((std::uint32_t) p[i] << 8) | ((std::uint32_t) p[i + 1] << 16)
                                                       | ((std::uint32_t) p[i + 2] << 24)) >> 8;
                out.push_back (bitsOf ((float) ((double) s / 8388608.0)));
            }

            return ! out.empty();
        }

        pos += 8 + size + (size & 1u);
    }

    return false;
}

Bits tableBits (const blds::Table& t)
{
    Bits b;
    b.reserve (t.samples.size());

    for (float f : t.samples)
        b.push_back (bitsOf (f));

    return b;
}

//==============================================================================
// Driver: renders through the real processor and checks every sample.
//==============================================================================
class Driver
{
public:
    explicit Driver (Processor& p) : proc (p), storage (2, maxBlock)
    {
        midi.ensureSize (256);
    }

    void prepare (double sampleRate, int blockSize)
    {
        proc.setPlayConfigDetails (proc.getTotalNumInputChannels(), 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
    }

    // expected == nullptr means "exact silence on both channels".
    void setExpectation (const Bits* expectedTable, std::uint64_t startPos)
    {
        expected = expectedTable;
        expectedPos = startPos;
    }

    std::uint64_t getExpectedPos() const noexcept { return expectedPos; }

    // Returns false (and fills 'error') on the first mismatch.
    bool block (int n, bool bypassed, std::string& error)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = storage.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                std::memcpy (d + i, &sentinelBits, 4);   // also acts as "input" for the effect variant
        }

        juce::AudioBuffer<float> view (storage.getArrayOfWritePointers(), 2, n);
        midi.clear();

        const auto before = guardViolations.load();
        guardActive = true;

        if (bypassed)
            proc.processBlockBypassed (view, midi);
        else
            proc.processBlock (view, midi);

        guardActive = false;

        if (guardViolations.load() != before)
        {
            error = "heap allocation inside processBlock";
            return false;
        }

        const auto mode = proc.getOutputChannel();
        const float* l = storage.getReadPointer (0);
        const float* r = storage.getReadPointer (1);

        for (int i = 0; i < n; ++i)
        {
            const std::uint32_t lb = bitsOf (l[i]);
            const std::uint32_t rb = bitsOf (r[i]);
            std::uint32_t wantL = 0, wantR = 0;

            if (expected != nullptr)
            {
                const std::uint32_t s = (*expected)[(std::size_t) expectedPos];

                if (! bypassed)
                {
                    if (mode == Processor::OutputChannel::rightOnly)     wantR = s;
                    else if (mode == Processor::OutputChannel::leftOnly) wantL = s;
                    else                                                 wantL = wantR = s;
                }

                expectedPos = (expectedPos + 1 == expected->size()) ? 0 : expectedPos + 1;
            }

            if (lb != wantL || rb != wantR)
            {
                char buf[256];
                std::snprintf (buf, sizeof (buf), "sample %llu of block: L=0x%08X (want 0x%08X) R=0x%08X (want 0x%08X), table pos %llu",
                               (unsigned long long) i, lb, wantL, rb, wantR, (unsigned long long) expectedPos);
                error = buf;
                return false;
            }
        }

        samplesChecked += (std::uint64_t) n;
        return true;
    }

    std::uint64_t samplesChecked = 0;

private:
    Processor& proc;
    juce::AudioBuffer<float> storage;
    juce::MidiBuffer midi;
    const Bits* expected = nullptr;
    std::uint64_t expectedPos = 0;
};

void setOutputChannel (Processor& p, Processor::OutputChannel c)
{
    auto* param = p.getValueTreeState().getParameter (Processor::outputChannelParamID);
    param->setValueNotifyingHost (param->convertTo0to1 ((float) (int) c));
}

using Factory = std::function<std::unique_ptr<Processor>()>;

// Renders 'seconds' of audio with the given block-size policy (blockSize == 0 => random).
bool renderRun (Processor& p, int rate, const Bits* expected, std::uint64_t seconds, int blockSize,
                std::uint32_t seed, std::string& detail)
{
    Driver d (p);
    d.prepare ((double) rate, blockSize > 0 ? blockSize : maxBlock);
    d.setExpectation (expected, 0);

    std::mt19937 rng (seed);
    std::uniform_int_distribution<int> dist (1, maxBlock);
    const std::uint64_t total = (std::uint64_t) rate * seconds;
    std::uint64_t done = 0;
    std::string error;

    while (done < total)
    {
        int n = blockSize > 0 ? blockSize : dist (rng);
        n = (int) std::min<std::uint64_t> ((std::uint64_t) n, total - done);
        if (! d.block (n, false, error))
        {
            detail = "at output sample " + std::to_string (done) + ": " + error;
            return false;
        }

        done += (std::uint64_t) n;
    }

    detail = std::to_string (done) + " samples bit-exact";

    if (expected != nullptr)
        detail += ", " + std::to_string (done / expected->size()) + " loop wraps";

    return true;
}

//==============================================================================
// Test groups
//==============================================================================
void testConversion()
{
    std::uint64_t bad = 0;

    for (std::int32_t s = -8388608; s <= 8388607; ++s)
    {
        const float f = blds::int24ToFloat (s);

        // Exact: widening to double and the double-precision quotient must be identical.
        const double viaFloat = (double) f, exact = (double) s / 8388608.0;
        std::uint64_t a, b;
        std::memcpy (&a, &viaFloat, sizeof (a));
        std::memcpy (&b, &exact, sizeof (b));

        if (a != b || (std::int32_t) (f * 8388608.0f) != s)
            ++bad;
    }

    record ("int24 -> float exact for all 2^24 values", 0, bad == 0,
            bad == 0 ? "16777216 values round-trip exactly" : std::to_string (bad) + " inexact values");
}

// Proves the harness itself can fail: the allocation hook must trip, and the
// bit comparison must catch a single corrupted sample.
int* volatile allocationSink = nullptr;

void testHarnessSelfChecks()
{
    const auto before = guardViolations.load();
    guardActive = true;
    allocationSink = new int (1);
    guardActive = false;
    delete allocationSink;
    allocationSink = nullptr;
    const bool caught = guardViolations.load() == before + 1;
    guardViolations.store (before);   // the deliberate allocation is not a real failure
    record ("self-check: allocation hook trips", 0, caught, caught ? "deliberate allocation detected" : "hook did not fire");

    const auto samples = randomInt24 (4801, 77);
    WavSpec spec;
    const auto wav = makeWav (spec, samples);
    std::vector<blds::Library::Blob> blobs { { 48000, wav.data(), wav.size() } };
    Processor p (blds::Library::fromBlobs (blobs));

    Bits corrupted;
    referenceDecode (wav.data(), wav.size(), corrupted);
    corrupted[1234] ^= 1u;   // flip one mantissa bit

    std::string detail;
    const bool detected = ! renderRun (p, 48000, &corrupted, 1, 0, 1, detail);
    record ("self-check: 1-bit corruption detected", 48000, detected, detected ? detail : "corruption NOT detected");
}

void testParser()
{
    const auto samples = randomInt24 (1001, 7);

    auto expectOk = [&] (const char* name, const WavSpec& spec)
    {
        const auto wav = makeWav (spec, samples);
        const auto r = blds::decodeBldsWav (wav.data(), wav.size(), spec.sampleRate);
        bool exact = r.ok && r.samples.size() == samples.size();

        for (std::size_t i = 0; exact && i < samples.size(); ++i)
            exact = bitsOf (r.samples[i]) == bitsOf ((float) ((double) samples[i] / 8388608.0));

        record (std::string ("parser accepts ") + name, (int) spec.sampleRate, exact, r.ok ? "decoded exactly" : r.error);
    };

    auto expectReject = [&] (const char* name, const std::vector<unsigned char>& wav, std::uint32_t rate, const char* mustContain)
    {
        const auto r = blds::decodeBldsWav (wav.data(), wav.size(), rate);
        const bool pass = ! r.ok && r.error.find (mustContain) != std::string::npos;
        record (std::string ("parser rejects ") + name, (int) rate, pass, r.ok ? "accepted!" : "\"" + r.error + "\"");
    };

    WavSpec base;
    expectOk ("plain PCM 24/mono", base);
    { WavSpec s = base; s.extensible = true; expectOk ("WAVE_FORMAT_EXTENSIBLE", s); }
    { WavSpec s = base; s.oddChunkBeforeData = true; expectOk ("odd chunk + pad byte", s); }

    { WavSpec s = base; s.channels = 2;     expectReject ("stereo", makeWav (s, samples), 48000, "expected mono"); }
    { WavSpec s = base; s.bits = 16;        expectReject ("16-bit", makeWav (s, samples), 48000, "expected 24-bit"); }
    { WavSpec s = base; s.formatTag = 3; s.bits = 32; expectReject ("float", makeWav (s, samples), 48000, "float"); }
    { WavSpec s = base; s.sampleRate = 44100; expectReject ("rate mismatch", makeWav (s, samples), 48000, "header says 44100 Hz"); }
    expectReject ("zero length", makeWav (base, {}), 48000, "no audio samples");

    {
        auto wav = makeWav (base, samples);
        wav.resize (wav.size() - 10);
        expectReject ("truncated file", wav, 48000, "truncated");
    }
    {
        auto wav = makeWav (base, samples);
        std::memcpy (wav.data(), "RIFX", 4);
        expectReject ("non-RIFF", wav, 48000, "not a RIFF");
    }
    {
        // Empty file ends with the data chunk header; give it 4 bytes (not a multiple of 3).
        auto wav = makeWav (base, {});
        wav[wav.size() - 4] = 4;
        wav.insert (wav.end(), { 1, 2, 3, 4 });
        const auto riff = (std::uint32_t) wav.size() - 8;
        for (int b = 0; b < 4; ++b)
            wav[(std::size_t) (4 + b)] = (unsigned char) (riff >> (8 * b));
        expectReject ("partial sample", wav, 48000, "whole number");
    }
}

void testProcessorBasics (const Factory& make)
{
    auto p = make();
    record ("latency is 0", 0, p->getLatencySamples() == 0, std::to_string (p->getLatencySamples()));
    record ("tail is infinite", 0, std::isinf (p->getTailLengthSeconds()) && p->getTailLengthSeconds() > 0, "");
    record ("accepts no MIDI", 0, ! p->acceptsMidi() && ! p->producesMidi(), "");
    record ("default Output Channel = Right only", 0, p->getOutputChannel() == Processor::OutputChannel::rightOnly, "");

    const int ins = p->getTotalNumInputChannels(), outs = p->getTotalNumOutputChannels();
    record (BLDS_EFFECT ? "bus layout 2 in / 2 out" : "bus layout 0 in / 2 out", 0,
            outs == 2 && ins == (BLDS_EFFECT ? 2 : 0), std::to_string (ins) + " in / " + std::to_string (outs) + " out");

    setOutputChannel (*p, Processor::OutputChannel::leftOnly);
    juce::MemoryBlock state;
    p->getStateInformation (state);

    auto q = make();
    q->setStateInformation (state.getData(), (int) state.getSize());
    record ("Output Channel saved/restored in state", 0, q->getOutputChannel() == Processor::OutputChannel::leftOnly, "");
}

// Full sweep for one rate: random blocks + fixed block sizes, 'seconds' each.
void sweepRate (const std::string& label, const Factory& make, int rate, const Bits& expected, std::uint64_t seconds)
{
    const std::vector<int> fixedSizes { 32, 64, 128, 256, 512, 1024, 2048 };

    {
        auto p = make();
        std::string detail;
        const bool ok = renderRun (*p, rate, &expected, seconds, 0, 0xB1D5u + (std::uint32_t) rate, detail);
        record (label + " random blocks 1-4096", rate, ok, detail);
    }

    for (int bs : fixedSizes)
    {
        auto p = make();
        std::string detail;
        const bool ok = renderRun (*p, rate, &expected, seconds, bs, 1, detail);
        record (label + " block " + std::to_string (bs), rate, ok, detail);
    }

    {
        // Size-1 blocks (1 sample per processBlock call).
        auto p = make();
        std::string detail;
        const bool ok = renderRun (*p, rate, &expected, std::min<std::uint64_t> (seconds, 60), 1, 1, detail);
        record (label + " block 1", rate, ok, detail);
    }

    for (auto mode : { Processor::OutputChannel::leftOnly, Processor::OutputChannel::both })
    {
        auto p = make();
        setOutputChannel (*p, mode);
        std::string detail;
        const bool ok = renderRun (*p, rate, &expected, std::min<std::uint64_t> (seconds, 30), 0, 99, detail);
        record (label + (mode == Processor::OutputChannel::leftOnly ? " Left only" : " Both"), rate, ok, detail);
    }

    {
        // Bypass: exact silence, and the loop keeps running underneath.
        auto p = make();
        Driver d (*p);
        d.prepare ((double) rate, maxBlock);
        d.setExpectation (&expected, 0);
        std::mt19937 rng (5);
        std::uniform_int_distribution<int> dist (1, maxBlock);
        std::string error;
        bool ok = true;

        for (int i = 0; ok && i < 400; ++i)
            ok = d.block (dist (rng), (i / 7) % 2 == 1, error);

        record (label + " bypass = silence, free-running", rate, ok, ok ? "silent while bypassed, resumes in phase" : error);
    }
}

// Several sample-rate changes on one processor instance, mid-run.
void testRateChanges (const std::string& label, const Factory& make, const std::vector<int>& validRates,
                      const std::vector<Bits>& expected, const blds::Library& lib)
{
    auto p = make();
    Driver d (*p);
    std::mt19937 rng (0x5EED);
    std::uniform_int_distribution<int> dist (1, maxBlock);
    std::string error;

    // Sequence: every rate, an unsupported rate, back again, a fractional host rate,
    // and a same-rate re-prepare (must continue, not restart).
    std::vector<double> seq;
    for (int r : blds::supportedRates) seq.push_back (r);
    seq.push_back (22050.0);
    for (auto it = blds::supportedRates.rbegin(); it != blds::supportedRates.rend(); ++it) seq.push_back (*it);
    seq.push_back (47999.6);
    seq.push_back (48000.0);
    seq.push_back (96000.0);

    const blds::Table* currentTable = nullptr;
    bool ok = true;
    int changes = 0;

    for (std::size_t i = 0; ok && i < seq.size(); ++i)
    {
        const int rate = (int) std::lround (seq[i]);
        const bool releaseFirst = (i % 2) == 0;

        if (releaseFirst)
            p->releaseResources();

        d.prepare (seq[i], dist (rng));

        const auto* table = lib.validTableForRate (rate);
        const Bits* bits = nullptr;

        for (std::size_t k = 0; k < validRates.size(); ++k)
            if (validRates[k] == rate)
                bits = &expected[k];

        const std::uint64_t start = (table == currentTable && table != nullptr) ? d.getExpectedPos() : 0;
        d.setExpectation (bits, start);
        currentTable = table;
        ++changes;

        const auto wantStatus = table != nullptr ? Processor::Status::ok
                              : (lib.tableForRate (rate) != nullptr && lib.tableForRate (rate)->state == blds::Table::State::invalid)
                                    ? Processor::Status::invalidFile : Processor::Status::noFile;

        if (p->getStatus() != wantStatus || p->getHostRate() != rate)
        {
            error = "status/rate flags wrong after switching to " + std::to_string (rate) + " Hz";
            ok = false;
            break;
        }

        const std::uint64_t segment = (std::uint64_t) rate * (1 + (i % 3));

        for (std::uint64_t done = 0; ok && done < segment;)
        {
            const int n = (int) std::min<std::uint64_t> ((std::uint64_t) dist (rng), segment - done);
            ok = d.block (n, false, error);

            if (! ok)
                error = "after switch to " + std::to_string (rate) + " Hz: " + error;

            done += (std::uint64_t) n;
        }
    }

    record (label + " sample-rate changes mid-run", 0, ok,
            ok ? std::to_string (changes) + " switches incl. unsupported 22050 Hz, all bit-exact" : error);
}

//==============================================================================
std::uint64_t parseSeconds (int argc, char** argv)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp (argv[i], "--seconds") == 0)
            return (std::uint64_t) std::max (1, std::atoi (argv[i + 1]));

    return 600;
}

int runAll (std::uint64_t seconds)
{
    std::printf ("BLDS Player verification (%s variant), %llu s per run\n\n",
                 BLDS_EFFECT ? "effect" : "instrument", (unsigned long long) seconds);

    std::printf ("== Unit checks\n");
    testHarnessSelfChecks();
    testConversion();
    testParser();

    //--------------------------------------------------------------------------
    std::printf ("\n== Embedded official BLDS files (real plugin factory)\n");
    {
        Factory make = [] { return std::unique_ptr<Processor> (dynamic_cast<Processor*> (createPluginFilter())); };
        testProcessorBasics (make);

        auto probe = make();
        const auto& lib = probe->getLibrary();
        std::vector<int> validRates;
        std::vector<Bits> expected;

        for (int i = 0; i < blds::numRates; ++i)
        {
            const auto& t = lib.slot (i);
            const auto* embedded = blds::embeddedFileForRate (t.rate);

            if (embedded == nullptr)
            {
                record (t.fileName + " embedded", t.rate, true, "not present (rate will report 'No BLDS file')");

                std::string detail;
                const bool ok = renderRun (*make(), t.rate, nullptr, 10, 0, 3, detail)
                                && probe->getLibrary().validTableForRate (t.rate) == nullptr;
                record (t.fileName + " missing -> exact silence", t.rate, ok, detail);
                continue;
            }

            {
                // The bytes compiled into the binary must equal the file on disk.
                std::ifstream in (std::string (BLDS_FILES_DIR) + "/" + t.fileName, std::ios::binary);
                const std::vector<unsigned char> disk ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
                const bool same = ! disk.empty() && disk.size() == embedded->size()
                                  && std::equal (disk.begin(), disk.end(), embedded->begin);
                record (t.fileName + " embedded bytes == file on disk", t.rate, same,
                        std::to_string (embedded->size()) + " bytes" + (same ? "" : " - MISMATCH (or file missing on disk)"));
            }

            if (! t.isValid())
            {
                record (t.fileName + " valid", t.rate, false, "INVALID: " + t.reason);
                continue;
            }

            Bits ref;
            const bool decoded = referenceDecode (embedded->begin, embedded->size(), ref);
            const bool same = decoded && ref == tableBits (t);
            record (t.fileName + " valid, matches reference decode", t.rate, same,
                    std::to_string (t.length) + " samples" + (same ? "" : " - MISMATCH vs independent decoder"));

            if (same)
            {
                validRates.push_back (t.rate);
                expected.push_back (std::move (ref));
            }
        }

        for (std::size_t k = 0; k < validRates.size(); ++k)
            sweepRate ("embedded", make, validRates[k], expected[k], seconds);

        if (validRates.empty())
            std::printf ("  WARNING: no valid official BLDS files are embedded. Place them in blds_files/ and rebuild.\n");
        else
            testRateChanges ("embedded", make, validRates, expected, lib);
    }

    //--------------------------------------------------------------------------
    std::printf ("\n== Synthetic random 24-bit fixtures, all six rates (engine check, not BLDS)\n");
    {
        std::vector<std::vector<unsigned char>> wavs;
        std::vector<int> validRates;
        std::vector<Bits> expected;

        for (int i = 0; i < blds::numRates; ++i)
        {
            const int rate = blds::supportedRates[(std::size_t) i];
            // Odd, non-power-of-two lengths so every block size straddles the loop point.
            const auto samples = randomInt24 ((std::size_t) (rate / 10 + 17 + 2 * i), 1000u + (std::uint32_t) i);
            WavSpec spec;
            spec.sampleRate = (std::uint32_t) rate;
            wavs.push_back (makeWav (spec, samples));

            Bits ref;
            referenceDecode (wavs.back().data(), wavs.back().size(), ref);
            validRates.push_back (rate);
            expected.push_back (std::move (ref));
        }

        // Make one slot invalid in a second library to check error reporting.
        Factory make = [&wavs]
        {
            std::vector<blds::Library::Blob> blobs;
            for (int i = 0; i < blds::numRates; ++i)
                blobs.push_back ({ blds::supportedRates[(std::size_t) i], wavs[(std::size_t) i].data(), wavs[(std::size_t) i].size() });
            return std::make_unique<Processor> (blds::Library::fromBlobs (blobs));
        };

        for (std::size_t k = 0; k < validRates.size(); ++k)
            sweepRate ("synthetic", make, validRates[k], expected[k], seconds);

        auto probe = make();
        testRateChanges ("synthetic", make, validRates, expected, probe->getLibrary());

        {
            // Invalid + missing slots: exact silence and the right status.
            WavSpec bad;
            bad.sampleRate = 44100;   // header rate disagrees with the 96000 slot
            const auto badWav = makeWav (bad, randomInt24 (100, 3));
            std::vector<blds::Library::Blob> blobs { { 96000, badWav.data(), badWav.size() } };
            Processor p (blds::Library::fromBlobs (blobs));

            std::string d1, d2;
            const bool silent96 = renderRun (p, 96000, nullptr, 5, 0, 4, d1);
            const bool st96 = p.getStatus() == Processor::Status::invalidFile;
            record ("invalid file -> silence + 'Invalid file' status", 96000, silent96 && st96,
                    "reason: " + p.getLibrary().tableForRate (96000)->reason);

            const bool silent48 = renderRun (p, 48000, nullptr, 5, 0, 4, d2);
            record ("missing file -> silence + 'No BLDS file' status", 48000,
                    silent48 && p.getStatus() == Processor::Status::noFile, d2);
        }
    }

    //--------------------------------------------------------------------------
    int failures = 0;

    std::printf ("\n================================ SUMMARY ================================\n");
    std::printf ("%-6s %-52s %7s\n", "RESULT", "TEST", "RATE");

    for (const auto& r : rows)
    {
        std::printf ("%-6s %-52s %7s\n", r.pass ? "PASS" : "FAIL", r.test.c_str(), r.rate.c_str());
        failures += r.pass ? 0 : 1;
    }

    std::printf ("=========================================================================\n");
    std::printf ("%s: %d of %d checks passed, %lld allocation(s) inside processBlock\n",
                 failures == 0 ? "PASS" : "FAIL", (int) rows.size() - failures, (int) rows.size(),
                 guardViolations.load());

    return failures == 0 && guardViolations.load() == 0 ? 0 : 1;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    return runAll (parseSeconds (argc, argv));
}
