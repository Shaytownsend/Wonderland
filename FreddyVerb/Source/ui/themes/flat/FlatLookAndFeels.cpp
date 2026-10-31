#include "FlatLookAndFeels.h"
#include <random>

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

// ---------------------------------------------------------------------------
// 04 Pad Station
// ---------------------------------------------------------------------------
void PadStationLookAndFeel::drawPad (juce::Graphics& g, juce::Rectangle<float> b, const PadState& p)
{
    auto body = b.reduced (2.0f);
    const auto accent = p.accent.isTransparent() ? t.c.accent : p.accent;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (body.translated (0, 2.0f), 5.0f);
    g.setColour (p.on ? accent : (p.down ? t.c.pad.darker (0.15f) : t.c.pad));
    g.fillRoundedRectangle (body, 5.0f);
    const auto ink = p.on ? juce::Colours::white : t.c.padText;
    auto content = body.reduced (5.0f, 3.0f);
    if (p.hasLed)
    {
        auto led = content.removeFromTop (std::min (8.0f, content.getHeight() * 0.22f)).removeFromRight (10.0f);
        g.setColour (p.on ? juce::Colours::white : t.c.ledOff);
        g.fillRect (led.withSizeKeepingCentre (8.0f, 4.0f));
    }
    if (p.glyph >= 0)
    {
        auto gl = content.reduced (2.0f);
        auto txt = gl.removeFromBottom (std::min (12.0f, gl.getHeight() * 0.3f));
        drawCurveGlyph (g, gl.reduced (2.0f, 1.0f), p.glyph, ink);
        g.setColour (ink);
        g.setFont (labelFont (txt.getHeight()));
        g.drawText (p.text, txt, juce::Justification::centred);
        return;
    }
    g.setColour (ink);
    g.setFont (labelFont (std::min (t.labelSize + 2.0f, content.getHeight() * 0.7f)).withExtraKerningFactor (0.08f));
    g.drawFittedText (p.text, content.toNearestInt(), juce::Justification::centred, 1, 0.7f);
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
    // flat, uneven sun-fading: large soft-edged blotches of slightly lighter/yellower tone (flat fills only)
    std::mt19937 rng (1989);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    for (int i = 0; i < 9; ++i)
    {
        auto r = juce::Rectangle<float> (b.getWidth() * (0.15f + 0.35f * u (rng)), b.getHeight() * (0.15f + 0.4f * u (rng)))
                    .withCentre ({ b.getX() + b.getWidth() * u (rng), b.getY() + b.getHeight() * u (rng) });
        g.setColour ((i % 2 ? t.c.chassis.brighter (0.05f) : juce::Colour (0xffe8d89f)).withAlpha (0.35f));
        g.fillEllipse (r);
    }
    g.setColour (t.c.chassisEdge);
    g.fillRect (b.removeFromTop (3.0f));
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
    juce::Path p;
    p.addArrow ({ a, b }, 1.4f, 8.0f, 9.0f);
    g.setColour (t.c.sectionEdge);
    g.fillPath (p);
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

void FloppyLookAndFeel::drawSection (juce::Graphics& g, juce::Rectangle<float> b, const SectionInfo& s)
{
    g.setColour (t.c.section);
    g.fillRoundedRectangle (b, t.cornerRadius);
    if (s.title.isEmpty()) return;
    auto band = b.removeFromTop (t.labelSize + 9.0f);
    g.setColour (stripeColour (s.index));
    g.fillRoundedRectangle (band, t.cornerRadius);
    g.fillRect (band.withTrimmedTop (band.getHeight() * 0.5f));
    g.setColour (t.c.sectionLabel);
    g.setFont (labelFont (t.labelSize).withExtraKerningFactor (0.18f));
    g.drawText (s.title, band.reduced (8.0f, 0), juce::Justification::centredLeft);
}
} // namespace fv::ui
