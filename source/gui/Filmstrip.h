#pragma once

#include <BinaryData.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace queen
{
namespace assets
{
    inline const char* find (const juce::String& filename, int& size)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            if (filename == BinaryData::originalFilenames[i])
                return BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        size = 0;
        return nullptr;
    }

    inline juce::Image image (const juce::String& filename)
    {
        int size = 0;
        if (auto* data = find (filename, size))
            return juce::ImageCache::getFromMemory (data, size);
        jassertfalse; // every sprite named in layout.json must be in assets/
        return {};
    }

    inline juce::String text (const juce::String& filename)
    {
        int size = 0;
        if (auto* data = find (filename, size))
            return juce::String::fromUTF8 (data, size);
        return {};
    }
} // namespace assets

// A vertical strip of equally sized frames. The frame count is derived from the image and the frame's
// logical aspect ratio, so 2x renders (128 frames) and 1x placeholders (fewer frames) both just work.
class Filmstrip
{
public:
    Filmstrip() = default;

    Filmstrip (juce::Image strip, float frameAspectHOverW, int expectedFrames = 0)
    {
        image = std::move (strip);
        if (! image.isValid())
            return;
        int frameH = expectedFrames > 0 && image.getHeight() % expectedFrames == 0
                         ? image.getHeight() / expectedFrames
                         : juce::roundToInt ((float) image.getWidth() * frameAspectHOverW);
        frameH = juce::jmax (1, frameH);
        const int n = juce::jmax (1, image.getHeight() / frameH);
        for (int i = 0; i < n; ++i)
            frames.push_back (image.getClippedImage ({ 0, i * frameH, image.getWidth(), frameH }));
    }

    bool isValid() const { return ! frames.empty(); }
    int getNumFrames() const { return (int) frames.size(); }
    const juce::Image& getStrip() const { return image; }

    // proportion 0..1 -> frame round(p * (n-1))
    int frameFor (double proportion) const
    {
        return juce::jlimit (0, getNumFrames() - 1, (int) std::lround (proportion * (getNumFrames() - 1)));
    }

    void draw (juce::Graphics& g, int frame, juce::Rectangle<float> dest) const
    {
        if (frames.empty())
            return;
        g.drawImage (frames[(size_t) juce::jlimit (0, getNumFrames() - 1, frame)], dest, juce::RectanglePlacement::stretchToFit);
    }

private:
    juce::Image image;
    std::vector<juce::Image> frames;
};
} // namespace queen
