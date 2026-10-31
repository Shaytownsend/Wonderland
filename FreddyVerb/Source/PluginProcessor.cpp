#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace fv
{
struct FreddyVerbProcessor::Ptrs
{
    std::atomic<float>* get (juce::AudioProcessorValueTreeState& s, const char* id)
    {
        auto* p = s.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    }

    explicit Ptrs (juce::AudioProcessorValueTreeState& s)
    {
        using namespace pid;
        #define FV_P(member, id) member = get (s, id)
        FV_P (q, quality); FV_P (pre, predelay); FV_P (preSync, predelaySync); FV_P (preNote, predelayNote);
        FV_P (keyHp, keyHpf); FV_P (keyLp, keyLpf); FV_P (det, detMode); FV_P (rms, detRms);
        FV_P (look, lookahead); FV_P (listen, keyListen);
        FV_P (dOn, duckOn); FV_P (dMode, duckMode); FV_P (dDepth, duckDepth); FV_P (dThr, duckThresh);
        FV_P (dAtt, duckAttack); FV_P (dRel, duckRelease); FV_P (dAmt, duckAmount); FV_P (dCurve, duckRelCurve);
        FV_P (cShape, curveShape); FV_P (cDur, curveDuration); FV_P (cSync, curveSync); FV_P (cNote, curveNote);
        FV_P (cTens, curveTension); FV_P (cRetrig, curveRetrig); FV_P (cGuard, curveGuard); FV_P (cHyst, curveHyst);
        FV_P (gOn, gateOn); FV_P (gThr, gateThresh); FV_P (gHyst, gateHyst); FV_P (gAtt, gateAttack);
        FV_P (gHold, gateHold); FV_P (gHoldSync, gateHoldSync); FV_P (gHoldNote, gateHoldNote);
        FV_P (gRel, gateRelease); FV_P (gFloor, gateFloor); FV_P (gCurve, gateRelCurve);
        FV_P (eqOnP, eqOn); FV_P (eqPos, eqPosition);
        FV_P (hpOn, hpfOn); FV_P (hpF, hpfFreq); FV_P (hpS, hpfSlope);
        FV_P (lpOn, lpfOn); FV_P (lpF, lpfFreq); FV_P (lpS, lpfSlope);
        FV_P (lsOnP, lsOn); FV_P (lsF, lsFreq); FV_P (lsG, lsGain); FV_P (lsQP, lsQ);
        FV_P (hsOnP, hsOn); FV_P (hsF, hsFreq); FV_P (hsG, hsGain); FV_P (hsQP, hsQ);
        FV_P (b1OnP, b1On); FV_P (b1F, b1Freq); FV_P (b1G, b1Gain); FV_P (b1QP, b1Q);
        FV_P (b2OnP, b2On); FV_P (b2F, b2Freq); FV_P (b2G, b2Gain); FV_P (b2QP, b2Q);
        FV_P (wid, width); FV_P (monoOn, monoBelowOn); FV_P (mono, monoBelow); FV_P (bal, balance);
        FV_P (mixP, mix); FV_P (dry, dryLevel); FV_P (wet, wetLevel); FV_P (solo, wetSolo);
        FV_P (out, outGain); FV_P (byp, bypass);
        #undef FV_P
    }

    std::atomic<float> *q, *pre, *preSync, *preNote, *keyHp, *keyLp, *det, *rms, *look, *listen,
        *dOn, *dMode, *dDepth, *dThr, *dAtt, *dRel, *dAmt, *dCurve, *cShape, *cDur, *cSync, *cNote,
        *cTens, *cRetrig, *cGuard, *cHyst, *gOn, *gThr, *gHyst, *gAtt, *gHold, *gHoldSync, *gHoldNote,
        *gRel, *gFloor, *gCurve, *eqOnP, *eqPos, *hpOn, *hpF, *hpS, *lpOn, *lpF, *lpS,
        *lsOnP, *lsF, *lsG, *lsQP, *hsOnP, *hsF, *hsG, *hsQP, *b1OnP, *b1F, *b1G, *b1QP,
        *b2OnP, *b2F, *b2G, *b2QP, *wid, *monoOn, *mono, *bal, *mixP, *dry, *wet, *solo, *out, *byp;
};

FreddyVerbProcessor::FreddyVerbProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "FreddyVerb", createParameterLayout())
{
    cacheParameterPointers();
    irManager = std::make_unique<IRManager> (apvts, engine, irLibrary);
    presetManager = std::make_unique<PresetManager> (apvts, *irManager);
    for (auto* id : { pid::quality, pid::lookahead })
        apvts.addParameterListener (id, this);
}

