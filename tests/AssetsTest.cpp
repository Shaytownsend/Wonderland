#include <catch2/catch_test_macros.hpp>
#include <gui/Filmstrip.h>
#include <gui/Layout.h>

// The shipped build must contain the Blender renders, never tools/make_placeholders.py output.
TEST_CASE ("Assets are real renders, not placeholders", "[assets]")
{
    const auto panel = queen::assets::image ("panel@2x.png");
    REQUIRE (panel.isValid());
    CHECK (panel.getWidth() > 3000);

    for (auto colour : { "cream", "red", "blue", "black" })
    {
        INFO (colour);
        const auto strip = queen::assets::image (juce::String ("knob_") + colour + "@2x.png");
        REQUIRE (strip.isValid());
        CHECK (strip.getHeight() == strip.getWidth() * 128); // square frames x 128
        CHECK (queen::Filmstrip (strip, 1.0f).getNumFrames() == 128);
    }
}
