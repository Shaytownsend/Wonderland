#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace queen
{
// One control from assets/layout.json (generated from the registry by tools/make_layout.py).
struct ControlSpec
{
    juce::String id, type, param, action, source, label, ledFor, cap;
    float x = 0, y = 0, r = 0, w = 0, h = 0;
    int value = -1; // radio buttons: the choice index this button selects
    bool momentary = false;
    juce::Array<int> positions; // switches: choice index per position, top -> bottom
    juce::var sprite;           // sprite file names / sizes
    juce::var raw;

    juce::String spriteName (const char* key) const { return sprite.getProperty (key, {}).toString(); }
    float spriteFloat (const char* key, float fallback = 0.0f) const { return (float) (double) sprite.getProperty (key, fallback); }
};

struct Layout
{
    float width = 1800.0f, height = 830.0f;
    juce::String background;
    std::vector<ControlSpec> controls;

    static Layout fromJson (const juce::String& json);
    static const Layout& get(); // parsed once from the embedded layout.json

    const ControlSpec* find (const juce::String& id) const;
    juce::StringArray spriteFiles() const;
};
} // namespace queen
