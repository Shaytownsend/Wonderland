#include "MainPanel.h"
#include "../PluginProcessor.h"
#include "../state/Parameters.h"

namespace fv::ui
{
namespace
{
    enum Sec { REVERB = 0, DUCK, GATE, TONE, SPACE, MIX, DETECT, TOP, IRCOL, LCD };
    const char* sectionTitle (int s)
    {
        switch (s)
        {
            case REVERB: return "REVERB"; case DUCK: return "DUCK"; case GATE: return "GATE";
            case TONE: return "TONE"; case SPACE: return "SPACE"; case MIX: return "MIX";
            case DETECT: return "DETECT"; default: return "";
        }
    }
}

/** LCD readout strip: shows the last touched parameter and its value. */
struct MainPanel::Readout : public juce::Component
{
    explicit Readout (ControlHub& h) : hub (h) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        auto* lnf = freddyLnf (*this);
        if (lnf == nullptr) return;
        auto inner = lnf->drawDisplayFrame (g, getLocalBounds().toFloat());
        auto* p = hub.apvts.getParameter (hub.getLastTouched());
        juce::String name = "--", value;
        if (p != nullptr)
        {
            name = p->getName (24).toUpperCase();
            value = p->getCurrentValueAsText().toUpperCase();
        }
        auto r = inner.reduced (6.0f, inner.getHeight() * 0.22f);
        auto right = r.removeFromRight (r.getWidth() * 0.42f);
        lnf->drawDisplayText (g, name, r, juce::Justification::centredLeft);
        lnf->drawValueReadout (g, value, right);
        lnf->drawDisplayOverlay (g, inner);
    }
    ControlHub& hub;
};

/** Preset name LCD in the top bar. */
struct MainPanel::PresetLcd : public juce::Component
{
    void paint (juce::Graphics& g) override
    {
        auto* lnf = freddyLnf (*this);
        if (lnf == nullptr) return;
        auto inner = lnf->drawDisplayFrame (g, getLocalBounds().toFloat());
        auto r = inner.reduced (6.0f, inner.getHeight() * 0.2f);
        lnf->drawDisplayText (g, juce::String (ab == 0 ? "A:" : "B:") + name + (dirty ? " *" : ""), r, juce::Justification::centredLeft);
        lnf->drawDisplayOverlay (g, inner);
    }
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    juce::String name { "INIT - MEDIUM PLATE" };
    bool dirty = false;
    int ab = 0;
    std::function<void()> onClick;
};

