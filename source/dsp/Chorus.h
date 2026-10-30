#pragma once

#include <juce_dsp/juce_dsp.h>

namespace queen
{
// Juno chorus: I (0.5 Hz), II (0.83 Hz), I+II = both in series.
class Chorus
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, int start, int n, bool on1, bool on2) noexcept;

private:
    juce::dsp::Chorus<float> chorus1, chorus2;
};
} // namespace queen