FreddyVerbProcessor::~FreddyVerbProcessor()
{
    for (auto* id : { pid::quality, pid::lookahead })
        apvts.removeParameterListener (id, this);
    cancelPendingUpdate();
    presetManager.reset();
    irManager.reset();
}

void FreddyVerbProcessor::cacheParameterPointers() { ptrs = std::make_unique<Ptrs> (apvts); }

bool FreddyVerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void FreddyVerbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());
    irManager->prepare (sampleRate, samplesPerBlock, isNonRealtime());
    updateLatency();
}

static double divisionMs (const std::vector<choices::NoteDiv>& divs, float idx, double bpm)
{
    const auto i = (size_t) std::clamp ((int) idx, 0, (int) divs.size() - 1);
    return divs[i].beats * 60000.0 / std::max (20.0, bpm);
}

dsp::EngineParams FreddyVerbProcessor::snapshotParams() const noexcept
{
    auto& P = *ptrs;
    dsp::EngineParams e;
    auto f = [] (std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    auto b = [&] (std::atomic<float>* a) { return f (a) > 0.5f; };
    auto i = [&] (std::atomic<float>* a) { return (int) std::lround (f (a)); };
    const double bpm = engine.bpm;

    e.quality = i (P.q);
    e.predelayMs = b (P.preSync) ? (float) divisionMs (choices::shortDivisions(), f (P.preNote), bpm) : f (P.pre);
    e.predelayMs = std::min (e.predelayMs, 250.0f);
    e.keyHpfHz = f (P.keyHp); e.keyLpfHz = f (P.keyLp); e.detMode = i (P.det); e.rmsMs = f (P.rms);
    e.lookaheadMs = f (P.look); e.keyListen = b (P.listen);
    e.duckOn = b (P.dOn); e.duckMode = i (P.dMode); e.duckDepthDb = f (P.dDepth); e.duckThreshDb = f (P.dThr);
    e.duckAttackMs = f (P.dAtt); e.duckReleaseMs = f (P.dRel); e.duckAmount = f (P.dAmt); e.duckRelCurve = i (P.dCurve);
    e.curveShape = i (P.cShape);
    e.curveDurMs = b (P.cSync) ? (float) divisionMs (choices::longDivisions(), f (P.cNote), bpm) : f (P.cDur);
    e.curveTension = f (P.cTens); e.curveRetrig = i (P.cRetrig); e.curveGuardMs = f (P.cGuard); e.curveHystDb = f (P.cHyst);
    e.gateOn = b (P.gOn); e.gateThreshDb = f (P.gThr); e.gateHystDb = f (P.gHyst); e.gateAttackMs = f (P.gAtt);
    e.gateHoldMs = b (P.gHoldSync) ? (float) divisionMs (choices::shortDivisions(), f (P.gHoldNote), bpm) : f (P.gHold);
    e.gateReleaseMs = f (P.gRel); e.gateFloorDb = f (P.gFloor); e.gateRelCurve = i (P.gCurve);
    e.eqOn = b (P.eqOnP); e.eqPosition = i (P.eqPos);
    e.hpfOn = b (P.hpOn); e.hpfHz = f (P.hpF); e.hpfSlope = i (P.hpS);
    e.lpfOn = b (P.lpOn); e.lpfHz = f (P.lpF); e.lpfSlope = i (P.lpS);
    e.lsOn = b (P.lsOnP); e.lsHz = f (P.lsF); e.lsGainDb = f (P.lsG); e.lsQ = f (P.lsQP);
    e.hsOn = b (P.hsOnP); e.hsHz = f (P.hsF); e.hsGainDb = f (P.hsG); e.hsQ = f (P.hsQP);
    e.b1On = b (P.b1OnP); e.b1Hz = f (P.b1F); e.b1GainDb = f (P.b1G); e.b1Q = f (P.b1QP);
    e.b2On = b (P.b2OnP); e.b2Hz = f (P.b2F); e.b2GainDb = f (P.b2G); e.b2Q = f (P.b2QP);
    e.widthPct = f (P.wid); e.monoBelowOn = b (P.monoOn); e.monoBelowHz = f (P.mono); e.balance = f (P.bal);
    e.mixPct = f (P.mixP); e.dryDb = f (P.dry); e.wetDb = f (P.wet); e.wetSolo = b (P.solo);
    e.outDb = f (P.out); e.bypass = b (P.byp);
    return e;
}

void FreddyVerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = std::max (1, getTotalNumInputChannels());
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                engine.bpm = *bpm;

    auto params = snapshotParams();
    const float* in[2] { buffer.getReadPointer (0), buffer.getReadPointer (std::min (1, numIn - 1)) };
    float* out[2] { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    engine.process (in, numIn, out, n, params);

    lastProcessMs.store (juce::Time::currentTimeMillis(), std::memory_order_relaxed);
}

void FreddyVerbProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Host-side bypass: keep the same latency and crossfade to the compensated dry.
    const int numIn = std::max (1, getTotalNumInputChannels());
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0) return;
    auto params = snapshotParams();
    params.bypass = true;
    const float* in[2] { buffer.getReadPointer (0), buffer.getReadPointer (std::min (1, numIn - 1)) };
    float* out[2] { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    engine.process (in, numIn, out, buffer.getNumSamples(), params);
}

