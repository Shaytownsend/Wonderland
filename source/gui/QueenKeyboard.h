#pragma once

#include "QueenLookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace queen
{
// MidiKeyboardComponent drawn with the rendered key sprites; key geometry matches the scene
// (white keys on a fixed pitch, black keys centred on the white-key boundaries).
class QueenKeyboard : public juce::MidiKeyboardComponent
{
public:
    QueenKeyboard (juce::MidiKeyboardState& state, const ControlSpec& keys, const QueenLookAndFeel& lnf);

    juce::Range<float> getKeyPosition (int midiNoteNumber, float keyWidth) const override;
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour, juce::Colour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour) override;

private:
    const ControlSpec& spec;
    const QueenLookAndFeel& lookAndFeel;
    float whiteW, whiteH, blackW, blackH, blackRatio;
};
} // namespace queen
