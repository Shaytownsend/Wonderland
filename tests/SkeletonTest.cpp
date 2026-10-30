#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE ("Plugin plays a note from MIDI", "[skeleton]")
{
    PluginProcessor p;
    p.setPlayConfigDetails (0, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    float peak = 0.0f;
    for (int b = 0; b < 20; ++b)
    {
        p.processBlock (buf, midi);
        midi.clear();
        peak = std::max (peak, buf.getMagnitude (0, 0, 512));
    }
    CHECK (p.getName() == "The Queen");
    CHECK (peak > 0.01f);
}
