#pragma once

#include <vector>
#include <memory>
#include <array>
#include <atomic>
#include "EngineParams.h"
#include "LatencyDelay.h"
#include "VizFeed.h"
#include "Convolver.h"

namespace fv::dsp
{
/** The complete Freddy Verb signal graph (audio thread side):

    in ─┬─ delay(lookahead) ─► [EQ pre] ─► pre-delay ─► Convolver ─► Duck ─► Gate ─► [EQ post] ─► Width ─┐
        ├─ delay(convLatency) ─► Detector (key) ─────────────────────────┘ (drives duck + gate)         ├─► Mix ─► Out
        └─ delay(convLatency + lookahead) ─► DRY ─────────────────────────────────────────────────────┘

    All allocation happens in prepare(). process() is real-time safe. */
class FreddyEngine
{
public:
    FreddyEngine();
    ~FreddyEngine();

    void prepare (double sampleRate, int maxBlockSize, int numInputChannels);
    void reset();

    /** in: numIn (1 or 2) channels; out: 2 channels. in and out may alias. */
    void process (const float* const* in, int numIn, float* const* out, int numSamples, const EngineParams& p) noexcept;

    /** Latency in samples for the given settings (convolution + lookahead). */
    int computeLatency (const EngineParams& p) const noexcept;
    int getConvolutionLatency (int quality) const noexcept;

    VizFeed& getVizFeed() noexcept   { return viz; }
    Meters&  getMeters() noexcept    { return meters; }
    double   getSampleRate() const noexcept { return sampleRate; }

    /** Host tempo for synced parameters (written by processor before process()). */
    double bpm = 120.0;

    // --- convolver hand-off (IR manager thread <-> audio thread, lock-free) ---------
    /** Offers a freshly built convolver. If one is already waiting it is replaced
        and the replaced one is returned (never reached the audio thread). */
    std::unique_ptr<Convolver> offerConvolver (std::unique_ptr<Convolver> c) noexcept;
    /** Returns a convolver the audio thread has finished with (delete it off-thread). */
    std::unique_ptr<Convolver> takeRetiredConvolver() noexcept;
    /** Installs a convolver immediately (not real-time safe; audio must be stopped). */
    void installConvolverNow (std::unique_ptr<Convolver> c);
    ConvolutionWorker& getWorker() noexcept { return worker; }
    /** True while an IR crossfade is running (diagnostics/tests). */
    bool isCrossfading() const noexcept;
    static constexpr double kIRCrossfadeMs = 50.0;

    struct Impl;
    struct MeterState { float peak[2] {}, ms[2] {}; float peakDecay = 0.99f; };

private:
    void accumulateMeters (const float* const* input, const float* const* wet, int n) noexcept;
    void accumulateOutputMeter (const float* const* out, int n) noexcept;
    MeterState inMeter, wetMeter, outMeter;
    float rmsCoef = 0.001f;
    double corrXY = 0, corrXX = 0, corrYY = 0;

    ConvolutionWorker worker;
    std::atomic<Convolver*> incoming { nullptr };
    std::array<std::atomic<Convolver*>, 8> retired {};
    std::unique_ptr<Impl> impl;
    VizFeed viz;
    Meters meters;
    double sampleRate = 48000.0;
    int maxBlock = 512;
};
} // namespace fv::dsp
