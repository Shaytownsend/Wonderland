#include "FmVoice.h"

namespace queen
{
void FmVoice::prepare (double sampleRate, int maxBlock)
{
    setCurrentPlaybackSampleRate (sampleRate);
    adsr.setSampleRate (sampleRate);
    filter.prepare (sampleRate, juce::jmax (maxBlock, maxChunk));
    gateStep = (float) (1.0 / (0.002 * sampleRate)); // 2 ms gate ramps
    lastHpf = -1;
}

void FmVoice::updateEnvelope()
{
    const auto& p = globals.p;
    juce::ADSR::Parameters np { p[QP ("attack")], p[QP ("decay")], p[QP ("sustain")], p[QP ("release")] };
    if (! juce::exactlyEqual (np.attack, adsrParams.attack) || ! juce::exactlyEqual (np.decay, adsrParams.decay)
        || ! juce::exactlyEqual (np.sustain, adsrParams.sustain) || ! juce::exactlyEqual (np.release, adsrParams.release))
    {
        adsrParams = np;
        adsr.setParameters (adsrParams);
    }
}

void FmVoice::startNote (int midiNote, float vel, juce::SynthesiserSound*, int)
{
    note = midiNote;
    velocity = vel;
    for (auto& op : ops)
        op.reset();
    filter.reset();
    updateEnvelope();
    adsr.reset();
    adsr.noteOn();
    gate = 0.0f;
    gateTarget = 1.0f;
    lastEnv = 0.0f;
}

void FmVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
        gateTarget = 0.0f;
    }
    else
    {
        adsr.reset();
        gate = gateTarget = 0.0f;
        clearCurrentNote();
    }
}

void FmVoice::renderNextBlock (juce::AudioBuffer<float>& out, int start, int num)
{
    while (num > 0 && isVoiceActive())
    {
        const int n = juce::jmin (num, maxChunk);
        renderChunk (out, start, n);
        start += n;
        num -= n;
    }
}

