#include <catch2/catch_test_macros.hpp>
#include <gui/Filmstrip.h>
#include <gui/Layout.h>

// The shipped build must contain the 4x Blender renders, never tools/make_placeholders.py output.
TEST_CASE ("Assets are real 4x renders, not placeholders", "[assets]")
{
    const auto& layout = queen::Layout::get();
    const auto panel = queen::assets::image (layout.background);
    REQUIRE (panel.isValid());
    CHECK (panel.getWidth() == 7200); // 1800 logical x 4
    CHECK (panel.getHeight() == 3320);

    int knobStrips = 0;
    for (const auto& c : layout.controls)
    {
        if (c.type != "knob")
            continue;
        INFO (c.id.toStdString());
        const auto strip = queen::assets::image (c.spriteName ("strip"));
        REQUIRE (strip.isValid());
        CHECK (strip.getWidth() == juce::roundToInt (c.spriteFloat ("size") * 4.0f));
        CHECK (strip.getHeight() == strip.getWidth() * 128); // square frames x 128
        CHECK (queen::Filmstrip (strip, 1.0f).getNumFrames() == 128);
        ++knobStrips;
    }
    CHECK (knobStrips == 21);

    for (const auto& f : layout.spriteFiles())
    {
        INFO (f.toStdString());
        CHECK (f.contains ("@4x"));
    }
}
