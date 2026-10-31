// The 8 flat "vintage sampler hardware" directions. Each is FreddyTheme data plus
// a LookAndFeel subclass that overrides only what makes it different.
#include "../../FreddyLookAndFeel.h"
#include "FlatLookAndFeels.h"

namespace fv::ui
{
namespace
{
    using C = juce::Colour;

    FreddyTheme puttyClassic()
    {
        FreddyTheme t;
        t.id = "putty_classic"; t.name = "Putty Classic"; t.number = 1;
        t.description = "Warm grey-beige chassis, green-yellow backlit LCD, red-orange accent, black screen-printed labels. The baseline.";
        // defaults in FreddyTheme are the Putty Classic palette
        return t;
    }

    FreddyTheme studioBlackRack()
    {
        FreddyTheme t;
        t.id = "studio_black_rack"; t.name = "Studio Black Rack"; t.number = 2;
        t.description = "Charcoal 2U-style rack front with flat ears and screw dots, amber LCD, orange pad LEDs, dense small labels.";
        t.width = 1360; t.height = 800;
        t.rackEars = true; t.denseLabels = true; t.labelSize = 11.5f; t.layout = LayoutVariant::rack;
        t.cornerRadius = 2.0f;
        auto& c = t.c;
        c.chassis = C (0xff2a2a2b); c.chassisEdge = C (0xff3a3a3c); c.section = C (0xff1c1c1d); c.sectionEdge = C (0xff121213);
        c.sectionLabel = C (0xffc9c4b8); c.chassisLabel = C (0xffd9d4c8); c.text = C (0xffe6e1d6); c.textDim = C (0xff8f8a80);
        c.accent = C (0xffff8a1f); c.secondary = C (0xff4fb3c8);
        c.knobBody = C (0xff3b3b3d); c.knobLine = C (0xfff2ede2); c.knobArc = C (0xffff8a1f); c.knobTrack = C (0xff4a4a4c);
        c.pad = C (0xff3c3c3e); c.padText = C (0xffe6e1d6); c.ledOn = C (0xffff8a1f); c.ledOff = C (0xff4a3020);
        c.lcdBg = C (0xff241503); c.lcdFg = C (0xffffb43c); c.lcdDim = C (0xff8a5a18); c.lcdGrid = C (0x22000000); c.lcdBezel = C (0xff0c0c0c);
        c.vizDry = C (0x50ffb43c); c.vizWet = C (0xffffb43c); c.vizDuck = C (0xffff6a2a); c.vizGate = C (0xff4fd0e8);
        c.vizCombined = C (0xfffff3c8); c.vizBloom = C (0x30fff3c8); c.vizThreshDuck = C (0xa0ff6a2a); c.vizThreshGate = C (0xa04fd0e8);
        t.lcdStyle = LcdStyle::amberLcd;
        return t;
    }

    FreddyTheme creamWorkstation()
    {
        FreddyTheme t;
        t.id = "cream_workstation"; t.name = "Cream Workstation"; t.number = 3;
        t.description = "Off-white body, blue-backlit LCD with white pixels, soft grey pads, teal secondary accent. Clean and spacious.";
        t.cornerRadius = 10.0f; t.labelSize = 13.0f; t.knobStyle = KnobStyle::flatSkirt;
        auto& c = t.c;
        c.chassis = C (0xfff1ecdf); c.chassisEdge = C (0xffdcd5c4); c.section = C (0xffe3ddcd); c.sectionEdge = C (0xffcfc7b3);
        c.sectionLabel = C (0xff2c2c2c); c.chassisLabel = C (0xff2c2c2c); c.text = C (0xff2c2c2c); c.textDim = C (0xff6d6a62);
        c.accent = C (0xffd4502b); c.secondary = C (0xff13857f);
        c.knobBody = C (0xfff7f4ec); c.knobLine = C (0xff2c2c2c); c.knobArc = C (0xff13857f); c.knobTrack = C (0xffc4bdab);
        c.pad = C (0xffc9c6bf); c.padText = C (0xff262626); c.ledOn = C (0xff13a39b); c.ledOff = C (0xff9a978f);
        c.lcdBg = C (0xff1c3c94); c.lcdFg = C (0xffeef3ff); c.lcdDim = C (0xff7c92cc); c.lcdGrid = C (0x1a000000); c.lcdBezel = C (0xff3a3a3a);
        c.vizDry = C (0x55eef3ff); c.vizWet = C (0xffbcd0ff); c.vizDuck = C (0xffffa072); c.vizGate = C (0xff52e0d4);
        c.vizCombined = C (0xffffffff); c.vizBloom = C (0x30ffffff); c.vizThreshDuck = C (0xa0ffa072); c.vizThreshGate = C (0xa052e0d4);
        t.lcdStyle = LcdStyle::blueLcd;
        return t;
    }

