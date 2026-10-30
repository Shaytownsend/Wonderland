#include "SynthEngine.h"

namespace
{
struct SineSound : juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

struct SineVoice : juce::SynthesiserVoice
{
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SineSound*> (s) != nullptr; }
    void startNote (int note, float velocity, juce::SynthesiserSound*, int) override
    {
        phase = 0.0;
        delta = juce::MathConstants<double>::twoPi * juce::MidiMessage::getMidiNoteInHertz (note) / getSampleRate();
        level = 0.2 * velocity;
    }
    void stopNote (float, bool) override { clearCurrentNote(); level = 0.0; }
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (level == 0.0)
            return;
        for (int i = start; i < start + num; ++i)
        {
            auto s = (float) (level * std::sin (phase));
            phase += delta;
            for (int c = 0; c < out.getNumChannels(); ++c)
                out.addSample (c, i, s);
        }
    }
    double phase = 0.0, delta = 0.0, level = 0.0;
};
}

SynthEngine::SynthEngine()
{
    for (int i = 0; i < 6; ++i)
        synth.addVoice (new SineVoice());
    synth.addSound (new SineSound());
}

void SynthEngine::prepare (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
}

void SynthEngine::render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
}
