#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

//==============================================================================
BldsPlayerAudioProcessor::BldsPlayerAudioProcessor()
    : BldsPlayerAudioProcessor (blds::Library::fromEmbeddedFiles())
{
}

BldsPlayerAudioProcessor::BldsPlayerAudioProcessor (std::unique_ptr<blds::Library> lib)
    : AudioProcessor (makeBuses()),
      library (std::move (lib)),
      apvts (*this, nullptr, "BLDSPlayerState", createParameterLayout())
{
    jassert (library != nullptr);
    outputChannelParam = apvts.getRawParameterValue (outputChannelParamID);
    jassert (outputChannelParam != nullptr);
    // Latency is 0 (the default) and is never changed after construction.
}

BldsPlayerAudioProcessor::~BldsPlayerAudioProcessor() = default;

juce::AudioProcessor::BusesProperties BldsPlayerAudioProcessor::makeBuses()
{
   #if BLDS_EFFECT
    return BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                            .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #else
    return BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #endif
}

juce::AudioProcessorValueTreeState::ParameterLayout BldsPlayerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { outputChannelParamID, 1 },
                                                              "Output Channel",
                                                              juce::StringArray { "Right only", "Left only", "Both" },
                                                              (int) OutputChannel::rightOnly));
    return layout;
}

bool BldsPlayerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if BLDS_EFFECT
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in.isDisabled();
   #else
    return true;
   #endif
}

//==============================================================================
void BldsPlayerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    // Exact integer match of the host rate, rounded to the nearest Hz.
    const int rate = (sampleRate > 0.0 && std::isfinite (sampleRate)) ? (int) std::lround (sampleRate) : 0;
    const auto* table = library->validTableForRate (rate);
    const auto* anyTable = library->tableForRate (rate);

    // On a table change (i.e. a sample-rate change) restart at sample 0. The audio
    // thread performs the actual reset so 'position' is only touched there.
    if (table != activeTable.load (std::memory_order_acquire))
    {
        activeTable.store (table, std::memory_order_release);
        resetPending.store (true, std::memory_order_release);
    }

    Status s = Status::ok;

    if (table == nullptr)
        s = (anyTable != nullptr && anyTable->state == blds::Table::State::invalid) ? Status::invalidFile
                                                                                     : Status::noFile;

    hostRate.store (rate, std::memory_order_release);
    status.store ((int) s, std::memory_order_release);
}

void BldsPlayerAudioProcessor::releaseResources()
{
    // Nothing to free: tables live as long as the plugin. The selected table is
    // kept so a re-prepare at the same rate continues seamlessly.
}

void BldsPlayerAudioProcessor::reset()
{
    // Intentionally a no-op: playback is free-running and must never jump, so
    // host "reset" calls (often issued on transport changes) don't move the loop.
}

//==============================================================================
void BldsPlayerAudioProcessor::consumePendingReset (const blds::Table* table) noexcept
{
    // Reset on an explicit request from prepareToPlay, or whenever the published
    // table differs from the one last played (covers any publish ordering).
    if (resetPending.exchange (false, std::memory_order_acq_rel) || table != lastRenderedTable)
    {
        position = 0;
        lastRenderedTable = table;
    }

    // Defensive: never index past the end of the current table.
    if (table != nullptr && position >= table->length)
        position = 0;
}

void BldsPlayerAudioProcessor::render (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const auto* table = activeTable.load (std::memory_order_acquire);
    consumePendingReset (table);

    // Channels beyond the stereo pair (should never exist) are silent.
    for (int ch = 2; ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0 || numChannels <= 0)
        return;

    float* left  = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    if (table == nullptr)
    {
        // No valid file for this rate: exact digital silence.
        std::fill (left, left + numSamples, 0.0f);

        if (right != nullptr)
            std::fill (right, right + numSamples, 0.0f);

        return;
    }

    const auto mode = getOutputChannel();
    float* signal = nullptr;
    float* silent = nullptr;
    float* copy   = nullptr;

    switch (mode)
    {
        case OutputChannel::leftOnly:  signal = left;  silent = right; break;
        case OutputChannel::both:      signal = left;  copy   = right; break;
        case OutputChannel::rightOnly:
        default:                       signal = right != nullptr ? right : left;
                                       silent = right != nullptr ? left  : nullptr; break;
    }

    // Bit-exact loop. Equivalent to, per sample:
    //     out = table[pos]; pos = (pos + 1 == length) ? 0 : pos + 1;
    // done as contiguous runs between wrap points (plain copies, no arithmetic).
    const float* src = table->samples.data();
    const std::uint64_t length = table->length;
    std::uint64_t pos = position;
    int written = 0;

    while (written < numSamples)
    {
        const std::uint64_t remaining = (std::uint64_t) (numSamples - written);
        const std::uint64_t run = std::min (length - pos, remaining);

        std::memcpy (signal + written, src + pos, (size_t) run * sizeof (float));

        written += (int) run;
        pos += run;

        if (pos == length)
            pos = 0;
    }

    position = pos;

    if (copy != nullptr)
        std::memcpy (copy, signal, (size_t) numSamples * sizeof (float));

    if (silent != nullptr)
        std::fill (silent, silent + numSamples, 0.0f);
}

void BldsPlayerAudioProcessor::advanceSilently (int numSamples) noexcept
{
    const auto* table = activeTable.load (std::memory_order_acquire);
    consumePendingReset (table);

    if (table != nullptr && numSamples > 0)
        position = (position + (std::uint64_t) numSamples) % table->length;
}

void BldsPlayerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);   // MIDI is not used; any input is ignored
    render (buffer);
}

void BldsPlayerAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Bypassed: exact silence on every channel (input is never passed through).
    // The loop keeps running in the background so un-bypassing stays free-running.
    buffer.clear();
    advanceSilently (buffer.getNumSamples());
}

//==============================================================================
juce::AudioProcessorEditor* BldsPlayerAudioProcessor::createEditor()
{
    return new BldsPlayerAudioProcessorEditor (*this);
}

void BldsPlayerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BldsPlayerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BldsPlayerAudioProcessor();
}
