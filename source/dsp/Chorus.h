#pragma once

#include <juce_dsp/juce_dsp.h>

namespace queen
{
// Juno chorus: I (0.5 Hz), II (0.83 Hz), I+II = both in series.
// Each mode uses two mono juce::dsp::Chorus (L, R); the R one is pre-rolled by half an LFO period
// so the sides modulate in anti-phase like the Juno BBD pair (juce's Chorus alone is not stereo).
class Chorus
{
public:
    void prepare (double sampleRate, int maxBlock);
    void process (juce::AudioBuffer<float>& buffer, int start, int n, bool on1, bool on2) noexcept;

private:
    struct Mode
    {
        juce::dsp::Chorus<float> left, right;
    };
    void prepareMode (Mode& m, float rate, double sampleRate, int maxBlock);
    static void processSide (juce::dsp::Chorus<float>& c, float* data, int n) noexcept;

    Mode one, two;
};
} // namespace queen