MainPanel::MainPanel (FreddyVerbProcessor& p, ControlHub& h) : proc (p), hub (h)
{
    using namespace fv::pid;
    hub.addListener (this);

    // --- displays -------------------------------------------------------------
    visualizer = std::make_unique<EnvelopeVisualizer> (hub, proc.getEngine().getVizFeed());
    addOther (visualizer.get(), "envelope", "display", LCD, 580, 300);
    irBrowser = std::make_unique<IRBrowser>();
    addOther (irBrowser.get(), "ir_browser", "display", IRCOL, 300, 210);
    irDisplay = std::make_unique<IRDisplay> (hub);
    addOther (irDisplay.get(), "ir_display", "display", IRCOL, 300, 150);
    eqDisplay = std::make_unique<EQDisplay> (hub);
    addOther (eqDisplay.get(), "eq_display", "display", TONE, 330, 110);
    readout = std::make_unique<Readout> (hub);
    addOther (readout.get(), "readout", "display", LCD, 470, 40);
    jog = std::make_unique<JogWheel> (hub);
    addOther (jog.get(), "jog", "jog", LCD, 44, 44);
    presetLcd = std::make_unique<PresetLcd>();
    presetLcd->onClick = [this] { if (onPresetMenu) onPresetMenu(); };
    addOther (presetLcd.get(), "preset", "display", TOP, 300, 36);

    // --- top bar buttons --------------------------------------------------------
    addButton ("preset_prev", "<", TOP, 30)->onClick = [this] { if (onPresetStep) onPresetStep (-1); };
    addButton ("preset_next", ">", TOP, 30)->onClick = [this] { if (onPresetStep) onPresetStep (1); };
    addButton ("ab_a", "A", TOP, 32)->onClick   = [this] { if (onABSelect) onABSelect (0); };
    addButton ("ab_b", "B", TOP, 32)->onClick   = [this] { if (onABSelect) onABSelect (1); };
    addButton ("ab_copy", "A>B", TOP, 44)->onClick = [this] { if (onABSelect) onABSelect (2); };
    addButton ("undo", "UNDO", TOP, 50)->onClick = [this] { if (onUndo) onUndo(); };
    addButton ("redo", "REDO", TOP, 50)->onClick = [this] { if (onRedo) onRedo(); };
    addButton ("settings", "SETUP", TOP, 56)->onClick = [this] { if (onSettings) onSettings(); };
    for (auto& it : items) if (it.kind == "button") static_cast<PadButton*> (it.comp)->setHasLed (false);

    // --- visualizer buttons -------------------------------------------------------
    scaleBtn = addButton ("viz_scale", "1000MS", LCD, 64);
    scaleBtn->setHasLed (false);
    scaleBtn->onClick = [this]
    {
        int idx = 0;
        for (int i = 0; i < 5; ++i) if (EnvelopeVisualizer::kTimeScales[i] == visualizer->getTimeScaleMs()) idx = i;
        visualizer->setTimeScaleMs (EnvelopeVisualizer::kTimeScales[(idx + 1) % 5]);
        scaleBtn->setText (juce::String (visualizer->getTimeScaleMs()) + "MS");
        proc.getUIState().setProperty ("vizScale", visualizer->getTimeScaleMs(), nullptr);
    };
    freezeBtn = addButton ("viz_freeze", "FREEZE", LCD, 64);
    freezeBtn->onClick = [this]
    {
        visualizer->setFrozen (! visualizer->isFrozen());
        freezeBtn->setToggleState (visualizer->isFrozen());
    };
    {
        const int scale = proc.getUIState().getProperty ("vizScale", 1000);
        visualizer->setTimeScaleMs (scale);
        scaleBtn->setText (juce::String (scale) + "MS");
    }

    // --- REVERB ---------------------------------------------------------------------
    addStepper (quality, REVERB, 0, 118);
    addPad (irReverse, "REVERSE", REVERB, 0, 70);
    addPad (irNormalize, "NORM", REVERB, 0, 56);
    addKnob (predelay, REVERB, 1);
    addKnob (irStart, REVERB, 1);
    addKnob (irLength, REVERB, 1);
    addKnob (irSize, REVERB, 1, true);
    addKnob (irDecay, REVERB, 1, true);
    addKnob (irXover, REVERB, 2);
    addKnob (irEarly, REVERB, 2, true);
    addKnob (irLate, REVERB, 2, true);
    addKnob (irDamping, REVERB, 2);
    addKnob (irDampFreq, REVERB, 2);
    addPad (predelaySync, "SYNC", REVERB, 3, 56);
    addStepper (predelayNote, REVERB, 3, 86);

    // --- DETECT -----------------------------------------------------------------------
    addKnob (keyHpf, DETECT, 0);
    addKnob (keyLpf, DETECT, 0);
    addStepper (detMode, DETECT, 0, 80);
    addKnob (detRms, DETECT, 0);
    addKnob (lookahead, DETECT, 0);
    addPad (keyListen, "LISTEN", DETECT, 0, 64);

    // --- DUCK -------------------------------------------------------------------------
    addPad (duckOn, "DUCK", DUCK, 0, 62);
    addChoicePad (duckMode, choices::follow, "FOLLOW", DUCK, 0, 70);
    addChoicePad (duckMode, choices::curve, "CURVE", DUCK, 0, 70);
    addStepper (curveRetrig, DUCK, 0, 92);
    addKnob (duckDepth, DUCK, 1);
    addKnob (duckThresh, DUCK, 1);
    addKnob (duckAttack, DUCK, 1);
    duckFollowOnly.push_back (addKnob (duckRelease, DUCK, 1));
    duckFollowOnly.push_back (addKnob (duckAmount, DUCK, 1));
    duckFollowOnlyOther.push_back (addStepper (duckRelCurve, DUCK, 1, 100));
    {
        const char* shortNames[] = { "LIN", "EXP", "LOG", "S", "HOLD", "PUMP", "SWELL" };
        for (int i = 0; i < choices::numCurveShapes; ++i)
        {
            auto* pad = addChoicePad (curveShape, i, shortNames[i], DUCK, 2, 44);
            pad->setGlyph (i);
            pad->setTooltip ("Curve shape: " + choices::curveShapeNames()[i]);
            duckCurveOnlyOther.push_back (pad);
        }
    }
    duckCurveOnly.push_back (addKnob (curveDuration, DUCK, 3));
    duckCurveOnly.push_back (addKnob (curveTension, DUCK, 3, true));
    duckCurveOnly.push_back (addKnob (curveGuard, DUCK, 3));
    duckCurveOnly.push_back (addKnob (curveHyst, DUCK, 3));
    duckCurveOnlyOther.push_back (addPad (curveSync, "SYNC", DUCK, 3, 52));
    duckCurveOnlyOther.push_back (addStepper (curveNote, DUCK, 3, 78));
    for (auto& it : items)
        if (it.section == DUCK)
        {
            if (auto* k = dynamic_cast<FreddyKnob*> (it.comp)) k->setAccent (juce::Colour());
        }

    // --- GATE ---------------------------------------------------------------------------
    addPad (gateOn, "GATE", GATE, 0, 62);
    addStepper (gateRelCurve, GATE, 0, 100);
    addKnob (gateThresh, GATE, 1);
    addKnob (gateHyst, GATE, 1);
    addKnob (gateAttack, GATE, 1);
    addKnob (gateHold, GATE, 2);
    addKnob (gateRelease, GATE, 2);
    addKnob (gateFloor, GATE, 2);
    addPad (gateHoldSync, "SYNC", GATE, 3, 52);
    addStepper (gateHoldNote, GATE, 3, 78);

    // --- TONE -----------------------------------------------------------------------------
    addPad (eqOn, "EQ", TONE, 0, 48);
    addChoicePad (eqPosition, choices::pre, "PRE", TONE, 0, 48);
    addChoicePad (eqPosition, choices::post, "POST", TONE, 0, 52);
    addPad (hpfOn, "HPF", TONE, 1, 46);
    addPad (lsOn, "LS", TONE, 1, 40);
    addPad (b1On, "B1", TONE, 1, 40);
    addPad (b2On, "B2", TONE, 1, 40);
    addPad (hsOn, "HS", TONE, 1, 40);
    addPad (lpfOn, "LPF", TONE, 1, 46);
    addKnob (hpfFreq, TONE, 2);
    addStepper (hpfSlope, TONE, 2, 78);
    addKnob (lpfFreq, TONE, 2);
    addStepper (lpfSlope, TONE, 2, 78);
    {
        const char* ids[4][3] = { { lsFreq, lsGain, lsQ }, { b1Freq, b1Gain, b1Q }, { b2Freq, b2Gain, b2Q }, { hsFreq, hsGain, hsQ } };
        for (int b = 0; b < 4; ++b)
            for (int k = 0; k < 3; ++k)
            {
                bandKnobs[b][k] = addKnob (ids[b][k], TONE, 3, k == 1);
                // all four bands share the same three slots
                bandKnobs[b][k]->setComponentID (juce::String ("eqsel_") + (k == 0 ? "freq" : (k == 1 ? "gain" : "q")) + "#" + juce::String (b));
            }
    }
    eqDisplay->onBandSelected = [this] (int band)
    {
        // display bands: 0 HPF, 1 LS, 2 B1, 3 B2, 4 HS, 5 LPF
        if (band >= 1 && band <= 4) updateEqBandKnobs (band - 1);
    };

    // --- SPACE ------------------------------------------------------------------------------
    addKnob (width, SPACE, 0);
    addKnob (monoBelow, SPACE, 0);
    addPad (monoBelowOn, "MONO LO", SPACE, 0, 70);
    addKnob (balance, SPACE, 0, true);
    correlation = std::make_unique<CorrelationMeter>();
    addOther (correlation.get(), "correlation", "meter", SPACE, 90, 12);

    // --- MIX ----------------------------------------------------------------------------------
    addKnob (mix, MIX, 0);
    addKnob (dryLevel, MIX, 0);
    addKnob (wetLevel, MIX, 0);
    addKnob (outGain, MIX, 0, true);
    addPad (mixLock, "LOCK", MIX, 1, 54);
    addPad (wetSolo, "SOLO", MIX, 1, 54);
    addPad (bypass, "BYPASS", MIX, 1, 66);
    meterIn = std::make_unique<LevelMeter> ("IN");
    meterWet = std::make_unique<LevelMeter> ("WET");
    meterOut = std::make_unique<LevelMeter> ("OUT");
    addOther (meterIn.get(), "meter_in", "meter", MIX, 22, 110);
    addOther (meterWet.get(), "meter_wet", "meter", MIX, 22, 110);
    addOther (meterOut.get(), "meter_out", "meter", MIX, 22, 110);

    updateEqBandKnobs (1);
    updateDuckModeDimming();

    // --- IR browser / display wiring -------------------------------------------------
    irBrowser->onSelect = [this] (const IRListItem& it)
    {
        if (it.factory) proc.getIRManager().loadFactory (it.id);
        else loadUserIR (juce::File (it.id));
    };
    irBrowser->onBrowse = [this] { chooseIRFile (false); };
    irBrowser->onFilesDropped = [this] (const juce::StringArray& files) { if (! files.isEmpty()) loadUserIR (juce::File (files[0])); };
    irBrowser->onFavourite = [this] (const IRListItem& it, bool fav)
    {
        auto favs = juce::StringArray::fromTokens (proc.getUIState().getProperty ("irFavourites").toString(), "|", "");
        favs.removeString (it.id);
        if (fav) favs.add (it.id);
        proc.getUIState().setProperty ("irFavourites", favs.joinIntoString ("|"), nullptr);
    };
    irDisplay->onRelinkRequested = [this] { chooseIRFile (true); };
    refreshIRBrowser();
}

