#include "SynthEngine.h"

namespace queen
{
juce::SynthesiserVoice* QueenSynth::findVoiceToSteal (juce::SynthesiserSound*, int, int) const
{
    juce::SynthesiserVoice* oldest = nullptr;
    for (auto* v : voices)
        if (oldest == nullptr || v->wasStartedBefore (*oldest))
            oldest = v;
    return oldest;
}

SynthEngine::SynthEngine()
{
    for (int i = 0; i < kNumParams; ++i)
    {
        ownValues[(size_t) i].store (kParams[(size_t) i].def);
        values[(size_t) i] = &ownValues[(size_t) i];
    }
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new FmVoice (globals));
    synth.addSound (new FmSound());
    synth.setNoteStealingEnabled (true);
}

void SynthEngine::bind (juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < kNumParams; ++i)
        if (auto* v = apvts.getRawParameterValue (toString (kParams[(size_t) i].id)))
            values[(size_t) i] = v;
}

void SynthEngine::setParam (std::string_view id, float plainValue)
{
    values[(size_t) indexOf (id)]->store (plainValue);
}

float SynthEngine::getParam (std::string_view id) const
{
    return values[(size_t) indexOf (id)]->load();
}

void SynthEngine::prepare (double sampleRate, int blockSize, int oversampling)
{
    baseRate = sampleRate;
    osFactor = oversampling >= 2 ? 2 : 1;
    internalRate = baseRate * osFactor;

    if (osFactor > 1)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampler->initProcessing ((size_t) blockSize);
    }
    else
    {
        oversampler.reset();
    }

    synth.setCurrentPlaybackSampleRate (internalRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<FmVoice*> (synth.getVoice (i)))
            v->prepare (internalRate, blockSize * osFactor);
    lfo.prepare (internalRate);
    arp.prepare (internalRate);
    chorus.prepare (baseRate, blockSize);

    for (int i = 0; i < kNumParams; ++i)
    {
        smoothers[(size_t) i].reset (internalRate, 0.01);
        smoothers[(size_t) i].setCurrentAndTargetValue (values[(size_t) i]->load());
        globals.p[(size_t) i] = values[(size_t) i]->load();
    }
    reset();
}

void SynthEngine::reset()
{
    synth.allNotesOff (0, false);
    arp.reset();
    keyDown.fill (false);
    keysHeld = 0;
    if (oversampler)
        oversampler->reset();
}

int SynthEngine::getLatencySamples() const
{
    return oversampler ? (int) std::round (oversampler->getLatencyInSamples()) : 0;
}

int SynthEngine::getActiveVoiceCount() const
{
    int n = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        n += synth.getVoice (i)->isVoiceActive() ? 1 : 0;
    return n;
}

void SynthEngine::noteOn (int note, float velocity)
{
    if (! keyDown[(size_t) note])
    {
        keyDown[(size_t) note] = true;
        if (keysHeld++ == 0)
            lfo.retrigger(); // Juno: delay restarts when the first key goes down
    }
    if (values[(size_t) QP ("arp_on")]->load() > 0.5f)
        arp.noteOn (note, velocity);
    else
        synth.noteOn (1, note, velocity);
}

void SynthEngine::noteOff (int note)
{
    if (keyDown[(size_t) note])
    {
        keyDown[(size_t) note] = false;
        --keysHeld;
    }
    if (values[(size_t) QP ("arp_on")]->load() > 0.5f)
        arp.noteOff (note);
    else
        synth.noteOff (1, note, 0.0f, true);
}

void SynthEngine::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        globals.bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isController() && m.getControllerNumber() == 1)
        globals.modWheel = (float) m.getControllerValue() / 127.0f;
    else if (m.isSustainPedalOn())
        synth.handleSustainPedal (1, true);
    else if (m.isSustainPedalOff())
        synth.handleSustainPedal (1, values[(size_t) QP ("hold")]->load() > 0.5f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        synth.allNotesOff (0, ! m.isAllSoundOff());
        arp.reset();
        keyDown.fill (false);
        keysHeld = 0;
    }
}

