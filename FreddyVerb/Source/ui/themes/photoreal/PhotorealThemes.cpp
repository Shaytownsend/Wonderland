// The 5 photoreal aged-rack-gear directions. Art comes from tools/panel_forge;
// this file holds each skin's display palette and envelope colours.
#include "PhotorealLookAndFeel.h"

namespace fv::ui
{
namespace
{
    using C = juce::Colour;

    FreddyTheme base (const char* id, const char* name, int number, const char* folder, const char* desc, bool fourU)
    {
        FreddyTheme t;
        t.id = id; t.name = name; t.number = number; t.skinFolder = folder; t.description = desc;
        t.group = ThemeGroup::photoreal;
        t.layout = LayoutVariant::photoreal;
        t.knobStyle = KnobStyle::filmstrip;
        t.padStyle = PadStyle::image;
        t.width = 1600;
        t.height = fourU ? 589 : 442;   // 19" x 7" (4U) or 19" x 5.25" (3U)
        // the skin's layout.json is authoritative for the size
        auto [data, size] = SkinAssets::get (folder, "layout.json");
        if (data != nullptr)
        {
            auto json = juce::JSON::parse (juce::String::fromUTF8 ((const char*) data, size));
            t.width  = (int) json.getProperty ("width", t.width);
            t.height = (int) json.getProperty ("height", t.height);
        }
        return t;
    }

    FreddyTheme harmonizer()
    {
        auto t = base ("harmonizer_black_79", "Harmonizer Black '79", 9, "harmonizer79",
                       "Black anodized brushed aluminium, worn white silkscreen, red 7-segment LEDs, square LED buttons, amber VFD envelope.", true);
        auto& c = t.c;
        c.chassis = C (0xff1a1a1b); c.chassisLabel = C (0xffe8e4dc); c.sectionLabel = C (0xffe8e4dc); c.text = C (0xffe8e4dc);
        c.accent = C (0xffff2a1a); c.secondary = C (0xff5ad1c8);
        c.lcdBg = C (0xff120a02); c.lcdFg = C (0xffffb347); c.lcdDim = C (0xff7a4a14);
        c.vizDry = C (0x40ffb347); c.vizWet = C (0xffffb347); c.vizDuck = C (0xffff5a2a); c.vizGate = C (0xff7fe8ff);
        c.vizCombined = C (0xfffff4d6); c.vizBloom = C (0x2effe0a0); c.vizThreshDuck = C (0xa0ff5a2a); c.vizThreshGate = C (0xa07fe8ff);
        c.ledOn = C (0xffff2a1a); c.ledOff = C (0xff3a0a06);
        t.lcdStyle = LcdStyle::vfdAmber;
        return t;
    }

    FreddyTheme reverb84()
    {
        auto t = base ("digital_reverb_84", "Digital Reverb Silver-Blue '84", 10, "reverb84",
                       "Silver-grey and blue-grey paint, green-backlit pixel LCD, grey rubber membrane keys, yellowed display bezel.", true);
        auto& c = t.c;
        c.chassis = C (0xffa9adb2); c.chassisLabel = C (0xff1e2a3a); c.sectionLabel = C (0xff1e2a3a); c.text = C (0xff1e2a3a);
        c.accent = C (0xffd8452a); c.secondary = C (0xff2d7f9a);
        c.lcdBg = C (0xff8fb04a); c.lcdFg = C (0xff17240c); c.lcdDim = C (0xff5d7a2e); c.lcdGrid = C (0x14000000);
        c.vizDry = C (0x3017240c); c.vizWet = C (0xa017240c); c.vizDuck = C (0xffa8260e); c.vizGate = C (0xff0a4a6a);
        c.vizCombined = C (0xff000000); c.vizBloom = C (0x30fff7c0); c.vizThreshDuck = C (0xb0a8260e); c.vizThreshGate = C (0xb00a4a6a);
        t.lcdStyle = LcdStyle::greenLcd;
        return t;
    }

