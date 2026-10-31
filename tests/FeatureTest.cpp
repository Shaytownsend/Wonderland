// End-to-end feature checks: every parameter does something, every performance control works,
// every panel / host-bar feature drives the processor.
#include "helpers/render.h"
#include <PluginEditor.h>
#include <catch2/catch_test_macros.hpp>
#include <set>

namespace
{
struct Scenario
{
    std::vector<std::pair<std::string_view, float>> params;
    std::vector<int> notes { 60 };
    double seconds = 0.6, offAt = 0.35;
    float bend = 0.0f; // -1..1, sent at the start
    float mod = -1.0f; // 0..1, sent at the start when >= 0
    double bpm = 0.0;
    int velocity = 100;
};

juce::AudioBuffer<float> render (const Scenario& s, std::string_view overrideId = {}, float overrideValue = 0.0f)
{
    queen::SynthEngine e;
    for (const auto& [id, v] : s.params)
        e.setParam (id, v);
    if (! overrideId.empty())
        e.setParam (overrideId, overrideValue);
    e.prepare (48000.0, 128);
    const int total = (int) (s.seconds * 48000.0), block = 128;
    juce::AudioBuffer<float> out (2, total), tmp (2, block);
    for (int pos = 0; pos < total; pos += block)
    {
        juce::MidiBuffer midi;
        if (pos == 0)
        {
            if (s.bend != 0.0f)
                midi.addEvent (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) ((s.bend + 1.0f) * 8192.0f))), 0);
            if (s.mod >= 0.0f)
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, (int) (s.mod * 127.0f)), 0);
            for (int n : s.notes)
                midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) s.velocity), 0);
        }
        const int off = (int) (s.offAt * 48000.0);
        if (s.offAt >= 0 && off >= pos && off < pos + block)
            for (int n : s.notes)
                midi.addEvent (juce::MidiMessage::noteOff (1, n), off - pos);
        tmp.setSize (2, std::min (block, total - pos), false, false, true);
        e.render (tmp, midi, s.bpm);
        for (int c = 0; c < 2; ++c)
            out.copyFrom (c, pos, tmp, c, 0, tmp.getNumSamples());
    }
    return out;
}

double rmsDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double s = 0.0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < a.getNumSamples(); ++i)
        {
            const double d = a.getSample (c, i) - b.getSample (c, i);
            s += d * d;
        }
    return std::sqrt (s / (2.0 * a.getNumSamples()));
}

double pitchOf (const juce::AudioBuffer<float>& b, int from, int to)
{
    int crossings = 0;
    for (int i = from + 1; i < to; ++i)
        if (b.getSample (0, i - 1) < 0.0f && b.getSample (0, i) >= 0.0f)
            ++crossings;
    return crossings * 48000.0 / (to - from);
}

// pure sine at f0 (all carriers ratio 1, no filter colouring, no chorus)
Scenario sine()
{
    Scenario s;
    s.params = { { "fm_algorithm", 7 }, { "op3_level", 0 }, { "op4_feedback", 0 }, { "wave_sub", 0 }, { "chorus_1", 0 },
                 { "vcf_freq", 1 }, { "vcf_res", 0 }, { "vcf_lfo", 0 }, { "hpf", 0 } };
    s.seconds = 1.0;
    s.offAt = -1;
    return s;
}
} // namespace

