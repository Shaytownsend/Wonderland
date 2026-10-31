#include "LatencyDelay.h"

namespace fv::dsp
{
void LatencyDelay::prepare (int numChannels, int maxDelaySamples, double sampleRate, double fadeMs)
{
    int size = 1;
    while (size < maxDelaySamples + 2) size <<= 1;
    buf.assign ((size_t) std::max (1, numChannels), std::vector<float> ((size_t) size, 0.0f));
    mask = size - 1;
    fadeLen = std::max (1, (int) (sampleRate * fadeMs * 0.001));
    reset();
}

void LatencyDelay::reset()
{
    for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
    writePos = 0;
    curDelay = oldDelay = targetDelay;
    fading = false;
    pendingDelay = -1;
}

void LatencyDelay::setDelay (int samples, bool immediate) noexcept
{
    samples = std::clamp (samples, 0, mask - 1);
    if (immediate)
    {
        targetDelay = curDelay = oldDelay = samples;
        fading = false;
        pendingDelay = -1;
        return;
    }
    if (samples == targetDelay && pendingDelay < 0) return;
    if (fading) { pendingDelay = samples; return; } // finish the current fade first
    oldDelay = curDelay;
    targetDelay = curDelay = samples;
    if (oldDelay != curDelay) { fading = true; fadePos = 0; }
}

void LatencyDelay::pushAndRead (const float* in, float* out, int numChannels) noexcept
{
    const int nch = std::min (numChannels, (int) buf.size());
    for (int c = 0; c < nch; ++c) buf[(size_t) c][(size_t) writePos] = in[c];

    if (! fading)
    {
        const int rp = (writePos - curDelay) & mask;
        for (int c = 0; c < nch; ++c) out[c] = buf[(size_t) c][(size_t) rp];
    }
    else
    {
        const float x = (float) fadePos / (float) fadeLen;
        const int rpNew = (writePos - curDelay) & mask;
        const int rpOld = (writePos - oldDelay) & mask;
        for (int c = 0; c < nch; ++c)
            out[c] = buf[(size_t) c][(size_t) rpOld] * (1.0f - x) + buf[(size_t) c][(size_t) rpNew] * x;
        if (++fadePos >= fadeLen)
        {
            fading = false;
            oldDelay = curDelay;
            if (pendingDelay >= 0) { const int p = pendingDelay; pendingDelay = -1; setDelay (p); }
        }
    }
    writePos = nextIndex (writePos);
}

void LatencyDelay::process (float* const* data, int numChannels, int numSamples) noexcept
{
    const int nch = std::min (numChannels, (int) buf.size());
    if (! fading && curDelay == 0) // still need to keep history for future changes
    {
        for (int i = 0; i < numSamples; ++i)
        {
            for (int c = 0; c < nch; ++c) buf[(size_t) c][(size_t) writePos] = data[c][i];
            writePos = nextIndex (writePos);
        }
        return;
    }
    float in[8], out[8];
    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < nch; ++c) in[c] = data[c][i];
        pushAndRead (in, out, nch);
        for (int c = 0; c < nch; ++c) data[c][i] = out[c];
    }
}
} // namespace fv::dsp
