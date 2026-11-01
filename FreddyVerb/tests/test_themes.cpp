// Acceptance tests for all 13 UI options: WCAG AA contrast and distinguishable visualizer colours.
#include <catch2/catch_test_macros.hpp>
#include "ui/Theme.h"
#include "ui/FreddyLookAndFeel.h"

using namespace fv::ui;

namespace
{
    float contrast (juce::Colour fg, juce::Colour bg) { return FreddyLookAndFeel::contrastRatio (fg.withAlpha (1.0f), bg); }

    /** Composite a translucent colour over a background. */
    juce::Colour over (juce::Colour c, juce::Colour bg) { return bg.interpolatedWith (c.withAlpha (1.0f), c.getFloatAlpha()); }

    /** CIE76 delta-E between two opaque colours (sRGB -> Lab). */
    double deltaE (juce::Colour a, juce::Colour b)
    {
        auto lab = [] (juce::Colour c)
        {
            auto lin = [] (double v) { return v <= 0.04045 ? v / 12.92 : std::pow ((v + 0.055) / 1.055, 2.4); };
            const double r = lin (c.getFloatRed()), g = lin (c.getFloatGreen()), bl = lin (c.getFloatBlue());
            double x = (0.4124 * r + 0.3576 * g + 0.1805 * bl) / 0.95047;
            double y = (0.2126 * r + 0.7152 * g + 0.0722 * bl);
            double z = (0.0193 * r + 0.1192 * g + 0.9505 * bl) / 1.08883;
            auto f = [] (double t) { return t > 0.008856 ? std::cbrt (t) : 7.787 * t + 16.0 / 116.0; };
            return std::array<double, 3> { 116 * f (y) - 16, 500 * (f (x) - f (y)), 200 * (f (y) - f (z)) };
        };
        auto A = lab (a), B = lab (b);
        return std::sqrt ((A[0] - B[0]) * (A[0] - B[0]) + (A[1] - B[1]) * (A[1] - B[1]) + (A[2] - B[2]) * (A[2] - B[2]));
    }
}

TEST_CASE ("There are 13 themes: 8 flat then 5 photoreal, numbered 1..13", "[themes]")
{
    const auto& t = allThemes();
    REQUIRE (t.size() == 13);
    for (size_t i = 0; i < t.size(); ++i)
    {
        CHECK (t[i].theme.number == (int) i + 1);
        CHECK (t[i].theme.group == (i < 8 ? ThemeGroup::flat : ThemeGroup::photoreal));
        CHECK (t[i].theme.description.isNotEmpty());
    }
}

TEST_CASE ("Text contrast meets WCAG AA (4.5:1) in every theme", "[themes][contrast]")
{
    for (auto& e : allThemes())
    {
        const auto& c = e.theme.c;
        INFO (e.theme.name);
        CHECK (contrast (c.lcdFg, c.lcdBg) >= 4.5f);          // LCD readouts
        CHECK (contrast (c.padText, c.pad) >= 4.5f);          // pad legends
        if (e.theme.group == ThemeGroup::flat)
        {
            CHECK (contrast (c.sectionLabel, c.section) >= 4.5f);  // printed section labels
            CHECK (contrast (c.chassisLabel, c.chassis) >= 4.5f);  // labels on the chassis
        }
    }
}

TEST_CASE ("Envelope colours stay distinguishable in every theme", "[themes][viz]")
{
    for (auto& e : allThemes())
    {
        const auto& c = e.theme.c;
        INFO (e.theme.name);
        const auto bg = c.lcdBg;
        const juce::Colour traces[] = { over (c.vizDuck, bg), over (c.vizGate, bg), over (c.vizCombined, bg) };
        for (int i = 0; i < 3; ++i)
        {
            CHECK (deltaE (traces[i], bg) > 25.0);              // visible on the display
            for (int j = i + 1; j < 3; ++j) CHECK (deltaE (traces[i], traces[j]) > 20.0);  // duck vs gate vs combined
        }
        CHECK (deltaE (over (c.vizBloom, bg), bg) > 3.0);       // bloom shading is visible...
        CHECK (deltaE (over (c.vizWet, bg), bg) > 15.0);
    }
}

TEST_CASE ("Release default theme is the M6 choice (Tour-Worn Champagne) and exists", "[themes]")
{
    const auto* t = findTheme (defaultThemeId());
    REQUIRE (t != nullptr);
    CHECK (t->theme.name == "Tour-Worn Champagne");
    CHECK (t->theme.group == ThemeGroup::photoreal);
}