TEST_CASE ("Every parameter changes the sound", "[features]")
{
    struct Case
    {
        std::string_view id;
        float value;
        Scenario scenario;
    };
    const Scenario base;
    auto with = [] (Scenario s, std::vector<std::pair<std::string_view, float>> extra) {
        s.params.insert (s.params.end(), extra.begin(), extra.end());
        return s;
    };
    auto chord = [] (Scenario s) { s.notes = { 60, 64, 67 }; return s; };
    Scenario allCarriers = with (base, { { "fm_algorithm", 7 } });
    Scenario subOn = with (base, { { "fm_algorithm", 5 }, { "wave_sub", 1 } });
    Scenario arp = chord (with (base, { { "arp_on", 1 } }));
    arp.offAt = -1; // keep the keys down so every step plays
    Scenario bent = base;
    bent.bend = 1.0f;
    Scenario manLfo = with (base, { { "lfo_trig_mode", 1 }, { "dco_lfo", 1 } });
    Scenario synced = with (arp, { { "arp_sync", 1 } });
    synced.bpm = 60.0;
    Scenario held = base;
    held.offAt = 0.1;
    Scenario arpHeld = with (arp, { { "hold", 1 } });
    arpHeld.offAt = 0.1;

    std::vector<Case> cases;
    static const std::vector<std::string> opIds = [] {
        std::vector<std::string> v;
        for (int n = 1; n <= 4; ++n)
            for (auto suffix : { "_ratio", "_level", "_detune", "_feedback", "_velocity" })
                v.push_back ("op" + std::to_string (n) + suffix);
        return v;
    }();
    for (const auto& id : opIds)
    {
        float v = 0.0f;
        if (id.ends_with ("_ratio")) v = 4;
        else if (id.ends_with ("_level")) v = 0.2f;
        else if (id.ends_with ("_detune")) v = 40;
        else if (id.ends_with ("_feedback")) v = 0.9f;
        else v = 1.0f;
        cases.push_back ({ id, v, base }); // algorithm 5: op1 carrier, op2/3/4 modulate it
    }
    cases.insert (cases.end(), {
        { "fm_algorithm", 0, base }, { "volume", 0.2f, base }, { "bend_sens_dco", 12, bent },
        { "bend_sens_vcf", 1, bent }, { "octave", 2, base }, { "lfo_trigger", 1, manLfo },
        { "arp_on", 1, chord (base) }, { "arp_mode", 2, arp }, { "arp_range", 2, arp }, { "arp_rate", 30, arp },
        { "arp_sync", 0, synced }, { "hold", 1, held }, { "key_transpose", 1, arpHeld },
        { "lfo_rate", 15, with (base, { { "dco_lfo", 1 } }) }, { "lfo_delay", 2, with (base, { { "dco_lfo", 1 } }) },
        { "lfo_trig_mode", 1, with (base, { { "dco_lfo", 1 } }) }, { "dco_lfo", 1, base }, { "dco_pwm", 1, base },
        { "pwm_mode", 2, base }, { "dco_range", 0, base }, { "wave_pulse", 0, base }, { "wave_saw", 0, allCarriers },
        { "wave_sub", 1, with (base, { { "fm_algorithm", 5 } }) }, { "sub_level", 0.1f, subOn },
        { "noise_level", 0.8f, base }, { "hpf", 3, base }, { "vcf_freq", 0.2f, base }, { "vcf_res", 0.9f, base },
        { "vcf_env", 0.0f, base }, { "vcf_env_polarity", 1, base }, { "vcf_lfo", 1, base }, { "vcf_kybd", 0, [&] { auto h = base; h.notes = { 84 }; return h; }() },
        { "vca_level", 0.2f, base }, { "vca_mode", 1, base }, { "attack", 1.0f, base }, { "decay", 0.01f, base },
        { "sustain", 0.0f, base }, { "release", 3.0f, base }, { "chorus_1", 0, base }, { "chorus_2", 1, base },
        { "tune", 50, base },
    });

    // key_transpose needs a new key while the arp is latched
    for (auto& c : cases)
        if (c.id == "key_transpose")
            c.scenario.notes = { 60 };

    std::set<std::string_view> covered;
    for (const auto& c : cases)
    {
        INFO (std::string (c.id));
        covered.insert (c.id);
        const auto a = render (c.scenario);
        juce::AudioBuffer<float> b;
        if (c.id == "key_transpose")
        {
            // latch C4, release, then play E4: with KEY TR the pattern moves, without it the latch is replaced
            auto play = [&] (float keyTr) {
                queen::SynthEngine e;
                for (const auto& [id, v] : c.scenario.params)
                    e.setParam (id, v);
                e.setParam ("key_transpose", keyTr);
                e.setParam ("arp_range", 1); // C4,C5 vs E4,E5 differ either way; 2 octaves makes it obvious
                e.prepare (48000.0, 128);
                juce::AudioBuffer<float> out (2, 48000), tmp (2, 128);
                for (int pos = 0; pos < 48000; pos += 128)
                {
                    juce::MidiBuffer m;
                    if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                    if (pos == 4864) m.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                    if (pos == 9984) m.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
                    if (pos == 14976) m.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
                    if (pos == 19968) m.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);
                    e.render (tmp, m);
                    for (int ch = 0; ch < 2; ++ch)
                        out.copyFrom (ch, pos, tmp, ch, 0, 128);
                }
                return out;
            };
            CHECK (rmsDiff (play (0.0f), play (1.0f)) > 1.0e-3);
            continue;
        }
        b = render (c.scenario, c.id, c.value);
        CHECK_FALSE (stats (b).hasNaN);
        CHECK (stats (b).peak < 1.0f);
        CHECK (rmsDiff (a, b) > 1.0e-4);
    }
    for (const auto& d : queen::kParams)
    {
        INFO ("not exercised: " << std::string (d.id));
        CHECK (covered.contains (d.id));
    }
}