void MainPanel::refreshIRBrowser()
{
    const auto favs = juce::StringArray::fromTokens (proc.getUIState().getProperty ("irFavourites").toString(), "|", "");
    std::vector<IRListItem> list;
    for (auto& e : proc.getIRLibrary().factory())
        list.push_back ({ e.id, e.name, e.category, e.description, e.rt60, favs.contains (e.id), true });
    // recently used user IRs
    for (auto& path : juce::StringArray::fromTokens (proc.getUIState().getProperty ("irRecent").toString(), "|", ""))
        if (juce::File (path).existsAsFile())
            list.push_back ({ path, juce::File (path).getFileNameWithoutExtension(), "User", path, 0.0, favs.contains (path), false });
    irBrowser->setItems (std::move (list));
    irBrowser->setCurrent (proc.getIRManager().getStatus().id);
}

void MainPanel::loadUserIR (const juce::File& f)
{
    juce::String err;
    if (! proc.getIRManager().loadFile (f, err))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Freddy Verb", "Could not load IR:\n" + err);
        return;
    }
    auto recent = juce::StringArray::fromTokens (proc.getUIState().getProperty ("irRecent").toString(), "|", "");
    recent.removeString (f.getFullPathName());
    recent.insert (0, f.getFullPathName());
    while (recent.size() > 32) recent.remove (recent.size() - 1);
    proc.getUIState().setProperty ("irRecent", recent.joinIntoString ("|"), nullptr);
    proc.getUIState().setProperty ("irLastFolder", f.getParentDirectory().getFullPathName(), nullptr);
    refreshIRBrowser();
}

