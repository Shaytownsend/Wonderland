#include "Theme.h"
#include "FreddyLookAndFeel.h"

namespace fv::ui
{
const std::vector<ThemeEntry>& allThemes()
{
    static const std::vector<ThemeEntry> themes = []
    {
        auto v = makeFlatThemes();
        for (auto& p : makePhotorealThemes()) v.push_back (std::move (p));
        return v;
    }();
    return themes;
}

const ThemeEntry* findTheme (const juce::String& id)
{
    for (auto& t : allThemes())
        if (t.theme.id == id) return &t;
    return nullptr;
}
} // namespace fv::ui
