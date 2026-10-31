#pragma once

#include "Controls.h"

namespace fv::ui
{
struct IRListItem
{
    juce::String id;            // factory id or absolute file path
    juce::String name, category, description;
    double rt60 = 0.0;
    bool favourite = false, factory = true;
};

/** Category column + IR list + search + favourites + prev/next, drawn on the
    theme's LCD. Data is supplied by the editor from the IR library. */
class IRBrowser : public juce::Component, public juce::FileDragAndDropTarget, private juce::TextEditor::Listener
{
public:
    IRBrowser();
    void setItems (std::vector<IRListItem> items);
    void setCurrent (const juce::String& id);
    juce::String getCurrentId() const { return currentId; }

    std::function<void (const IRListItem&)> onSelect;
    std::function<void (const IRListItem&, bool fav)> onFavourite;
    std::function<void()> onBrowse;                         // open file chooser
    std::function<void (const juce::StringArray&)> onFilesDropped;

    void step (int delta);  // previous / next for fast auditioning

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

    static juce::StringArray categories();

private:
    void textEditorTextChanged (juce::TextEditor&) override { filter(); }
    void filter();
    std::vector<IRListItem> all, visible;
    juce::String currentId, category { "ALL" };
    juce::TextEditor search;
    juce::Rectangle<float> catArea, listArea, headArea;
    int scroll = 0;
    bool dragOver = false, favouritesOnly = false;
    float rowH = 13.0f;
};
} // namespace fv::ui
