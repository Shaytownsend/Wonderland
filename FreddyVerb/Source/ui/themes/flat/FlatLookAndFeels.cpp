#include "FlatLookAndFeels.h"
#include "../../../state/Parameters.h"
#include <random>

namespace
{
    /** Printed text in a label box (outline or filled), used for model plates. */
    void labelBox (juce::Graphics& g, const fv::ui::FreddyLookAndFeel& lnf, juce::Rectangle<float> r, const juce::String& text,
                   juce::Colour ink, juce::Colour fill, float fontH, float radius = 2.0f)
    {
        if (! fill.isTransparent()) { g.setColour (fill); g.fillRoundedRectangle (r, radius); }
        else { g.setColour (ink); g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f); }
        g.setColour (ink);
        g.setFont (lnf.labelFont (fontH).withExtraKerningFactor (0.16f));
        g.drawFittedText (text, r.reduced (6.0f, 0).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }
}

namespace fv::ui
{
// ---------------------------------------------------------------------------
// 02 Studio Black Rack
// ---------------------------------------------------------------------------
void StudioRackLookAndFeel::drawChassis (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (t.c.chassis);
    g.fillRect (b);
    drawRackEars (g, b, 40.0f);
    // screw dots in the panel corners
    auto inner = b.reduced (40.0f, 0);
    g.setColour (t.c.chassisEdge.brighter (0.25f));
    for (auto p : { inner.getTopLeft() + juce::Point<float> (10, 10), inner.getTopRight() + juce::Point<float> (-10, 10),
                    inner.getBottomLeft() + juce::Point<float> (10, -10), inner.getBottomRight() + juce::Point<float> (-10, -10) })
    {
        g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre (p));
    }
    g.setColour (t.c.section);
    for (auto p : { inner.getTopLeft() + juce::Point<float> (10, 10), inner.getTopRight() + juce::Point<float> (-10, 10),
                    inner.getBottomLeft() + juce::Point<float> (10, -10), inner.getBottomRight() + juce::Point<float> (-10, -10) })
        g.fillRect (juce::Rectangle<float> (5, 1.2f).withCentre (p));
}

void StudioRackLookAndFeel::drawSection (juce::Graphics& g, juce::Rectangle<float> b, const SectionInfo& s)
{
    g.setColour (t.c.section);
    g.fillRect (b);
    g.setColour (t.c.sectionEdge.brighter (0.15f));
    g.drawRect (b, 1.0f);
    if (s.title.isEmpty()) return;
    auto strip = b.reduced (8.0f, 0).removeFromTop (t.labelSize + 9.0f);
    g.setColour (t.c.sectionLabel);
    g.setFont (labelFont (t.labelSize).withExtraKerningFactor (0.25f));
    g.drawText (s.title, strip, juce::Justification::centredLeft);
    g.setColour (s.accent);
    g.fillRect (juce::Rectangle<float> (b.getRight() - 14.0f, strip.getCentreY() - 2.5f, 5.0f, 5.0f));
}

void StudioRackLookAndFeel::drawRackEars (juce::Graphics& g, juce::Rectangle<float> full, float earW)
{
    FreddyLookAndFeel::drawRackEars (g, full, earW);
    // flat front handles on both ears
    for (int side = 0; side < 2; ++side)
    {
        auto ear = side == 0 ? full.withWidth (earW) : full.withLeft (full.getRight() - earW);
        auto h = ear.withSizeKeepingCentre (earW * 0.42f, full.getHeight() * 0.56f);
        g.setColour (t.c.knobBody.brighter (0.1f));
        g.fillRoundedRectangle (h, h.getWidth() * 0.5f);
        g.setColour (t.c.chassisEdge);
        g.fillRoundedRectangle (h.reduced (h.getWidth() * 0.3f, h.getWidth() * 0.6f), h.getWidth() * 0.2f);
        g.setColour (t.c.sectionEdge);
        g.fillRect (h.withHeight (3.0f).translated (0, -2.0f).expanded (2.0f, 0));
        g.fillRect (h.withTop (h.getBottom() - 1.0f).expanded (2.0f, 0).withHeight (3.0f));
    }
}

void StudioRackLookAndFeel::drawPanelDetail (juce::Graphics& g, juce::Rectangle<float> area)
{
    auto plate = area.removeFromRight (std::min (128.0f, area.getWidth() * 0.6f)).reduced (4.0f, 10.0f);
    labelBox (g, *this, plate, "FV-2400R  2U", t.c.chassisLabel.withAlpha (0.85f), juce::Colours::transparentBlack, 12.0f, 1.0f);
    // horizontal vent louvres
    auto v = area.reduced (8.0f, 9.0f);
    g.setColour (t.c.section);
    for (int i = 0; i < 4; ++i)
        g.fillRoundedRectangle (v.getX(), v.getY() + (float) i * v.getHeight() / 4.0f + 1.0f, v.getWidth(), v.getHeight() / 4.0f - 3.0f, 2.0f);
}

