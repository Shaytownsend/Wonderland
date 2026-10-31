#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fv
{
/** Every automatable parameter ID in one place. Never rename an ID once shipped:
    IDs are what hosts store in automation and what presets reference. */
namespace pid
{
    // Reverb / IR shaping
    inline constexpr const char* quality       = "quality";
    inline constexpr const char* predelay      = "predelay_ms";
    inline constexpr const char* predelaySync  = "predelay_sync";
    inline constexpr const char* predelayNote  = "predelay_note";
    inline constexpr const char* irStart       = "ir_start_ms";
    inline constexpr const char* irLength      = "ir_length_pct";
    inline constexpr const char* irDecay       = "ir_decay_pct";
    inline constexpr const char* irSize        = "ir_size_pct";
    inline constexpr const char* irXover       = "ir_xover_ms";
    inline constexpr const char* irEarly       = "ir_early_db";
    inline constexpr const char* irLate        = "ir_late_db";
    inline constexpr const char* irDamping     = "ir_damping_pct";
    inline constexpr const char* irDampFreq    = "ir_damp_freq";
    inline constexpr const char* irReverse     = "ir_reverse";
    inline constexpr const char* irNormalize   = "ir_normalize";

    // Shared detector
    inline constexpr const char* keyHpf        = "key_hpf";
    inline constexpr const char* keyLpf        = "key_lpf";
    inline constexpr const char* detMode       = "det_mode";
    inline constexpr const char* detRms        = "det_rms_ms";
    inline constexpr const char* lookahead     = "lookahead_ms";
    inline constexpr const char* keyListen     = "key_listen";

    // Ducker
    inline constexpr const char* duckOn        = "duck_on";
    inline constexpr const char* duckMode      = "duck_mode";
    inline constexpr const char* duckDepth     = "duck_depth_db";
    inline constexpr const char* duckThresh    = "duck_threshold_db";
    inline constexpr const char* duckAttack    = "duck_attack_ms";
    inline constexpr const char* duckRelease   = "duck_release_ms";
    inline constexpr const char* duckAmount    = "duck_amount";
    inline constexpr const char* duckRelCurve  = "duck_rel_curve";
    inline constexpr const char* curveShape    = "curve_shape";
    inline constexpr const char* curveDuration = "curve_duration_ms";
    inline constexpr const char* curveSync     = "curve_sync";
    inline constexpr const char* curveNote     = "curve_note";
    inline constexpr const char* curveTension  = "curve_tension";
    inline constexpr const char* curveRetrig   = "curve_retrigger";
    inline constexpr const char* curveGuard    = "curve_guard_ms";
    inline constexpr const char* curveHyst     = "curve_hyst_db";

    // Gate
    inline constexpr const char* gateOn        = "gate_on";
    inline constexpr const char* gateThresh    = "gate_threshold_db";
    inline constexpr const char* gateHyst      = "gate_hyst_db";
    inline constexpr const char* gateAttack    = "gate_attack_ms";
    inline constexpr const char* gateHold      = "gate_hold_ms";
    inline constexpr const char* gateHoldSync  = "gate_hold_sync";
    inline constexpr const char* gateHoldNote  = "gate_hold_note";
    inline constexpr const char* gateRelease   = "gate_release_ms";
    inline constexpr const char* gateFloor     = "gate_floor_db";
    inline constexpr const char* gateRelCurve  = "gate_rel_curve";

    // Wet EQ
    inline constexpr const char* eqOn          = "eq_on";
    inline constexpr const char* eqPosition    = "eq_position";
    inline constexpr const char* hpfOn         = "eq_hpf_on";
    inline constexpr const char* hpfFreq       = "eq_hpf_freq";
    inline constexpr const char* hpfSlope      = "eq_hpf_slope";
    inline constexpr const char* lpfOn         = "eq_lpf_on";
    inline constexpr const char* lpfFreq       = "eq_lpf_freq";
    inline constexpr const char* lpfSlope      = "eq_lpf_slope";
    inline constexpr const char* lsOn          = "eq_ls_on";
    inline constexpr const char* lsFreq        = "eq_ls_freq";
    inline constexpr const char* lsGain        = "eq_ls_gain";
    inline constexpr const char* lsQ           = "eq_ls_q";
    inline constexpr const char* hsOn          = "eq_hs_on";
    inline constexpr const char* hsFreq        = "eq_hs_freq";
    inline constexpr const char* hsGain        = "eq_hs_gain";
    inline constexpr const char* hsQ           = "eq_hs_q";
    inline constexpr const char* b1On          = "eq_b1_on";
    inline constexpr const char* b1Freq        = "eq_b1_freq";
    inline constexpr const char* b1Gain        = "eq_b1_gain";
    inline constexpr const char* b1Q           = "eq_b1_q";
    inline constexpr const char* b2On          = "eq_b2_on";
    inline constexpr const char* b2Freq        = "eq_b2_freq";
    inline constexpr const char* b2Gain        = "eq_b2_gain";
    inline constexpr const char* b2Q           = "eq_b2_q";

    // Space
    inline constexpr const char* width         = "width_pct";
    inline constexpr const char* monoBelowOn   = "mono_below_on";
    inline constexpr const char* monoBelow     = "mono_below_hz";
    inline constexpr const char* balance       = "wet_balance";

    // Mix / output
    inline constexpr const char* mix           = "mix_pct";
    inline constexpr const char* dryLevel      = "dry_level_db";
    inline constexpr const char* wetLevel      = "wet_level_db";
    inline constexpr const char* mixLock       = "mix_lock";
    inline constexpr const char* wetSolo       = "wet_solo";
    inline constexpr const char* outGain       = "output_gain_db";
    inline constexpr const char* bypass        = "bypass";
}

/** Choice lists shared by DSP and UI. */
namespace choices
{
    enum Quality       { zeroLatency = 0, efficient };
    enum DetMode       { peak = 0, rms };
    enum DuckMode      { follow = 0, curve };
    enum ReleaseCurve  { linear = 0, exponential, logarithmic, sCurve };
    enum CurveShape    { linearRamp = 0, expo, loga, sShape, holdRamp, pump, swell, numCurveShapes };
    enum Retrigger     { restart = 0, continueLevel };
    enum EqPosition    { pre = 0, post };
    enum Slope         { s12 = 0, s24, s48 };

    juce::StringArray qualityNames();
    juce::StringArray detModeNames();
    juce::StringArray duckModeNames();
    juce::StringArray duckReleaseCurveNames();
    juce::StringArray gateReleaseCurveNames();
    juce::StringArray curveShapeNames();
    juce::StringArray retriggerNames();
    juce::StringArray eqPositionNames();
    juce::StringArray slopeNames();

    /** Tempo divisions. */
    struct NoteDiv { const char* name; double beats; };
    /** 1/64 .. 1/4 (straight, dotted, triplet) - used for pre-delay and gate hold. */
    const std::vector<NoteDiv>& shortDivisions();
    /** 1/64 .. 1 bar - used for curve duration. */
    const std::vector<NoteDiv>& longDivisions();
    juce::StringArray namesOf (const std::vector<NoteDiv>&);
}

/** Values below these display as -inf / FULL. */
inline constexpr float kDuckFullDb   = -60.5f;
inline constexpr float kGateFloorInf = -80.5f;
inline constexpr float kLevelInfDb   = -60.5f;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Ordered list of IDs that make up the "mix" group (protected by Mix Lock). */
const juce::StringArray& mixLockedIDs();

/** Human-facing info for UI tooltips and the LCD readout. */
struct ParamInfo
{
    juce::String id, shortName, tooltip, section;
};
const std::vector<ParamInfo>& parameterInfo();
const ParamInfo* findParamInfo (const juce::String& id);

} // namespace fv