void MainPanel::chooseIRFile (bool relink)
{
    const juce::File start (proc.getUIState().getProperty ("irLastFolder",
                             juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName()).toString());
    chooser = std::make_unique<juce::FileChooser> (relink ? "Relink missing IR" : "Load impulse response", start, "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) loadUserIR (f);
                          });
}

void MainPanel::syncIRDisplay()
{
    const int v = proc.getIRManager().getVersion();
    if (v == irVersionSeen) return;
    irVersionSeen = v;
    const auto st = proc.getIRManager().getStatus();
    irDisplay->setThumbnail (st.display.peaks, st.display.lengthSeconds, st.name.toUpperCase(), st.display.rt60);
    irDisplay->setSpectrogram (st.display.spectrogram);
    irDisplay->setMissing (st.missing, st.path);
    irBrowser->setCurrent (st.id);
}

MainPanel::~MainPanel()
{
    hub.removeListener (this);
}

// ---------------------------------------------------------------------------
// creation helpers
// ---------------------------------------------------------------------------
FreddyKnob* MainPanel::addKnob (const juce::String& id, int section, int group, bool bipolar)
{
    auto k = std::make_unique<FreddyKnob> (hub, id);
    k->setBipolar (bipolar);
    k->setComponentID (id);
    if (section == GATE) k->setAccent (juce::Colour (0)); // resolved at paint by theme (see themeChanged)
    auto* raw = k.get();
    addAndMakeVisible (*raw);
    items.push_back ({ raw, section, 54, 64, "knob", group });
    owned.push_back (std::move (k));
    return raw;
}

PadButton* MainPanel::addPad (const juce::String& id, const juce::String& text, int section, int group, float w)
{
    auto b = std::make_unique<PadButton> (hub, id, text);
    b->setComponentID (id);
    auto* raw = b.get();
    addAndMakeVisible (*raw);
    items.push_back ({ raw, section, w, 26, "pad", group });
    owned.push_back (std::move (b));
    return raw;
}

PadButton* MainPanel::addChoicePad (const juce::String& id, int idx, const juce::String& text, int section, int group, float w)
{
    auto b = std::make_unique<PadButton> (hub, id, idx, text);
    b->setComponentID (id + "#" + juce::String (idx));
    auto* raw = b.get();
    addAndMakeVisible (*raw);
    items.push_back ({ raw, section, w, 26, "choicepad", group });
    owned.push_back (std::move (b));
    return raw;
}

ChoiceStepper* MainPanel::addStepper (const juce::String& id, int section, int group, float w)
{
    auto s = std::make_unique<ChoiceStepper> (hub, id);
    s->setComponentID (id + "@stepper");
    auto* raw = s.get();
    addAndMakeVisible (*raw);
    items.push_back ({ raw, section, w, 38, "stepper", group });
    owned.push_back (std::move (s));
    return raw;
}

