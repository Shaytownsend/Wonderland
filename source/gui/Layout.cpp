#include "Layout.h"
#include "Filmstrip.h"

namespace queen
{
Layout Layout::fromJson (const juce::String& json)
{
    Layout l;
    const auto root = juce::JSON::parse (json);
    if (auto* space = root["space"].getArray(); space != nullptr && space->size() == 2)
    {
        l.width = (float) (double) (*space)[0];
        l.height = (float) (double) (*space)[1];
    }
    l.background = root["background"].toString();
    if (auto* arr = root["controls"].getArray())
    {
        for (const auto& c : *arr)
        {
            ControlSpec s;
            s.id = c["id"].toString();
            s.type = c["t"].toString();
            s.param = c["param"].toString();
            s.action = c["action"].toString();
            s.source = c["source"].toString();
            s.label = c["label"].toString();
            s.ledFor = c["led_for"].toString();
            s.cap = c["cap"].toString();
            s.x = (float) (double) c["x"];
            s.y = (float) (double) c["y"];
            s.r = (float) (double) c["r"];
            s.w = (float) (double) c["w"];
            s.h = (float) (double) c["h"];
            s.value = c.hasProperty ("value") ? (int) c["value"] : -1;
            s.momentary = (bool) c["momentary"];
            if (auto* pos = c["positions"].getArray())
                for (const auto& p : *pos)
                    s.positions.add ((int) p);
            s.sprite = c["sprite"];
            s.raw = c;
            l.controls.push_back (std::move (s));
        }
    }
    return l;
}

const Layout& Layout::get()
{
    static const Layout layout = fromJson (assets::text ("layout.json"));
    return layout;
}

const ControlSpec* Layout::find (const juce::String& id) const
{
    for (const auto& c : controls)
        if (c.id == id)
            return &c;
    return nullptr;
}

juce::StringArray Layout::spriteFiles() const
{
    juce::StringArray files;
    files.add (background);
    for (const auto& c : controls)
        if (auto* obj = c.sprite.getDynamicObject())
            for (const auto& prop : obj->getProperties())
                if (prop.value.isString() && prop.value.toString().endsWith (".png"))
                    files.addIfNotAlreadyThere (prop.value.toString());
    return files;
}
} // namespace queen
