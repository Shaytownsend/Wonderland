#pragma once

#include "../PluginProcessor.h"
#include "QueenLookAndFeel.h"

namespace queen
{
// Flat strip above the panel: preset name (click for list), prev/next, save, init, settings.
class HostBar : public juce::Component, private juce::Timer
{
public:
    static constexpr int height = 70;

    explicit HostBar (PluginProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

    void step (int delta); // prev / next across all 40 slots
    void save();           // store the edit buffer into the current slot
    void init();           // load the init patch into the edit buffer
    void showPresetMenu();
    void showSettingsMenu();
    juce::String getPresetText() const;

private:
    struct IconButton : juce::Button
    {
        IconButton (const juce::String& name, std::function<void (juce::Graphics&, juce::Rectangle<float>)> d)
            : juce::Button (name), drawIcon (std::move (d)) {}
        void paintButton (juce::Graphics& g, bool over, bool down) override;
        std::function<void (juce::Graphics&, juce::Rectangle<float>)> drawIcon;
    };

    void timerCallback() override { repaint(); }

    PluginProcessor& processor;
    IconButton prev, next, saveButton, initButton, settingsButton, nameButton;
};
} // namespace queen
