#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Parameters.h"
#include "dsp/SynthEngine.h"
#include "state/PatchMemory.h"

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Manual"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    SynthEngine engine;
    queen::PatchMemory memory { apvts };
    juce::MidiKeyboardState keyboardState;

    // on-screen wheels -> MIDI (picked up at the start of the next block)
    void setWheel (bool pitchBend, float value);

    // host-bar settings (stored with the plugin state, not with patches)
    void setOversampling (int factor); // 1 or 2
    int getOversampling() const { return oversampling.load(); }
    void setMasterTune (double a4Hz);
    double getMasterTune() const { return masterTune.load(); }

private:
    std::atomic<int> oversampling { 1 };
    std::atomic<int> pendingBend { -1 }, pendingMod { -1 };
    std::atomic<double> masterTune { 440.0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