PadButton* MainPanel::addButton (const juce::String& cid, const juce::String& text, int section, float w)
{
    auto b = std::make_unique<PadButton> (text);
    b->setComponentID (cid);
    auto* raw = b.get();
    addAndMakeVisible (*raw);
    items.push_back ({ raw, section, w, 26, "button", 0 });
    owned.push_back (std::move (b));
    return raw;
}

void MainPanel::addOther (juce::Component* c, const juce::String& cid, const juce::String& kind, int section, float w, float h)
{
    c->setComponentID (cid);
    addAndMakeVisible (*c);
    items.push_back ({ c, section, w, h, kind, 0 });
}

std::vector<MainPanel::ManifestEntry> MainPanel::manifest() const
{
    std::vector<ManifestEntry> m;
    for (auto& it : items)
    {
        auto id = it.comp->getComponentID();
        if (id.startsWith ("eqsel_"))
        {
            if (! id.endsWith ("#0")) continue;   // one slot per selected-band knob
            id = id.upToFirstOccurrenceOf ("#", false, false);
        }
        juce::String sec;
        switch (it.section)
        {
            case TOP: sec = "TOP"; break; case IRCOL: sec = "IR"; break; case LCD: sec = "LCD"; break;
            default: sec = sectionTitle (it.section); break;
        }
        m.push_back ({ id, it.kind, sec, it.w, it.h });
    }
    return m;
}

// ---------------------------------------------------------------------------
// state-driven UI
// ---------------------------------------------------------------------------
void MainPanel::controlTouched (const juce::String&)
{
    readout->repaint();
}

void MainPanel::updateEqBandKnobs (int band)
{
    eqSelBand = band;
    for (int b = 0; b < 4; ++b)
        for (int k = 0; k < 3; ++k)
            bandKnobs[b][k]->setVisible (b == band);
}

void MainPanel::updateDuckModeDimming()
{
    const int mode = (int) hub.apvts.getRawParameterValue (pid::duckMode)->load();
    if (mode == lastDuckMode) return;
    lastDuckMode = mode;
    const float dim = 0.38f;
    for (auto* k : duckFollowOnly) k->setAlpha (mode == choices::follow ? 1.0f : dim);
    for (auto* c : duckFollowOnlyOther) c->setAlpha (mode == choices::follow ? 1.0f : dim);
    for (auto* k : duckCurveOnly) k->setAlpha (mode == choices::curve ? 1.0f : dim);
    for (auto* c : duckCurveOnlyOther) c->setAlpha (mode == choices::curve ? 1.0f : dim);
}

void MainPanel::setPresetDisplay (const juce::String& name, bool dirty, int abSlot)
{
    presetLcd->name = name.toUpperCase();
    presetLcd->dirty = dirty;
    presetLcd->ab = abSlot;
    presetLcd->repaint();
    for (auto& it : items)
    {
        if (it.comp->getComponentID() == "ab_a") static_cast<PadButton*> (it.comp)->setToggleState (abSlot == 0);
        if (it.comp->getComponentID() == "ab_b") static_cast<PadButton*> (it.comp)->setToggleState (abSlot == 1);
    }
}

void MainPanel::tick()
{
    auto& eng = proc.getEngine();
    auto& m = eng.getMeters();
    auto dbOf = [] (float v) { return v > 1.0e-6f ? 20.0f * std::log10 (v) : -120.0f; };
    auto setMeter = [&] (LevelMeter& lm, dsp::MeterValues& mv)
    {
        lm.setLevels (dbOf (std::max (mv.peakL.load(), mv.peakR.load())), dbOf (std::max (mv.rmsL.load(), mv.rmsR.load())));
    };
    setMeter (*meterIn, m.input);
    setMeter (*meterWet, m.wet);
    setMeter (*meterOut, m.output);
    correlation->setValue (m.correlation.load());

    syncIRDisplay();
    visualizer->pull (proc.isAudioRunning(), proc.snapshotParams(), eng.getSampleRate(), proc.getCurrentIRRt60());
    updateDuckModeDimming();
}

// ---------------------------------------------------------------------------
// layout
// ---------------------------------------------------------------------------
void MainPanel::themeChanged (const FreddyTheme& t)
{
    theme = &t;
    auto* lnf = freddyLnf (*this);
    hub.drawLabels = t.group == ThemeGroup::flat;
    for (auto& it : items)
        if (auto* k = dynamic_cast<FreddyKnob*> (it.comp))
        {
            if (it.section == DUCK) k->setAccent (lnf ? lnf->sectionAccent (1) : juce::Colour());
            else if (it.section == GATE) k->setAccent (lnf ? lnf->sectionAccent (2) : juce::Colour());
            else k->setAccent ({});
        }
        else if (auto* pb = dynamic_cast<PadButton*> (it.comp))
        {
            if (it.section == DUCK) pb->setAccent (lnf ? lnf->sectionAccent (1) : juce::Colour());
            else if (it.section == GATE) pb->setAccent (lnf ? lnf->sectionAccent (2) : juce::Colour());
        }
    resized();
    repaint();
}

