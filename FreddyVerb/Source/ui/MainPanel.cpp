#include "MainPanel.h"
#include "../PluginProcessor.h"
#include "../state/Parameters.h"

namespace fv::ui
{
namespace
{
    enum Sec { REVERB = 0, DUCK, GATE, TONE, SPACE, MIX, DETECT, TOP, IRCOL, LCD, PADS };
    const char* sectionTitle (int s)
    {
        switch (s)
        {
            case REVERB: return "REVERB"; case DUCK: return "DUCK"; case GATE: return "GATE";
            case TONE: return "TONE"; case SPACE: return "SPACE"; case MIX: return "MIX";
            case DETECT: return "DETECT"; case PADS: return "PAD BANK"; default: return "";
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
        // hovered control first, otherwise the last touched one (the jog wheel's target)
        const auto shown = hub.getHovered().isNotEmpty() ? hub.getHovered() : hub.getLastTouched();
        auto* p = hub.apvts.getParameter (shown);
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

/** Disk-drive style IR loader (Floppy Era Pro): click to load an IR / add a folder. */
struct MainPanel::DiskSlot : public juce::Component, public juce::SettableTooltipClient
{
    DiskSlot()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setRepaintsOnMouseActivity (true);
        setTooltip ("IR disk drive: load an impulse response file or add an IR folder");
        setTitle ("Load IR");
    }
    void paint (juce::Graphics& g) override
    {
        if (auto* lnf = freddyLnf (*this)) lnf->drawDiskSlot (g, getLocalBounds().toFloat(), isMouseOver (true), down);
    }
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        down = false;
        repaint();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }
    std::function<void()> onClick;
    bool down = false;
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
    addStepper (quality, REVERB, 0, 128);
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
    duckFollowOnlyOther.push_back (addStepper (duckRelCurve, DUCK, 0, 116));
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
    duckCurveOnlyOther.push_back (addPad (curveSync, "SYNC", DUCK, 4, 52));
    duckCurveOnlyOther.push_back (addStepper (curveNote, DUCK, 4, 78));
    for (auto& it : items)
        if (it.section == DUCK)
        {
            if (auto* k = dynamic_cast<FreddyKnob*> (it.comp)) k->setAccent (juce::Colour());
        }

    // --- GATE ---------------------------------------------------------------------------
    addPad (gateOn, "GATE", GATE, 0, 62);
    addStepper (gateRelCurve, GATE, 0, 116);
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
    irBrowser->onBrowse = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "Load IR file...");
        m.addItem (2, "Add IR folder...");
        const auto folders = juce::StringArray::fromTokens (proc.getUIState().getProperty ("irFolders").toString(), "|", "");
        if (! folders.isEmpty())
        {
            juce::PopupMenu rm;
            for (int i = 0; i < folders.size(); ++i) rm.addItem (100 + i, folders[i]);
            m.addSubMenu ("Remove IR folder", rm);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (irBrowser.get()), [this, folders] (int r)
        {
            if (r == 1) chooseIRFile (false);
            else if (r == 2)
            {
                chooser = std::make_unique<juce::FileChooser> ("Add IR folder", juce::File::getSpecialLocation (juce::File::userHomeDirectory));
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                      [this] (const juce::FileChooser& fc)
                                      {
                                          const auto d = fc.getResult();
                                          if (! d.isDirectory()) return;
                                          auto list = juce::StringArray::fromTokens (proc.getUIState().getProperty ("irFolders").toString(), "|", "");
                                          list.addIfNotAlreadyThere (d.getFullPathName());
                                          proc.getUIState().setProperty ("irFolders", list.joinIntoString ("|"), nullptr);
                                          refreshIRBrowser();
                                      });
            }
            else if (r >= 100)
            {
                auto list = folders;
                list.remove (r - 100);
                proc.getUIState().setProperty ("irFolders", list.joinIntoString ("|"), nullptr);
                refreshIRBrowser();
            }
        });
    };
    irBrowser->onFilesDropped = [this] (const juce::StringArray& files) { if (! files.isEmpty()) loadUserIR (juce::File (files[0])); };
    irBrowser->onFavourite = [this] (const IRListItem& it, bool fav)
    {
        auto favs = juce::StringArray::fromTokens (proc.getUIState().getProperty ("irFavourites").toString(), "|", "");
        favs.removeString (it.id);
        if (fav) favs.add (it.id);
        proc.getUIState().setProperty ("irFavourites", favs.joinIntoString ("|"), nullptr);
    };
    irDisplay->onRelinkRequested = [this] { chooseIRFile (true); };
    diskSlot = std::make_unique<DiskSlot>();
    diskSlot->onClick = [this] { if (irBrowser->onBrowse) irBrowser->onBrowse(); };
    addChildComponent (*diskSlot);
    refreshIRBrowser();
}

