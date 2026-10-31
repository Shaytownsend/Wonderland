#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fv::ui
{
/** Crisp bitmap-style text for the LCD/VFD readouts, drawn as vector dots so it
    stays sharp at every scale. Classic 5x7 dot-matrix glyphs, plus a 7-segment
    renderer for LED readouts. */
struct PixelFont
{
    enum class DotShape { square, round, segment };

    /** Draws text with 5x7 cells. `pixel` is the dot pitch in logical pixels.
        Returns the width used. */
    static float drawText (juce::Graphics& g, const juce::String& text, float x, float y, float pixel,
                           juce::Colour on, juce::Colour off = juce::Colours::transparentBlack,
                           DotShape shape = DotShape::square, float dotFill = 0.86f);

    static float textWidth (const juce::String& text, float pixel) { return (float) text.length() * 6.0f * pixel; }

    /** Fits text into a box (left/centred/right) choosing the pixel pitch from the box height. */
    static void drawFitted (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> box,
                            juce::Justification just, juce::Colour on,
                            juce::Colour off = juce::Colours::transparentBlack,
                            DotShape shape = DotShape::square, float maxPixel = 100.0f);

    /** Seven-segment LED readout (digits, a few letters, '-', '.', ' '). */
    static void drawSevenSegment (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> box,
                                  juce::Colour on, juce::Colour off, float slant = 0.08f);

    /** Raw glyph columns (5 bytes, LSB = top row) for ASCII 32..126. */
    static const uint8_t* glyph (juce::juce_wchar c) noexcept;
};
} // namespace fv::ui
