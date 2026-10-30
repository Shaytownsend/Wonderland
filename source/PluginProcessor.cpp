#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "QUEEN", queen::createParameterLayout())
{
    engine.bind (apvts);
    memory.recall (0, 0);
}

void PluginProcessor::setOversampling (int factor)
{
    factor = factor >= 2 ? 2 : 1;
    if (factor == oversampling.load())
        return;
    oversampling.store (factor);
    if (getSampleRate() > 0)
    {
        suspendProcessing (true);
        prepareToPlay (getSampleRate(), getBlockSize());
        suspendProcessing (false);
    }
}

void PluginProcessor::setMasterTune (double a4Hz)
{
    masterTune.store (juce::jlimit (430.0, 450.0, a4Hz));
    engine.setMasterTune (masterTune.load());
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, oversampling.load());
    engine.setMasterTune (masterTune.load());
    setLatencySamples (engine.getLatencySamples());
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    double bpm = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;
    engine.render (buffer, midi, bpm);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("MEMORY"), nullptr);
    state.setProperty ("oversampling", oversampling.load(), nullptr);
    state.setProperty ("masterTune", masterTune.load(), nullptr);
    state.appendChild (memory.toValueTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    auto mem = state.getChildWithName ("MEMORY");
    if (mem.isValid())
    {
        memory.fromValueTree (mem);
        state.removeChild (mem, nullptr);
    }
    setMasterTune ((double) state.getProperty ("masterTune", 440.0));
    setOversampling ((int) state.getProperty ("oversampling", 1));
    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
