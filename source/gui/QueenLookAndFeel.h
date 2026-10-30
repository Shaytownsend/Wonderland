#pragma once

#include "Filmstrip.h"
#include "Layout.h"

namespace queen
{
// All sprites are loaded once (ImageCache) and kept here. Nothing is hand-drawn: if a sprite is missing
// the control is simply invisible — placeholders come from tools/make_placeholders.py only.
class QueenLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit QueenLookAndFeel (const Layout& layout);

    const Filmstrip& strip (const juce::String& file) const;
    const juce::Image& image (const juce::String& file) const;

    // properties set on sliders: "strip" (knob) or "cap" + "capH" (fader)
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 0; }

    // flat host bar style (matches textures/C_host.png)
    static constexpr juce::uint32 hostBarColour = 0xff1c1c1f, hostBoxColour = 0xff0b0b0d, hostLineColour = 0xff3a3a40,
                                  hostTextColour = 0xffefefef, hostDimTextColour = 0xff9a9aa2;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

private:
    std::map<juce::String, Filmstrip> strips;
    std::map<juce::String, juce::Image> images;
    Filmstrip empty;
    juce::Image emptyImage;
};
} // namespace queen
