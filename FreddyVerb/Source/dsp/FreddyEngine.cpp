#include "FreddyEngine.h"
#include "../state/Parameters.h"

namespace fv::dsp
{
namespace
{
    constexpr int kSubBlock = 256;
    constexpr int kEfficientPartition = Convolver::kEfficientPartition;
    constexpr double kMaxPredelayMs = 250.0;
}

struct FreddyEngine::Impl
{
    LatencyDelay dryDelay, keyDelay, wetInDelay, predelay;
    std::vector<float> scratch[12];
    float* x[2] {};       // input (stereo)
    float* dry[2] {};     // latency-compensated dry
    float* key[2] {};     // detector key (aligned with the wet output)
    float* wet[2] {};     // wet path
    float* convA[2] {};   // current convolver output
    float* convB[2] {};   // incoming convolver output (during IR crossfade)

    std::unique_ptr<Convolver> current, next;
    Convolver* pendingRetire[4] {};
    int fadePos = 0, fadeLen = 1;

    LinearSmoother dryGain, wetGain, outGain, bypassMix, soloMix, listenMix;

    bool snap = true; // first block after prepare/reset: jump smoothers and delays to target

    // viz decimation accumulators
    int vizCount = 0;
    VizFrame vizAcc;
};

FreddyEngine::FreddyEngine() : impl (std::make_unique<Impl>())
{
    for (auto& r : retired) r.store (nullptr);
}

FreddyEngine::~FreddyEngine()
{
    impl->current.reset();
    impl->next.reset();
    for (auto* p : impl->pendingRetire) delete p;
    delete incoming.exchange (nullptr);
    for (auto& r : retired) delete r.exchange (nullptr);
}

int FreddyEngine::getConvolutionLatency (int quality) const noexcept
{
    return quality == choices::efficient ? kEfficientPartition : 0;
}

int FreddyEngine::computeLatency (const EngineParams& p) const noexcept
{
    return getConvolutionLatency (p.quality) + msToSamples (p.lookaheadMs, sampleRate);
}

void FreddyEngine::prepare (double sr, int maxBlockSize, int /*numInputChannels*/)
{
    sampleRate = sr;
    maxBlock = std::max (1, maxBlockSize);
    auto& d = *impl;

    const int maxLookahead = msToSamples (10.0, sr) + 1;
    const int maxLatency = kEfficientPartition + maxLookahead;
    d.dryDelay.prepare (2, maxLatency, sr);
    d.keyDelay.prepare (2, kEfficientPartition, sr);
    d.wetInDelay.prepare (2, maxLookahead, sr);
    d.predelay.prepare (2, msToSamples (kMaxPredelayMs, sr) + 1, sr, 30.0);

    for (auto& s : d.scratch) s.assign ((size_t) kSubBlock, 0.0f);
    float** ptrs[] = { d.x, d.dry, d.key, d.wet, d.convA, d.convB };
    for (int i = 0; i < 6; ++i) { ptrs[i][0] = d.scratch[2 * i].data(); ptrs[i][1] = d.scratch[2 * i + 1].data(); }

    for (auto* s : { &d.dryGain, &d.wetGain, &d.outGain, &d.bypassMix, &d.soloMix, &d.listenMix })
        s->reset (sr, 20.0);
    d.fadeLen = std::max (1, msToSamples (kIRCrossfadeMs, sr));

    viz.frameRate.store (sr / kVizDecimation);
    rmsCoef = (float) onePoleCoef (300.0, sr);
    for (auto* m : { &inMeter, &wetMeter, &outMeter })
        m->peakDecay = (float) std::exp (-1.0 / (0.25 * sr / kSubBlock)); // applied per sub-block
    reset();
}

void FreddyEngine::reset()
{
    auto& d = *impl;
    d.dryDelay.reset(); d.keyDelay.reset(); d.wetInDelay.reset(); d.predelay.reset();
    if (d.current) d.current->reset();
    if (d.next) { d.current = std::move (d.next); d.current->reset(); }
    inMeter = wetMeter = outMeter = {};
    d.vizCount = 0; d.vizAcc = {};
    d.snap = true;
}

// ---------------------------------------------------------------------------
// convolver hand-off
// ---------------------------------------------------------------------------
std::unique_ptr<Convolver> FreddyEngine::offerConvolver (std::unique_ptr<Convolver> c) noexcept
{
    return std::unique_ptr<Convolver> (incoming.exchange (c.release(), std::memory_order_acq_rel));
}

std::unique_ptr<Convolver> FreddyEngine::takeRetiredConvolver() noexcept
{
    for (auto& r : retired)
        if (auto* c = r.exchange (nullptr, std::memory_order_acq_rel))
            return std::unique_ptr<Convolver> (c);
    return {};
}

void FreddyEngine::installConvolverNow (std::unique_ptr<Convolver> c)
{
    auto& d = *impl;
    d.next.reset();
    d.current = std::move (c);
    if (d.current) d.current->reset();
}

bool FreddyEngine::isCrossfading() const noexcept { return impl->next != nullptr; }

// ---------------------------------------------------------------------------
// process
// ---------------------------------------------------------------------------
void FreddyEngine::process (const float* const* in, int numIn, float* const* out, int numSamples, const EngineParams& p) noexcept
{
    ScopedNoDenormalsFV noDenormals;
    auto& d = *impl;

    // retire convolvers that could not be handed back last time
    for (auto*& pr : d.pendingRetire)
        if (pr != nullptr)
            for (auto& r : retired)
            {
                Convolver* expected = nullptr;
                if (r.compare_exchange_strong (expected, pr)) { pr = nullptr; break; }
            }
    auto retire = [&] (std::unique_ptr<Convolver> c)
    {
        Convolver* raw = c.release();
        for (auto& r : retired)
        {
            Convolver* expected = nullptr;
            if (r.compare_exchange_strong (expected, raw)) return;
        }
        for (auto*& pr : d.pendingRetire) if (pr == nullptr) { pr = raw; return; }
        // should never happen (bounded number of swaps in flight); keep running rather than delete here
        d.pendingRetire[0] = raw;
    };

    // accept a new convolver (only when no crossfade is running)
    if (d.next == nullptr)
        if (auto* c = incoming.exchange (nullptr, std::memory_order_acq_rel))
        {
            if (d.current == nullptr) d.current.reset (c);
            else { d.next.reset (c); d.fadePos = 0; }
        }

    const int convLatency = getConvolutionLatency (p.quality);
    const int lookahead   = msToSamples (p.lookaheadMs, sampleRate);
    d.dryDelay.setDelay (convLatency + lookahead, d.snap);
    d.keyDelay.setDelay (convLatency, d.snap);
    d.wetInDelay.setDelay (lookahead, d.snap);
    d.predelay.setDelay (msToSamples (std::clamp ((double) p.predelayMs, 0.0, kMaxPredelayMs), sampleRate), d.snap);

    // Equal-power mix law applied on top of the independent dry/wet faders.
    const float mixAngle = (float) (std::clamp (p.mixPct, 0.0f, 100.0f) * 0.01 * kPi * 0.5);
    const float dryLin = p.dryDb <= kLevelInfDb ? 0.0f : dbToGain (p.dryDb);
    const float wetLin = p.wetDb <= kLevelInfDb ? 0.0f : dbToGain (p.wetDb);
    d.dryGain.setTarget (std::cos (mixAngle) * dryLin);
    d.wetGain.setTarget (std::sin (mixAngle) * wetLin);
    d.outGain.setTarget (dbToGain (p.outDb));
    d.bypassMix.setTarget (p.bypass ? 1.0f : 0.0f);
    d.soloMix.setTarget (p.wetSolo ? 1.0f : 0.0f);
    d.listenMix.setTarget (p.keyListen ? 1.0f : 0.0f);
    if (d.snap)
    {
        for (auto* sm : { &d.dryGain, &d.wetGain, &d.outGain, &d.bypassMix, &d.soloMix, &d.listenMix })
            sm->setCurrentAndTarget (sm->target);
        d.snap = false;
    }

    for (int start = 0; start < numSamples; start += kSubBlock)
    {
        const int n = std::min (kSubBlock, numSamples - start);
        const size_t bytes = sizeof (float) * (size_t) n;

        // 1) gather input as stereo
        for (int c = 0; c < 2; ++c)
            std::memcpy (d.x[c], in[std::min (c, numIn - 1)] + start, bytes);

        // 2) latency-compensated branches
        for (int c = 0; c < 2; ++c)
        {
            std::memcpy (d.dry[c], d.x[c], bytes);
            std::memcpy (d.key[c], d.x[c], bytes);
            std::memcpy (d.wet[c], d.x[c], bytes);
        }
        d.dryDelay.process (d.dry, 2, n);
        d.keyDelay.process (d.key, 2, n);
        d.wetInDelay.process (d.wet, 2, n);

        // 3) wet path: [EQ pre] -> pre-delay -> convolution (with IR crossfade)
        d.predelay.process (d.wet, 2, n);
        if (d.current != nullptr)
        {
            d.current->process (d.wet[0], d.wet[1], d.convA[0], d.convA[1], n);
            if (d.next != nullptr)
            {
                d.next->process (d.wet[0], d.wet[1], d.convB[0], d.convB[1], n);
                for (int i = 0; i < n; ++i)
                {
                    const float x = std::min (1.0f, (float) d.fadePos / (float) d.fadeLen);
                    const float gOld = std::cos (x * (float) kPi * 0.5f), gNew = std::sin (x * (float) kPi * 0.5f);
                    for (int c = 0; c < 2; ++c) d.wet[c][i] = d.convA[c][i] * gOld + d.convB[c][i] * gNew;
                    ++d.fadePos;
                }
                if (d.fadePos >= d.fadeLen)
                {
                    retire (std::move (d.current));
                    d.current = std::move (d.next);
                }
            }
            else
                for (int c = 0; c < 2; ++c) std::memcpy (d.wet[c], d.convA[c], bytes);
        }
        else
            for (int c = 0; c < 2; ++c) std::fill (d.wet[c], d.wet[c] + n, 0.0f);

        // 4) metering + visualizer feed (wet is post duck/gate here)
        accumulateMeters (d.x, d.wet, n);
        for (int i = 0; i < n; ++i)
        {
            auto& v = d.vizAcc;
            v.dryPeak = std::max (v.dryPeak, std::max (std::abs (d.dry[0][i]), std::abs (d.dry[1][i])));
            v.wetPeak = std::max (v.wetPeak, std::max (std::abs (d.wet[0][i]), std::abs (d.wet[1][i])));
            const float k = std::max (std::abs (d.key[0][i]), std::abs (d.key[1][i]));
            v.keyDb = std::max (v.keyDb, gainToDb (k, -120.0f));
            v.duckGain = 1.0f;
            v.gateGain = 1.0f;
            if (++d.vizCount >= kVizDecimation)
            {
                viz.push (v);
                d.vizAcc = {};
                d.vizCount = 0;
            }
        }

        // 5) mix + output
        for (int i = 0; i < n; ++i)
        {
            const float dg = d.dryGain.next(), wg = d.wetGain.next(), og = d.outGain.next();
            const float bp = d.bypassMix.next(), solo = d.soloMix.next(), listen = d.listenMix.next();
            const float wetG = wg * (1.0f - solo) + wetLin * solo;
            for (int c = 0; c < 2; ++c)
            {
                const float dr = d.dry[c][i];
                float y = dr * dg * (1.0f - solo) + d.wet[c][i] * wetG;
                y = y * (1.0f - listen) + d.key[c][i] * listen;
                y *= og;
                y = y * (1.0f - bp) + dr * bp; // bypass: latency-compensated dry
                out[c][start + i] = y;
            }
        }
        const float* o[2] { out[0] + start, out[1] + start };
        accumulateOutputMeter (o, n);
    }
}

namespace
{
    void accumulate (FreddyEngine::MeterState& m, const float* const* x, int n, float rmsCoef) noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            float pk = 0.0f, ms = m.ms[c];
            for (int i = 0; i < n; ++i)
            {
                const float v = x[c][i];
                pk = std::max (pk, std::abs (v));
                ms += rmsCoef * (v * v - ms);
            }
            m.ms[c] = flushDenormal (ms);
            m.peak[c] = std::max (m.peak[c] * m.peakDecay, pk);
        }
    }
    void publish (const FreddyEngine::MeterState& m, MeterValues& out) noexcept
    {
        out.peakL.store (m.peak[0], std::memory_order_relaxed);
        out.peakR.store (m.peak[1], std::memory_order_relaxed);
        out.rmsL.store (std::sqrt (m.ms[0]), std::memory_order_relaxed);
        out.rmsR.store (std::sqrt (m.ms[1]), std::memory_order_relaxed);
    }
}

void FreddyEngine::accumulateMeters (const float* const* input, const float* const* wet, int n) noexcept
{
    accumulate (inMeter, input, n, rmsCoef);
    accumulate (wetMeter, wet, n, rmsCoef);
    publish (inMeter, meters.input);
    publish (wetMeter, meters.wet);
    // wet correlation (EMA of products, ~300 ms)
    for (int i = 0; i < n; ++i)
    {
        const double l = wet[0][i], r = wet[1][i];
        corrXY += rmsCoef * (l * r - corrXY);
        corrXX += rmsCoef * (l * l - corrXX);
        corrYY += rmsCoef * (r * r - corrYY);
    }
    corrXY = flushDenormal (corrXY); corrXX = flushDenormal (corrXX); corrYY = flushDenormal (corrYY);
    const double den = std::sqrt (corrXX * corrYY);
    meters.correlation.store (den > 1.0e-12 ? (float) (corrXY / den) : 1.0f, std::memory_order_relaxed);
}

void FreddyEngine::accumulateOutputMeter (const float* const* out, int n) noexcept
{
    accumulate (outMeter, out, n, rmsCoef);
    publish (outMeter, meters.output);
}
} // namespace fv::dsp