    FreddyTheme padStation()
    {
        FreddyTheme t;
        t.id = "pad_station"; t.name = "Pad Station"; t.number = 4;
        t.description = "Drum-machine style: big square rubber pads select mode and curve shapes. Chunky, playful, high contrast on dark grey.";
        t.height = 860; t.layout = LayoutVariant::padStation; t.padStyle = PadStyle::chunky; t.knobStyle = KnobStyle::chunky;
        t.labelSize = 13.5f; t.cornerRadius = 4.0f;
        auto& c = t.c;
        c.chassis = C (0xff3b3c3f); c.chassisEdge = C (0xff2c2d30); c.section = C (0xff2a2b2e); c.sectionEdge = C (0xff1f2022);
        c.sectionLabel = C (0xfff0ece2); c.chassisLabel = C (0xfff0ece2); c.text = C (0xfff0ece2); c.textDim = C (0xffa09c94);
        c.accent = C (0xffff5a2a); c.secondary = C (0xff2ec4b6);
        c.knobBody = C (0xffe9e5da); c.knobLine = C (0xff1c1c1c); c.knobArc = C (0xffff5a2a); c.knobTrack = C (0xff4a4b4e);
        c.pad = C (0xffdcd7cb); c.padText = C (0xff1a1a1a); c.ledOn = C (0xffff5a2a); c.ledOff = C (0xff6a625a);
        c.lcdBg = C (0xff27300f); c.lcdFg = C (0xffc4d66e); c.lcdDim = C (0xff5f6b2c);
        t.lcdStyle = LcdStyle::greenLcd;
        return t;
    }

    FreddyTheme sunbleached89()
    {
        FreddyTheme t;
        t.id = "sunbleached_89"; t.name = "Sunbleached '89"; t.number = 5;
        t.description = "Aged, slightly yellowed plastic, muted pastel buttons, orange LCD, worn feel from flat colour only.";
        t.cornerRadius = 7.0f;
        auto& c = t.c;
        c.chassis = C (0xffe2d6b2); c.chassisEdge = C (0xffcfc19a); c.section = C (0xff7d7262); c.sectionEdge = C (0xff6d6354);
        c.sectionLabel = C (0xfffbf3dd); c.chassisLabel = C (0xff3b3426); c.text = C (0xfffbf3dd); c.textDim = C (0xffd6ccb4);
        c.accent = C (0xffd9653a); c.secondary = C (0xff4f8f8c);
        c.knobBody = C (0xffefe6cc); c.knobLine = C (0xff3b3426); c.knobArc = C (0xfff0a070); c.knobTrack = C (0xff665d4f);
        c.pad = C (0xffb3cbbd); c.padText = C (0xff2b2a22); c.ledOn = C (0xffff6a3a); c.ledOff = C (0xff8a7f6a);
        c.lcdBg = C (0xff331a06); c.lcdFg = C (0xffffa04a); c.lcdDim = C (0xff8f5521); c.lcdGrid = C (0x1e000000); c.lcdBezel = C (0xff4a4134);
        c.vizDry = C (0x55ffa04a); c.vizWet = C (0xffffb870); c.vizDuck = C (0xffff6040); c.vizGate = C (0xff6fd6c6);
        c.vizCombined = C (0xfffff0c0); c.vizBloom = C (0x30fff0c0); c.vizThreshDuck = C (0xa0ff6040); c.vizThreshGate = C (0xa06fd6c6);
        t.lcdStyle = LcdStyle::orangeLcd;
        return t;
    }

