#include "Convolver.h"
#include "DspCommon.h"
#include <thread>
#include <chrono>
#include <cassert>

namespace fv::dsp
{
// ===========================================================================
// Stage: one uniform-partitioned segment of the IR (overlap-save, FDL)
// ===========================================================================
struct Convolver::Stage
{
    int P = 64;          // partition (block) size
    int K = 1;           // number of partitions
    int irOffset = 0;    // first IR sample covered
    int outOffset = 0;   // output delay of this stage's first contribution (irOffset + latency)
    int nSlots = 2;
    bool async = false;
    std::unique_ptr<RealFFT> fft;

    // IR spectra, contiguous: [path][partition][2P]
    AlignedBuffer irSpec;
    // input ring per input channel (4P samples, block aligned)
    std::array<AlignedBuffer, 2> ring;
    // frequency-domain delay line per input channel: K spectra of 2P
    std::array<AlignedBuffer, 2> fdl;
    // output slots per output channel: nSlots blocks of P
    std::array<AlignedBuffer, 2> slots;
    // compute scratch (owned by whoever holds the claim for the current block)
    AlignedBuffer window, work, timeOut;
    std::array<AlignedBuffer, 2> acc;

    std::atomic<int64_t> posted { -1 }, claimed { -1 }, completed { -1 };

    const double* spec (int path, int k) const noexcept { return irSpec.data() + ((size_t) path * (size_t) K + (size_t) k) * (size_t) (2 * P); }
};

namespace
{
    struct StagePlan { int P, irOffset, irEnd; };