void MainPanel::resized()
{
    if (theme == nullptr) return;
    sectionRects.clear();
    arrows.clear();
    floppySlot = {};
    for (auto& it : items) it.comp->setVisible (true);
    updateEqBandKnobs (eqSelBand);

    auto full = getLocalBounds().toFloat();
    if (auto* lnf = freddyLnf (*this); lnf != nullptr && lnf->skinLayout() != nullptr)
        layoutPhotoreal (*lnf->skinLayout());
    else if (theme->layout == LayoutVariant::floppyWide)
        layoutFloppy (full);
    else
        layoutStandard (full);
}

void MainPanel::layoutTopBar (juce::Rectangle<float> bar)
{
    sectionRects[TOP] = bar;
    auto r = bar.reduced (10.0f, 9.0f);
    logoArea = r.removeFromLeft (290.0f);
    auto place = [&] (const juce::String& id, float w, float gap = 6.0f)
    {
        for (auto& it : items)
            if (it.comp->getComponentID() == id) { it.comp->setBounds (r.removeFromLeft (w).toNearestInt()); r.removeFromLeft (gap); }
    };
    r.removeFromLeft (10.0f);
    place ("preset_prev", 30, 4);
    place ("preset", std::min (340.0f, r.getWidth() * 0.42f), 4);
    place ("preset_next", 30, 18);
    place ("ab_a", 32, 4); place ("ab_b", 32, 4); place ("ab_copy", 46, 18);
    place ("undo", 52, 4); place ("redo", 52, 18);
    // settings at far right
    for (auto& it : items)
        if (it.comp->getComponentID() == "settings")
            it.comp->setBounds (bar.reduced (10.0f, 9.0f).removeFromRight (64.0f).toNearestInt());
}

void MainPanel::layoutSection (int section, juce::Rectangle<float> r)
{
    sectionRects[section] = r;
    const float ks = theme->knobScale;
    const bool pads = theme->layout == LayoutVariant::padStation;
    auto content = r.reduced (8.0f, 4.0f).withTrimmedTop (theme->labelSize + 8.0f);

    // gather this section's items by group
    std::map<int, std::vector<Item*>> groups;
    for (auto& it : items)
        if (it.section == section && it.comp->isVisible()) groups[it.group].push_back (&it);
    if (groups.empty()) return;

    // special: TONE puts the EQ display on top
    if (section == TONE)
        for (auto& g : groups)
            for (auto it = g.second.begin(); it != g.second.end(); )
            {
                if ((*it)->comp == eqDisplay.get())
                {
                    eqDisplay->setBounds (content.removeFromTop (std::max (70.0f, content.getHeight() * 0.36f)).toNearestInt());
                    content.removeFromTop (4.0f);
                    it = g.second.erase (it);
                }
                else ++it;
            }
    // special: MIX meters on the right
    if (section == MIX)
    {
        auto meters = content.removeFromRight (84.0f);
        const float mw = 22.0f;
        int i = 0;
        for (auto* m : { meterIn.get(), meterWet.get(), meterOut.get() })
            m->setBounds (juce::Rectangle<float> (meters.getX() + (float) i++ * (mw + 6.0f), meters.getY() + 2, mw, meters.getHeight() - 4).toNearestInt());
        for (auto& g : groups)
            g.second.erase (std::remove_if (g.second.begin(), g.second.end(), [] (Item* it) { return it->kind == "meter"; }), g.second.end());
    }

    // flex rows: one FlexBox per group, stacked vertically. If the content does
    // not fit, everything is scaled down uniformly until it does.
    auto build = [&] (float fit, juce::FlexBox& column, std::vector<std::unique_ptr<juce::FlexBox>>& rows) -> float
    {
        column = {};
        rows.clear();
        column.flexDirection = juce::FlexBox::Direction::column;
        column.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        float needed = 0;
        for (auto& [gid, list] : groups)
        {
            if (list.empty()) continue;
            auto row = std::make_unique<juce::FlexBox>();
            row->flexWrap = juce::FlexBox::Wrap::wrap;
            row->justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            row->alignContent = juce::FlexBox::AlignContent::center;
            row->alignItems = juce::FlexBox::AlignItems::center;
            float rowH = 0, x = 0;
            int lines = 1;
            for (auto* it : list)
            {
                float w = it->w, h = it->h;
                if (it->kind == "knob") { w *= ks; h *= ks; }
                if (pads && (it->kind == "choicepad" || it->kind == "pad")) { w *= 1.2f; h *= 1.5f; }
                if (pads && it->comp->getComponentID().startsWith ("curve_shape#")) { w = 56; h = 50; }
                w *= fit; h *= fit;
                row->items.add (juce::FlexItem (*it->comp).withWidth (w).withHeight (h).withMargin (2.0f));
                rowH = std::max (rowH, h + 4.0f);
                if (x + w + 4.0f > content.getWidth() && x > 0) { ++lines; x = 0; }
                x += w + 4.0f;
            }
            const float hh = rowH * (float) lines;
            needed += hh;
            column.items.add (juce::FlexItem (*row).withMinHeight (hh).withFlex (hh));
            rows.push_back (std::move (row));
        }
        return needed;
    };
    juce::FlexBox column;
    std::vector<std::unique_ptr<juce::FlexBox>> rows;
    float fit = 1.0f;
    for (int iter = 0; iter < 6; ++iter)
    {
        const float needed = build (fit, column, rows);
        if (needed <= content.getHeight() + 0.5f) break;
        fit *= std::max (0.6f, content.getHeight() / needed);
    }
    column.performLayout (content);
}