    FreddyTheme nightSession()
    {
        FreddyTheme t;
        t.id = "night_session"; t.name = "Night Session"; t.number = 6;
        t.description = "Dark mode: near-black panel, red 7-segment LED readouts, dim red-on-black LCD, minimal labels.";
        t.knobStyle = KnobStyle::flatCap; t.lcdStyle = LcdStyle::redLed; t.pixelGrid = false;
        t.labelSize = 12.0f; t.cornerRadius = 3.0f;
        auto& c = t.c;
        c.chassis = C (0xff0c0c0d); c.chassisEdge = C (0xff1a1a1c); c.section = C (0xff141416); c.sectionEdge = C (0xff1e1e21);
        c.sectionLabel = C (0xff9a9a9e); c.chassisLabel = C (0xffa0a0a4); c.text = C (0xffc8c8cc); c.textDim = C (0xff76767a);
        c.accent = C (0xffff3b2f); c.secondary = C (0xff3fa9ff);
        c.knobBody = C (0xff1f1f22); c.knobLine = C (0xffff5a4a); c.knobArc = C (0xffc0281e); c.knobTrack = C (0xff2a2a2e);
        c.pad = C (0xff1d1d20); c.padText = C (0xffb0b0b4); c.ledOn = C (0xffff3b2f); c.ledOff = C (0xff3a1412);
        c.lcdBg = C (0xff0e0303); c.lcdFg = C (0xffff4a3a); c.lcdDim = C (0xff7a2018); c.lcdGrid = C (0x00000000); c.lcdBezel = C (0xff050505);
        c.vizDry = C (0x45ff4a3a); c.vizWet = C (0xffd83a2c); c.vizDuck = C (0xffffa040); c.vizGate = C (0xff4ab4ff);
        c.vizCombined = C (0xffffe6e0); c.vizBloom = C (0x28ff8a7a); c.vizThreshDuck = C (0xa0ffa040); c.vizThreshGate = C (0xa04ab4ff);
        return t;
    }

    FreddyTheme serviceManual()
    {
        FreddyTheme t;
        t.id = "service_manual"; t.name = "Service Manual"; t.number = 7;
        t.description = "Drawn like a vintage service manual: two-tone line art, printed signal-flow arrows between sections, mono LCD.";
        t.knobStyle = KnobStyle::outline; t.padStyle = PadStyle::outline; t.lcdStyle = LcdStyle::mono;
        t.signalArrows = true; t.layout = LayoutVariant::manual; t.cornerRadius = 0.0f; t.pixelGrid = false;
        auto& c = t.c;
        c.chassis = C (0xfff3efe4); c.chassisEdge = C (0xff1d1d1d); c.section = C (0xfff3efe4); c.sectionEdge = C (0xff1d1d1d);
        c.sectionLabel = C (0xff1d1d1d); c.chassisLabel = C (0xff1d1d1d); c.text = C (0xff1d1d1d); c.textDim = C (0xff5a5a5a);
        c.accent = C (0xffc0392b); c.secondary = C (0xff1f6f8b);
        c.knobBody = C (0xfff3efe4); c.knobLine = C (0xff1d1d1d); c.knobArc = C (0xffc0392b); c.knobTrack = C (0xffb9b4a8);
        c.pad = C (0xfff3efe4); c.padText = C (0xff1d1d1d); c.ledOn = C (0xffc0392b); c.ledOff = C (0xfff3efe4);
        c.lcdBg = C (0xffdde0d2); c.lcdFg = C (0xff1d1d1d); c.lcdDim = C (0xff8a8d80); c.lcdGrid = C (0x00000000); c.lcdBezel = C (0xff1d1d1d);
        c.vizDry = C (0x40000000); c.vizWet = C (0xff6a6a6a); c.vizDuck = C (0xffc0392b); c.vizGate = C (0xff1f6f8b);
        c.vizCombined = C (0xff111111); c.vizBloom = C (0x26c0392b); c.vizThreshDuck = C (0xb0c0392b); c.vizThreshGate = C (0xb01f6f8b);
        return t;
    }