void FmVoice::renderChunk (juce::AudioBuffer<float>& out, int start, int n)
{
    const auto& g = globals;
    const auto& p = g.p;
    const double sr = getSampleRate();

    updateEnvelope();
    const int hpf = (int) p[QP ("hpf")];
    if (hpf != lastHpf)
    {
        filter.setHpf (hpf);
        lastHpf = hpf;
    }

    const auto& algo = kAlgorithms[(size_t) juce::jlimit (0, 7, (int) p[QP ("fm_algorithm")])];

    // ---- pitch
    const float octave = (float) ((int) p[QP ("octave")] - 1);
    const float range = (float) ((int) p[QP ("dco_range")] - 1); // 16' = -1, 8' = 0, 4' = +1
    const float vibDepth = juce::jlimit (0.0f, 1.0f, p[QP ("dco_lfo")] + g.modWheel);
    const float semis = (float) note + 12.0f * (octave + range) + p[QP ("tune")] / 100.0f
                        + g.bend * p[QP ("bend_sens_dco")] + g.lfo * vibDepth;
    const double f0 = g.a4 * std::pow (2.0, (semis - 69.0) / 12.0);

    // ---- PWM -> modulator index (op2/op4) by source
    const float pwm = p[QP ("dco_pwm")];
    float pwmFactor = 1.0f;
    switch ((int) p[QP ("pwm_mode")])
    {
        case 0: pwmFactor = 1.0f + pwm * (2.0f * lastEnv - 1.0f); break; // env
        case 1: pwmFactor = 2.0f * pwm; break;                           // manual (0.5 = neutral)
        default: pwmFactor = 1.0f + pwm * g.lfo; break;                   // lfo
    }
    pwmFactor = juce::jmax (0.0f, pwmFactor);

    static constexpr int ratioIds[4] { QP ("op1_ratio"), QP ("op2_ratio"), QP ("op3_ratio"), QP ("op4_ratio") };
    static constexpr int levelIds[4] { QP ("op1_level"), QP ("op2_level"), QP ("op3_level"), QP ("op4_level") };
    static constexpr int detuneIds[4] { QP ("op1_detune"), QP ("op2_detune"), QP ("op3_detune"), QP ("op4_detune") };
    static constexpr int fbIds[4] { QP ("op1_feedback"), QP ("op2_feedback"), QP ("op3_feedback"), QP ("op4_feedback") };
    static constexpr int velIds[4] { QP ("op1_velocity"), QP ("op2_velocity"), QP ("op3_velocity"), QP ("op4_velocity") };

    for (size_t k = 0; k < 4; ++k)
    {
        double ratio = kRatioValues[(size_t) juce::jlimit (0, 11, (int) p[(size_t) ratioIds[k]])];
        if (k == 2 && algo.carrier[2])
            ratio *= 0.5; // op3 as carrier = SUB, one octave down
        ops[k].inc = f0 * ratio * std::pow (2.0, p[(size_t) detuneIds[k]] / 1200.0) / sr;
        const float vs = p[(size_t) velIds[k]];
        float level = p[(size_t) levelIds[k]] * (1.0f - vs + vs * velocity);
        if (k == 1 || k == 3)
            level *= pwmFactor;
        ops[k].level = level;
        ops[k].feedback = p[(size_t) fbIds[k]];
    }

    // carrier gates: PULSE=op1, SAW=op2, SUB=op3 (x sub_level), op4 always if carrier
    const std::array<float, 4> carrierGain {
        algo.carrier[0] && p[QP ("wave_pulse")] > 0.5f ? 1.0f : 0.0f,
        algo.carrier[1] && p[QP ("wave_saw")] > 0.5f ? 1.0f : 0.0f,
        algo.carrier[2] && p[QP ("wave_sub")] > 0.5f ? p[QP ("sub_level")] : 0.0f,
        algo.carrier[3] ? 1.0f : 0.0f,
    };
    const float mixGain = voiceGain / (float) numCarriers (algo);
    const float noiseLevel = p[QP ("noise_level")];

    // ---- filter cutoff from the envelope value at chunk start
    const float polarity = (int) p[QP ("vcf_env_polarity")] == 0 ? 1.0f : -1.0f;
    const float octaves = p[QP ("vcf_env")] * polarity * lastEnv * 8.0f + p[QP ("vcf_lfo")] * g.lfo * 4.0f
                          + p[QP ("vcf_kybd")] * ((float) note - 60.0f) / 12.0f + g.bend * p[QP ("bend_sens_vcf")] * 2.0f;
    filter.setCutoff (JunoFilter::baseCutoff (p[QP ("vcf_freq")]) * std::exp2 (octaves));
    filter.setResonance (p[QP ("vcf_res")]);

    float buf[maxChunk];
    float env[maxChunk];
    for (int i = 0; i < n; ++i)
    {
        float o[4];
        for (int k = 3; k >= 0; --k)
        {
            float pm = 0.0f;
            for (int j = k + 1; j < 4; ++j)
                if (algo.mod[(size_t) k][(size_t) j])
                    pm += o[j];
            o[k] = ops[(size_t) k].tick (pm);
        }
        float s = 0.0f;
        for (size_t k = 0; k < 4; ++k)
            s += o[k] * carrierGain[k];
        s *= mixGain;
        if (noiseLevel > 0.0f)
            s += noiseLevel * voiceGain * (noise.nextFloat() * 2.0f - 1.0f);
        buf[i] = s;
        env[i] = adsr.getNextSample();
    }
    lastEnv = env[n - 1];

    filter.process (buf, n);

    const bool gateMode = (int) p[QP ("vca_mode")] == 1;
    const float amp = p[QP ("vca_level")] * p[QP ("volume")];
    for (int i = 0; i < n; ++i)
    {
        if (gate < gateTarget)
            gate = juce::jmin (gateTarget, gate + gateStep);
        else if (gate > gateTarget)
            gate = juce::jmax (gateTarget, gate - gateStep);
        buf[i] *= amp * (gateMode ? gate : env[i]);
    }

    for (int c = 0; c < out.getNumChannels(); ++c)
        out.addFrom (c, start, buf, n);

    const bool finished = gateMode ? (gate <= 0.0f && gateTarget <= 0.0f) : ! adsr.isActive();
    if (finished)
        clearCurrentNote();
}
} // namespace queen
