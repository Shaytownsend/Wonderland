#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "IRLibrary.h"
#include "../dsp/FreddyEngine.h"

namespace fv
{
/** Owns the current IR source and turns it into convolvers on a background
    thread whenever the IR, the shaping parameters, the quality mode or the
    session sample rate change. Hand-off to the audio thread is lock-free (the
    engine crossfades old -> new over 50 ms). Retired convolvers are deleted here.

    IR identity in the plugin state:
      <IR kind="factory|file" id="plate_medium" path="/abs/file.wav" hash="..." name="..."
          embedded="base64 (small user IRs only)"/>  */
class IRManager : private juce::Thread
{
public:
    IRManager (juce::AudioProcessorValueTreeState&, dsp::FreddyEngine&, const IRLibrary&);
    ~IRManager() override;

    /** Synchronous rebuild for the new sample rate / block size (call from prepareToPlay). */
    void prepare (double sampleRate, int maxBlockSize, bool nonRealtime);

    void loadFactory (const juce::String& id);
    bool loadFile (const juce::File& file, juce::String& error);
    /** Re-points a missing user IR to a new file. */
    bool relink (const juce::File& file, juce::String& error) { return loadFile (file, error); }

    juce::ValueTree getState() const;
    void setState (const juce::ValueTree& irState);

    struct Status
    {
        juce::String id, name, path, category;
        bool factory = true, missing = false, embedded = false;
        dsp::IRDisplayInfo display;
        int version = 0;         // increments on every rebuild (UI polls this)
    };
    Status getStatus() const;
    int getVersion() const noexcept { return version.load(); }
    double getRt60() const noexcept { return rt60.load(); }

    /** Blocks until all pending rebuilds are applied (tests / offline). */
    void waitUntilIdle (int timeoutMs = 5000);

    static constexpr size_t kMaxEmbedBytes = 2 * 1024 * 1024;

private:
    void run() override;
    dsp::IRShapeParams readShape() const;
    int readQuality() const;
    void rebuild (bool installNow);
    void setSource (std::shared_ptr<const dsp::IRSource>, const juce::String& id, const juce::String& name,
                    const juce::String& path, const juce::String& hash, bool factory, juce::MemoryBlock embeddedData);

    juce::AudioProcessorValueTreeState& apvts;
    dsp::FreddyEngine& engine;
    const IRLibrary& library;

    juce::CriticalSection buildLock;          // serialises rebuilds (loader thread vs prepareToPlay)
    mutable juce::CriticalSection lock;       // guards source + status (never taken on the audio thread)
    std::shared_ptr<const dsp::IRSource> source;
    Status status;
    juce::String hash;
    juce::MemoryBlock embedded;

    std::atomic<bool> dirty { true }, busy { false };
    std::atomic<int> version { 0 };
    std::atomic<double> rt60 { 1.6 };
    std::atomic<double> sampleRate { 48000.0 };
    std::atomic<int> maxBlock { 512 };
    std::atomic<bool> offline { false };
    dsp::IRShapeParams lastShape;
    int lastQuality = -1;
    double lastRate = 0;
    juce::uint32 lastChangeMs = 0;
};
} // namespace fv
