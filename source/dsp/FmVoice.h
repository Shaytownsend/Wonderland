#pragma once

#include "../Parameters.h"
#include "Algorithms.h"
#include "FmOperator.h"
#include "JunoFilter.h"

namespace queen
{
// Snapshot of everything a voice needs for one render chunk; written by SynthEngine before each chunk.
struct VoiceGlobals
{
    std::array<float, kNumParams> p {}; // plain (smoothed) parameter values
    float lfo = 0.0f;                    // bipolar, already faded in
    float bend = 0.0f;                   // -1..1
    float modWheel = 0.0f;               // 0..1
    double a4 = 440.0;
};

namespace pi
{
    template <size_t N>
    struct Fixed
    {
        char s[N] {};
        constexpr Fixed (const char (&str)[N])
        {
            for (size_t i = 0; i < N; ++i)
                s[i] = str[i];
        }
    };
    template <Fixed name>
    inline constexpr int idx = [] {
        constexpr int i = indexOf (std::string_view (name.s, sizeof (name.s) - 1));
        static_assert (i >= 0, "unknown parameter id");
        return i;
    }();
} // namespace pi

#define QP(name) ::queen::pi::idx<name>

struct FmSound : juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class FmVoice : public juce::SynthesiserVoice
{
public:
    static constexpr int maxChunk = 64;
    static constexpr float voiceGain = 0.5f;

    explicit FmVoice (const VoiceGlobals& g) : globals (g) {}

    void prepare (double sampleRate, int maxBlock);

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<FmSound*> (s) != nullptr; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override;
    using SynthesiserVoice::renderNextBlock;

private:
    void renderChunk (juce::AudioBuffer<float>& out, int start, int n);
    void updateEnvelope();

    const VoiceGlobals& globals;
    std::array<FmOperator, 4> ops;
    JunoFilter filter;
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    juce::Random noise;
    float velocity = 0.0f;
    float gate = 0.0f, gateTarget = 0.0f, gateStep = 0.0f;
    float lastEnv = 0.0f;
    int note = 60;
    int lastHpf = -1;
};
} // namespace queen