void MainPanel::refreshIRBrowser()
{
    const auto favs = juce::StringArray::fromTokens (proc.getUIState().getProperty ("irFavourites").toString(), "|", "");
    std::vector<IRListItem> list;
    for (auto& e : proc.getIRLibrary().factory())
    {
        // RT60 is meaningless for gated / non-linear / reversed IRs: show their length instead
        const bool shaped = e.category == "Non-Linear" || e.id.contains ("reverse");
        list.push_back ({ e.id, e.name, e.category, e.description, shaped ? e.lengthSeconds : e.rt60, favs.contains (e.id), true });
    }
    // remembered user folders (scanned recursively, capped)
    for (auto& folder : juce::StringArray::fromTokens (proc.getUIState().getProperty ("irFolders").toString(), "|", ""))
    {
        int count = 0;
        for (const auto& entry : juce::RangedDirectoryIterator (juce::File (folder), true, "*.wav;*.aif;*.aiff;*.flac"))
        {
            const auto f = entry.getFile();
            list.push_back ({ f.getFullPathName(), f.getFileNameWithoutExtension(), "User", f.getParentDirectory().getFileName(),
                              0.0, favs.contains (f.getFullPathName()), false });
            if (++count >= 1000) break;
        }
    }
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

void MainPanel::controlHovered (const juce::String&)
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
    const float dim = theme != nullptr && theme->layout == LayoutVariant::padStation ? 0.5f : 0.4f;
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
    lastDuckMode = -1;
    updateDuckModeDimming();
    resized();
    repaint();
}

void MainPanel::resized()
{
    if (theme == nullptr) return;
    sectionRects.clear();
    arrows.clear();
    captions.clear();
    placedElsewhere.clear();
    detailArea = {};
    for (auto& it : items) it.comp->setVisible (true);
    diskSlot->setVisible (false);
    updateEqBandKnobs (eqSelBand);

    auto full = getLocalBounds().toFloat();
    if (auto* lnf = freddyLnf (*this); lnf != nullptr && lnf->skinLayout() != nullptr)
        layoutPhotoreal (*lnf->skinLayout());
    else if (theme->layout == LayoutVariant::floppyWide)
        layoutFloppy (full);
    else
        layoutStandard (full);
}

MainPanel::Item* MainPanel::findItem (const juce::String& id)
{
    for (auto& it : items)
        if (it.comp->getComponentID() == id) return &it;
    return nullptr;
}

void MainPanel::layoutTopBar (juce::Rectangle<float> bar)
{
    sectionRects[TOP] = bar;
    auto r = bar.reduced (10.0f, 9.0f);
    logoArea = r.removeFromLeft (290.0f);
    auto place = [&] (const juce::String& id, float w, float gap = 6.0f)
    {
        if (auto* it = findItem (id)) { it->comp->setBounds (r.removeFromLeft (w).toNearestInt()); r.removeFromLeft (gap); }
    };
    r.removeFromLeft (10.0f);
    place ("preset_prev", 30, 4);
    place ("preset", std::min (340.0f, r.getWidth() * 0.42f), 4);
    place ("preset_next", 30, 18);
    place ("ab_a", 32, 4); place ("ab_b", 32, 4); place ("ab_copy", 46, 18);
    place ("undo", 52, 4); place ("redo", 52, 18);
    // settings at far right
    auto settings = bar.reduced (10.0f, 9.0f).removeFromRight (64.0f);
    if (auto* it = findItem ("settings")) it->comp->setBounds (settings.toNearestInt());
    detailArea = r.withRight (settings.getX() - 18.0f);
    if (detailArea.getWidth() < 30.0f) detailArea = {};
}

