#include <catch2/catch_test_macros.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/PixelFont.h"

TEST_CASE ("7-segment readout draws the decimal point", "[ui]")
{
    auto render = [] (const juce::String& s)
    {
        juce::Image im (juce::Image::RGB, 300, 60, true);
        juce::Graphics g (im);
        fv::ui::PixelFont::drawSevenSegment (g, s, { 10, 10, 280, 40 }, juce::Colours::red, juce::Colours::transparentBlack);
        int lit = 0;
        for (int y = 0; y < 60; ++y) for (int x = 0; x < 300; ++x) if (im.getPixelAt (x, y).getRed() > 128) ++lit;
        return lit;
    };
    CHECK (render ("-24.0 DB") > render ("-240 DB"));
}
