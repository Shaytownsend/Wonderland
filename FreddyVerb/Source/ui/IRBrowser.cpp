#include "IRBrowser.h"

namespace fv::ui
{
juce::StringArray IRBrowser::categories()
{
    return { "ALL", "FAVS", "ROOMS", "PLATES", "CHAMBERS", "HALLS", "SPRINGS", "VINTAGE", "NONLIN", "FX", "USER" };
}

IRBrowser::IRBrowser()
{
    search.setTextToShowWhenEmpty ("SEARCH", juce::Colours::grey);
    search.addListener (this);
    search.setJustification (juce::Justification::centredLeft);
    search.setTitle ("Search IRs");
    addAndMakeVisible (search);
    setTitle ("IR browser");
}

void IRBrowser::setItems (std::vector<IRListItem> items)
{
    all = std::move (items);
    filter();
}

void IRBrowser::setCurrent (const juce::String& id)
{
    currentId = id;
    // keep the current IR on screen
    const int rows = std::max (1, (int) (listArea.getHeight() / rowH));
    for (int i = 0; i < (int) visible.size(); ++i)
        if (visible[(size_t) i].id == id)
        {
            if (i < scroll) scroll = i;
            else if (i >= scroll + rows) scroll = i - rows + 1;
            break;
        }
    repaint();
}

void IRBrowser::filter()
{
    visible.clear();
    const auto q = search.getText().trim().toLowerCase();
    for (auto& it : all)
    {
        const auto cat = it.category.toUpperCase();
        if (category == "FAVS" && ! it.favourite) continue;
        if (category == "USER" && it.factory) continue;
        if (category != "ALL" && category != "FAVS" && category != "USER" && ! cat.startsWith (category.substring (0, 4))) continue;
        if (q.isNotEmpty() && ! (it.name.toLowerCase().contains (q) || it.description.toLowerCase().contains (q))) continue;
        visible.push_back (it);
    }
    scroll = 0;
    setCurrent (currentId);
}

void IRBrowser::step (int delta)
{
    if (visible.empty()) return;
    int idx = 0;
    for (int i = 0; i < (int) visible.size(); ++i) if (visible[(size_t) i].id == currentId) idx = i;
    idx = (idx + delta + (int) visible.size()) % (int) visible.size();
    currentId = visible[(size_t) idx].id;
    const int rows = (int) (listArea.getHeight() / rowH);
    if (idx < scroll) scroll = idx;
    if (idx >= scroll + rows) scroll = idx - rows + 1;
    if (onSelect) onSelect (visible[(size_t) idx]);
    repaint();
}

void IRBrowser::resized()
{
    const auto keep = currentId;
    auto b = getLocalBounds().toFloat().reduced (5.0f);
    // row 1: search + load; row 2: category stepper "< PLATES >"; then a full-width list.
    // (A category *column* was unreadable at 75 % scale in the compact rack skins.)
    headArea = b.removeFromTop (17.0f);
    search.setBounds (headArea.withTrimmedRight (headArea.getWidth() * 0.45f).toNearestInt());
    b.removeFromTop (3.0f);
    catArea = b.removeFromTop (16.0f);
    b.removeFromTop (3.0f);
    listArea = b;
    rowH = juce::jlimit (14.0f, 18.0f, listArea.getHeight() / 8.0f);
    setCurrent (keep);
}

void IRBrowser::paint (juce::Graphics& g)
{
    auto* lnf = freddyLnf (*this);
    if (lnf == nullptr) return;
    const auto& c = lnf->theme().c;
    auto inner = lnf->drawDisplayFrame (g, getLocalBounds().toFloat());
    juce::ignoreUnused (inner);
    search.applyColourToAllText (c.lcdFg);
    search.setFont (lnf->monoFont (13.0f));

    // header right: browse button + fav filter
    auto hr = headArea.withTrimmedLeft (headArea.getWidth() * 0.57f);
    lnf->drawDisplayText (g, "[LOAD...]", hr, juce::Justification::centredRight);

    // category stepper
    {
        g.setColour (c.lcdFg.withAlpha (0.12f));
        g.fillRect (catArea);
        auto r = catArea.reduced (3.0f, 2.0f);
        lnf->drawDisplayText (g, "<", r.removeFromLeft (10.0f), juce::Justification::centred);
        lnf->drawDisplayText (g, ">", r.removeFromRight (10.0f), juce::Justification::centred);
        const juce::String label = category + "  " + juce::String ((int) visible.size());
        PixelFont::drawFitted (g, label, r, juce::Justification::centred, c.lcdFg, juce::Colours::transparentBlack,
                               PixelFont::DotShape::square, 1.8f);
    }

    // list
    const int rows = (int) (listArea.getHeight() / rowH);
    for (int r = 0; r < rows && scroll + r < (int) visible.size(); ++r)
    {
        const auto& it = visible[(size_t) (scroll + r)];
        auto row = juce::Rectangle<float> (listArea.getX(), listArea.getY() + (float) r * rowH, listArea.getWidth(), rowH);
        const bool sel = it.id == currentId;
        if (sel) { g.setColour (c.lcdFg); g.fillRect (row); }
        const auto fg = sel ? c.lcdBg : c.lcdFg;
        PixelFont::drawFitted (g, (it.favourite ? "*" : " ") + it.name.toUpperCase(), row.reduced (2.0f, rowH * 0.18f).withTrimmedRight (36.0f),
                               juce::Justification::centredLeft, fg, juce::Colours::transparentBlack, PixelFont::DotShape::square, 1.8f);
        if (it.rt60 > 0)
            PixelFont::drawFitted (g, juce::String (it.rt60, 1) + "S", row.reduced (2.0f, rowH * 0.22f).removeFromRight (34.0f),
                                   juce::Justification::centredRight, sel ? c.lcdBg : c.lcdFg.withAlpha (0.75f), juce::Colours::transparentBlack,
                                   PixelFont::DotShape::square, 1.6f);
    }
    if (visible.empty())
        lnf->drawDisplayText (g, all.empty() ? "NO IRS" : "NO MATCH", listArea.withHeight (16.0f), juce::Justification::centred, true);

    if (dragOver)
    {
        g.setColour (c.accent.withAlpha (0.3f));
        g.fillRect (getLocalBounds().toFloat().reduced (5));
        lnf->drawDisplayText (g, "DROP IR FILE", getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth(), 16.0f), juce::Justification::centred);
    }
    lnf->drawDisplayOverlay (g, getLocalBounds().toFloat().reduced (5.0f));
}