void SynthEngine::syncModes()
{
    const bool arpOn = values[(size_t) QP ("arp_on")]->load() > 0.5f;
    const bool hold = values[(size_t) QP ("hold")]->load() > 0.5f;

    if (arpOn != arpWasOn)
    {
        synth.allNotesOff (0, true);
        arp.reset();
        if (arpOn)
            for (int n = 0; n < 128; ++n)
                if (keyDown[(size_t) n])
                    arp.noteOn (n, 0.8f);
        arpWasOn = arpOn;
    }
    if (hold != holdWasOn)
    {
        synth.handleSustainPedal (1, hold && ! arpOn);
        holdWasOn = hold;
    }
    arp.setHold (hold);
    arp.setKeyTranspose (values[(size_t) QP ("key_transpose")]->load() > 0.5f);
    arp.setMode ((int) values[(size_t) QP ("arp_mode")]->load());
    arp.setRange ((int) values[(size_t) QP ("arp_range")]->load() + 1);
}

void SynthEngine::updateGlobals (int numSamples)
{
    for (size_t i = 0; i < (size_t) kNumParams; ++i)
        globals.p[i] = kParams[i].kind == Kind::Float ? smoothers[i].skip (numSamples) : values[i]->load();

    const auto& p = globals.p;
    globals.lfo = lfo.advance (numSamples, p[QP ("lfo_rate")], p[QP ("lfo_delay")],
                               (int) p[QP ("lfo_trig_mode")] == 1, p[QP ("lfo_trigger")] > 0.5f);
    globals.a4 = a4.load();
}

void SynthEngine::render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, double hostBpm)
{
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    if (numSamples == 0)
        return;

    for (size_t i = 0; i < (size_t) kNumParams; ++i)
        if (kParams[i].kind == Kind::Float)
            smoothers[i].setTargetValue (values[i]->load());

    syncModes();
    const bool arpOn = arpWasOn;
    double arpRate = values[(size_t) QP ("arp_rate")]->load();
    if (values[(size_t) QP ("arp_sync")]->load() > 0.5f && hostBpm > 0.0)
        arpRate = hostBpm / 60.0 * 4.0; // 1/16 notes
    arp.setRate (arpRate);

    // with 2x oversampling the voices render into the oversampler's upsampled block
    juce::dsp::AudioBlock<float> outerBlock (buffer);
    juce::AudioBuffer<float> work;
    if (oversampler && buffer.getNumChannels() == 2)
    {
        auto up = oversampler->processSamplesUp (outerBlock);
        float* chans[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };
        work.setDataToReferTo (chans, 2, (int) up.getNumSamples());
        work.clear();
    }
    else
    {
        work.setDataToReferTo (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), numSamples);
    }
    const int factor = work.getNumSamples() / numSamples;
    const int total = work.getNumSamples();

    auto emit = [this] (const Arpeggiator::Event& e) {
        if (e.on)
            synth.noteOn (1, e.note, e.velocity);
        else
            synth.noteOff (1, e.note, 0.0f, true);
    };

    auto it = midi.cbegin();
    int pos = 0;
    while (pos < total)
    {
        while (it != midi.cend() && (*it).samplePosition * factor <= pos)
        {
            handleMidi ((*it).getMessage());
            ++it;
        }
        if (arpOn)
            arp.processDue (emit);

        const int nextMidi = it != midi.cend() ? (*it).samplePosition * factor : total;
        int len = juce::jmin (chunk * factor, total - pos, nextMidi - pos);
        if (arpOn)
            len = juce::jmin (len, juce::jmax (1, arp.samplesUntilNextEvent()));
        len = juce::jmax (1, len);

        updateGlobals (len);
        synth.renderNextBlock (work, emptyMidi, pos, len);
        if (arpOn)
            arp.advance (len);
        pos += len;
    }
    for (; it != midi.cend(); ++it)
        handleMidi ((*it).getMessage());

    if (oversampler && buffer.getNumChannels() == 2)
        oversampler->processSamplesDown (outerBlock);

    const bool c1 = values[(size_t) QP ("chorus_1")]->load() > 0.5f;
    const bool c2 = values[(size_t) QP ("chorus_2")]->load() > 0.5f;
    chorus.process (buffer, 0, numSamples, c1, c2);
}
} // namespace queen
