#pragma once

// THE single place where every parameter / control is defined.
// Everything else (APVTS layout, engine cache, GUI attachments, tests) reads this table.

#include <array>
#include <string_view>
#include <juce_audio_processors/juce_audio_processors.h>

namespace queen
{
enum class Kind { Float, Choice, Bool };

struct ParamDef
{
    std::string_view id, name;
    Kind kind;
    float min, max, def; // Choice: def is the default index; Bool: 0/1
    float skew;          // NormalisableRange skew (1 = linear)
    std::string_view choices; // "a|b|c" for Choice
    std::string_view group;
    bool onPanel = true; // false = no registry control (host/settings only)
    float interval = 0.0f;
};

constexpr std::string_view kRatios = "0.5|1|2|3|4|5|6|7|8|10|12|16";
constexpr std::array<float, 12> kRatioValues { 0.5f, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 16 };

#define QUEEN_OP(n, lvl, fb)                                                                         \
    ParamDef { "op" #n "_ratio", "Op" #n " Ratio", Kind::Choice, 0, 11, 1, 1, kRatios, "FM OPERATOR " #n }, \
    ParamDef { "op" #n "_level", "Op" #n " Level", Kind::Float, 0, 1, lvl, 1, {}, "FM OPERATOR " #n },        \
    ParamDef { "op" #n "_detune", "Op" #n " Detune", Kind::Float, -50, 50, 0, 1, {}, "FM OPERATOR " #n },     \
    ParamDef { "op" #n "_feedback", "Op" #n " Feedback", Kind::Float, 0, 1, fb, 1, {}, "FM OPERATOR " #n },   \
    ParamDef { "op" #n "_velocity", "Op" #n " Velocity", Kind::Float, 0, 1, 0.5f, 1, {}, "FM OPERATOR " #n }

inline constexpr std::array kParams {
    QUEEN_OP (1, 1.0f, 0.0f),
    QUEEN_OP (2, 0.66f, 0.0f),
    QUEEN_OP (3, 0.9f, 0.0f),
    QUEEN_OP (4, 0.5f, 0.3f),
    ParamDef { "fm_algorithm", "FM Algorithm", Kind::Choice, 0, 7, 4, 1, "1|2|3|4|5|6|7|8", "FM ALGORITHM" },

    ParamDef { "volume", "Volume", Kind::Float, 0, 1, 0.75f, 1, {}, "CONTROLLERS" },
    ParamDef { "bend_sens_dco", "Bend Sens DCO", Kind::Float, 0, 12, 2, 1, {}, "CONTROLLERS", true, 1.0f },
    ParamDef { "bend_sens_vcf", "Bend Sens VCF", Kind::Float, 0, 1, 0, 1, {}, "CONTROLLERS" },
    ParamDef { "octave", "Octave", Kind::Choice, 0, 2, 1, 1, "down|norm|up", "CONTROLLERS" },
    ParamDef { "lfo_trigger", "LFO Trigger", Kind::Bool, 0, 1, 0, 1, {}, "CONTROLLERS" },

    ParamDef { "arp_on", "Arp On", Kind::Bool, 0, 1, 0, 1, {}, "ARPEGGIO" },
    ParamDef { "arp_mode", "Arp Mode", Kind::Choice, 0, 2, 0, 1, "up|updown|down", "ARPEGGIO" },
    ParamDef { "arp_range", "Arp Range", Kind::Choice, 0, 2, 0, 1, "1|2|3", "ARPEGGIO" },
    ParamDef { "arp_rate", "Arp Rate", Kind::Float, 1.5f, 50, 8, 0.3623f, {}, "ARPEGGIO" },
    ParamDef { "arp_sync", "Arp Sync", Kind::Bool, 0, 1, 0, 1, {}, "ARPEGGIO", false },
    ParamDef { "hold", "Hold", Kind::Bool, 0, 1, 0, 1, {}, "ARPEGGIO" },
    ParamDef { "key_transpose", "Key Transpose", Kind::Bool, 0, 1, 0, 1, {}, "ARPEGGIO" },

    ParamDef { "lfo_rate", "LFO Rate", Kind::Float, 0.3f, 20, 4, 0.3129f, {}, "LFO" },
    ParamDef { "lfo_delay", "LFO Delay", Kind::Float, 0, 2, 0, 1, {}, "LFO" },
    ParamDef { "lfo_trig_mode", "LFO Trig Mode", Kind::Choice, 0, 1, 0, 1, "auto|man", "LFO" },

    ParamDef { "dco_lfo", "DCO LFO", Kind::Float, 0, 1, 0, 1, {}, "DCO" },
    ParamDef { "dco_pwm", "DCO PWM", Kind::Float, 0, 1, 0.5f, 1, {}, "DCO" },
    ParamDef { "pwm_mode", "PWM Mode", Kind::Choice, 0, 2, 1, 1, "env|man|lfo", "DCO" },
    ParamDef { "dco_range", "DCO Range", Kind::Choice, 0, 2, 1, 1, "16|8|4", "DCO" },
    ParamDef { "wave_pulse", "Pulse", Kind::Bool, 0, 1, 1, 1, {}, "DCO" },
    ParamDef { "wave_saw", "Saw", Kind::Bool, 0, 1, 1, 1, {}, "DCO" },
    ParamDef { "wave_sub", "Sub", Kind::Bool, 0, 1, 0, 1, {}, "DCO" },
    ParamDef { "sub_level", "Sub Level", Kind::Float, 0, 1, 0.6f, 1, {}, "DCO" },
    ParamDef { "noise_level", "Noise Level", Kind::Float, 0, 1, 0, 1, {}, "DCO" },

    ParamDef { "hpf", "HPF", Kind::Choice, 0, 3, 1, 1, "0|1|2|3", "HPF" },

    ParamDef { "vcf_freq", "VCF Freq", Kind::Float, 0, 1, 0.55f, 1, {}, "VCF" },
    ParamDef { "vcf_res", "VCF Res", Kind::Float, 0, 1, 0.3f, 1, {}, "VCF" },
    ParamDef { "vcf_env", "VCF Env", Kind::Float, 0, 1, 0.6f, 1, {}, "VCF" },
    ParamDef { "vcf_env_polarity", "VCF Env Polarity", Kind::Choice, 0, 1, 0, 1, "pos|neg", "VCF" },
    ParamDef { "vcf_lfo", "VCF LFO", Kind::Float, 0, 1, 0.2f, 1, {}, "VCF" },
    ParamDef { "vcf_kybd", "VCF Kybd", Kind::Float, 0, 1, 0.5f, 1, {}, "VCF" },

    ParamDef { "vca_level", "VCA Level", Kind::Float, 0, 1, 0.7f, 1, {}, "VCA" },
    ParamDef { "vca_mode", "VCA Mode", Kind::Choice, 0, 1, 0, 1, "env|gate", "VCA" },

    ParamDef { "attack", "Attack", Kind::Float, 0.001f, 3, 0.01f, 0.1724f, {}, "ENV" },
    ParamDef { "decay", "Decay", Kind::Float, 0.002f, 12, 0.5f, 0.1589f, {}, "ENV" },
    ParamDef { "sustain", "Sustain", Kind::Float, 0, 1, 0.6f, 1, {}, "ENV" },
    ParamDef { "release", "Release", Kind::Float, 0.002f, 12, 0.3f, 0.1589f, {}, "ENV" },

    ParamDef { "chorus_1", "Chorus I", Kind::Bool, 0, 1, 1, 1, {}, "CHORUS" },
    ParamDef { "chorus_2", "Chorus II", Kind::Bool, 0, 1, 0, 1, {}, "CHORUS" },
    ParamDef { "tune", "Tune", Kind::Float, -50, 50, 0, 1, {}, "CHORUS" },
};
#undef QUEEN_OP

constexpr int kNumParams = (int) kParams.size();

constexpr int indexOf (std::string_view id)
{
    for (int i = 0; i < kNumParams; ++i)
        if (kParams[(size_t) i].id == id)
            return i;
    return -1;
}

constexpr const ParamDef& def (std::string_view id) { return kParams[(size_t) indexOf (id)]; }

inline juce::String toString (std::string_view s) { return juce::String (s.data(), s.size()); }

inline juce::StringArray choicesOf (const ParamDef& d)
{
    return juce::StringArray::fromTokens (toString (d.choices), "|", {});
}

inline juce::NormalisableRange<float> rangeOf (const ParamDef& d)
{
    juce::NormalisableRange<float> r (d.min, d.max, d.interval);
    r.skew = d.skew;
    return r;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    std::map<juce::String, juce::AudioProcessorParameterGroup*> groups;
    std::vector<std::unique_ptr<juce::AudioProcessorParameterGroup>> owned;

    for (const auto& d : kParams)
    {
        auto g = toString (d.group);
        if (! groups.contains (g))
        {
            owned.push_back (std::make_unique<juce::AudioProcessorParameterGroup> (g.removeCharacters (" ").toLowerCase(), g, "|"));
            groups[g] = owned.back().get();
        }
        const juce::ParameterID pid { toString (d.id), 1 };
        std::unique_ptr<juce::RangedAudioParameter> p;
        switch (d.kind)
        {
            case Kind::Float:
                p = std::make_unique<juce::AudioParameterFloat> (pid, toString (d.name), rangeOf (d), d.def);
                break;
            case Kind::Choice:
                p = std::make_unique<juce::AudioParameterChoice> (pid, toString (d.name), choicesOf (d), (int) d.def);
                break;
            case Kind::Bool:
                p = std::make_unique<juce::AudioParameterBool> (pid, toString (d.name), d.def > 0.5f);
                break;
        }
        groups[g]->addChild (std::move (p));
    }
    for (auto& g : owned)
        layout.add (std::move (g));
    return layout;
}
} // namespace queen