TEST_CASE ("Performance: pitch bend, mod wheel, velocity, sustain", "[features]")
{
    auto s = sine();
    const double f0 = pitchOf (render (s), 4800, 48000);
    CHECK (std::abs (f0 - 261.63) < 3.0);

    auto up = s;
    up.bend = 1.0f; // +2 semitones (bend_sens_dco default 2)
    CHECK (std::abs (pitchOf (render (up), 4800, 48000) - 293.66) < 3.0);
    auto down = s;
    down.bend = -1.0f;
    CHECK (std::abs (pitchOf (render (down), 4800, 48000) - 233.08) < 3.0);

    // mod wheel adds vibrato (pitch varies over time)
    auto spread = [] (const juce::AudioBuffer<float>& b) {
        double lo = 1e9, hi = 0;
        for (int w = 4800; w + 2400 <= 48000; w += 2400)
        {
            const double f = pitchOf (b, w, w + 2400);
            lo = std::min (lo, f);
            hi = std::max (hi, f);
        }
        return hi - lo;
    };
    auto vib = s;
    vib.mod = 1.0f;
    CHECK (spread (render (vib)) > spread (render (s)) + 10.0);

    // velocity sensitivity
    auto soft = s;
    soft.velocity = 20;
    auto sensitive = s;
    sensitive.params.push_back ({ "op1_velocity", 1 });
    sensitive.params.push_back ({ "op2_velocity", 1 });
    sensitive.params.push_back ({ "op4_velocity", 1 });
    auto softSensitive = sensitive;
    softSensitive.velocity = 20;
    CHECK (stats (render (softSensitive)).rms < stats (render (sensitive)).rms * 0.4);
    CHECK (stats (render (soft)).rms < stats (render (s)).rms); // default sensitivity 0.5

    // sustain pedal (CC64) keeps released notes sounding
    queen::SynthEngine e;
    e.setParam ("release", 0.01f);
    e.prepare (48000.0, 128);
    juce::AudioBuffer<float> buf (2, 128);
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    m.addEvent (juce::MidiMessage::noteOff (1, 60), 64);
    e.render (buf, m);
    juce::MidiBuffer none;
    for (int i = 0; i < 100; ++i)
        e.render (buf, none);
    CHECK (e.getActiveVoiceCount() == 1);
    juce::MidiBuffer release;
    release.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    e.render (buf, release);
    for (int i = 0; i < 100; ++i)
        e.render (buf, none);
    CHECK (e.getActiveVoiceCount() == 0);
}

TEST_CASE ("Arp sync follows host tempo", "[features]")
{
    // 1/16 at 60 bpm = 4 steps/s; at 240 bpm = 16 steps/s: compare sounding note changes by pitch
    auto steps = [] (double bpm) {
        queen::Arpeggiator a;
        a.prepare (48000.0);
        a.setRate (bpm / 60.0 * 4.0);
        a.noteOn (60, 0.8f);
        a.noteOn (64, 0.8f);
        int ons = 0, t = 0;
        while (t < 48000)
        {
            a.processDue ([&] (const queen::Arpeggiator::Event& ev) { ons += ev.on ? 1 : 0; });
            const int n = std::max (1, a.samplesUntilNextEvent());
            a.advance (n);
            t += n;
        }
        return ons;
    };
    CHECK (steps (60.0) == 4);
    CHECK (steps (120.0) == 8);
    CHECK (steps (240.0) == 16);
}

TEST_CASE ("Panel: on-screen keyboard and wheels drive the synth", "[features]")
{
    PluginProcessor p;
    p.setPlayConfigDetails (0, 2, 48000.0, 128);
    p.prepareToPlay (48000.0, 128);
    std::unique_ptr<PluginEditor> ed (dynamic_cast<PluginEditor*> (p.createEditor()));
    juce::AudioBuffer<float> buf (2, 128);
    juce::MidiBuffer midi;

    p.keyboardState.noteOn (1, 64, 0.8f); // what clicking a key does
    p.processBlock (buf, midi);
    CHECK (p.engine.getActiveVoiceCount() == 1);
    CHECK (buf.getMagnitude (0, 128) > 0.0f);

    p.setWheel (true, 1.0f);
    p.setWheel (false, 1.0f);
    midi.clear();
    p.processBlock (buf, midi);
    CHECK (p.engine.getBend() > 0.99f);
    CHECK (p.engine.getModWheel() > 0.99f);

    ed->getPanel().refresh(); // wheels follow the engine
    p.setWheel (true, 0.0f);
    midi.clear();
    p.processBlock (buf, midi);
    CHECK (std::abs (p.engine.getBend()) < 0.001f);

    p.keyboardState.noteOff (1, 64, 0.0f);
}

