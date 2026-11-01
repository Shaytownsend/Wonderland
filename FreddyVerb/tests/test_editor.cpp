#include <catch2/catch_test_macros.hpp>
#include "PluginEditor.h"
#include "ui/Theme.h"

TEST_CASE ("Editor opens on the default skin, resizes 75-200 % with a fixed aspect ratio, and switches skins live", "[ui][editor]")
{
    fv::FreddyVerbProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    std::unique_ptr<fv::FreddyVerbEditor> ed (dynamic_cast<fv::FreddyVerbEditor*> (proc.createEditor()));
    REQUIRE (ed != nullptr);
    CHECK (ed->getThemeId() == fv::ui::defaultThemeId());
    const auto logical = ed->logicalBounds();
    for (float s : { 0.75f, 1.0f, 1.5f, 2.0f })
    {
        ed->setScale (s);
        CHECK (std::abs (ed->getWidth() - logical.getWidth() * s) <= 1.0f);
        CHECK (std::abs ((double) ed->getWidth() / ed->getHeight() - (double) logical.getWidth() / logical.getHeight()) < 0.01);
    }
    ed->setScale (3.0f);   // clamped
    CHECK (std::abs (ed->getScale() - 2.0f) < 1e-6f);
    for (auto& t : fv::ui::allThemes())
    {
        ed->setTheme (t.theme.id);
        CHECK (ed->getThemeId() == t.theme.id);
        ed->tickNow();
    }
    ed.reset();
}