    std::vector<StagePlan> planZeroLatency (int len)
    {
        // head [0,64) is direct form; then partitions grow 64 -> 512 -> 4096 -> 32768.
        // Each stage starts at >= 2P (except the first, at P), which leaves one full
        // block period of slack for the background thread.
        const int sizes[]  = { 64, 512, 4096, 32768 };
        const int starts[] = { 64, 1024, 8192, 65536 };
        std::vector<StagePlan> plan;
        for (int i = 0; i < 4; ++i)
        {
            if (starts[i] >= len) break;
            const int end = (i < 3) ? std::min (len, starts[i + 1]) : len;
            plan.push_back ({ sizes[i], starts[i], end });
        }
        return plan;
    }
}

// ===========================================================================
// Convolver
// ===========================================================================
Convolver::Convolver (const IRData& ir, const Config& cfg, ConvolutionWorker* w)
    : quality (cfg.quality), worker (w)
{
    irLength = std::max (1, ir.length());
    for (int p = 0; p < 4; ++p) active[(size_t) p] = ir.active[(size_t) p] && ! ir.path[(size_t) p].empty();

    auto irSample = [&] (int p, int i) -> double
    {
        const auto& v = ir.path[(size_t) p];
        return (i >= 0 && i < (int) v.size()) ? v[(size_t) i] : 0.0;
    };

    std::vector<StagePlan> plan;
    if (quality == 0)
    {
        latency = 0;
        headLen = std::min (kHeadLength, irLength);
        plan = planZeroLatency (irLength);
        grid = 64;
    }
    else
    {
        latency = kEfficientPartition;
        headLen = 0;
        plan.push_back ({ kEfficientPartition, 0, irLength });
        grid = kEfficientPartition;
    }

    // head (direct form)
    if (headLen > 0)
    {
        for (int p = 0; p < 4; ++p)
        {
            auto& taps = headTapsRev[(size_t) p];
            taps.assign ((size_t) headLen, 0.0);
            for (int m = 0; m < headLen; ++m) taps[(size_t) (headLen - 1 - m)] = irSample (p, m);
        }
        for (auto& h : headHist) h.resize ((size_t) (2 * headLen));
    }

    // partitioned stages
    for (auto& sp : plan)
    {
        auto st = std::make_unique<Stage>();
        st->P = sp.P;
        st->K = std::max (1, (sp.irEnd - sp.irOffset + sp.P - 1) / sp.P);
        st->irOffset = sp.irOffset;
        st->outOffset = sp.irOffset + latency;
        st->nSlots = st->outOffset / st->P + 1;
        st->async = cfg.allowAsync && worker != nullptr && (st->outOffset - st->P) >= cfg.maxBlockSize;
        st->fft = std::make_unique<RealFFT> (2 * st->P);

        const int N = 2 * st->P;
        const double scale = st->fft->getConvolutionScale();
        st->irSpec.resize ((size_t) 4 * (size_t) st->K * (size_t) N);
        AlignedBuffer tmp ((size_t) N), w2 ((size_t) N);
        for (int p = 0; p < 4; ++p)
        {
            if (! active[(size_t) p]) continue;
            for (int k = 0; k < st->K; ++k)
            {
                tmp.clear();
                const int base = st->irOffset + k * st->P;
                const int end = std::min (sp.irEnd, base + st->P);
                for (int i = base; i < end; ++i) tmp[(size_t) (i - base)] = irSample (p, i) * scale;
                st->fft->forward (tmp.data(), const_cast<double*> (st->spec (p, k)), w2.data());
            }
        }
        for (auto& r : st->ring)  r.resize ((size_t) (4 * st->P));
        for (auto& f : st->fdl)   f.resize ((size_t) st->K * (size_t) N);
        for (auto& s : st->slots) s.resize ((size_t) st->nSlots * (size_t) st->P);
        st->window.resize ((size_t) N);
        st->work.resize ((size_t) N);
        st->timeOut.resize ((size_t) N);
        for (auto& a : st->acc) a.resize ((size_t) N);
        stages.push_back (std::move (st));
    }

    mixL.assign ((size_t) grid, 0.0);
    mixR.assign ((size_t) grid, 0.0);

    if (worker != nullptr)
        for (auto& s : stages)
            if (s->async) { worker->registerConvolver (this); break; }
}

Convolver::~Convolver()
{
    if (worker != nullptr)
        worker->unregisterConvolver (this);
}

bool Convolver::isStageAsync (int i) const noexcept { return stages[(size_t) i]->async; }

void Convolver::reset()
{
    // wait for any in-flight background work to finish
    for (auto& s : stages)
        while (s->completed.load (std::memory_order_acquire) < s->claimed.load (std::memory_order_acquire))
            std::this_thread::yield();
    if (worker != nullptr) worker->unregisterConvolver (this);

    for (auto& h : headHist) h.clear();
    headPos = 0;
    for (auto& s : stages)
    {
        for (auto& r : s->ring) r.clear();
        for (auto& f : s->fdl) f.clear();
        for (auto& sl : s->slots) sl.clear();
        s->posted.store (-1); s->claimed.store (-1); s->completed.store (-1);
    }
    pos = 0;

    if (worker != nullptr)
        for (auto& s : stages)
            if (s->async) { worker->registerConvolver (this); break; }
}

void Convolver::computeBlock (Stage& s, int64_t j) noexcept
{
    const int P = s.P, N = 2 * P;
    const int ringLen = 4 * P;
    const size_t specLen = (size_t) N;
    const int slotFdl = (int) (j % s.K);

    // 1) input windows -> FDL
    for (int ch = 0; ch < 2; ++ch)
    {
        const double* r = s.ring[(size_t) ch].data();
        int start = (int) (((j - 1) * P) % ringLen);
        if (start < 0) start += ringLen;
        // two blocks, possibly wrapping
        const int first = std::min (N, ringLen - start);
        std::memcpy (s.window.data(), r + start, sizeof (double) * (size_t) first);
        if (first < N) std::memcpy (s.window.data() + first, r, sizeof (double) * (size_t) (N - first));
        s.fft->forward (s.window.data(), s.fdl[(size_t) ch].data() + (size_t) slotFdl * specLen, s.work.data());
    }

    // 2) multiply-accumulate over all partitions
    // out L = inL*LL + inR*RL ; out R = inL*LR + inR*RR
    s.acc[0].clear();
    s.acc[1].clear();
    for (int k = 0; k < s.K; ++k)
    {
        int idx = slotFdl - k;
        if (idx < 0) idx += s.K;
        if (j - k < 0) break; // older blocks are silence
        const double* xL = s.fdl[0].data() + (size_t) idx * specLen;
        const double* xR = s.fdl[1].data() + (size_t) idx * specLen;
        if (active[IRData::LL]) s.fft->multiplyAccumulate (xL, s.spec (IRData::LL, k), s.acc[0].data());
        if (active[IRData::RL]) s.fft->multiplyAccumulate (xR, s.spec (IRData::RL, k), s.acc[0].data());
        if (active[IRData::LR]) s.fft->multiplyAccumulate (xL, s.spec (IRData::LR, k), s.acc[1].data());
        if (active[IRData::RR]) s.fft->multiplyAccumulate (xR, s.spec (IRData::RR, k), s.acc[1].data());
    }

    // 3) back to time domain; overlap-save keeps the second half
    const int slot = (int) (j % s.nSlots);
    for (int ch = 0; ch < 2; ++ch)
    {
        s.fft->inverse (s.acc[(size_t) ch].data(), s.timeOut.data(), s.work.data());
        std::memcpy (s.slots[(size_t) ch].data() + (size_t) slot * (size_t) P, s.timeOut.data() + P, sizeof (double) * (size_t) P);
    }
}

void Convolver::ensureComputed (Stage& s, int64_t j) noexcept
{
    if (s.completed.load (std::memory_order_acquire) >= j) return;
    // Help out: claim and compute any outstanding blocks up to j in order.
    for (;;)
    {
        int64_t c = s.claimed.load (std::memory_order_acquire);
        if (c >= j)
        {
            // the worker holds the claim for a block we need: it is mid-computation,
            // wait for it (bounded by one block's compute time).
            while (s.completed.load (std::memory_order_acquire) < j)
                std::this_thread::yield();
            return;
        }
        if (s.claimed.compare_exchange_strong (c, c + 1, std::memory_order_acq_rel))
        {
            while (s.completed.load (std::memory_order_acquire) < c)
                std::this_thread::yield();
            computeBlock (s, c + 1);
            s.completed.store (c + 1, std::memory_order_release);
            if (worker != nullptr) worker->lateTasks.fetch_add (1, std::memory_order_relaxed);
        }
    }
}

bool Convolver::workerStep() noexcept
{
    bool did = false;
    for (auto& sp : stages)
    {
        auto& s = *sp;
        if (! s.async) continue;
        int64_t c = s.claimed.load (std::memory_order_acquire);
        if (c >= s.posted.load (std::memory_order_acquire)) continue;
        if (! s.claimed.compare_exchange_strong (c, c + 1, std::memory_order_acq_rel)) continue;
        while (s.completed.load (std::memory_order_acquire) < c)
            std::this_thread::yield();
        computeBlock (s, c + 1);
        s.completed.store (c + 1, std::memory_order_release);
        did = true;
    }
    return did;
}

void Convolver::process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    int i = 0;
    while (i < n)
    {
        const int chunk = std::min (n - i, grid - (int) (pos % grid));

        // 1) write input into every stage's ring
        for (auto& sp : stages)
        {
            auto& s = *sp;
            const int ringLen = 4 * s.P;
            int w = (int) (pos % ringLen);
            double* rL = s.ring[0].data();
            double* rR = s.ring[1].data();
            for (int k = 0; k < chunk; ++k)
            {
                rL[w] = inL[i + k];
                rR[w] = inR[i + k];
                if (++w == ringLen) w = 0;
            }
        }

        // 2) head: direct-form FIR on the first taps
        double* yL = mixL.data();
        double* yR = mixR.data();
        if (headLen > 0)
        {
            double* hL = headHist[0].data();
            double* hR = headHist[1].data();
            const double* tLL = headTapsRev[IRData::LL].data();
            const double* tLR = headTapsRev[IRData::LR].data();
            const double* tRL = headTapsRev[IRData::RL].data();
            const double* tRR = headTapsRev[IRData::RR].data();
            for (int k = 0; k < chunk; ++k)
            {
                hL[headPos] = hL[headPos + headLen] = inL[i + k];
                hR[headPos] = hR[headPos + headLen] = inR[i + k];
                const double* xl = hL + headPos + 1;
                const double* xr = hR + headPos + 1;
                double aL = 0.0, aR = 0.0;
                if (active[IRData::LL]) for (int m = 0; m < headLen; ++m) aL += tLL[m] * xl[m];
                if (active[IRData::RL]) for (int m = 0; m < headLen; ++m) aL += tRL[m] * xr[m];
                if (active[IRData::LR]) for (int m = 0; m < headLen; ++m) aR += tLR[m] * xl[m];
                if (active[IRData::RR]) for (int m = 0; m < headLen; ++m) aR += tRR[m] * xr[m];
                yL[k] = aL;
                yR[k] = aR;
                if (++headPos == headLen) headPos = 0;
            }
        }
        else
        {
            std::fill (yL, yL + chunk, 0.0);
            std::fill (yR, yR + chunk, 0.0);
        }

        // 3) add stage outputs that are due now
        for (auto& sp : stages)
        {
            auto& s = *sp;
            if (pos + chunk <= s.outOffset) continue;
            const int64_t rel = pos - s.outOffset;   // chunks are grid-aligned, so never straddle a slot
            if (rel < 0) continue;
            const int64_t j = rel / s.P;
            const int off = (int) (rel % s.P);
            if (s.async) ensureComputed (s, j);
            const int slot = (int) (j % s.nSlots);
            const double* oL = s.slots[0].data() + (size_t) slot * (size_t) s.P + off;
            const double* oR = s.slots[1].data() + (size_t) slot * (size_t) s.P + off;
            for (int k = 0; k < chunk; ++k) { yL[k] += oL[k]; yR[k] += oR[k]; }
        }

        for (int k = 0; k < chunk; ++k)
        {
            outL[i + k] = (float) yL[k];
            outR[i + k] = (float) yR[k];
        }

        // 4) completed blocks: compute (sync) or post (async)
        pos += chunk;
        for (auto& sp : stages)
        {
            auto& s = *sp;
            if (pos % s.P != 0) continue;
            const int64_t j = pos / s.P - 1;
            if (s.async)
            {
                s.posted.store (j, std::memory_order_release);
                if (worker != nullptr) worker->notify();
            }
            else
            {
                s.claimed.store (j, std::memory_order_relaxed);
                computeBlock (s, j);
                s.completed.store (j, std::memory_order_relaxed);
            }
        }
        i += chunk;
    }
}