// ---------------------------------------------------------------------------
// 03 Cream Workstation
// ---------------------------------------------------------------------------
void CreamLookAndFeel::drawPad (juce::Graphics& g, juce::Rectangle<float> b, const PadState& p)
{
    auto body = b.reduced (1.5f);
    const float rad = body.getHeight() * 0.5f;
    g.setColour (p.on ? t.c.pad.brighter (0.15f) : t.c.pad);
    if (p.down) g.setColour (t.c.pad.darker (0.1f));
    g.fillRoundedRectangle (body, rad);
    g.setColour (t.c.sectionEdge);
    g.drawRoundedRectangle (body, rad, 1.0f);
    auto content = body.reduced (rad * 0.6f, 1.0f);
    const auto accent = p.accent.isTransparent() ? t.c.secondary : p.accent;
    if (p.hasLed)
    {
        g.setColour (p.on ? accent : t.c.ledOff.withAlpha (0.5f));
        g.fillEllipse (juce::Rectangle<float> (6, 6).withCentre ({ content.getX() + 4.0f, content.getCentreY() }));
        content.removeFromLeft (11.0f);
    }
    if (p.glyph >= 0) { drawCurveGlyph (g, content.reduced (1, 4), p.glyph, p.on ? accent : t.c.padText); return; }
    g.setColour (t.c.padText);
    g.setFont (labelFont (std::min (t.labelSize, content.getHeight() * 0.8f)));
    g.drawFittedText (p.text, content.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void CreamLookAndFeel::drawChassis (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (t.c.chassis);
    g.fillRect (b);
    // two printed pinstripes under the top moulding: teal + accent
    const float y = b.getY() + topBarHeight - 3.0f;
    g.setColour (t.c.secondary);
    g.fillRect (b.getX(), y, b.getWidth(), 2.0f);
    g.setColour (t.c.accent);
    g.fillRect (b.getX(), y + 3.0f, b.getWidth(), 1.0f);
    // soft rounded base foot line
    g.setColour (t.c.chassisEdge);
    g.fillRect (b.withTop (b.getBottom() - 3.0f));
}

void CreamLookAndFeel::drawSection (juce::Graphics& g, juce::Rectangle<float> b, const SectionInfo& s)
{
    g.setColour (t.c.section);
    g.fillRoundedRectangle (b, t.cornerRadius);
    if (s.title.isEmpty()) return;
    auto strip = b.reduced (12.0f, 0).removeFromTop (t.labelSize + 12.0f);
    g.setColour (s.accent.isTransparent() ? t.c.secondary : s.accent);
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ strip.getX() + 4.0f, strip.getCentreY() }));
    auto f = labelFont (t.labelSize + 1.0f).withExtraKerningFactor (0.14f);
    g.setColour (t.c.sectionLabel);
    g.setFont (f);
    g.drawText (s.title, strip.withTrimmedLeft (14.0f), juce::Justification::centredLeft);
    const float tw = juce::GlyphArrangement::getStringWidth (f, s.title);
    g.setColour (t.c.sectionEdge);
    g.fillRect (strip.getX() + tw + 24.0f, strip.getCentreY(), std::max (0.0f, strip.getWidth() - tw - 24.0f), 1.0f);
}

void CreamLookAndFeel::drawPanelDetail (juce::Graphics& g, juce::Rectangle<float> area)
{
    auto badge = area.withSizeKeepingCentre (std::min (area.getWidth(), 210.0f), 22.0f);
    labelBox (g, *this, badge, "WORKSTATION  -  16 VOICE DRUM REVERB", t.c.chassisLabel.withAlpha (0.85f), juce::Colours::transparentBlack, 11.5f, 11.0f);
}

