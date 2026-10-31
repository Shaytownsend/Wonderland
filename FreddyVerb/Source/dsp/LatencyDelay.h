#pragma once

#include <vector>
#include "DspCommon.h"

namespace fv::dsp
{
/** Integer multi-channel delay line used for latency compensation (dry path,
    key path, lookahead) and for the pre-delay. When the delay changes it
    crossfades from the old tap to the new tap instead of jumping, so changes
    are click-free. Real-time safe after prepare(). */
class LatencyDelay
{
public:
    void prepare (int numChannels, int maxDelaySamples, double sampleRate, double fadeMs = 20.0);
    void reset();

    /** Sets the target delay. Takes effect immediately when `immediate`,
        otherwise via a crossfade. */
    void setDelay (int samples, bool immediate = false) noexcept;
    int  getDelay() const noexcept { return targetDelay; }

    /** In-place processing of numChannels channels. */
    void process (float* const* data, int numChannels, int numSamples) noexcept;
    /** Single-sample processing for one channel set (used inside sample loops). */
    void pushAndRead (const float* in, float* out, int numChannels) noexcept;

private:
    int nextIndex (int i) const noexcept { return (i + 1) & mask; }
    std::vector<std::vector<float>> buf;
    int mask = 0, writePos = 0;
    int curDelay = 0, targetDelay = 0, oldDelay = 0;
    int fadeLen = 1, fadePos = 0;
    bool fading = false;
    int pendingDelay = -1;
};
} // namespace fv::dsp
