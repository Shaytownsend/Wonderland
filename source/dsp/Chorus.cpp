#include "Chorus.h"

namespace queen
{
void Chorus::prepareMode (Mode& m, float rate, double sampleRate, int maxBlock)
{
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 1 };
    for (auto* c : { &m.left, &m.right })
    {
        c->prepare (spec);
        c->setRate (rate);
        c->setDepth (0.3f);
        c->setMix (0.5f);
        c->setCentreDelay (7.0f);
        c->setFeedback (0.0f);
    }
    // pre-roll the right side by half an LFO period (message thread, prepare time only)
    juce::AudioBuffer<float> silence (1, maxBlock);
    int remaining = (int) (sampleRate / rate * 0.5);
    while (remaining > 0)
    {
        const int n = juce::jmin (remaining, maxBlock);
        silence.clear();
        processSide (m.right, silence.getWritePointer (0), n);
        remaining -= n;
    }
}

void Chorus::prepare (double sampleRate, int maxBlock)
{
    prepareMode (one, 0.5f, sampleRate, maxBlock);
    prepareMode (two, 0.83f, sampleRate, maxBlock);
}

void Chorus::processSide (juce::dsp::Chorus<float>& c, float* data, int n) noexcept
{
    float* chans[] = { data };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    c.process (juce::dsp::ProcessContextReplacing<float> (block));
}

void Chorus::process (juce::AudioBuffer<float>& buffer, int start, int n, bool on1, bool on2) noexcept
{
    if (buffer.getNumChannels() < 2)
        return;
    float* l = buffer.getWritePointer (0, start);
    float* r = buffer.getWritePointer (1, start);
    if (on1)
    {
        processSide (one.left, l, n);
        processSide (one.right, r, n);
    }
    if (on2)
    {
        processSide (two.left, l, n);
        processSide (two.right, r, n);
    }
}
} // namespace queen
