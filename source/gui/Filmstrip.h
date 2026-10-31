#pragma once

#include <BinaryData.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <tuple>

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

    // Where non-embedded art is looked for (QUEEN_EMBED_ASSETS=0 builds): $QUEEN_ASSETS_DIR, then the
    // installer's shared folder, then the per-user one.
    inline juce::Array<juce::File> searchDirectories()
    {
        juce::Array<juce::File> dirs;
        const auto env = juce::SystemStats::getEnvironmentVariable ("QUEEN_ASSETS_DIR", {});
        if (env.isNotEmpty())
            dirs.add (juce::File (env));
        for (auto where : { juce::File::commonApplicationDataDirectory, juce::File::userApplicationDataDirectory })
        {
            auto base = juce::File::getSpecialLocation (where); // macOS: /Library, ~/Library; Linux: /opt, ~/.config
           #if JUCE_MAC
            base = base.getChildFile ("Application Support");
           #endif
            dirs.add (base.getChildFile ("The Queen").getChildFile ("Assets"));
        }
        return dirs;
    }

    inline juce::Image image (const juce::String& filename)
    {
        int size = 0;
        if (auto* data = find (filename, size))
            return juce::ImageCache::getFromMemory (data, size);
        for (const auto& dir : searchDirectories())
            if (auto f = dir.getChildFile (filename); f.existsAsFile())
                return juce::ImageCache::getFromFile (f);
        jassertfalse; // every sprite named in layout.json must be in assets/ (embedded or installed)
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

// Draws `src` into `dest` from a copy pre-scaled to the exact physical pixel size (halving steps, then a
// high-quality final resample), so the 2x renders stay sharp at any window size / display density instead
// of being resampled on the fly by the graphics backend. Message thread only.
// With `owner` (the component being painted) the image is also snapped to the physical pixel grid, so
// 1-pixel details are not smeared across two screen pixels by a fractional position.
inline void drawCrisp (juce::Graphics& g, const juce::Image& src, juce::Rectangle<float> dest,
                       const juce::Component* owner = nullptr)
{
    if (! src.isValid() || dest.isEmpty())
        return;

    struct Cache
    {
        float scale = 0.0f;
        std::map<std::tuple<const void*, int, int>, juce::Image> images;
    };
    static Cache cache;

    const float t = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (! juce::approximatelyEqual (t, cache.scale))
    {
        cache.images.clear(); // new window size or display: rebuild lazily
        cache.scale = t;
    }
    const int pw = juce::jmax (1, juce::roundToInt (dest.getWidth() * t));
    const int ph = juce::jmax (1, juce::roundToInt (dest.getHeight() * t));
    const auto key = std::make_tuple ((const void*) src.getPixelData().get(), pw, ph);

    auto it = cache.images.find (key);
    if (it == cache.images.end())
    {
        juce::Image img = src;
        while (img.getWidth() >= pw * 2 && img.getHeight() >= ph * 2)
            img = img.rescaled (img.getWidth() / 2, img.getHeight() / 2, juce::Graphics::highResamplingQuality);
        if (img.getWidth() != pw || img.getHeight() != ph)
            img = img.rescaled (pw, ph, juce::Graphics::highResamplingQuality);
        it = cache.images.emplace (key, img).first;
    }

    if (owner != nullptr)
    {
        // physical position of dest's corner: component -> top level (logical, transforms included) -> peer pixels
        const auto* top = owner->getTopLevelComponent();
        const auto origin = top->getLocalPoint (owner, dest.getTopLeft());
        const float componentScale = top->getLocalPoint (owner, juce::Point<float> (1.0f, 0.0f)).x
                                     - top->getLocalPoint (owner, juce::Point<float>()).x;
        if (componentScale > 0.0f)
        {
            const float peerScale = t / componentScale;
            const auto phys = origin * peerScale;
            const juce::Point<float> snap ((std::round (phys.x) - phys.x) / t, (std::round (phys.y) - phys.y) / t);
            dest = dest.translated (snap.x, snap.y);
        }
    }

    juce::Graphics::ScopedSaveState state (g);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageTransformed (it->second, juce::AffineTransform::scale (dest.getWidth() / (float) pw, dest.getHeight() / (float) ph)
                                            .translated (dest.getX(), dest.getY()));
}

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

    void draw (juce::Graphics& g, int frame, juce::Rectangle<float> dest, const juce::Component* owner = nullptr) const
    {
        if (frames.empty())
            return;
        drawCrisp (g, frames[(size_t) juce::jlimit (0, getNumFrames() - 1, frame)], dest, owner);
    }

private:
    juce::Image image;
    std::vector<juce::Image> frames;
};
} // namespace queen
