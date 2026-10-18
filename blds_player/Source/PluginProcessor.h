#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BldsLibrary.h"

#include <atomic>
#include <cstdint>
#include <memory>

#ifndef BLDS_EFFECT
 #define BLDS_EFFECT 0
#endif

class BldsPlayerAudioProcessor final : public juce::AudioProcessor
{
public:
    enum class OutputChannel { rightOnly = 0, leftOnly = 1, both = 2 };

    enum class Status { waitingForHost = 0, ok = 1, noFile = 2, invalidFile = 3 };

    // Production constructor: decodes the files embedded in the binary.
    BldsPlayerAudioProcessor();

    // Test constructor: uses the given (already decoded) library.
    explicit BldsPlayerAudioProcessor (std::unique_ptr<blds::Library> library);

    ~BldsPlayerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                       { return true; }

    const juce::String getName() const override           { return JucePlugin_Name; }
    bool acceptsMidi() const override                     { return false; }
    bool producesMidi() const override                    { return false; }
    bool isMidiEffect() const override                    { return false; }
    double getTailLengthSeconds() const override          { return std::numeric_limits<double>::infinity(); }

    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Read-only state for the UI / tests (atomics, safe from any thread).
    const blds::Library& getLibrary() const noexcept      { return *library; }
    int getHostRate() const noexcept                      { return hostRate.load (std::memory_order_acquire); }
    Status getStatus() const noexcept                     { return (Status) status.load (std::memory_order_acquire); }
    const blds::Table* getActiveTable() const noexcept    { return activeTable.load (std::memory_order_acquire); }
    OutputChannel getOutputChannel() const noexcept       { return (OutputChannel) juce::jlimit (0, 2, (int) outputChannelParam->load (std::memory_order_relaxed)); }

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }

    static constexpr const char* outputChannelParamID = "outputChannel";

private:
    static BusesProperties makeBuses();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void render (juce::AudioBuffer<float>& buffer) noexcept;
    void advanceSilently (int numSamples) noexcept;
    void consumePendingReset (const blds::Table* table) noexcept;

    // Allocated once in the constructor, never freed while the plugin exists.
    const std::unique_ptr<const blds::Library> library;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* outputChannelParam = nullptr;

    // Published by prepareToPlay, read by the audio thread.
    std::atomic<const blds::Table*> activeTable { nullptr };
    std::atomic<bool> resetPending { false };

    // UI state.
    std::atomic<int> hostRate { 0 };
    std::atomic<int> status { (int) Status::waitingForHost };

    // Audio-thread only.
    std::uint64_t position = 0;
    const blds::Table* lastRenderedTable = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BldsPlayerAudioProcessor)
};
