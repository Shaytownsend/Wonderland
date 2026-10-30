#pragma once

#include "PluginProcessor.h"
#include "gui/HostBar.h"
#include "gui/PanelComponent.h"
#include "gui/QueenLookAndFeel.h"

// Logical canvas: HostBar (1800 x 70) above the panel (1800 x 830, registry space).
// The whole canvas is scaled with a transform; the window keeps the 1800:900 aspect.
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int logicalWidth = 1800, logicalHeight = 830 + queen::HostBar::height;

    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    queen::PanelComponent& getPanel() { return panel; }
    queen::HostBar& getHostBar() { return hostBar; }
    float getScale() const { return scale; }

private:
    PluginProcessor& processorRef;
    queen::QueenLookAndFeel lookAndFeel;
    juce::Component canvas;
    queen::HostBar hostBar;
    queen::PanelComponent panel;
    juce::TooltipWindow tooltips { this, 600 };
    float scale = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