/*  Grid layout of one section. Each item group becomes one or more rows; a group
    that does not fit on one line is split into *balanced* rows (5 -> 3+2, 7 -> 4+3,
    never a lone orphan) and uniform knob rows share equal-width cells so columns
    line up. Everything is scaled by one factor (up to 1.2x when there is room,
    down to 0.6x when crowded) so all controls in a section stay the same size. */
float MainPanel::layoutSection (int section, juce::Rectangle<float> r, float maxFit, bool place)
{
    sectionRects[section] = r;
    const float ks = theme->knobScale;
    const bool pads = theme->layout == LayoutVariant::padStation;
    const float header = theme->labelSize + (theme->sectionStripes ? 16.0f : 10.0f);
    auto content = r.reduced (10.0f, 6.0f).withTrimmedTop (header);

    std::map<int, std::vector<Item*>> groups;
    for (auto& it : items)
        if (it.section == section && it.comp->isVisible() && placedElsewhere.count (it.comp) == 0)
            groups[it.group].push_back (&it);
    if (groups.empty()) return 1.0f;
    {
        // merge consecutive knob-only groups so they share one balanced grid
        auto allKnobs = [] (const std::vector<Item*>& v) { return ! v.empty() && std::all_of (v.begin(), v.end(), [] (Item* i) { return i->kind == "knob"; }); };
        std::map<int, std::vector<Item*>> merged;
        int prev = -1;
        for (auto& [gid, list] : groups)
        {
            if (prev >= 0 && allKnobs (list) && allKnobs (merged[prev]))
                merged[prev].insert (merged[prev].end(), list.begin(), list.end());
            else { merged[gid] = list; prev = gid; }
        }
        groups = std::move (merged);
    }

    if (section == TONE)
        for (auto& g : groups)
            for (auto it = g.second.begin(); it != g.second.end(); )
            {
                if ((*it)->comp == eqDisplay.get())
                {
                    if (place) eqDisplay->setBounds (content.removeFromTop (std::max (70.0f, content.getHeight() * 0.36f)).toNearestInt());
                    content.removeFromTop (6.0f);
                    it = g.second.erase (it);
                }
                else ++it;
            }
    if (section == MIX)
    {
        auto meters = content.removeFromRight (86.0f).reduced (0, 2.0f);
        content.removeFromRight (6.0f);
        const float mw = 22.0f;
        int i = 0;
        for (auto* m : { meterIn.get(), meterWet.get(), meterOut.get() })
            if (place) m->setBounds (juce::Rectangle<float> (meters.getX() + (float) i++ * (mw + 6.0f), meters.getY(), mw, meters.getHeight()).toNearestInt());
        for (auto& g : groups)
            g.second.erase (std::remove_if (g.second.begin(), g.second.end(), [] (Item* it) { return it->kind == "meter"; }), g.second.end());
    }

    auto baseSize = [&] (const Item* it)
    {
        float w = it->w, h = it->h;
        if (it->kind == "knob") { w *= ks; h *= ks; }
        if (pads && (it->kind == "choicepad" || it->kind == "pad")) { w *= 1.15f; h *= 1.4f; }
        if (it->kind == "stepper") h = std::max (h, 40.0f);
        return std::pair<float, float> { w, h };
    };
    // only knobs grow beyond their preferred size; pads/steppers/displays keep it
    auto scaleOf = [] (const Item* it, float f) { return it->kind == "knob" ? f : std::min (f, 1.0f); };

    struct Row { std::vector<Item*> list; float h = 0; bool uniform = false; int cols = 0; float cellW = 0; };
    const float W = content.getWidth(), H = content.getHeight();
    const float gapX = 6.0f, gapY = 6.0f;

    // returns the height needed, or a huge value if a row is too wide at this scale
    auto arrange = [&] (float f, std::vector<Row>& rows) -> float
    {
        rows.clear();
        const float fl = std::min (f, 1.0f);   // line breaks are decided at <= preferred size
        bool tooWide = false;
        for (auto& [gid, list] : groups)
        {
            const int n = (int) list.size();
            if (n == 0) continue;
            bool uniform = true;
            for (auto* it : list) uniform = uniform && it->kind == list.front()->kind && it->w == list.front()->w;
            auto rowWidth = [&] (int from, int cnt, float sc)
            {
                float w = 0;
                for (int k = 0; k < cnt; ++k)
                {
                    auto* it = list[(size_t) (from + k)];
                    w += baseSize (it).first * scaleOf (it, sc) + (k > 0 ? (uniform ? 2.0f : gapX) : 0.0f);
                }
                return w;
            };
            // smallest number of balanced lines in which every line fits
            int lines = 1;
            for (; lines < n; ++lines)
            {
                bool fits = true;
                const int base = n / lines, extra = n % lines;
                for (int l = 0, idx = 0; l < lines && fits; ++l)
                {
                    const int cnt = base + (l < extra ? 1 : 0);
                    fits = rowWidth (idx, cnt, fl) <= W + 0.5f;
                    idx += cnt;
                }
                if (fits) break;
            }
            const int base = n / lines, extra = n % lines;
            const int cols = base + (extra > 0 ? 1 : 0);
            float maxW = 0;
            for (auto* it : list) maxW = std::max (maxW, baseSize (it).first * scaleOf (it, f));
            for (int l = 0, idx = 0; l < lines; ++l)
            {
                Row row;
                row.uniform = uniform;
                row.cols = cols;
                row.cellW = uniform ? std::min (W / (float) cols, maxW * 1.9f) : 0.0f;
                const int cnt = base + (l < extra ? 1 : 0);
                if (rowWidth (idx, cnt, f) > W + 0.5f) tooWide = true;
                for (int k = 0; k < cnt; ++k)
                {
                    auto* it = list[(size_t) idx++];
                    row.list.push_back (it);
                    row.h = std::max (row.h, baseSize (it).second * scaleOf (it, f));
                }
                rows.push_back (std::move (row));
            }
        }
        float total = 0;
        for (auto& row : rows) total += row.h;
        total += gapY * (float) std::max (0, (int) rows.size() - 1);
        return tooWide ? 1.0e9f : total;
    };

    std::vector<Row> rows;
    float f = maxFit;
    for (; f > 0.6f; f -= 0.02f)
        if (arrange (f, rows) <= H) break;
    f = std::max (0.6f, f);
    if (! place) return f;
    float needed = arrange (f, rows);
    if (needed > 1.0e8f) { f = 0.6f; needed = arrange (f, rows); }

    // distribute spare height evenly between and around rows (capped so sparse
    // sections stay grouped rather than drifting apart)
    const float spare = std::max (0.0f, H - needed);
    const float gap = std::min (spare / (float) (rows.size() + 1), 26.0f);
    float y = content.getY() + (H - needed - gap * (float) (rows.size() - 1)) * 0.5f;
    for (auto& row : rows)
    {
        if (row.uniform)
        {
            const float blockW = row.cellW * (float) row.list.size();
            float x = content.getCentreX() - blockW * 0.5f;
            for (auto* it : row.list)
            {
                auto [w, h] = baseSize (it);
                const float sc = scaleOf (it, f);
                it->comp->setBounds (juce::Rectangle<float> (w * sc, h * sc)
                                        .withCentre ({ x + row.cellW * 0.5f, y + row.h * 0.5f }).toNearestInt());
                x += row.cellW;
            }
        }
        else
        {
            float sum = 0;
            for (auto* it : row.list) sum += baseSize (it).first * scaleOf (it, f);
            const float sp = std::min ((W - sum) / (float) (row.list.size() + 1), 40.0f);
            float x = content.getCentreX() - (sum + sp * (float) (row.list.size() - 1)) * 0.5f;
            for (auto* it : row.list)
            {
                auto [w, h] = baseSize (it);
                const float sc = scaleOf (it, f);
                it->comp->setBounds (juce::Rectangle<float> (x, y + (row.h - h * sc) * 0.5f, w * sc, h * sc).toNearestInt());
                x += w * sc + sp;
            }
        }
        y += row.h + gap;
    }
    return f;
}

