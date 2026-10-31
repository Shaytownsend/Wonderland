#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <algorithm>

namespace fv::dsp
{
/** One decimated frame of the envelope visualizer (one per kVizDecimation samples). */
struct VizFrame
{
    float dryPeak  = 0.0f;  // latency-aligned dry input, linear peak (max |L|,|R|)
    float wetPeak  = 0.0f;  // wet after duck + gate, linear peak
    float duckGain = 1.0f;  // linear 0..~1.19 (swell overshoots)
    float gateGain = 1.0f;  // linear 0..1
    float keyDb    = -120.0f; // detector level in dB
};

inline constexpr int kVizDecimation = 64;

/** Lock-free single-producer / single-consumer FIFO: the audio thread pushes,
    the UI thread pops at 60 fps. Overflow drops frames (never blocks). */
class VizFeed
{
public:
    static constexpr int kCapacity = 1 << 13;

    bool push (const VizFrame& f) noexcept
    {
        const auto w = write.load (std::memory_order_relaxed);
        const auto r = read.load (std::memory_order_acquire);
        if (w - r >= (uint32_t) kCapacity) return false;
        frames[w & (kCapacity - 1)] = f;
        write.store (w + 1, std::memory_order_release);
        return true;
    }

    /** Pops up to maxFrames into dest, returns the count. */
    int pop (VizFrame* dest, int maxFrames) noexcept
    {
        const auto r = read.load (std::memory_order_relaxed);
        const auto w = write.load (std::memory_order_acquire);
        const int n = (int) std::min<uint32_t> (w - r, (uint32_t) maxFrames);
        for (int i = 0; i < n; ++i) dest[i] = frames[(r + (uint32_t) i) & (kCapacity - 1)];
        read.store (r + (uint32_t) n, std::memory_order_release);
        return n;
    }

    /** Sample rate of the frames (sampleRate / kVizDecimation). Written on prepare. */
    std::atomic<double> frameRate { 48000.0 / kVizDecimation };

private:
    std::array<VizFrame, kCapacity> frames {};
    std::atomic<uint32_t> write { 0 }, read { 0 };
};

/** Peak + RMS meter values published by the audio thread. */
struct MeterValues
{
    std::atomic<float> peakL { 0 }, peakR { 0 }, rmsL { 0 }, rmsR { 0 };
};

struct Meters
{
    MeterValues input, wet, output;
    std::atomic<float> correlation { 1.0f };
    std::atomic<float> duckGainDb { 0.0f }, gateGainDb { 0.0f }, keyDb { -120.0f };
};
} // namespace fv::dsp
