#include "Parameters.h"

namespace fv
{
namespace choices
{
    juce::StringArray qualityNames()          { return { "Zero Latency", "Efficient" }; }
    juce::StringArray detModeNames()          { return { "Peak", "RMS" }; }
    juce::StringArray duckModeNames()         { return { "Follow", "Curve" }; }
    juce::StringArray duckReleaseCurveNames() { return { "Linear", "Exponential", "Logarithmic", "S-Curve" }; }
    juce::StringArray gateReleaseCurveNames() { return { "Linear", "Exponential", "Logarithmic" }; }
    juce::StringArray curveShapeNames()       { return { "Linear Ramp", "Exponential", "Logarithmic", "S-Curve",
                                                         "Hold-Ramp", "Pump", "Swell" }; }
    juce::StringArray retriggerNames()        { return { "Restart", "Continue" }; }
    juce::StringArray eqPositionNames()       { return { "Pre", "Post" }; }
    juce::StringArray slopeNames()            { return { "12 dB/oct", "24 dB/oct", "48 dB/oct" }; }

    const std::vector<NoteDiv>& shortDivisions()
    {
        static const std::vector<NoteDiv> d {
            { "1/64T", 1.0 / 24.0 }, { "1/64", 1.0 / 16.0 }, { "1/64D", 1.5 / 16.0 },
            { "1/32T", 1.0 / 12.0 }, { "1/32", 1.0 / 8.0 },  { "1/32D", 1.5 / 8.0 },
            { "1/16T", 1.0 / 6.0 },  { "1/16", 1.0 / 4.0 },  { "1/16D", 1.5 / 4.0 },
            { "1/8T",  1.0 / 3.0 },  { "1/8",  1.0 / 2.0 },  { "1/8D",  1.5 / 2.0 },
            { "1/4T",  2.0 / 3.0 },  { "1/4",  1.0 },        { "1/4D",  1.5 } };
        return d;
    }

    const std::vector<NoteDiv>& longDivisions()
    {
        static const std::vector<NoteDiv> d = []
        {
            auto v = shortDivisions();
            v.push_back ({ "1/2T", 4.0 / 3.0 });
            v.push_back ({ "1/2",  2.0 });
            v.push_back ({ "1/2D", 3.0 });
            v.push_back ({ "1 Bar", 4.0 });
            return v;
        }();
        return d;
    }

