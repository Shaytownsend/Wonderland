#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/FreddyEngine.h"
#include "state/Parameters.h"
#include "state/IRManager.h"

namespace fv
{
class FreddyVerbProcessor : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::AsyncUpdater
{
public:
    FreddyVerbProcessor();
    ~FreddyVerbProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Freddy Verb"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Freddy specifics
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    dsp::FreddyEngine& getEngine() noexcept { return engine; }
    dsp::EngineParams snapshotParams() const noexcept;
    bool isAudioRunning() const noexcept;

    /** Version written into the state tree. Bump when the format changes and add
        a migration step in setStateInformation(). */
    static constexpr int kStateVersion = 1;

    /** Free-form UI state persisted with the plugin (theme, scale, viz options). */
    juce::ValueTree& getUIState() noexcept { return uiState; }
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    double getCurrentIRRt60() const noexcept { return irManager ? irManager->getRt60() : 1.0; }
    IRManager& getIRManager() noexcept { return *irManager; }
    const IRLibrary& getIRLibrary() const noexcept { return irLibrary; }

private:
    void parameterChanged (const juce::String& id, float) override;
    void handleAsyncUpdate() override;
    void updateLatency();
    void cacheParameterPointers();

    juce::UndoManager undoManager { 30000, 30 };
    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree uiState { "UI" };
    dsp::FreddyEngine engine;
    IRLibrary irLibrary;
    std::unique_ptr<IRManager> irManager;
    std::atomic<int64_t> lastProcessMs { 0 };

    struct Ptrs;
    std::unique_ptr<Ptrs> ptrs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreddyVerbProcessor)
};
} // namespace fv