void MainPanel::layoutStandard (juce::Rectangle<float> full)
{
    auto b = full;
    if (theme->rackEars)
    {
        b = b.reduced (40.0f, 0);
    }
    const bool padStation = theme->layout == LayoutVariant::padStation;
    const float g = 8.0f;
    layoutTopBar (b.removeFromTop (54.0f));
    b = b.reduced (g, 0).withTrimmedBottom (g);

    const float leftW = 300.0f, rightW = 352.0f;
    auto left = b.removeFromLeft (leftW);
    b.removeFromLeft (g);
    auto right = b.removeFromRight (rightW);
    b.removeFromRight (g);
    auto centre = b;

    // left column: IR browser, IR display, REVERB
    irBrowser->setBounds (left.removeFromTop (left.getHeight() * 0.29f).toNearestInt());
    left.removeFromTop (g);
    irDisplay->setBounds (left.removeFromTop (left.getHeight() * 0.27f).toNearestInt());
    left.removeFromTop (g);
    sectionRects[IRCOL] = {};
    layoutSection (REVERB, left);

    // centre: envelope LCD, readout strip, DUCK | GATE, DETECT
    auto lcd = centre.removeFromTop (centre.getHeight() * (padStation ? 0.33f : 0.355f));
    visualizer->setBounds (lcd.toNearestInt());
    centre.removeFromTop (g);
    auto strip = centre.removeFromTop (44.0f);
    jog->setBounds (strip.removeFromRight (44.0f).toNearestInt());
    strip.removeFromRight (g);
    freezeBtn->setBounds (strip.removeFromRight (66.0f).reduced (0, 9).toNearestInt());
    strip.removeFromRight (4.0f);
    scaleBtn->setBounds (strip.removeFromRight (66.0f).reduced (0, 9).toNearestInt());
    strip.removeFromRight (g);
    readout->setBounds (strip.toNearestInt());
    centre.removeFromTop (g);
    auto detect = centre.removeFromBottom (padStation ? 82.0f : 92.0f);
    centre.removeFromBottom (g);
    auto duck = centre.removeFromLeft (centre.getWidth() * 0.6f);
    centre.removeFromLeft (g);
    layoutSection (DUCK, duck);
    layoutSection (GATE, centre);
    layoutSection (DETECT, detect);

    // right: TONE, SPACE, MIX
    auto tone = right.removeFromTop (right.getHeight() * 0.47f);
    right.removeFromTop (g);
    auto space = right.removeFromTop (right.getHeight() * 0.36f);
    right.removeFromTop (g);
    layoutSection (TONE, tone);
    layoutSection (SPACE, space);
    layoutSection (MIX, right);
    if (auto it = std::find_if (items.begin(), items.end(), [] (const Item& i) { return i.kind == "meter" && i.section == SPACE; }); it != items.end())
        juce::ignoreUnused (it);

    if (theme->signalArrows)
    {
        auto mid = [&] (int s) { return sectionRects[s]; };
        arrows.push_back ({ { mid (REVERB).getRight() - 2.0f, mid (REVERB).getY() + 14.0f }, { mid (DUCK).getX() + 2.0f, mid (DUCK).getY() + 14.0f } });
        arrows.push_back ({ { mid (DUCK).getRight() - 2.0f, mid (DUCK).getY() + 14.0f }, { mid (GATE).getX() + 2.0f, mid (GATE).getY() + 14.0f } });
        arrows.push_back ({ { mid (GATE).getRight() - 2.0f, mid (GATE).getY() + 14.0f }, { mid (TONE).getX() + 2.0f, mid (TONE).getBottom() - 20.0f } });
        arrows.push_back ({ { mid (TONE).getCentreX(), mid (TONE).getBottom() - 2.0f }, { mid (SPACE).getCentreX(), mid (SPACE).getY() + 2.0f } });
        arrows.push_back ({ { mid (SPACE).getCentreX(), mid (SPACE).getBottom() - 2.0f }, { mid (MIX).getCentreX(), mid (MIX).getY() + 2.0f } });
    }
}

