#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/MainPanel.h"
#include "ui/Theme.h"

namespace fv
{
class FreddyVerbEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FreddyVerbEditor (FreddyVerbProcessor&);
    ~FreddyVerbEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Switch to one of the 13 themes by id (live, while audio plays). */
    void setTheme (const juce::String& themeId);
    juce::String getThemeId() const { return currentThemeId; }
    void setScale (float s);
    float getScale() const noexcept { return scale; }

    ui::MainPanel& getPanel() { return *panel; }
    /** Logical size of the current theme. */
    juce::Rectangle<int> logicalBounds() const { return { logicalW, logicalH }; }

    /** Used by the snapshot tool to drive the 60 fps update manually. */
    void tickNow() { timerCallback(); }

private:
    void timerCallback() override;
    void showSettingsMenu();
    void applyScale();

    FreddyVerbProcessor& proc;
    ui::ControlHub hub;
    std::unique_ptr<ui::FreddyLookAndFeel> lnf;
    std::unique_ptr<ui::MainPanel> panel;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    juce::String currentThemeId;
    int logicalW = 1280, logicalH = 800;
    float scale = 1.0f;
    bool settingSize = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreddyVerbEditor)
};
} // namespace fv