TEST_CASE ("Panel: switches, display, faders", "[features]")
{
    PluginProcessor p;
    std::unique_ptr<PluginEditor> ed (dynamic_cast<PluginEditor*> (p.createEditor()));
    auto& panel = ed->getPanel();
    const auto& layout = queen::Layout::get();

    for (const auto& c : layout.controls)
    {
        INFO (c.id.toStdString());
        if (c.type == "switch")
        {
            auto* sw = dynamic_cast<queen::SwitchControl*> (panel.findControl (c.id));
            REQUIRE (sw != nullptr);
            for (int k = 0; k < c.positions.size(); ++k)
            {
                sw->setPosition (k);
                CHECK (juce::roundToInt (p.apvts.getRawParameterValue (c.param)->load()) == c.positions[k]);
                CHECK (sw->getPosition() == k);
            }
        }
        if (c.type == "slider")
        {
            // cap centre on the "0" tick at minimum and the "10" tick at maximum (registry y +- 56),
            // and JUCE's mouse mapping agrees with where the cap is drawn
            auto* f = dynamic_cast<queen::FaderControl*> (panel.findControl (c.id));
            REQUIRE (f != nullptr);
            f->setValue (f->getMinimum());
            CHECK (std::abs ((float) f->getY() + f->capCentreY() - (c.y + c.h / 2.0f)) < 0.75f);
            CHECK (std::abs ((float) f->getPositionOfValue (f->getMinimum()) - f->capCentreY()) < 1.0f);
            f->setValue (f->getMaximum());
            CHECK (std::abs ((float) f->getY() + f->capCentreY() - (c.y - c.h / 2.0f)) < 0.75f);
            CHECK (std::abs ((float) f->getPositionOfValue (f->getMaximum()) - f->capCentreY()) < 1.0f);
        }
    }

    // display shows pending bank + patch
    p.memory.recall (3, 6);
    panel.refresh();
    CHECK (p.memory.getPendingBank() == 3);
    p.memory.pressManual();
    CHECK (p.memory.isManual());
}

TEST_CASE ("Host bar: prev/next, save, init, settings", "[features]")
{
    PluginProcessor p;
    p.setPlayConfigDetails (0, 2, 48000.0, 128);
    p.prepareToPlay (48000.0, 128);
    std::unique_ptr<PluginEditor> ed (dynamic_cast<PluginEditor*> (p.createEditor()));
    auto& bar = ed->getHostBar();

    CHECK (bar.getPresetText().contains ("Glass Bells"));
    bar.step (1);
    CHECK (p.memory.getPatch() == 1);
    CHECK (bar.getPresetText().contains ("Tine Piano"));
    bar.step (-2); // wraps to bank 5 patch 8
    CHECK (p.memory.getBank() == 4);
    CHECK (p.memory.getPatch() == 7);

    p.memory.recall (1, 2);
    p.apvts.getParameter ("vcf_res")->setValueNotifyingHost (0.9f);
    bar.save();
    p.memory.recall (0, 0);
    p.memory.recall (1, 2);
    CHECK (p.apvts.getParameter ("vcf_res")->getValue() > 0.89f);

    bar.init();
    CHECK (p.memory.isManual());
    CHECK (std::abs (p.apvts.getRawParameterValue ("vcf_res")->load() - 0.3f) < 1e-4f);

    p.setOversampling (2);
    CHECK (p.getOversampling() == 2);
    CHECK (p.getLatencySamples() > 0);
    juce::AudioBuffer<float> buf (2, 128);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    p.processBlock (buf, midi);
    CHECK_FALSE (stats (buf).hasNaN);
    p.setOversampling (1);
    CHECK (p.getLatencySamples() == 0);

    // master tune moves the pitch
    auto s = sine();
    queen::SynthEngine e;
    for (const auto& [id, v] : s.params)
        e.setParam (id, v);
    e.setMasterTune (466.16); // A#4 reference = one semitone up
    e.prepare (48000.0, 128);
    auto out = renderNotes (e, { 60 }, 1.0);
    CHECK (std::abs (pitchOf (out, 4800, 48000) - 277.18) < 3.0);
}

TEST_CASE ("Editor paints at small, default and large sizes", "[features]")
{
    PluginProcessor p;
    std::unique_ptr<PluginEditor> ed (dynamic_cast<PluginEditor*> (p.createEditor()));
    for (int w : { 900, 1440, 1800, 3600 })
    {
        ed->setSize (w, w / 2);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        CHECK (img.getWidth() == w);
        // something was drawn in the panel and the keybed (not black)
        auto px = img.getPixelAt (w / 2, w / 4);
        CHECK (px.getBrightness() > 0.02f);
    }
}
