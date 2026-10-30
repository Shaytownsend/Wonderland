#pragma once

#include <juce_dsp/juce_dsp.h>

namespace queen
{
// Per-voice Juno filter section: 1-pole HPF {off,120,300,700 Hz} then a 24 dB ladder LPF.
class JunoFilter
{
public:
    static constexpr float hpfFreqs[4] { 0.0f, 120.0f, 300.0f, 700.0f };

    void prepare (double sampleRate, int maxBlock);
    void reset();

    // vcf_freq 0..1 -> 20 Hz..20 kHz (log)
    static float baseCutoff (float vcfFreq) { return 20.0f * std::pow (1000.0f, vcfFreq); }

    void setHpf (int index);
    void setCutoff (float hz);
    void setResonance (float res01);
    void process (float* samples, int n) noexcept;

private:
    double sr = 44100.0;
    float hpfCoeff = 0.0f, hpfState = 0.0f, hpfPrevIn = 0.0f;
    bool hpfOn = false;
    juce::dsp::LadderFilter<float> ladder;
};
} // namespace queen
