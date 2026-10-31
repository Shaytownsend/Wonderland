#pragma once

#include "Controls.h"
#include "EnvelopeVisualizer.h"
#include "Displays.h"
#include "IRBrowser.h"
#include <set>

namespace fv { class FreddyVerbProcessor; }

namespace fv::ui
{
/** The whole plugin face. Owns every control exactly once; themes only change
    how they are drawn and where they are placed. */
class MainPanel : public juce::Component, private ControlHub::Listener
{
public:
    MainPanel (FreddyVerbProcessor&, ControlHub&);
    ~MainPanel() override;

    /** Re-layouts for the given theme (the LookAndFeel is already set on us). */
    void themeChanged (const FreddyTheme&);

    /** 60 fps tick from the editor. */
    void tick();

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    EnvelopeVisualizer& getVisualizer() { return *visualizer; }
    IRDisplay& getIRDisplay() { return *irDisplay; }
    IRBrowser& getIRBrowser() { return *irBrowser; }

    /** Every control/display as (componentID, kind, section). Used to export the
        manifest that tools/panel_forge lays the photoreal skins out from. */
    struct ManifestEntry { juce::String id, kind, section; float prefW, prefH; };
    std::vector<ManifestEntry> manifest() const;

    std::function<void()> onSettings;
    std::function<void()> onPresetMenu;
    std::function<void (int)> onPresetStep;
    std::function<void (int)> onABSelect;   // 0 = A, 1 = B, 2 = copy A->B
    std::function<void()> onUndo, onRedo;
    juce::Label presetName;

    /** For the static mockups: text for the preset LCD. */
    void setPresetDisplay (const juce::String& name, bool dirty, int abSlot);

private:
    struct Item
    {
        juce::Component* comp = nullptr;
        int section = 0;
        float w = 50, h = 60;   // preferred size at knobScale 1
        juce::String kind;
        int group = 0;          // sub-row grouping inside a section
    };

    void controlTouched (const juce::String& id) override;
    FreddyKnob*    addKnob (const juce::String& id, int section, int group = 0, bool bipolar = false);
    PadButton*     addPad (const juce::String& id, const juce::String& text, int section, int group = 0, float w = 56);
    PadButton*     addChoicePad (const juce::String& id, int idx, const juce::String& text, int section, int group = 0, float w = 56);
    ChoiceStepper* addStepper (const juce::String& id, int section, int group = 0, float w = 104);
    PadButton*     addButton (const juce::String& cid, const juce::String& text, int section, float w = 40);
    void addOther (juce::Component* c, const juce::String& cid, const juce::String& kind, int section, float w, float h);

    void layoutStandard (juce::Rectangle<float> full);
    void layoutFloppy (juce::Rectangle<float> full);
    void layoutPhotoreal (const SkinLayout&);
    void layoutTopBar (juce::Rectangle<float>);
    float layoutSection (int section, juce::Rectangle<float> r, float maxFit = 1.2f, bool place = true);
    void layoutSections (const std::vector<std::pair<int, juce::Rectangle<float>>>&);
    void layoutPadBank (juce::Rectangle<float> r);
    Item* findItem (const juce::String& componentID);
    void updateDuckModeDimming();
    void updateEqBandKnobs (int band);
    void refreshIRBrowser();
    void syncIRDisplay();
    void chooseIRFile (bool relink);
    void loadUserIR (const juce::File&);
    std::unique_ptr<juce::FileChooser> chooser;
    int irVersionSeen = -1;

    FreddyVerbProcessor& proc;
    ControlHub& hub;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Item> items;

    std::unique_ptr<EnvelopeVisualizer> visualizer;
    std::unique_ptr<IRDisplay> irDisplay;
    std::unique_ptr<IRBrowser> irBrowser;
    std::unique_ptr<EQDisplay> eqDisplay;
    std::unique_ptr<JogWheel> jog;
    std::unique_ptr<LevelMeter> meterIn, meterWet, meterOut;
    std::unique_ptr<CorrelationMeter> correlation;

    struct Readout;
    std::unique_ptr<Readout> readout;
    struct PresetLcd;
    std::unique_ptr<PresetLcd> presetLcd;

    std::vector<FreddyKnob*> duckFollowOnly, duckCurveOnly;
    std::vector<juce::Component*> duckCurveOnlyOther, duckFollowOnlyOther;
    FreddyKnob* bandKnobs[4][3] {};    // LS, B1, B2, HS x (freq, gain, q)
    int eqSelBand = 1;
    PadButton* scaleBtn = nullptr;
    PadButton* freezeBtn = nullptr;

    std::map<int, juce::Rectangle<float>> sectionRects;
    std::vector<std::pair<juce::Point<float>, juce::Point<float>>> arrows;
    juce::Rectangle<float> logoArea, detailArea;
    std::set<juce::Component*> placedElsewhere;     // laid out outside their own section (pad bank)
    std::vector<std::pair<juce::Rectangle<float>, juce::String>> captions;
    struct DiskSlot;
    std::unique_ptr<DiskSlot> diskSlot;             // Floppy Era Pro IR loader (not a manifest control)
    const FreddyTheme* theme = nullptr;
    int lastDuckMode = -1;
};
} // namespace fv::ui