// ===========================================================================
// ConvolutionWorker
// ===========================================================================
struct ConvolutionWorker::Thread
{
    std::thread t;
};

ConvolutionWorker::ConvolutionWorker() : thread (std::make_unique<Thread>())
{
    for (auto& s : slots) s.store (nullptr);
    thread->t = std::thread ([this] { run(); });
}

ConvolutionWorker::~ConvolutionWorker()
{
    quit.store (true);
    notify();
    if (thread->t.joinable()) thread->t.join();
}

void ConvolutionWorker::registerConvolver (Convolver* c) noexcept
{
    for (auto& s : slots)
    {
        Convolver* expected = nullptr;
        if (s.compare_exchange_strong (expected, c)) return;
    }
    // no free slot: the convolver still works, the audio thread computes everything itself
}

void ConvolutionWorker::unregisterConvolver (Convolver* c) noexcept
{
    for (auto& s : slots)
    {
        Convolver* expected = c;
        s.compare_exchange_strong (expected, nullptr);
    }
    while (busyWith.load (std::memory_order_acquire) == c)
        std::this_thread::yield();
}

void ConvolutionWorker::run()
{
    ScopedNoDenormalsFV noDenormals;
    while (! quit.load (std::memory_order_relaxed))
    {
        // read the signal *before* scanning, so a post that lands during the scan wakes us
        const auto seen = signal.load (std::memory_order_acquire);
        bool did = false;
        for (auto& s : slots)
        {
            Convolver* c = s.load (std::memory_order_acquire);
            if (c == nullptr) continue;
            busyWith.store (c, std::memory_order_release);
            // re-check after publishing busyWith (unregister may have raced)
            if (s.load (std::memory_order_acquire) == c)
                did |= c->workerStep();
            busyWith.store (nullptr, std::memory_order_release);
        }
        if (! did && ! quit.load (std::memory_order_relaxed))
            signal.wait (seen, std::memory_order_acquire);
    }
}
} // namespace fv::dsp
