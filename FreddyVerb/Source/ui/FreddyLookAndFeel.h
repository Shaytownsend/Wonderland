#pragma once

#include "Theme.h"
#include "PixelFont.h"
#include <map>

namespace fv::ui
{
/** State passed to knob drawing. */
struct KnobState
{
    float value = 0.0f;         // normalised 0..1
    float defaultValue = 0.0f;  // normalised; arcs start here for bipolar params
    bool  bipolar = false;
    bool  hover = false, dragging = false, enabled = true;
    juce::Colour accent;        // per-section accent (duck / gate colours)
    juce::String label, valueText;
    juce::String paramID;
    juce::String componentID;   // == paramID except shared slots (e.g. eqsel_gain#2)
};

struct PadState
{
    juce::String text;
    bool on = false, hover = false, down = false, enabled = true;
    bool hasLed = true;
    juce::Colour accent;
    juce::String paramID;
    juce::String componentID;   // e.g. "duck_mode#1", "undo"
    int  glyph = -1;            // >= 0: draw curve-shape icon (CurveShape index)
};

/** A section rectangle (REVERB, DUCK, ...) with its printed label. */
struct SectionInfo
{
    juce::String title;
    juce::Colour accent;
    int index = 0;              // 0 REVERB, 1 DUCK, 2 GATE, 3 TONE, 4 SPACE, 5 MIX, 6 DETECT, 7 TOP
};

/** Absolute control rectangles for photoreal skins (from the skin's layout.json),
    keyed by component ID, in logical (100%) editor coordinates. */
struct SkinLayout
{
    std::map<juce::String, juce::Rectangle<float>> rects;
    std::map<juce::String, juce::String> kinds;
};

/** Base LookAndFeel for every Freddy Verb theme. All drawing goes through these
    virtuals; a theme subclass overrides only what makes it different. The
    default implementations are the flat "Putty Classic" look driven entirely
    by FreddyTheme colours and style enums. */
class FreddyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit FreddyLookAndFeel (const FreddyTheme& t);
    ~FreddyLookAndFeel() override = default;

    const FreddyTheme& theme() const noexcept { return t; }

    // --- Panels -----------------------------------------------------------
    virtual void drawChassis (juce::Graphics&, juce::Rectangle<float> bounds);
    virtual void drawSection (juce::Graphics&, juce::Rectangle<float> bounds, const SectionInfo&);
    virtual void drawSignalArrow (juce::Graphics&, juce::Point<float> from, juce::Point<float> to);
    virtual void drawLogo (juce::Graphics&, juce::Rectangle<float> bounds);
    virtual void drawRackEars (juce::Graphics&, juce::Rectangle<float> full, float earWidth);

    /** Photoreal skins return their layout; flat themes return nullptr. */
    virtual const SkinLayout* skinLayout() const { return nullptr; }

    /** Optional overlay drawn above all child components (photoreal glass, dust). */
    virtual void drawOverlay (juce::Graphics&, juce::Rectangle<float>) {}

    // --- Controls ---------------------------------------------------------
    virtual void drawKnob (juce::Graphics&, juce::Rectangle<float> bounds, const KnobState&);
    virtual void drawPad (juce::Graphics&, juce::Rectangle<float> bounds, const PadState&);
    virtual void drawControlLabel (juce::Graphics&, juce::Rectangle<float> bounds, const juce::String& text,
                                   bool onSection, juce::Justification = juce::Justification::centred);
    virtual void drawJogWheel (juce::Graphics&, juce::Rectangle<float> bounds, float angle, bool active);
    virtual void drawMeter (juce::Graphics&, juce::Rectangle<float> bounds, float peakDb, float rmsDb, const juce::String& label);
    virtual void drawCorrelation (juce::Graphics&, juce::Rectangle<float> bounds, float correlation);
    virtual void drawCurveGlyph (juce::Graphics&, juce::Rectangle<float> bounds, int shape, juce::Colour);

    // --- LCD / displays ----------------------------------------------------
    /** Background of an LCD/VFD/LED display, including bezel. Returns the inner content area. */
    virtual juce::Rectangle<float> drawDisplayFrame (juce::Graphics&, juce::Rectangle<float> bounds);
    /** Pixel grid / scanlines / glass drawn on top of the display contents. */
    virtual void drawDisplayOverlay (juce::Graphics&, juce::Rectangle<float> inner);
    /** Readout text inside a display (dot matrix / 7-seg / VFD). */
    virtual void drawDisplayText (juce::Graphics&, const juce::String&, juce::Rectangle<float>,
                                  juce::Justification, bool dim = false);
    /** Larger value readout (7-segment on LED themes). */
    virtual void drawValueReadout (juce::Graphics&, const juce::String&, juce::Rectangle<float>);
    /** Glow/persistence factor for envelope traces (CRT/VFD themes). */
    virtual float traceGlow() const { return 0.0f; }

    // --- Fonts --------------------------------------------------------------
    juce::Font labelFont (float height) const;      // condensed screen-print font
    juce::Font monoFont (float height) const;       // mono readout font

    // --- LookAndFeel_V4 overrides for stock widgets ------------------------
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    juce::Font getPopupMenuFont() override;
    void drawTooltip (juce::Graphics&, const juce::String&, int, int) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void fillTextEditorBackground (juce::Graphics&, int, int, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int, int, int, int, bool, int, int, bool, bool) override;

    /** Colour used for a section accent (duck = accent, gate = secondary, ...). */
    juce::Colour sectionAccent (int sectionIndex) const;

    /** Returns true if text drawn on `bg` in `fg` meets WCAG AA (4.5:1). */
    static float contrastRatio (juce::Colour fg, juce::Colour bg);

protected:
    FreddyTheme t;
    juce::Typeface::Ptr condensed, condensedMedium, mono;
};
} // namespace fv::ui
