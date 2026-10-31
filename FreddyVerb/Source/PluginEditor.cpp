#include "PluginEditor.h"
#include "ui/FreddyLookAndFeel.h"

namespace fv
{
FreddyVerbEditor::FreddyVerbEditor (FreddyVerbProcessor& p)
    : AudioProcessorEditor (p), proc (p), hub (p.getAPVTS())
{
    panel = std::make_unique<ui::MainPanel> (proc, hub);
    addAndMakeVisible (*panel);
    tooltips = std::make_unique<juce::TooltipWindow> (this, 600);

    panel->onSettings = [this] { showSettingsMenu(); };
    panel->onUndo = [this] { proc.getUndoManager().undo(); };
    panel->onRedo = [this] { proc.getUndoManager().redo(); };

    auto& ui = proc.getUIState();
    scale = (float) (double) ui.getProperty ("scale", 1.0);
    setResizable (true, true);
    setTheme (ui.getProperty ("theme", ui::allThemes().front().theme.id).toString());
    startTimerHz (60);
}

FreddyVerbEditor::~FreddyVerbEditor()
{
    stopTimer();
    panel->setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
    tooltips.reset();
    panel.reset();
}

void FreddyVerbEditor::setTheme (const juce::String& id)
{
    const auto* entry = ui::findTheme (id);
    if (entry == nullptr) entry = &ui::allThemes().front();
    currentThemeId = entry->theme.id;
    proc.getUIState().setProperty ("theme", currentThemeId, nullptr);

    auto next = entry->makeLookAndFeel (entry->theme);
    panel->setLookAndFeel (next.get());
    setLookAndFeel (next.get());
    if (tooltips) tooltips->setLookAndFeel (next.get());
    lnf = std::move (next);

    logicalW = entry->theme.width;
    logicalH = entry->theme.height;
    panel->setBounds (0, 0, logicalW, logicalH);
    panel->themeChanged (lnf->theme());
    applyScale();
}

void FreddyVerbEditor::setScale (float s)
{
    scale = juce::jlimit (0.75f, 2.0f, s);
    proc.getUIState().setProperty ("scale", scale, nullptr);
    applyScale();
}

void FreddyVerbEditor::applyScale()
{
    settingSize = true;
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) logicalW / (double) logicalH);
        c->setSizeLimits (juce::roundToInt (logicalW * 0.75f), juce::roundToInt (logicalH * 0.75f),
                          logicalW * 2, logicalH * 2);
    }
    setSize (juce::roundToInt (logicalW * scale), juce::roundToInt (logicalH * scale));
    settingSize = false;
    resized();
}

void FreddyVerbEditor::paint (juce::Graphics& g)
{
    g.fillAll (lnf ? lnf->theme().c.chassis : juce::Colours::black);
}

void FreddyVerbEditor::resized()
{
    if (panel == nullptr) return;
    if (! settingSize)
    {
        scale = juce::jlimit (0.75f, 2.0f, (float) getWidth() / (float) logicalW);
        proc.getUIState().setProperty ("scale", scale, nullptr);
    }
    panel->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) logicalW,
                                                       (float) getHeight() / (float) logicalH));
}

void FreddyVerbEditor::timerCallback()
{
    panel->tick();
}

void FreddyVerbEditor::showSettingsMenu()
{
    juce::PopupMenu m, flat, photo, sizes;
    int idx = 0;
    for (auto& t : ui::allThemes())
    {
        auto& menu = t.theme.group == ui::ThemeGroup::flat ? flat : photo;
        menu.addItem (100 + idx, juce::String (t.theme.number).paddedLeft ('0', 2) + "  " + t.theme.name, true, t.theme.id == currentThemeId);
        ++idx;
    }
    for (int pct : { 75, 100, 125, 150, 175, 200 })
        sizes.addItem (pct, juce::String (pct) + "%", true, juce::roundToInt (scale * 100.0f) == pct);
    m.addSectionHeader ("UI THEME");
    m.addSubMenu ("Flat", flat);
    m.addSubMenu ("Photoreal Rack", photo);
    m.addSubMenu ("UI Size", sizes);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe = juce::Component::SafePointer<FreddyVerbEditor> (this)] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         if (r >= 100) safe->setTheme (ui::allThemes()[(size_t) (r - 100)].theme.id);
                         else safe->setScale ((float) r / 100.0f);
                     });
}
} // namespace fv
