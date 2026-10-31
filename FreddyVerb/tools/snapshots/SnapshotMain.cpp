// FreddyVerb_Snapshots: renders the editor offscreen in every theme.
//
//   FreddyVerb_Snapshots --out ui_previews/m1_foundation --milestone "M1 Foundation" [--scale 1]
//   FreddyVerb_Snapshots --manifest tools/panel_forge/controls_manifest.json
//
// The processor is fed a short synthetic drum pattern first, so the envelope
// display shows real duck/gate/bloom data from the engine.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/Theme.h"
#include "state/Parameters.h"
#include <random>

namespace
{
juce::String arg (const juce::StringArray& args, const juce::String& name, const juce::String& def = {})
{
    const int i = args.indexOf (name);
    return i >= 0 && i + 1 < args.size() ? args[i + 1] : def;
}

/** A snare-ish hit: tone + noise with a fast decay. */
void renderDrumPattern (fv::FreddyVerbProcessor& proc, double sr, double seconds, const std::vector<double>& hitTimes)
{
    const int block = 256;
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    std::mt19937 rng (42);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        buf.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const double t = (pos + i) / sr;
            float s = 0.0f;
            for (double h : hitTimes)
                if (t >= h)
                {
                    const double dt = t - h;
                    s += (float) (std::exp (-dt * 38.0) * (0.55 * std::sin (2.0 * juce::MathConstants<double>::pi * 190.0 * dt)
                                                         + 0.45 * u (rng)));
                }
            buf.setSample (0, i, s * 0.8f);
            buf.setSample (1, i, s * 0.8f);
        }
        proc.processBlock (buf, midi);
    }
}