    juce::StringArray namesOf (const std::vector<NoteDiv>& v)
    {
        juce::StringArray s;
        for (auto& d : v) s.add (d.name);
        return s;
    }
}

namespace
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using Attr = juce::AudioParameterFloatAttributes;

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float interval = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, interval);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::String fmtMs (float v, int)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " s";
        if (v < 10.0f)    return juce::String (v, 2) + " ms";
        if (v < 100.0f)   return juce::String (v, 1) + " ms";
        return juce::String (juce::roundToInt (v)) + " ms";
    }
    juce::String fmtHz (float v, int)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
        return juce::String (juce::roundToInt (v)) + " Hz";
    }
    juce::String fmtDb (float v, int)       { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String fmtPct (float v, int)      { return juce::String (juce::roundToInt (v)) + " %"; }

    float parseNumber (const juce::String& t)
    {
        auto s = t.trim().toLowerCase();
        auto v = s.getFloatValue();
        if (s.endsWith ("khz") || s.endsWith ("k")) v *= 1000.0f;
        else if (s.endsWith (" s") || (s.endsWith ("s") && ! s.endsWith ("ms"))) v *= 1000.0f;
        return v;
    }

    Attr msAttr()  { return Attr().withLabel ("ms").withStringFromValueFunction (fmtMs).withValueFromStringFunction (parseNumber); }
    Attr hzAttr()  { return Attr().withLabel ("Hz").withStringFromValueFunction (fmtHz).withValueFromStringFunction (parseNumber); }
    Attr dbAttr()  { return Attr().withLabel ("dB").withStringFromValueFunction (fmtDb).withValueFromStringFunction (parseNumber); }
    Attr pctAttr() { return Attr().withLabel ("%").withStringFromValueFunction (fmtPct).withValueFromStringFunction (parseNumber); }

    Attr dbInfAttr (float infBelow, const char* infText)
    {
        return Attr().withLabel ("dB")
            .withStringFromValueFunction ([infBelow, infText] (float v, int) -> juce::String
            {
                if (v <= infBelow) return infText;
                return fmtDb (v, 0);
            })
            .withValueFromStringFunction ([infBelow] (const juce::String& t)
            {
                auto s = t.trim().toLowerCase();
                if (s.contains ("inf") || s.contains ("full") || s.contains ("off")) return infBelow - 0.5f;
                return s.getFloatValue();
            });
    }

    juce::ParameterID P (const char* id) { return { id, 1 }; }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace choices;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto reverb = std::make_unique<juce::AudioProcessorParameterGroup> ("reverb", "Reverb", "|");
    reverb->addChild (std::make_unique<APC> (P (pid::quality), "Quality", qualityNames(), 0));
    reverb->addChild (std::make_unique<APF> (P (pid::predelay), "Pre-Delay", skewed (0.0f, 250.0f, 40.0f), 8.0f, msAttr()));
    reverb->addChild (std::make_unique<APB> (P (pid::predelaySync), "Pre-Delay Sync", false));
    reverb->addChild (std::make_unique<APC> (P (pid::predelayNote), "Pre-Delay Note", namesOf (shortDivisions()), 4));
    reverb->addChild (std::make_unique<APF> (P (pid::irStart), "IR Start", skewed (0.0f, 100.0f, 20.0f), 0.0f, msAttr()));
    reverb->addChild (std::make_unique<APF> (P (pid::irLength), "IR Length", juce::NormalisableRange<float> (10.0f, 100.0f), 100.0f, pctAttr()));
    reverb->addChild (std::make_unique<APF> (P (pid::irDecay), "Decay", skewed (25.0f, 200.0f, 100.0f), 100.0f, pctAttr()));
    reverb->addChild (std::make_unique<APF> (P (pid::irSize), "Size", skewed (50.0f, 200.0f, 100.0f), 100.0f, pctAttr()));
    reverb->addChild (std::make_unique<APF> (P (pid::irXover), "Early/Late X-Over", skewed (10.0f, 300.0f, 80.0f), 80.0f, msAttr()));
    reverb->addChild (std::make_unique<APF> (P (pid::irEarly), "Early Level", juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f, dbAttr()));
    reverb->addChild (std::make_unique<APF> (P (pid::irLate), "Late Level", juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f, dbAttr()));
    reverb->addChild (std::make_unique<APF> (P (pid::irDamping), "Damping", juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f, pctAttr()));
    reverb->addChild (std::make_unique<APF> (P (pid::irDampFreq), "Damping Freq", skewed (1000.0f, 16000.0f, 4000.0f), 5000.0f, hzAttr()));
    reverb->addChild (std::make_unique<APB> (P (pid::irReverse), "Reverse", false));
    reverb->addChild (std::make_unique<APB> (P (pid::irNormalize), "Normalize", true));
    layout.add (std::move (reverb));

    auto det = std::make_unique<juce::AudioProcessorParameterGroup> ("detector", "Detector", "|");
    det->addChild (std::make_unique<APF> (P (pid::keyHpf), "Key HPF", skewed (20.0f, 2000.0f, 200.0f), 20.0f, hzAttr()));
    det->addChild (std::make_unique<APF> (P (pid::keyLpf), "Key LPF", skewed (200.0f, 20000.0f, 2000.0f), 20000.0f, hzAttr()));
    det->addChild (std::make_unique<APC> (P (pid::detMode), "Detection", detModeNames(), 0));
    det->addChild (std::make_unique<APF> (P (pid::detRms), "RMS Window", skewed (1.0f, 50.0f, 10.0f), 10.0f, msAttr()));
    det->addChild (std::make_unique<APF> (P (pid::lookahead), "Lookahead", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 0.0f, msAttr()));
    det->addChild (std::make_unique<APB> (P (pid::keyListen), "Key Listen", false));
    layout.add (std::move (det));

    auto duck = std::make_unique<juce::AudioProcessorParameterGroup> ("duck", "Duck", "|");
    duck->addChild (std::make_unique<APB> (P (pid::duckOn), "Duck On", true));
    duck->addChild (std::make_unique<APC> (P (pid::duckMode), "Duck Mode", duckModeNames(), 0));
    duck->addChild (std::make_unique<APF> (P (pid::duckDepth), "Duck Depth", juce::NormalisableRange<float> (-61.0f, 0.0f), -18.0f, dbInfAttr (kDuckFullDb, "FULL")));
    duck->addChild (std::make_unique<APF> (P (pid::duckThresh), "Duck Threshold", juce::NormalisableRange<float> (-60.0f, 0.0f), -30.0f, dbAttr()));
    duck->addChild (std::make_unique<APF> (P (pid::duckAttack), "Duck Attack", skewed (0.1f, 50.0f, 5.0f), 2.0f, msAttr()));
    duck->addChild (std::make_unique<APF> (P (pid::duckRelease), "Bloom", skewed (5.0f, 2000.0f, 250.0f), 250.0f, msAttr()));
    duck->addChild (std::make_unique<APF> (P (pid::duckAmount), "Duck Amount", skewed (0.1f, 4.0f, 1.0f), 1.0f,
                     Attr().withLabel ("x").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + "x"; })));
    duck->addChild (std::make_unique<APC> (P (pid::duckRelCurve), "Bloom Curve", duckReleaseCurveNames(), 1));
    duck->addChild (std::make_unique<APC> (P (pid::curveShape), "Curve Shape", curveShapeNames(), 0));
    duck->addChild (std::make_unique<APF> (P (pid::curveDuration), "Curve Duration", skewed (10.0f, 2000.0f, 300.0f), 300.0f, msAttr()));
    duck->addChild (std::make_unique<APB> (P (pid::curveSync), "Curve Sync", false));
    duck->addChild (std::make_unique<APC> (P (pid::curveNote), "Curve Note", namesOf (longDivisions()), 10));
    duck->addChild (std::make_unique<APF> (P (pid::curveTension), "Curve Tension", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                     Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)); })));
    duck->addChild (std::make_unique<APC> (P (pid::curveRetrig), "Retrigger", retriggerNames(), 0));
    duck->addChild (std::make_unique<APF> (P (pid::curveGuard), "Retrigger Guard", skewed (5.0f, 500.0f, 50.0f), 30.0f, msAttr()));
    duck->addChild (std::make_unique<APF> (P (pid::curveHyst), "Trigger Hysteresis", juce::NormalisableRange<float> (0.0f, 12.0f), 3.0f, dbAttr()));
    layout.add (std::move (duck));

    auto gate = std::make_unique<juce::AudioProcessorParameterGroup> ("gate", "Gate", "|");
    gate->addChild (std::make_unique<APB> (P (pid::gateOn), "Gate On", true));
    gate->addChild (std::make_unique<APF> (P (pid::gateThresh), "Gate Threshold", juce::NormalisableRange<float> (-80.0f, 0.0f), -40.0f, dbAttr()));
    gate->addChild (std::make_unique<APF> (P (pid::gateHyst), "Gate Hysteresis", juce::NormalisableRange<float> (0.0f, 12.0f), 4.0f, dbAttr()));
    gate->addChild (std::make_unique<APF> (P (pid::gateAttack), "Gate Attack", skewed (0.05f, 50.0f, 2.0f), 1.0f, msAttr()));
    gate->addChild (std::make_unique<APF> (P (pid::gateHold), "Gate Hold", skewed (0.0f, 2000.0f, 200.0f), 150.0f, msAttr()));
    gate->addChild (std::make_unique<APB> (P (pid::gateHoldSync), "Gate Hold Sync", false));
    gate->addChild (std::make_unique<APC> (P (pid::gateHoldNote), "Gate Hold Note", namesOf (shortDivisions()), 10));
    gate->addChild (std::make_unique<APF> (P (pid::gateRelease), "Gate Release", skewed (5.0f, 5000.0f, 300.0f), 300.0f, msAttr()));
    gate->addChild (std::make_unique<APF> (P (pid::gateFloor), "Gate Floor", juce::NormalisableRange<float> (-81.0f, 0.0f), -81.0f, dbInfAttr (kGateFloorInf, "-inf dB")));
    gate->addChild (std::make_unique<APC> (P (pid::gateRelCurve), "Gate Curve", gateReleaseCurveNames(), 1));
    layout.add (std::move (gate));

    auto eq = std::make_unique<juce::AudioProcessorParameterGroup> ("tone", "Tone", "|");
    eq->addChild (std::make_unique<APB> (P (pid::eqOn), "EQ On", true));
    eq->addChild (std::make_unique<APC> (P (pid::eqPosition), "EQ Position", eqPositionNames(), 1));
    eq->addChild (std::make_unique<APB> (P (pid::hpfOn), "HPF On", true));
    eq->addChild (std::make_unique<APF> (P (pid::hpfFreq), "HPF Freq", skewed (20.0f, 2000.0f, 200.0f), 120.0f, hzAttr()));
    eq->addChild (std::make_unique<APC> (P (pid::hpfSlope), "HPF Slope", slopeNames(), 1));
    eq->addChild (std::make_unique<APB> (P (pid::lpfOn), "LPF On", true));
    eq->addChild (std::make_unique<APF> (P (pid::lpfFreq), "LPF Freq", skewed (1000.0f, 20000.0f, 6000.0f), 14000.0f, hzAttr()));
    eq->addChild (std::make_unique<APC> (P (pid::lpfSlope), "LPF Slope", slopeNames(), 0));
    eq->addChild (std::make_unique<APB> (P (pid::lsOn), "Low Shelf On", false));
    eq->addChild (std::make_unique<APF> (P (pid::lsFreq), "Low Shelf Freq", skewed (20.0f, 2000.0f, 200.0f), 200.0f, hzAttr()));
    eq->addChild (std::make_unique<APF> (P (pid::lsGain), "Low Shelf Gain", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f, dbAttr()));
    eq->addChild (std::make_unique<APF> (P (pid::lsQ), "Low Shelf Q", skewed (0.3f, 2.0f, 0.707f), 0.707f));
    eq->addChild (std::make_unique<APB> (P (pid::hsOn), "High Shelf On", false));
    eq->addChild (std::make_unique<APF> (P (pid::hsFreq), "High Shelf Freq", skewed (1000.0f, 20000.0f, 5000.0f), 6000.0f, hzAttr()));
    eq->addChild (std::make_unique<APF> (P (pid::hsGain), "High Shelf Gain", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f, dbAttr()));
    eq->addChild (std::make_unique<APF> (P (pid::hsQ), "High Shelf Q", skewed (0.3f, 2.0f, 0.707f), 0.707f));
    eq->addChild (std::make_unique<APB> (P (pid::b1On), "Bell 1 On", false));
    eq->addChild (std::make_unique<APF> (P (pid::b1Freq), "Bell 1 Freq", skewed (20.0f, 20000.0f, 1000.0f), 500.0f, hzAttr()));
    eq->addChild (std::make_unique<APF> (P (pid::b1Gain), "Bell 1 Gain", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f, dbAttr()));
    eq->addChild (std::make_unique<APF> (P (pid::b1Q), "Bell 1 Q", skewed (0.1f, 10.0f, 1.0f), 1.0f));
    eq->addChild (std::make_unique<APB> (P (pid::b2On), "Bell 2 On", false));
    eq->addChild (std::make_unique<APF> (P (pid::b2Freq), "Bell 2 Freq", skewed (20.0f, 20000.0f, 1000.0f), 3000.0f, hzAttr()));
    eq->addChild (std::make_unique<APF> (P (pid::b2Gain), "Bell 2 Gain", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f, dbAttr()));
    eq->addChild (std::make_unique<APF> (P (pid::b2Q), "Bell 2 Q", skewed (0.1f, 10.0f, 1.0f), 1.0f));
    layout.add (std::move (eq));

    auto space = std::make_unique<juce::AudioProcessorParameterGroup> ("space", "Space", "|");
    space->addChild (std::make_unique<APF> (P (pid::width), "Width", juce::NormalisableRange<float> (0.0f, 200.0f), 100.0f, pctAttr()));
    space->addChild (std::make_unique<APB> (P (pid::monoBelowOn), "Mono Below On", false));
    space->addChild (std::make_unique<APF> (P (pid::monoBelow), "Mono Below", skewed (20.0f, 500.0f, 120.0f), 120.0f, hzAttr()));
    space->addChild (std::make_unique<APF> (P (pid::balance), "Wet Balance", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f,
                     Attr().withStringFromValueFunction ([] (float v, int)
                     {
                         auto i = juce::roundToInt (v);
                         return i == 0 ? juce::String ("C") : (i < 0 ? "L" + juce::String (-i) : "R" + juce::String (i));
                     })));
    layout.add (std::move (space));

    auto mix = std::make_unique<juce::AudioProcessorParameterGroup> ("mix", "Mix", "|");
    mix->addChild (std::make_unique<APF> (P (pid::mix), "Mix", juce::NormalisableRange<float> (0.0f, 100.0f), 35.0f, pctAttr()));
    mix->addChild (std::make_unique<APF> (P (pid::dryLevel), "Dry Level", juce::NormalisableRange<float> (-61.0f, 6.0f), 0.0f, dbInfAttr (kLevelInfDb, "-inf dB")));
    mix->addChild (std::make_unique<APF> (P (pid::wetLevel), "Wet Level", juce::NormalisableRange<float> (-61.0f, 6.0f), 0.0f, dbInfAttr (kLevelInfDb, "-inf dB")));
    mix->addChild (std::make_unique<APB> (P (pid::mixLock), "Mix Lock", false));
    mix->addChild (std::make_unique<APB> (P (pid::wetSolo), "Wet Solo", false));
    mix->addChild (std::make_unique<APF> (P (pid::outGain), "Output Gain", juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f, dbAttr()));
    mix->addChild (std::make_unique<APB> (P (pid::bypass), "Bypass", false));
    layout.add (std::move (mix));

    return layout;
}

