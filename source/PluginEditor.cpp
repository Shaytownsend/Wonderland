#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), lookAndFeel (queen::Layout::get()), hostBar (p), panel (p, lookAndFeel)
{
    setLookAndFeel (&lookAndFeel);
    canvas.setSize (logicalWidth, logicalHeight);
    canvas.addAndMakeVisible (hostBar);
    canvas.addAndMakeVisible (panel);
    hostBar.setBounds (0, 0, logicalWidth, queen::HostBar::height);
    panel.setTopLeftPosition (0, queen::HostBar::height);
    addAndMakeVisible (canvas);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) logicalWidth / logicalHeight);
    setResizeLimits (logicalWidth / 2, logicalHeight / 2, logicalWidth * 2, logicalHeight * 2);
    setSize (logicalWidth * 4 / 5, logicalHeight * 4 / 5);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void PluginEditor::resized()
{
    scale = (float) getWidth() / (float) logicalWidth;
    canvas.setTransform (juce::AffineTransform::scale (scale));
}
