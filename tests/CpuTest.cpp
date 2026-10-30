#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>

// DSP load with 6 held notes at 48 kHz / 128, measured on the real processBlock (single core).
// Hidden from ctest; run: build/Tests "[.cpu]"
static double measureLoad (int patch, int oversampling)
{
    PluginProcessor p;
    p.memory.recall (0, patch);
    p.setOversampling (oversampling);
    p.setPlayConfigDetails (0, 2, 48000.0, 128);
    p.prepareToPlay (48000.0, 128);
    juce::AudioBuffer<float> buf (2, 128);
    juce::MidiBuffer midi;
    for (int n : { 48, 55, 60, 64, 67, 72 })
        midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
    p.processBlock (buf, midi);
    midi.clear();

    const int blocks = 48000 * 10 / 128; // 10 s of audio
    const auto start = juce::Time::getHighResolutionTicks();
    for (int b = 0; b < blocks; ++b)
        p.processBlock (buf, midi);
    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
    return secs / 10.0 * 100.0;
}

TEST_CASE ("CPU: 6 held notes at 48k/128", "[.cpu]")
{
    for (int patch : { 4, 7 })
        for (int os : { 1, 2 })
        {
            const double load = measureLoad (patch, os);
            std::printf ("patch 1-%d oversampling %dx: %.2f %% of one core\n", patch + 1, os, load);
            if (os == 1) // the 5 % budget is for the default (no oversampling); 2x is reported only
                CHECK (load < 5.0);
        }
}