    FreddyTheme broadcast()
    {
        auto t = base ("broadcast_silver", "Broadcast Silver Console Rack", 11, "broadcast",
                       "Heavy brushed natural aluminium 4U, engraved black-filled labels, knurled black knobs, twin VU meters, green CRT scope.", true);
        auto& c = t.c;
        c.chassis = C (0xffc4c6c8); c.chassisLabel = C (0xff141414); c.sectionLabel = C (0xff141414); c.text = C (0xff141414);
        c.accent = C (0xffc8321e); c.secondary = C (0xff2a8a7a);
        c.lcdBg = C (0xff031208); c.lcdFg = C (0xff59ff8a); c.lcdDim = C (0xff1f6a35);
        c.vizDry = C (0x3559ff8a); c.vizWet = C (0xff59ff8a); c.vizDuck = C (0xffffb04a); c.vizGate = C (0xff6ad8ff);
        c.vizCombined = C (0xffeaffef); c.vizBloom = C (0x2a9affc0); c.vizThreshDuck = C (0xa0ffb04a); c.vizThreshGate = C (0xa06ad8ff);
        t.lcdStyle = LcdStyle::crtGreen;
        return t;
    }

    FreddyTheme champagne()
    {
        auto t = base ("tour_worn_champagne", "Tour-Worn Champagne", 12, "champagne",
                       "Champagne-gold anodized panel with heavy road wear, gaffer residue, masking-tape notes, cyan VFD, chunky toggles.", true);
        auto& c = t.c;
        c.chassis = C (0xffcbb68a); c.chassisLabel = C (0xff1e1a12); c.sectionLabel = C (0xff1e1a12); c.text = C (0xff1e1a12);
        c.accent = C (0xffe0482a); c.secondary = C (0xff1f8f9a);
        c.lcdBg = C (0xff021012); c.lcdFg = C (0xff4ff0e0); c.lcdDim = C (0xff1a6a66);
        c.vizDry = C (0x384ff0e0); c.vizWet = C (0xff4ff0e0); c.vizDuck = C (0xffffa050); c.vizGate = C (0xff8fb8ff);
        c.vizCombined = C (0xffeafffc); c.vizBloom = C (0x2affe6b0); c.vizThreshDuck = C (0xa0ffa050); c.vizThreshGate = C (0xa08fb8ff);
        t.lcdStyle = LcdStyle::vfdCyan;
        return t;
    }

    FreddyTheme lab()
    {
        auto t = base ("studio_lab_prototype", "Studio Lab Prototype", 13, "lab",
                       "Grey hammertone steel, embossed label-tape section names, cyan-green VFD dot matrix, chicken-head knobs, rocker switches.", true);
        auto& c = t.c;
        c.chassis = C (0xff6e7375); c.chassisLabel = C (0xfff2f2f2); c.sectionLabel = C (0xfff2f2f2); c.text = C (0xfff2f2f2);
        c.accent = C (0xffe8562e); c.secondary = C (0xff38b0a0);
        c.lcdBg = C (0xff021208); c.lcdFg = C (0xff5affc8); c.lcdDim = C (0xff1d6a52);
        c.vizDry = C (0x385affc8); c.vizWet = C (0xff5affc8); c.vizDuck = C (0xffffa64d); c.vizGate = C (0xff9ab8ff);
        c.vizCombined = C (0xffefffff); c.vizBloom = C (0x2affffb0); c.vizThreshDuck = C (0xa0ffa64d); c.vizThreshGate = C (0xa09ab8ff);
        t.lcdStyle = LcdStyle::vfdCyan;
        return t;
    }

    std::unique_ptr<FreddyLookAndFeel> make (const FreddyTheme& t) { return std::make_unique<PhotorealLookAndFeel> (t); }
}

std::vector<ThemeEntry> makePhotorealThemes()
{
    return { { harmonizer(), make }, { reverb84(), make }, { broadcast(), make }, { champagne(), make }, { lab(), make } };
}
} // namespace fv::ui