/*  Lays out several sections with a shared knob scale so neighbouring panels use
    the same knob size (like real hardware); a roomy section may go up to 12 %
    larger than the most crowded one. */
void MainPanel::layoutSections (const std::vector<std::pair<int, juce::Rectangle<float>>>& list)
{
    float minFit = 1.2f;
    for (auto& [sec, r] : list) minFit = std::min (minFit, layoutSection (sec, r, 1.2f, false));
    for (auto& [sec, r] : list) layoutSection (sec, r, std::min (1.2f, minFit * 1.12f), true);
}

/*  Pad Station: a full-width bank of large square rubber pads, drum-machine style.
    ENABLE (duck, gate) | DUCK MODE (follow, curve) | CURVE SHAPE (7) | CURVE SYNC. */
void MainPanel::layoutPadBank (juce::Rectangle<float> r)
{
    using namespace fv::pid;
    sectionRects[PADS] = r;
    std::vector<std::pair<juce::String, std::vector<juce::String>>> bank {
        { "ENABLE",      { duckOn, gateOn } },
        { "DUCK MODE",   { juce::String (duckMode) + "#0", juce::String (duckMode) + "#1" } },
        { "CURVE SHAPE", {} },
        { "SYNC",        { curveSync } } };
    for (int i = 0; i < choices::numCurveShapes; ++i) bank[2].second.push_back (juce::String (curveShape) + "#" + juce::String (i));

    auto content = r.reduced (14.0f, 6.0f).withTrimmedTop (theme->labelSize + 10.0f);
    auto capRow = content.removeFromTop (16.0f);
    auto numRow = content.removeFromBottom (15.0f);
    content.removeFromBottom (3.0f);
    int n = 0; for (auto& b : bank) n += (int) b.second.size();
    const float gap = 10.0f, groupGap = 30.0f;
    const float cell = (content.getWidth() - gap * (float) (n - (int) bank.size()) - groupGap * (float) (bank.size() - 1)) / (float) n;
    const float pad = std::min (cell, content.getHeight());
    float x = content.getX();
    int step = 1;
    for (size_t gi = 0; gi < bank.size(); ++gi)
    {
        const float gx = x;
        for (auto& id : bank[gi].second)
        {
            if (auto* it = findItem (id))
            {
                auto cellR = juce::Rectangle<float> (x, content.getY(), cell, content.getHeight());
                it->comp->setBounds (cellR.withSizeKeepingCentre (pad, pad).toNearestInt());
                placedElsewhere.insert (it->comp);
                captions.push_back ({ juce::Rectangle<float> (x, numRow.getY(), cell, numRow.getHeight()), juce::String (step) });
            }
            ++step;
            x += cell + gap;
        }
        x -= gap;
        captions.push_back ({ juce::Rectangle<float> (gx, capRow.getY(), x - gx, capRow.getHeight()), "#" + bank[gi].first });
        x += groupGap;
    }
}

