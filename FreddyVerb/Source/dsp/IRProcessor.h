#pragma once

#include <vector>
#include "Convolver.h"

namespace fv::dsp
{
/** A raw impulse response as loaded from disk / factory library. 1, 2 or 4 channels. */
struct IRSource
{
    std::vector<std::vector<double>> channels;   // 1 (mono), 2 (stereo) or 4 (LL, LR, RL, RR)
    double sampleRate = 48000.0;
    int length() const noexcept { return channels.empty() ? 0 : (int) channels[0].size(); }
};

/** Non-destructive IR shaping parameters (everything is recomputed off the audio thread). */
struct IRShapeParams
{
    float startMs     = 0.0f;     // skip the first part of the IR (0..100 ms)
    float lengthPct   = 100.0f;   // truncate with a smooth fade (10..100 %)
    float decayPct    = 100.0f;   // apparent RT60 relative to the original (25..200 %)
    float sizePct     = 100.0f;   // time stretch by resampling (50..200 %)
    float xoverMs     = 80.0f;    // early/late split
    float earlyDb     = 0.0f;
    float lateDb      = 0.0f;
    float dampingPct  = 0.0f;     // extra high-frequency decay over time
    float dampFreqHz  = 5000.0f;
    bool  reverse     = false;
    bool  normalize   = true;

    bool operator== (const IRShapeParams& o) const noexcept
    {
        return startMs == o.startMs && lengthPct == o.lengthPct && decayPct == o.decayPct && sizePct == o.sizePct
            && xoverMs == o.xoverMs && earlyDb == o.earlyDb && lateDb == o.lateDb && dampingPct == o.dampingPct
            && dampFreqHz == o.dampFreqHz && reverse == o.reverse && normalize == o.normalize;
    }
    bool operator!= (const IRShapeParams& o) const noexcept { return ! (*this == o); }
};

/** Display data computed alongside the shaped IR. */
struct IRDisplayInfo
{
    std::vector<float> peaks;                    // thumbnail (0..1), fixed number of points
    std::vector<std::vector<float>> spectrogram; // [band][time] 0..1
    double lengthSeconds = 0.0;
    double rt60 = 0.0;                           // of the shaped IR
    double sourceRt60 = 0.0;                     // of the unshaped IR
};

/** All the off-audio-thread IR maths. Stateless; every function is pure. */
namespace IRProcessor
{
    /** Full pipeline: upmix -> resample (session rate x size) -> normalise ->
        decay -> damping -> early/late -> reverse -> start trim -> length fade. */
    IRData render (const IRSource& src, const IRShapeParams& shape, double sessionRate, IRDisplayInfo* info = nullptr);

    /** High-quality resampling (r8brain-free-src, 24-bit quality class). */
    std::vector<double> resample (const std::vector<double>& x, double fromRate, double toRate);

    /** RT60 estimate (Schroeder backward integration, T30 extrapolated; falls back to
        T20 / EDT for short or noisy IRs). Returns seconds. */
    double estimateRt60 (const std::vector<double>& x, double sampleRate);

    /** Gain that loudness-normalises the IR (K-weighted energy per output channel). */
    double loudnessNormalisationGain (const IRData& ir);

    /** Target K-weighted energy per output channel (-6 dB power gain). */
    inline constexpr double kTargetEnergy = 0.25;

    /** Helpers exposed for tests. */
    void applyDamping (std::vector<double>& x, double sampleRate, double amount01, double cornerHz);
    IRSource upmixToFourPaths (const IRSource& src);
}
} // namespace fv::dsp
