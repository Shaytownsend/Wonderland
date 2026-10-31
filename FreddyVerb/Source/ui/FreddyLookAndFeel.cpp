#include "FreddyLookAndFeel.h"
#include "FreddyBinaryData.h"

namespace fv::ui
{
FreddyLookAndFeel::FreddyLookAndFeel (const FreddyTheme& theme) : t (theme)
{
    condensed       = juce::Typeface::createSystemTypefaceFor (FreddyBinaryData::BarlowCondensedSemiBold_ttf, FreddyBinaryData::BarlowCondensedSemiBold_ttfSize);
    condensedMedium = juce::Typeface::createSystemTypefaceFor (FreddyBinaryData::BarlowCondensedMedium_ttf, FreddyBinaryData::BarlowCondensedMedium_ttfSize);
    mono            = juce::Typeface::createSystemTypefaceFor (FreddyBinaryData::ShareTechMonoRegular_ttf, FreddyBinaryData::ShareTechMonoRegular_ttfSize);

    setColour (juce::PopupMenu::backgroundColourId, t.c.section);
    setColour (juce::PopupMenu::textColourId, t.c.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.c.accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ComboBox::backgroundColourId, t.c.lcdBg);
    setColour (juce::ComboBox::textColourId, t.c.lcdFg);
    setColour (juce::ComboBox::outlineColourId, t.c.lcdBezel);
    setColour (juce::ComboBox::arrowColourId, t.c.lcdFg);
    setColour (juce::TextEditor::backgroundColourId, t.c.lcdBg);
    setColour (juce::TextEditor::textColourId, t.c.lcdFg);
    setColour (juce::TextEditor::highlightColourId, t.c.lcdDim);
    setColour (juce::CaretComponent::caretColourId, t.c.lcdFg);
    setColour (juce::TooltipWindow::backgroundColourId, t.c.lcdBg);
    setColour (juce::TooltipWindow::textColourId, t.c.lcdFg);
    setColour (juce::ListBox::backgroundColourId, t.c.lcdBg);
    setColour (juce::ListBox::textColourId, t.c.lcdFg);
    setColour (juce::ScrollBar::thumbColourId, t.c.lcdDim);
    setColour (juce::Label::textColourId, t.c.text);
    setColour (juce::TextButton::buttonColourId, t.c.pad);
    setColour (juce::TextButton::textColourOffId, t.c.padText);
    setColour (juce::AlertWindow::backgroundColourId, t.c.section);
    setColour (juce::AlertWindow::textColourId, t.c.text);
    setColour (juce::ResizableWindow::backgroundColourId, t.c.chassis);
    setDefaultSansSerifTypeface (condensedMedium);
}

juce::Font FreddyLookAndFeel::labelFont (float h) const
{
    return juce::Font (juce::FontOptions (condensed).withHeight (h));
}

juce::Font FreddyLookAndFeel::monoFont (float h) const
{
    return juce::Font (juce::FontOptions (mono).withHeight (h));
}

juce::Colour FreddyLookAndFeel::sectionAccent (int idx) const
{
    switch (idx)
    {
        case 1: return t.c.vizDuck;
        case 2: return t.c.vizGate;
        default: return t.c.accent;
    }
}

float FreddyLookAndFeel::contrastRatio (juce::Colour a, juce::Colour b)
{
    auto lum = [] (juce::Colour c)
    {
        auto ch = [] (float v) { return v <= 0.03928f ? v / 12.92f : std::pow ((v + 0.055f) / 1.055f, 2.4f); };
        return 0.2126f * ch (c.getFloatRed()) + 0.7152f * ch (c.getFloatGreen()) + 0.0722f * ch (c.getFloatBlue());
    };
    const float la = lum (a), lb = lum (b);
    return (std::max (la, lb) + 0.05f) / (std::min (la, lb) + 0.05f);
}

// ---------------------------------------------------------------------------
// Panels
// ---------------------------------------------------------------------------
void FreddyLookAndFeel::drawChassis (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (t.c.chassis);
    g.fillRect (b);
    // flat seam lines like moulded plastic panel joins
    g.setColour (t.c.chassisEdge);
    g.fillRect (b.removeFromTop (2.0f));
    g.fillRect (b.removeFromBottom (2.0f));
    if (t.rackEars) drawRackEars (g, b.expanded (0, 2), 34.0f);
}

void FreddyLookAndFeel::drawRackEars (juce::Graphics& g, juce::Rectangle<float> full, float earW)
{
    for (int side = 0; side < 2; ++side)
    {
        auto ear = side == 0 ? full.withWidth (earW) : full.withLeft (full.getRight() - earW);
        g.setColour (t.c.chassisEdge);
        g.fillRect (ear);
        g.setColour (t.c.sectionEdge);
        g.fillRect (side == 0 ? ear.removeFromRight (2.0f) : ear.removeFromLeft (2.0f));
        // EIA slotted holes
        for (int i = 0; i < 2; ++i)
        {
            const float cy = i == 0 ? full.getY() + 34.0f : full.getBottom() - 34.0f;
            auto slot = juce::Rectangle<float> (16.0f, 9.0f).withCentre ({ ear.getCentreX(), cy });
            g.setColour (t.c.section.darker (0.6f));
            g.fillRoundedRectangle (slot, 4.5f);
            g.setColour (t.c.knobLine.withAlpha (0.55f));
            g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre (slot.getCentre()));
        }
    }
}