void MainPanel::layoutStandard (juce::Rectangle<float> full)
{
    auto b = full;
    if (theme->rackEars)
        b = b.reduced (40.0f, 0);
    const bool padStation = theme->layout == LayoutVariant::padStation;
    const float g = theme->signalArrows ? 20.0f : 8.0f;   // wide gutters carry the printed arrows
    layoutTopBar (b.removeFromTop (FreddyLookAndFeel::topBarHeight));
    b = b.reduced (theme->signalArrows ? 12.0f : g, 0).withTrimmedBottom (theme->signalArrows ? 22.0f : g);
    b.removeFromTop (theme->rackEars ? 2.0f : 4.0f);

    if (padStation)
    {
        layoutPadBank (b.removeFromBottom (166.0f));
        b.removeFromBottom (g);
    }

    const float leftW = theme->signalArrows ? 292.0f : 300.0f, rightW = theme->signalArrows ? 340.0f : 352.0f;
    auto left = b.removeFromLeft (leftW);
    b.removeFromLeft (g);
    auto right = b.removeFromRight (rightW);
    b.removeFromRight (g);
    auto centre = b;

    // left column: IR browser, IR display, REVERB
    irBrowser->setBounds (left.removeFromTop (std::round (left.getHeight() * (padStation ? 0.3f : 0.29f))).toNearestInt());
    left.removeFromTop (8.0f);
    irDisplay->setBounds (left.removeFromTop (std::round (left.getHeight() * (padStation ? 0.25f : 0.27f))).toNearestInt());
    left.removeFromTop (theme->signalArrows ? 12.0f : g);
    sectionRects[IRCOL] = {};
    const auto reverbRect = left;

    // centre: envelope LCD, readout strip, DUCK | GATE, DETECT
    auto lcd = centre.removeFromTop (std::round (centre.getHeight() * (padStation ? 0.37f : 0.345f)));
    visualizer->setBounds (lcd.toNearestInt());
    centre.removeFromTop (8.0f);
    auto strip = centre.removeFromTop (44.0f);
    jog->setBounds (strip.removeFromRight (44.0f).toNearestInt());
    strip.removeFromRight (8.0f);
    freezeBtn->setBounds (strip.removeFromRight (66.0f).reduced (0, 9).toNearestInt());
    strip.removeFromRight (4.0f);
    scaleBtn->setBounds (strip.removeFromRight (66.0f).reduced (0, 9).toNearestInt());
    strip.removeFromRight (8.0f);
    readout->setBounds (strip.toNearestInt());
    centre.removeFromTop (theme->signalArrows ? 10.0f : g);
    auto detect = centre.removeFromBottom (86.0f);
    centre.removeFromBottom (g);
    auto duck = centre.removeFromLeft (std::round (centre.getWidth() * 0.58f));
    centre.removeFromLeft (g);

    // right: TONE, SPACE, MIX
    auto tone = right.removeFromTop (std::round (right.getHeight() * 0.47f));
    right.removeFromTop (g);
    auto space = right.removeFromTop (std::round (right.getHeight() * 0.36f));
    right.removeFromTop (g);
    layoutSections ({ { REVERB, reverbRect }, { DUCK, duck }, { GATE, centre }, { DETECT, detect },
                      { TONE, tone }, { SPACE, space }, { MIX, right } });

    if (theme->signalArrows)
    {
        // printed signal flow: REVERB -> DUCK -> GATE -> TONE -> SPACE -> MIX
        auto sr = [&] (int s) { return sectionRects[s]; };
        auto across = [&] (int a, int b)
        {
            const float y = std::max (sr (a).getY(), sr (b).getY()) + 30.0f;
            arrows.push_back ({ { sr (a).getRight() + 2.0f, y }, { sr (b).getX() - 2.0f, y } });
        };
        auto down = [&] (int a, int b)
        {
            const float x = sr (a).getCentreX();
            arrows.push_back ({ { x, sr (a).getBottom() + 2.0f }, { x, sr (b).getY() - 2.0f } });
        };
        across (REVERB, DUCK);
        across (DUCK, GATE);
        across (GATE, TONE);
        down (TONE, SPACE);
        down (SPACE, MIX);
    }
}

