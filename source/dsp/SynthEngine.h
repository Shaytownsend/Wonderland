#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// M1 skeleton: plain sine synth so MIDI -> audio is proven end to end.
class SynthEngine
{
public:
    SynthEngine();
    void prepare (double sampleRate, int blockSize);
    void render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

private:
    juce::Synthesiser synth;
};