const juce::StringArray& mixLockedIDs()
{
    static const juce::StringArray ids { pid::mix };
    return ids;
}

const std::vector<ParamInfo>& parameterInfo()
{
    static const std::vector<ParamInfo> info {
        { pid::quality,      "QUALITY",  "Zero Latency: non-uniform partitioned convolution, 0 samples. Efficient: uniform partitions, adds latency.", "REVERB" },
        { pid::predelay,     "PREDLY",   "Delay before the reverb starts (0-250 ms).", "REVERB" },
        { pid::predelaySync, "SYNC",     "Lock pre-delay to the host tempo.", "REVERB" },
        { pid::predelayNote, "NOTE",     "Tempo-synced pre-delay division.", "REVERB" },
        { pid::irStart,      "START",    "Skip the first part of the impulse response.", "REVERB" },
        { pid::irLength,     "LENGTH",   "Truncate the impulse response with a smooth fade.", "REVERB" },
        { pid::irDecay,      "DECAY",    "Reshape the decay: below 100% shortens, above lengthens the apparent tail.", "REVERB" },
        { pid::irSize,       "SIZE",     "Time-stretch the IR by resampling (50-200%).", "REVERB" },
        { pid::irXover,      "X-OVER",   "Split point between early reflections and late tail.", "REVERB" },
        { pid::irEarly,      "EARLY",    "Gain of the early reflections.", "REVERB" },
        { pid::irLate,       "LATE",     "Gain of the late tail.", "REVERB" },
        { pid::irDamping,    "DAMP",     "Frequency-dependent decay: highs die faster over time.", "REVERB" },
        { pid::irDampFreq,   "DAMP F",   "Frequency above which damping takes effect.", "REVERB" },
        { pid::irReverse,    "REVERSE",  "Play the impulse response backwards.", "REVERB" },
        { pid::irNormalize,  "NORM",     "Loudness-normalize IRs so switching does not jump in level.", "REVERB" },
        { pid::keyHpf,       "KEY HP",   "High-pass the detector key (e.g. remove kick bleed).", "DETECT" },
        { pid::keyLpf,       "KEY LP",   "Low-pass the detector key.", "DETECT" },
        { pid::detMode,      "DETECT",   "Peak or RMS detection of the dry key.", "DETECT" },
        { pid::detRms,       "RMS WIN",  "RMS averaging window.", "DETECT" },
        { pid::lookahead,    "LOOKAHD",  "Delays the audio so ducking can react before the hit (adds latency).", "DETECT" },
        { pid::keyListen,    "LISTEN",   "Solo the filtered key signal.", "DETECT" },
        { pid::duckOn,       "DUCK",     "Enable the ducker.", "DUCK" },
        { pid::duckMode,     "MODE",     "Follow tracks the drum envelope; Curve fires a fixed shape per hit.", "DUCK" },
        { pid::duckDepth,    "DEPTH",    "Maximum reduction of the reverb while the drum hits.", "DUCK" },
        { pid::duckThresh,   "THRESH",   "Key level where ducking starts.", "DUCK" },
        { pid::duckAttack,   "ATTACK",   "How fast the reverb is pushed down.", "DUCK" },
        { pid::duckRelease,  "BLOOM",    "How fast the reverb comes back up after the hit.", "DUCK" },
        { pid::duckAmount,   "AMOUNT",   "dB of reduction per dB of key above threshold.", "DUCK" },
        { pid::duckRelCurve, "B.CURVE",  "Shape of the bloom (release).", "DUCK" },
        { pid::curveShape,   "SHAPE",    "Curve-mode gain shape fired on each hit.", "DUCK" },
        { pid::curveDuration,"DURATN",   "Length of the curve.", "DUCK" },
        { pid::curveSync,    "SYNC",     "Lock curve duration to the host tempo.", "DUCK" },
        { pid::curveNote,    "NOTE",     "Tempo-synced curve duration.", "DUCK" },
        { pid::curveTension, "TENSION",  "Bends the curve shape.", "DUCK" },
        { pid::curveRetrig,  "RETRIG",   "Restart the curve on each hit, or continue from the current level.", "DUCK" },
        { pid::curveGuard,   "GUARD",    "Minimum time between triggers.", "DUCK" },
        { pid::curveHyst,    "HYST",     "Key must fall this far below threshold to re-arm.", "DUCK" },
        { pid::gateOn,       "GATE",     "Enable the gate (off = plain ducked reverb).", "GATE" },
        { pid::gateThresh,   "THRESH",   "Key level that opens the gate.", "GATE" },
        { pid::gateHyst,     "HYST",     "Gate closes this far below threshold (prevents chatter).", "GATE" },
        { pid::gateAttack,   "ATTACK",   "Gate opening time.", "GATE" },
        { pid::gateHold,     "HOLD",     "How long the bloom lives after the drum drops below threshold.", "GATE" },
        { pid::gateHoldSync, "SYNC",     "Lock gate hold to the host tempo.", "GATE" },
        { pid::gateHoldNote, "NOTE",     "Tempo-synced gate hold.", "GATE" },
        { pid::gateRelease,  "RELEASE",  "How fast the tail fades out once the hold ends.", "GATE" },
        { pid::gateFloor,    "FLOOR",    "Level the gate closes to; above -inf keeps a little tail.", "GATE" },
        { pid::gateRelCurve, "CURVE",    "Shape of the gate release.", "GATE" },
        { pid::eqOn,         "EQ",       "Enable the wet EQ.", "TONE" },
        { pid::eqPosition,   "PRE/POST", "Pre: shape what feeds the IR. Post: shape the final tail.", "TONE" },
        { pid::hpfOn,        "HPF",      "Wet high-pass on/off.", "TONE" },
        { pid::hpfFreq,      "HPF",      "Wet high-pass frequency.", "TONE" },
        { pid::hpfSlope,     "HP SLP",   "Wet high-pass slope.", "TONE" },
        { pid::lpfOn,        "LPF",      "Wet low-pass on/off.", "TONE" },
        { pid::lpfFreq,      "LPF",      "Wet low-pass frequency.", "TONE" },
        { pid::lpfSlope,     "LP SLP",   "Wet low-pass slope.", "TONE" },
        { pid::lsOn,         "LO SHF",   "Low shelf on/off.", "TONE" },
        { pid::lsFreq,       "LS FRQ",   "Low shelf frequency.", "TONE" },
        { pid::lsGain,       "LS GAIN",  "Low shelf gain.", "TONE" },
        { pid::lsQ,          "LS Q",     "Low shelf Q.", "TONE" },
        { pid::hsOn,         "HI SHF",   "High shelf on/off.", "TONE" },
        { pid::hsFreq,       "HS FRQ",   "High shelf frequency.", "TONE" },
        { pid::hsGain,       "HS GAIN",  "High shelf gain.", "TONE" },
        { pid::hsQ,          "HS Q",     "High shelf Q.", "TONE" },
        { pid::b1On,         "BELL 1",   "Bell 1 on/off.", "TONE" },
        { pid::b1Freq,       "B1 FRQ",   "Bell 1 frequency.", "TONE" },
        { pid::b1Gain,       "B1 GAIN",  "Bell 1 gain.", "TONE" },
        { pid::b1Q,          "B1 Q",     "Bell 1 Q.", "TONE" },
        { pid::b2On,         "BELL 2",   "Bell 2 on/off.", "TONE" },
        { pid::b2Freq,       "B2 FRQ",   "Bell 2 frequency.", "TONE" },
        { pid::b2Gain,       "B2 GAIN",  "Bell 2 gain.", "TONE" },
        { pid::b2Q,          "B2 Q",     "Bell 2 Q.", "TONE" },
        { pid::width,        "WIDTH",    "Wet stereo width (0% mono - 200%).", "SPACE" },
        { pid::monoBelowOn,  "MONO LO",  "Make the wet mono below a frequency.", "SPACE" },
        { pid::monoBelow,    "MONO <",   "Mono-below crossover frequency (Linkwitz-Riley).", "SPACE" },
        { pid::balance,      "BAL",      "Wet balance / pan.", "SPACE" },
        { pid::mix,          "MIX",      "Equal-power dry/wet mix.", "MIX" },
        { pid::dryLevel,     "DRY",      "Dry level.", "MIX" },
        { pid::wetLevel,     "WET",      "Wet level.", "MIX" },
        { pid::mixLock,      "LOCK",     "Keep the current Mix when browsing presets.", "MIX" },
        { pid::wetSolo,      "SOLO",     "Hear the wet signal only.", "MIX" },
        { pid::outGain,      "OUTPUT",   "Output gain.", "MIX" },
        { pid::bypass,       "BYPASS",   "Click-free, latency-compensated bypass.", "MIX" },
    };
    return info;
}

const ParamInfo* findParamInfo (const juce::String& id)
{
    for (auto& p : parameterInfo())
        if (p.id == id) return &p;
    return nullptr;
}

} // namespace fv
