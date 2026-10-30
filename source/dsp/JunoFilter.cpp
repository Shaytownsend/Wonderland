#include "JunoFilter.h"

namespace queen
{
void JunoFilter::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    ladder.prepare ({ sampleRate, (juce::uint32) maxBlock, 1 });
    ladder.setMode (juce::dsp::LadderFilterMode::LPF24);
    ladder.setDrive (1.0f);
    reset();
}

void JunoFilter::reset()
{
    hpfState = hpfPrevIn = 0.0f;
    ladder.reset();
}

void JunoFilter::setHpf (int index)
{
    const float f = hpfFreqs[juce::jlimit (0, 3, index)];
    hpfOn = f > 0.0f;
    // one-pole high-pass: y[n] = a * (y[n-1] + x[n] - x[n-1])
    const float rc = hpfOn ? 1.0f / (juce::MathConstants<float>::twoPi * f) : 1.0f;
    const float dt = (float) (1.0 / sr);
    hpfCoeff = rc / (rc + dt);
}

void JunoFilter::setCutoff (float hz)
{
    ladder.setCutoffFrequencyHz (juce::jlimit (20.0f, (float) (sr * 0.45), hz));
}

void JunoFilter::setResonance (float res01)
{
    ladder.setResonance (juce::jlimit (0.0f, 0.97f, res01 * 0.97f));
}

void JunoFilter::process (float* samples, int n) noexcept
{
    if (hpfOn)
    {
        for (int i = 0; i < n; ++i)
        {
            const float x = samples[i];
            hpfState = hpfCoeff * (hpfState + x - hpfPrevIn);
            hpfPrevIn = x;
            samples[i] = hpfState;
        }
    }
    float* chans[] = { samples };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    ladder.process (juce::dsp::ProcessContextReplacing<float> (block));
}
} // namespace queen
