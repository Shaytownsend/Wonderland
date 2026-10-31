#include "PhotorealLookAndFeel.h"

namespace fv::ui
{
PhotorealLookAndFeel::PhotorealLookAndFeel (const FreddyTheme& th) : FreddyLookAndFeel (th), folder (th.skinFolder)
{
    auto [data, size] = SkinAssets::get (folder, "layout.json");
    if (data == nullptr) return;

    auto json = juce::JSON::parse (juce::String::fromUTF8 ((const char*) data, size));
    if (! json.isObject()) return;
    logicalW = (float) (double) json.getProperty ("width", 1600);
    logicalH = (float) (double) json.getProperty ("height", 442);
    knobFrames = (int) json.getProperty ("knob_frames", 128);
    if (auto* arr = json.getProperty ("controls", {}).getArray())
        for (auto& c : *arr)
        {
            const auto id = c.getProperty ("id", {}).toString();
            juce::Rectangle<float> r ((float) (double) c.getProperty ("x", 0), (float) (double) c.getProperty ("y", 0),
                                      (float) (double) c.getProperty ("w", 0), (float) (double) c.getProperty ("h", 0));
            layout.rects[id] = r;
            layout.kinds[id] = c.getProperty ("kind", {}).toString();
            art[id] = c.getProperty ("art", {}).toString();
            if (auto* led = c.getProperty ("led", {}).getArray(); led != nullptr && led->size() == 4)
                leds[id] = { (float) (double) (*led)[0], (float) (double) (*led)[1], (float) (double) (*led)[2], (float) (double) (*led)[3] };
        }
    hasLayout = ! layout.rects.empty();
}

int PhotorealLookAndFeel::pickScale (juce::Graphics& g)
{
    const float s = g.getInternalContext().getPhysicalPixelScaleFactor();
    return s <= 1.05f ? 1 : (s <= 2.05f ? 2 : 3);
}

const juce::Image& PhotorealLookAndFeel::image (const juce::String& base, int scale)
{
    // fall back to the next available resolution (prefer larger: downscaling is clean)
    for (int s : { scale, scale + 1, scale + 2, scale - 1, scale - 2 })
    {
        if (s < 1 || s > 3) continue;
        const auto key = base + "@" + juce::String (s) + "x";
        if (auto it = cache.find (key); it != cache.end()) return it->second;
        for (auto ext : { ".png", ".jpg" })
        {
            auto [data, size] = SkinAssets::get (folder, key + ext);
            if (data != nullptr)
            {
                auto img = juce::ImageFileFormat::loadFrom (data, (size_t) size);
                if (img.isValid()) return cache[key] = img;
            }
        }
    }
    return empty;
}

juce::String PhotorealLookAndFeel::controlArt (const juce::String& id, const juce::String& fallback) const
{
    auto key = id.startsWith ("eqsel_") ? id.upToFirstOccurrenceOf ("#", false, false) : id;
    if (auto it = art.find (key); it != art.end() && it->second.isNotEmpty()) return it->second;
    return fallback;
}

void PhotorealLookAndFeel::drawChassis (juce::Graphics& g, juce::Rectangle<float> b)
{
    const auto& img = image ("panel", pickScale (g));
    if (img.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (img, b, juce::RectanglePlacement::stretchToFit);
        return;
    }
    // procedural fallback until panel_forge assets exist
    juce::ColourGradient grad (t.c.chassis.brighter (0.15f), 0, b.getY(), t.c.chassis.darker (0.25f), 0, b.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (b);
    drawRackEars (g, b, b.getWidth() * 0.04f);
    g.setColour (t.c.chassisLabel);
    g.setFont (labelFont (18.0f));
    g.drawText ("FREDDY VERB  " + t.name.toUpperCase() + "  (panel_forge assets pending)", b.reduced (80, 6).removeFromTop (24), juce::Justification::centredLeft);
}

void PhotorealLookAndFeel::drawOverlay (juce::Graphics& g, juce::Rectangle<float> b)
{
    const auto& img = image ("overlay", pickScale (g));
    if (img.isValid())
        g.drawImage (img, b, juce::RectanglePlacement::stretchToFit);
}

void PhotorealLookAndFeel::drawSection (juce::Graphics& g, juce::Rectangle<float> b, const SectionInfo& s)
{
    if (hasLayout) return; // sections are part of the rendered panel
    FreddyLookAndFeel::drawSection (g, b, s);
}

void PhotorealLookAndFeel::drawKnob (juce::Graphics& g, juce::Rectangle<float> b, const KnobState& k)
{
    const auto& strip = image (controlArt (k.componentID, "knob_small"), pickScale (g));
    if (! strip.isValid()) { FreddyLookAndFeel::drawKnob (g, b, k); return; }
    const int fw = strip.getWidth();
    const int frames = std::max (1, strip.getHeight() / fw);
    const int idx = juce::jlimit (0, frames - 1, juce::roundToInt (k.value * (float) (frames - 1)));
    const float size = std::min (b.getWidth(), b.getHeight());
    auto dst = juce::Rectangle<float> (size, size).withCentre (b.getCentre());
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (strip, dst.getX(), dst.getY(), dst.getWidth(), dst.getHeight(), 0, idx * fw, fw, fw);
    if (k.hover || k.dragging)
    {
        // subtle value ring so the hovered knob is identifiable
        g.setColour (t.c.lcdFg.withAlpha (k.dragging ? 0.5f : 0.25f));
        g.drawEllipse (dst.reduced (1.0f), 1.2f);
    }
}

void PhotorealLookAndFeel::drawPad (juce::Graphics& g, juce::Rectangle<float> b, const PadState& p)
{
    const auto base = controlArt (p.componentID, "btn");
    const int sc = pickScale (g);
    const auto& up = image (base + "_up", sc);
    if (! up.isValid()) { FreddyLookAndFeel::drawPad (g, b, p); return; }
    const auto& down = image (base + "_down", sc);
    const auto& on = image (base + "_on", sc);
    const juce::Image* img = &up;
    if (p.down && down.isValid()) img = &down;
    else if (p.on && on.isValid()) img = &on;
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (*img, b, juce::RectanglePlacement::stretchToFit);

    // LED lens
    auto ledIt = leds.find (p.componentID);
    if (p.hasLed && ledIt != leds.end())
    {
        const auto& led = image (p.on ? "led_on" : "led_off", sc);
        // panel_forge stores LED rects relative to the control's own bounds
        auto r = ledIt->second;
        if (led.isValid())
            g.drawImage (led, r.withSizeKeepingCentre (r.getWidth() * 2.5f, r.getHeight() * 2.5f), juce::RectanglePlacement::stretchToFit);
        else
        {
            g.setColour (p.on ? t.c.ledOn : t.c.ledOff);
            g.fillEllipse (r);
        }
    }
    if (p.down && ! down.isValid())
    {
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRect (b);
    }
}

void PhotorealLookAndFeel::drawJogWheel (juce::Graphics& g, juce::Rectangle<float> b, float angle, bool active)
{
    const auto& strip = image (controlArt ("jog", "knob_large"), pickScale (g));
    if (! strip.isValid()) { FreddyLookAndFeel::drawJogWheel (g, b, angle, active); return; }
    const int fw = strip.getWidth();
    const int frames = std::max (1, strip.getHeight() / fw);
    float a = std::fmod (angle, juce::MathConstants<float>::twoPi);
    if (a < 0) a += juce::MathConstants<float>::twoPi;
    const int idx = juce::jlimit (0, frames - 1, (int) (a / juce::MathConstants<float>::twoPi * (float) frames));
    const float size = std::min (b.getWidth(), b.getHeight());
    auto dst = juce::Rectangle<float> (size, size).withCentre (b.getCentre());
    g.drawImage (strip, dst.getX(), dst.getY(), dst.getWidth(), dst.getHeight(), 0, idx * fw, fw, fw);
}

void PhotorealLookAndFeel::drawMeter (juce::Graphics& g, juce::Rectangle<float> b, float peakDb, float rmsDb, const juce::String& label)
{
    const auto& needle = image ("vu_needle", pickScale (g));
    if (needle.isValid() && b.getWidth() > b.getHeight())
    {
        // analog VU: needle follows RMS (the LevelMeter already applies ballistics)
        const float vu = juce::jlimit (-20.0f, 3.0f, rmsDb + 18.0f); // -18 dBFS = 0 VU
        const float norm = (vu + 20.0f) / 23.0f;
        const float ang = juce::degreesToRadians (-48.0f + 96.0f * std::pow (norm, 1.6f));
        const juce::Point<float> pivot (b.getCentreX(), b.getBottom() - b.getHeight() * 0.06f);
        const float h = b.getHeight() * 0.88f;
        const float w = h * (float) needle.getWidth() / (float) needle.getHeight();
        auto tr = juce::AffineTransform::scale (w / (float) needle.getWidth(), h / (float) needle.getHeight())
                      .translated (pivot.x - w * 0.5f, pivot.y - h)
                      .rotated (ang, pivot.x, pivot.y);
        g.drawImageTransformed (needle, tr);
        return;
    }
    FreddyLookAndFeel::drawMeter (g, b.withTrimmedBottom (hasLayout ? -14.0f : 0.0f), peakDb, rmsDb, hasLayout ? juce::String() : label);
}

void PhotorealLookAndFeel::drawControlLabel (juce::Graphics& g, juce::Rectangle<float> b, const juce::String& text, bool onSection, juce::Justification j)
{
    if (hasLayout) return; // silkscreen is baked into the panel art
    FreddyLookAndFeel::drawControlLabel (g, b, text, onSection, j);
}

juce::Rectangle<float> PhotorealLookAndFeel::drawDisplayFrame (juce::Graphics& g, juce::Rectangle<float> b)
{
    if (! hasLayout) return FreddyLookAndFeel::drawDisplayFrame (g, b);
    // the bezel is part of the panel art; draw the unlit display surface
    juce::ColourGradient grad (t.c.lcdBg.brighter (0.06f), b.getX(), b.getY(), t.c.lcdBg.darker (0.2f), b.getX(), b.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (b);
    return b.reduced (2.0f);
}

void PhotorealLookAndFeel::drawDisplayOverlay (juce::Graphics& g, juce::Rectangle<float> inner)
{
    switch (t.lcdStyle)
    {
        case LcdStyle::vfdCyan:
        case LcdStyle::vfdAmber:
        {
            // fine VFD mesh grid
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            for (float x = inner.getX(); x < inner.getRight(); x += 2.0f) g.fillRect (x, inner.getY(), 0.5f, inner.getHeight());
            for (float y = inner.getY(); y < inner.getBottom(); y += 2.0f) g.fillRect (inner.getX(), y, inner.getWidth(), 0.5f);
            break;
        }
        case LcdStyle::crtGreen:
        {
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            for (float y = inner.getY(); y < inner.getBottom(); y += 2.0f) g.fillRect (inner.getX(), y, inner.getWidth(), 0.8f);
            juce::ColourGradient vignette (juce::Colours::transparentBlack, inner.getCentreX(), inner.getCentreY(),
                                           juce::Colours::black.withAlpha (0.45f), inner.getX(), inner.getY(), true);
            g.setGradientFill (vignette);
            g.fillRect (inner);
            break;
        }
        default:
            FreddyLookAndFeel::drawDisplayOverlay (g, inner);
            break;
    }
}

void PhotorealLookAndFeel::drawDisplayText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> b, juce::Justification j, bool dim)
{
    const bool vfd = t.lcdStyle == LcdStyle::vfdCyan || t.lcdStyle == LcdStyle::vfdAmber || t.lcdStyle == LcdStyle::crtGreen;
    const auto col = dim ? t.c.lcdDim : t.c.lcdFg;
    const auto shape = vfd ? PixelFont::DotShape::round : PixelFont::DotShape::square;
    PixelFont::drawFitted (g, s.toUpperCase(), b, j, col, {}, shape, 2.2f);
}

void PhotorealLookAndFeel::drawValueReadout (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> b)
{
    if (t.lcdStyle == LcdStyle::redLed || t.id == "harmonizer_black_79")
    {
        const auto led = t.id == "harmonizer_black_79" ? juce::Colour (0xffff2a1a) : t.c.lcdFg;
        // soft bloom: the same segments, offset by a fraction of a pixel (same cell grid,
        // so the glow lines up with the digits), then the lit segments on top
        for (auto o : { juce::Point<float> (-0.7f, 0.0f), { 0.7f, 0.0f }, { 0.0f, -0.7f }, { 0.0f, 0.7f } })
            PixelFont::drawSevenSegment (g, s, b.translated (o.x, o.y), led.withAlpha (0.16f), juce::Colours::transparentBlack);
        PixelFont::drawSevenSegment (g, s, b, led, led.withAlpha (0.07f));
        return;
    }
    drawDisplayText (g, s, b, juce::Justification::centred, false);
}

float PhotorealLookAndFeel::traceGlow() const
{
    switch (t.lcdStyle)
    {
        case LcdStyle::vfdCyan: case LcdStyle::vfdAmber: return 0.9f;
        case LcdStyle::crtGreen: return 1.0f;
        default: return 0.25f;
    }
}
} // namespace fv::ui
