#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "FFT.h"

namespace fv::dsp
{
/** A prepared impulse response at the session sample rate: four paths
    (L->L, L->R, R->L, R->R). Inactive paths are skipped entirely. */
struct IRData
{
    enum Path { LL = 0, LR = 1, RL = 2, RR = 3 };
    std::array<std::vector<double>, 4> path;
    std::array<bool, 4> active { false, false, false, false };
    double sampleRate = 48000.0;

    int length() const noexcept
    {
        size_t n = 0;
        for (int p = 0; p < 4; ++p) if (active[(size_t) p]) n = std::max (n, path[(size_t) p].size());
        return (int) n;
    }
};

class Convolver;

/** Background thread that runs the large (tail) partitions of every live
    convolver. The audio thread only posts block indices (atomics, no locks,
    no allocation). If a task is not finished when its output is due, the audio
    thread computes it itself, so a late or starved worker never causes a
    dropout or a block. */
class ConvolutionWorker
{
public:
    ConvolutionWorker();
    ~ConvolutionWorker();

    void registerConvolver (Convolver*) noexcept;
    void unregisterConvolver (Convolver*) noexcept;   // waits until the worker has left it

    std::atomic<int> lateTasks { 0 };   // diagnostics: tasks the audio thread had to take over

private:
    void run();
    static constexpr int kMaxConvolvers = 16;
    std::array<std::atomic<Convolver*>, kMaxConvolvers> slots {};
    std::atomic<Convolver*> busyWith { nullptr };
    std::atomic<bool> quit { false };
    struct Thread;
    std::unique_ptr<Thread> thread;
    friend class Convolver;
};

/** Non-uniform partitioned convolution (zero latency) or uniform partitioned
    convolution (efficient, reports its latency). True stereo, double-precision
    accumulation. Construction is expensive (FFTs of the IR) and must happen off
    the audio thread; process() is real-time safe. */
class Convolver
{
public:
    struct Config
    {
        int  quality = 0;            // 0 = zero latency, 1 = efficient
        int  maxBlockSize = 512;     // host block size (decides which stages may run async)
        bool allowAsync = true;      // false: every stage runs on the calling thread (offline/tests)
    };

    static constexpr int kHeadLength = 64;
    static constexpr int kEfficientPartition = 1024;

    Convolver (const IRData& ir, const Config& cfg, ConvolutionWorker* worker);
    ~Convolver();
    Convolver (const Convolver&) = delete;
    Convolver& operator= (const Convolver&) = delete;

    /** Latency in samples (0 in zero-latency mode). */
    int getLatency() const noexcept { return latency; }
    int getIRLength() const noexcept { return irLength; }
    int getQuality() const noexcept { return quality; }

    /** in/out: n samples per channel. Output is written (not accumulated). */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;

    /** Clears all history. Not real-time safe (waits for in-flight work). */
    void reset();

    /** Number of partition stages (for diagnostics/tests). */
    int getNumStages() const noexcept { return (int) stages.size(); }
    bool isStageAsync (int i) const noexcept;

private:
    struct Stage;
    friend class ConvolutionWorker;
    bool workerStep() noexcept;                  // called by the worker: returns true if it did work
    void computeBlock (Stage&, int64_t block) noexcept;
    void ensureComputed (Stage&, int64_t block) noexcept;

    int quality = 0, latency = 0, irLength = 0, grid = 64;
    int headLen = 0;
    std::array<bool, 4> active {};
    std::array<std::vector<double>, 4> headTapsRev;   // reversed head taps
    std::array<AlignedBuffer, 2> headHist;            // mirrored history, 2*headLen
    int headPos = 0;

    std::vector<std::unique_ptr<Stage>> stages;
    int64_t pos = 0;
    ConvolutionWorker* worker = nullptr;
    std::vector<double> mixL, mixR;                  // per-chunk double accumulators
};
} // namespace fv::dsp
