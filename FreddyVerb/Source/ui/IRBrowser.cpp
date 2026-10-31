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
    repaint();
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
    auto b = getLocalBounds().toFloat().reduced (5.0f);
    headArea = b.removeFromTop (18.0f);
    search.setBounds (headArea.withTrimmedRight (headArea.getWidth() * 0.45f).toNearestInt());
    b.removeFromTop (3.0f);
    catArea = b.removeFromLeft (std::max (62.0f, b.getWidth() * 0.27f));
    b.removeFromLeft (4.0f);
    listArea = b;
    rowH = std::max (12.0f, std::min (15.0f, getHeight() / 16.0f));
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

    // categories
    const auto cats = categories();
    const float ch = catArea.getHeight() / (float) cats.size();
    for (int i = 0; i < cats.size(); ++i)
    {
        auto r = juce::Rectangle<float> (catArea.getX(), catArea.getY() + (float) i * ch, catArea.getWidth(), ch);
        const bool sel = cats[i] == category;
        if (sel) { g.setColour (c.lcdFg); g.fillRect (r.reduced (0, 1)); }
        PixelFont::drawFitted (g, cats[i], r.reduced (3.0f, ch * 0.22f), juce::Justification::centredLeft,
                               sel ? c.lcdBg : c.lcdFg, juce::Colours::transparentBlack, PixelFont::DotShape::square, 1.6f);
    }
    g.setColour (c.lcdDim);
    g.fillRect (catArea.getRight() + 1.5f, catArea.getY(), 1.0f, catArea.getHeight());

    // list
    const int rows = (int) (listArea.getHeight() / rowH);
    for (int r = 0; r < rows && scroll + r < (int) visible.size(); ++r)
    {
        const auto& it = visible[(size_t) (scroll + r)];
        auto row = juce::Rectangle<float> (listArea.getX(), listArea.getY() + (float) r * rowH, listArea.getWidth(), rowH);
        const bool sel = it.id == currentId;
        if (sel) { g.setColour (c.lcdFg); g.fillRect (row); }
        const auto fg = sel ? c.lcdBg : c.lcdFg;
        PixelFont::drawFitted (g, (it.favourite ? "*" : " ") + it.name.toUpperCase(), row.reduced (2.0f, rowH * 0.2f).withTrimmedRight (30.0f),
                               juce::Justification::centredLeft, fg, juce::Colours::transparentBlack, PixelFont::DotShape::square, 1.5f);
        if (it.rt60 > 0)
            PixelFont::drawFitted (g, juce::String (it.rt60, 1) + "S", row.reduced (2.0f, rowH * 0.25f).removeFromRight (28.0f),
                                   juce::Justification::centredRight, sel ? c.lcdBg : c.lcdDim, juce::Colours::transparentBlack,
                                   PixelFont::DotShape::square, 1.3f);
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
        const int i = (int) ((e.position.y - catArea.getY()) / (catArea.getHeight() / (float) cats.size()));
        category = cats[juce::jlimit (0, cats.size() - 1, i)];
        filter();
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
