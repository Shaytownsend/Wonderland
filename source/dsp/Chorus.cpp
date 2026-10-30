#include "Chorus.h"

namespace queen
{
void Chorus::prepare (double sampleRate, int maxBlock)
{
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 2 };
    chorus1.prepare (spec);
    chorus2.prepare (spec);
    chorus1.setRate (0.5f);
    chorus2.setRate (0.83f);
    for (auto* c : { &chorus1, &chorus2 })
    {
        c->setDepth (0.3f);
        c->setMix (0.5f);
        c->setCentreDelay (7.0f);
        c->setFeedback (0.0f);
    }
}

void Chorus::reset()
{
    chorus1.reset();
    chorus2.reset();
}

void Chorus::process (juce::AudioBuffer<float>& buffer, int start, int n, bool on1, bool on2) noexcept
{
    if (buffer.getNumChannels() < 2)
        return;
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) start, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    if (on1)
        chorus1.process (ctx);
    if (on2)
        chorus2.process (ctx);
}
} // namespace queen
