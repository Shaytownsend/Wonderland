#include "QueenLookAndFeel.h"

namespace queen
{
QueenLookAndFeel::QueenLookAndFeel (const Layout& layout)
{
    for (const auto& file : layout.spriteFiles())
        images[file] = assets::image (file);

    // strips: frame aspect from the layout's logical frame size
    for (const auto& c : layout.controls)
    {
        const auto name = c.spriteName ("strip");
        if (name.isEmpty() || strips.contains (name))
            continue;
        float aspect = 1.0f;
        int expected = 0;
        if (c.type == "switch" || c.type == "display")
        {
            aspect = c.spriteFloat ("h") / juce::jmax (1.0f, c.spriteFloat ("w"));
            expected = (int) c.sprite.getProperty ("frames", 0);
        }
        strips[name] = Filmstrip (images[name], aspect, expected);
    }

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (hostBoxColour));
    setColour (juce::PopupMenu::textColourId, juce::Colour (hostTextColour));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (hostLineColour));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

const Filmstrip& QueenLookAndFeel::strip (const juce::String& file) const
{
    auto it = strips.find (file);
    return it != strips.end() ? it->second : empty;
}

const juce::Image& QueenLookAndFeel::image (const juce::String& file) const
{
    auto it = images.find (file);
    return it != images.end() ? it->second : emptyImage;
}

void QueenLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                         juce::Slider& s)
{
    const auto& fs = strip (s.getProperties()["strip"].toString());
    fs.draw (g, fs.frameFor (pos), juce::Rectangle<int> (x, y, w, h).toFloat());
}

juce::Slider::SliderLayout QueenLookAndFeel::getSliderLayout (juce::Slider& s)
{
    juce::Slider::SliderLayout layout;
    layout.sliderBounds = s.getLocalBounds();
    if (! s.isRotary())
    {
        // the travel region is the cap centre's path: inset by half a cap
        const int inset = juce::roundToInt ((float) (double) s.getProperties()["capH"] / 2.0f);
        layout.sliderBounds = s.getLocalBounds().reduced (0, inset);
    }
    return layout;
}

void QueenLookAndFeel::drawLinearSlider (juce::Graphics& g, int, int, int, int, float, float, float,
                                         juce::Slider::SliderStyle, juce::Slider& s)
{
    const auto& img = image (s.getProperties()["cap"].toString());
    if (! img.isValid())
        return;
    // position straight from the value over the whole component (travel = height - capH), so the cap
    // never depends on when JUCE last computed the slider layout
    const float capH = (float) (double) s.getProperties()["capH"];
    const float capW = (float) s.getWidth();
    const float x = 0.0f;
    const auto proportion = (float) s.valueToProportionOfLength (s.getValue());
    const float sliderPos = capH / 2.0f + (1.0f - proportion) * ((float) s.getHeight() - capH);
    drawCrisp (g, img, juce::Rectangle<float> ((float) x, sliderPos - capH / 2.0f, capW, capH));
}

void QueenLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (juce::Colour (hostBoxColour));
    g.setColour (juce::Colour (hostLineColour));
    g.drawRect (0, 0, width, height);
}
} // namespace queen