void writeIndex (const juce::File& dir, const juce::String& milestone,
                 const std::vector<std::pair<const fv::ui::ThemeEntry*, juce::String>>& files)
{
    juce::String html;
    html << "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Freddy Verb UI options - " << milestone << "</title>\n"
         << "<style>body{background:#1b1b1b;color:#eee;font-family:Helvetica,Arial,sans-serif;margin:24px}"
         << "h1{font-weight:600;letter-spacing:.06em}h2{margin-top:36px;border-bottom:1px solid #444;padding-bottom:6px;letter-spacing:.1em}"
         << ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(560px,1fr));gap:22px}"
         << ".card{background:#262626;border-radius:8px;padding:12px}.card img{width:100%;height:auto;border-radius:4px;display:block;cursor:zoom-in}"
         << ".card h3{margin:10px 0 4px;font-size:16px}.card p{margin:0;color:#aaa;font-size:13px;line-height:1.4}"
         << ".num{color:#e2532d;font-weight:700;margin-right:6px}</style></head><body>\n"
         << "<h1>FREDDY VERB &mdash; 13 UI options</h1><p>Milestone: <b>" << milestone << "</b> &middot; rendered "
         << juce::Time::getCurrentTime().toString (true, true) << " by FreddyVerb_Snapshots. Click an image to open it full size.</p>\n";
    for (int group = 0; group < 2; ++group)
    {
        html << "<h2>" << (group == 0 ? "FLAT (vintage sampler, 2D)" : "PHOTOREAL RACK (aged studio gear)") << "</h2><div class='grid'>\n";
        for (auto& [e, file] : files)
        {
            if ((int) e->theme.group != group) continue;
            html << "<div class='card'><a href='" << file << "' target='_blank'><img src='" << file << "' alt='" << e->theme.name << "'></a>"
                 << "<h3><span class='num'>" << juce::String (e->theme.number).paddedLeft ('0', 2) << "</span>" << e->theme.name << "</h3>"
                 << "<p>" << e->theme.description << "</p></div>\n";
        }
        html << "</div>\n";
    }
    html << "</body></html>\n";
    dir.getChildFile ("index.html").replaceWithText (html);
}
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);

    auto proc = std::make_unique<fv::FreddyVerbProcessor>();
    const double sr = 48000.0;
    proc->setPlayConfigDetails (2, 2, sr, 512);
    proc->prepareToPlay (sr, 512);

    // Realistic state: a snare-ish setting with a visible bloom window.
    auto set = [&] (const char* id, float v)
    {
        if (auto* p = proc->getAPVTS().getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    set (fv::pid::duckDepth, -24.0f);
    set (fv::pid::duckRelease, 340.0f);
    set (fv::pid::gateHold, 180.0f);
    set (fv::pid::gateRelease, 320.0f);

    std::unique_ptr<fv::FreddyVerbEditor> editor (dynamic_cast<fv::FreddyVerbEditor*> (proc->createEditor()));
    if (editor == nullptr) { std::cerr << "no editor\n"; return 1; }

    if (auto manifestPath = arg (args, "--manifest"); manifestPath.isNotEmpty())
    {
        juce::Array<juce::var> list;
        for (auto& m : editor->getPanel().manifest())
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", m.id);
            o->setProperty ("kind", m.kind);
            o->setProperty ("section", m.section);
            o->setProperty ("w", m.prefW);
            o->setProperty ("h", m.prefH);
            juce::String label;
            const auto pidStr = m.id.upToFirstOccurrenceOf ("#", false, false).upToFirstOccurrenceOf ("@", false, false);
            if (auto* info = fv::findParamInfo (pidStr)) label = info->shortName;
            if (auto* c = editor->getPanel().findChildWithID (m.id))
                if (auto* pad = dynamic_cast<fv::ui::PadButton*> (c)) label = pad->getTitle();
            if (m.id.startsWith ("eqsel_")) label = m.id.fromFirstOccurrenceOf ("_", false, false).toUpperCase();
            if (m.id == "meter_in") label = "IN";
            if (m.id == "meter_wet") label = "WET";
            if (m.id == "meter_out") label = "OUT";
            if (m.id == "correlation") label = "CORR";
            if (m.id == "jog") label = "DATA";
            if (m.id == "viz_scale") label = "TIME";
            o->setProperty ("label", label);
            list.add (juce::var (o));
        }
        auto* root = new juce::DynamicObject();
        root->setProperty ("controls", list);
        juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (manifestPath)).replaceWithText (juce::JSON::toString (juce::var (root)));
        std::cout << "wrote manifest " << manifestPath << " (" << list.size() << " controls)\n";
        editor.reset();
        return 0;
    }

    const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile (arg (args, "--out", "ui_previews/latest"));
    const auto milestone = arg (args, "--milestone", "latest");
    const float scale = arg (args, "--scale", "1").getFloatValue();
    outDir.createDirectory();

    // Feed the engine and capture its visualizer frames once.
    renderDrumPattern (*proc, sr, 1.25, { 0.12, 0.62 });
    std::vector<fv::dsp::VizFrame> frames (fv::dsp::VizFeed::kCapacity);
    const int nFrames = proc->getEngine().getVizFeed().pop (frames.data(), (int) frames.size());
    frames.resize ((size_t) nFrames);
    editor->getPanel().getVisualizer().injectFrames (frames, sr / fv::dsp::kVizDecimation);
    editor->getPanel().setPresetDisplay ("Snare Bloom Plate", false, 0);

    std::vector<std::pair<const fv::ui::ThemeEntry*, juce::String>> written;
    for (auto& entry : fv::ui::allThemes())
    {
        editor->setTheme (entry.theme.id);
        editor->setScale (1.0f);
        editor->tickNow();
        auto& panel = editor->getPanel();
        const auto img = panel.createComponentSnapshot (panel.getLocalBounds(), true, scale);
        const auto name = juce::String (entry.theme.number).paddedLeft ('0', 2) + "_" + entry.theme.id + ".png";
        auto file = outDir.getChildFile (name);
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat png;
        if (! os.openedOk() || ! png.writeImageToStream (img, os)) { std::cerr << "failed " << name << "\n"; return 1; }
        written.push_back ({ &entry, name });
        std::cout << "rendered " << name << " (" << img.getWidth() << "x" << img.getHeight() << ")\n";
    }
    writeIndex (outDir, milestone, written);
    editor.reset();
    proc.reset();
    return 0;
}