void MainPanel::layoutFloppy (juce::Rectangle<float> full)
{
    auto b = full;
    const float g = 8.0f;
    layoutTopBar (b.removeFromTop (54.0f));
    b = b.reduced (g, 0).withTrimmedBottom (g);

    // upper band: [IR browser + disk slot] [big envelope LCD + strip] [IR display]
    auto upper = b.removeFromTop (b.getHeight() * 0.46f);
    b.removeFromTop (g);
    auto irCol = upper.removeFromLeft (300.0f);
    upper.removeFromLeft (g);
    auto irDisp = upper.removeFromRight (300.0f);
    upper.removeFromRight (g);
    floppySlot = irCol.removeFromBottom (34.0f);
    irCol.removeFromBottom (6.0f);
    irBrowser->setBounds (irCol.toNearestInt());
    irDisplay->setBounds (irDisp.removeFromTop (irDisp.getHeight() - 52.0f).toNearestInt());
    irDisp.removeFromTop (8.0f);
    jog->setBounds (irDisp.removeFromRight (44.0f).toNearestInt());
    irDisp.removeFromRight (g);
    freezeBtn->setBounds (irDisp.removeFromRight (66.0f).reduced (0, 9).toNearestInt());
    irDisp.removeFromRight (4.0f);
    scaleBtn->setBounds (irDisp.removeFromRight (66.0f).reduced (0, 9).toNearestInt());
    auto strip = upper.removeFromBottom (40.0f);
    upper.removeFromBottom (6.0f);
    readout->setBounds (strip.toNearestInt());
    visualizer->setBounds (upper.toNearestInt());

    // lower band: colour-striped sections in signal order
    const int order[] = { REVERB, DETECT, DUCK, GATE, TONE, SPACE, MIX };
    const float weights[] = { 1.25f, 0.62f, 1.35f, 0.82f, 1.2f, 0.62f, 1.0f };
    float total = 0; for (float w : weights) total += w;
    const float avail = b.getWidth() - g * 6.0f;
    for (int i = 0; i < 7; ++i)
    {
        auto r = b.removeFromLeft (avail * weights[i] / total);
        b.removeFromLeft (g);
        layoutSection (order[i], r);
    }
}

void MainPanel::layoutPhotoreal (const SkinLayout& L)
{
    for (auto& it : items)
    {
        auto id = it.comp->getComponentID();
        auto key = id.startsWith ("eqsel_") ? id.upToFirstOccurrenceOf ("#", false, false) : id;
        auto found = L.rects.find (key);
        if (found != L.rects.end())
            it.comp->setBounds (found->second.toNearestInt());
        else
            it.comp->setVisible (false);
    }
    updateEqBandKnobs (eqSelBand);
    // logo is baked into the panel art
    logoArea = {};
}

// ---------------------------------------------------------------------------
// painting
// ---------------------------------------------------------------------------
void MainPanel::paint (juce::Graphics& g)
{
    auto* lnf = freddyLnf (*this);
    if (lnf == nullptr || theme == nullptr) return;
    lnf->drawChassis (g, getLocalBounds().toFloat());
    if (lnf->skinLayout() != nullptr) return;

    for (auto& [idx, r] : sectionRects)
    {
        if (idx == TOP || r.isEmpty()) continue;
        lnf->drawSection (g, r, { sectionTitle (idx), lnf->sectionAccent (idx), idx });
    }
    if (! logoArea.isEmpty()) lnf->drawLogo (g, logoArea.reduced (0, 2));
    for (auto& a : arrows) lnf->drawSignalArrow (g, a.first, a.second);

    if (! floppySlot.isEmpty())
    {
        // disk-drive style IR loader slot
        auto s = floppySlot;
        g.setColour (theme->c.section);
        g.fillRoundedRectangle (s, 3.0f);
        auto slot = s.reduced (12.0f, s.getHeight() * 0.38f).withTrimmedRight (60.0f);
        g.setColour (juce::Colours::black);
        g.fillRect (slot);
        g.setColour (theme->c.accent);
        g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre ({ s.getRight() - 46.0f, s.getCentreY() }));
        g.setColour (theme->c.sectionLabel);
        g.setFont (lnf->labelFont (11.0f));
        g.drawText ("IR DISK", s.removeFromRight (36.0f), juce::Justification::centred);
    }
}

void MainPanel::paintOverChildren (juce::Graphics& g)
{
    if (auto* lnf = freddyLnf (*this)) lnf->drawOverlay (g, getLocalBounds().toFloat());
}
} // namespace fv::ui
