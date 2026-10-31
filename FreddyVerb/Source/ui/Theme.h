#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace fv::ui
{
class FreddyLookAndFeel;

enum class ThemeGroup { flat, photoreal };

/** How knobs are drawn by the flat LookAndFeel. */
enum class KnobStyle
{
    flatLine,       // flat disc + indicator line + value arc (Putty Classic)
    flatSkirt,      // disc with a flat contrasting skirt ring
    flatCap,        // small cap with a big coloured arc around it (Night Session)
    outline,        // two-tone line art (Service Manual)
    chunky,         // large high-contrast pad-like knob (Pad Station)
    filmstrip       // photoreal: frames from a rendered filmstrip
};

enum class PadStyle { rubber, square, outline, chunky, membrane, image };

enum class LcdStyle
{
    greenLcd,     // green-yellow backlit LCD (#B6C76A on dark olive)
    amberLcd,     // amber
    blueLcd,      // blue backlight, white pixels
    orangeLcd,    // orange
    redLed,       // dim red-on-black, red 7-segment readouts
    mono,         // monochrome manual-print look
    vfdCyan,      // vacuum fluorescent, cyan-green with grid + glow
    vfdAmber,     // amber VFD
    crtGreen      // green CRT scope with persistence
};

enum class LayoutVariant
{
    standard,       // top bar / left IR / centre LCD / duck+gate below / right tone-space-mix
    rack,           // standard inside a rack front with ears
    padStation,     // standard, but big pad grid for mode + curve shapes
    manual,         // standard with signal-flow arrows printed between sections
    floppyWide,     // wide, horizontal colour-striped sections + disk slot IR loader
    photoreal       // positions come from the skin's layout JSON
};

/** Everything that differs between the 13 UI options. Swapping themes must never
    touch DSP or parameter code: the editor only reads this struct and calls the
    theme's LookAndFeel. */
struct FreddyTheme
{
    juce::String id;            // stable key, e.g. "putty_classic"
    juce::String name;          // "Putty Classic"
    juce::String description;   // one line for the contact sheet
    ThemeGroup group = ThemeGroup::flat;
    int number = 1;             // 1..13, used for file names

    // Logical (100%) editor size; the editor keeps this aspect ratio.
    int width = 1280, height = 800;

    struct Colours
    {
        juce::Colour chassis      { 0xffd8d3c8 };
        juce::Colour chassisEdge  { 0xffb9b3a6 };
        juce::Colour section      { 0xff2b2b2b };
        juce::Colour sectionEdge  { 0xff1e1e1e };
        juce::Colour sectionLabel { 0xffd8d3c8 };   // label printed on a section
        juce::Colour chassisLabel { 0xff1d1d1d };   // label printed on the chassis
        juce::Colour text         { 0xffece8df };
        juce::Colour textDim      { 0xff9c978d };
        juce::Colour accent       { 0xffe2532d };   // logo + active states
        juce::Colour secondary    { 0xff3f8f8a };   // gate (teal)
        juce::Colour knobBody     { 0xff3a3a3a };
        juce::Colour knobLine     { 0xffece8df };
        juce::Colour knobArc      { 0xffe2532d };
        juce::Colour knobTrack    { 0xff555555 };
        juce::Colour pad          { 0xff4a4a4a };
        juce::Colour padText      { 0xffece8df };
        juce::Colour ledOn        { 0xffff4a1f };
        juce::Colour ledOff       { 0xff4a2a22 };

        // LCD / display
        juce::Colour lcdBg        { 0xff2f3518 };
        juce::Colour lcdFg        { 0xffb6c76a };
        juce::Colour lcdDim       { 0xff5f6a33 };
        juce::Colour lcdGrid      { 0x18000000 };
        juce::Colour lcdBezel     { 0xff1a1a1a };

        // Envelope visualizer (must stay distinguishable in every theme)
        juce::Colour vizDry       { 0x55b6c76a };
        juce::Colour vizWet       { 0xffb6c76a };
        juce::Colour vizDuck      { 0xffff7a4a };
        juce::Colour vizGate      { 0xff5ad1c8 };
        juce::Colour vizCombined  { 0xfffff2a0 };
        juce::Colour vizBloom     { 0x33fff2a0 };
        juce::Colour vizThreshDuck{ 0x99ff7a4a };
        juce::Colour vizThreshGate{ 0x995ad1c8 };
    } c;

    KnobStyle knobStyle = KnobStyle::flatLine;
    PadStyle padStyle = PadStyle::rubber;
    LcdStyle lcdStyle = LcdStyle::greenLcd;
    LayoutVariant layout = LayoutVariant::standard;

    float cornerRadius = 6.0f;      // section corner radius
    float labelSize = 13.0f;        // section/control label font height
    float knobScale = 1.0f;         // relative knob size
    bool  denseLabels = false;
    bool  rackEars = false;
    bool  signalArrows = false;
    bool  sectionStripes = false;
    bool  floppySlot = false;
    bool  pixelGrid = true;         // LCD pixel grid overlay

    // Photoreal
    juce::String skinFolder;        // Resources/skins/<skinFolder>
};

/** A theme = its data + its LookAndFeel. */
struct ThemeEntry
{
    FreddyTheme theme;
    std::function<std::unique_ptr<FreddyLookAndFeel> (const FreddyTheme&)> makeLookAndFeel;
};

/** All 13 options in display order: 8 flat, then 5 photoreal. */
const std::vector<ThemeEntry>& allThemes();
const ThemeEntry* findTheme (const juce::String& id);

// Registration helpers implemented in themes/flat/FlatThemes.cpp and
// themes/photoreal/PhotorealThemes.cpp.
std::vector<ThemeEntry> makeFlatThemes();
std::vector<ThemeEntry> makePhotorealThemes();
} // namespace fv::ui
