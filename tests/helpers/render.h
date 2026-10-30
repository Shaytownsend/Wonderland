#pragma once

#include <dsp/SynthEngine.h>

struct RenderStats
{
    float peak = 0.0f;
    double rms = 0.0;
    bool hasNaN = false;
};

inline RenderStats stats (const juce::AudioBuffer<float>& b)
{
    RenderStats s;
    double sum = 0.0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (c, i);
            if (! std::isfinite (v))
                s.hasNaN = true;
            s.peak = std::max (s.peak, std::abs (v));
            sum += (double) v * v;
        }
    s.rms = std::sqrt (sum / std::max (1, b.getNumChannels() * b.getNumSamples()));
    return s;
}

// Renders `seconds` with note-on at 0 and optional note-off at offSeconds (<0: never), block size 128.
inline juce::AudioBuffer<float> renderNotes (queen::SynthEngine& e, std::initializer_list<int> notes, double seconds,
                                             double offSeconds = -1.0, double sr = 48000.0, int block = 128)
{
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> out (2, total), tmp (2, block);
    const int offAt = offSeconds < 0 ? -1 : (int) (offSeconds * sr);
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        juce::MidiBuffer midi;
        if (pos == 0)
            for (int note : notes)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        if (offAt >= pos && offAt < pos + n)
            for (int note : notes)
                midi.addEvent (juce::MidiMessage::noteOff (1, note), offAt - pos);
        tmp.setSize (2, n, false, false, true);
        e.render (tmp, midi);
        for (int c = 0; c < 2; ++c)
            out.copyFrom (c, pos, tmp, c, 0, n);
    }
    return out;
}
