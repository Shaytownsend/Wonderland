#include "QueenKeyboard.h"

namespace queen
{
QueenKeyboard::QueenKeyboard (juce::MidiKeyboardState& keyState, const ControlSpec& keys, const QueenLookAndFeel& lnf)
    : juce::MidiKeyboardComponent (keyState, horizontalKeyboard), spec (keys), lookAndFeel (lnf)
{
    whiteW = spec.spriteFloat ("whiteW", 43.33f);
    whiteH = spec.spriteFloat ("whiteH", 242.0f);
    blackW = spec.spriteFloat ("blackW", 31.0f);
    blackH = spec.spriteFloat ("blackH", 154.0f);
    const float pitch = spec.w / 36.0f;
    blackRatio = (float) (double) spec.raw.getProperty ("black_w", 25.5) / pitch;

    const int lowest = (int) spec.raw.getProperty ("lowest_note", 36);
    setAvailableRange (lowest, lowest + 60);
    setScrollButtonsVisible (false);
    setBlackNoteWidthProportion (blackRatio);
    setBlackNoteLengthProportion ((float) (double) spec.raw.getProperty ("black_h", 146.3) / spec.h);
    setOctaveForMiddleC (4);
    // the base class background/shadow/separators would cover the felt: keys are sprites only
    for (auto id : { whiteNoteColourId, shadowColourId, keySeparatorLineColourId, blackNoteColourId,
                     keyDownOverlayColourId, mouseOverKeyOverlayColourId })
        setColour (id, juce::Colours::transparentBlack);
    setOpaque (false);
}

juce::Range<float> QueenKeyboard::getKeyPosition (int note, float kw) const
{
    static constexpr int whiteIndex[12] { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };
    static constexpr bool isBlack[12] { false, true, false, true, false, false, true, false, true, false, true, false };
    const int octave = note / 12, n = note % 12;
    const float octaveStart = (float) octave * 7.0f * kw;
    if (! isBlack[n])
        return { octaveStart + (float) whiteIndex[n] * kw, octaveStart + (float) (whiteIndex[n] + 1) * kw };
    const float centre = octaveStart + (float) whiteIndex[n] * kw; // on the boundary
    const float half = kw * blackRatio * 0.5f;
    return { centre - half, centre + half };
}

void QueenKeyboard::drawWhiteNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool, juce::Colour,
                                   juce::Colour)
{
    const auto& img = lookAndFeel.image (spec.spriteName (isDown ? "white_down" : "white_up"));
    if (! img.isValid())
        return;
    const float scale = area.getWidth() / whiteW; // keyboard may be scaled
    drawCrisp (g, img, juce::Rectangle<float> (area.getX(), area.getY() - 3.0f * scale, whiteW * scale, whiteH * scale));
}

void QueenKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool, juce::Colour)
{
    const auto& img = lookAndFeel.image (spec.spriteName (isDown ? "black_down" : "black_up"));
    if (! img.isValid())
        return;
    const float scale = getKeyWidth() / (spec.w / 36.0f);
    const float w = blackW * scale, h = blackH * scale;
    drawCrisp (g, img, juce::Rectangle<float> (area.getCentreX() - w / 2.0f, area.getY() - 3.0f * scale, w, h));
}
} // namespace queen