    FreddyTheme floppyEraPro()
    {
        FreddyTheme t;
        t.id = "floppy_era_pro"; t.name = "Floppy Era Pro"; t.number = 8;
        t.description = "Wide, horizontal and sectioned: a colour band per stage, a big central LCD and a disk-drive slot IR loader.";
        t.width = 1560; t.height = 760; t.layout = LayoutVariant::floppyWide; t.sectionStripes = true; t.floppySlot = true;
        t.cornerRadius = 3.0f; t.labelSize = 12.5f;
        auto& c = t.c;
        c.chassis = C (0xffcdc8bc); c.chassisEdge = C (0xffb5afa2); c.section = C (0xff36363b); c.sectionEdge = C (0xff27272b);
        c.sectionLabel = C (0xff141414); c.chassisLabel = C (0xff1c1c1c); c.text = C (0xffe9e5da); c.textDim = C (0xff9e9a90);
        c.accent = C (0xffe2532d); c.secondary = C (0xff2fa6a0);
        c.knobBody = C (0xff4a4a50); c.knobLine = C (0xfff0ece2); c.knobArc = C (0xffe2532d); c.knobTrack = C (0xff55555b);
        c.pad = C (0xff4c4c52); c.padText = C (0xffe9e5da);
        return t;
    }
}

std::vector<ThemeEntry> makeFlatThemes()
{
    auto mk = [] (FreddyTheme t, auto factory) { return ThemeEntry { std::move (t), factory }; };
    std::vector<ThemeEntry> v;
    v.push_back (mk (puttyClassic(),     [] (const FreddyTheme& t) -> std::unique_ptr<FreddyLookAndFeel> { return std::make_unique<FreddyLookAndFeel> (t); }));
    v.push_back (mk (studioBlackRack(),  [] (const FreddyTheme& t) -> std::unique_ptr<FreddyLookAndFeel> { return std::make_unique<StudioRackLookAndFeel> (t); }));
    v.push_back (mk (creamWorkstation(), [] (const FreddyTheme& t) -> std::unique_ptr<FreddyLookAndFeel> { return std::make_unique<CreamLookAndFeel> (t); }));
    v.push_back (mk (padStation(),       [] (const FreddyTheme& t) -> std::unique_ptr<FreddyLookAndFeel> { return std::make_unique<PadStationLookAndFeel> (t); }));
    v.push_back (mk (sunbleached89(),    [] (const FreddyTheme& t) -> std::unique_ptr<FreddyLookAndFeel> { return std::make_unique<SunbleachedLookAndFeel> (t); }));
    v.push_back (mk (nightSession(),     [] (const FreddyTheme& t) -> std::unique_ptr<FreddyLookAndFeel> { return std::make_unique<NightSessionLookAndFeel> (t); }));
    v.push_back (mk (serviceManual(),    [] (const FreddyTheme& t) -> std::unique_ptr<FreddyLookAndFeel> { return std::make_unique<ServiceManualLookAndFeel> (t); }));
    v.push_back (mk (floppyEraPro(),     [] (const FreddyTheme& t) -> std::unique_ptr<FreddyLookAndFeel> { return std::make_unique<FloppyLookAndFeel> (t); }));
    return v;
}
} // namespace fv::ui
