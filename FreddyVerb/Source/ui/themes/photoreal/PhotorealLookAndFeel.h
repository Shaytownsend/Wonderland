#pragma once

#include "../../FreddyLookAndFeel.h"

namespace fv::ui
{
/** Raw access to the embedded files of one photoreal skin (Resources/skins/<folder>). */
struct SkinAssets
{
    /** Returns the file data for `fileName` (e.g. "panel@2x.jpg"), or {nullptr,0}. */
    static std::pair<const void*, int> get (const juce::String& skinFolder, const juce::String& fileName);
    static bool exists (const juce::String& skinFolder);
};

/** Image-based LookAndFeel for the 5 photoreal aged-rack skins.

    Static art (panel with baked lighting + wear + silkscreen, knob filmstrips,
    button states, LED lenses, glass/dust overlay) is rendered offline by
    tools/panel_forge. Live parts (display contents, LED states, meter needles)
    are drawn in code on top. Only the active skin's images are decoded, and only
    at the resolution needed for the current display scale (1x / 2x / 3x). */
class PhotorealLookAndFeel : public FreddyLookAndFeel
{
public:
    explicit PhotorealLookAndFeel (const FreddyTheme&);

    const SkinLayout* skinLayout() const override { return hasLayout ? &layout : nullptr; }

    void drawChassis (juce::Graphics&, juce::Rectangle<float>) override;
    void drawOverlay (juce::Graphics&, juce::Rectangle<float>) override;
    void drawSection (juce::Graphics&, juce::Rectangle<float>, const SectionInfo&) override;
    void drawKnob (juce::Graphics&, juce::Rectangle<float>, const KnobState&) override;
    void drawPad (juce::Graphics&, juce::Rectangle<float>, const PadState&) override;
    void drawJogWheel (juce::Graphics&, juce::Rectangle<float>, float angle, bool active) override;
    void drawMeter (juce::Graphics&, juce::Rectangle<float>, float peakDb, float rmsDb, const juce::String&) override;
    void drawControlLabel (juce::Graphics&, juce::Rectangle<float>, const juce::String&, bool, juce::Justification) override;
    juce::Rectangle<float> drawDisplayFrame (juce::Graphics&, juce::Rectangle<float>) override;
    void drawDisplayOverlay (juce::Graphics&, juce::Rectangle<float>) override;
    void drawDisplayText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification, bool dim) override;
    void drawValueReadout (juce::Graphics&, const juce::String&, juce::Rectangle<float>) override;
    float traceGlow() const override;

private:
    /** Picks 1, 2 or 3 for the current physical scale. */
    static int pickScale (juce::Graphics&);
    const juce::Image& image (const juce::String& baseName, int scale);  // "panel", "knob_large", ...
    juce::String controlArt (const juce::String& id, const juce::String& fallback) const;

    juce::String folder;
    SkinLayout layout;
    bool hasLayout = false;
    int knobFrames = 128;
    float logicalW = 1600, logicalH = 442;
    std::map<juce::String, juce::String> art;            // control id -> art name
    std::map<juce::String, juce::Rectangle<float>> leds; // control id -> LED rect (logical)
    std::map<juce::String, juce::Image> cache;           // "name@2x" -> decoded image
    juce::Image empty;
};
} // namespace fv::ui