juce::AudioProcessorParameter* FreddyVerbProcessor::getBypassParameter() const
{
    return apvts.getParameter (pid::bypass);
}

bool FreddyVerbProcessor::isAudioRunning() const noexcept
{
    return juce::Time::currentTimeMillis() - lastProcessMs.load (std::memory_order_relaxed) < 250;
}

void FreddyVerbProcessor::parameterChanged (const juce::String&, float) { triggerAsyncUpdate(); }
void FreddyVerbProcessor::handleAsyncUpdate() { updateLatency(); }

void FreddyVerbProcessor::updateLatency()
{
    const auto p = snapshotParams();
    const int lat = engine.computeLatency (p);
    if (lat != getLatencySamples())
        setLatencySamples (lat);
}

// ---------------------------------------------------------------------------
// State: versioned ValueTree -> XML -> binary
// ---------------------------------------------------------------------------
void FreddyVerbProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("FreddyVerbState");
    root.setProperty ("version", kStateVersion, nullptr);
    root.setProperty ("pluginVersion", JucePlugin_VersionString, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (uiState.createCopy(), nullptr);
    root.appendChild (irManager->getState(), nullptr);
    root.setProperty ("presetName", presetManager->getCurrentName(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void FreddyVerbProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("FreddyVerbState")) return;

    const int version = root.getProperty ("version", 0);
    juce::ignoreUnused (version); // migrations go here when kStateVersion > 1

    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid())
    {
        apvts.replaceState (params);
        // replaceState only notifies parameters whose stored value *changed* in the tree.
        // A bool/choice parameter holding e.g. 0.85 (= "on") stores 1, so restoring 1
        // would not update it. Push every saved value explicitly.
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto child = params.getChildWithProperty ("id", rp->getParameterID());
                if (child.isValid() && child.hasProperty ("value"))
                    rp->setValueNotifyingHost (rp->convertTo0to1 ((float) (double) child.getProperty ("value")));
            }
    }
    auto ui = root.getChildWithName ("UI");
    if (ui.isValid())
        uiState.copyPropertiesAndChildrenFrom (ui, nullptr);
    irManager->setState (root.getChildWithName ("IR"));
    if (root.hasProperty ("presetName"))
        presetManager->markLoaded (root.getProperty ("presetName").toString());
    updateLatency();
}

juce::AudioProcessorEditor* FreddyVerbProcessor::createEditor()
{
    return new FreddyVerbEditor (*this);
}
} // namespace fv

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new fv::FreddyVerbProcessor();
}