void IRBrowser::mouseDown (const juce::MouseEvent& e)
{
    if (catArea.contains (e.position))
    {
        const auto cats = categories();
        const float x = (e.position.x - catArea.getX()) / catArea.getWidth();
        if (x < 0.25f || x > 0.75f)
        {
            const int i = cats.indexOf (category);
            category = cats[(i + (x < 0.25f ? cats.size() - 1 : 1)) % cats.size()];
            filter();
            return;
        }
        juce::PopupMenu m;
        for (int i = 0; i < cats.size(); ++i) m.addItem (i + 1, cats[i], true, cats[i] == category);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [safe = juce::Component::SafePointer<IRBrowser> (this), cats] (int r)
                         {
                             if (safe == nullptr || r <= 0) return;
                             safe->category = cats[r - 1];
                             safe->filter();
                         });
        return;
    }
    if (headArea.contains (e.position) && e.position.x > headArea.getCentreX())
    {
        if (onBrowse) onBrowse();
        return;
    }
    if (listArea.contains (e.position))
    {
        const int r = scroll + (int) ((e.position.y - listArea.getY()) / rowH);
        if (r >= 0 && r < (int) visible.size())
        {
            auto& it = visible[(size_t) r];
            if (e.position.x < listArea.getX() + 10.0f || e.mods.isPopupMenu())
            {
                it.favourite = ! it.favourite;
                for (auto& a : all) if (a.id == it.id) a.favourite = it.favourite;
                if (onFavourite) onFavourite (it, it.favourite);
                repaint();
                return;
            }
            currentId = it.id;
            if (onSelect) onSelect (it);
            repaint();
        }
    }
}

void IRBrowser::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int rows = (int) (listArea.getHeight() / rowH);
    scroll = juce::jlimit (0, std::max (0, (int) visible.size() - rows), scroll + (w.deltaY > 0 ? -1 : 1));
    repaint();
}

bool IRBrowser::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac"))
            return true;
    return false;
}

void IRBrowser::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    if (onFilesDropped) onFilesDropped (files);
    repaint();
}
} // namespace fv::ui