void FreddyLookAndFeel::drawSection (juce::Graphics& g, juce::Rectangle<float> b, const SectionInfo& s)
{
    g.setColour (t.c.section);
    g.fillRoundedRectangle (b, t.cornerRadius);
    g.setColour (t.c.sectionEdge);
    g.drawRoundedRectangle (b.reduced (0.5f), t.cornerRadius, 1.0f);

    if (s.title.isNotEmpty())
    {
        auto strip = b.reduced (10.0f, 0).removeFromTop (t.labelSize + 10.0f);
        g.setColour (t.c.sectionLabel);
        g.setFont (labelFont (t.labelSize + 1.0f).withExtraKerningFactor (0.12f));
        g.drawText (s.title, strip, juce::Justification::centredLeft);
        // printed rule after the title
        const float tw = juce::GlyphArrangement::getStringWidth (labelFont (t.labelSize + 1.0f).withExtraKerningFactor (0.12f), s.title);
        g.setColour (s.accent.isTransparent() ? t.c.accent : s.accent);
        g.fillRect (juce::Rectangle<float> (strip.getX() + tw + 8.0f, strip.getCentreY() - 1.0f,
                                            std::max (0.0f, strip.getWidth() - tw - 8.0f), 2.0f));
    }
}

void FreddyLookAndFeel::drawSignalArrow (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b)
{
    juce::Path p;
    p.addArrow ({ a, b }, 2.0f, 9.0f, 8.0f);
    g.setColour (t.c.chassisLabel);
    g.fillPath (p);
}

void FreddyLookAndFeel::drawLogo (juce::Graphics& g, juce::Rectangle<float> b)
{
    const float h = b.getHeight();
    auto f = labelFont (h * 0.9f).withExtraKerningFactor (0.06f);
    g.setFont (f);
    g.setColour (t.c.accent);
    const juce::String word1 ("FREDDY");
    const float w1 = juce::GlyphArrangement::getStringWidth (f, word1);
    g.drawText (word1, b.withWidth (w1 + 2), juce::Justification::centredLeft);
    g.setColour (t.c.chassisLabel);
    g.drawText ("VERB", b.withTrimmedLeft (w1 + h * 0.25f), juce::Justification::centredLeft);
    // small model plate
    g.setFont (labelFont (h * 0.32f).withExtraKerningFactor (0.2f));
    g.setColour (t.c.chassisLabel.withAlpha (0.75f));
    const float w2 = juce::GlyphArrangement::getStringWidth (f, "VERB");
    g.drawText ("FV-2400  DRUM REVERB", b.withTrimmedLeft (w1 + w2 + h * 0.6f), juce::Justification::centredLeft);
}

// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------
void FreddyLookAndFeel::drawKnob (juce::Graphics& g, juce::Rectangle<float> b, const KnobState& k)
{
    const float size = std::min (b.getWidth(), b.getHeight());
    auto r = juce::Rectangle<float> (size, size).withCentre (b.getCentre());
    const float a0 = juce::MathConstants<float>::pi * 1.25f;
    const float a1 = juce::MathConstants<float>::pi * 2.75f;
    const float ang = a0 + (a1 - a0) * k.value;
    const auto accent = k.accent.isTransparent() ? t.c.knobArc : k.accent;
    const float arcW = std::max (2.0f, size * 0.08f);

    // value arc track
    auto arcR = r.reduced (arcW * 0.5f);
    juce::Path track;
    track.addCentredArc (arcR.getCentreX(), arcR.getCentreY(), arcR.getWidth() * 0.5f, arcR.getHeight() * 0.5f, 0, a0, a1, true);
    g.setColour (t.c.knobTrack);
    g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    const float from = k.bipolar ? a0 + (a1 - a0) * k.defaultValue : a0;
    if (std::abs (ang - from) > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (arcR.getCentreX(), arcR.getCentreY(), arcR.getWidth() * 0.5f, arcR.getHeight() * 0.5f, 0,
                           std::min (from, ang), std::max (from, ang), true);
        g.setColour (k.enabled ? accent : accent.withSaturation (0.1f));
        g.strokePath (val, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    // body
    auto body = r.reduced (arcW * 2.2f);
    if (t.knobStyle == KnobStyle::flatSkirt)
    {
        g.setColour (t.c.knobBody.darker (0.35f));
        g.fillEllipse (body);
        body = body.reduced (body.getWidth() * 0.14f);
    }
    g.setColour (k.hover || k.dragging ? t.c.knobBody.brighter (0.12f) : t.c.knobBody);
    g.fillEllipse (body);

    // indicator line
    const float rad = body.getWidth() * 0.5f;
    const auto c = body.getCentre();
    juce::Line<float> line (c.getPointOnCircumference (rad * 0.25f, ang), c.getPointOnCircumference (rad * 0.92f, ang));
    g.setColour (t.c.knobLine);
    g.drawLine (line, std::max (1.5f, size * 0.06f));
}

void FreddyLookAndFeel::drawPad (juce::Graphics& g, juce::Rectangle<float> b, const PadState& p)
{
    const auto accent = p.accent.isTransparent() ? t.c.accent : p.accent;
    auto body = b.reduced (1.0f);
    const float rad = std::min (4.0f, body.getHeight() * 0.18f);
    juce::Colour fill = t.c.pad;
    if (p.down) fill = fill.darker (0.2f);
    else if (p.hover) fill = fill.brighter (0.1f);
    g.setColour (fill);
    g.fillRoundedRectangle (body, rad);
    // flat "rubber" lip at the bottom
    g.setColour (fill.darker (0.25f));
    g.fillRoundedRectangle (body.removeFromBottom (std::max (2.0f, body.getHeight() * 0.1f)), rad);

    auto content = b.reduced (4.0f, 2.0f);
    if (p.hasLed)
    {
        auto led = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ content.getX() + 5.0f, content.getCentreY() });
        g.setColour (p.on ? accent : t.c.ledOff);
        g.fillEllipse (led);
        content.removeFromLeft (12.0f);
    }
    if (p.glyph >= 0)
        drawCurveGlyph (g, content.reduced (2.0f, 3.0f), p.glyph, p.on ? accent : t.c.padText);
    else
    {
        g.setColour (p.enabled ? t.c.padText : t.c.padText.withAlpha (0.4f));
        g.setFont (labelFont (std::min (t.labelSize, content.getHeight() * 0.8f)).withExtraKerningFactor (0.06f));
        g.drawFittedText (p.text, content.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

void FreddyLookAndFeel::drawControlLabel (juce::Graphics& g, juce::Rectangle<float> b, const juce::String& text,
                                          bool onSection, juce::Justification j)
{
    g.setColour (onSection ? t.c.sectionLabel : t.c.chassisLabel);
    g.setFont (labelFont (t.labelSize).withExtraKerningFactor (0.08f));
    g.drawFittedText (text.toUpperCase(), b.toNearestInt(), j, 1, 0.75f);
}

void FreddyLookAndFeel::drawJogWheel (juce::Graphics& g, juce::Rectangle<float> b, float angle, bool active)
{
    const float size = std::min (b.getWidth(), b.getHeight());
    auto r = juce::Rectangle<float> (size, size).withCentre (b.getCentre());
    g.setColour (t.c.knobBody.darker (0.3f));
    g.fillEllipse (r);
    g.setColour (t.c.knobBody);
    g.fillEllipse (r.reduced (size * 0.06f));
    // finger dimple
    const auto c = r.getCentre();
    auto dimple = juce::Rectangle<float> (size * 0.16f, size * 0.16f).withCentre (c.getPointOnCircumference (size * 0.32f, angle));
    g.setColour (t.c.knobBody.darker (0.4f));
    g.fillEllipse (dimple);
    // ticks
    g.setColour (active ? t.c.accent : t.c.knobTrack);
    for (int i = 0; i < 24; ++i)
    {
        const float a = angle + (float) i * juce::MathConstants<float>::twoPi / 24.0f;
        g.drawLine ({ c.getPointOnCircumference (size * 0.42f, a), c.getPointOnCircumference (size * 0.47f, a) }, 1.2f);
    }
}

void FreddyLookAndFeel::drawMeter (juce::Graphics& g, juce::Rectangle<float> b, float peakDb, float rmsDb, const juce::String& label)
{
    auto labelArea = b.removeFromBottom (14.0f);
    drawControlLabel (g, labelArea, label, true);
    g.setColour (t.c.lcdBg);
    g.fillRect (b);
    auto norm = [] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f); };
    const int segs = 24;
    const float sh = b.getHeight() / (float) segs;
    const int litR = juce::roundToInt (norm (rmsDb) * segs);
    const int litP = juce::roundToInt (norm (peakDb) * segs);
    for (int i = 0; i < segs; ++i)
    {
        auto s = juce::Rectangle<float> (b.getX() + 1, b.getBottom() - (float) (i + 1) * sh + 1, b.getWidth() - 2, sh - 1.5f);
        const bool hot = i >= segs - 3;
        const auto onCol = hot ? t.c.accent : t.c.lcdFg;
        g.setColour (i < litR ? onCol : (i == litP - 1 ? onCol.withAlpha (0.85f) : t.c.lcdDim.withAlpha (0.35f)));
        g.fillRect (s);
    }
}

void FreddyLookAndFeel::drawCorrelation (juce::Graphics& g, juce::Rectangle<float> b, float corr)
{
    g.setColour (t.c.lcdBg);
    g.fillRect (b);
    g.setColour (t.c.lcdDim);
    g.fillRect (b.getCentreX() - 0.5f, b.getY(), 1.0f, b.getHeight());
    const float x = b.getX() + (juce::jlimit (-1.0f, 1.0f, corr) * 0.5f + 0.5f) * b.getWidth();
    g.setColour (corr < 0 ? t.c.accent : t.c.lcdFg);
    g.fillRect (juce::Rectangle<float> (x - 2.0f, b.getY() + 1.0f, 4.0f, b.getHeight() - 2.0f));
}

void FreddyLookAndFeel::drawCurveGlyph (juce::Graphics& g, juce::Rectangle<float> b, int shape, juce::Colour col)
{
    // Small icon of the duck curve (0 = ducked, 1 = recovered).
    juce::Path p;
    const int n = 24;
    for (int i = 0; i <= n; ++i)
    {
        const float x = (float) i / (float) n;
        float y = x;
        switch (shape)
        {
            case 1: y = (1.0f - std::exp (-4.0f * x)) / (1.0f - std::exp (-4.0f)); break;
            case 2: y = (std::exp (4.0f * x) - 1.0f) / (std::exp (4.0f) - 1.0f); break;
            case 3: y = x * x * (3.0f - 2.0f * x); break;
            case 4: y = x < 0.5f ? 0.0f : (x - 0.5f) * 2.0f; break;
            case 5: y = std::pow (std::sin (x * juce::MathConstants<float>::halfPi), 0.6f) * (x < 0.08f ? x / 0.08f : 1.0f); break;
            case 6: y = std::min (1.0f, 1.1f * (1.0f - std::exp (-5.0f * x))) - (x > 0.6f ? 0.0f : 0.0f); if (x > 0.45f) y = 1.0f + 0.12f * std::exp (-8.0f * (x - 0.45f)) * std::sin ((x - 0.45f) * 6.0f); break;
            default: break;
        }
        const float px = b.getX() + x * b.getWidth();
        const float py = b.getBottom() - juce::jlimit (-0.1f, 1.15f, y) * b.getHeight() * 0.85f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ---------------------------------------------------------------------------
// Displays
// ---------------------------------------------------------------------------
juce::Rectangle<float> FreddyLookAndFeel::drawDisplayFrame (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (t.c.lcdBezel);
    g.fillRoundedRectangle (b, 4.0f);
    auto inner = b.reduced (5.0f);
    g.setColour (t.c.lcdBg);
    g.fillRect (inner);
    return inner;
}

void FreddyLookAndFeel::drawDisplayOverlay (juce::Graphics& g, juce::Rectangle<float> inner)
{
    if (! t.pixelGrid) return;
    // flat pixel grid: thin lines every 3 px
    g.setColour (t.c.lcdGrid);
    for (float x = inner.getX(); x < inner.getRight(); x += 3.0f) g.fillRect (x, inner.getY(), 1.0f, inner.getHeight());
    for (float y = inner.getY(); y < inner.getBottom(); y += 3.0f) g.fillRect (inner.getX(), y, inner.getWidth(), 1.0f);
}

void FreddyLookAndFeel::drawDisplayText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> b,
                                         juce::Justification j, bool dim)
{
    const auto shape = (t.lcdStyle == LcdStyle::vfdCyan || t.lcdStyle == LcdStyle::vfdAmber || t.lcdStyle == LcdStyle::redLed)
                     ? PixelFont::DotShape::round : PixelFont::DotShape::square;
    PixelFont::drawFitted (g, s.toUpperCase(), b, j, dim ? t.c.lcdDim : t.c.lcdFg, juce::Colours::transparentBlack, shape, 2.2f);
}

void FreddyLookAndFeel::drawValueReadout (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> b)
{
    if (t.lcdStyle == LcdStyle::redLed)
        PixelFont::drawSevenSegment (g, s, b, t.c.lcdFg, t.c.lcdFg.withAlpha (0.08f));
    else
        drawDisplayText (g, s, b, juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// Stock widgets
// ---------------------------------------------------------------------------
void FreddyLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (t.c.section);
    g.setColour (t.c.accent);
    g.drawRect (0, 0, w, h, 1);
}

juce::Font FreddyLookAndFeel::getPopupMenuFont() { return labelFont (16.0f); }

void FreddyLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (t.c.lcdBg);
    g.setColour (t.c.lcdFg);
    g.drawRect (0, 0, w, h, 1);
    g.setFont (monoFont (14.0f));
    g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (6, 3), juce::Justification::centredLeft, 3);
}

juce::Rectangle<int> FreddyLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const int w = std::min (360, (int) juce::GlyphArrangement::getStringWidth (monoFont (14.0f), text) + 16);
    const int lines = juce::GlyphArrangement::getStringWidth (monoFont (14.0f), text) > 344 ? 2 : 1;
    const int h = 10 + lines * 16;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h).constrainedWithin (parent);
}

void FreddyLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    juce::Rectangle<float> b (0, 0, (float) w, (float) h);
    g.setColour (t.c.lcdBezel); g.fillRoundedRectangle (b, 3.0f);
    g.setColour (t.c.lcdBg);    g.fillRoundedRectangle (b.reduced (2.0f), 2.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (t.c.lcdFg); g.fillPath (arrow);
}

juce::Font FreddyLookAndFeel::getComboBoxFont (juce::ComboBox&) { return monoFont (14.0f); }

void FreddyLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (t.c.lcdBg);
    g.fillRect (0, 0, w, h);
}

void FreddyLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? t.c.accent : t.c.lcdDim);
    g.drawRect (0, 0, w, h, 1);
}

void FreddyLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                       int thumbStart, int thumbSize, bool, bool)
{
    g.setColour (t.c.lcdDim.withAlpha (0.6f));
    if (vertical) g.fillRect ((float) x + (float) w * 0.3f, (float) thumbStart, (float) w * 0.4f, (float) thumbSize);
    else          g.fillRect ((float) thumbStart, (float) y + (float) h * 0.3f, (float) thumbSize, (float) h * 0.4f);
}
} // namespace fv::ui