void MainPanel::layoutFloppy (juce::Rectangle<float> full)
{
    auto b = full;
    const float g = 8.0f;
    layoutTopBar (b.removeFromTop (FreddyLookAndFeel::topBarHeight));
    b = b.reduced (g + 2.0f, 0).withTrimmedBottom (g + 2.0f);
    b.removeFromTop (6.0f);

    // upper band: [IR browser + disk drive] [big envelope LCD + readout] [IR display + data entry]
    auto upper = b.removeFromTop (338.0f);
    b.removeFromTop (12.0f);
    auto irCol = upper.removeFromLeft (318.0f);
    upper.removeFromLeft (12.0f);
    auto irDisp = upper.removeFromRight (318.0f);
    upper.removeFromRight (12.0f);

    auto slot = irCol.removeFromBottom (74.0f);
    irCol.removeFromBottom (8.0f);
    irBrowser->setBounds (irCol.toNearestInt());
    diskSlot->setBounds (slot.toNearestInt());
    diskSlot->setVisible (true);

    auto dataRow = irDisp.removeFromBottom (54.0f);
    irDisp.removeFromBottom (8.0f);
    irDisplay->setBounds (irDisp.toNearestInt());
    jog->setBounds (dataRow.removeFromRight (54.0f).toNearestInt());
    dataRow.removeFromRight (12.0f);
    freezeBtn->setBounds (dataRow.removeFromRight (80.0f).reduced (0, 12).toNearestInt());
    dataRow.removeFromRight (6.0f);
    scaleBtn->setBounds (dataRow.removeFromRight (80.0f).reduced (0, 12).toNearestInt());
    captions.push_back ({ dataRow.withTrimmedRight (6.0f).withSizeKeepingCentre (dataRow.getWidth() - 6.0f, 16.0f), "!DATA ENTRY" });

    auto strip = upper.removeFromBottom (46.0f);
    upper.removeFromBottom (8.0f);
    readout->setBounds (strip.toNearestInt());
    visualizer->setBounds (upper.toNearestInt());

    // lower band: colour-striped sections in signal order
    const int order[] = { REVERB, DETECT, DUCK, GATE, TONE, SPACE, MIX };
    const float weights[] = { 1.45f, 0.62f, 1.5f, 0.9f, 1.32f, 0.68f, 1.0f };
    float total = 0; for (float w : weights) total += w;
    const float avail = b.getWidth() - g * 6.0f;
    std::vector<std::pair<int, juce::Rectangle<float>>> secs;
    for (int i = 0; i < 7; ++i)
    {
        secs.push_back ({ order[i], b.removeFromLeft (std::round (avail * weights[i] / total)) });
        b.removeFromLeft (g);
    }
    layoutSections (secs);
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
        lnf->drawSection (g, r, { sectionTitle (idx), lnf->sectionAccent (idx == PADS ? DUCK : idx), idx });
    }
    if (! logoArea.isEmpty()) lnf->drawLogo (g, logoArea.reduced (0, 2));
    if (! detailArea.isEmpty()) lnf->drawPanelDetail (g, detailArea);
    for (auto& a : arrows) lnf->drawSignalArrow (g, a.first, a.second);
    for (auto& [r, text] : captions)
    {
        if (text.startsWith ("#"))
        {
            // group caption with a printed bracket line under it
            lnf->drawCaption (g, r.withTrimmedBottom (4.0f), text.substring (1));
            g.setColour (theme->c.sectionLabel.withAlpha (0.5f));
            g.fillRect (r.getX() + 2.0f, r.getBottom() - 2.0f, r.getWidth() - 4.0f, 1.0f);
        }
        else if (text.startsWith ("!"))
            lnf->drawControlLabel (g, r, text.substring (1), false, juce::Justification::centredRight);
        else
            lnf->drawCaption (g, r, text);
    }
}

void MainPanel::paintOverChildren (juce::Graphics& g)
{
    if (auto* lnf = freddyLnf (*this)) lnf->drawOverlay (g, getLocalBounds().toFloat());
}
} // namespace fv::ui
