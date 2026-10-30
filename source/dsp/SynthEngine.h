#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Arpeggiator.h"
#include "Chorus.h"
#include "FmVoice.h"
#include "Lfo.h"

namespace queen
{
// Steals the oldest voice.
class QueenSynth : public juce::Synthesiser
{
public:
    juce::SynthesiserVoice* findVoiceToSteal (juce::SynthesiserSound*, int, int) const override;
};

class SynthEngine
{
public:
    static constexpr int numVoices = 6;
    static constexpr int chunk = 16; // control-rate granularity (samples)

    SynthEngine();

    // Points the engine at APVTS raw values. Without bind() it uses its own defaults (tests).
    void bind (juce::AudioProcessorValueTreeState& apvts);
    void setParam (std::string_view id, float plainValue);
    float getParam (std::string_view id) const;

    void prepare (double sampleRate, int blockSize, int oversampling = 1);
    void reset();
    int getLatencySamples() const;

    void render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, double hostBpm = 0.0);

    void setMasterTune (double a4Hz) { a4.store (a4Hz); }
    float getBend() const { return bendOut.load(); }         // for the GUI wheels
    float getModWheel() const { return modOut.load(); }
    int getActiveVoiceCount() const;

private:
    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void updateGlobals (int numSamples);
    void syncModes();

    std::array<std::atomic<float>, kNumParams> ownValues;
    std::array<std::atomic<float>*, kNumParams> values {};
    std::array<juce::SmoothedValue<float>, kNumParams> smoothers;

    VoiceGlobals globals;
    QueenSynth synth;
    Lfo lfo;
    Arpeggiator arp;
    Chorus chorus;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::MidiBuffer emptyMidi;
    std::atomic<double> a4 { 440.0 };
    std::atomic<float> bendOut { 0.0f }, modOut { 0.0f };

    double baseRate = 44100.0, internalRate = 44100.0;
    int osFactor = 1;
    std::array<bool, 128> keyDown {};
    int keysHeld = 0;
    bool arpWasOn = false, holdWasOn = false;
};
} // namespace queen

using SynthEngine = queen::SynthEngine;