// ---------------------------------------------------------------------------
// 04 Pad Station
// ---------------------------------------------------------------------------
void PadStationLookAndFeel::drawPad (juce::Graphics& g, juce::Rectangle<float> b, const PadState& p)
{
    const bool big = b.getHeight() >= 56.0f;
    const auto accent = p.accent.isTransparent() ? t.c.accent : p.accent;
    // colour-coded groups, like the coloured step keys of a classic drum machine
    juce::Colour group = accent;
    if (p.paramID == fv::pid::duckMode) group = juce::Colour (0xffffc23a);
    else if (p.paramID == fv::pid::curveShape) group = t.c.accent;
    else if (p.paramID == fv::pid::curveSync) group = juce::Colour (0xffb9b4ff);
    else if (p.paramID.isEmpty()) group = juce::Colour (0xffdcd7cb);

    auto body = b.reduced (big ? 3.0f : 2.0f);
    const float rad = big ? 9.0f : 5.0f;
    const float lip = big ? 5.0f : 2.5f;
    // flat drop "skirt" under the rubber pad (moves when pressed)
    g.setColour (juce::Colour (0xff141416));
    g.fillRoundedRectangle (body.withTrimmedTop (lip), rad);
    auto top = body.withTrimmedBottom (p.down ? 1.0f : lip).withTrimmedTop (p.down ? lip - 1.0f : 0.0f);
    const auto fill = p.on ? group : (p.hover ? t.c.pad.brighter (0.06f) : t.c.pad);
    g.setColour (fill);
    g.fillRoundedRectangle (top, rad);
    if (! p.on && big)
    {
        // colour-key strip printed along the bottom of unlit pads
        g.setColour (group);
        g.fillRoundedRectangle (top.removeFromBottom (6.0f).reduced (rad * 0.6f, 0), 2.0f);
    }
    const auto ink = juce::Colour (0xff17171a);
    auto content = top.reduced (big ? 9.0f : 5.0f, big ? 7.0f : 3.0f);

    if (p.hasLed)
    {
        const float d = big ? 9.0f : 6.0f;
        auto led = juce::Rectangle<float> (d, d).withPosition (content.getRight() - d, content.getY());
        g.setColour (p.on ? juce::Colour (0xffff2a10) : t.c.ledOff);
        g.fillEllipse (led);
        if (p.on) { g.setColour (juce::Colours::white.withAlpha (0.7f)); g.fillEllipse (led.reduced (d * 0.3f).translated (-d * 0.1f, -d * 0.1f)); }
        if (! big) content.removeFromRight (d + 3.0f);
    }
    if (big)
    {
        auto txt = content.removeFromBottom (std::max (12.0f, content.getHeight() * 0.22f));
        g.setColour (ink);
        g.setFont (labelFont (txt.getHeight() + 1.0f).withExtraKerningFactor (0.1f));
        g.drawFittedText (p.text, txt.toNearestInt(), juce::Justification::centred, 1, 0.8f);
        if (p.glyph >= 0)
            drawCurveGlyph (g, content.reduced (content.getWidth() * 0.1f, content.getHeight() * 0.14f).translated (0, 2.0f), p.glyph, ink);
        return;
    }
    if (p.glyph >= 0) { drawCurveGlyph (g, content.reduced (2.0f, 2.0f), p.glyph, ink); return; }
    g.setColour (ink);
    g.setFont (labelFont (std::min (t.labelSize + 1.0f, content.getHeight() * 0.8f)).withExtraKerningFactor (0.08f));
    g.drawFittedText (p.text, content.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void PadStationLookAndFeel::drawSection (juce::Graphics& g, juce::Rectangle<float> b, const SectionInfo& s)
{
    g.setColour (t.c.section);
    g.fillRoundedRectangle (b, t.cornerRadius);
    if (s.title.isEmpty()) return;
    auto strip = b.reduced (10.0f, 0).removeFromTop (t.labelSize + 10.0f);
    auto f = labelFont (t.labelSize + 3.0f).withExtraKerningFactor (0.1f);
    const float tw = juce::GlyphArrangement::getStringWidth (f, s.title);
    // chunky screen-printed title block: accent tab + white title
    auto tab = strip.withWidth (tw + 22.0f).withSizeKeepingCentre (tw + 22.0f, strip.getHeight() - 6.0f).translated (0, 2.0f);
    g.setColour (s.accent.isTransparent() ? t.c.accent : s.accent);
    g.fillRect (tab.removeFromLeft (6.0f));
    g.setColour (t.c.sectionLabel);
    g.setFont (f);
    g.drawText (s.title, tab.withTrimmedLeft (8.0f), juce::Justification::centredLeft);
    g.setColour (t.c.sectionEdge.brighter (0.25f));
    g.fillRect (strip.getX() + tw + 30.0f, strip.getCentreY() + 1.0f, std::max (0.0f, strip.getWidth() - tw - 30.0f), 3.0f);
}

void PadStationLookAndFeel::drawPanelDetail (juce::Graphics& g, juce::Rectangle<float> area)
{
    // printed colour key for the pad bank
    const std::pair<juce::Colour, const char*> keys[] = { { t.c.vizDuck, "DUCK" }, { t.c.vizGate, "GATE" },
                                                          { juce::Colour (0xffffc23a), "MODE" }, { t.c.accent, "SHAPE" } };
    const float w = std::min (66.0f, area.getWidth() / 4.0f);
    auto r = area.withSizeKeepingCentre (w * 4.0f, 18.0f);
    for (auto& [c, name] : keys)
    {
        auto cell = r.removeFromLeft (w);
        g.setColour (c);
        g.fillRoundedRectangle (cell.removeFromLeft (12.0f).withSizeKeepingCentre (12.0f, 12.0f), 3.0f);
        g.setColour (t.c.chassisLabel);
        g.setFont (labelFont (12.0f).withExtraKerningFactor (0.06f));
        g.drawFittedText (name, cell.withTrimmedLeft (4.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    }
}

void PadStationLookAndFeel::drawKnob (juce::Graphics& g, juce::Rectangle<float> b, const KnobState& k)
{
    const float size = std::min (b.getWidth(), b.getHeight());
    auto r = juce::Rectangle<float> (size, size).withCentre (b.getCentre());
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float ang = a0 + (a1 - a0) * k.value;
    const auto accent = k.accent.isTransparent() ? t.c.knobArc : k.accent;
    // segmented arc of LEDs
    const int segs = 11;
    for (int i = 0; i < segs; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / (float) (segs - 1);
        const bool lit = (float) i / (float) (segs - 1) <= k.value + 0.001f;
        auto c = r.getCentre().getPointOnCircumference (size * 0.46f, a);
        g.setColour (lit ? accent : t.c.knobTrack);
        g.fillRect (juce::Rectangle<float> (size * 0.07f, size * 0.07f).withCentre (c));
    }
    auto body = r.reduced (size * 0.17f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (body.translated (0, 2));
    g.setColour (t.c.knobBody);
    g.fillEllipse (body);
    g.setColour (t.c.knobLine);
    const auto c = body.getCentre();
    g.drawLine ({ c.getPointOnCircumference (body.getWidth() * 0.12f, ang), c.getPointOnCircumference (body.getWidth() * 0.46f, ang) }, size * 0.09f);
}

// ---------------------------------------------------------------------------
// 05 Sunbleached '89
// ---------------------------------------------------------------------------
void SunbleachedLookAndFeel::drawChassis (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (t.c.chassis);
    g.fillRect (b);
    // flat, uneven sun-fading: overlapping blotches of yellower / paler plastic (flat fills only)
    std::mt19937 rng (1989);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    for (int i = 0; i < 14; ++i)
    {
        auto r = juce::Rectangle<float> (b.getWidth() * (0.12f + 0.3f * u (rng)), b.getHeight() * (0.1f + 0.3f * u (rng)))
                    .withCentre ({ b.getX() + b.getWidth() * u (rng), b.getY() + b.getHeight() * u (rng) });
        g.setColour ((i % 3 == 0 ? juce::Colour (0xffeee4c4) : juce::Colour (0xffe6d293)).withAlpha (0.5f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.45f);
    }
    // the top edge yellowed most (faced the window)
    g.setColour (juce::Colour (0x30c8a040));
    g.fillRect (b.withHeight (topBarHeight));
    g.setColour (t.c.chassisEdge);
    g.fillRect (b.withHeight (3.0f));
    g.fillRect (b.getX(), b.getY() + topBarHeight - 1.0f, b.getWidth(), 1.5f);
    for (auto p : { juce::Point<float> (b.getX() + 7.0f, b.getY() + 9.0f), juce::Point<float> (b.getRight() - 7.0f, b.getY() + 9.0f),
                    juce::Point<float> (b.getX() + 7.0f, b.getBottom() - 8.0f), juce::Point<float> (b.getRight() - 7.0f, b.getBottom() - 8.0f) })
        drawScrew (g, p, 7.0f, 1.3f);
}

void SunbleachedLookAndFeel::drawPad (juce::Graphics& g, juce::Rectangle<float> b, const PadState& p)
{
    // muted pastel buttons, colour by function
    juce::Colour fill = juce::Colour (0xffb9cfdc);                       // powder blue
    if (p.paramID.isEmpty()) fill = juce::Colour (0xffe9d9a6);           // butter (top bar)
    else if (p.accent == t.c.vizDuck) fill = juce::Colour (0xfff0bfa8);  // faded salmon
    else if (p.accent == t.c.vizGate) fill = juce::Colour (0xffb3d6c8);  // mint
    if (p.down) fill = fill.darker (0.12f);
    else if (p.hover) fill = fill.brighter (0.06f);
    auto body = b.reduced (1.0f);
    const float rad = std::min (5.0f, body.getHeight() * 0.25f);
    g.setColour (fill.darker (0.3f));
    g.fillRoundedRectangle (body, rad);
    auto top = body.withTrimmedBottom (2.0f);
    g.setColour (fill);
    g.fillRoundedRectangle (top, rad);
    // rubbed-shiny thumb spot (flat, lighter)
    g.setColour (fill.brighter (0.25f).withAlpha (0.7f));
    g.fillEllipse (top.withSizeKeepingCentre (top.getWidth() * 0.4f, top.getHeight() * 0.3f).translated (top.getWidth() * 0.12f, -top.getHeight() * 0.18f));

    auto content = b.reduced (5.0f, 2.0f);
    const auto accent = p.accent.isTransparent() ? t.c.accent : p.accent;
    if (p.hasLed)
    {
        auto led = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ content.getX() + 4.0f, content.getCentreY() });
        g.setColour (p.on ? t.c.ledOn : fill.darker (0.35f));
        g.fillEllipse (led);
        content.removeFromLeft (11.0f);
    }
    if (p.glyph >= 0) { drawCurveGlyph (g, content.reduced (2.0f, 3.0f), p.glyph, p.on ? accent.darker (0.3f) : t.c.padText); return; }
    g.setColour (t.c.padText);
    g.setFont (labelFont (std::min (t.labelSize, content.getHeight() * 0.8f)).withExtraKerningFactor (0.06f));
    g.drawFittedText (p.text, content.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void SunbleachedLookAndFeel::drawPanelDetail (juce::Graphics& g, juce::Rectangle<float> area)
{
    // a strip of old masking tape someone wrote the studio name on
    auto tape = area.withSizeKeepingCentre (std::min (area.getWidth() - 10.0f, 170.0f), 24.0f);
    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (juce::AffineTransform::rotation (-0.035f, tape.getCentreX(), tape.getCentreY()));
    g.setColour (juce::Colour (0xffe8dcae));
    g.fillRect (tape);
    g.setColour (juce::Colour (0xffd4c58f));
    g.fillRect (tape.removeFromLeft (3.0f));
    g.fillRect (tape.removeFromRight (3.0f));
    g.setColour (juce::Colour (0xff3b3426));
    g.setFont (labelFont (15.0f).withExtraKerningFactor (0.12f));
    g.drawText ("ROOM B  -  SNARE VERB", tape, juce::Justification::centred);
}

void SunbleachedLookAndFeel::drawSection (juce::Graphics& g, juce::Rectangle<float> b, const SectionInfo& s)
{
    FreddyLookAndFeel::drawSection (g, b, s);
    // worn corner: paint rubbed through to chassis colour (flat shapes)
    juce::Path wear;
    wear.addTriangle (b.getRight() - 16.0f, b.getBottom(), b.getRight(), b.getBottom() - 12.0f, b.getRight(), b.getBottom());
    g.setColour (t.c.chassis.withAlpha (0.55f));
    g.fillPath (wear);
}

// ---------------------------------------------------------------------------
// 06 Night Session
// ---------------------------------------------------------------------------
void NightSessionLookAndFeel::drawSection (juce::Graphics& g, juce::Rectangle<float> b, const SectionInfo& s)
{
    g.setColour (t.c.section);
    g.fillRoundedRectangle (b, t.cornerRadius);
    if (s.title.isEmpty()) return;
    auto strip = b.reduced (10.0f, 0).removeFromTop (t.labelSize + 9.0f);
    g.setColour (t.c.sectionLabel);
    g.setFont (labelFont (t.labelSize).withExtraKerningFactor (0.3f));
    g.drawText (s.title, strip, juce::Justification::centredLeft);
    g.setColour (s.accent.withAlpha (0.8f));
    g.fillEllipse (juce::Rectangle<float> (4, 4).withCentre ({ b.getRight() - 12.0f, strip.getCentreY() }));
}

void NightSessionLookAndFeel::drawPad (juce::Graphics& g, juce::Rectangle<float> b, const PadState& p)
{
    auto body = b.reduced (1.0f);
    g.setColour (p.down ? t.c.pad.brighter (0.1f) : t.c.pad);
    g.fillRoundedRectangle (body, 2.0f);
    const auto accent = p.accent.isTransparent() ? t.c.accent : p.accent;
    if (p.on)
    {
        g.setColour (accent.withAlpha (0.18f));
        g.fillRoundedRectangle (body, 2.0f);
        g.setColour (accent);
        g.fillRect (body.removeFromBottom (2.0f));
    }
    auto content = b.reduced (4.0f, 2.0f);
    if (p.glyph >= 0) { drawCurveGlyph (g, content.reduced (2, 3), p.glyph, p.on ? accent : t.c.padText); return; }
    g.setColour (p.on ? accent.brighter (0.4f) : t.c.padText);
    g.setFont (labelFont (std::min (t.labelSize, content.getHeight() * 0.8f)).withExtraKerningFactor (0.1f));
    g.drawFittedText (p.text, content.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void NightSessionLookAndFeel::drawChassis (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (t.c.chassis);
    g.fillRect (b);
    g.setColour (t.c.chassisEdge);
    g.fillRect (b.getX(), b.getY() + topBarHeight - 1.0f, b.getWidth(), 1.0f);
    // recessed black screws, barely visible
    for (auto p : { juce::Point<float> (b.getX() + 7.0f, b.getY() + 8.0f), juce::Point<float> (b.getRight() - 7.0f, b.getY() + 8.0f),
                    juce::Point<float> (b.getX() + 7.0f, b.getBottom() - 8.0f), juce::Point<float> (b.getRight() - 7.0f, b.getBottom() - 8.0f) })
        drawScrew (g, p, 6.0f);
}

void NightSessionLookAndFeel::drawPanelDetail (juce::Graphics& g, juce::Rectangle<float> area)
{
    // status LEDs: PWR lit, MIDI / CLIP off
    const char* names[] = { "PWR", "SYNC", "CLIP" };
    auto r = area.withSizeKeepingCentre (std::min (area.getWidth(), 170.0f), 30.0f);
    const float w = r.getWidth() / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        auto cell = r.removeFromLeft (w);
        auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ cell.getCentreX(), cell.getY() + 6.0f });
        if (i == 0) { g.setColour (t.c.ledOn.withAlpha (0.25f)); g.fillEllipse (led.expanded (3.0f)); }
        g.setColour (i == 0 ? t.c.ledOn : t.c.ledOff);
        g.fillEllipse (led);
        g.setColour (t.c.chassisLabel);
        g.setFont (labelFont (11.0f).withExtraKerningFactor (0.3f));
        g.drawText (names[i], cell.withTrimmedTop (14.0f), juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------
// 07 Service Manual
// ---------------------------------------------------------------------------
void ServiceManualLookAndFeel::drawChassis (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (t.c.chassis);
    g.fillRect (b);
    g.setColour (t.c.chassisEdge);
    g.drawRect (b.reduced (4.0f), 1.5f);
    g.drawRect (b.reduced (7.0f), 0.6f);
    // figure caption like a manual plate
    g.setFont (labelFont (11.0f).withExtraKerningFactor (0.2f));
    g.drawText ("FIG. 1  FRONT PANEL - FREDDY VERB FV-2400", b.reduced (14.0f, 9.0f).removeFromBottom (12.0f), juce::Justification::centredRight);
}

void ServiceManualLookAndFeel::drawSection (juce::Graphics& g, juce::Rectangle<float> b, const SectionInfo& s)
{
    g.setColour (t.c.sectionEdge);
    g.drawRect (b, 1.2f);
    if (s.title.isEmpty()) return;
    auto f = labelFont (t.labelSize).withExtraKerningFactor (0.2f);
    const float tw = juce::GlyphArrangement::getStringWidth (f, s.title) + 14.0f;
    auto box = juce::Rectangle<float> (b.getX() + 10.0f, b.getY() - 1.0f, tw, t.labelSize + 6.0f);
    g.setColour (t.c.sectionEdge);
    g.fillRect (box);
    g.setColour (t.c.chassis);
    g.setFont (f);
    g.drawText (s.title, box, juce::Justification::centred);
    // section number like a schematic block
    g.setColour (t.c.sectionEdge);
    g.setFont (labelFont (10.0f));
    g.drawText (juce::String (s.index + 1) + "." + juce::String (s.index == 0 ? "0" : "1"), b.reduced (6.0f, 3.0f).removeFromTop (12.0f), juce::Justification::topRight);
}

void ServiceManualLookAndFeel::drawKnob (juce::Graphics& g, juce::Rectangle<float> b, const KnobState& k)
{
    const float size = std::min (b.getWidth(), b.getHeight());
    auto r = juce::Rectangle<float> (size, size).withCentre (b.getCentre()).reduced (size * 0.12f);
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float ang = a0 + (a1 - a0) * k.value;
    const auto c = r.getCentre();
    g.setColour (t.c.knobLine);
    // tick marks like a printed dial scale
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / 10.0f;
        const float len = (i % 5 == 0) ? 0.18f : 0.1f;
        g.drawLine ({ c.getPointOnCircumference (size * 0.42f, a), c.getPointOnCircumference (size * (0.42f + len * 0.4f), a) }, 0.9f);
    }
    g.drawEllipse (r, 1.4f);
    g.drawEllipse (r.reduced (size * 0.08f), 0.6f);
    const auto accent = k.accent.isTransparent() ? t.c.knobArc : k.accent;
    g.setColour (accent);
    g.drawLine ({ c, c.getPointOnCircumference (r.getWidth() * 0.5f, ang) }, 2.0f);
    g.fillEllipse (juce::Rectangle<float> (4, 4).withCentre (c));
}

void ServiceManualLookAndFeel::drawPad (juce::Graphics& g, juce::Rectangle<float> b, const PadState& p)
{
    auto body = b.reduced (1.5f);
    const auto accent = p.accent.isTransparent() ? t.c.accent : p.accent;
    g.setColour (p.on ? accent.withAlpha (0.12f) : t.c.chassis);
    g.fillRect (body);
    g.setColour (t.c.sectionEdge);
    g.drawRect (body, p.on ? 1.8f : 1.0f);
    auto content = body.reduced (4.0f, 2.0f);
    if (p.hasLed)
    {
        auto led = juce::Rectangle<float> (6, 6).withCentre ({ content.getX() + 4.0f, content.getCentreY() });
        g.setColour (accent);
        if (p.on) g.fillEllipse (led); else g.drawEllipse (led, 1.0f);
        content.removeFromLeft (11.0f);
    }
    if (p.glyph >= 0) { drawCurveGlyph (g, content.reduced (1, 3), p.glyph, p.on ? accent : t.c.padText); return; }
    g.setColour (t.c.padText);
    g.setFont (labelFont (std::min (t.labelSize, content.getHeight() * 0.8f)).withExtraKerningFactor (0.1f));
    g.drawFittedText (p.text, content.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

juce::Rectangle<float> ServiceManualLookAndFeel::drawDisplayFrame (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (t.c.lcdBg);
    g.fillRect (b);
    g.setColour (t.c.lcdBezel);
    g.drawRect (b, 1.5f);
    g.drawRect (b.reduced (3.0f), 0.6f);
    return b.reduced (4.0f);
}

void ServiceManualLookAndFeel::drawSignalArrow (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b)
{
    // printed signal-flow arrow: tail dot, line, solid head
    juce::Path p;
    p.addArrow ({ a, b }, 2.0f, 11.0f, 10.0f);
    g.setColour (t.c.accent);
    g.fillPath (p);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (a));
}

void ServiceManualLookAndFeel::drawPanelDetail (juce::Graphics& g, juce::Rectangle<float> area)
{
    // engineering-drawing title block
    auto r = area.withSizeKeepingCentre (std::min (area.getWidth() - 8.0f, 220.0f), 34.0f);
    g.setColour (t.c.sectionEdge);
    g.drawRect (r, 1.2f);
    auto left = r.removeFromLeft (r.getWidth() * 0.62f);
    g.fillRect (left.getRight(), r.getY(), 1.0f, r.getHeight());
    g.fillRect (left.getX(), left.getCentreY(), left.getWidth(), 1.0f);
    g.fillRect (r.getX(), r.getCentreY(), r.getWidth(), 1.0f);
    g.setFont (labelFont (10.5f).withExtraKerningFactor (0.18f));
    g.drawText ("DWG  FV-2400-01", left.withHeight (left.getHeight() * 0.5f).reduced (5, 0), juce::Justification::centredLeft);
    g.drawText ("FRONT PANEL ASSY", left.withTrimmedTop (left.getHeight() * 0.5f).reduced (5, 0), juce::Justification::centredLeft);
    g.drawText ("REV  C", r.withHeight (r.getHeight() * 0.5f).reduced (5, 0), juce::Justification::centredLeft);
    g.drawText ("SCALE 1:1", r.withTrimmedTop (r.getHeight() * 0.5f).reduced (5, 0), juce::Justification::centredLeft);
}

void ServiceManualLookAndFeel::drawJogWheel (juce::Graphics& g, juce::Rectangle<float> b, float angle, bool active)
{
    const float size = std::min (b.getWidth(), b.getHeight());
    auto r = juce::Rectangle<float> (size, size).withCentre (b.getCentre()).reduced (1.0f);
    const auto c = r.getCentre();
    g.setColour (t.c.knobLine);
    g.drawEllipse (r, 1.4f);
    g.drawEllipse (r.reduced (size * 0.1f), 0.6f);
    g.setColour (active ? t.c.accent : t.c.knobLine);
    g.drawEllipse (juce::Rectangle<float> (size * 0.18f, size * 0.18f).withCentre (c.getPointOnCircumference (size * 0.28f, angle)), 1.2f);
    g.setColour (t.c.knobLine);
    g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (c));
}

// ---------------------------------------------------------------------------
// 08 Floppy Era Pro
// ---------------------------------------------------------------------------
juce::Colour FloppyLookAndFeel::stripeColour (int s)
{
    switch (s)
    {
        case 0: return juce::Colour (0xff9b7fd1); // REVERB violet
        case 1: return juce::Colour (0xffff8a4c); // DUCK orange
        case 2: return juce::Colour (0xff3fc1b5); // GATE teal
        case 3: return juce::Colour (0xffe9c44a); // TONE yellow
        case 4: return juce::Colour (0xff6aa7e8); // SPACE blue
        case 5: return juce::Colour (0xffe8594a); // MIX red
        case 6: return juce::Colour (0xffb8c26a); // DETECT olive
        default: return juce::Colour (0xffcccccc);
    }
}

void FloppyLookAndFeel::drawChassis (juce::Graphics& g, juce::Rectangle<float> b)
{
    FreddyLookAndFeel::drawChassis (g, b);
    // darker front lip along the bottom edge of the wide case
    g.setColour (t.c.chassisEdge);
    g.fillRect (b.withTop (b.getBottom() - 6.0f));
    g.setColour (t.c.chassisEdge.darker (0.2f));
    g.fillRect (b.withTop (b.getBottom() - 7.0f).withHeight (1.0f));
}

void FloppyLookAndFeel::drawPanelDetail (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float bw = std::min (310.0f, area.getWidth() * 0.62f);
    auto badge = area.removeFromLeft (bw).withSizeKeepingCentre (bw, 24.0f);
    g.setColour (t.c.section);
    g.fillRoundedRectangle (badge, 3.0f);
    auto pro = badge.removeFromLeft (52.0f);
    g.setColour (t.c.accent);
    g.fillRoundedRectangle (pro.reduced (3.0f), 2.0f);
    g.setColour (juce::Colour (0xff141414));
    g.setFont (labelFont (15.0f).withExtraKerningFactor (0.2f));
    g.drawText ("PRO", pro, juce::Justification::centred);
    // stage colour key
    auto key = badge.removeFromRight (84.0f).reduced (6.0f, 8.0f);
    const int order[] = { 0, 6, 1, 2, 3, 4, 5 };
    const float w = key.getWidth() / 7.0f;
    for (int i = 0; i < 7; ++i) { g.setColour (stripeColour (order[i])); g.fillRect (key.removeFromLeft (w)); }
    g.setColour (t.c.text);
    g.setFont (labelFont (12.0f).withExtraKerningFactor (0.2f));
    g.drawText ("7-STAGE DRUM REVERB", badge.reduced (8.0f, 0), juce::Justification::centredLeft);
    FreddyLookAndFeel::drawPanelDetail (g, area.reduced (12.0f, 0));
}

void FloppyLookAndFeel::drawSection (juce::Graphics& g, juce::Rectangle<float> b, const SectionInfo& s)
{
    g.setColour (t.c.section);
    g.fillRoundedRectangle (b, t.cornerRadius);
    if (s.title.isEmpty()) return;
    auto band = b.removeFromTop (t.labelSize + 12.0f);
    const auto stripe = stripeColour (s.index);
    g.setColour (stripe);
    g.fillRoundedRectangle (band, t.cornerRadius);
    g.fillRect (band.withTrimmedTop (band.getHeight() * 0.5f));
    // thin darker rule under the stripe, like a printed colour band on a bezel
    g.setColour (stripe.darker (0.45f));
    g.fillRect (band.withTop (band.getBottom() - 2.0f));
    // stage number box + title printed in black on the stripe
    const juce::String num = juce::String (s.index == 6 ? 2 : (s.index == 0 ? 1 : (s.index < 6 ? s.index + 2 : 0)));
    auto box = band.reduced (6.0f, 4.0f).removeFromLeft (band.getHeight() - 8.0f);
    g.setColour (juce::Colour (0xff141414));
    g.fillRect (box.withHeight (band.getHeight() - 10.0f));
    g.setColour (stripe);
    g.setFont (labelFont (t.labelSize).withExtraKerningFactor (0.05f));
    g.drawText (s.index <= 6 ? num : juce::String(), box.withHeight (band.getHeight() - 10.0f), juce::Justification::centred);
    g.setColour (juce::Colour (0xff141414));
    g.setFont (labelFont (t.labelSize + 2.0f).withExtraKerningFactor (0.2f));
    g.drawText (s.title, band.withTrimmedLeft (box.getRight() - band.getX() + 6.0f).withTrimmedRight (6.0f), juce::Justification::centredLeft);
}
} // namespace fv::ui
